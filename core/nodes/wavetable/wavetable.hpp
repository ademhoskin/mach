#pragma once
#include "core/common/constants.hpp"

#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <numbers>

/**
 * @file wavetable.hpp
 * @brief Compile-time wavetable generation and runtime linear-interpolation lookup.
 *
 * @details All wavetables are generated at compile time via `consteval` shape functors
 *          and stored as `static constexpr std::array<float, TableSize>`. At runtime the
 *          only hot-path operation is `get_interpolated_sample()` — a few integer shifts,
 *          a mask, and a lerp — with zero heap interaction.
 *
 * @note **Thread Safety:** All state is `static constexpr`; the lookup functions are
 *       stateless and reentrant. Safe to call from the audio thread.
 */

/// @brief Compile-time wavetable shapes and the interpolating Wavetable<> template.
namespace mach::nodes::wavetable {

// TODO: split shapes
/// @brief Default number of samples in each wavetable. Power-of-two for bitmask wrapping.
constexpr std::size_t DEFAULT_TABLE_SIZE {4096UZ};

/**
 * @brief Compile-time sine shape using a 6-term Maclaurin series.
 *
 * @details `std::sin` is not `constexpr`, so the series approximation is used instead.
 *          Six terms give sufficient accuracy across [-π, π] for audio purposes.
 *
 * @note **Thread Safety:** `consteval` — evaluated only at compile time.
 */
struct SineShape {
    /**
     * @brief Evaluates sin(x) via a 6-term Maclaurin series.
     * @param SAMPLE_INPUT Phase angle in radians, centred in [-π, π].
     * @return Approximated sine value in [-1, 1].
     */
    static consteval auto compute(const float SAMPLE_INPUT) noexcept -> float {
        const auto INPUT_SQUARED {SAMPLE_INPUT * SAMPLE_INPUT};
        const auto INPUT_CUBED {INPUT_SQUARED * SAMPLE_INPUT};
        const auto INPUT_QUINTIC {INPUT_CUBED * INPUT_SQUARED};
        const auto INPUT_SEPTIC {INPUT_QUINTIC * INPUT_SQUARED};
        const auto INPUT_NONIC {INPUT_SEPTIC * INPUT_SQUARED};
        const auto INPUT_UNDECIC {INPUT_NONIC * INPUT_SQUARED};

        const auto THREE_FACTORIAL {6.0F};
        const auto FIVE_FACTORIAL {120.0F};
        const auto SEVEN_FACTORIAL {5040.0F};
        const auto NINE_FACTORIAL {362880.0F};
        const auto ELEVEN_FACTORIAL {39916800.0F};

        return SAMPLE_INPUT - (INPUT_CUBED / THREE_FACTORIAL)
               + (INPUT_QUINTIC / FIVE_FACTORIAL) - (INPUT_SEPTIC / SEVEN_FACTORIAL)
               + (INPUT_NONIC / NINE_FACTORIAL) - (INPUT_UNDECIC / ELEVEN_FACTORIAL);
    }
};

/**
 * @brief Compile-time sawtooth shape.
 *
 * @note **Thread Safety:** `consteval` — evaluated only at compile time.
 */
struct SawtoothShape {
    /**
     * @brief Maps a phase in [-π, π] to a linear ramp in [-1, 1].
     * @param PHASE_INPUT Phase angle in radians, centred in [-π, π].
     * @return Sawtooth value in [-1/π, 1/π] (≈ [-0.318, 0.318]).
     */
    static consteval auto compute(const float PHASE_INPUT) noexcept -> float {
        return PHASE_INPUT * std::numbers::inv_pi_v<float>;
    };
};

/**
 * @brief Compile-time triangle shape.
 *
 * @note **Thread Safety:** `consteval` — evaluated only at compile time.
 */
struct TriangleShape {
    /**
     * @brief Maps a phase in [-π, π] to a triangle wave in [-1, 0].
     * @param PHASE_INPUT Phase angle in radians, centred in [-π, π].
     * @return Triangle value.
     */
    static consteval auto compute(const float PHASE_INPUT) noexcept -> float {
        // NOLINTNEXTLINE
        return -(1.0F - (2.0F * std::abs(PHASE_INPUT) * std::numbers::inv_pi_v<float>));
    }
};

/**
 * @brief Compile-time square shape.
 *
 * @note **Thread Safety:** `consteval` — evaluated only at compile time.
 */
struct SquareShape {
    /**
     * @brief Returns -1 for negative phase, +1 for non-negative phase.
     * @param PHASE_INPUT Phase angle in radians, centred in [-π, π].
     * @return -1.0f or 1.0f.
     */
    static consteval auto compute(const float PHASE_INPUT) noexcept -> float {
        return (PHASE_INPUT < 0.0F) ? -1.0F : 1.0F;
    }
};

/**
 * @brief Maps a table index to a phase angle centred in [-π, π].
 *
 * @details Used during compile-time table generation to ensure all shapes receive a
 *          correctly centred input domain.
 *
 * @param IDX Table index in [0, DEFAULT_TABLE_SIZE).
 * @return Phase angle in radians in [-π, π].
 *
 * @note **Thread Safety:** `constexpr` free function — no mutable state.
 */
constexpr auto compute_centered_phase_angle(const std::size_t IDX) noexcept -> float {
    const auto CALCULATED_ANGLE {mach::constants::TWO_TIMES_PI_FLOAT
                                 * static_cast<float>(IDX)
                                 / static_cast<float>(DEFAULT_TABLE_SIZE)};

    return (CALCULATED_ANGLE > std::numbers::pi_v<float>)
               ? (CALCULATED_ANGLE - mach::constants::TWO_TIMES_PI_FLOAT)
               : CALCULATED_ANGLE;
};

/**
 * @brief Compile-time wavetable with runtime linear-interpolation lookup.
 *
 * @details The full table is generated at compile time by calling `Shape::compute()`
 *          for every sample. At runtime, `get_interpolated_sample()` maps a uint32_t
 *          phase accumulator (full-range 0–2^32) to a table position using fixed-point
 *          arithmetic:
 *          - Upper `INDEX_BITS` bits → integer table index.
 *          - Lower `FRACTION_BITS` bits → linear interpolation weight.
 *
 * @tparam Shape    A shape functor providing `static consteval float compute(float)`.
 * @tparam TableSize Number of samples in the table. Must be a power of two.
 *
 * @note **Thread Safety:** All data is `static constexpr`. `get_interpolated_sample()`
 *       is a pure function; safe to call from the audio thread.
 */
template<typename Shape, std::size_t TableSize = DEFAULT_TABLE_SIZE>
class Wavetable {
public:
    /**
     * @brief Looks up and linearly interpolates a sample for the given phase.
     *
     * @details Uses fixed-point arithmetic to split the 32-bit phase accumulator into a
     *          table index and a fractional blend weight, then lerps between adjacent
     *          table entries. Wrapping is handled by `& IDX_MASK` (power-of-two table).
     *
     * @param PHASE Full-range uint32_t phase accumulator (wraps at 2^32).
     * @return Interpolated sample value in the shape's output range.
     *
     * @par Complexity O(1)
     * @note **Thread Safety:** Audio Thread. Real-time Safe.
     */
    [[nodiscard]] static auto get_interpolated_sample(const uint32_t PHASE) noexcept
        -> float {
        static constexpr auto INDEX_BITS {std::bit_width(TABLE_SIZE) - 1};
        static constexpr auto FRACTION_BITS {32U - INDEX_BITS};
        static constexpr float FRACTION_SCALE {1.0F
                                               / static_cast<float>(1U << FRACTION_BITS)};

        const auto KNOWN_POINT_ONE {static_cast<std::size_t>(PHASE >> FRACTION_BITS)
                                    & IDX_MASK};
        const auto KNOWN_POINT_TWO {(KNOWN_POINT_ONE + 1UZ) & IDX_MASK};
        const auto FLOAT_KNOWN_POINT_ONE_DIFF {
            static_cast<float>(PHASE & ((1U << FRACTION_BITS) - 1U)) * FRACTION_SCALE};

        const float SAMPLE_ONE {WAVETABLE[KNOWN_POINT_ONE]};
        const float SAMPLE_TWO {WAVETABLE[KNOWN_POINT_TWO]};
        const float SLOPE {SAMPLE_TWO - SAMPLE_ONE};

        return SAMPLE_ONE + (FLOAT_KNOWN_POINT_ONE_DIFF * SLOPE);
    };

private:
    /// @brief Generates the full wavetable at compile time.
    static constexpr auto generate() noexcept -> std::array<float, TableSize> {
        std::array<float, TableSize> wavetable {};
        auto idx {0UZ};
        for (auto& value : wavetable) {
            value = Shape::compute(compute_centered_phase_angle(idx++));
        }
        return wavetable;
    }
    static constexpr auto        WAVETABLE {generate()};
    static constexpr auto        TABLE_SIZE {TableSize};
    static constexpr std::size_t IDX_MASK {TABLE_SIZE - 1UZ};
};

/// @brief Sine wavetable with 4096 samples generated at compile time.
using SineWavetable = Wavetable<SineShape>;
/// @brief Sawtooth wavetable with 4096 samples generated at compile time.
using SawtoothWavetable = Wavetable<SawtoothShape>;
/// @brief Triangle wavetable with 4096 samples generated at compile time.
using TriangleWavetable = Wavetable<TriangleShape>;
/// @brief Square wavetable with 4096 samples generated at compile time.
using SquareWavetable = Wavetable<SquareShape>;

} // namespace mach::nodes::wavetable
