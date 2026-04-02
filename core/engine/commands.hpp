#pragma once

#include "core/nodes/node.hpp"

#include <cstdint>
#include <type_traits>
#include <variant>

/**
 * @file commands.hpp
 * @brief Command payload types for the SPSC command queue.
 *
 * @details Each payload struct represents one mutation the Python/main thread wants to
 *          apply to the engine state. Payloads are wrapped in `ScheduledCommandPayload`
 *          (which adds a `deadline_abs_sample`) and pushed into the SPSC queue. The
 *          audio thread pops them, hands them to `EDFScheduler::schedule()`, and
 *          dispatches them at the appropriate block boundary.
 *
 * @note All payload types must remain trivially copyable so they can be stored in the
 *       SPSC ring buffer without constructor/destructor calls on the audio thread.
 *       The `static_assert` on `ScheduledCommandPayload` enforces this.
 */

/// @brief Command payload types carried through the SPSC queue.
namespace mach::engine::commands::detail {

/// @brief Opaque node identifier — a packed `(slot_idx, generation)` pair cast to
///        `uint64_t`. Never dereference directly; always pass through `NodePool`.
using NodeId = uint64_t;

/**
 * @brief Requests activation of a node slot that was `acquire()`-d on the main thread.
 *
 * @details Transitions the slot from `ACQUIRED → ACTIVE` so the audio graph begins
 *          rendering it. Pushed immediately (deadline = 0) so it takes effect in the
 *          next block.
 */
struct AddNodePayload {
    NodeId node_id; ///< Handle returned by `NodePool::acquire()`.
};

/**
 * @brief Requests removal of an active node from the DSP graph.
 *
 * @details The scheduler will: disconnect all edges for this node, transition it to
 *          `ACTIVE → ABANDONED`, and hand the handle to `JanitorThread` for deferred
 *          recycling. Pushed immediately (deadline = 0).
 */
struct RemoveNodePayload {
    NodeId node_id; ///< Handle of the node to remove.
};

/**
 * @brief Requests an immediate or scheduled parameter update on a live node.
 *
 * @details Dispatched by the EDF scheduler at `deadline_abs_sample`. The node's
 *          `set_param()` is called on the audio thread with `update`.
 */
struct SetNodeParamPayload {
    NodeId node_id;          ///< Target node handle.
    nodes::NodeParamUpdate update; ///< Parameter ID and new float value.
};

/**
 * @brief Requests a connection between a generator node and a sink node.
 *
 * @details Adds a `(source, dest)` edge to the `ConnectionTable`. The audio callback
 *          will route audio from `source` into `dest` starting from the next block.
 */
struct ConnectNodesPayload {
    NodeId source_id; ///< Generator node handle.
    NodeId dest_id;   ///< Sink node handle.
};

/**
 * @brief Requests removal of a connection between two nodes.
 *
 * @details Removes the matching `(source, dest)` edge from the `ConnectionTable`.
 */
struct DisconnectNodesPayload {
    NodeId source_id; ///< Generator node handle.
    NodeId dest_id;   ///< Sink node handle.
};

/**
 * @brief Requests a tempo change.
 *
 * @details The scheduler stores the new value into `AudioEngine::bpm_` (a
 *          `std::atomic<double>`) at the scheduled sample boundary. Subsequent
 *          `Beats`-based deadline calculations on the main thread will observe the
 *          updated value via `memory_order_relaxed` loads.
 */
struct SetBpmPayload {
    double bpm; ///< New tempo in beats per minute. Must be in (0, 1000].
};

/**
 * @brief Discriminated union of all command payload types.
 *
 * @details Stored inside `ScheduledCommandPayload` and carried through the SPSC queue.
 *          `EDFScheduler::dispatch_command()` dispatches on this variant using
 *          `std::visit`.
 */
using CommandPayload =
    std::variant<AddNodePayload, RemoveNodePayload, SetNodeParamPayload,
                 ConnectNodesPayload, DisconnectNodesPayload, SetBpmPayload>;

/**
 * @brief A command and its EDF deadline, as stored in the SPSC ring buffer.
 *
 * @details `deadline_abs_sample = 0` means "fire as soon as possible" (first block
 *          after the command is popped). Non-zero deadlines allow sample-accurate
 *          scheduling via `EDFScheduler`.
 *
 * @note Must remain trivially copyable — the SPSC queue copies slots via assignment.
 */
struct ScheduledCommandPayload {
    CommandPayload command;          ///< The payload to dispatch.
    uint64_t       deadline_abs_sample; ///< Absolute sample at which to fire (0 = immediate).
};

static_assert(std::is_trivially_copyable_v<ScheduledCommandPayload>);

} // namespace mach::engine::commands::detail
