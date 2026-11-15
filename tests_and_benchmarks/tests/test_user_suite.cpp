#include "test_framework.hpp"

// test_user_suite.cpp
//
// Directly runs the user's legacy TestSuite from Tests/test_suite.cpp
// to ensure complete backward compatibility and continuous verification of all original tests.

#include "Tests/test_suite.cpp"

TEST(UserSuite, RunOriginalTestSuite) {
  TestSuite suite;
  EXPECT_TRUE(suite.success()) << "User test_suite.cpp failed!";
  EXPECT_EQ(suite.result().failed, 0);
  EXPECT_GT(suite.result().passed, 0);
}

