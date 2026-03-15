#include "core/nodes/wavetable/wavetable_osc.hpp"

#include <algorithm>

#include <doctest/doctest.h>
using namespace mach::nodes::wavetable;

constexpr auto VALID_BLOCK_SIZE {128UZ};

struct TestWavetableOscFixture {
    /*
     * Default ctor:
     *   sample_rate = mach::constants::DEFAULT_SAMPLE_RATE
     *   waveform = Waveform::SINE
     *   frequency = mach::constants::NOTE_A4
     *   phase = 0
     *   phase_increment = compute_phase_increment() result
     *   get_interpolated_sample_ptr = Sine::get_interpolated_sample, via
     *                                 bind_interpolated_sample_ptr()
     */
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

    // We check that our func ptr changes by verifying side effect
    SUBCASE("changes output when waveform changes") {
        std::array<float, VALID_BLOCK_SIZE> sine_buffer {};
        std::array<float, VALID_BLOCK_SIZE> sawtooth_buffer {};
        oscillator.render_frame(sine_buffer);
        oscillator.set_waveform(Waveform::SAWTOOTH);
        oscillator.render_frame(sawtooth_buffer);
        CHECK(sine_buffer != sawtooth_buffer);
    }
}
