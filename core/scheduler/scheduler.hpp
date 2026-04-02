#pragma once

#include "core/engine/commands.hpp"
#include "core/graph/connection_table.hpp"
#include "core/janitor/janitor.hpp"
#include "core/memory/node_pool.hpp"

#include <cassert>
#include <cstdint>
#include <vector>

/**
 * @file scheduler.hpp
 * @brief Earliest-Deadline-First event scheduler for sample-accurate command dispatch.
 *
 * @details The EDF scheduler decouples command arrival (SPSC pop, any block boundary)
 *          from command execution (the correct sample position). Commands are inserted
 *          into a min-heap keyed by `deadline_abs_sample` and fired during
 *          `process_block()` when their deadline falls within the current block window.
 *
 *          The heap is pre-reserved at construction (`heap_.reserve(heap_size)`) to
 *          avoid reallocations on the audio thread. If the heap fills to capacity,
 *          `schedule()` returns `false` and the command is dropped.
 */

namespace mach::scheduler {

/**
 * @brief EDF scheduler that fires commands at sample-accurate block boundaries.
 *
 * @details Internally uses a `std::vector` min-heap (via `std::ranges::push_heap` /
 *          `pop_heap`). The vector is pre-`reserve()`d so no heap allocation occurs
 *          during normal operation. Capacity is set to a power-of-two multiple of the
 *          node pool size in `AudioEngine`'s constructor.
 */
class EDFScheduler {
public:
    /**
     * @brief Constructs the scheduler and pre-reserves the internal heap.
     *
     * @param heap_size Number of slots to reserve. Should be a power-of-two multiple
     *                  of the node pool capacity.
     *
     * @note **Thread Safety:** Python/Main Thread — called during engine construction.
     */
    explicit EDFScheduler(std::size_t heap_size);

    /**
     * @brief Inserts a command into the EDF heap.
     *
     * @details If `heap_.size() == heap_.capacity()` the command is dropped and `false`
     *          is returned. Otherwise the command is heap-pushed with
     *          `deadline_in_abs_sample` as the sort key.
     *
     * @param command                The payload to schedule.
     * @param deadline_in_abs_sample Absolute sample at which the command should fire.
     *                               Pass `0` for "as soon as possible".
     * @return `true` if the command was accepted; `false` if the heap is full.
     *
     * @note **Thread Safety:** Audio Thread. Real-time Safe (no allocation — heap is
     *       pre-reserved).
     */
    [[nodiscard]] auto schedule(const engine::commands::detail::CommandPayload& command,
                                uint64_t deadline_in_abs_sample) noexcept -> bool;

    /**
     * @brief Fires all commands whose deadline falls within the current block.
     *
     * @details Pops commands from the min-heap while
     *          `deadline_abs_sample < current_abs_sample + block_size` and dispatches
     *          each via `dispatch_command()`. Commands with `deadline = 0` always fire
     *          immediately.
     *
     * @param current_abs_sample Absolute sample index at the start of this block.
     * @param block_size         Number of frames in the current callback invocation.
     * @param pool               Node pool — passed through to `dispatch_command()`.
     * @param janitor            Janitor thread — receives handles of removed nodes.
     * @param connections        Connection table mutated by connect/disconnect commands.
     * @param bpm                Atomic BPM value updated by `SetBpmPayload` commands.
     *
     * @note **Thread Safety:** Audio Thread. Real-time Safe.
     */
    void process_block(uint64_t current_abs_sample, std::size_t block_size,
                       memory::node_pool::NodePool& pool, janitor::JanitorThread& janitor,
                       graph::ConnectionTable& connections,
                       std::atomic<double>& bpm) noexcept;

private:
    /**
     * @brief Internal heap element pairing a command with its EDF deadline.
     */
    struct ScheduledCommand {
        uint64_t                                    deadline_abs_sample;
        engine::commands::detail::CommandPayload command;
    };

    /**
     * @brief Min-heap comparator — earlier deadlines have higher priority.
     */
    constexpr static auto COMPARE_DEADLINE =
        [](const ScheduledCommand& lhs, const ScheduledCommand& rhs) noexcept -> bool {
        return lhs.deadline_abs_sample > rhs.deadline_abs_sample;
    };

    /**
     * @brief Executes a single command payload against the engine subsystems.
     *
     * @details Dispatches on the `CommandPayload` variant:
     *          - `AddNodePayload`      → `pool.activate()`
     *          - `RemoveNodePayload`   → disconnect all, abandon, enqueue to Janitor
     *          - `SetNodeParamPayload` → `node.set_param()`
     *          - `ConnectNodesPayload` → `connections.add()`
     *          - `DisconnectNodesPayload` → `connections.remove()`
     *          - `SetBpmPayload`       → `bpm.store()`
     *
     * @param cmd         The command to execute.
     * @param pool        Node pool for slot state transitions.
     * @param janitor     Receives handles of nodes transitioning to `ABANDONED`.
     * @param connections DSP graph edge table.
     * @param bpm         Atomic BPM value.
     *
     * @note **Thread Safety:** Audio Thread. Real-time Safe.
     */
    static void dispatch_command(const engine::commands::detail::CommandPayload& cmd,
                                 memory::node_pool::NodePool& pool,
                                 janitor::JanitorThread& janitor,
                                 graph::ConnectionTable& connections,
                                 std::atomic<double>& bpm) noexcept;

    /// @brief Pre-reserved min-heap of pending commands sorted by deadline.
    std::vector<ScheduledCommand> heap_;
};

} // namespace mach::scheduler
