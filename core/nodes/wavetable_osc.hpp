#include "core/common/constants.hpp"

#include <cstdint>
#include <span>

namespace mach::nodes::wavetable {

enum class Waveform : uint8_t { SINE, SAWTOOTH, TRIANGLE, SQUARE };

class WavetableOscillator {
  public:
    explicit WavetableOscillator(float sample_rate = mach::constants::DEFAULT_SAMPLE_RATE,
                                 Waveform waveform = Waveform::SINE) noexcept;

    void set_sample_rate(float sample_rate) noexcept;
    void set_frequency(float frequency_in_hz) noexcept;
    void set_waveform(Waveform waveform) noexcept;
    void render_frame(std::span<float> output) noexcept;

  private:
    [[nodiscard]] auto compute_phase_increment() const noexcept -> uint32_t;
    void bind_interpolated_sample_ptr() noexcept;

    float sample_rate_;
    float frequency_hz_ {mach::constants::NOTE_A4};

    uint32_t phase_ {};
    uint32_t phase_increment_ {};
    Waveform waveform_;
    // NOTE: We DO NOT use std::function since it sucks in audio (dynamic alloc, vcall, can't
    // inline)
    float (*get_interpolated_sample_ptr_)(uint32_t) noexcept;
};

} // namespace mach::nodes::wavetable
