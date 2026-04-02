#pragma once

#include <concepts>
#include <cstdint>
#include <span>
#include <string_view>

/**
 * @file node.hpp
 * @brief Core DSP node concepts, parameter descriptor types, and parameter update types.
 *
 * @details Defines the compile-time contracts that all DSP nodes must satisfy. Nodes are
 *          never accessed through virtual dispatch — the engine uses `std::variant` and
 *          `std::visit` to call concrete types directly, keeping the audio callback
 *          branch-predictable and cache-friendly.
 */

/// @brief DSP node concepts, parameter types, and node variant registration.
namespace mach::nodes {

/**
 * @brief Describes a single parameter exposed by a DSP node.
 *
 * @details Returned by `Node::get_params()` and used by the Python bindings to build
 *          the `NodeHandle::param_map` name-to-ID lookup table at node creation time.
 */
struct ParamDescriptor {
    uint64_t         param_id; ///< Stable numeric ID matching the node's `ParamId` enum.
    std::string_view name;     ///< Human-readable name exposed to Python (e.g. `"frequency"`).
};

/**
 * @brief Carries a single parameter mutation from the SPSC queue to a live node.
 *
 * @details Embedded in `SetNodeParamPayload` and forwarded to `Node::set_param()` on
 *          the audio thread. All values are encoded as `float` regardless of the
 *          parameter's logical type; the node casts as needed.
 */
struct NodeParamUpdate {
    uint32_t param_id; ///< Parameter to update (matches a `ParamDescriptor::param_id`).
    float    value;    ///< New value, coerced to `float` by the Python binding layer.
};

/**
 * @brief Minimum interface required of every DSP node.
 *
 * @details Satisfied by any type that provides:
 *          - `set_sample_rate(float)` — called once when the node is activated.
 *          - `set_param(NodeParamUpdate)` — dispatched by the EDF scheduler on the
 *            audio thread; must be real-time safe.
 *          - `static get_params()` — returns a contiguous range of `ParamDescriptor`
 *            values; used only on the main thread during binding registration.
 */
template<typename Node>
concept DSPNode = requires(Node node, ParamDescriptor param, float sample_rate,
                           NodeParamUpdate update) {
    { node.set_sample_rate(sample_rate) } noexcept -> std::same_as<void>;
    { node.set_param(update) } noexcept -> std::same_as<void>;
    {
        Node::get_params()
    } noexcept -> std::convertible_to<std::span<const ParamDescriptor>>;
};

/**
 * @brief Refines `DSPNode` for nodes that consume audio from upstream generators.
 *
 * @details A sink node accumulates (mixes) rendered audio into the hardware output
 *          buffer. `mix_to_output` is called on the audio thread for every connection
 *          that terminates at this node.
 *
 * @note `INPUT` is `const` — sinks must not modify the source buffer.
 */
template<typename Node>
concept SinkNode =
    DSPNode<Node>
    && requires(Node node, const std::span<float> INPUT, std::span<float> output) {
           { node.mix_to_output(INPUT, output) } noexcept -> std::same_as<void>;
       };

/**
 * @brief Refines `DSPNode` for nodes that synthesise audio into a provided buffer.
 *
 * @details A generator node fills `output` with rendered samples each callback.
 *          Exposes `FREQ_PARAM_ID` and `AMP_PARAM_ID` as static constants so the
 *          engine can apply universal pitch/amplitude modulation without knowing the
 *          concrete node type.
 */
template<typename Node>
concept GeneratorNode = DSPNode<Node> && requires(Node node, std::span<float> output) {
    { node.render_frame(output) } noexcept -> std::same_as<void>;
    { Node::FREQ_PARAM_ID } -> std::same_as<const uint32_t&>;
    { Node::AMP_PARAM_ID } -> std::same_as<const uint32_t&>;
};

} // namespace mach::nodes
