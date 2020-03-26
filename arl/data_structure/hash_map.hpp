#ifndef ARL_HASH_MAP_HPP
#define ARL_HASH_MAP_HPP

#include "external/libcuckoo/cuckoohash_map.hh"

namespace arl {

template <
    typename Key,
    typename Val,
    typename Hash = std::hash<Key>,
    typename KeyEqual = std::equal_to<Key>
>
class HashMap {
public:
  // initialize the local map
  HashMap(size_t size) {
    my_size = (size + proc::rank_n() - 1) / proc::rank_n();
    total_size = my_size * proc::rank_n();

    map_ptrs.resize(proc::rank_n());
    if (local::rank_me() == 0) {
      map_ptrs[proc::rank_me()] = new local_map_t();
      map_ptrs[proc::rank_me()]->reserve(my_size);
    }
    for (size_t i = 0; i < proc::rank_n(); ++i) {
      broadcast(map_ptrs[i], i * local::rank_n());
    }
  }

  ~HashMap() {
    arl::barrier();
    if (local::rank_me() == 0) {
      delete map_ptrs[proc::rank_me()];
    }
  }

  Future<bool> insert(Key key, Val val) {
    size_t target_proc = get_target_proc(key);
    return rpc(target_proc * local::rank_n(),
                      [](local_map_t* lmap, Key key, Val val){
                        lmap->insert(key, val);
                        return true; // hashmap will never be full
                      }, map_ptrs[target_proc], key, val);
  }

  Future<Val> find(Key key){
    size_t target_proc = get_target_proc(key);
    return rpc(target_proc * local::rank_n(),
                      [](local_map_t* lmap, Key key)
                          -> Val {
                          Val out;
                        if (lmap->find(key, out)) {
//                            printf("Node %lu find {%lu, %lu}\n", proc::rank_me(), key, out);
                          return out;
                        } else {
//                            printf("Worker %lu cannot find %lu\n", rank_me(), key.hash());
                          return Val();
                        }
                      }, map_ptrs[target_proc], key);
  }

  void insert_ff(Key key, Val val) {
    size_t target_proc = get_target_proc(key);
    rpc_ff(target_proc * local::rank_n(), insert_fn, map_ptrs[target_proc], key, val);
  }

  void register_insert_ffrd() {
    register_amffrd<decltype(insert_fn), local_map_t*, Key, Val>(insert_fn);
  }

  void insert_ffrd(Key key, Val val) {
    size_t target_proc = get_target_proc(key);
    rpc_ffrd(target_proc * local::rank_n(), insert_fn, map_ptrs[target_proc], key, val);
  }

  Future<Val> find_agg(Key key){
    size_t target_proc = get_target_proc(key);
    return rpc_agg(target_proc * local::rank_n(), find_fn, map_ptrs[target_proc], key);
  }

  void register_find_aggrd() const {
    register_amaggrd<decltype(find_fn), local_map_t*, Key>(find_fn);
  }

  Future<Val> find_aggrd(Key key){
    size_t target_proc = get_target_proc(key);
    return rpc_aggrd(target_proc * local::rank_n(), find_fn, map_ptrs[target_proc], key);
  }

 private:
  using local_map_t = libcuckoo::cuckoohash_map<Key, Val, Hash, KeyEqual>;

  static constexpr auto insert_fn = [](local_map_t* lmap, Key key, Val val){
    lmap->insert(key, val);
  };

  static constexpr auto find_fn = [](local_map_t* lmap, Key key) -> Val {
    Val out;
    if (lmap->find(key, out)) {
      return out;
    } else {
      return Val();
    }
  };

  std::vector<local_map_t*> map_ptrs;
  size_t my_size;
  size_t total_size;
  // map the key to a target process
  int get_target_proc(const Key &key) {
    return uint64_t(Hash{}(key) % total_size) % (total_size / my_size);
  }

}; // class HashMap
} // namespace arl

#endif //ARL_HASH_MAP_HPP
