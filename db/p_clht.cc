#include <iostream>
#include "kvbench/kvbench.h"
#include "../P-CLHT/include/clht.h"

template<typename Key, typename Value>
class CLHT;

template<>
class CLHT<uint64_t, uint64_t> : public kvbench::DB<uint64_t, uint64_t> {
 public:
  CLHT() : db_(clht_create(512)) {
    clht_gc_thread_init(db_, 0);
  }

  ~CLHT() { clht_gc_destroy(db_); }

  int Get(uint64_t key, uint64_t* value) {
    clht_get(db_->ht, key);
    return 0;
  }

  int Put(uint64_t key, uint64_t value) {
    clht_put(db_, key, value);
    return 0;
  }

  int Update(uint64_t key,  uint64_t value) {
    return 0;
  }

  int Delete(uint64_t key) {
    clht_remove(db_, key);
    return 0;
  }

  int Scan(uint64_t min_key, std::vector<uint64_t>* values) {
    return 0;
  }

  std::string Name() const {
    return "Level Hashing";
  }

  int GetThreadNumber() const {
    return nr_thread_;
  }

  void PhaseEnd(kvbench::Operation op, size_t size) {
    if (op == kvbench::Operation::LOAD)
      nr_thread_ = 1;
    else
      nr_thread_ *= 2;
  }

 private:
  clht_t* db_;
  int nr_thread_;
};

int main(int argc, char** argv) {
  kvbench::Bench<uint64_t, uint64_t>* bench = new kvbench::Bench<uint64_t, uint64_t>(argc, argv);
  kvbench::DB<uint64_t, uint64_t>* db = new CLHT<uint64_t, uint64_t>();
  bench->SetDB(db);
  bench->Run();
  delete bench;
  return 0;
}