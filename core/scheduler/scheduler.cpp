#include "core/scheduler/scheduler.hpp"

#include <algorithm>
#include <variant>

namespace mach::scheduler {

template<typename... Ts>
struct Overloaded : Ts... {
    using Ts::operator()...;
};

void EDFScheduler::dispatch_command(const engine::commands::CommandPayload& cmd,
                                    memory::node_pool::NodePool& pool) noexcept {
    using namespace engine::commands;
    std::visit(
        Overloaded {
            [&](const AddNodePayload& payload) -> void {
                [[maybe_unused]] auto activated {pool.activate(payload.node_id)};
                assert(activated);
            },
            [&](const RemoveNodePayload& payload) -> void {
                [[maybe_unused]] auto deactivated {pool.deactivate(payload.node_id)};
                assert(deactivated);
            },
            [&](const SetNodeParamPayload& payload) -> void {
                auto node {pool.get_node(payload.node_id)};
                if (!node) {
                    return;
                }
                std::visit([&](auto& node) -> void { node.set_param(payload.update); },
                           *node.value());
            },
        },
        cmd);
}

EDFScheduler::EDFScheduler(std::size_t heap_size) {
    heap_.reserve(heap_size);
};

auto EDFScheduler::schedule(const engine::commands::CommandPayload& command,
                            uint64_t deadline_in_abs_sample) noexcept -> bool {
    [[unlikely]] if (heap_.size() == heap_.capacity()) { return false; }

    heap_.push_back({.deadline_abs_sample = deadline_in_abs_sample, .command = command});
    std::ranges::push_heap(heap_, COMPARE_DEADLINE);
    return true;
}

void EDFScheduler::process_block(uint64_t current_abs_sample, std::size_t block_size,
                                 memory::node_pool::NodePool& pool) noexcept {
    uint64_t block_end {current_abs_sample + block_size};
    while (!heap_.empty() && heap_.front().deadline_abs_sample < block_end) {
        std::ranges::pop_heap(heap_, COMPARE_DEADLINE);
        auto cmd {heap_.back()};
        heap_.pop_back();
        dispatch_command(cmd.command, pool);
    }
}

} // namespace mach::scheduler
