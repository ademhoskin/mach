#include "core/nodes/wavetable/wavetable_osc.hpp"

#include <array>

auto main() -> int {
    mach::nodes::wavetable::WavetableOscillator osc {};
    std::array<float, 128> buffer {};

    for (auto i {0}; i < 1000000; ++i) {
        osc.render_frame(buffer);
    }
}
