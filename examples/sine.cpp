#include "core/engine/engine.hpp"

#include <chrono>
#include <thread>

auto main() -> int {
    mach::engine::AudioEngine engine {{48000U, 128U}};
    engine.play();
    std::this_thread::sleep_for(std::chrono::seconds(3));
    engine.stop();
}
