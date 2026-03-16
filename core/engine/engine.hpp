#pragma once

#include "core/nodes/wavetable/wavetable_osc.hpp"

#include <cstdint>

#include <miniaudio.h>

namespace mach::engine {

struct EngineInitParams {
    uint32_t sample_rate;
    uint32_t block_size;
};

class AudioEngine {
  public:
    explicit AudioEngine(const EngineInitParams& params) noexcept;
    ~AudioEngine() noexcept;

    AudioEngine(const AudioEngine&) = delete;
    auto operator=(const AudioEngine&) -> AudioEngine& = delete;

    AudioEngine(AudioEngine&&) = delete;
    auto operator=(AudioEngine&&) -> AudioEngine& = delete;

    void play() noexcept;
    void stop() noexcept;

  private:
    static void audio_callback(ma_device* device, void* output, const void* input,
                               ma_uint32 frame_count);

    uint32_t sample_rate_;
    uint32_t block_size_;
    // TODO: replace with node pool when implemented, using sine wavetable to test audio callback
    nodes::wavetable::WavetableOscillator wt_oscillator_;
    ma_device device_ {};
};

} // namespace mach::engine
