// latency.cpp: Per-operation tail-latency measurement harness for BookCore.
//
// Google Benchmark reports averages over repeated batches. For low-latency
// order books, tail latencies (p99, p99.9, max) determine real-world execution
// quality and slippage.
//
// This tool timestamps EVERY single operation using hardware RDTSC/RDTSCP
// with serialization fences on x86-64, or std::chrono::steady_clock fallback.
// It buckets results by operation type:
//   - passive: resting limit order
//   - aggressive: crossing / matching order
//   - cancel: successful cancellation
//   - cancel_miss: cancelled order already filled / dead
//   - all: entire order stream aggregate
//
// Supports CPU core pinning on both Windows (SetThreadAffinityMask) and Linux (sched_setaffinity).

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

#if defined(__x86_64__) || defined(_M_X64)
#if defined(_MSC_VER)
#include <intrin.h>
#else
#include <x86intrin.h>
#endif
#define OB_HAVE_TSC 1
#else
#define OB_HAVE_TSC 0
#endif

#if defined(_WIN32)
#include <windows.h>
#elif defined(__linux__)
#include <sched.h>
#endif

#include "reference_book.hpp"
#include "workload.hpp"

namespace {

using namespace ob;
using Clock = std::chrono::steady_clock;

// ------------------------------------------------------------------ timing

bool g_use_tsc = OB_HAVE_TSC;
double g_ns_per_tick = 1.0;

inline std::uint64_t t_begin() {
#if OB_HAVE_TSC
  if (g_use_tsc) {
    _mm_lfence();
    std::uint64_t t = __rdtsc();
    _mm_lfence();
    return t;
  }
#endif
  return static_cast<std::uint64_t>(Clock::now().time_since_epoch().count());
}

inline std::uint64_t t_end() {
#if OB_HAVE_TSC
  if (g_use_tsc) {
    unsigned aux;
    std::uint64_t t = __rdtscp(&aux);
    _mm_lfence();
    return t;
  }
#endif
  return static_cast<std::uint64_t>(Clock::now().time_since_epoch().count());
}

void calibrate() {
  if (!g_use_tsc) {
    g_ns_per_tick = 1e9 * Clock::period::num / Clock::period::den;
    return;
  }
  // Sample three 100ms windows and take the minimum ratio for accuracy
  double best = 0;
  for (int i = 0; i < 3; ++i) {
    auto c0 = Clock::now();
    auto t0 = t_begin();
    while (Clock::now() - c0 < std::chrono::milliseconds(100)) {}
    auto t1 = t_end();
    auto c1 = Clock::now();
    double ns = std::chrono::duration<double, std::nano>(c1 - c0).count();
    double r = ns / static_cast<double>(t1 - t0);
    if (best == 0 || r < best) best = r;
  }
  g_ns_per_tick = best;
}

std::uint64_t measure_overhead() {
  std::vector<std::uint64_t> v(100000);
  for (auto& x : v) {
    auto a = t_begin();
    auto b = t_end();
    x = b - a;
  }
  std::nth_element(v.begin(), v.begin() + v.size() / 2, v.end());
  return v[v.size() / 2];
}

// ----------------------------------------------------------------- stats

struct Summary {
  std::string impl;
  std::string op;
  std::size_t count = 0;
  double mean = 0, p50 = 0, p90 = 0, p99 = 0, p999 = 0, p9999 = 0, max = 0;
};

double pct(const std::vector<double>& sorted, double p) {
  if (sorted.empty()) return 0;
  std::size_t k = static_cast<std::size_t>(std::ceil(p / 100.0 * sorted.size()));
  k = std::min(std::max<std::size_t>(k, 1), sorted.size());
  return sorted[k - 1];
}

Summary summarize(const std::string& impl, const std::string& op, std::vector<double>& ns) {
  Summary s;
  s.impl = impl;
  s.op = op;
  s.count = ns.size();
  if (ns.empty()) return s;
  std::sort(ns.begin(), ns.end());
  double sum = 0;
  for (double x : ns) sum += x;
  s.mean = sum / ns.size();
  s.p50 = pct(ns, 50);
  s.p90 = pct(ns, 90);
  s.p99 = pct(ns, 99);
  s.p999 = pct(ns, 99.9);
  s.p9999 = pct(ns, 99.99);
  s.max = ns.back();
  return s;
}

constexpr int kKinds = 4;
const char* op_name(int t) {
  switch (t) {
    case 0: return "passive";
    case 1: return "aggressive";
    case 2: return "cancel";
    case 3: return "cancel_miss";
  }
  return "?";
}

template <class Book>
std::vector<Summary> run(const char* impl, const Workload& w, std::size_t warmup, std::uint64_t overhead) {
  auto book = std::make_unique<Book>(w.cfg.band);
  ReplayStats st;
  for (std::size_t i = 0; i < warmup; ++i) apply(*book, w.ops[i], st);

  const std::size_t n = w.ops.size() - warmup;
  std::vector<std::uint64_t> ticks(n);
  std::vector<std::uint8_t> kind(n);

  for (std::size_t i = 0; i < n; ++i) {
    const Op& op = w.ops[warmup + i];
    std::uint8_t k = 0;
    const auto a = t_begin();
    switch (op.type) {
      case OpType::Passive:
        book->submit(op.order);
        k = 0;
        break;
      case OpType::Aggressive:
        book->submit(op.order);
        k = 1;
        break;
      case OpType::Cancel:
        k = book->cancel(op.cancel_id) ? 2 : 3;
        break;
    }
    const auto b = t_end();
    ticks[i] = b - a;
    kind[i] = k;
  }

  std::vector<double> per[kKinds], all;
  all.reserve(n);
  for (std::size_t i = 0; i < n; ++i) {
    const double ns = static_cast<double>(ticks[i] > overhead ? ticks[i] - overhead : 0) * g_ns_per_tick;
    per[kind[i]].push_back(ns);
    all.push_back(ns);
  }

  std::vector<Summary> out;
  for (int k = 0; k < kKinds; ++k) out.push_back(summarize(impl, op_name(k), per[k]));
  out.push_back(summarize(impl, "all", all));
  return out;
}

void print_table(const std::vector<Summary>& rows) {
  std::printf("%-12s %-12s %10s %8s %8s %8s %8s %9s %9s %10s\n",
              "impl", "op", "count", "mean", "p50", "p90", "p99", "p99.9", "p99.99", "max");
  std::string last;
  for (const auto& r : rows) {
    if (!last.empty() && last != r.impl) std::printf("\n");
    last = r.impl;
    if (r.count == 0) continue;
    std::printf("%-12s %-12s %10zu %8.1f %8.1f %8.1f %8.1f %9.1f %9.1f %10.1f\n",
                r.impl.c_str(), r.op.c_str(), r.count, r.mean, r.p50, r.p90, r.p99, r.p999, r.p9999, r.max);
  }
  std::printf("(all latencies in ns, timer overhead subtracted)\n");
}

void write_json(const std::string& path, const std::vector<Summary>& rows, const WorkloadConfig& cfg,
                std::size_t warmup, double overhead_ns, int cpu) {
  std::ofstream f(path);
  f << "{\n  \"meta\": {\n"
    << "    \"timer\": \"" << (g_use_tsc ? "rdtsc" : "steady_clock") << "\",\n"
    << "    \"ns_per_tick\": " << g_ns_per_tick << ",\n"
    << "    \"timer_overhead_ns\": " << overhead_ns << ",\n"
    << "    \"ops\": " << cfg.num_ops << ",\n"
    << "    \"warmup_ops\": " << warmup << ",\n"
    << "    \"seed\": " << cfg.seed << ",\n"
    << "    \"max_live\": " << cfg.max_live << ",\n"
    << "    \"pinned_cpu\": " << cpu << "\n  },\n  \"results\": [\n";
  for (std::size_t i = 0; i < rows.size(); ++i) {
    const auto& r = rows[i];
    f << "    {\"impl\": \"" << r.impl << "\", \"op\": \"" << r.op << "\", \"count\": " << r.count
      << ", \"mean_ns\": " << r.mean << ", \"p50_ns\": " << r.p50 << ", \"p90_ns\": " << r.p90
      << ", \"p99_ns\": " << r.p99 << ", \"p99_9_ns\": " << r.p999 << ", \"p99_99_ns\": " << r.p9999
      << ", \"max_ns\": " << r.max << "}" << (i + 1 < rows.size() ? "," : "") << "\n";
  }
  f << "  ]\n}\n";
}

bool arg(const char* a, const char* key, std::string& out) {
  const std::size_t n = std::strlen(key);
  if (std::strncmp(a, key, n) == 0 && a[n] == '=') {
    out = a + n + 1;
    return true;
  }
  return false;
}

}  // namespace

int main(int argc, char** argv) {
  WorkloadConfig cfg;
  cfg.num_ops = 500000;
  std::string impl = "all", json, timer = "auto", v;
  double warmup_frac = 0.1;
  int cpu = -1;

  for (int i = 1; i < argc; ++i) {
    const char* a = argv[i];
    if (arg(a, "--impl", v)) impl = v;
    else if (arg(a, "--ops", v)) cfg.num_ops = std::stoull(v);
    else if (arg(a, "--seed", v)) cfg.seed = std::stoull(v);
    else if (arg(a, "--warmup", v)) warmup_frac = std::stod(v);
    else if (arg(a, "--max-live", v)) cfg.max_live = std::stoull(v);
    else if (arg(a, "--cpu", v)) cpu = std::stoi(v);
    else if (arg(a, "--timer", v)) timer = v;
    else if (arg(a, "--json", v)) json = v;
    else {
      std::fprintf(stderr, "unknown argument: %s\n", a);
      return 2;
    }
  }

  if (cpu >= 0) {
#if defined(_WIN32)
    DWORD_PTR mask = 1ULL << cpu;
    if (SetThreadAffinityMask(GetCurrentThread(), mask) == 0) {
      std::fprintf(stderr, "Warning: SetThreadAffinityMask to CPU %d failed\n", cpu);
    }
#elif defined(__linux__)
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(cpu, &set);
    if (sched_setaffinity(0, sizeof(set), &set) != 0) std::perror("sched_setaffinity");
#else
    std::fprintf(stderr, "--cpu is not supported on this OS; ignoring\n");
    cpu = -1;
#endif
  }

  if (timer == "clock") g_use_tsc = false;
  else if (timer == "tsc" && !OB_HAVE_TSC) {
    std::fprintf(stderr, "TSC timer not available on this CPU architecture\n");
    return 2;
  }
  calibrate();
  const std::uint64_t overhead = measure_overhead();
  const double overhead_ns = overhead * g_ns_per_tick;

  const Workload w = generate_workload(cfg);
  const std::size_t warmup = static_cast<std::size_t>(warmup_frac * w.ops.size());

  std::printf("timer=%s  ns/tick=%.4f  overhead=%.1f ns  ops=%zu (warmup %zu)  seed=%llu\n\n",
              g_use_tsc ? "rdtsc" : "steady_clock", g_ns_per_tick, overhead_ns, w.ops.size(), warmup,
              static_cast<unsigned long long>(cfg.seed));

  std::vector<Summary> rows;
  auto add = [&](std::vector<Summary> r) { rows.insert(rows.end(), r.begin(), r.end()); };
  bool any = false;
  if (impl == "all" || impl == "contiguous") add(run<ContiguousBook>("contiguous", w, warmup, overhead)), any = true;
  if (impl == "all" || impl == "sparse") add(run<SparseBook>("sparse", w, warmup, overhead)), any = true;
  if (impl == "all" || impl == "stdmap") add(run<StdMapBook>("stdmap", w, warmup, overhead)), any = true;
  if (!any) {
    std::fprintf(stderr, "unknown --impl=%s\n", impl.c_str());
    return 2;
  }

  print_table(rows);
  if (!json.empty()) {
    write_json(json, rows, cfg, warmup, overhead_ns, cpu);
    std::printf("wrote %s\n", json.c_str());
  }
  return 0;
}

