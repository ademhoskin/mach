#include "core/engine/engine.hpp"

#include "core/nodes/wavetable/wavetable_osc.hpp"

#include <cstdint>

#include <doctest/doctest.h>

using namespace mach::engine;
using namespace mach::nodes::wavetable;

constexpr uint32_t TEST_POOL_SIZE {4U};
constexpr float TEST_FREQUENCY {880.0F};

struct TestAudioEngineFixture {
    AudioEngine engine {{.sample_rate = 44100U,
                         .block_size = 128UZ,
                         .max_node_pool_size = TEST_POOL_SIZE}};
};

TEST_CASE_FIXTURE(TestAudioEngineFixture, "AudioEngine") {
    SUBCASE("add_node returns valid handle") {
        CHECK(engine.add_node<WavetableOscillator>().has_value());
    }

    SUBCASE("set_node_parameter succeeds on valid handle") {
        auto handle {engine.add_node<WavetableOscillator>().value()};
        CHECK(engine.set_node_parameter(handle, 0U, TEST_FREQUENCY).has_value());
    }

    SUBCASE("remove_node succeeds on valid handle") {
        auto handle {engine.add_node<WavetableOscillator>().value()};
        CHECK(engine.remove_node(handle).has_value());
    }

    SUBCASE("add_node returns pool capacity exceeded") {
        for (uint32_t i {0U}; i < TEST_POOL_SIZE; ++i) {
            std::ignore = engine.add_node<WavetableOscillator>();
        }
        CHECK(engine.add_node<WavetableOscillator>().error()
              == EngineError::POOL_CAPACITY_EXCEEDED);
    }

    SUBCASE("connect succeeds on valid handles") {
        auto osc {engine.add_node<WavetableOscillator>().value()};
        auto out {engine.get_master_output()};
        CHECK(engine.connect(osc, out).has_value());
    }

    SUBCASE("disconnect succeeds on valid handles") {
        auto osc {engine.add_node<WavetableOscillator>().value()};
        auto out {engine.get_master_output()};
        std::ignore = engine.connect(osc, out);
        CHECK(engine.disconnect(osc, out).has_value());
    }

    SUBCASE("disconnect returns command queue full") {
        auto osc {engine.add_node<WavetableOscillator>().value()};
        auto out {engine.get_master_output()};
        for (auto i {0UZ}; i < 1024UZ; ++i) {
            std::ignore = engine.set_node_parameter(osc, 0U, TEST_FREQUENCY);
        }
        CHECK(engine.disconnect(osc, out).error() == EngineError::COMMAND_QUEUE_FULL);
    }

    SUBCASE("add_node returns command queue full") {
        auto handle {engine.add_node<WavetableOscillator>().value()};
        for (auto i {0UZ}; i < 1024UZ; ++i) {
            std::ignore = engine.set_node_parameter(handle, 0U, TEST_FREQUENCY);
        }
        CHECK(engine.add_node<WavetableOscillator>().error()
              == EngineError::COMMAND_QUEUE_FULL);
    }
}
