#include <iostream>
#include "kvbench/kvbench.h"
#include "../third-party/CCEH/src/Level_hashing.h"

template<typename Key, typename Value>
class LevelHash;

template<>
class LevelHash<uint64_t, uint64_t> : public kvbench::DB<uint64_t, uint64_t> {
 public:
  LevelHash() : db_(new LevelHashing(10)) {}
  ~LevelHash() { delete db_; }

  int Get(uint64_t key, uint64_t* value) {
    db_->Get(key);
    return 0;
  }

  int Put(uint64_t key, uint64_t value) {
    db_->Insert(key, reinterpret_cast<const char*>(value));
    return 0;
  }

  int Update(uint64_t key,  uint64_t value) {
    return 0;
  }

  int Delete(uint64_t key) {
    db_->Delete(key);
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
  Hash* db_;
  int nr_thread_ = 1;
};

int main(int argc, char** argv) {
  kvbench::Bench<uint64_t, uint64_t>* bench = new kvbench::Bench<uint64_t, uint64_t>(argc, argv);
  kvbench::DB<uint64_t, uint64_t>* db = new LevelHash<uint64_t, uint64_t>();
  bench->SetDB(db);
  bench->Run();
  delete bench;
  return 0;
}