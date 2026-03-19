#pragma once

#include "core/nodes/all_nodes.hpp"

#include <atomic>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <ranges>
#include <span>

namespace mach::memory::node_pool {
// Project targets little endian
static_assert(std::endian::native == std::endian::little,
              "NodePool handle packing assumes little-endian");

enum class NodeSlotState : uint8_t { FREE, ACQUIRED, ACTIVE, INACTIVE };

struct alignas(constants::ALIGNAS_SIZE) NodeSlot {
    std::atomic<NodeSlotState> current_state {NodeSlotState::FREE};
    std::optional<nodes::AnyDSPNode> node;
    uint32_t generation {};
};

enum class PoolError : uint8_t { CAPACITY_EXCEEDED };

struct NodeAddress {
    uint32_t slot_idx {};   // 32 bit address space
    uint32_t generation {}; // generation index, prevents potential use after free
};

class NodePool {
  public:
    explicit NodePool(uint32_t capacity)
        // NOLINTNEXTLINE  NOTE: see private defintion
        : slots_ {std::make_unique<NodeSlot[]>(capacity)}, CAPACITY {capacity} {};

    template<typename Node, typename... Args>
        requires(nodes::DSPNode<Node> && std::constructible_from<Node, Args...>)
    auto acquire(Args&&... args) noexcept -> std::expected<uint64_t, PoolError> {
        auto slots {std::span {slots_.get(), CAPACITY}};

        for (auto&& [idx, slot] : std::views::enumerate(slots)) {
            auto expected_state {NodeSlotState::FREE};
            if (slot.current_state.compare_exchange_strong(expected_state,
                                                           NodeSlotState::ACQUIRED)) {
                slot.node.emplace(Node {std::forward<Args>(args)...});
                return pack_node_to_handle({.slot_idx = static_cast<uint32_t>(idx),
                                            .generation = slot.generation});
            }
        }

        return std::unexpected {PoolError::CAPACITY_EXCEEDED};
    }

    using NodeHandleID = uint64_t;
    [[nodiscard]] auto activate(NodeHandleID handle) noexcept -> bool;
    [[nodiscard]] auto deactivate(NodeHandleID handle) noexcept -> bool;
    [[nodiscard]] auto get_node(NodeHandleID handle) noexcept
        -> std::optional<nodes::AnyDSPNode*>;
    void recycle(NodeHandleID handle) noexcept;

    template<typename F>
    void for_each_active_node(F&& func) noexcept {
        auto slots {std::span {slots_.get(), CAPACITY}};
        for (auto&& [idx, slot] : std::views::enumerate(slots)) {
            if (slot.current_state.load(std::memory_order_acquire)
                == NodeSlotState::ACTIVE) {
                std::visit(std::forward<F>(func), slot.node.value());
            }
        }
    }

  private:
    static auto pack_node_to_handle(NodeAddress addr) noexcept -> NodeHandleID {
        return std::bit_cast<NodeHandleID>(addr);
    };

    static auto unpack_node_from_handle(NodeHandleID handle) noexcept -> NodeAddress {
        return std::bit_cast<NodeAddress>(handle);
    };

    /*
     * NOTE: we use std::unique_ptr here to avoid memory leaks,
     * so we are good to use a C-style array
     */
    std::unique_ptr<NodeSlot[]> slots_; // NOLINT
    const uint32_t CAPACITY;            // NOLINT this is an actual constraint
};

} // namespace mach::memory::node_pool
