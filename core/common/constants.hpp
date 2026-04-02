#pragma once

#include <cstddef>
#include <cstdint>
#include <new>
#include <numbers>

/**
 * @file constants.hpp
 * @brief Compile-time engine-wide constants shared across all subsystems.
 *
 * @note All values are compile-time constants. No thread safety concerns apply.
 */

namespace mach::constants {

/**
 * @brief Cache-line size used for `alignas` annotations.
 *
 * @details Equals `std::hardware_destructive_interference_size` (typically 64 bytes on
 *          x86-64). Apply to any struct that holds atomic indices accessed by different
 *          threads (e.g., SPSC head/tail, telemetry counters) to prevent false sharing.
 *
 * @note **Thread Safety:** Compile-time constant — no thread concerns.
 */
constexpr std::size_t ALIGNAS_SIZE {std::hardware_destructive_interference_size};

/**
 * @brief Default audio device sample rate in Hz.
 * @note **Thread Safety:** Compile-time constant — no thread concerns.
 */
constexpr auto DEFAULT_SAMPLE_RATE {44100.0F};

/**
 * @brief Default number of frames per audio callback block.
 * @note **Thread Safety:** Compile-time constant — no thread concerns.
 */
constexpr std::size_t DEFAULT_BLOCK_SIZE {512UZ};

/**
 * @brief Default playback tempo in beats per minute.
 * @note **Thread Safety:** Compile-time constant — no thread concerns.
 */
constexpr double DEFAULT_BPM {120.0};

/**
 * @brief Default maximum node pool capacity (number of slots).
 * @note **Thread Safety:** Compile-time constant — no thread concerns.
 */
constexpr uint32_t DEFAULT_MAX_POOL_SIZE {64U};

/**
 * @brief Concert pitch A4 in Hz (ISO 16 standard tuning).
 * @note **Thread Safety:** Compile-time constant — no thread concerns.
 */
constexpr auto NOTE_A4 {440.0F};

/**
 * @brief 2π as a single-precision float.
 *
 * @details Used in phase accumulator computations where a full cycle maps to [0, 2π).
 *
 * @note **Thread Safety:** Compile-time constant — no thread concerns.
 */
constexpr auto TWO_TIMES_PI_FLOAT {2.0F * std::numbers::pi_v<float>};

/**
 * @brief 2^32 as a single-precision float.
 *
 * @details Converts a normalised frequency ratio (freq / sample_rate) to a fixed-point
 *          uint32_t phase increment that wraps naturally at 2^32, enabling branchless
 *          wavetable indexing via bit-masking.
 *
 * @note **Thread Safety:** Compile-time constant — no thread concerns.
 */
constexpr auto TWO_TO_POWER_OF_32 {4294967296.0F};

} // namespace mach::constants
