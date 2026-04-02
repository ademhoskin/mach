#pragma once

#include "core/memory/node_pool.hpp"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <span>

/**
 * @file connection_table.hpp
 * @brief Flat, pre-allocated table of directed DSP graph edges.
 *
 * @details Stores `(source, dest)` node handle pairs in a contiguous C-array. All
 *          mutations happen on the audio thread (dispatched by the EDF scheduler) so
 *          there is no concurrent-modification concern between `add()`, `remove()`, and
 *          `for_each_connection()`.
 *
 *          Removal uses swap-and-shrink (O(N) scan, O(1) removal) to avoid shifting
 *          elements.
 */

/// @brief Flat pre-allocated table of directed DSP graph edges.
namespace mach::graph {

/// @brief Type alias matching `NodePool`'s opaque handle type.
using NodeHandleID = memory::node_pool::NodePool::NodeHandleID;

/**
 * @brief A single directed edge in the DSP graph.
 */
struct Connection {
    NodeHandleID source; ///< Generator node handle.
    NodeHandleID dest;   ///< Sink node handle.
};

/**
 * @brief Fixed-capacity flat table of DSP graph connections.
 *
 * @details Pre-allocated at engine start. All methods are called exclusively on the
 *          audio thread via `EDFScheduler::dispatch_command()` or
 *          `AudioEngine::audio_callback()`.
 */
class ConnectionTable {
public:
    /**
     * @brief Constructs the table with a fixed maximum number of connections.
     *
     * @param capacity Maximum number of simultaneous edges.
     *
     * @note **Thread Safety:** Python/Main Thread — called during engine construction.
     */
    explicit ConnectionTable(std::size_t capacity)
        : connections_ {std::make_unique<Connection[]>(capacity)}, CAPACITY {capacity} {}

    /**
     * @brief Adds a directed edge from `source` to `dest`.
     *
     * @param source Generator node handle.
     * @param dest   Sink node handle.
     * @return `true` if the edge was added; `false` if the table is at capacity.
     *
     * @note **Thread Safety:** Audio Thread. Real-time Safe.
     */
    [[nodiscard]] auto add(NodeHandleID source, NodeHandleID dest) noexcept -> bool {
        if (count_ >= CAPACITY) {
            return false;
        }
        connections_[count_] = {.source = source, .dest = dest};
        ++count_;
        return true;
    }

    /**
     * @brief Removes the first edge matching `(source, dest)`.
     *
     * @details Scans linearly, fills the removed slot with the last element, and
     *          decrements `count_` — preserving compactness without a shift.
     *
     * @param source Generator node handle.
     * @param dest   Sink node handle.
     * @return `true` if a matching edge was found and removed; `false` otherwise.
     *
     * @note **Thread Safety:** Audio Thread. Real-time Safe.
     */
    auto remove(NodeHandleID source, NodeHandleID dest) noexcept -> bool {
        auto conns {std::span {connections_.get(), count_}};
        for (auto&& [idx, conn] : std::views::enumerate(conns)) {
            if (conn.source == source && conn.dest == dest) {
                conn = connections_[count_ - 1];
                --count_;
                return true;
            }
        }
        return false;
    }

    /**
     * @brief Removes all edges involving `node_id` as either source or destination.
     *
     * @details Called when a node is removed, before it is transitioned to `ABANDONED`.
     *          Uses the same swap-and-shrink strategy as `remove()`.
     *
     * @param node_id Handle of the node being removed.
     *
     * @note **Thread Safety:** Audio Thread. Real-time Safe.
     */
    void remove_all_for(NodeHandleID node_id) noexcept {
        std::size_t i {0};
        while (i < count_) {
            if (connections_[i].source == node_id || connections_[i].dest == node_id) {
                connections_[i] = connections_[count_ - 1];
                --count_;
            } else {
                ++i;
            }
        }
    }

    /**
     * @brief Invokes `func` for every active connection.
     *
     * @details Called once per audio block by `AudioEngine::audio_callback()` to route
     *          audio from each generator to its sink.
     *
     * @tparam F Callable with signature `void(const Connection&)`.
     * @param func Visitor applied to each `Connection` in insertion order.
     *
     * @note **Thread Safety:** Audio Thread. Real-time Safe.
     */
    template<typename F>
    void for_each_connection(const F& func) const noexcept {
        auto conns {std::span {connections_.get(), count_}};
        for (const auto& conn : conns) {
            func(conn);
        }
    }

private:
    std::unique_ptr<Connection[]> connections_; // NOLINT
    const std::size_t             CAPACITY;     // NOLINT
    std::size_t                   count_ {0};
};

} // namespace mach::graph
