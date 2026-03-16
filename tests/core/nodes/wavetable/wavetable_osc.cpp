#include "core/nodes/wavetable/wavetable_osc.hpp"

#include <algorithm>

#include <doctest/doctest.h>
using namespace mach::nodes::wavetable;

constexpr auto VALID_BLOCK_SIZE {128UZ};

struct TestWavetableOscFixture {
    WavetableOscillator oscillator;
};

TEST_CASE_FIXTURE(TestWavetableOscFixture, "WavetableOscillator") {
    SUBCASE("produces non-zero output after default ctor") {
        std::array<float, VALID_BLOCK_SIZE> output_buffer {};
        oscillator.render_frame(output_buffer);
        CHECK(std::ranges::any_of(output_buffer.begin(), output_buffer.end(),
                                  [](float sample) -> bool { return sample != 0.0F; }));
    }

    SUBCASE("progresses phase across consecutive render frames") {
        std::array<float, VALID_BLOCK_SIZE> buffer_one {};
        std::array<float, VALID_BLOCK_SIZE> buffer_two {};
        oscillator.render_frame(buffer_one);
        oscillator.render_frame(buffer_two);
        CHECK(buffer_one != buffer_two);
    }

    SUBCASE("changes output when waveform changes") {
        std::array<float, VALID_BLOCK_SIZE> sine_buffer {};
        std::array<float, VALID_BLOCK_SIZE> sawtooth_buffer {};
        oscillator.render_frame(sine_buffer);
        oscillator.set_waveform(Waveform::SAWTOOTH);
        oscillator.render_frame(sawtooth_buffer);
        CHECK(sine_buffer != sawtooth_buffer);
    }
}
