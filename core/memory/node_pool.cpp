#include "core/memory/node_pool.hpp"

#include <cassert>

namespace mach::memory::node_pool {

auto NodePool::activate(NodeHandleID handle) noexcept -> bool {
    auto [idx, generation] {unpack_node_from_handle(handle)};
    if (generation != slots_[idx].generation) {
        return false;
    }

    auto expected_state {SlotState::ACQUIRED};
    return slots_[idx].current_state.compare_exchange_strong(expected_state,
                                                             SlotState::ACTIVE);
}

auto NodePool::abandon_active_nodes(NodeHandleID handle) noexcept -> bool {
    auto [idx, generation] {unpack_node_from_handle(handle)};
    if (generation != slots_[idx].generation) {
        return false;
    }

    auto expected_state {SlotState::ACTIVE};
    return slots_[idx].current_state.compare_exchange_strong(expected_state,
                                                             SlotState::ABANDONED);
}

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

auto NodePool::abandon_acquired_node(NodeHandleID handle) noexcept -> bool {
    auto [idx, generation] {unpack_node_from_handle(handle)};
    if (generation != slots_[idx].generation) {
        return false;
    }

    auto expected_state {SlotState::ACQUIRED};
    return slots_[idx].current_state.compare_exchange_strong(expected_state,
                                                             SlotState::ABANDONED);
}

void NodePool::recycle(NodeHandleID handle) noexcept {
    auto [idx, generation] {unpack_node_from_handle(handle)};
    assert(slots_[idx].current_state == SlotState::ABANDONED);
    slots_[idx].node.reset();
    slots_[idx].generation++;
    slots_[idx].current_state.store(SlotState::FREE, std::memory_order_release);
}

} // namespace mach::memory::node_pool
