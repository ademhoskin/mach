#include "core/nodes/wavetable/wavetable.hpp"

#include <bit>

#include <doctest/doctest.h>

constexpr auto FRACTION_BITS {static_cast<uint32_t>(
    32UZ - (std::bit_width(mach::nodes::wavetable::DEFAULT_TABLE_SIZE) - 1UZ))};

constexpr auto QUARTER_CYCLE {static_cast<uint32_t>(1024UZ << FRACTION_BITS)};
constexpr auto HALF_CYCLE {static_cast<uint32_t>(2048UZ << FRACTION_BITS)};
constexpr auto THREE_QUARTERS_CYCLE {static_cast<uint32_t>(3072UZ << FRACTION_BITS)};
constexpr auto AUDIO_EPSILON {0.001F};
constexpr auto HALF_AMPLITUDE {0.5F};

using namespace mach::nodes::wavetable;

TEST_CASE("Sine Wavetable") {
    SUBCASE("returns 0 at phase 0") {
        CHECK(Sine::get_interpolated_sample(0UZ) == doctest::Approx(0.0F).epsilon(AUDIO_EPSILON));
    }

    SUBCASE("returns 1 at 1/4 phase") {
        CHECK(Sine::get_interpolated_sample(QUARTER_CYCLE)
              == doctest::Approx(1.0F).epsilon(AUDIO_EPSILON));
    }

    SUBCASE("returns 0 at 1/2 phase") {
        CHECK(Sine::get_interpolated_sample(HALF_CYCLE)
              == doctest::Approx(0.0F).epsilon(AUDIO_EPSILON));
    }

    SUBCASE("returns -1 at 3/4 phase") {
        CHECK(Sine::get_interpolated_sample(THREE_QUARTERS_CYCLE)
              == doctest::Approx(-1.0F).epsilon(AUDIO_EPSILON));
    }
}

TEST_CASE("Triangle Wavetable") {
    SUBCASE("returns -1 at phase 0") {
        CHECK(Triangle::get_interpolated_sample(0UZ)
              == doctest::Approx(-1.0F).epsilon(AUDIO_EPSILON));
    }

    SUBCASE("returns 0 at 1/4 phase") {
        CHECK(Triangle::get_interpolated_sample(QUARTER_CYCLE)
              == doctest::Approx(0.0F).epsilon(AUDIO_EPSILON));
    }

    SUBCASE("returns 1 at 1/2 phase") {
        CHECK(Triangle::get_interpolated_sample(HALF_CYCLE)
              == doctest::Approx(1.0F).epsilon(AUDIO_EPSILON));
    }

    SUBCASE("returns 0 at 3/4 phase") {
        CHECK(Triangle::get_interpolated_sample(THREE_QUARTERS_CYCLE)
              == doctest::Approx(0.0F).epsilon(AUDIO_EPSILON));
    }
}

TEST_CASE("Sawtooth Wavetable") {
    SUBCASE("returns 0 at phase 0") {
        CHECK(Sawtooth::get_interpolated_sample(0UZ)
              == doctest::Approx(0.0F).epsilon(AUDIO_EPSILON));
    }

    SUBCASE("returns 0.5 at 1/4 phase") {
        CHECK(Sawtooth::get_interpolated_sample(QUARTER_CYCLE)
              == doctest::Approx(HALF_AMPLITUDE).epsilon(AUDIO_EPSILON));
    }

    SUBCASE("returns 1 at 1/2 cycle") {
        CHECK(Sawtooth::get_interpolated_sample(HALF_CYCLE)
              == doctest::Approx(1.0F).epsilon(AUDIO_EPSILON));
    }

    SUBCASE("returns -0.5 at 3/4 phase") {
        CHECK(Sawtooth::get_interpolated_sample(THREE_QUARTERS_CYCLE)
              == doctest::Approx(-HALF_AMPLITUDE).epsilon(AUDIO_EPSILON));
    }
}

TEST_CASE("Square Wavetable") {
    SUBCASE("returns 1 at phase 0") {
        CHECK(Square::get_interpolated_sample(0UZ) == doctest::Approx(1.0F).epsilon(AUDIO_EPSILON));
    }

    SUBCASE("returns 1 at 1/4 phase") {
        CHECK(Square::get_interpolated_sample(QUARTER_CYCLE)
              == doctest::Approx(1.0F).epsilon(AUDIO_EPSILON));
    }

    SUBCASE("returns 1 at 1/2 phase") {
        CHECK(Square::get_interpolated_sample(HALF_CYCLE)
              == doctest::Approx(1.0F).epsilon(AUDIO_EPSILON));
    }

    SUBCASE("returns -1 at 3/4 phase") {
        CHECK(Square::get_interpolated_sample(THREE_QUARTERS_CYCLE)
              == doctest::Approx(-1.0F).epsilon(AUDIO_EPSILON));
    }
}
