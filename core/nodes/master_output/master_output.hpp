#pragma once

#include "core/common/constants.hpp"
#include "core/nodes/node.hpp"

#include <array>
#include <cstdint>
#include <span>

/**
 * @file master_output.hpp
 * @brief Terminal sink node that writes accumulated audio into the hardware output buffer.
 *
 * @details Every active `GeneratorNode` in the DSP graph is connected to a
 *          `MasterOutput` instance. `mix_to_output()` is called once per connection per
 *          block; it accumulates (adds) the source signal into the engine's output
 *          buffer which miniaudio then hands to the hardware.
 *
 *          There is exactly one `MasterOutput` per engine, pre-activated during
 *          `AudioEngine` construction and never removed.
 */

namespace mach::nodes::master_output {

/**
 * @brief Terminal sink node satisfying the `SinkNode` concept.
 *
 * @details Currently has no parameters (`get_params()` returns an empty array).
 */
class MasterOutput {
public:
    /**
     * @brief Constructs the master output with an optional sample rate.
     *
     * @param sample_rate Device sample rate in Hz. Defaults to
     *                    `constants::DEFAULT_SAMPLE_RATE`.
     *
     * @note **Thread Safety:** Python/Main Thread — called during engine construction.
     */
    explicit MasterOutput(
        uint32_t sample_rate = mach::constants::DEFAULT_SAMPLE_RATE) noexcept
        : sample_rate_ {sample_rate} {}

    /**
     * @brief Updates the stored sample rate.
     *
     * @param sample_rate New device sample rate in Hz.
     *
     * @note **Thread Safety:** Python/Main Thread — called after `acquire()`.
     */
    void set_sample_rate(uint32_t sample_rate) noexcept { sample_rate_ = sample_rate; }

    // TODO: CURRENTLY NO OP
    /**
     * @brief No-op parameter handler (no parameters defined yet).
     * @note **Thread Safety:** Audio Thread. Real-time Safe.
     */
    void set_param(NodeParamUpdate /*update*/) noexcept {}

    /**
     * @brief Returns an empty parameter descriptor array.
     *
     * @return `std::array<ParamDescriptor, 0>` — this node exposes no parameters.
     *
     * @note **Thread Safety:** Any thread. Constexpr.
     */
    [[nodiscard]] static constexpr auto get_params() noexcept
        -> std::array<ParamDescriptor, 0> {
        return {};
    }

    /**
     * @brief Accumulates `INPUT` into `output` sample-by-sample.
     *
     * @details Adds up to `min(INPUT.size(), output.size())` samples from `INPUT` into
     *          `output`. Called by the audio callback for every connection that
     *          terminates at this node.
     *
     * @param INPUT  Read-only source buffer from the upstream generator.
     * @param output Hardware output buffer to accumulate into. Must be pre-zeroed by
     *               the audio callback before the first mix call each block.
     *
     * @complexity O(N) where N = `min(INPUT.size(), output.size())`.
     * @note **Thread Safety:** Audio Thread. Real-time Safe.
     */
    static void mix_to_output(const std::span<const float> INPUT,
                              std::span<float> output) noexcept {
        auto count {std::min(INPUT.size(), output.size())};
        for (std::size_t i {0}; i < count; ++i) {
            output[i] += INPUT[i];
        }
    }

private:
    uint32_t sample_rate_;
};

static_assert(SinkNode<MasterOutput>);

} // namespace mach::nodes::master_output
