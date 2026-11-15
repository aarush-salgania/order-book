#include "test_framework.hpp"

#ifdef OB_USE_EXTERNAL_GTEST
int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
#else
int main(int argc, char** argv) {
  return ::testing::UnitTestRegistry::instance().run_all_tests(argc, argv);
}
#endif

