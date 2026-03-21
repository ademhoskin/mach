#include "bind_common.hpp"

#include "core/nodes/wavetable/wavetable_osc.hpp"

using namespace mach::nodes::wavetable;

void register_enums(nb::module_& m) {
    nb::enum_<Waveform>(m, "Waveform")
        .value("SINE",     Waveform::SINE)
        .value("SAWTOOTH", Waveform::SAWTOOTH)
        .value("TRIANGLE", Waveform::TRIANGLE)
        .value("SQUARE",   Waveform::SQUARE);
}
