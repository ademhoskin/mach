#pragma once
#include <array>
#include <cmath>
#include <numbers>

namespace mach::nodes::wavetable {

// TODO: split shapes
constexpr std::size_t DEFAULT_TABLE_SIZE {4096UZ};
constexpr auto TWO_TIMES_PI_FLOAT {2.0F * std::numbers::pi_v<float>};

struct SineShape {
    // Using first 6 terms of Maclaurin series to compute at compile time
    // this is due to std::sin not being constexpr
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

        return SAMPLE_INPUT - (INPUT_CUBED / THREE_FACTORIAL) + (INPUT_QUINTIC / FIVE_FACTORIAL)
               - (INPUT_SEPTIC / SEVEN_FACTORIAL) + (INPUT_NONIC / NINE_FACTORIAL)
               - (INPUT_UNDECIC / ELEVEN_FACTORIAL);
    }
};

struct SawtoothShape {
    static consteval auto compute(const float PHASE_INPUT) noexcept -> float {
        return PHASE_INPUT * std::numbers::inv_pi_v<float>;
    };
};

struct TriangleShape {
    static consteval auto compute(const float PHASE_INPUT) noexcept -> float {
        // NOLINTNEXTLINE
        return -(1.0F - (2.0F * std::abs(PHASE_INPUT) * std::numbers::inv_pi_v<float>));
    }
};

struct SquareShape {
    static consteval auto compute(const float PHASE_INPUT) noexcept -> float {
        return (PHASE_INPUT < 0.0F) ? -1.0F : 1.0F;
    }
};

// center to [-pi, pi]
constexpr auto compute_centered_phase_angle(const std::size_t IDX) noexcept -> float {
    const auto CALCULATED_ANGLE {TWO_TIMES_PI_FLOAT * static_cast<float>(IDX)
                                 / static_cast<float>(DEFAULT_TABLE_SIZE)};

    return (CALCULATED_ANGLE > std::numbers::pi_v<float>) ? (CALCULATED_ANGLE - TWO_TIMES_PI_FLOAT)
                                                          : CALCULATED_ANGLE;
};

template<typename Shape, std::size_t TableSize = DEFAULT_TABLE_SIZE>
class Wavetable {
  public:
    // Uses linear interpolation
    // TODO: Read how to use 4 point cubic interpolation
    [[nodiscard]] static auto get_interpolated_sample(const float PHASE) noexcept -> float {
        const auto FLOAT_IDX {static_cast<float>(PHASE) * TABLE_SIZE};
        const auto KNOWN_POINT_ONE {static_cast<std::size_t>(FLOAT_IDX) & IDX_MASK};
        const auto KNOWN_POINT_TWO {(KNOWN_POINT_ONE + 1UZ) & IDX_MASK};
        const auto FLOAT_KNOWN_POINT_ONE_DIFF {FLOAT_IDX - static_cast<float>(KNOWN_POINT_ONE)};

        const float SAMPLE_ONE {WAVETABLE[KNOWN_POINT_ONE]};
        const float SAMPLE_TWO {WAVETABLE[KNOWN_POINT_TWO]};
        const float SLOPE {SAMPLE_TWO - SAMPLE_ONE};

        return SAMPLE_ONE + (FLOAT_KNOWN_POINT_ONE_DIFF * SLOPE);
    };

  private:
    static constexpr auto generate() noexcept -> std::array<float, TableSize> {
        std::array<float, TableSize> wavetable {};
        auto idx {0UZ};
        for (auto& value : wavetable) {
            value = Shape::compute(compute_centered_phase_angle(idx++));
        }
        return wavetable;
    }
    static constexpr auto WAVETABLE {generate()};
    static constexpr auto TABLE_SIZE {TableSize};
    static constexpr std::size_t IDX_MASK {TABLE_SIZE - 1UZ};
};

using Sine = Wavetable<SineShape>;
using Sawtooth = Wavetable<SawtoothShape>;
using Triangle = Wavetable<TriangleShape>;
using Square = Wavetable<SquareShape>;
} // namespace mach::nodes::wavetable
