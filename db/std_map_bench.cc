#include <iostream>
#include <map>
#include "kvbench.h"

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
  Options<int, int>* options = new Options<int, int>(2, true);
  options->Append(Operation::LOAD, 1000000);
  options->Append(Operation::PUT, 1000000);
  options->Append(Operation::GET, 1000000);
  options->Append(Operation::DELETE, 1000000);

  DB<int, int>* db = new Map<int, int>();

  Bench<int, int>* bench = new Bench<int, int>(db, options);
  bench->Run();
  bench->PrintStats();
  bench->GeneratePDF();
  bench->GenerateHTML();

  delete bench;

  return 0;
}