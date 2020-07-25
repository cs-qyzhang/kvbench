#include <iostream>
#include "kvbench/kvbench.h"

#include "../../examples/libpmemobj_cpp_examples_common.hpp"
#include "../polymorphic_string.h"
#include "../profile.hpp"
#include <libpmemobj++/experimental/clevel_hash.hpp>

#define LAYOUT "clevel_hash"
#define KEY_LEN 15
#define HASH_POWER 12

namespace nvobj = pmem::obj;

#define PATH "/mnt/pmem0/persistent"

const uint64_t NVM_NODE_SIZE = 100 * (1ULL << 30);  // 100GB
const uint64_t NVM_VALUE_SIZE = 10 * (1ULL << 30);  // 10GB

typedef nvobj::experimental::clevel_hash<uint64_t, uint64_t>
  persistent_map_type;

struct root {
  nvobj::persistent_ptr<persistent_map_type> cons;
};

template<typename Key, typename Value>
class ClevelHash;

template<>
class ClevelHash<uint64_t, uint64_t> : public kvbench::DB<uint64_t, uint64_t> {
 public:
  ClevelHash() {
    remove(PATH); // delete the mapped file.

    pop_ = nvobj::pool<root>::create(
      PATH, LAYOUT, PMEMOBJ_MIN_POOL * 256, S_IWUSR | S_IRUSR);
    auto proot = pop_.root();

    {
      nvobj::transaction::manual tx(pop_);

      proot->cons = nvobj::make_persistent<persistent_map_type>();
      proot->cons->set_thread_num(2);

      nvobj::transaction::commit();
    }

    db_ = proot->cons;
  }

  ~ClevelHash() {}

  int Get(uint64_t key, uint64_t* value) {
    db_->search(persistent_map_type::key_type(key));
    return 0;
  }

  int Put(uint64_t key, uint64_t value) {
    db_->insert(persistent_map_type::value_type(key, value), 1, 0);
    return 0;
  }

  int Update(uint64_t key,  uint64_t value) {
    return 0;
  }

  int Delete(uint64_t key) {
    db_->erase(persistent_map_type::key_type(key), 0);
    return 0;
  }

  int Scan(uint64_t min_key, uint64_t max_key, std::vector<uint64_t>* values) {
    return 0;
  }

  int GetThreadNumber() const {
    if (get_phase_count_)
      return get_phase_count_ * 2;
    else
      return 1;
  }

  void PhaseEnd(kvbench::Operation op, size_t size) {
    get_phase_count_++;

    nvobj::transaction::manual tx(pop_);
		db_->set_thread_num(get_phase_count_ * 2 + 1);
		nvobj::transaction::commit();
  }

  std::string Name() const {
    return "Clevel Hashing";
  }

 private:
  pmem::obj::persistent_ptr<persistent_map_type> db_;
  nvobj::pool<root> pop_;
  int get_phase_count_ = 0;
};

int main(int argc, char** argv) {
  kvbench::Bench<uint64_t, uint64_t>* bench = new kvbench::Bench<uint64_t, uint64_t>(argc, argv);
  kvbench::DB<uint64_t, uint64_t>* db = new ClevelHash<uint64_t, uint64_t>();
  bench->SetDB(db);
  bench->Run();
  delete bench;
  return 0;
}