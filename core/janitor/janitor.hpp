#pragma once

#include "core/ipc/spsc_queue.hpp"
#include "core/memory/node_pool.hpp"

#include <functional>
#include <semaphore>
#include <stop_token>
#include <thread>

/**
 * @file janitor.hpp
 * @brief Background thread that recycles `ABANDONED` DSP node slots.
 *
 * @details The audio thread must never call destructors (The One Rule). Instead, when a
 *          node is removed, the audio thread transitions its slot to `ABANDONED` and
 *          pushes its handle onto a lock-free SPSC queue. The Janitor thread sleeps on a
 *          `std::counting_semaphore`, wakes on each `enqueue_dead_node()` call, and
 *          calls `NodePool::recycle()` — which destructs the node and frees the slot.
 *
 *          On shutdown, the destructor drains any remaining handles synchronously before
 *          the `std::jthread` is joined.
 */

/// @brief Background thread for deferred DSP node recycling.
namespace mach::janitor {

/**
 * @brief Deferred node recycler running on a dedicated background thread.
 *
 * @details Lifecycle:
 *          1. Audio thread calls `enqueue_dead_node(handle)` → pushes to SPSC, releases
 *             semaphore.
 *          2. Janitor thread wakes, pops the handle, calls `pool_.recycle(handle)`.
 *          3. On `AudioEngine::stop()`, the `JanitorThread` destructor is called:
 *             drains any remaining handles then joins the thread.
 *
 * @note Not copyable or movable — owns a live `std::jthread`.
 */
class JanitorThread {
public:
    /// @brief Opaque handle type matching `NodePool`.
    using NodeHandleID = memory::node_pool::NodePool::NodeHandleID;

    /**
     * @brief Constructs the Janitor and starts the background thread.
     *
     * @param pool               Reference to the shared `NodePool`. The pool must
     *                           outlive this object.
     * @param dead_queue_capacity Capacity of the SPSC dead-node queue. Must be a
     *                            power of two greater than 1.
     *
     * @note **Thread Safety:** Python/Main Thread — called during engine construction.
     */
    explicit JanitorThread(memory::node_pool::NodePool& pool,
                           std::size_t dead_queue_capacity) noexcept
        : pool_ {pool}, dead_node_queue_ {dead_queue_capacity},
          thread_ {run_thread, std::ref(*this)} {};

    /**
     * @brief Signals the Janitor to stop, drains remaining handles, then joins.
     *
     * @details Called on the Python/main thread during `AudioEngine::stop()` after the
     *          audio callback has been halted. The drain loop here handles any handles
     *          that were pushed before the semaphore-based loop had a chance to process
     *          them.
     *
     * @note **Thread Safety:** Python/Main Thread.
     */
    ~JanitorThread() noexcept {
        wake_signal_.release();
        NodeHandleID handle {};
        while (dead_node_queue_.try_pop(handle)) {
            pool_.recycle(handle);
        }
    };

    JanitorThread(const JanitorThread&) = delete;
    auto operator=(const JanitorThread&) -> JanitorThread& = delete;
    JanitorThread(JanitorThread&&) noexcept = delete;
    auto operator=(JanitorThread&&) noexcept -> JanitorThread& = delete;

    /**
     * @brief Pushes a dead node handle onto the recycling queue and wakes the Janitor.
     *
     * @details Called by the audio thread immediately after abandoning a node slot.
     *          The SPSC push is wait-free. The semaphore `release()` is also wait-free
     *          (it increments an atomic counter).
     *
     * @param handle Opaque handle of the abandoned node.
     * @return `true` if the handle was enqueued; `false` if the dead-node queue is full.
     *
     * @note **Thread Safety:** Audio Thread. Real-time Safe.
     */
    [[nodiscard]] auto enqueue_dead_node(NodeHandleID handle) noexcept -> bool {
        if (!dead_node_queue_.try_push(handle)) {
            return false;
        }
        wake_signal_.release();
        return true;
    }

private:
    /**
     * @brief Janitor thread body — sleeps until signalled, then recycles one handle.
     *
     * @details Loops until `stop_token` is requested. Blocks on `wake_signal_.acquire()`
     *          between recycles. The stop signal is delivered via a final `release()` in
     *          the destructor.
     *
     * @param stop_token Cooperative cancellation token provided by `std::jthread`.
     * @param self       Reference to the owning `JanitorThread`.
     *
     * @note **Thread Safety:** Janitor Thread.
     */
    static void run_thread(const std::stop_token& stop_token,
                           JanitorThread& self) noexcept {
        while (!stop_token.stop_requested()) {
            self.wake_signal_.acquire();
            NodeHandleID handle {};
            if (self.dead_node_queue_.try_pop(handle)) {
                self.pool_.recycle(handle);
            }
        }
    }

    memory::node_pool::NodePool&      pool_;
    ipc::SPSCQueue<NodeHandleID>      dead_node_queue_;
    std::counting_semaphore<>         wake_signal_ {0};
    std::jthread                      thread_;
};

} // namespace mach::janitor
