#include "bench_framework.hpp"

#ifdef OB_USE_EXTERNAL_GBENCH
int main(int argc, char** argv) {
  ::benchmark::Initialize(&argc, argv);
  ::benchmark::RunSpecifiedBenchmarks();
  ::benchmark::Shutdown();
  return 0;
}
#else
int main(int argc, char** argv) {
  return ::benchmark::BenchmarkRunner::instance().RunAll(argc, argv);
}
#endif

