#include "core/nodes/wavetable/wavetable_osc.hpp"

#include "core/common/constants.hpp"
#include "core/nodes/node.hpp"
#include "core/nodes/wavetable/wavetable.hpp"

namespace mach::nodes::wavetable {

WavetableOscillator::WavetableOscillator(float sample_rate, Waveform waveform) noexcept
    : sample_rate_ {sample_rate}, phase_increment_ {compute_phase_increment()},
      waveform_ {waveform}, get_interpolated_sample_ptr_() {
    /*
     * XXX: This seems opaque as fuck, we are setting interp sample func ptr here based on
     * initalized waveform
     * */
    bind_interpolated_sample_ptr();
};

void WavetableOscillator::set_sample_rate(float sample_rate) noexcept {
    sample_rate_ = sample_rate;

    phase_increment_ = compute_phase_increment();
}

void WavetableOscillator::set_frequency(float frequency_in_hz) noexcept {
    frequency_hz_ = frequency_in_hz;
    phase_increment_ = compute_phase_increment();
}

auto WavetableOscillator::compute_phase_increment() const noexcept -> uint32_t {
    return static_cast<uint32_t>(frequency_hz_ / sample_rate_
                                 * mach::constants::TWO_TO_POWER_OF_32);
}

void WavetableOscillator::render_frame(std::span<float> output) noexcept {
    for (auto& sample : output) {
        sample = get_interpolated_sample_ptr_(phase_);
        phase_ += phase_increment_;
    }
}

void WavetableOscillator::set_waveform(Waveform waveform) noexcept {
    waveform_ = waveform;
    bind_interpolated_sample_ptr();
}

void WavetableOscillator::bind_interpolated_sample_ptr() noexcept {
    switch (waveform_) {
        case Waveform::SINE:
            get_interpolated_sample_ptr_ = Sine::get_interpolated_sample;
            break;
        case Waveform::SAWTOOTH:
            get_interpolated_sample_ptr_ = Sawtooth::get_interpolated_sample;
            break;
        case Waveform::TRIANGLE:
            get_interpolated_sample_ptr_ = Triangle::get_interpolated_sample;
            break;
        case Waveform::SQUARE:
            get_interpolated_sample_ptr_ = Square::get_interpolated_sample;
            break;
    }
}

static_assert(GeneratorNode<WavetableOscillator>);

} // namespace mach::nodes::wavetable
