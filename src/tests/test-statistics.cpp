// Plain-check test binary for the statistics module. No framework by design.
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include "../statistics.hpp"

static int checks = 0;
static int failures = 0;

#define CHECK(cond) do {                                                    \
    ++checks;                                                               \
    if (!(cond)) {                                                          \
        std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);\
        ++failures;                                                         \
    }                                                                       \
} while (0)

#define CHECK_NEAR(actual, expected, eps) do {                              \
    ++checks;                                                               \
    double a_ = (actual), e_ = (expected);                                  \
    if (!(std::fabs(a_ - e_) <= (eps))) {                                   \
        std::fprintf(stderr, "FAIL %s:%d: %s = %g, expected %g\n",          \
                     __FILE__, __LINE__, #actual, a_, e_);                  \
        ++failures;                                                         \
    }                                                                       \
} while (0)

static void test_ctor_zeroes_members() {
    statistics::Interface iface("eth0");
    CHECK_NEAR(iface.getReceiveSpeed(), 0.0, 1e-12);
    CHECK_NEAR(iface.getTransmitSpeed(), 0.0, 1e-12);
    CHECK_NEAR(iface.getReceiveMax(), 0.0, 1e-12);
    CHECK_NEAR(iface.getTransmitMax(), 0.0, 1e-12);
    CHECK(iface.getName() == "eth0");
}

int main() {
    test_ctor_zeroes_members();
    if (failures) {
        std::fprintf(stderr, "%d/%d checks FAILED\n", failures, checks);
        return EXIT_FAILURE;
    }
    std::printf("PASSED: %d checks\n", checks);
    return EXIT_SUCCESS;
}
