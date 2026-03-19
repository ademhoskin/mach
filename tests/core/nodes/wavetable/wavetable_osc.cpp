#include "core/nodes/wavetable/wavetable_osc.hpp"

#include <algorithm>

#include <doctest/doctest.h>
using namespace mach::nodes::wavetable;

constexpr auto VALID_BLOCK_SIZE {8UZ};

struct TestWavetableOscFixture {
    WavetableOscillator oscillator;
};

TEST_CASE_FIXTURE(TestWavetableOscFixture, "WavetableOscillator") {
    SUBCASE("produces non-zero output after default ctor") {
        std::array<float, VALID_BLOCK_SIZE> output_buffer {};
        oscillator.render_frame(output_buffer);
        CHECK(std::ranges::any_of(output_buffer,
                                  [](float sample) -> bool { return sample != 0.0F; }));
    }

    SUBCASE("progresses phase across consecutive render frames") {
        std::array<float, VALID_BLOCK_SIZE> buffer_one {};
        std::array<float, VALID_BLOCK_SIZE> buffer_two {};
        oscillator.render_frame(buffer_one);
        oscillator.render_frame(buffer_two);
        CHECK(buffer_one != buffer_two);
    }

    SUBCASE("changes output when frequency changes") {
        std::array<float, VALID_BLOCK_SIZE> before {};
        std::array<float, VALID_BLOCK_SIZE> after {};
        oscillator.render_frame(before);
        oscillator.set_param({.param_id = 0U, .value = 880.0F});
        oscillator.render_frame(after);
        CHECK(before != after);
    }

    SUBCASE("amplitude scales output") {
        constexpr float HALF_AMPLITUDE {0.5F};
        std::array<float, VALID_BLOCK_SIZE> full_buffer {};
        std::array<float, VALID_BLOCK_SIZE> half_buffer {};

        oscillator.render_frame(full_buffer);

        WavetableOscillator quiet_osc;
        quiet_osc.set_param({.param_id = 1U, .value = HALF_AMPLITUDE});
        quiet_osc.render_frame(half_buffer);

        for (auto idx {0UZ}; idx < VALID_BLOCK_SIZE; ++idx) {
            CHECK(half_buffer.at(idx)
                  == doctest::Approx(full_buffer.at(idx) * HALF_AMPLITUDE));
        }
    }

    SUBCASE("zero amplitude produces silence") {
        std::array<float, VALID_BLOCK_SIZE> output_buffer {};
        oscillator.set_param({.param_id = 1U, .value = 0.0F});
        oscillator.render_frame(output_buffer);
        CHECK(std::ranges::none_of(output_buffer,
                                   [](float sample) -> bool { return sample != 0.0F; }));
    }

    SUBCASE("changes output when waveform changes") {
        std::array<float, VALID_BLOCK_SIZE> sine_buffer {};
        std::array<float, VALID_BLOCK_SIZE> sawtooth_buffer {};
        oscillator.render_frame(sine_buffer);
        oscillator.set_param(
            {.param_id = 2U, .value = static_cast<float>(Waveform::SAWTOOTH)});
        oscillator.render_frame(sawtooth_buffer);
        CHECK(sine_buffer != sawtooth_buffer);
    }
}
