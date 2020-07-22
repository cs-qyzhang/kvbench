#include <iostream>
#include <map>
#include "kvbench/kvbench.h"

using namespace kvbench;

template<typename Key, typename Value>
class Map : public kvbench::DB<Key, Value> {
 public:
  int Get(Key key, Value* value) {
    auto iter = map_.find(key);
    if (iter == map_.end())
      return false;
    else
      *value = iter->second;
    return true;
  }

  int Put(Key key, Value value) {
    map_.insert(std::pair<Key, Value>(key, value));
    return true;
  }

  int Update(Key key,  Value value) {
    return 0;
  }

  int Delete(Key key) {
    map_.erase(key);
    return 0;
  }

  int Scan(Key min_key, Key max_key, std::vector<Value>* values) {
    return 0;
  }

  std::string Name() const {
    return "std::map";
  }

  int GetThreadNumber() const {
    return 1;
  }

  void SetThreadNumber(int thread_num) {}

 private:
  std::map<Key, Value> map_;
};

int main(int argc, char** argv) {
  Bench<uint64_t, uint64_t>* bench = new Bench<uint64_t, uint64_t>(argc, argv);
  DB<uint64_t, uint64_t>* db = new Map<uint64_t, uint64_t>();
  bench->SetDB(db);
  bench->Run();
  delete bench;
  return 0;
}