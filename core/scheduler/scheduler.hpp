#pragma once

#include "core/engine/commands.hpp"
#include "core/memory/node_pool.hpp"

#include <cassert>
#include <cstdint>
#include <vector>

namespace mach::scheduler {

class EDFScheduler {
  public:
    explicit EDFScheduler(std::size_t heap_size);

    [[nodiscard]] auto schedule(const engine::commands::CommandPayload& command,
                                uint64_t deadline_in_abs_sample) noexcept -> bool;

    void process_block(uint64_t current_abs_sample, std::size_t block_size,
                       memory::node_pool::NodePool& pool) noexcept;

  private:
    struct ScheduledCommand {
        uint64_t deadline_abs_sample;
        engine::commands::CommandPayload command;
    };

    constexpr static auto COMPARE_DEADLINE =
        [](const ScheduledCommand& lhs, const ScheduledCommand& rhs) noexcept -> bool {
        return lhs.deadline_abs_sample > rhs.deadline_abs_sample;
    };

    static void dispatch_command(const engine::commands::CommandPayload& cmd,
                                 memory::node_pool::NodePool& pool) noexcept;

    std::vector<ScheduledCommand> heap_;
};

} // namespace mach::scheduler
