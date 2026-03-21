#pragma once

#include "core/common/constants.hpp"

#include <atomic>
#include <bit>
#include <cassert>
#include <cstddef>
#include <memory>
#include <type_traits>

namespace mach::ipc {

template<typename T>
concept ValidQueueElement = std::is_trivially_copyable_v<T>;

template<typename QueueElement>
    requires(ValidQueueElement<QueueElement>)
class SPSCQueue {
  public:
    explicit SPSCQueue(std::size_t capacity)
        : CAPACITY {capacity}, MASK {capacity - 1},
          ring_buffer_ {std::make_unique<QueueElement[]>(capacity)} { // NOLINT
        assert(capacity > 1 && std::has_single_bit(capacity));
    }

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

    alignas(mach::constants::ALIGNAS_SIZE) std::atomic_size_t reader_idx_ {0UZ};
    alignas(mach::constants::ALIGNAS_SIZE) std::atomic_size_t writer_idx_ {0UZ};
    std::unique_ptr<QueueElement[]> ring_buffer_; // NOLINT
};

} // namespace mach::ipc
