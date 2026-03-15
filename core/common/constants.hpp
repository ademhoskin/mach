#pragma once

#include <cstddef>
#include <new>
#include <numbers>

namespace mach::constants {

// used to mitigate false sharing, based on harward cache line
constexpr std::size_t ALIGNAS_SIZE {std::hardware_destructive_interference_size};
constexpr auto DEFAULT_SAMPLE_RATE {44100.0F};
constexpr auto DEFAULT_BLOCK_SIZE {512};
constexpr auto NOTE_A4 {440.0F};
constexpr auto TWO_TIMES_PI_FLOAT {2.0F * std::numbers::pi_v<float>};
constexpr auto TWO_TO_POWER_OF_32 {4294967296.0F};

} // namespace mach::constants
