#pragma once

#ifdef OB_USE_EXTERNAL_GBENCH
#include <benchmark/benchmark.h>
#else

#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace benchmark {

// Prevent compiler from optimizing away expressions
template <class Tp>
inline void DoNotOptimize(Tp& value) {
  asm volatile("" : "+m"(value) : : "memory");
}

template <class Tp>
inline void DoNotOptimize(const Tp& value) {
  asm volatile("" : : "r,m"(value) : "memory");
}

inline void ClobberMemory() {
  asm volatile("" : : : "memory");
}

class BenchmarkRegistration;

class State {
 public:
  State(int64_t max_iters, const std::vector<int64_t>& args = {})
      : max_iters_(max_iters), args_(args) {}

  struct Iterator {
    int64_t iters_left_;
    State* state_;

    Iterator(int64_t iters, State* s) : iters_left_(iters), state_(s) {}

    bool operator!=(const Iterator& o) const { return iters_left_ != o.iters_left_; }
    Iterator& operator++() {
      --iters_left_;
      ++state_->total_iters_;
      return *this;
    }
    int64_t operator*() const { return state_->total_iters_; }
  };

  Iterator begin() {
    start_time_ = std::chrono::steady_clock::now();
    return Iterator(max_iters_, this);
  }

  Iterator end() { return Iterator(0, this); }

  void PauseTiming() {
    pause_start_ = std::chrono::steady_clock::now();
  }

  void ResumeTiming() {
    auto pause_end = std::chrono::steady_clock::now();
    paused_duration_ += (pause_end - pause_start_);
  }

  void SetItemsProcessed(int64_t n) { items_processed_ = n; }
  int64_t items_processed() const { return items_processed_; }

  int64_t iterations() const { return total_iters_; }

  int64_t range(size_t idx = 0) const {
    if (idx < args_.size()) return args_[idx];
    return 0;
  }

  std::chrono::nanoseconds compute_elapsed() const {
    auto end_time = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - start_time_ - paused_duration_);
  }

  std::map<std::string, double> counters;

 private:
  int64_t max_iters_;
  std::vector<int64_t> args_;
  int64_t total_iters_ = 0;
  int64_t items_processed_ = 0;
  std::chrono::steady_clock::time_point start_time_;
  std::chrono::steady_clock::time_point pause_start_;
  std::chrono::nanoseconds paused_duration_{0};
};

struct BenchmarkResult {
  std::string name;
  std::string run_name;
  std::string run_type = "iteration";
  std::string aggregate_name;
  int64_t iterations = 0;
  double real_time_ns = 0.0;
  double cpu_time_ns = 0.0;
  std::string time_unit = "ns";
  double items_per_second = 0.0;
  std::map<std::string, double> counters;
};

class BenchmarkInstance {
 public:
  virtual ~BenchmarkInstance() = default;
  virtual void Run(State& state) = 0;
  virtual std::string GetName() const = 0;
  virtual std::vector<int64_t> GetArgs() const { return {}; }
};

class BenchmarkRunner {
 public:
  static BenchmarkRunner& instance() {
    static BenchmarkRunner r;
    return r;
  }

  void Register(std::shared_ptr<BenchmarkInstance> b) {
    benchmarks_.push_back(b);
  }

  int RunAll(int argc, char** argv) {
    std::string filter = "";
    std::string out_path = "";
    std::string out_format = "json";
    int repetitions = 1;

    for (int i = 1; i < argc; ++i) {
      std::string arg = argv[i];
      if (arg.rfind("--benchmark_filter=", 0) == 0) {
        filter = arg.substr(19);
      } else if (arg.rfind("--benchmark_out=", 0) == 0) {
        out_path = arg.substr(16);
      } else if (arg.rfind("--benchmark_out_format=", 0) == 0) {
        out_format = arg.substr(23);
      } else if (arg.rfind("--benchmark_repetitions=", 0) == 0) {
        repetitions = std::max(1, std::stoi(arg.substr(24)));
      }
    }

    std::printf("%-50s %12s %14s %12s\n", "Benchmark", "Time (ns)", "CPU (ns)", "Iterations");
    std::printf("------------------------------------------------------------------------------------------------\n");

    std::vector<BenchmarkResult> all_results;

    for (auto& b : benchmarks_) {
      std::string name = b->GetName();
      if (!filter.empty() && name.find(filter) == std::string::npos) {
        continue;
      }

      std::vector<BenchmarkResult> rep_results;
      for (int rep = 0; rep < repetitions; ++rep) {
        // Adaptive iteration sizing: probe with 10 iterations first
        int64_t iters = 10;
        State probe_state(iters, b->GetArgs());
        b->Run(probe_state);
        auto elapsed = probe_state.compute_elapsed();

        if (elapsed.count() < 10'000'000) { // < 10ms
          double est_per_iter = std::max(1.0, static_cast<double>(elapsed.count()) / iters);
          // Aim for ~100ms - 200ms per run
          iters = std::min<int64_t>(10'000'000, std::max<int64_t>(100, static_cast<int64_t>(150'000'000.0 / est_per_iter)));
        } else if (elapsed.count() > 500'000'000) { // > 500ms
          iters = std::max<int64_t>(1, static_cast<int64_t>(iters * 200'000'000.0 / elapsed.count()));
        }

        State state(iters, b->GetArgs());
        b->Run(state);
        auto real_elapsed = state.compute_elapsed();
        double real_time_ns = static_cast<double>(real_elapsed.count()) / state.iterations();
        double items_per_sec = 0.0;
        if (state.items_processed() > 0 && real_elapsed.count() > 0) {
          items_per_sec = static_cast<double>(state.items_processed()) / (real_elapsed.count() * 1e-9);
        }

        BenchmarkResult res;
        res.name = name + (repetitions > 1 ? "/repeats:" + std::to_string(rep) : "");
        res.run_name = name;
        res.run_type = repetitions > 1 ? "iteration" : "iteration";
        res.iterations = state.iterations();
        res.real_time_ns = real_time_ns;
        res.cpu_time_ns = real_time_ns;
        res.items_per_second = items_per_sec;
        res.counters = state.counters;
        rep_results.push_back(res);
        all_results.push_back(res);

        std::string display_name = name;
        if (display_name.size() > 50) display_name = display_name.substr(0, 47) + "...";
        std::printf("%-50s %12.1f %14.1f %12lld\n",
                    display_name.c_str(), real_time_ns, real_time_ns, static_cast<long long>(state.iterations()));
      }

      if (repetitions > 1 && !rep_results.empty()) {
        double sum = 0, sum_sq = 0;
        for (const auto& r : rep_results) {
          sum += r.real_time_ns;
          sum_sq += r.real_time_ns * r.real_time_ns;
        }
        double mean = sum / rep_results.size();
        double variance = (sum_sq / rep_results.size()) - (mean * mean);
        double stddev = std::sqrt(std::max(0.0, variance));
        double cv = mean > 0 ? (stddev / mean) : 0.0;

        BenchmarkResult median_res = rep_results[rep_results.size() / 2];
        median_res.name = name + "_median";
        median_res.run_name = name;
        median_res.run_type = "aggregate";
        median_res.aggregate_name = "median";
        all_results.push_back(median_res);

        BenchmarkResult cv_res = median_res;
        cv_res.name = name + "_cv";
        cv_res.run_name = name;
        cv_res.run_type = "aggregate";
        cv_res.aggregate_name = "cv";
        cv_res.real_time_ns = cv;
        cv_res.cpu_time_ns = cv;
        all_results.push_back(cv_res);
      }
    }

    if (!out_path.empty()) {
      WriteJson(out_path, all_results);
      std::cout << "\nBenchmark results written to " << out_path << "\n";
    }

    return 0;
  }

 private:
  void WriteJson(const std::string& path, const std::vector<BenchmarkResult>& results) {
    std::ofstream f(path);
    f << "{\n  \"context\": {\n"
      << "    \"date\": \"\",\n"
      << "    \"executable\": \"ob_bench\",\n"
      << "    \"num_cpus\": 1,\n"
      << "    \"mhz_per_cpu\": 0,\n"
      << "    \"cpu_scaling_enabled\": false,\n"
      << "    \"library_build_type\": \"release\"\n"
      << "  },\n  \"benchmarks\": [\n";

    for (size_t i = 0; i < results.size(); ++i) {
      const auto& r = results[i];
      f << "    {\n"
        << "      \"name\": \"" << r.name << "\",\n"
        << "      \"run_name\": \"" << r.run_name << "\",\n"
        << "      \"run_type\": \"" << r.run_type << "\",\n";
      if (!r.aggregate_name.empty()) {
        f << "      \"aggregate_name\": \"" << r.aggregate_name << "\",\n";
      }
      f << "      \"iterations\": " << r.iterations << ",\n"
        << "      \"real_time\": " << r.real_time_ns << ",\n"
        << "      \"cpu_time\": " << r.cpu_time_ns << ",\n"
        << "      \"time_unit\": \"" << r.time_unit << "\"";

      if (r.items_per_second > 0) {
        f << ",\n      \"items_per_second\": " << r.items_per_second;
      }
      for (const auto& [k, v] : r.counters) {
        f << ",\n      \"" << k << "\": " << v;
      }
      f << "\n    }" << (i + 1 < results.size() ? "," : "") << "\n";
    }
    f << "  ]\n}\n";
  }

  std::vector<std::shared_ptr<BenchmarkInstance>> benchmarks_;
};

class SimpleBenchmarkInstance : public BenchmarkInstance {
 public:
  SimpleBenchmarkInstance(std::string name, std::function<void(State&)> fn, std::vector<int64_t> args = {})
      : name_(name), fn_(fn), args_(args) {}

  void Run(State& state) override { fn_(state); }
  std::string GetName() const override { return name_; }
  std::vector<int64_t> GetArgs() const override { return args_; }

 private:
  std::string name_;
  std::function<void(State&)> fn_;
  std::vector<int64_t> args_;
};

class BenchmarkRegistration {
 public:
  BenchmarkRegistration(std::string base_name, std::function<void(State&)> fn)
      : base_name_(base_name), fn_(fn) {
    auto inst = std::make_shared<SimpleBenchmarkInstance>(base_name_, fn_);
    BenchmarkRunner::instance().Register(inst);
  }

  BenchmarkRegistration* Arg(int64_t arg) {
    std::string name = base_name_ + "/" + std::to_string(arg);
    auto inst = std::make_shared<SimpleBenchmarkInstance>(name, fn_, std::vector<int64_t>{arg});
    BenchmarkRunner::instance().Register(inst);
    return this;
  }

  BenchmarkRegistration* Range(int64_t lo, int64_t hi) {
    for (int64_t v = lo; v <= hi; v = (v == 0 ? 1 : v * 8)) {
      Arg(v);
      if (v == hi) break;
      if (v * 8 > hi && v < hi) {
        Arg(hi);
        break;
      }
    }
    return this;
  }

  BenchmarkRegistration* Args(const std::vector<int64_t>& args) {
    std::string name = base_name_;
    for (auto a : args) name += "/" + std::to_string(a);
    auto inst = std::make_shared<SimpleBenchmarkInstance>(name, fn_, args);
    BenchmarkRunner::instance().Register(inst);
    return this;
  }

  BenchmarkRegistration* Repetitions(int) { return this; }
  BenchmarkRegistration* DisplayAggregatesOnly(bool) { return this; }
  BenchmarkRegistration* ReportAggregatesOnly(bool) { return this; }
  BenchmarkRegistration* UseRealTime() { return this; }

 private:
  std::string base_name_;
  std::function<void(State&)> fn_;
};

inline BenchmarkRegistration* RegisterBenchmark(const char* name, void (*fn)(State&)) {
  return new BenchmarkRegistration(name, fn);
}

template <typename Fn>
inline BenchmarkRegistration* RegisterBenchmarkFn(const char* name, Fn fn) {
  return new BenchmarkRegistration(name, fn);
}

}  // namespace benchmark

#define BENCHMARK(fn) \
  static auto benchmark_reg_##fn = ::benchmark::RegisterBenchmark(#fn, fn)

#define BENCHMARK_TEMPLATE(fn, type) \
  static auto benchmark_reg_##fn##_##type = ::benchmark::RegisterBenchmark(#fn "/" #type, fn<type>)

#define BENCHMARK_TEMPLATE2(fn, t1, t2) \
  static auto benchmark_reg_##fn##_##t1##_##t2 = ::benchmark::RegisterBenchmark(#fn "/" #t1 "/" #t2, fn<t1, t2>)

#endif  // OB_USE_EXTERNAL_GBENCH

