#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <random>
#include <string_view>
#include <unordered_map>
#include "util/Random32.h"
#include "util/doNotOptimize.h"

/// YCSB Benchmark workload, based on Alexander van Renen's version
static constexpr size_t ycsb_tuple_count = 1000000;
static constexpr size_t ycsb_field_count = 10;
static constexpr size_t ycsb_field_length = 100;
static constexpr size_t ycsb_tx_count = 1000000;
using YcsbKey = uint32_t;

struct YcsbDataSet;
using YcsbValue = YcsbDataSet;

struct RandomString {
   Random32 rand;

   void fill(size_t len, char* dest) {
      using namespace std::literals::string_view_literals;
      static constexpr auto alphanum = "0123456789"
                                       "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                       "abcdefghijklmnopqrstuvwxyz"sv;
      std::generate(&dest[0], &dest[len - 1], [&] { return alphanum[rand.next() % alphanum.size()]; });
      dest[len - 1] = '\0';
   }

   template<typename CharContainer>
   void fill(CharContainer &container) {
      fill(container.size(), container.data());
   }
};

struct YcsbDataSet {
   template<class T, size_t ROWS, size_t COLUMNS>
   using Matrix = std::array<std::array<T, COLUMNS>, ROWS>;

   Matrix<char, ycsb_field_count, ycsb_field_length> rows{};

   std::array<char, ycsb_field_length> &operator[](size_t i) {
      return rows[i];
   }

   const std::array<char, ycsb_field_length> &operator[](size_t i) const {
      return rows[i];
   }

   YcsbDataSet() = default;

   explicit YcsbDataSet(RandomString &gen) {
      for (auto &row : rows) {
         gen.fill(row);
      }
   }
};

auto generateLookupKeys(size_t count, uint32_t maxValue) {
   auto rand = Random32();
   auto res = std::vector<YcsbKey>();
   res.reserve(count);
   std::generate_n(std::back_inserter(res), count, [&] { return rand.next() % maxValue; });
   return res;
}

auto generateZipfLookupKeys(size_t count, double factor = 1.0) {
   using distribution = std::discrete_distribution<size_t>;
   std::mt19937 generator(88172645463325252ull);
   auto zipfdist = [&] {
      std::vector<double> buffer(ycsb_tuple_count + 1);
      for (unsigned rank = 1; rank <= ycsb_tuple_count; ++rank) {
         buffer[rank] = std::pow(rank, -factor);
      }

      return distribution(buffer.begin() + 1, buffer.end());
   }();
   auto rand = [&] { return zipfdist(generator); };

   auto res = std::vector<YcsbKey>();
   res.reserve(count);
   std::generate_n(std::back_inserter(res), count, [&] { return rand(); });
   return res;
}

struct YcsbDatabase {
   std::unordered_map<YcsbKey, YcsbDataSet> database{};

   YcsbDatabase() {
      auto gen = RandomString();
      database.reserve(ycsb_tuple_count);

      auto nextKey = uint32_t(0);
      for (size_t i = 0; i < ycsb_tuple_count; ++i) {
         database.emplace(nextKey++, YcsbDataSet(gen));
      }
   }

   template<typename OutputIterator>
   void lookup(YcsbKey lookupKey, size_t field, OutputIterator target) const {
      const auto fieldPtr = database.find(lookupKey);
      if (fieldPtr == database.end()) {
         throw;
      }
      const auto begin = fieldPtr->second[field].begin();
      const auto end = fieldPtr->second[field].end();
      DoNotOptimize(std::copy(begin, end, target));
   }
};
