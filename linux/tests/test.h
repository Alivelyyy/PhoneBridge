#ifndef PHONEBRIDGE_TEST_H
#define PHONEBRIDGE_TEST_H

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#define TEST_ASSERT(condition, message) \
    do { \
        if (!(condition)) { \
            fprintf(stderr, "  FAIL: %s (line %d)\n", message, __LINE__); \
            return false; \
        } \
    } while(0)

#define TEST_ASSERT_EQ(a, b, message) \
    do { \
        if ((a) != (b)) { \
            fprintf(stderr, "  FAIL: %s (line %d)\n", message, __LINE__); \
            return false; \
        } \
    } while(0)

#define TEST_ASSERT_STREQ(a, b, message) \
    do { \
        if (std::string(a) != std::string(b)) { \
            fprintf(stderr, "  FAIL: %s (line %d)\n", message, __LINE__); \
            return false; \
        } \
    } while(0)

#define TEST_ASSERT_TRUE(condition, message) TEST_ASSERT(condition, message)
#define TEST_ASSERT_FALSE(condition, message) TEST_ASSERT(!(condition), message)

struct TestCase {
    std::string name;
    bool (*func)();
};

class TestRunner {
public:
    static TestRunner& instance() {
        static TestRunner runner;
        return runner;
    }

    void addTest(const std::string& name, bool (*func)()) {
        tests_.push_back({name, func});
    }

    int run() {
        int passed = 0;
        int failed = 0;
        int total = tests_.size();

        printf("Running %d tests...\n\n", total);

        for (auto& test : tests_) {
            printf("  %-50s", test.name.c_str());
            if (test.func()) {
                printf("PASS\n");
                passed++;
            } else {
                printf("FAIL\n");
                failed++;
            }
        }

        printf("\nResults: %d/%d passed", passed, total);
        if (failed > 0) {
            printf(", %d FAILED", failed);
        }
        printf("\n");

        return failed > 0 ? 1 : 0;
    }

private:
    std::vector<TestCase> tests_;
};

// Helper macros for unique naming using __LINE__
#define PB_CONCAT_(a, b) a##b
#define PB_CONCAT(a, b) PB_CONCAT_(a, b)

#define TEST(name) \
    static bool PB_CONCAT(test_line_, __LINE__)(); \
    static struct PB_CONCAT(reg_line_, __LINE__) { \
        PB_CONCAT(reg_line_, __LINE__)() { \
            TestRunner::instance().addTest(name, PB_CONCAT(test_line_, __LINE__)); \
        } \
    } PB_CONCAT(reg_instance_, __LINE__); \
    static bool PB_CONCAT(test_line_, __LINE__)()

#endif // PHONEBRIDGE_TEST_H
