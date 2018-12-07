#include <iostream>
#include <libpq-fe.h>
#include "ycsb.h"
#include <chrono>

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

   if (auto res = PQexec(postgres, create.c_str());
         PQresultStatus(res) != PGRES_COMMAND_OK) {
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

      if (auto res = PQexec(postgres, create.c_str());
            PQresultStatus(res) != PGRES_COMMAND_OK) {
         throw std::runtime_error(std::string("INSERT failed: ") + PQerrorMessage(postgres));
      }
   }
   std::cout << "\n";
}

void doSmallTx(pg_conn* postgres) {
   static constexpr auto iterations = size_t(1e6);
   static constexpr auto preparedStatement = "smallTx";

   std::cout << iterations << " very small prepared statements\n";

   if (auto res = PQprepare(postgres, preparedStatement, "SELECT 1;", 0, nullptr);
         PQresultStatus(res) != PGRES_COMMAND_OK) {
      throw std::runtime_error(std::string("PQprepare failed: ") + PQerrorMessage(postgres));
   }

   const auto timeTaken = bench([&] {
      for (size_t i = 0; i < iterations; ++i) {
         auto res = PQexecPrepared(postgres, preparedStatement, 0, nullptr, nullptr, nullptr, 0);
         if (PQresultStatus(res) != PGRES_TUPLES_OK) {
            throw std::runtime_error(std::string("PQexecPrepared failed: ") + PQerrorMessage(postgres));
         }

         if (PQntuples(res) != 1) {
            throw std::runtime_error("Unexpected data returned");
         }
         auto result = PQgetvalue(res, 0, 0);
         if (result[0] != '1') {
            throw std::runtime_error(std::string("Unexpected data returned: ") + result);
         }
         PQclear(res);
      }
   });

   std::cout << iterations / timeTaken << " msg/s\n";
}

void doLargeResultSet(pg_conn* /*postgres*/) {
   // TODO
}

int main(int argc, char** /*argv*/) {
   if (argc < 3) {
      std::cout << "Usage: mySqlBenchmark <user> <password> <host> <database>\n";
      return 1;
   }
   //auto user = argv[1];
   //auto password = argv[2];
   //auto host = argc > 3 ? argv[3] : nullptr;
   //auto database = argc > 4 ? argv[4] : "mysql";

   auto connections = {
         "TCP",
         "SharedMemory",
         "NamedPipe",
         "Socket"
   };

   for (auto connection : connections) {
      try {
         // TODO postgres connect
         auto postgres = PQconnectdb(connection);
         if (PQstatus(postgres) == CONNECTION_BAD) {
            throw std::runtime_error(std::string("Connection to database failed: ") + PQerrorMessage(postgres));
         }

         std::cout << "Preparing YCSB temporary table\n";
         prepareYcsb(postgres);

         doSmallTx(postgres);
         doLargeResultSet(postgres);
         PQfinish(postgres);
      } catch (const std::runtime_error &e) {
         std::cout << e.what() << '\n';
      }
      std::cout << '\n';
   }

   return 0;
}