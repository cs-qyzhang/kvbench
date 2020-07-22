#pragma once

#include <random>
#include <chrono>

namespace kvbench {

template<typename T>
class Random {
 public:
  virtual ~Random() {}

  virtual T Next() = 0;
};

class RandomUniformInt : public Random<int> {
 public:
  RandomUniformInt() : dist_() {
    // https://stackoverflow.com/a/13446015/7640227
    std::random_device rd;
    // seed value is designed specifically to make initialization
    // parameters of std::mt19937 (instance of std::mersenne_twister_engine<>)
    // different across executions of application
    std::mt19937::result_type seed = rd() ^ (
            (std::mt19937::result_type)
            std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()
                ).count() +
            (std::mt19937::result_type)
            std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::high_resolution_clock::now().time_since_epoch()
                ).count() );
    gen_.seed(seed);
  }

  int Next() {
    return dist_(gen_);
  }

 private:
  std::mt19937 gen_;
  std::uniform_int_distribution<int> dist_;
};

class RandomUniformUint64 : public Random<int> {
 public:
  RandomUniformUint64() : dist_() {
    // https://stackoverflow.com/a/13446015/7640227
    std::random_device rd;
    // seed value is designed specifically to make initialization
    // parameters of std::mt19937 (instance of std::mersenne_twister_engine<>)
    // different across executions of application
    std::mt19937_64::result_type seed = rd() ^ (
            (std::mt19937_64::result_type)
            std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()
                ).count() +
            (std::mt19937_64::result_type)
            std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::high_resolution_clock::now().time_since_epoch()
                ).count() );
    gen_.seed(seed);
  }

  int Next() {
    return dist_(gen_);
  }

 private:
  std::mt19937_64 gen_;
  std::uniform_int_distribution<uint64_t> dist_;
};

template<typename T>
class RandomDefault {};

template<>
class RandomDefault<int> : public Random<int> {
 public:
  int Next() { return rnd_.Next(); }

 private:
  RandomUniformInt rnd_;
};

template<>
class RandomDefault<uint64_t> : public Random<uint64_t> {
 public:
  uint64_t Next() { return rnd_.Next(); }

 private:
  RandomUniformUint64 rnd_;
};

template<>
class RandomDefault<std::string> : public Random<std::string> {
 public:
  std::string Next() { return "test"; }

};

} // namespace kvbench