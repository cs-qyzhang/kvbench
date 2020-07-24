#include <iostream>
#include "kvbench/kvbench.h"
#include "src/scaled_kv.h"

using namespace scaledkv;

#define NODEPATH "/mnt/pmem0/persistent"
#define VALUEPATH "/mnt/pmem0/value_persistent"

const uint64_t NVM_NODE_SIZE = 100 * (1ULL << 30);  // 100GB
const uint64_t NVM_VALUE_SIZE = 10 * (1ULL << 30);  // 10GB

template<typename Key, typename Value>
class ComboTree;

template<>
class ComboTree<uint64_t, uint64_t> : public kvbench::DB<uint64_t, uint64_t> {
 public:
  ComboTree() {
    if (AllocatorInit(NODEPATH, NVM_NODE_SIZE, VALUEPATH, NVM_VALUE_SIZE) < 0)
      exit(0);
    db_ = new NVMScaledKV();
    db_->Initialize(10, sizeof(uint64_t), 256);
  }

  ~ComboTree() {
    delete db_;
    AllocatorExit();
  }

  int Get(uint64_t key, uint64_t* value) {
    char keybuf[NVM_KeySize + 1];
    char *pvalue = nullptr;
    fillchar8wirhint64(keybuf, key);
    std::string key_str(keybuf, NVM_KeySize);
    db_->Get(key_str, pvalue);
    return 0;
  }

  int Put(uint64_t key, uint64_t value) {
    char keybuf[NVM_KeySize + 1];
    fillchar8wirhint64(keybuf, key);
    std::string key_str(keybuf, NVM_KeySize);
    char *pvalue = (char*)(key << 9);
    db_->Insert(key_str, pvalue);
    return 0;
  }

  int Update(uint64_t key,  uint64_t value) {
    return 0;
  }

  int Delete(uint64_t key) {
    char keybuf[NVM_KeySize + 1];
    fillchar8wirhint64(keybuf, key);
    std::string key_str(keybuf, NVM_KeySize);
    db_->Delete(key_str);
    return 0;
  }

  int Scan(uint64_t min_key, uint64_t max_key, std::vector<uint64_t>* values) {
    return 0;
  }

  std::string Name() const {
    return "Combo Tree";
  }

 private:
  NVMScaledKV* db_;
};

int main(int argc, char** argv) {
  kvbench::Bench<uint64_t, uint64_t>* bench = new kvbench::Bench<uint64_t, uint64_t>(argc, argv);
  kvbench::DB<uint64_t, uint64_t>* db = new ComboTree<uint64_t, uint64_t>();
  bench->SetDB(db);
  bench->Run();
  delete bench;
  return 0;
}