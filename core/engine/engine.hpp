#pragma once

#include <cstdint>

namespace mach {

struct EngineInitParams {
    float    sample_rate;
    uint32_t block_size;
};

class Engine {
  public:
    explicit Engine(const EngineInitParams& params)
        : sample_rate_{params.sample_rate}, block_size_{params.block_size} {}

  private:
    float    sample_rate_;
    uint32_t block_size_;
};

} // namespace mach
