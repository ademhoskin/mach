#pragma once

#include "core/engine/commands.hpp"
#include "core/ipc/spsc_queue.hpp"
#include "core/memory/node_pool.hpp"
#include "core/scheduler/scheduler.hpp"

#include <cassert>
#include <cstdint>
#include <expected>

#include <miniaudio.h>

namespace mach::engine {
constexpr auto COMMAND_QUEUE_SIZE {1024UZ};

struct EngineInitParams {
    uint32_t sample_rate;
    std::size_t block_size;
    uint32_t max_node_pool_size;
};

enum class EngineError : uint8_t { POOL_CAPACITY_EXCEEDED, COMMAND_QUEUE_FULL };

class AudioEngine {
  public:
    explicit AudioEngine(const EngineInitParams& params) noexcept;
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
            // propagate all the way up to the user
            return std::unexpected<EngineError>(EngineError::POOL_CAPACITY_EXCEEDED);
        }

        auto handle {acquire_result.value()};

        auto acquired_node {node_pool_.get_node(handle)};
        std::visit([this](auto& node) -> void { node.set_sample_rate(sample_rate_); },
                   *acquired_node.value());

        // NOTE: we activate when we pop from queue, if we fail, janitor recycles
        if (!command_queue_.try_push(commands::AddNodePayload {.node_id = handle})) {
            return std::unexpected<EngineError>(EngineError::COMMAND_QUEUE_FULL);
        }
        return handle;
    };

    auto remove_node(NodeHandleID handle) noexcept -> std::expected<void, EngineError>;

    auto set_node_parameter(NodeHandleID handle, uint32_t param_id, float value) noexcept
        -> std::expected<void, EngineError>;

    void play() noexcept;
    void stop() noexcept;

  private:
    static void audio_callback(ma_device* device, void* output, const void* input,
                               ma_uint32 frame_count);

    memory::node_pool::NodePool node_pool_;
    ipc::SPSCQueue<commands::CommandPayload, COMMAND_QUEUE_SIZE> command_queue_ {};
    uint32_t sample_rate_;
    std::size_t block_size_;

    ma_device device_ {};

    scheduler::EDFScheduler event_scheduler_;
    uint64_t current_sample_ {0ULL};
};

} // namespace mach::engine
