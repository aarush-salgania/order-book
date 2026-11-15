#pragma once

#ifdef OB_USE_EXTERNAL_GTEST
#include <gtest/gtest.h>
#else

#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <functional>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <tuple>
#include <type_traits>
#include <vector>

namespace testing {

class Test {
 public:
  virtual ~Test() = default;
  virtual void SetUp() {}
  virtual void TearDown() {}
};

template <typename... Ts>
struct Types {};

template <typename T>
struct TestParamInfo {
  T param;
  std::string param_name;
};

template <typename T>
class WithParamInterface {
 public:
  static void SetParam(const T& p) { current_param_ = p; }
  static const T& GetParam() { return current_param_; }
 private:
  static inline T current_param_{};
};

template <typename T>
class TestWithParam : public Test, public WithParamInterface<T> {};

struct Failure {
  std::string file;
  int line = 0;
  std::string expression;
  std::string message;
};

class AssertionStream {
 public:
  AssertionStream(bool fatal, const char* file, int line, const std::string& expr)
      : fatal_(fatal), file_(file), line_(line), expr_(expr) {}

  ~AssertionStream() noexcept(false);

  template <typename T>
  AssertionStream& operator<<(const T& val) {
    oss_ << val;
    return *this;
  }

 private:
  bool fatal_;
  const char* file_;
  int line_;
  std::string expr_;
  std::ostringstream oss_;
};

class FatalAssertionException : public std::exception {
 public:
  const char* what() const noexcept override { return "Fatal assertion failure"; }
};

struct TestCaseInfo {
  std::string suite_name;
  std::string test_name;
  std::function<void()> run_fn;
  bool is_disabled = false;
  std::vector<Failure> failures;
  double elapsed_ms = 0.0;
};

class UnitTestRegistry {
 public:
  static UnitTestRegistry& instance() {
    static UnitTestRegistry reg;
    return reg;
  }

  void register_test(const std::string& suite, const std::string& name, std::function<void()> fn, bool disabled = false) {
    tests_.push_back({suite, name, std::move(fn), disabled, {}, 0.0});
  }

  void add_failure(const char* file, int line, const std::string& expr, const std::string& msg, bool fatal) {
    if (current_test_) {
      current_test_->failures.push_back({file, line, expr, msg});
    } else {
      std::cerr << file << ":" << line << ": " << expr << " " << msg << "\n";
    }
    if (fatal) {
      throw FatalAssertionException();
    }
  }

  int run_all_tests(int argc, char** argv) {
    std::string filter = "*";
    bool run_known_bugs = false;

    for (int i = 1; i < argc; ++i) {
      std::string arg = argv[i];
      if (arg.rfind("--gtest_filter=", 0) == 0) {
        filter = arg.substr(15);
      } else if (arg.rfind("--filter=", 0) == 0) {
        filter = arg.substr(9);
      } else if (arg == "--known-bugs") {
        run_known_bugs = true;
      }
    }

    std::vector<TestCaseInfo*> to_run;
    for (auto& t : tests_) {
      bool is_disabled = t.is_disabled || t.test_name.rfind("DISABLED_", 0) == 0;
      if (is_disabled && !run_known_bugs) {
        continue;
      }
      std::string full_name = t.suite_name + "." + t.test_name;
      if (filter != "*" && full_name.find(filter) == std::string::npos) {
        continue;
      }
      to_run.push_back(&t);
    }

    std::cout << "[==========] Running " << to_run.size() << " tests from test suite.\n";
    auto total_start = std::chrono::steady_clock::now();
    int passed = 0;
    int failed = 0;

    for (auto* t : to_run) {
      current_test_ = t;
      std::cout << "[ RUN      ] " << t->suite_name << "." << t->test_name << "\n";
      auto t0 = std::chrono::steady_clock::now();
      try {
        t->run_fn();
      } catch (const FatalAssertionException&) {
        // Handled via failures vector
      } catch (const std::exception& ex) {
        add_failure("unknown", 0, "Unexpected std::exception", ex.what(), false);
      } catch (...) {
        add_failure("unknown", 0, "Unexpected non-std exception", "...", false);
      }
      auto t1 = std::chrono::steady_clock::now();
      t->elapsed_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

      if (t->failures.empty()) {
        std::cout << "[       OK ] " << t->suite_name << "." << t->test_name << " ("
                  << static_cast<int>(t->elapsed_ms) << " ms)\n";
        ++passed;
      } else {
        std::cout << "[  FAILED  ] " << t->suite_name << "." << t->test_name << " ("
                  << static_cast<int>(t->elapsed_ms) << " ms)\n";
        for (const auto& f : t->failures) {
          std::cout << f.file << ":" << f.line << ": Failure\n";
          std::cout << "Value of: " << f.expression << "\n";
          if (!f.message.empty()) {
            std::cout << f.message << "\n";
          }
        }
        ++failed;
      }
      current_test_ = nullptr;
    }

    auto total_end = std::chrono::steady_clock::now();
    double total_ms = std::chrono::duration<double, std::milli>(total_end - total_start).count();

    std::cout << "[==========] " << to_run.size() << " tests ran. (" << static_cast<int>(total_ms) << " ms total)\n";
    std::cout << "[  PASSED  ] " << passed << " tests.\n";
    if (failed > 0) {
      std::cout << "[  FAILED  ] " << failed << " tests.\n";
      for (auto* t : to_run) {
        if (!t->failures.empty()) {
          std::cout << "[  FAILED  ] " << t->suite_name << "." << t->test_name << "\n";
        }
      }
      return 1;
    }
    return 0;
  }

 private:
  std::vector<TestCaseInfo> tests_;
  TestCaseInfo* current_test_ = nullptr;
};

inline AssertionStream::~AssertionStream() noexcept(false) {
  UnitTestRegistry::instance().add_failure(file_, line_, expr_, oss_.str(), fatal_);
}

}  // namespace testing

#define GTEST_ASSERT_(expr, val, fatal) \
  if (!(expr))                          \
  ::testing::AssertionStream(fatal, __FILE__, __LINE__, #val)

#define EXPECT_TRUE(cond) GTEST_ASSERT_((cond), (cond), false)
#define ASSERT_TRUE(cond) GTEST_ASSERT_((cond), (cond), true)

#define EXPECT_FALSE(cond) GTEST_ASSERT_(!(cond), !(cond), false)
#define ASSERT_FALSE(cond) GTEST_ASSERT_(!(cond), !(cond), true)

#define EXPECT_EQ(val1, val2) GTEST_ASSERT_((val1) == (val2), val1 == val2, false)
#define ASSERT_EQ(val1, val2) GTEST_ASSERT_((val1) == (val2), val1 == val2, true)

#define EXPECT_NE(val1, val2) GTEST_ASSERT_((val1) != (val2), val1 != val2, false)
#define ASSERT_NE(val1, val2) GTEST_ASSERT_((val1) != (val2), val1 != val2, true)

#define EXPECT_LE(val1, val2) GTEST_ASSERT_((val1) <= (val2), val1 <= val2, false)
#define ASSERT_LE(val1, val2) GTEST_ASSERT_((val1) <= (val2), val1 <= val2, true)

#define EXPECT_LT(val1, val2) GTEST_ASSERT_((val1) < (val2), val1 < val2, false)
#define ASSERT_LT(val1, val2) GTEST_ASSERT_((val1) < (val2), val1 < val2, true)

#define EXPECT_GE(val1, val2) GTEST_ASSERT_((val1) >= (val2), val1 >= val2, false)
#define ASSERT_GE(val1, val2) GTEST_ASSERT_((val1) >= (val2), val1 >= val2, true)

#define EXPECT_GT(val1, val2) GTEST_ASSERT_((val1) > (val2), val1 > val2, false)
#define ASSERT_GT(val1, val2) GTEST_ASSERT_((val1) > (val2), val1 > val2, true)

#define TEST(test_suite_name, test_name)                                                                    \
  class test_suite_name##_##test_name##_Test : public ::testing::Test {                                      \
   public:                                                                                                  \
    void TestBody();                                                                                        \
    static void RunTest() {                                                                                 \
      test_suite_name##_##test_name##_Test fixture;                                                          \
      fixture.SetUp();                                                                                      \
      fixture.TestBody();                                                                                   \
      fixture.TearDown();                                                                                   \
    }                                                                                                       \
  };                                                                                                        \
  static struct test_suite_name##_##test_name##_Reg {                                                       \
    test_suite_name##_##test_name##_Reg() {                                                                 \
      ::testing::UnitTestRegistry::instance().register_test(#test_suite_name, #test_name,                   \
                                                            &test_suite_name##_##test_name##_Test::RunTest); \
    }                                                                                                       \
  } test_suite_name##_##test_name##_reg_instance;                                                           \
  void test_suite_name##_##test_name##_Test::TestBody()

// ---------------------------------------------------------------- TYPED_TEST
namespace testing_internal {

template <template <typename> class Fixture, typename TypeList, typename NameGenerator>
struct TypedTestSuiteRegistrar;

template <template <typename> class Fixture, typename Head, typename... Tail, typename NameGenerator>
struct TypedTestSuiteRegistrar<Fixture, ::testing::Types<Head, Tail...>, NameGenerator> {
  template <template <typename> class TestClass>
  static void Register(const char* suite_name, const char* test_name) {
    std::string full_suite = std::string(suite_name) + "/" + NameGenerator::template GetName<Head>(0);
    ::testing::UnitTestRegistry::instance().register_test(full_suite, test_name, []() {
      TestClass<Head> test_instance;
      test_instance.SetUp();
      test_instance.TestBody();
      test_instance.TearDown();
    });
    TypedTestSuiteRegistrar<Fixture, ::testing::Types<Tail...>, NameGenerator>::template Register<TestClass>(
        suite_name, test_name);
  }
};

template <template <typename> class Fixture, typename NameGenerator>
struct TypedTestSuiteRegistrar<Fixture, ::testing::Types<>, NameGenerator> {
  template <template <typename> class TestClass>
  static void Register(const char*, const char*) {}
};

}  // namespace testing_internal

#define TYPED_TEST_SUITE(SuiteName, TypesList, NamesList) \
  using SuiteName##_Types_ = TypesList;                   \
  using SuiteName##_Names_ = NamesList;

#define TYPED_TEST(SuiteName, TestName)                                                                          \
  template <typename gtest_TypeParam_>                                                                           \
  class SuiteName##_##TestName : public SuiteName<gtest_TypeParam_> {                                            \
   public:                                                                                                       \
    void TestBody();                                                                                             \
  };                                                                                                             \
  static struct SuiteName##_##TestName##_Reg {                                                                   \
    SuiteName##_##TestName##_Reg() {                                                                             \
      ::testing_internal::TypedTestSuiteRegistrar<SuiteName, SuiteName##_Types_, SuiteName##_Names_>::Register<  \
          SuiteName##_##TestName>(#SuiteName, #TestName);                                                        \
    }                                                                                                            \
  } SuiteName##_##TestName##_instance;                                                                           \
  template <typename gtest_TypeParam_>                                                                           \
  void SuiteName##_##TestName<gtest_TypeParam_>::TestBody()

// ---------------------------------------------------------------- TEST_P
#define TEST_P(SuiteName, TestName)                                                                   \
  class SuiteName##_##TestName##_Test : public SuiteName {                                            \
   public:                                                                                            \
    void TestBody();                                                                                  \
  };                                                                                                  \
  template <typename Dummy = void>                                                                    \
  void SuiteName##_##TestName##_Register(const std::string& prefix, const auto& values, auto name_fn) { \
    for (const auto& v : values) {                                                                    \
      ::testing::TestParamInfo<std::decay_t<decltype(v)>> info{v, name_fn(::testing::TestParamInfo<std::decay_t<decltype(v)>>{v, ""})}; \
      std::string full_name = prefix + "/" + info.param_name;                                        \
      ::testing::UnitTestRegistry::instance().register_test(                                          \
          #SuiteName, full_name, [v]() {                                                              \
            SuiteName##_##TestName##_Test fixture;                                                    \
            fixture.SetParam(v);                                                                      \
            fixture.SetUp();                                                                          \
            fixture.TestBody();                                                                       \
            fixture.TearDown();                                                                       \
          });                                                                                         \
    }                                                                                                 \
  }                                                                                                   \
  void SuiteName##_##TestName##_Test::TestBody()

#define INSTANTIATE_TEST_SUITE_P(Prefix, SuiteName, ValuesGen, NameGen)                 \
  static struct SuiteName##_##Prefix##_Instantiator {                                    \
    SuiteName##_##Prefix##_Instantiator() {                                              \
      /* Registration is linked by instantiating test registrations */                   \
      SuiteName##_ContiguousMatchesReference_Register(#Prefix, ValuesGen, NameGen);      \
      SuiteName##_SparseMatchesReference_Register(#Prefix, ValuesGen, NameGen);          \
    }                                                                                    \
  } SuiteName##_##Prefix##_instantiator_instance;

namespace testing {
template <typename Container>
inline const Container& ValuesIn(const Container& c) { return c; }
}

#endif  // OB_USE_EXTERNAL_GTEST
