#include "core/engine/engine.hpp"

#include <iostream>
#include <print>
#include <string>

namespace mach::engine {
// NOTE: we fail fdst for now, will make more robust after getting callback to work
AudioEngine::AudioEngine(const EngineInitParams& params) noexcept
    : sample_rate_ {params.sample_rate}, block_size_ {params.block_size} {
    ma_device_config config {ma_device_config_init(ma_device_type_playback)};
    config.playback.format = ma_format_f32;
    config.playback.channels = 2;
    config.sampleRate = sample_rate_;
    config.dataCallback = audio_callback;
    config.pUserData = this;

    ma_result result = ma_device_init(nullptr, &config, &device_);
    // XXX: move ma_result checking into its own func?
    if (result != MA_SUCCESS) {
        std::println(std::cerr, "Failed to initialize audio device!: {}", std::to_string(result));
        std::terminate();
    }

    wt_oscillator_.set_sample_rate(sample_rate_);
}

AudioEngine::~AudioEngine() noexcept {
    ma_device_uninit(&device_);
}

void AudioEngine::play() noexcept {
    ma_result result = ma_device_start(&device_);
    if (result != MA_SUCCESS) {
        std::println(std::cerr, "Failed to start audio device!: {}", std::to_string(result));
        std::terminate();
    }
}

void AudioEngine::stop() noexcept {
    ma_result result = ma_device_stop(&device_);
    if (result != MA_SUCCESS) {
        std::println(std::cerr, "Failed to stop audio device!: {}", std::to_string(result));
        std::terminate();
    }
}

// NOLINTNEXTLINE
void AudioEngine::audio_callback(ma_device* device, void* output, const void* input,
                                 ma_uint32 frame_count) {
    auto* engine {static_cast<AudioEngine*>(device->pUserData)};
    auto* out {static_cast<float*>(output)};

    std::span<float> output_buffer {out, frame_count * 2UZ};
    engine->wt_oscillator_.render_frame(output_buffer);
}

} // namespace mach::engine
