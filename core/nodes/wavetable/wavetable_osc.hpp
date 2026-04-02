#pragma once

#include "core/common/constants.hpp"
#include "core/nodes/node.hpp"
#include "core/nodes/wavetable/wavetable.hpp"

#include <cstdint>
#include <span>
#include <variant>

/**
 * @file wavetable_osc.hpp
 * @brief Band-limited wavetable oscillator DSP node.
 *
 * @details Implements the `GeneratorNode` concept. Uses a uint32_t phase accumulator
 *          that wraps naturally at 2^32, avoiding modulo operations. The active waveform
 *          is stored as a `std::variant<SineWavetable, SawtoothWavetable, ...>` so that
 *          shape dispatch goes through `std::visit` rather than a runtime branch per
 *          sample.
 */

namespace mach::nodes::wavetable {

/// @brief Selects which pre-generated wavetable the oscillator reads from.
enum class Waveform : uint8_t { SINE, SAWTOOTH, TRIANGLE, SQUARE };

/**
 * @brief Wavetable oscillator that satisfies the `GeneratorNode` concept.
 *
 * @details Maintains a uint32_t phase accumulator advanced by `phase_increment_` each
 *          sample. `phase_increment_` is recomputed whenever frequency or sample rate
 *          changes via the fixed-point formula:
 *
 *          `phase_increment = (frequency_hz / sample_rate) * 2^32`
 *
 *          The waveform variant is swapped atomically on the audio thread inside
 *          `set_param()` — the variant itself lives in node pool memory so no heap
 *          allocation occurs during the swap.
 */
class WavetableOscillator {
public:
    /**
     * @brief Constructs the oscillator with an optional sample rate and waveform.
     *
     * @param sample_rate Device sample rate in Hz. Defaults to
     *                    `constants::DEFAULT_SAMPLE_RATE`.
     * @param waveform    Initial waveform shape. Defaults to `Waveform::SINE`.
     *
     * @note **Thread Safety:** Python/Main Thread — called inside `NodePool::acquire()`
     *       before the node is visible to the audio thread.
     */
    explicit WavetableOscillator(
        uint32_t sample_rate = mach::constants::DEFAULT_SAMPLE_RATE,
        Waveform waveform = Waveform::SINE) noexcept
        : sample_rate_ {sample_rate}, phase_increment_ {compute_phase_increment()},
          wavetable_ {make_wavetable(waveform)}, waveform_ {waveform} {}

    /**
     * @brief Updates the sample rate and recomputes the phase increment.
     *
     * @param sample_rate New device sample rate in Hz.
     *
     * @note **Thread Safety:** Python/Main Thread — called once after `acquire()`,
     *       before `AddNodePayload` is pushed to the SPSC queue.
     */
    void set_sample_rate(uint32_t sample_rate) noexcept {
        sample_rate_ = sample_rate;
        phase_increment_ = compute_phase_increment();
    }

    /**
     * @brief Renders one audio block into `output`, accumulating (adding) into it.
     *
     * @details For each sample, reads an interpolated value from the active wavetable,
     *          scales by `amplitude_`, adds it to `output[i]`, then advances `phase_`
     *          by `phase_increment_`. The `std::visit` over the waveform variant incurs
     *          no per-sample branch — the lambda is monomorphised per wavetable type.
     *
     * @param output Interleaved stereo (or mono) output buffer to accumulate into.
     *               Size must equal `frame_count * channels`.
     *
     * @complexity O(N) where N = `output.size()`.
     * @note **Thread Safety:** Audio Thread. Real-time Safe.
     */
    void render_frame(std::span<float> output) noexcept {
        std::visit(
            [&](const auto& table) -> void {
                for (auto& sample : output) {
                    sample +=
                        std::decay_t<decltype(table)>::get_interpolated_sample(phase_)
                        * amplitude_;
                    phase_ += phase_increment_;
                }
            },
            wavetable_);
    }

    /**
     * @brief Applies a parameter update dispatched by the EDF scheduler.
     *
     * @details Dispatches on `ParamId`:
     *          - `FREQUENCY`: recomputes `phase_increment_`.
     *          - `AMPLITUDE`: clamps to [0, 1].
     *          - `WAVEFORM`: swaps the active wavetable variant (no heap allocation).
     *
     * @param update Parameter ID and new float value. Waveform values are cast
     *               `float → uint8_t → Waveform`.
     *
     * @note **Thread Safety:** Audio Thread. Real-time Safe.
     */
    void set_param(NodeParamUpdate update) noexcept {
        switch (static_cast<ParamId>(update.param_id)) {
            case ParamId::FREQUENCY:
                set_frequency(update.value);
                break;
            case ParamId::AMPLITUDE:
                (update.value > 1.0F) ? amplitude_ = 1.0F : amplitude_ = update.value;
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

    static constexpr uint32_t FREQ_PARAM_ID {0U}; ///< Parameter ID for frequency.
    static constexpr uint32_t AMP_PARAM_ID  {1U}; ///< Parameter ID for amplitude.
    static constexpr uint32_t WAVE_PARAM_ID {2U}; ///< Parameter ID for waveform.

    /**
     * @brief Returns the static array of parameter descriptors for this node type.
     *
     * @details Called once on the Python/main thread when a `NodeHandle` is built.
     *          The returned descriptors are used to populate `NodeHandle::param_map`.
     *
     * @return A `std::array<ParamDescriptor, 3>` — frequency, amplitude, waveform.
     *
     * @note **Thread Safety:** Any thread. The returned array is `static constexpr`.
     */
    [[nodiscard]] static auto get_params() noexcept -> std::array<ParamDescriptor, 3> {
        static constexpr std::array<ParamDescriptor, 3> PARAMS {{
            {.param_id = FREQ_PARAM_ID, .name = "frequency"},
            {.param_id = AMP_PARAM_ID, .name = "amplitude"},
            {.param_id = WAVE_PARAM_ID, .name = "waveform"},
        }};
        return PARAMS;
    }

private:
    // NOLINTNEXTLINE uint32_t is convention for audio
    enum class ParamId : uint32_t {
        FREQUENCY = FREQ_PARAM_ID,
        AMPLITUDE = AMP_PARAM_ID,
        WAVEFORM  = WAVE_PARAM_ID,
    };

    using ShapedWavetable = std::variant<SineWavetable, SawtoothWavetable,
                                         TriangleWavetable, SquareWavetable>;

    /**
     * @brief Recomputes the phase increment from the current frequency and sample rate.
     * @return Fixed-point phase increment for the uint32_t accumulator.
     */
    [[nodiscard]] auto compute_phase_increment() const noexcept -> uint32_t {
        return static_cast<uint32_t>(frequency_hz_ / static_cast<float>(sample_rate_)
                                     * mach::constants::TWO_TO_POWER_OF_32);
    }

    /// @brief Constructs the appropriate wavetable variant for the given waveform.
    [[nodiscard]] static auto make_wavetable(Waveform waveform) noexcept
        -> ShapedWavetable {
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

    /// @brief Updates frequency and recomputes `phase_increment_`. Audio Thread.
    void set_frequency(float frequency_hz) noexcept {
        frequency_hz_ = frequency_hz;
        phase_increment_ = compute_phase_increment();
    }

    /// @brief Swaps the active wavetable variant. No heap allocation. Audio Thread.
    void set_waveform(Waveform waveform) noexcept {
        waveform_ = waveform;
        wavetable_ = make_wavetable(waveform);
    }

    uint32_t       sample_rate_;
    float          frequency_hz_ {mach::constants::NOTE_A4};
    uint32_t       phase_ {};
    uint32_t       phase_increment_ {};
    ShapedWavetable wavetable_;
    Waveform       waveform_ {};
    float          amplitude_ {1.0F};
};

static_assert(GeneratorNode<WavetableOscillator>);

} // namespace mach::nodes::wavetable
