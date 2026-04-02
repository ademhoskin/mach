#include "bind_common.hpp"

#include "core/nodes/wavetable/wavetable_osc.hpp"

/**
 * @file bind_enums.cpp
 * @brief Registers C++ enumerations with the Python module.
 *
 * @details Currently exposes `Waveform` so Python callers can write
 *          `mach.Waveform.SINE` rather than passing raw integers.
 */

using namespace mach::nodes::wavetable;

/**
 * @brief Registers all engine enumerations into the nanobind module.
 *
 * @param m The `_mach_ext` module object.
 *
 * @note **Thread Safety:** Python/Main Thread — called once during module
 *       initialisation.
 */
void register_enums(nb::module_& m) {
    nb::enum_<Waveform>(m, "Waveform")
        .value("SINE",     Waveform::SINE)
        .value("SAWTOOTH", Waveform::SAWTOOTH)
        .value("TRIANGLE", Waveform::TRIANGLE)
        .value("SQUARE",   Waveform::SQUARE);
}
