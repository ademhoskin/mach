#include "core/memory/node_pool.hpp"

#include <cassert>

namespace mach::memory::node_pool {

/// @note **Thread Safety:** Audio Thread. Real-time Safe.
auto NodePool::activate(NodeHandleID handle) noexcept -> bool {
    auto [idx, generation] {unpack_node_from_handle(handle)};
    if (generation != slots_[idx].generation) {
        return false;
    }

    auto expected_state {SlotState::ACQUIRED};
    return slots_[idx].current_state.compare_exchange_strong(expected_state,
                                                             SlotState::ACTIVE);
}

/// @note **Thread Safety:** Audio Thread. Real-time Safe.
auto NodePool::abandon_active_nodes(NodeHandleID handle) noexcept -> bool {
    auto [idx, generation] {unpack_node_from_handle(handle)};
    if (generation != slots_[idx].generation) {
        return false;
    }

    auto expected_state {SlotState::ACTIVE};
    return slots_[idx].current_state.compare_exchange_strong(expected_state,
                                                             SlotState::ABANDONED);
}

/**
 * @details Rejects stale handles and slots not in `ACTIVE` or `ACQUIRED` state.
 *          The pointer remains valid as long as the slot does not transition to
 *          `ABANDONED` or `FREE`.
 *
 * @note **Thread Safety:** Audio Thread. Real-time Safe.
 */
auto NodePool::get_node(NodeHandleID handle) noexcept
    -> std::optional<nodes::AnyDSPNode*> {
    auto [idx, generation] {unpack_node_from_handle(handle)};
    if (generation != slots_[idx].generation) {
        return std::nullopt;
    }

    if (slots_[idx].current_state != SlotState::ACTIVE
        && slots_[idx].current_state != SlotState::ACQUIRED) {
        return std::nullopt;
    }

    return &slots_[idx].node.value();
}

/**
 * @details Called on the Python/main thread when the SPSC push fails after `acquire()`
 *          already succeeded. Without this, the slot would be permanently stuck in
 *          `ACQUIRED` and invisible to both the audio thread and the Janitor.
 *
 * @note **Thread Safety:** Python/Main Thread.
 */
auto NodePool::abandon_acquired_node(NodeHandleID handle) noexcept -> bool {
    auto [idx, generation] {unpack_node_from_handle(handle)};
    if (generation != slots_[idx].generation) {
        return false;
    }

    auto expected_state {SlotState::ACQUIRED};
    return slots_[idx].current_state.compare_exchange_strong(expected_state,
                                                             SlotState::ABANDONED);
}

/**
 * @details Destructs the node via `std::optional::reset()`, bumps the generation
 *          counter to invalidate all outstanding handles, then `release`-stores `FREE`
 *          so a racing `acquire()` on the main thread sees a consistent slot.
 *
 * @note **Thread Safety:** Janitor Thread.
 */
void NodePool::recycle(NodeHandleID handle) noexcept {
    auto [idx, generation] {unpack_node_from_handle(handle)};
    assert(slots_[idx].current_state == SlotState::ABANDONED);
    slots_[idx].node.reset();
    slots_[idx].generation++;
    slots_[idx].current_state.store(SlotState::FREE, std::memory_order_release);
}

} // namespace mach::memory::node_pool
