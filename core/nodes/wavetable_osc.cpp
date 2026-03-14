#include "core/nodes/wavetable_osc.hpp"

#include "core/nodes/wavetable.hpp"

namespace mach::nodes::wavetable {

WavetableOscillator::WavetableOscillator(float sample_rate, Waveform waveform) noexcept
    : sample_rate_ {sample_rate}, waveform_() {
    set_waveform(waveform);
};
void WavetableOscillator::set_frequency(float frequency) noexcept {
    frequency_hz_ = frequency;
}

void WavetableOscillator::set_waveform(Waveform waveform) noexcept {
    waveform_ = waveform;
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

void WavetableOscillator::render_frame(std::span<float> output) noexcept {
    for (auto& sample : output) {
        sample = get_interpolated_sample_ptr_(phase_);
        phase_ += (frequency_hz_ / sample_rate_);
        phase_ -= static_cast<float>(phase_ >= 1.0F);
    }
}

} // namespace mach::nodes::wavetable
