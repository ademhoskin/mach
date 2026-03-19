#include "core/scheduler/scheduler.hpp"

#include "core/engine/commands.hpp"
#include "core/memory/node_pool.hpp"
#include "core/nodes/wavetable/wavetable_osc.hpp"

#include <cstdint>

#include <doctest/doctest.h>

using namespace mach::scheduler;
using namespace mach::engine::commands;
using namespace mach::memory::node_pool;
using namespace mach::nodes::wavetable;

constexpr auto TEST_HEAP_SIZE {64UZ};
constexpr auto TEST_BLOCK_SIZE {128UZ};
constexpr uint32_t TEST_POOL_SIZE {4U};

struct TestEDFSchedulerFixture {
    NodePool pool {TEST_POOL_SIZE};
    EDFScheduler scheduler {TEST_HEAP_SIZE};
};

TEST_CASE_FIXTURE(TestEDFSchedulerFixture, "EDFScheduler") {
    SUBCASE("schedules and fires command within block") {
        auto handle {pool.acquire<WavetableOscillator>().value()};
        REQUIRE(scheduler.schedule(AddNodePayload {.node_id = handle}, 0ULL));
        scheduler.process_block(0ULL, TEST_BLOCK_SIZE, pool);
        CHECK(pool.get_node(handle).has_value());
    }

    SUBCASE("does not fire command beyond block boundary") {
        auto handle {pool.acquire<WavetableOscillator>().value()};
        REQUIRE(scheduler.schedule(AddNodePayload {.node_id = handle}, 256ULL));
        scheduler.process_block(0ULL, TEST_BLOCK_SIZE, pool);
        CHECK_FALSE(pool.deactivate(handle));
    }

    SUBCASE("fires commands in deadline order") {
        auto h1 {pool.acquire<WavetableOscillator>().value()};
        auto h2 {pool.acquire<WavetableOscillator>().value()};
        REQUIRE(scheduler.schedule(AddNodePayload {.node_id = h2}, 64ULL));
        REQUIRE(scheduler.schedule(AddNodePayload {.node_id = h1}, 0ULL));
        scheduler.process_block(0ULL, TEST_BLOCK_SIZE, pool);
        CHECK(pool.get_node(h1).has_value());
        CHECK(pool.get_node(h2).has_value());
    }

    SUBCASE("returns false when heap is full") {
        for (auto i {0UZ}; i < TEST_HEAP_SIZE; ++i) {
            CHECK(scheduler.schedule(AddNodePayload {.node_id = 0ULL}, i));
        }
        CHECK_FALSE(scheduler.schedule(AddNodePayload {.node_id = 0ULL}, 0ULL));
    }
}
