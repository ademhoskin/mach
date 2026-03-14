#include "core/common/constants.hpp"

#include <cstdint>
#include <functional>
#include <span>

namespace mach::nodes::wavetable {

enum class Waveform : uint8_t { SINE, SAWTOOTH, TRIANGLE, SQUARE };

class WavetableOscillator {
  public:
    explicit WavetableOscillator(float sample_rate = mach::constants::DEFAULT_SAMPLE_RATE,
                                 Waveform waveform = Waveform::SINE) noexcept;

    void set_sample_rate(float sample_rate) noexcept { sample_rate_ = sample_rate; };
    void set_frequency(float frequency_in_hz) noexcept;
    void set_waveform(Waveform waveform) noexcept;
    void render_frame(std::span<float> output) noexcept;

  private:
    static constexpr auto NOTE_A4 {440.0F};

    uint32_t phase_ {};
    uint32_t phase_inc {NOTE_A4};
    float sample_rate_;
    std::function<float(const float)> get_interpolated_sample_ptr_;
    Waveform waveform_;
};

} // namespace mach::nodes::wavetable
