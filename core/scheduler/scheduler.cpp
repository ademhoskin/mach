#include "core/scheduler/scheduler.hpp"

#include <algorithm>
#include <variant>

namespace mach::scheduler {

template<typename... Ts>
struct Overloaded : Ts... {
    using Ts::operator()...;
};

/**
 * @brief Executes a single command against the engine subsystems.
 *
 * @details Uses a local `Overloaded` visitor to dispatch on the `CommandPayload`
 *          variant. Each lambda is a direct, allocation-free mutation of the passed
 *          subsystem references.
 *
 * @param cmd         Command to dispatch.
 * @param pool        Node pool for slot lifecycle transitions.
 * @param janitor     Receives handles of nodes moving to `ABANDONED`.
 * @param connections DSP graph edge table.
 * @param bpm         Atomic BPM — updated by `SetBpmPayload`.
 *
 * @note **Thread Safety:** Audio Thread. Real-time Safe.
 */
void EDFScheduler::dispatch_command(const engine::commands::detail::CommandPayload& cmd,
                                    memory::node_pool::NodePool& pool,
                                    janitor::JanitorThread& janitor,
                                    graph::ConnectionTable& connections,
                                    std::atomic<double>& bpm) noexcept {
    using namespace engine::commands::detail;
    std::visit(Overloaded {
                   [&](const AddNodePayload& payload) -> void {
                       [[maybe_unused]] auto activated {pool.activate(payload.node_id)};
                       assert(activated);
                   },
                   [&](const RemoveNodePayload& payload) -> void {
                       connections.remove_all_for(payload.node_id);
                       [[maybe_unused]] auto deactivated {
                           pool.abandon_active_nodes(payload.node_id)};
                       assert(deactivated);
                       [[maybe_unused]] auto enqueued {
                           janitor.enqueue_dead_node(payload.node_id)};
                       assert(enqueued);
                   },
                   [&](const SetNodeParamPayload& payload) -> void {
                       auto node {pool.get_node(payload.node_id)};
                       if (!node) {
                           return;
                       }
                       std::visit(
                           [&](auto& node) -> void { node.set_param(payload.update); },
                           *node.value());
                   },
                   [&](const ConnectNodesPayload& payload) -> void {
                       [[maybe_unused]] auto added {
                           connections.add(payload.source_id, payload.dest_id)};
                       assert(added);
                   },
                   [&](const DisconnectNodesPayload& payload) -> void {
                       [[maybe_unused]] auto removed {
                           connections.remove(payload.source_id, payload.dest_id)};
                       assert(removed);
                   },
                   [&](const SetBpmPayload& payload) -> void { bpm.store(payload.bpm); }},
               cmd);
}

EDFScheduler::EDFScheduler(std::size_t heap_size) {
    heap_.reserve(heap_size);
}

/**
 * @brief Inserts a command into the EDF min-heap.
 *
 * @details Returns `false` without modifying the heap if `size == capacity` (the vector
 *          was fully pre-reserved; `push_back` would allocate).
 *
 * @param command                Payload to schedule.
 * @param deadline_in_abs_sample Target sample position.
 * @return `true` if inserted; `false` if the heap is full.
 *
 * @note **Thread Safety:** Audio Thread. Real-time Safe.
 */
auto EDFScheduler::schedule(const engine::commands::detail::CommandPayload& command,
                            uint64_t deadline_in_abs_sample) noexcept -> bool {
    [[unlikely]] if (heap_.size() == heap_.capacity()) { return false; }

    heap_.push_back({.deadline_abs_sample = deadline_in_abs_sample, .command = command});
    std::ranges::push_heap(heap_, COMPARE_DEADLINE);
    return true;
}

/**
 * @brief Dispatches all commands due within `[current_abs_sample, block_end)`.
 *
 * @details Pops from the min-heap while the front element's deadline is strictly less
 *          than `current_abs_sample + block_size`. Commands with `deadline = 0` satisfy
 *          this condition for any non-empty block and fire immediately.
 *
 * @param current_abs_sample Start of the current block.
 * @param block_size         Frames in this callback.
 * @param pool               Node pool.
 * @param janitor            Janitor thread for recycling abandoned nodes.
 * @param connections        Connection table.
 * @param bpm                Atomic BPM.
 *
 * @note **Thread Safety:** Audio Thread. Real-time Safe.
 */
void EDFScheduler::process_block(uint64_t current_abs_sample, std::size_t block_size,
                                 memory::node_pool::NodePool& pool,
                                 janitor::JanitorThread& janitor,
                                 graph::ConnectionTable& connections,
                                 std::atomic<double>& bpm) noexcept {
    uint64_t block_end {current_abs_sample + block_size};
    while (!heap_.empty() && heap_.front().deadline_abs_sample < block_end) {
        std::ranges::pop_heap(heap_, COMPARE_DEADLINE);
        auto cmd {heap_.back()};
        heap_.pop_back();
        dispatch_command(cmd.command, pool, janitor, connections, bpm);
    }
}

} // namespace mach::scheduler
