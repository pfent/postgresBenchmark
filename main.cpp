#include <iostream>
#include <libpq-fe.h>
#include "ycsb.h"
#include <chrono>
#include <memory>

auto connect(const char* connection) {
   auto res = PQconnectdb(connection);
   return std::unique_ptr<PGconn, decltype(&PQfinish)>(res, &PQfinish);
}

auto exec(PGconn* connection, const char* query) {
   auto res = PQexec(connection, query);
   return std::unique_ptr<PGresult, decltype(&PQclear)>(res, &PQclear);
}

auto execPrepared(PGconn* connection, const char* stmtName, int nParams = 0, const char* const* paramValues = nullptr,
                  const int* paramLengths = nullptr, const int* paramFormats = nullptr, int resultFormat = 0) {
   auto res = PQexecPrepared(connection, stmtName, nParams, paramValues, paramLengths, paramFormats, resultFormat);
   return std::unique_ptr<PGresult, decltype(&PQclear)>(res, &PQclear);
}

auto
prepare(PGconn* connection, const char* stmtName, const char* query, int nParams = 0, const Oid* paramTypes = nullptr) {
   auto res = PQprepare(connection, stmtName, query, nParams, paramTypes);
   return std::unique_ptr<PGresult, decltype(&PQclear)>(res, &PQclear);
}


static auto db = YcsbDatabase();

template<typename T>
auto bench(T &&fun) {
   const auto start = std::chrono::high_resolution_clock::now();

   fun();

   const auto end = std::chrono::high_resolution_clock::now();

   return std::chrono::duration<double>(end - start).count();
}

void prepareYcsb(PGconn* postgres) {
   auto create = std::string("CREATE TEMPORARY TABLE Ycsb ( ycsb_key INTEGER PRIMARY KEY NOT NULL, ");
   for (size_t i = 1; i < ycsb_field_count; ++i) {
      create += "v" + std::to_string(i) + " CHAR(" + std::to_string(ycsb_field_length) + ") NOT NULL, ";
   }
   create += "v" + std::to_string(ycsb_field_count) + " CHAR(" + std::to_string(ycsb_field_length) + ") NOT NULL";
   create += ");";

   if (auto res = exec(postgres, create.c_str());
         PQresultStatus(res.get()) != PGRES_COMMAND_OK) {
      throw std::runtime_error(std::string("CREATE TABLE failed: ") + PQerrorMessage(postgres));
   }

   for (auto it = db.database.begin(); it != db.database.end();) {
      auto i = std::distance(db.database.begin(), it);
      if (i % (ycsb_tuple_count / 100) == 0) {
         std::cout << "\r" << static_cast<double>(i) * 100 / ycsb_tuple_count << "%" << std::flush;
      }
      auto statement = std::string("INSERT INTO Ycsb VALUES ");
      for (int j = 0; j < 1000; ++j, ++it) {
         auto&[key, value] = *it;
         statement += "(" + std::to_string(key) + ", ";
         for (auto &v : value.rows) {
            statement += "'";
            statement += v.data();
            statement += "', ";
         }
         statement.resize(statement.length() - 2); // remove last comma
         statement += "), ";
      }
      statement.resize(statement.length() - 2); // remove last comma
      statement += ";";

      if (auto res = exec(postgres, statement.c_str());
            PQresultStatus(res.get()) != PGRES_COMMAND_OK) {
         throw std::runtime_error(std::string("INSERT failed: ") + PQerrorMessage(postgres));
      }
   }
   std::cout << "\n";
}

void doSmallTx(pg_conn* postgres) {
   static constexpr auto iterations = size_t(1e6);
   static constexpr auto preparedStatement = "smallTx";

   std::cout << iterations << " very small prepared statements\n";

   if (auto res = prepare(postgres, preparedStatement, "SELECT 1;", 0, nullptr);
         PQresultStatus(res.get()) != PGRES_COMMAND_OK) {
      throw std::runtime_error(std::string("PQprepare failed: ") + PQerrorMessage(postgres));
   }

   const auto timeTaken = bench([&] {
      for (size_t i = 0; i < iterations; ++i) {
         auto res = execPrepared(postgres, preparedStatement, 0, nullptr, nullptr, nullptr, 0);
         if (PQresultStatus(res.get()) != PGRES_TUPLES_OK) {
            throw std::runtime_error(std::string("PQexecPrepared failed: ") + PQerrorMessage(postgres));
         }

         if (PQntuples(res.get()) != 1) {
            throw std::runtime_error("Unexpected data returned");
         }
         auto result = PQgetvalue(res.get(), 0, 0);
         if (result[0] != '1') {
            throw std::runtime_error(std::string("Unexpected data returned: ") + result);
         }
      }
   });

   std::cout << iterations / timeTaken << " msg/s\n";
}

void doLargeResultSet(pg_conn* /*postgres*/) {
   // TODO
}

int main(int argc, char** argv) {
   if (argc < 3) {
      std::cout << "Usage: mySqlBenchmark <user> <password> <host> <database>\n";
      return 1;
   }
   auto user = argv[1];
   auto password = argv[2];
   auto host = argc > 3 ? argv[3] : "127.0.0.1";
   auto database = argc > 4 ? argv[4] : "mysql";

   auto connections = {
         std::string("user=") + user + " password=" + password + " dbname=" + database + " hostaddr=" + host,
         std::string("user=") + user + " password=" + password + " dbname=" +
         database, // no hostaddr connects via socket
   };

   for (const auto &connection : connections) {
      try {
         std::cout << "connecting with: " << connection << "...\n";
         auto postgres = connect(connection.c_str());
         if (PQstatus(postgres.get()) == CONNECTION_BAD) {
            throw std::runtime_error(std::string("Connection to database failed: ") + PQerrorMessage(postgres.get()));
         }
         std::cout << "connected to: " << PQhost(postgres.get()) << "\n";

         std::cout << "Preparing YCSB temporary table\n";
         prepareYcsb(postgres.get());

         doSmallTx(postgres.get());
         doLargeResultSet(postgres.get());
      } catch (const std::runtime_error &e) {
         std::cout << e.what() << '\n';
      }
      std::cout << '\n';
   }

   return 0;
}