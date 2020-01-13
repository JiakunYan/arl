//
// Created by Jiakun Yan on 1/6/20.
//

#ifndef ARL_BLOOM_FILTER_HPP
#define ARL_BLOOM_FILTER_HPP

#include "detail/hash_funcs.h"
#include <math.h>

namespace arl {

  namespace local {
    struct BloomNextHash {
      uint64_t hash_fn(uint64_t key) {
        key = (~key) + (key << 21); // key = (key << 21) - key - 1;
        key = key ^ (key >> 24);
        key = (key + (key << 3)) + (key << 8); // key * 265
        key = key ^ (key >> 14);
        key = (key + (key << 2)) + (key << 4); // key * 21
        key = key ^ (key >> 28);
        key = key + (key << 31);
        return key;
      }

      uint64_t operator()(uint64_t hashValue) {
        return hash_fn(hashValue);
      }
    };

    // a concurrent shared-memory bloom filter
    template <
        typename T,
        typename Hash = std::hash<T>,
        typename NextHash = BloomNextHash
    >
    class BloomFilter {
      using bucket_t = uint64_t;
      size_t hash_n;
      size_t bucket_n;
      std::vector<std::atomic<bucket_t>> buckets;

    public:
      BloomFilter(size_t n, double error) {
        double num = log(error);
        double denom = 0.480453013918201; // ln(2)^2
        double bpe = -(num / denom);

        double dentries = (double) n;
        size_t bit_n = (size_t) (dentries * bpe);
        hash_n = (int) ceil(0.693147180559945 * bpe); // ln(2)

        bucket_n = (bit_n + 8 * sizeof(bucket_t) - 1) / (8 * sizeof(bucket_t));
//        printf("bit_n: %lu; bpe: %lf; bucket_n: %lu\n", bit_n, bpe, bucket_n);

        try {
          buckets = std::vector<std::atomic<bucket_t>>(bucket_n);
        } catch (std::exception &e) {
          ARL_Error(e.what(), " note: num bits is ", bit_n, " dentries is ", dentries, " bpe is ", bpe);
        }
        for (int i = 0; i < bucket_n; ++i) {
          buckets[i] = 0;
        }
      }
      size_t hash_fn_n() const {
        return hash_n;
      }

      size_t memory_usage() const {
        return buckets.size() * sizeof(bucket_t);
      }

      // return: whether data is already (possibly) contained
      bool add(const T& data) {
        auto hash_values = NextHash()(Hash()(data));
        size_t bucket_id = hash_values % bucket_n;

        bucket_t my_bucket = 0x0;

        for (int n = 1; n <= hash_n; n++) {
          hash_values = NextHash()(hash_values);
          int my_bit = hash_values % (8*sizeof(bucket_t));
          my_bucket |= (bucket_t) 0x1 << my_bit;
        }

        bucket_t old_bucket = buckets[bucket_id].fetch_or(my_bucket);
        bool is_contained = ((old_bucket & my_bucket) == my_bucket);
//        printf("add %s: (%lu, %016lx) old %016lx, %s\n", data.str, bucket_id, my_bucket, old_bucket, is_contained? "true": "false");

        return is_contained;
      }

      // return: whether data is already (possibly) contained
      bool possibly_contains(const T& data) const {
        auto hash_values = NextHash()(Hash()(data));
        size_t bucket_id = hash_values % bucket_n;

        bucket_t my_bucket = 0x0;

        for (int n = 1; n <= hash_n; n++) {
          hash_values = NextHash()(hash_values);
          int my_bit = hash_values % (8*sizeof(bucket_t));
          my_bucket |= (bucket_t) 0x1 << my_bit;
        }

        bucket_t the_bucket = buckets[bucket_id].load();
        bool is_contained = ((the_bucket & my_bucket) == my_bucket);
//        printf("find %s: (%lu, %016lx) old %016lx, %s\n", data.str, bucket_id, my_bucket, the_bucket, is_contained? "true": "false");

        return is_contained;
      }
    };
  }
}

#endif //ARL_BLOOM_FILTER_HPP
