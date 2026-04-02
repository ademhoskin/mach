#pragma once

#include <concepts>
#include <cstdint>
#include <variant>

/**
 * @file time.hpp
 * @brief Musical time representations and conversion utilities.
 *
 * @details Provides three concrete time units — sample-accurate (`Samples`), wall-clock
 *          (`Seconds`), and tempo-relative (`Beats`) — unified under the `TimeSpec`
 *          discriminated union. Free functions convert any `TimeSpec` to an absolute
 *          sample position or a wall-clock duration.
 */

namespace mach {

/**
 * @brief A time offset expressed as an integer sample count.
 * @note **Thread Safety:** Plain data — no thread concerns.
 */
struct Samples {
    uint64_t count; ///< Number of samples relative to the current position.
};

/**
 * @brief A time offset expressed as wall-clock seconds.
 * @note **Thread Safety:** Plain data — no thread concerns.
 */
struct Seconds {
    double value; ///< Duration in seconds (must be non-negative for scheduling).
};

/**
 * @brief A time offset expressed in musical beats (quarter notes).
 *
 * @details Conversion to samples requires the engine's current BPM. One beat equals one
 *          quarter note; see `mach::note` for named durations.
 *
 * @note **Thread Safety:** Plain data — no thread concerns.
 */
struct Beats {
    double value; ///< Duration in beats (1.0 = one quarter note).
};

/**
 * @brief Discriminated union over all supported time representations.
 *
 * @details Passed to `AudioEngine::schedule()` and `AudioEngine::sleep()` from Python.
 *          The audio thread converts it once via `to_abs_sample()` and never stores the
 *          variant itself.
 */
using TimeSpec = std::variant<Samples, Seconds, Beats>;

/**
 * @brief Named musical duration constants expressed as `Beats`.
 *
 * @details All values assume 4/4 time with the beat unit equal to a quarter note (1.0).
 *          Use these with `AudioEngine::schedule()` and `AudioEngine::sleep()`.
 *
 * @note **Thread Safety:** Compile-time constants — no thread concerns.
 */
namespace note {
constexpr Beats WHOLE {4.0};                   ///< Whole note (4 beats).
constexpr Beats DOTTED_WHOLE {6.0};            ///< Dotted whole note (6 beats).
constexpr Beats HALF {2.0};                    ///< Half note (2 beats).
constexpr Beats DOTTED_HALF {3.0};             ///< Dotted half note (3 beats).
constexpr Beats QUARTER {1.0};                 ///< Quarter note (1 beat).
constexpr Beats DOTTED_QUARTER {1.5};          ///< Dotted quarter note (1.5 beats).
constexpr Beats EIGHTH {0.5};                  ///< Eighth note (0.5 beats).
constexpr Beats DOTTED_EIGHTH {0.75};          ///< Dotted eighth note (0.75 beats).
constexpr Beats SIXTEENTH {0.25};              ///< Sixteenth note (0.25 beats).
constexpr Beats DOTTED_SIXTEENTH {0.375};      ///< Dotted sixteenth note (0.375 beats).
constexpr Beats THIRTY_SECOND {0.125};         ///< Thirty-second note (0.125 beats).
constexpr Beats DOTTED_THIRTY_SECOND {0.1875}; ///< Dotted thirty-second note (0.1875 beats).
constexpr Beats SIXTY_FOURTH {0.0625};         ///< Sixty-fourth note (0.0625 beats).
constexpr Beats DOTTED_SIXTY_FOURTH {0.09375}; ///< Dotted sixty-fourth note (0.09375 beats).
} // namespace note

/**
 * @brief Converts a `TimeSpec` to an absolute sample position.
 *
 * @details Dispatches on the active variant alternative:
 *          - `Samples`: adds `spec.count` directly to `current_abs_sample`.
 *          - `Seconds`: converts via `sample_rate`.
 *          - `Beats`: converts using `bpm` — `beats * sample_rate * 60 / bpm`.
 *
 * @param time               The time offset to convert.
 * @param current_abs_sample The engine's current absolute sample counter (snapshotted
 *                           on the Python thread via `memory_order_relaxed`).
 * @param sample_rate        Device sample rate in Hz.
 * @param bpm                Current engine tempo in beats per minute.
 * @return Absolute sample index at which the event should fire.
 *
 * @note **Thread Safety:** Python/Main Thread. Called inside `AudioEngine::schedule()`
 *       before the command is pushed to the SPSC queue.
 */
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

/**
 * @brief Converts a `TimeSpec` to a wall-clock duration in seconds.
 *
 * @details Used by `AudioEngine::sleep()` for the `Seconds` fast-path and for
 *          polling-based beat waits.
 *          - `Samples`: divides by `sample_rate`.
 *          - `Seconds`: identity.
 *          - `Beats`: converts via `bpm` — `beats * 60 / bpm`.
 *
 * @param time        The time offset to convert.
 * @param sample_rate Device sample rate in Hz.
 * @param bpm         Current engine tempo in beats per minute.
 * @return Equivalent duration in seconds.
 *
 * @note **Thread Safety:** Python/Main Thread.
 */
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
