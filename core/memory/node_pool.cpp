#include "core/memory/node_pool.hpp"

#include <cassert>

namespace mach::memory::node_pool {

/**
 * @brief Transitions a slot from `ACQUIRED` to `ACTIVE`.
 *
 * @details Validates the generation tag before attempting the CAS, ensuring stale
 *          handles from a previous occupant of the slot are silently rejected.
 *
 * @param handle Opaque handle returned by `acquire()`.
 * @return `true` if the CAS succeeded; `false` on stale generation or wrong state.
 *
 * @note **Thread Safety:** Audio Thread. Real-time Safe.
 */
auto NodePool::activate(NodeHandleID handle) noexcept -> bool {
    auto [idx, generation] {unpack_node_from_handle(handle)};
    if (generation != slots_[idx].generation) {
        return false;
    }

    auto expected_state {SlotState::ACQUIRED};
    return slots_[idx].current_state.compare_exchange_strong(expected_state,
                                                             SlotState::ACTIVE);
}

/**
 * @brief Transitions an `ACTIVE` slot to `ABANDONED`.
 *
 * @details After this returns `true`, the audio callback will no longer visit the slot
 *          in `for_each_active_node()`. The Janitor thread is responsible for the
 *          subsequent `recycle()`.
 *
 * @param handle Opaque handle of the active node.
 * @return `true` if the CAS succeeded; `false` on stale generation or wrong state.
 *
 * @note **Thread Safety:** Audio Thread. Real-time Safe.
 */
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
 * @brief Returns a non-owning pointer to the node variant, or `std::nullopt`.
 *
 * @details Rejects stale handles and slots not in `ACTIVE` or `ACQUIRED` state.
 *          The pointer remains valid as long as the slot's state does not transition
 *          to `ABANDONED` or `FREE`.
 *
 * @param handle Opaque node handle.
 * @return Pointer to `AnyDSPNode` on success; `std::nullopt` otherwise.
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
 * @brief Transitions an `ACQUIRED` slot to `ABANDONED` (rollback path).
 *
 * @details Called on the Python/main thread when the SPSC push fails after `acquire()`
 *          already succeeded. Without this, the slot would be permanently stuck in
 *          `ACQUIRED` and invisible to both the audio thread and the Janitor.
 *
 * @param handle Opaque handle of the acquired node.
 * @return `true` if the CAS succeeded; `false` on stale generation or wrong state.
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
 * @brief Recycles an `ABANDONED` slot back to `FREE`.
 *
 * @details Destructs the node (via `std::optional::reset()`), bumps the generation
 *          counter to invalidate all outstanding handles, then `release`-stores `FREE`
 *          so a racing `acquire()` on the main thread sees a consistent slot.
 *
 * @param handle Opaque handle of the abandoned node. Must be in `ABANDONED` state
 *               (asserted in debug builds).
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
