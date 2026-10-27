#include "lob_test_suite.cpp"

int main() {
  lob_test::LOBTestSuite tests;

  return tests.success() ? 0 : 1;
}
