#include "core/common/time.hpp"

#include <cstdint>

#include <doctest/doctest.h>

using namespace mach;

constexpr uint32_t TEST_SAMPLE_RATE {48000U};
constexpr double TEST_BPM {120.0};

TEST_CASE("to_abs_sample") {
    constexpr uint64_t CURRENT_SAMPLE {0ULL};

    SUBCASE("Samples adds count directly") {
        auto result {to_abs_sample(Samples {.count = 100}, CURRENT_SAMPLE,
                                   TEST_SAMPLE_RATE, TEST_BPM)};
        CHECK(result == 100ULL);
    }

    SUBCASE("Seconds converts to samples") {
        auto result {to_abs_sample(Seconds {.value = 1.0}, CURRENT_SAMPLE,
                                   TEST_SAMPLE_RATE, TEST_BPM)};
        CHECK(result == 48000ULL);
    }

    SUBCASE("Seconds fractional") {
        auto result {to_abs_sample(Seconds {.value = 0.5}, CURRENT_SAMPLE,
                                   TEST_SAMPLE_RATE, TEST_BPM)};
        CHECK(result == 24000ULL);
    }

    SUBCASE("Beats at 120 bpm") {
        // 120 bpm = 2 beats/sec = 0.5 sec/beat
        // 1 beat = 48000 * 60 / 120 = 24000 samples
        auto result {to_abs_sample(Beats {.value = 1.0}, CURRENT_SAMPLE,
                                   TEST_SAMPLE_RATE, TEST_BPM)};
        CHECK(result == 24000ULL);
    }

    SUBCASE("Beats at different bpm") {
        // 140 bpm: 1 beat = 48000 * 60 / 140 ≈ 20571 samples
        auto result {to_abs_sample(Beats {.value = 1.0}, CURRENT_SAMPLE,
                                   TEST_SAMPLE_RATE, 140.0)};
        CHECK(result == static_cast<uint64_t>(48000.0 * 60.0 / 140.0));
    }

    SUBCASE("offsets from nonzero current sample") {
        constexpr uint64_t OFFSET {10000ULL};
        auto result {to_abs_sample(Samples {.count = 500}, OFFSET,
                                   TEST_SAMPLE_RATE, TEST_BPM)};
        CHECK(result == 10500ULL);
    }

    SUBCASE("four beats at 120 bpm") {
        // 4 beats = 4 * 24000 = 96000 samples
        auto result {to_abs_sample(Beats {.value = 4.0}, CURRENT_SAMPLE,
                                   TEST_SAMPLE_RATE, TEST_BPM)};
        CHECK(result == 96000ULL);
    }
}
