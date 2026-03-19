#include "core/engine/engine.hpp"

#include "core/engine/command.hpp"

#include <algorithm>
#include <iostream>
#include <print>
#include <span>
#include <string>
#include <variant>

namespace mach::engine {
template<typename... Ts>
struct Overloaded : Ts... {
    using Ts::operator()...;
};

// NOTE: we fail fdst for now, will make more robust after getting callback to work
AudioEngine::AudioEngine(const EngineInitParams& params) noexcept
    : node_pool_ {params.max_node_pool_size}, sample_rate_ {params.sample_rate},
      block_size_ {params.block_size} {
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
    if (!command_queue_.try_push(RemoveNodePayload {.node_id = handle})) {
        // Propagate so controller knows to retry
        return std::unexpected<EngineError>(EngineError::COMMAND_QUEUE_FULL);
    }
    return {};
}

auto AudioEngine::set_node_parameter(AudioEngine::NodeHandleID handle, uint32_t param_id,
                                     float value) noexcept
    -> std::expected<void, EngineError> {
    if (!command_queue_.try_push(SetNodeParamPayload {
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
}

// NOLINTNEXTLINE
void AudioEngine::audio_callback(ma_device* device, void* output, const void* input,
                                 ma_uint32 frame_count) {
    auto* engine {static_cast<AudioEngine*>(device->pUserData)};
    auto output_buffer {
        std::span<float> {static_cast<float*>(output), frame_count * 2UZ}};

    CommandPayload cmd;
    while (engine->command_queue_.try_pop(cmd)) {
        std::visit(Overloaded {[&](const AddNodePayload& payload) -> void {
                                   engine->node_pool_.activate(payload.node_id);
                               },
                               [&](const RemoveNodePayload& payload) -> void {
                                   engine->node_pool_.deactivate(payload.node_id);
                               },
                               [&](const SetNodeParamPayload& payload) -> void {
                                   auto result {
                                       engine->node_pool_.get_node(payload.node_id)};
                                   if (!result) {
                                       return;
                                   }

                                   std::visit(Overloaded {[&](auto& node) -> void {
                                                  node.set_param(payload.update);
                                              }},
                                              *result.value());
                               }},
                   cmd);
    }
    std::ranges::fill(output_buffer, 0.0F);
    // TODO:
    engine->node_pool_.for_each_active_node(
        [&](auto& node) -> void { node.render_frame(output_buffer); });
}

} // namespace mach::engine
