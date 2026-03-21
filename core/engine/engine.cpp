#include "core/engine/engine.hpp"

#include "core/engine/commands.hpp"

#include <algorithm>
#include <bit>
#include <iostream>
#include <print>
#include <span>
#include <string>

namespace mach::engine {
AudioEngine::AudioEngine(const EngineInitParams& params) noexcept
    : node_pool_ {params.max_node_pool_size},
      command_queue_ {
          std::bit_ceil(static_cast<std::size_t>(params.max_node_pool_size) * 4UZ)},
      sample_rate_ {params.sample_rate}, block_size_ {params.block_size},
      event_scheduler_ {
          std::bit_ceil(static_cast<std::size_t>(params.max_node_pool_size) * 4UZ)},
      janitor_ {node_pool_,
                std::bit_ceil(static_cast<std::size_t>(params.max_node_pool_size))} {
    ma_device_config config {ma_device_config_init(ma_device_type_playback)};
    config.playback.format = ma_format_f32;
    config.playback.channels = 2;
    config.sampleRate = sample_rate_;
    config.dataCallback = audio_callback;
    config.pUserData = this;

    ma_result result = ma_device_init(nullptr, &config, &device_);
    if (result != MA_SUCCESS) {
        std::println(std::cerr, "Failed to initialize audio device!: {}",
                     std::to_string(result));
        std::terminate();
    }
}

AudioEngine::~AudioEngine() noexcept {
    ma_device_uninit(&device_);
}

auto AudioEngine::remove_node(AudioEngine::NodeHandleID handle) noexcept
    -> std::expected<void, EngineError> {
    if (!command_queue_.try_push(
            commands::detail::RemoveNodePayload {.node_id = handle})) {
        // Propagate so controller knows to retry
        return std::unexpected<EngineError>(EngineError::COMMAND_QUEUE_FULL);
    }
    return {};
}

auto AudioEngine::set_node_parameter(AudioEngine::NodeHandleID handle, uint32_t param_id,
                                     float value) noexcept
    -> std::expected<void, EngineError> {
    if (!command_queue_.try_push(commands::detail::SetNodeParamPayload {
            .node_id = handle, .update = {.param_id = param_id, .value = value}})) {
        return std::unexpected<EngineError>(EngineError::COMMAND_QUEUE_FULL);
    }

    return {};
}

void AudioEngine::play() noexcept {
    ma_result result = ma_device_start(&device_);
    if (result != MA_SUCCESS) {
        std::println(std::cerr, "Failed to start audio device!: {}",
                     std::to_string(result));
        std::terminate();
    }
}

void AudioEngine::stop() noexcept {
    ma_result result = ma_device_stop(&device_);
    if (result != MA_SUCCESS) {
        std::println(std::cerr, "Failed to stop audio device!: {}",
                     std::to_string(result));
        std::terminate();
    }

    // drain remaining commands in command queue
    commands::detail::CommandPayload cmd;
    while (command_queue_.try_pop(cmd)) {
        [[maybe_unused]] auto scheduled {event_scheduler_.schedule(cmd, current_sample_)};
        assert(scheduled);
    }

    event_scheduler_.process_block(current_sample_, 0, node_pool_, janitor_);
}

// NOLINTNEXTLINE we are matching miniaudio API
void AudioEngine::audio_callback(ma_device* device, void* output, const void* input,
                                 ma_uint32 frame_count) {
    static_cast<void>(input);

    auto* engine {static_cast<AudioEngine*>(device->pUserData)};
    auto output_buffer {
        std::span<float> {static_cast<float*>(output), frame_count * 2UZ}};

    commands::detail::CommandPayload cmd;
    while (engine->command_queue_.try_pop(cmd)) {
        [[maybe_unused]] auto scheduled {
            engine->event_scheduler_.schedule(cmd, engine->current_sample_)};
        assert(scheduled);
    }

    engine->event_scheduler_.process_block(engine->current_sample_, frame_count,
                                           engine->node_pool_, engine->janitor_);

    std::ranges::fill(output_buffer, 0.0F);
    engine->node_pool_.for_each_active_node(
        [&](auto& node) -> void { node.render_frame(output_buffer); });

    engine->current_sample_ += frame_count;
}

} // namespace mach::engine
