#pragma once

#include "core/common/constants.hpp"

#include <array>
#include <atomic>
#include <bit>
#include <cstddef>
#include <type_traits>

namespace mach::ipc {

template<typename T>
concept ValidQueueElement = std::is_trivially_copyable_v<T>;

template<std::size_t N>
concept ValidQueueSize = N > 1 && std::has_single_bit(N);

template<typename QueueElement, std::size_t QueueSize>
    requires(ValidQueueElement<QueueElement> && ValidQueueSize<QueueSize>)

class SPSCQueue {
  public:
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
    static constexpr std::size_t MASK {QueueSize - 1};

    alignas(mach::constants::ALIGNAS_SIZE) std::atomic_size_t reader_idx_ {0UZ};
    alignas(mach::constants::ALIGNAS_SIZE) std::atomic_size_t writer_idx_ {0UZ};
    alignas(mach::constants::ALIGNAS_SIZE)
        std::array<QueueElement, QueueSize> ring_buffer_ {};
};

} // namespace mach::ipc
