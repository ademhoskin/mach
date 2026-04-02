#include "core/nodes/node.hpp"
#include "core/nodes/master_output/master_output.hpp"
#include "core/nodes/wavetable/wavetable_osc.hpp"

#include <variant>

/**
 * @file all_nodes.hpp
 * @brief Central registration point for every concrete DSP node type.
 *
 * @details `AnyDSPNode` is the `std::variant` that the `NodePool` stores in each slot.
 *          Adding a new node type here automatically propagates it through the pool,
 *          the scheduler dispatch, and the audio callback's `std::visit` chains — no
 *          other files need to change.
 *
 * @par Adding a new node
 * 1. Implement the node satisfying `DSPNode` (and optionally `GeneratorNode` or
 *    `SinkNode`).
 * 2. Include its header here.
 * 3. Append the type to `NodeVariantFactory<...>` below.
 */

namespace mach::nodes {

/**
 * @brief Helper alias that validates every type against `DSPNode` at instantiation.
 *
 * @tparam Ts Node types, each of which must satisfy `DSPNode<Ts>`.
 */
template<typename... Ts>
    requires(DSPNode<Ts> && ...)
using NodeVariantFactory = std::variant<Ts...>;

/**
 * @brief Discriminated union of all concrete DSP node types known to the engine.
 *
 * @details Stored inside `NodePool::NodeSlot::node`. The audio callback uses
 *          `std::visit` on this variant to call `render_frame()` or `mix_to_output()`
 *          without virtual dispatch.
 *
 * @note To register a new node type, append it to the parameter list here.
 */
using AnyDSPNode =
    NodeVariantFactory<wavetable::WavetableOscillator, master_output::MasterOutput>;

} // namespace mach::nodes
