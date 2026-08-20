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

static void test_marked_updated_after_first_sample() {
    statistics::Statistics s; std::memset(&s, 0, sizeof(s));
    statistics::Interface iface("eth0");
    iface.update(s);
    CHECK(iface.getUpdated());
}

static void test_marked_updated_during_warmup() {   // the count<8 path also skips it today
    statistics::Statistics s; std::memset(&s, 0, sizeof(s));
    statistics::Interface iface("eth0");
    for (int i = 0; i < 8; ++i) iface.update(s);
    CHECK(iface.getUpdated());
}

static const char* const TWO_IFACE_FIXTURE =
    "Inter-|   Receive                             |  Transmit\n"
    " face |bytes    packets errs drop fifo frame compressed multicast|bytes    packets\n"
    "  lo: 123456 789 0 0 0 0 0 0  654321 432 0 0 0 0 0 0\n"
    "  eth0: 1000 10 0 0 0 0 0 0  2000 20 0 0 0 0 0 0\n";

static void test_parse_extracts_interfaces() {
    struct timeval tv; tv.tv_sec = 42; tv.tv_usec = 7;
    statistics::SampleList out = statistics::parseProcNetDev(TWO_IFACE_FIXTURE, tv);
    CHECK(out.size() == 2);
    CHECK(out[0].name == "lo");
    CHECK(out[1].name == "eth0");
    CHECK(out[1].statistics.rx_bytes == 1000);
    CHECK(out[1].statistics.tx_bytes == 2000);
    CHECK(out[1].statistics.rx_packets == 10);
    CHECK(out[1].statistics.timestamp.tv_sec == 42);
}

static void test_parse_skips_malformed_lines() {
    struct timeval tv; tv.tv_sec = 0; tv.tv_usec = 0;
    statistics::SampleList out = statistics::parseProcNetDev(
        "garbage no colon\n eth9: 1 2 3\n", tv);
    CHECK(out.empty());
}

static void test_reader_update_from_string() {    // would have caught a7a3b52
    statistics::Reader reader;
    reader.update(TWO_IFACE_FIXTURE);
    reader.update(TWO_IFACE_FIXTURE);
    CHECK(reader.getInterfaces().size() == 2);
}

static void test_counter_reset_does_not_spike() {
    statistics::Interface iface("eth0");
    statistics::Statistics s; std::memset(&s, 0, sizeof(s));
    s.rx_bytes = 100000; s.tx_bytes = 80000;
    for (int i = 0; i < 20; ++i) {
        s.timestamp.tv_sec = 100 + i;
        iface.update(s);
        s.rx_bytes += 1000; s.tx_bytes += 1000;
    }
    CHECK_NEAR(iface.getReceiveSpeed(), 1000.0, 1e-9);
    CHECK_NEAR(iface.getReceiveMax(), 1000.0, 1e-9);

    s.timestamp.tv_sec = 120;                  // counters reset (x1 < x0)
    s.rx_bytes = 100; s.tx_bytes = 50;
    iface.update(s);
    CHECK_NEAR(iface.getReceiveSpeed(), 0.0, 1e-9);
    CHECK_NEAR(iface.getTransmitSpeed(), 0.0, 1e-9);
    CHECK_NEAR(iface.getReceiveMax(), 1000.0, 1e-9);
    CHECK(iface.getUpdated());

    s.timestamp.tv_sec = 121;                  // recovery from new baseline
    s.rx_bytes = 600; s.tx_bytes = 300;
    iface.update(s);
    CHECK_NEAR(iface.getReceiveSpeed(), 500.0, 1e-9);
}

static void test_late_interface_gets_own_warmup() {
    statistics::Interface early("eth0"), late("wlan0");
    statistics::Statistics s; std::memset(&s, 0, sizeof(s));
    s.rx_bytes = 1000; s.tx_bytes = 1000;
    for (int i = 0; i < 50; ++i) {
        s.timestamp.tv_sec = 100 + i;
        early.update(s);
        s.rx_bytes += 100; s.tx_bytes += 100;
    }
    CHECK(early.getReceiveMax() > 0.0);
    s.timestamp.tv_sec = 200;
    s.rx_bytes = 500000; s.tx_bytes = 500000;
    late.update(s);
    s.timestamp.tv_sec = 201; s.rx_bytes += 10; s.tx_bytes += 10;
    late.update(s);
    CHECK_NEAR(late.getReceiveMax(), 0.0, 1e-9);   // FAILS at HEAD: global count >= 8
    CHECK_NEAR(late.getReceiveSpeed(), 10.0, 1e-9);
}

static void test_max_recorded_after_warmup_window() {
    statistics::Interface iface("eth0");
    statistics::Statistics s; std::memset(&s, 0, sizeof(s));
    s.rx_bytes = 0; s.tx_bytes = 0;
    for (int i = 0; i <= 8; ++i) {
        s.timestamp.tv_sec = i;
        iface.update(s);
        s.rx_bytes += 100; s.tx_bytes += 100;
    }
    CHECK_NEAR(iface.getReceiveMax(), 100.0, 1e-9);
}

int main() {
    test_ctor_zeroes_members();
    test_marked_updated_after_first_sample();
    test_marked_updated_during_warmup();
    test_parse_extracts_interfaces();
    test_parse_skips_malformed_lines();
    test_reader_update_from_string();
    test_counter_reset_does_not_spike();
    test_late_interface_gets_own_warmup();
    test_max_recorded_after_warmup_window();
    if (failures) {
        std::fprintf(stderr, "%d/%d checks FAILED\n", failures, checks);
        return EXIT_FAILURE;
    }
    std::printf("PASSED: %d checks\n", checks);
    return EXIT_SUCCESS;
}
