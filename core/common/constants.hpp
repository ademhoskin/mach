#pragma once

#include <cstddef>
#include <new>

namespace mach::constants {
// used to mitigate false sharing, based on harward cache line
constexpr std::size_t ALIGNAS_SIZE {std::hardware_destructive_interference_size};
constexpr auto DEFAULT_SAMPLE_RATE {44100.0F};
constexpr auto DEFAULT_BLOCK_SIZE {512};
} // namespace mach::constants
