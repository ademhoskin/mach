#pragma once

#include "core/common/constants.hpp"
#include "core/nodes/node.hpp"
#include "core/nodes/wavetable/wavetable.hpp"

#include <cstdint>
#include <span>
#include <variant>

namespace mach::nodes::wavetable {
// NOLINTNEXTLINE standard in audio for params is 32 bits
enum class ParamId : uint32_t { FREQUENCY = 0, WAVEFORM = 1 };

enum class Waveform : uint8_t { SINE, SAWTOOTH, TRIANGLE, SQUARE };

using ShapedWavetable =
    std::variant<SineWavetable, SawtoothWavetable, TriangleWavetable, SquareWavetable>;

class WavetableOscillator {
  public:
    explicit WavetableOscillator(uint32_t sample_rate = mach::constants::DEFAULT_SAMPLE_RATE,
                                 Waveform waveform = Waveform::SINE) noexcept
        : sample_rate_ {sample_rate}, phase_increment_ {compute_phase_increment()},
          wavetable_ {make_wavetable(waveform)}, waveform_ {waveform} {}

    void set_sample_rate(uint32_t sample_rate) noexcept {
        sample_rate_ = sample_rate;
        phase_increment_ = compute_phase_increment();
    }

    void render_frame(std::span<float> output) noexcept {
        std::visit(
            [&](const auto& table) -> void {
                for (auto& sample : output) {
                    sample = std::decay_t<decltype(table)>::get_interpolated_sample(phase_);
                    phase_ += phase_increment_;
                }
            },
            wavetable_);
    }

    void set_param(NodeParamUpdate update) noexcept {
        switch (static_cast<ParamId>(update.param_id)) {
            case ParamId::FREQUENCY:
                set_frequency(update.value);
                break;
            case ParamId::WAVEFORM:
                /*
                 * NOTE: We are converting a float to a uint8_t Waveform enum,
                 * so we cast to uint8_t first to document what we are doing
                 */
                set_waveform(static_cast<Waveform>(static_cast<uint8_t>(update.value)));
                break;
            default:
                break;
        }
    }

  private:
    [[nodiscard]] auto compute_phase_increment() const noexcept -> uint32_t {
        return static_cast<uint32_t>(frequency_hz_ / static_cast<float>(sample_rate_)
                                     * mach::constants::TWO_TO_POWER_OF_32);
    }

    [[nodiscard]] static auto make_wavetable(Waveform waveform) noexcept -> ShapedWavetable {
        switch (waveform) {
            case Waveform::SINE:
                return SineWavetable {};
            case Waveform::SAWTOOTH:
                return SawtoothWavetable {};
            case Waveform::TRIANGLE:
                return TriangleWavetable {};
            case Waveform::SQUARE:
                return SquareWavetable {};
        }
    }

    void set_frequency(float frequency_hz) noexcept {
        frequency_hz_ = frequency_hz;
        phase_increment_ = compute_phase_increment();
    }

    void set_waveform(Waveform waveform) noexcept {
        waveform_ = waveform;
        wavetable_ = make_wavetable(waveform);
    }

    uint32_t sample_rate_;
    float frequency_hz_ {mach::constants::NOTE_A4};
    uint32_t phase_ {};
    uint32_t phase_increment_ {};
    ShapedWavetable wavetable_;
    Waveform waveform_ {};
};

static_assert(GeneratorNode<WavetableOscillator>);

} // namespace mach::nodes::wavetable
