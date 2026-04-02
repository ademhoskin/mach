#pragma once

#include "core/common/constants.hpp"

#include <atomic>
#include <cassert>
#include <cstddef>
#include <memory>
#include <type_traits>

/**
 * @file spsc_queue.hpp
 * @brief Lock-free single-producer / single-consumer ring buffer.
 *
 * @details The canonical inter-thread channel in mach. Python/main pushes commands;
 *          the audio callback pops them at the start of each block. The two atomic
 *          indices are placed on separate cache lines to eliminate false sharing between
 *          the producer and consumer cores.
 */

namespace mach::ipc {

/**
 * @brief Constrains queue elements to trivially copyable types.
 *
 * @details Ensures that copy-assignment into the ring buffer slots is a plain bitwise
 *          copy — no constructors, destructors, or virtual dispatch on the audio thread.
 */
template<typename T>
concept ValidQueueElement = std::is_trivially_copyable_v<T>;

/**
 * @brief Lock-free SPSC ring buffer with power-of-two capacity.
 *
 * @details Uses a bitmask wrap (`& MASK`) instead of modulo for O(1) index arithmetic.
 *          `writer_idx_` and `reader_idx_` each occupy their own `alignas(ALIGNAS_SIZE)`
 *          cache line to prevent false sharing between the producer and consumer.
 *
 *          Ordering contract:
 *          - Producer: store to slot then `release`-store the new writer index.
 *          - Consumer: `acquire`-load the writer index then read the slot.
 *
 * @tparam QueueElement Must satisfy `ValidQueueElement` (trivially copyable).
 */
template<typename QueueElement>
    requires(ValidQueueElement<QueueElement>)
class SPSCQueue {
public:
    /**
     * @brief Constructs the queue with a fixed power-of-two capacity.
     *
     * @param capacity Number of slots. Must be > 1 and a power of two (asserted).
     *
     * @note **Thread Safety:** Python/Main Thread — called once during engine
     *       construction, before any threads are started.
     */
    explicit SPSCQueue(std::size_t capacity)
        : CAPACITY {capacity}, MASK {capacity - 1},
          ring_buffer_ {std::make_unique<QueueElement[]>(capacity)} { // NOLINT
        assert(capacity > 1 && std::has_single_bit(capacity));
    }

    /**
     * @brief Attempts to enqueue one item without blocking.
     *
     * @details Checks fullness by comparing the next writer index against the
     *          `acquire`-loaded reader index, then stores the item and `release`-stores
     *          the new writer index so the consumer sees a consistent slot.
     *
     * @param item The item to copy into the ring buffer.
     * @return `true` if the item was enqueued; `false` if the queue was full.
     *
     * @note **Thread Safety:** Python/Main Thread (producer side only).
     */
    [[nodiscard]] auto try_push(const QueueElement& item) noexcept -> bool {
        const auto OLD_WRITER_IDX {writer_idx_.load(std::memory_order_relaxed)};
        const auto NEW_WRITER_IDX {(OLD_WRITER_IDX + 1) & MASK};
        const auto CURRENT_READ_IDX {reader_idx_.load(std::memory_order_acquire)};

        if (NEW_WRITER_IDX == CURRENT_READ_IDX) {
            return false; // we are full
        }

        ring_buffer_[OLD_WRITER_IDX] = item;
        writer_idx_.store(NEW_WRITER_IDX, std::memory_order_release);
        return true;
    };

    /**
     * @brief Attempts to dequeue one item without blocking.
     *
     * @details Checks emptiness by comparing the reader index against the
     *          `acquire`-loaded writer index, copies the slot into `popped_item`, then
     *          `release`-stores the advanced reader index.
     *
     * @param[out] popped_item Receives the dequeued value on success. Unchanged on
     *                         failure.
     * @return `true` if an item was dequeued; `false` if the queue was empty.
     *
     * @note **Thread Safety:** Audio Thread (consumer side only). Real-time Safe.
     */
    [[nodiscard]] auto try_pop(QueueElement& popped_item) noexcept -> bool {
        const auto POPPED_VALUE_IDX {reader_idx_.load(std::memory_order_relaxed)};
        const auto NEW_READER_IDX {(POPPED_VALUE_IDX + 1) & MASK};
        const auto CURRENT_WRITER_IDX {writer_idx_.load(std::memory_order_acquire)};

        if (POPPED_VALUE_IDX == CURRENT_WRITER_IDX) {
            return false; // we are empty
        }
        popped_item = ring_buffer_[POPPED_VALUE_IDX];

        reader_idx_.store(NEW_READER_IDX, std::memory_order_release);
        return true;
    };

private:
    const std::size_t CAPACITY; // NOLINT
    const std::size_t MASK;     // NOLINT

    /// @brief Consumer index. Isolated on its own cache line to avoid false sharing.
    alignas(mach::constants::ALIGNAS_SIZE) std::atomic_size_t reader_idx_ {0UZ};
    /// @brief Producer index. Isolated on its own cache line to avoid false sharing.
    alignas(mach::constants::ALIGNAS_SIZE) std::atomic_size_t writer_idx_ {0UZ};
    std::unique_ptr<QueueElement[]> ring_buffer_; // NOLINT
};

} // namespace mach::ipc
