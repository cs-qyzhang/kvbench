#include <iostream>
#include "kvbench/kvbench.h"
#include "../third-party/FAST_FAIR/btree.h"

using namespace fastfair;

template<typename Key, typename Value>
class FastFair;

template<>
class FastFair<uint64_t, uint64_t> : public kvbench::DB<uint64_t, uint64_t> {
 public:
  FastFair() : db_(new btree()) {}
  ~FastFair() { delete db_; }

  int Get(uint64_t key, uint64_t* value) {
    db_->btree_search(key);
    return 0;
  }

  int Put(uint64_t key, uint64_t value) {
    db_->btree_insert(key, (char *)value);
    return 0;
  }

  int Update(uint64_t key,  uint64_t value) {
    return 0;
  }

  int Delete(uint64_t key) {
    db_->btree_delete(key);
    return 0;
  }

  int Scan(uint64_t min_key, uint64_t max_key, std::vector<uint64_t>* values) {
    return 0;
  }

  std::string Name() const {
    return "Fast Fair";
  }

  int GetThreadNumber() const {
    return 1;
  }

  void SetThreadNumber(int thread_num) {}

 private:
  btree* db_;
};

int main(int argc, char** argv) {
  kvbench::DB<uint64_t, uint64_t>* db = new FastFair<uint64_t, uint64_t>();
  kvbench::Bench<uint64_t, uint64_t>* bench = new kvbench::Bench<uint64_t, uint64_t>(db);
  bench->Run(argc, argv);
  delete bench;
  return 0;
}