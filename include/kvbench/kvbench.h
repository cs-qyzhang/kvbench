#pragma once

#include <cassert>
#include <cstddef>
#include <vector>
#include <iostream>
#include <numeric>
#include <algorithm>

#include "random.h"

namespace kvbench {

const std::string DEFAULT_SERVER = "http://jianyue.tech";

enum class Operation {
  LOAD,
  PUT,
  GET,
  UPDATE,
  DELETE,
  SCAN,
};

std::ostream& operator<<(std::ostream& os, const Operation& op) {
  switch (op) {
    case Operation::LOAD:
      os << "LOAD"; break;
    case Operation::PUT:
      os << "PUT"; break;
    case Operation::GET:
      os << "GET"; break;
    case Operation::UPDATE:
      os << "UPDATE"; break;
    case Operation::DELETE:
      os << "DELETE"; break;
    case Operation::SCAN:
      os << "SCAN"; break;
    default:
      break;
  }
  return os;
}


struct Stat {
  std::vector<double>* latency;
  double average_latency;
  double max_latency;
  double duration;
  double throughput;
  size_t success;
  size_t failed;
  size_t total;
};

namespace {  // anonymous namespace

template <typename Key, typename Value>
struct TestPhase {
  TestPhase(Operation op, size_t size,
            Random<Key>* random_key, Random<Value>* random_value,
            int test_threads, bool record_latency)
      : op(op),
        size(size),
        random_key(random_key),
        random_value(random_value),
        test_threads(test_threads),
        record_latency(record_latency) {}
  Operation op;
  size_t size;
  Random<Key>* random_key;
  Random<Value>* random_value;
  int test_threads;
  bool record_latency;
};

class Timer {
 public:
  void Start() {
    start_ = std::chrono::high_resolution_clock::now();
  }

  double End() {
    end_ = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> duration = end_ - start_;
    return duration.count();
  }

 private:
  std::chrono::time_point<std::chrono::high_resolution_clock> start_;
  std::chrono::time_point<std::chrono::high_resolution_clock> end_;
};

}  // anonymous namespace

template <typename Key, typename Value>
class Bench;

template <typename Key, typename Value>
class Options {
 public:
  Options(int default_test_threads = 1, bool default_record_latency = true)
      : default_test_threads_(default_test_threads),
        default_record_latency_(default_record_latency) {}

  void Append(Operation op, size_t size) {
    phases_.emplace_back(op, size,
                         new RandomDefault<Key>(),
                         new RandomDefault<Value>(),
                         default_test_threads_,
                         default_record_latency_);
  }

  void Append(Operation op, size_t size, int test_threads) {
    phases_.emplace_back(op, size,
                         new RandomDefault<Key>(),
                         new RandomDefault<Value>(),
                         test_threads, default_record_latency_);
  }

  void Append(Operation op, size_t size, int test_threads,
              bool record_latency) {
    phases_.emplace_back(op, size,
                         new RandomDefault<Key>(),
                         new RandomDefault<Value>(),
                         test_threads, record_latency);
  }

  void Append(Operation op, size_t size,
              Random<Key>* random_key, Random<Value>* random_value,
              int test_threads, bool record_latency) {
    phases_.emplace_back(op, size,
                         random_key, random_value,
                         test_threads, record_latency);
  }

 private:
  std::vector<TestPhase<Key, Value>> phases_;
  bool default_record_latency_ = true;
  int default_test_threads_ = 1;

  friend class Bench<Key, Value>;
};

template <typename Key, typename Value>
class DB {
 public:
  virtual ~DB() {}

  virtual int Get(Key key, Value* value) = 0;

  virtual int Put(Key key, Value value) = 0;

  virtual int Update(Key key, Value value) = 0;

  virtual int Delete(Key key) = 0;

  virtual int Scan(Key min_key, Key max_key, std::vector<Value>* values) = 0;

  virtual std::string Name() const = 0;

  virtual int GetThreadNumber() const = 0;

  virtual void SetThreadNumber(int thread_num) = 0;
};

template <typename Key, typename Value>
class Bench {
 public:
  Bench(DB<Key, Value>* db, Options<Key, Value>* option)
      : db_(db), options_(option) {}

  ~Bench() {
    delete db_;
  }

  void Run() {
    Stat* stat = new Stat();
    stats_.push_back(stat);
    Timer run;
    run.Start();
    for (auto& phase : options_->phases_)
      RunPhase_(phase);
    stat->duration = run.End();
    CaculateStatistic_();
  }

  void PrintStats() const {
    std::cout << std::endl
      << "============================== STATICS ==============================" << std::endl
      << "DB name:        " << db_->Name() << std::endl
      << "Total run time: " << stats_[0]->duration << std::endl;

    for (int i = 0; i < options_->phases_.size(); ++i) {
      std::cout << std::endl
        << "-------------------- PHASE " << i + 1 << ": " << options_->phases_[i].op << "--------------------" << std::endl
        << "  " << "Run time:        " << stats_[i + 1]->duration << std::endl
        << "  " << "Average latency: " << stats_[i + 1]->average_latency << std::endl
        << "  " << "Maximum latency: " << stats_[i + 1]->max_latency << std::endl
        << "  " << "Throughput:      " << stats_[i + 1]->throughput << std::endl;
    }

    std::cout << "============================ END STATICS ============================" << std::endl;
  }

  void GeneratePDF(std::string URL = DEFAULT_SERVER) {}

  void GenerateHTML(std::string URL = DEFAULT_SERVER) {}

  Options<Key, Value>* GetOptions() const { return options_; }

  Stat* GetStats(int phase) const {
    if (phase >= stats_.size())
      return nullptr;
    return stats_[phase];
  }

 private:
  DB<Key, Value>* db_;
  Options<Key, Value>* options_;
  std::vector<Stat*> stats_;

  void CaculateStatistic_() {
    for (int i = 1; i < stats_.size(); ++i) {
      auto& latency = stats_[i]->latency;
      stats_[i]->average_latency =
        std::accumulate(latency->begin(), latency->end(), 0.0) / stats_[i]->total;
      stats_[i]->max_latency = *std::max_element(latency->begin(), latency->end());
      stats_[i]->throughput = stats_[i]->total / stats_[i]->duration;
    }
  }

  void RunPhase_(TestPhase<Key, Value>& phase) {
    Stat* stat = new Stat();
    stat->latency = new std::vector<double>();
    stat->total = phase.size;
    Timer run_time;
    Timer latency_timer;

    run_time.Start();
    if (phase.op == Operation::LOAD || phase.op == Operation::PUT) {
      for (int i = 0; i < phase.size; ++i) {
        latency_timer.Start();
        db_->Put(phase.random_key->Next(), phase.random_value->Next());
        stat->latency->push_back(latency_timer.End());
      }
    } else if (phase.op == Operation::GET) {
      for (int i = 0; i < phase.size; ++i) {
        latency_timer.Start();
        Value value;
        db_->Get(phase.random_key->Next(), &value);
        stat->latency->push_back(latency_timer.End());
      }
    } else if (phase.op == Operation::UPDATE) {
      for (int i = 0; i < phase.size; ++i) {
        latency_timer.Start();
        db_->Update(phase.random_key->Next(), phase.random_value->Next());
        stat->latency->push_back(latency_timer.End());
      }
    } else if (phase.op == Operation::DELETE) {
      for (int i = 0; i < phase.size; ++i) {
        latency_timer.Start();
        db_->Delete(phase.random_key->Next());
        stat->latency->push_back(latency_timer.End());
      }
    } else if (phase.op == Operation::SCAN) {
      for (int i = 0; i < phase.size; ++i) {
        latency_timer.Start();
        Key min_key = phase.random_key->Next();
        std::vector<Value> values;
        db_->Scan(min_key, min_key + 1000, &values);  // TODO
        stat->latency->push_back(latency_timer.End());
      }
    } else {
      assert(0);
    }
    stat->duration = run_time.End();

    stats_.push_back(stat);
  }
};

}  // namespace kvbench