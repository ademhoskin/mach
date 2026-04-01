#pragma once

#include "core/common/constants.hpp"
#include "core/common/time.hpp"
#include "core/engine/commands.hpp"
#include "core/graph/connection_table.hpp"
#include "core/ipc/spsc_queue.hpp"
#include "core/memory/node_pool.hpp"
#include "core/scheduler/scheduler.hpp"

#include <cassert>
#include <cstdint>
#include <expected>

#include <miniaudio.h>

namespace mach::engine {

struct EngineInitParams {
    uint32_t sample_rate;
    std::size_t block_size;
    uint32_t max_node_pool_size;
    double bpm;
};

enum class EngineError : uint8_t {
    POOL_CAPACITY_EXCEEDED,
    COMMAND_QUEUE_FULL,
    INVALID_PARAMETER
};

class AudioEngine {
public:
    explicit AudioEngine(const EngineInitParams& params);
    ~AudioEngine() noexcept;

    AudioEngine(const AudioEngine&) = delete;
    auto operator=(const AudioEngine&) -> AudioEngine& = delete;

    AudioEngine(AudioEngine&&) = delete;
    auto operator=(AudioEngine&&) -> AudioEngine& = delete;

    using NodeHandleID = memory::node_pool::NodePool::NodeHandleID;

    template<typename Node, typename... Args>
        requires(nodes::DSPNode<Node> && std::constructible_from<Node, Args...>)
    auto add_node(Args&&... args) noexcept -> std::expected<NodeHandleID, EngineError> {
        auto acquire_result {node_pool_.acquire<Node>(std::forward<Args>(args)...)};
        if (!acquire_result) {
            return std::unexpected<EngineError>(EngineError::POOL_CAPACITY_EXCEEDED);
        }

        auto handle {acquire_result.value()};

        auto acquired_node {node_pool_.get_node(handle)};
        std::visit([this](auto& node) -> void { node.set_sample_rate(sample_rate_); },
                   *acquired_node.value());

        if (!command_queue_.try_push({.command = commands::detail::AddNodePayload {
                                             .node_id = handle},
                                         .deadline_abs_sample = 0})) {
            [[maybe_unused]] auto abandon_result {
                node_pool_.abandon_acquired_node(handle)};
            assert(abandon_result);
            janitor_.enqueue_dead_node(handle);
            return std::unexpected<EngineError>(EngineError::COMMAND_QUEUE_FULL);
        }
        return handle;
    };

    auto remove_node(const NodeHandleID& handle) noexcept
        -> std::expected<void, EngineError>;

    auto set_node_parameter(NodeHandleID handle, uint32_t param_id, float value) noexcept
        -> std::expected<void, EngineError>;

    auto connect(NodeHandleID source, NodeHandleID dest) noexcept
        -> std::expected<void, EngineError>;

    auto disconnect(NodeHandleID source, NodeHandleID dest) noexcept
        -> std::expected<void, EngineError>;

    auto set_bpm(double bpm) noexcept -> std::expected<void, EngineError>;

    auto schedule(commands::detail::CommandPayload cmd, TimeSpec time) noexcept
        -> std::expected<void, EngineError>;

    [[nodiscard]] auto get_master_output() const noexcept -> NodeHandleID;

    void sleep(TimeSpec time) noexcept;

    void play() noexcept;
    void stop() noexcept;

private:
    static void audio_callback(ma_device* device, void* output, const void* input,
                               ma_uint32 frame_count);

    memory::node_pool::NodePool node_pool_;
    ipc::SPSCQueue<commands::detail::ScheduledCommandPayload> command_queue_;
    uint32_t sample_rate_;
    std::size_t block_size_;

    ma_device device_ {};

    scheduler::EDFScheduler event_scheduler_;
    janitor::JanitorThread janitor_;
    graph::ConnectionTable connection_table_;
    NodeHandleID master_output_id_;

    std::atomic<uint64_t> current_sample_ {0ULL};
    std::atomic<double> bpm_ {constants::DEFAULT_BPM}; // NOLINT
};

} // namespace mach::engine
