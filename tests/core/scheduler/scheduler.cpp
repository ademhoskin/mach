#include "core/scheduler/scheduler.hpp"

#include "core/engine/commands.hpp"
#include "core/memory/node_pool.hpp"
#include "core/nodes/wavetable/wavetable_osc.hpp"

#include <bit>
#include <cstdint>

#include <doctest/doctest.h>

using namespace mach::scheduler;
using namespace mach::engine::commands::detail;
using namespace mach::memory::node_pool;
using namespace mach::nodes::wavetable;

constexpr auto TEST_HEAP_SIZE {64UZ};
constexpr auto TEST_BLOCK_SIZE {128UZ};
constexpr uint32_t TEST_POOL_SIZE {4U};

struct TestEDFSchedulerFixture {
    NodePool pool {TEST_POOL_SIZE};
    mach::janitor::JanitorThread janitor {
        pool, std::bit_ceil(static_cast<std::size_t>(TEST_POOL_SIZE))};
    mach::graph::ConnectionTable connections {TEST_HEAP_SIZE};
    EDFScheduler scheduler {TEST_HEAP_SIZE};
    std::atomic<double> bpm {120.0};
};

TEST_CASE_FIXTURE(TestEDFSchedulerFixture, "EDFScheduler") {
    SUBCASE("schedules and fires command within block") {
        auto handle {pool.acquire<WavetableOscillator>().value()};
        REQUIRE(scheduler.schedule(AddNodePayload {.node_id = handle}, 0ULL));
        scheduler.process_block(0ULL, TEST_BLOCK_SIZE, pool, janitor, connections, bpm);
        CHECK(pool.abandon_active_nodes(handle));
    }

    SUBCASE("does not fire command beyond block boundary") {
        auto handle {pool.acquire<WavetableOscillator>().value()};
        REQUIRE(scheduler.schedule(AddNodePayload {.node_id = handle}, 256ULL));
        scheduler.process_block(0ULL, TEST_BLOCK_SIZE, pool, janitor, connections, bpm);
        CHECK_FALSE(pool.abandon_active_nodes(handle));
    }

    SUBCASE("SetBpmPayload updates bpm on dispatch") {
        REQUIRE(scheduler.schedule(SetBpmPayload {.bpm = 140.0}, 0ULL));
        scheduler.process_block(0ULL, TEST_BLOCK_SIZE, pool, janitor, connections, bpm);
        CHECK(bpm.load() == doctest::Approx(140.0));
    }

    SUBCASE("SetBpmPayload respects deadline") {
        REQUIRE(scheduler.schedule(SetBpmPayload {.bpm = 160.0}, 256ULL));
        scheduler.process_block(0ULL, TEST_BLOCK_SIZE, pool, janitor, connections, bpm);
        CHECK(bpm.load() == doctest::Approx(120.0));
    }

    SUBCASE("returns false when heap is full") {
        for (auto i {0UZ}; i < TEST_HEAP_SIZE; ++i) {
            CHECK(scheduler.schedule(AddNodePayload {.node_id = 0ULL}, i));
        }
        CHECK_FALSE(scheduler.schedule(AddNodePayload {.node_id = 0ULL}, 0ULL));
    }
}
