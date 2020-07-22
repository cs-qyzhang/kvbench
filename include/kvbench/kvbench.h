#pragma once

#include <cassert>
#include <cstddef>
#include <vector>
#include <iostream>
#include <numeric>
#include <algorithm>
#include <fstream>

#include "random.h"
#include "kvbench.pb.h"
#include "kvbench.pb.cc"

namespace kvbench {

enum class Operation {
  LOAD,
  PUT,
  GET,
  UPDATE,
  DELETE,
  SCAN,
  ERROR,
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
      os << "ERROR"; break;
  }
  return os;
}

template <typename Key, typename Value>
class Bench;

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
    return duration.count() * 1000000;
  }

 private:
  std::chrono::time_point<std::chrono::high_resolution_clock> start_;
  std::chrono::time_point<std::chrono::high_resolution_clock> end_;
};

template <typename Key, typename Value>
class Options {
 public:
  Options(int default_test_threads = 1, bool default_record_latency = true)
      : test_threads_(default_test_threads),
        record_latency_(default_record_latency) {}

  void Append(Operation op, size_t size) {
    phases_.emplace_back(op, size,
                         new RandomDefault<Key>(),
                         new RandomDefault<Value>(),
                         test_threads_,
                         record_latency_);
  }

  void Append(Operation op, size_t size, int test_threads) {
    phases_.emplace_back(op, size,
                         new RandomDefault<Key>(),
                         new RandomDefault<Value>(),
                         test_threads, record_latency_);
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

  void SetThreadNum(unsigned int nr_thread) {
    test_threads_ = nr_thread;
  }

 private:
  std::vector<TestPhase<Key, Value>> phases_;
  bool record_latency_ = true;
  int test_threads_ = 1;

  friend class Bench<Key, Value>;
};

}  // anonymous namespace

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
  Bench(DB<Key, Value>* db) : db_(db), options_(new Options<Key, Value>()) {
    GOOGLE_PROTOBUF_VERIFY_VERSION;
  }

  ~Bench() {
    delete db_;
    google::protobuf::ShutdownProtobufLibrary();
  }

  void Run(int argc, char** argv) {
    ParseArguments_(argc, argv);
    Run_();
  }

  void PrintStats() const {
    if (stats_.stat_size() < 2)
      return;
    std::cout << std::endl
      << "============================== STATICS ==============================" << std::endl
      << "DB name:        " << db_->Name() << std::endl
      << "Total run time: " << stats_.stat(0).duration() << std::endl;

    for (int i = 0; i < options_->phases_.size(); ++i) {
      auto stat = stats_.stat(i + 1);
      std::cout << std::endl
        << "-------------------- PHASE " << i + 1 << ": " << options_->phases_[i].op << "--------------------" << std::endl
        << "  " << "Run time:        " << stat.duration() << std::endl
        << "  " << "Total:           " << stat.total() << std::endl
        << "  " << "Average latency: " << stat.average_latency() << std::endl
        << "  " << "Maximum latency: " << stat.max_latency() << std::endl
        << "  " << "Throughput:      " << stat.throughput() << std::endl;
    }

    std::cout << "============================ END STATICS ============================" << std::endl;
  }

  bool DumpStatistics(std::string file_path = "kvbench.proto.dat") {
    std::fstream output(file_path, std::ios::out | std::ios::trunc | std::ios::binary);
    if (!stats_.SerializeToOstream(&output)) {
      std::cerr << "Failed to write " << file_path << "!" << std::endl;
      return false;
    }
    return true;
  }

  Options<Key, Value>* GetOptions() const { return options_; }

  const Stat& GetStats(int phase) const {
    if (phase >= stats_.stat_size()) {
      std::cerr << "GetStats: invalid index!" << std::endl;
      exit(-1);
    }
    return stats_.stat(phase);
  }

 private:
  DB<Key, Value>* db_;
  Options<Key, Value>* options_;
  Stats stats_;
  std::vector<double> total_latency_;
  std::vector<double> max_latency_;

  static Operation ToOperation_(char* str) {
    if (strcmp(str, "LOAD") == 0) return Operation::LOAD;
    if (strcmp(str, "PUT") == 0) return Operation::PUT;
    if (strcmp(str, "GET") == 0) return Operation::GET;
    if (strcmp(str, "UPDATE") == 0) return Operation::UPDATE;
    if (strcmp(str, "DELETE") == 0) return Operation::DELETE;
    if (strcmp(str, "SCAN") == 0) return Operation::SCAN;
    return Operation::ERROR;
  }

  void ParseArguments_(int argc, char** argv) {
    for (int i = 1; i < argc; ++i) {
      Operation op;
      if ((op = ToOperation_(argv[i])) != Operation::ERROR) {
        assert(i + 1 < argc);
        size_t size = std::stoi(argv[i + 1]);
        options_->Append(op, size);
        i++;
      } else if (strcmp(argv[i], "-thread") == 0) {
        assert(i + 1 < argc);
        size_t nr_thread = std::stoi(argv[i + 1]);
        options_->SetThreadNum(nr_thread);
        i++;
      } else if (strcmp(argv[i], "are-you-kvbench") == 0) {
        std::cout << "YES!" << std::endl;
        exit(0);
      }
    }
  }

  void CaculateStatistic_() {
    if (stats_.stat_size() < 2)
      return;
    size_t total_op = 0;
    double latency_sum = 0.0;
    for (int i = 1; i < stats_.stat_size(); ++i) {
      auto stat = stats_.mutable_stat(i);
      auto latency = stat->mutable_latency();
      stat->set_average_latency(total_latency_[i - 1] / stat->total());
      stat->set_max_latency(max_latency_[i - 1]);
      stat->set_throughput(stat->total() / stat->duration() * 1000000);
      total_op += stat->total();
      latency_sum += stat->average_latency();
    }
    auto stat = stats_.mutable_stat(0);
    stat->set_throughput(total_op / stat->duration() * 1000000);
    stat->set_max_latency(*std::max_element(max_latency_.cbegin(), max_latency_.cend()));
    stat->set_average_latency(latency_sum / (stats_.stat_size() - 1));
  }

  void Run_() {
    Stat* stat = stats_.add_stat();
    Timer run;
    run.Start();
    for (auto& phase : options_->phases_)
      RunPhase_(phase);
    stat->set_duration(run.End());
    CaculateStatistic_();
    PrintStats();
    DumpStatistics();
  }

  void RunPhase_(TestPhase<Key, Value>& phase) {
    Stat* stat = stats_.add_stat();
    stat->set_total(phase.size);
    Timer run_time;
    Timer latency_timer;
    int sample_interval = phase.size < 2000000 ? 1 : phase.size / 2000000;
    double latency;
    double total_latency = 0.0;
    double max_latency = 0.0;

    run_time.Start();
    if (phase.op == Operation::LOAD || phase.op == Operation::PUT) {
      for (int i = 0; i < phase.size; ++i) {
        latency_timer.Start();
        db_->Put(phase.random_key->Next(), phase.random_value->Next());
        latency = latency_timer.End();
        total_latency += latency;
        max_latency = std::max(max_latency, latency);
        if (i % sample_interval == 0)
          stat->add_latency(latency_timer.End());
      }
    } else if (phase.op == Operation::GET) {
      for (int i = 0; i < phase.size; ++i) {
        latency_timer.Start();
        Value value;
        db_->Get(phase.random_key->Next(), &value);
        latency = latency_timer.End();
        total_latency += latency;
        max_latency = std::max(max_latency, latency);
        if (i % sample_interval == 0)
          stat->add_latency(latency_timer.End());
      }
    } else if (phase.op == Operation::UPDATE) {
      for (int i = 0; i < phase.size; ++i) {
        latency_timer.Start();
        db_->Update(phase.random_key->Next(), phase.random_value->Next());
        latency = latency_timer.End();
        total_latency += latency;
        max_latency = std::max(max_latency, latency);
        if (i % sample_interval == 0)
          stat->add_latency(latency_timer.End());
      }
    } else if (phase.op == Operation::DELETE) {
      for (int i = 0; i < phase.size; ++i) {
        latency_timer.Start();
        db_->Delete(phase.random_key->Next());
        latency = latency_timer.End();
        total_latency += latency;
        max_latency = std::max(max_latency, latency);
        if (i % sample_interval == 0)
          stat->add_latency(latency_timer.End());
      }
    } else if (phase.op == Operation::SCAN) {
      for (int i = 0; i < phase.size; ++i) {
        latency_timer.Start();
        Key min_key = phase.random_key->Next();
        Key max_key = phase.random_key->Next();
        std::vector<Value> values;
        db_->Scan(min_key, max_key, &values);  // TODO
        latency = latency_timer.End();
        total_latency += latency;
        max_latency = std::max(max_latency, latency);
        if (i % sample_interval == 0)
          stat->add_latency(latency_timer.End());
      }
    } else {
      assert(0);
    }
    stat->set_duration(run_time.End());
    total_latency_.push_back(total_latency);
    max_latency_.push_back(max_latency);
  }
};

}  // namespace kvbench