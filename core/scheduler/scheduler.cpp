#include "core/scheduler/scheduler.hpp"

#include <algorithm>
#include <variant>

namespace mach::scheduler {

template<typename... Ts>
struct Overloaded : Ts... {
    using Ts::operator()...;
};

void EDFScheduler::dispatch_command(const engine::commands::detail::CommandPayload& cmd,
                                    memory::node_pool::NodePool& pool,
                                    janitor::JanitorThread& janitor,
                                    graph::ConnectionTable& connections) noexcept {
    using namespace engine::commands::detail;
    std::visit(Overloaded {
                   [&](const AddNodePayload& payload) -> void {
                       [[maybe_unused]] auto activated {pool.activate(payload.node_id)};
                       assert(activated);
                   },
                   [&](const RemoveNodePayload& payload) -> void {
                       connections.remove_all_for(payload.node_id);
                       [[maybe_unused]] auto deactivated {
                           pool.abandon_active_nodes(payload.node_id)};
                       assert(deactivated);
                       [[maybe_unused]] auto enqueued {
                           janitor.enqueue_dead_node(payload.node_id)};
                       assert(enqueued);
                   },
                   [&](const SetNodeParamPayload& payload) -> void {
                       auto node {pool.get_node(payload.node_id)};
                       if (!node) {
                           return;
                       }
                       std::visit(
                           [&](auto& node) -> void { node.set_param(payload.update); },
                           *node.value());
                   },
                   [&](const ConnectNodesPayload& payload) -> void {
                       [[maybe_unused]] auto added {
                           connections.add(payload.source_id, payload.dest_id)};
                       assert(added);
                   },
                   [&](const DisconnectNodesPayload& payload) -> void {
                       connections.remove(payload.source_id, payload.dest_id);
                   },
               },
               cmd);
}

EDFScheduler::EDFScheduler(std::size_t heap_size) {
    heap_.reserve(heap_size);
};

auto EDFScheduler::schedule(const engine::commands::detail::CommandPayload& command,
                            uint64_t deadline_in_abs_sample) noexcept -> bool {
    [[unlikely]] if (heap_.size() == heap_.capacity()) { return false; }

    heap_.push_back({.deadline_abs_sample = deadline_in_abs_sample, .command = command});
    std::ranges::push_heap(heap_, COMPARE_DEADLINE);
    return true;
}

void EDFScheduler::process_block(uint64_t current_abs_sample, std::size_t block_size,
                                 memory::node_pool::NodePool& pool,
                                 janitor::JanitorThread& janitor,
                                 graph::ConnectionTable& connections) noexcept {
    uint64_t block_end {current_abs_sample + block_size};
    while (!heap_.empty() && heap_.front().deadline_abs_sample < block_end) {
        std::ranges::pop_heap(heap_, COMPARE_DEADLINE);
        auto cmd {heap_.back()};
        heap_.pop_back();
        dispatch_command(cmd.command, pool, janitor, connections);
    }
}

} // namespace mach::scheduler
