#pragma once

#include <concepts>
#include <cstdint>
#include <variant>

namespace mach {

struct Samples {
    uint64_t count;
};

struct Seconds {
    double value;
};

struct Beats {
    double value;
};

using TimeSpec = std::variant<Samples, Seconds, Beats>;

namespace note {
constexpr Beats WHOLE {4.0};
constexpr Beats DOTTED_WHOLE {6.0};
constexpr Beats HALF {2.0};
constexpr Beats DOTTED_HALF {3.0};
constexpr Beats QUARTER {1.0};
constexpr Beats DOTTED_QUARTER {1.5};
constexpr Beats EIGHTH {0.5};
constexpr Beats DOTTED_EIGHTH {0.75};
constexpr Beats SIXTEENTH {0.25};
constexpr Beats DOTTED_SIXTEENTH {0.375};
constexpr Beats THIRTY_SECOND {0.125};
constexpr Beats DOTTED_THIRTY_SECOND {0.1875};
constexpr Beats SIXTY_FOURTH {0.0625};
constexpr Beats DOTTED_SIXTY_FOURTH {0.09375};
} // namespace note

[[nodiscard]] inline auto to_abs_sample(TimeSpec time, uint64_t current_abs_sample,
                                        uint32_t sample_rate, double bpm) noexcept
    -> uint64_t {
    return std::visit(
        [&]<typename T>(T spec) -> uint64_t {
            if constexpr (std::same_as<T, Samples>) {
                return current_abs_sample + spec.count;
            } else if constexpr (std::same_as<T, Seconds>) {
                return current_abs_sample
                       + static_cast<uint64_t>(spec.value * sample_rate);
            } else {
                return current_abs_sample
                       + static_cast<uint64_t>(spec.value * sample_rate * 60.0 / bpm);
            }
        },
        time);
}

[[nodiscard]] inline auto to_seconds(TimeSpec time, uint32_t sample_rate,
                                     double bpm) noexcept -> double {
    return std::visit(
        [&]<typename T>(T spec) -> double {
            if constexpr (std::same_as<T, Samples>) {
                return static_cast<double>(spec.count) / sample_rate;
            } else if constexpr (std::same_as<T, Seconds>) {
                return spec.value;
            } else {
                return spec.value * 60.0 / bpm;
            }
        },
        time);
}

} // namespace mach
