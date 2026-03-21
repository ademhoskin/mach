#include "core/janitor/janitor.hpp"

#include "core/memory/node_pool.hpp"
#include "core/nodes/wavetable/wavetable_osc.hpp"

#include <array>
#include <bit>
#include <cstdint>
#include <thread>

#include <doctest/doctest.h>

using namespace mach::janitor;
using namespace mach::memory::node_pool;
using namespace mach::nodes::wavetable;

constexpr uint32_t TEST_POOL_SIZE {4U};
constexpr auto TEST_QUEUE_CAPACITY {
    std::bit_ceil(static_cast<std::size_t>(TEST_POOL_SIZE))};

struct TestJanitorFixture {
    NodePool pool {TEST_POOL_SIZE};
    JanitorThread janitor {pool, TEST_QUEUE_CAPACITY};
};

TEST_CASE_FIXTURE(TestJanitorFixture, "Janitor") {
    using namespace std::chrono_literals;

    SUBCASE("recycles inactive node back to free") {
        auto handle {pool.acquire<WavetableOscillator>().value()};
        REQUIRE(pool.activate(handle));
        REQUIRE(pool.deactivate(handle));
        REQUIRE(janitor.enqueue_dead_node(handle));

        std::this_thread::sleep_for(10ms);

        for (uint32_t i {0U}; i < TEST_POOL_SIZE; ++i) {
            CHECK(pool.acquire<WavetableOscillator>().has_value());
        }
    }

    SUBCASE("recycles multiple nodes") {
        std::array<uint64_t, TEST_POOL_SIZE> handles {};
        for (uint32_t i {0U}; i < TEST_POOL_SIZE; ++i) {
            handles.at(i) = pool.acquire<WavetableOscillator>().value();
            REQUIRE(pool.activate(handles.at(i)));
        }
        CHECK_FALSE(pool.acquire<WavetableOscillator>().has_value());

        for (uint32_t i {0U}; i < TEST_POOL_SIZE; ++i) {
            REQUIRE(pool.deactivate(handles.at(i)));
            REQUIRE(janitor.enqueue_dead_node(handles.at(i)));
        }

        std::this_thread::sleep_for(10ms);

        for (uint32_t i {0U}; i < TEST_POOL_SIZE; ++i) {
            CHECK(pool.acquire<WavetableOscillator>().has_value());
        }
    }

    SUBCASE("enqueue returns true on valid push") {
        auto handle {pool.acquire<WavetableOscillator>().value()};
        REQUIRE(pool.activate(handle));
        REQUIRE(pool.deactivate(handle));
        CHECK(janitor.enqueue_dead_node(handle));
    }
}
