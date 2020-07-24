#pragma once

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <fstream>
#include <functional>
#include <iostream>
#include <numeric>
#include <thread>
#include <vector>
#include <iterator>

#include "kvbench.pb.cc"
#include "kvbench.pb.h"
#include "random.h"

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
      os << "LOAD";
      break;
    case Operation::PUT:
      os << "PUT";
      break;
    case Operation::GET:
      os << "GET";
      break;
    case Operation::UPDATE:
      os << "UPDATE";
      break;
    case Operation::DELETE:
      os << "DELETE";
      break;
    case Operation::SCAN:
      os << "SCAN";
      break;
    default:
      os << "ERROR";
      break;
  }
  return os;
}

template <typename Key, typename Value>
class Bench;

namespace {  // anonymous namespace

thread_local int thread_id_ = 0;

template <typename Key, typename Value>
struct TestPhase {
  TestPhase(Operation op, size_t size, Random<Key>* random_key,
            Random<Value>* random_value, int test_threads, bool record_latency)
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
  void Start() { start_ = std::chrono::high_resolution_clock::now(); }

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
    phases_.emplace_back(op, size, new RandomDefault<Key>(),
                         new RandomDefault<Value>(), test_threads_,
                         record_latency_);
  }

  void Append(Operation op, size_t size, int test_threads) {
    phases_.emplace_back(op, size, new RandomDefault<Key>(),
                         new RandomDefault<Value>(), test_threads,
                         record_latency_);
  }

  void Append(Operation op, size_t size, int test_threads,
              bool record_latency) {
    phases_.emplace_back(op, size, new RandomDefault<Key>(),
                         new RandomDefault<Value>(), test_threads,
                         record_latency);
  }

  void Append(Operation op, size_t size, Random<Key>* random_key,
              Random<Value>* random_value, int test_threads,
              bool record_latency) {
    phases_.emplace_back(op, size, random_key, random_value, test_threads,
                         record_latency);
  }

  void SetThreadNum(unsigned int nr_thread) { test_threads_ = nr_thread; }

 private:
  std::vector<TestPhase<Key, Value>> phases_;
  int test_threads_ = 1;    // TODO: deprecated
  bool record_latency_ = true;

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

  int GetThreadNumber() const { return nr_thread_; };

  void SetThreadNumber(int nr_thread) { nr_thread_ = nr_thread; }

  int GetThreadId() const { return thread_id_; }

  void SetThreadId(int thread_id) { thread_id_ = thread_id; }

 private:
  int nr_thread_;
};

template <typename Key, typename Value>
class Bench {
 public:
  Bench(int argc, char** argv)
      : options_(new Options<Key, Value>()), nr_thread_(1) {
    ParseArguments_(argc, argv);
    GOOGLE_PROTOBUF_VERIFY_VERSION;
  }

  ~Bench() {
    delete db_;
    google::protobuf::ShutdownProtobufLibrary();
  }

  void SetDB(DB<Key, Value>* db) {
    db_ = db;
    db_->SetThreadNumber(nr_thread_);
  }

  void Run() { Run_(); }

  void PrintStats() const {
    if (stats_.stat_size() < 2) return;
    std::cout << std::endl
              << "============================== STATICS "
                 "=============================="
              << std::endl
              << "DB name:            " << db_->Name() << std::endl
              << "Total run time (s): " << stats_.stat(0).duration() / 1000000.0 << std::endl;

    for (size_t i = 0; i < options_->phases_.size(); ++i) {
      auto stat = stats_.stat(i + 1);
      std::cout << std::endl
                << "-------------------- PHASE " << i + 1 << ": "
                << options_->phases_[i].op << "--------------------"
                << std::endl
                << "  "
                << "Run time (s):         " << stat.duration() / 1000000.0 << std::endl
                << "  "
                << "Total:                " << stat.total() << std::endl
                << "  "
                << "Average latency (us): " << stat.average_latency() << std::endl
                << "  "
                << "Maximum latency (us): " << stat.max_latency() << std::endl
                << "  "
                << "Throughput (ops/s):   " << stat.throughput() << std::endl;
    }

    std::cout << "============================ END STATICS "
                 "============================"
              << std::endl;
  }

  bool DumpStatistics(std::string file_path = "kvbench.proto.dat") {
    std::fstream output(file_path,
                        std::ios::out | std::ios::trunc | std::ios::binary);
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
  int nr_thread_;
  std::vector<double> total_latency_;
  std::vector<double> max_latency_;

  static Operation ToOperation_(char* str) {
    if (strcmp(str, "LOAD") == 0)   return Operation::LOAD;
    if (strcmp(str, "PUT") == 0)    return Operation::PUT;
    if (strcmp(str, "GET") == 0)    return Operation::GET;
    if (strcmp(str, "UPDATE") == 0) return Operation::UPDATE;
    if (strcmp(str, "DELETE") == 0) return Operation::DELETE;
    if (strcmp(str, "SCAN") == 0)   return Operation::SCAN;
    return Operation::ERROR;
  }

  void ParseArguments_(int argc, char** argv) {
    for (int i = 1; i < argc; ++i) {
      Operation op;
      if (strcmp(argv[i], "are-you-kvbench") == 0) {
        std::cout << "YES!" << std::endl;
        exit(0);
      } else if (strcmp(argv[i], "-thread") == 0) {
        if (i == argc - 1) {
          std::cout << "ERROR! -thread argument must follow a number!" << std::endl;
          exit(0);
        }
        size_t nr_thread = std::stoi(argv[i + 1]);
        nr_thread_ = nr_thread;
        options_->SetThreadNum(nr_thread);
        i++;
      } else if ((op = ToOperation_(argv[i])) != Operation::ERROR) {
        if (i == argc - 1) {
          std::cout << "ERROR! " << op << " argument must follow a number!" << std::endl;
          exit(0);
        }
        size_t size = std::stoi(argv[i + 1]);
        options_->Append(op, size);
        i++;
      }
    }
  }

  void CaculateStatistic_() {
    if (stats_.stat_size() < 2) return;
    size_t total_op = 0;
    double latency_sum = 0.0;
    for (int i = 1; i < stats_.stat_size(); ++i) {
      auto stat = stats_.mutable_stat(i);
      stat->set_average_latency(total_latency_[i - 1] / stat->total());
      stat->set_max_latency(max_latency_[i - 1]);
      stat->set_throughput(stat->total() / stat->duration() * 1000000);
      total_op += stat->total();
      latency_sum += stat->average_latency();
    }
    auto stat = stats_.mutable_stat(0);
    stat->set_throughput(total_op / stat->duration() * 1000000);
    stat->set_max_latency(
        *std::max_element(max_latency_.cbegin(), max_latency_.cend()));
    stat->set_average_latency(latency_sum / (stats_.stat_size() - 1));
  }

  void Run_() {
    Stat* stat = stats_.add_stat();
    double run_time = 0.0;
    for (auto& phase : options_->phases_) run_time += RunPhase_(phase);
    stat->set_duration(run_time);
    CaculateStatistic_();
    PrintStats();
    DumpStatistics();
  }

  void RunPhaseMain_(int thread_id, TestPhase<Key, Value>& phase,
                     size_t test_size, double& total_latency,
                     double& max_latency,
                     google::protobuf::RepeatedField<double>& latencys,
                     int sample_interval) {
    db_->SetThreadId(thread_id);

    double latency;
    Timer latency_timer;
    max_latency = 0.0;
    total_latency = 0.0;

#define KVBENCH_RECORD_START \
  do {                       \
    latency_timer.Start();   \
  } while (0)

#define KVBENCH_RECORD_END                                   \
  do {                                                       \
    latency = latency_timer.End();                           \
    total_latency += latency;                                \
    max_latency = std::max(max_latency, latency);            \
    if (i % sample_interval == 0) *latencys.Add() = latency; \
  } while (0)

    if (phase.op == Operation::LOAD || phase.op == Operation::PUT) {
      for (size_t i = 0; i < test_size; ++i) {
        Key key = phase.random_key->Next();
        Value value = phase.random_value->Next();
        KVBENCH_RECORD_START;
        db_->Put(key, value);
        KVBENCH_RECORD_END;
      }
    } else if (phase.op == Operation::GET) {
      for (size_t i = 0; i < test_size; ++i) {
        Key key = phase.random_key->Next();
        Value value;
        KVBENCH_RECORD_START;
        db_->Get(key, &value);
        KVBENCH_RECORD_END;
      }
    } else if (phase.op == Operation::UPDATE) {
      for (size_t i = 0; i < test_size; ++i) {
        Key key = phase.random_key->Next();
        Value value = phase.random_value->Next();
        KVBENCH_RECORD_START;
        db_->Update(key, value);
        KVBENCH_RECORD_END;
      }
    } else if (phase.op == Operation::DELETE) {
      for (size_t i = 0; i < test_size; ++i) {
        Key key = phase.random_key->Next();
        KVBENCH_RECORD_START;
        db_->Delete(key);
        KVBENCH_RECORD_END;
      }
    } else if (phase.op == Operation::SCAN) {
      for (size_t i = 0; i < test_size; ++i) {
        Key min_key = phase.random_key->Next();
        Key max_key = phase.random_key->Next();  // TODO
        std::vector<Value> values;
        KVBENCH_RECORD_START;
        db_->Scan(min_key, max_key, &values);  // TODO
        KVBENCH_RECORD_END;
      }
    } else {
      assert(0);
    }

#undef KVBENCH_RECORD_START
#undef KVBENCH_RECORD_END
  }  // namespace kvbench

  double RunPhase_(TestPhase<Key, Value>& phase) {
    Stat* stat = stats_.add_stat();
    stat->set_total(phase.size);
    Timer timer;
    int sample_interval = phase.size < 2000000 ? 1 : phase.size / 2000000;

    google::protobuf::RepeatedField<double>* latencys =
        new google::protobuf::RepeatedField<double>[nr_thread_];
    std::vector<double> total_latency;
    std::vector<double> max_latency;
    total_latency.resize(nr_thread_);
    max_latency.resize(nr_thread_);

    timer.Start();

    std::vector<std::thread> test_threads;
    for (int thread_id = 0; thread_id < nr_thread_; ++thread_id) {
      size_t test_size;
      if (thread_id != nr_thread_ - 1)
        test_size = phase.size / nr_thread_;
      else
        test_size = phase.size - (phase.size / nr_thread_) *
                                     (nr_thread_ - 1);
      test_threads.emplace_back(&Bench::RunPhaseMain_, this,
                                thread_id, std::ref(phase), test_size,
                                std::ref(total_latency[thread_id]),
                                std::ref(max_latency[thread_id]),
                                std::ref(latencys[thread_id]), sample_interval);
    }

    for (auto&& test_thread : test_threads)
      if (test_thread.joinable()) test_thread.join();

    double run_time = timer.End();
    stat->set_duration(run_time);

    auto stat_latency = stat->mutable_latency();
    for (int i = 0; i < nr_thread_; ++i)
      stat_latency->MergeFrom(latencys[i]);

    total_latency_.push_back(
        std::accumulate(total_latency.cbegin(), total_latency.cend(), 0.0));
    max_latency_.push_back(
        *std::max_element(max_latency.cbegin(), max_latency.cend()));

    return run_time;
  }
};

}  // namespace kvbench