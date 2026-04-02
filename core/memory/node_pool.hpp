#pragma once

#include "core/nodes/all_nodes.hpp"

#include <atomic>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <ranges>
#include <span>

/**
 * @file node_pool.hpp
 * @brief Fixed-capacity, generation-tagged object pool for DSP nodes.
 *
 * @details Pre-allocates all node slots at engine start (`engine.play()`). No heap
 *          allocation occurs after initialisation. Each slot progresses through the
 *          lifecycle:
 *
 *          ```
 *          Free → Acquired → Active → Abandoned → Free
 *                          ↑                    ↑
 *                          └── Acquired ────────┘  (on queue-full rollback)
 *          ```
 *
 *          Generation tags prevent use-after-recycle: every handle encodes the
 *          generation at acquisition time. If the slot has been recycled (generation
 *          incremented), the handle is stale and all operations return failure/nullopt.
 *
 * @note This file assumes a little-endian target for the `bit_cast` handle packing.
 */

namespace mach::memory::node_pool {

// Project targets little endian
static_assert(std::endian::native == std::endian::little,
              "NodePool handle packing assumes little-endian");

/// @brief Error codes returned by pool operations via `std::expected`.
enum class PoolError : uint8_t {
    CAPACITY_EXCEEDED ///< All slots are occupied; no free slot could be acquired.
};

/**
 * @brief Fixed-capacity object pool for `AnyDSPNode` variants.
 *
 * @details Each `NodeSlot` is `alignas(ALIGNAS_SIZE)` to prevent false sharing between
 *          adjacent slots when the audio thread and the Janitor thread touch different
 *          slots concurrently.
 *
 *          **Thread ownership per operation:**
 *          | Operation               | Thread        |
 *          |-------------------------|---------------|
 *          | `acquire()`             | Python/Main   |
 *          | `activate()`            | Audio Thread  |
 *          | `get_node()`            | Audio Thread  |
 *          | `abandon_active_nodes()`| Audio Thread  |
 *          | `abandon_acquired_node()`| Python/Main  |
 *          | `recycle()`             | Janitor Thread|
 *          | `for_each_active_node()`| Audio Thread  |
 */
class NodePool {
public:
    /// @brief Opaque node handle: `bit_cast` of a `(slot_idx, generation)` pair.
    using NodeHandleID = uint64_t;

    /**
     * @brief Constructs the pool with the given number of pre-allocated slots.
     *
     * @param capacity Maximum number of concurrently live nodes.
     *
     * @note **Thread Safety:** Python/Main Thread — called during `AudioEngine`
     *       construction before any worker threads are started.
     */
    explicit NodePool(uint32_t capacity)
        // NOLINTNEXTLINE  NOTE: see private defintion
        : slots_ {std::make_unique<NodeSlot[]>(capacity)}, CAPACITY {capacity} {};

    /**
     * @brief Acquires a free slot and constructs a `Node` in it.
     *
     * @details Scans slots linearly for the first `FREE` slot, atomically CAS-es it to
     *          `ACQUIRED`, constructs the node via `emplace`, and returns a packed
     *          handle. If no free slot is found, returns `PoolError::CAPACITY_EXCEEDED`.
     *
     *          The node remains in `ACQUIRED` state until `activate()` is called by
     *          the audio thread after it pops the `AddNodePayload` command. This
     *          prevents the audio callback from seeing a partially constructed node.
     *
     * @tparam Node Must satisfy `DSPNode` and be constructible from `Args...`.
     * @param args  Arguments forwarded to `Node`'s constructor.
     * @return `NodeHandleID` on success, `PoolError::CAPACITY_EXCEEDED` on failure.
     *
     * @note **Thread Safety:** Python/Main Thread.
     */
    template<typename Node, typename... Args>
        requires(nodes::DSPNode<Node> && std::constructible_from<Node, Args...>)
    auto acquire(Args&&... args) noexcept -> std::expected<uint64_t, PoolError> {
        auto slots {std::span {slots_.get(), CAPACITY}};

        for (auto&& [idx, slot] : std::views::enumerate(slots)) {
            auto expected_state {SlotState::FREE};
            if (slot.current_state.compare_exchange_strong(expected_state,
                                                           SlotState::ACQUIRED)) {
                slot.node.emplace(Node {std::forward<Args>(args)...});
                return pack_node_to_handle({.slot_idx = static_cast<uint32_t>(idx),
                                            .generation = slot.generation});
            }
        }

        return std::unexpected {PoolError::CAPACITY_EXCEEDED};
    }

    /**
     * @brief Transitions a node slot from `ACQUIRED` to `ACTIVE`.
     *
     * @details Called by `EDFScheduler::dispatch_command()` when processing an
     *          `AddNodePayload`. Only after this call will `for_each_active_node()`
     *          visit the slot.
     *
     * @param handle Opaque handle returned by `acquire()`.
     * @return `true` on success; `false` if the generation is stale or the CAS fails.
     *
     * @note **Thread Safety:** Audio Thread. Real-time Safe.
     */
    [[nodiscard]] auto activate(NodeHandleID handle) noexcept -> bool;

    /**
     * @brief Transitions an `ACTIVE` node to `ABANDONED`.
     *
     * @details Called by `EDFScheduler::dispatch_command()` when processing a
     *          `RemoveNodePayload`. After this call the audio callback stops visiting
     *          the slot. The Janitor thread will later call `recycle()`.
     *
     * @param handle Opaque handle of the node to abandon.
     * @return `true` on success; `false` if the generation is stale or the CAS fails.
     *
     * @note **Thread Safety:** Audio Thread. Real-time Safe.
     */
    [[nodiscard]] auto abandon_active_nodes(NodeHandleID handle) noexcept -> bool;

    /**
     * @brief Transitions an `ACQUIRED` node to `ABANDONED` (rollback path).
     *
     * @details Called on the Python/main thread when `try_push()` into the SPSC queue
     *          fails after `acquire()` has already succeeded. Prevents the slot from
     *          being permanently stuck in `ACQUIRED`.
     *
     * @param handle Opaque handle of the node to roll back.
     * @return `true` on success; `false` if the generation is stale or the CAS fails.
     *
     * @note **Thread Safety:** Python/Main Thread.
     */
    [[nodiscard]] auto abandon_acquired_node(NodeHandleID handle) noexcept -> bool;

    /**
     * @brief Returns a pointer to the `AnyDSPNode` variant for the given handle.
     *
     * @details Returns `std::nullopt` if the handle is stale (generation mismatch) or
     *          if the slot is neither `ACTIVE` nor `ACQUIRED`. The pointer is valid only
     *          as long as the slot remains in one of those states.
     *
     * @param handle Opaque node handle.
     * @return Non-owning pointer wrapped in `std::optional`, or `std::nullopt`.
     *
     * @note **Thread Safety:** Audio Thread. Real-time Safe.
     */
    [[nodiscard]] auto get_node(NodeHandleID handle) noexcept
        -> std::optional<nodes::AnyDSPNode*>;

    /**
     * @brief Recycles an `ABANDONED` slot back to `FREE`.
     *
     * @details Destructs the `std::optional<AnyDSPNode>`, increments the generation
     *          counter to invalidate any outstanding handles, and `release`-stores
     *          `FREE` so a future `acquire()` can reuse the slot.
     *
     * @param handle Opaque handle of the node to recycle. Must be in `ABANDONED` state
     *               (asserted).
     *
     * @note **Thread Safety:** Janitor Thread.
     */
    void recycle(NodeHandleID handle) noexcept;

    /**
     * @brief Invokes `func` on every node currently in `ACTIVE` state.
     *
     * @details Used by the audio callback to apply per-block operations (e.g.,
     *          `set_sample_rate`) across all live nodes. Slot state is loaded with
     *          `memory_order_acquire` to ensure any preceding `activate()` write is
     *          visible.
     *
     * @tparam F Callable accepting `AnyDSPNode&` (via `std::visit` internally).
     * @param func Visitor forwarded to `std::visit` for each active slot's variant.
     *
     * @note **Thread Safety:** Audio Thread. Real-time Safe.
     */
    template<typename F>
    void for_each_active_node(F&& func) noexcept {
        auto slots {std::span {slots_.get(), CAPACITY}};
        for (auto&& [idx, slot] : std::views::enumerate(slots)) {
            if (slot.current_state.load(std::memory_order_acquire) == SlotState::ACTIVE) {
                std::visit(std::forward<F>(func), slot.node.value());
            }
        }
    }

private:
    /// @brief Lifecycle state of a single pool slot.
    enum class SlotState : uint8_t { FREE, ACQUIRED, ACTIVE, ABANDONED };

    /**
     * @brief Decomposed view of a `NodeHandleID` for slot lookup and generation check.
     */
    struct NodeAddress {
        uint32_t slot_idx {};   ///< Index into `slots_`.
        uint32_t generation {}; ///< Generation at the time of `acquire()`.
    };

    /**
     * @brief One node pool slot.
     *
     * @details `alignas(ALIGNAS_SIZE)` ensures each slot occupies its own cache line,
     *          preventing false sharing when the audio thread and Janitor thread access
     *          different slots simultaneously.
     */
    struct alignas(constants::ALIGNAS_SIZE) NodeSlot {
        std::atomic<SlotState>        current_state {SlotState::FREE};
        std::optional<nodes::AnyDSPNode> node;
        uint32_t                      generation {};
    };

    /// @brief Packs a `NodeAddress` into a `NodeHandleID` via `std::bit_cast`.
    static auto pack_node_to_handle(NodeAddress addr) noexcept -> NodeHandleID {
        return std::bit_cast<NodeHandleID>(addr);
    };

    /// @brief Unpacks a `NodeHandleID` back into a `NodeAddress` via `std::bit_cast`.
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
