#include "core/memory/node_pool.hpp"

#include "core/nodes/wavetable/wavetable_osc.hpp"

#include <cstdint>
#include <variant>

#include <doctest/doctest.h>

using namespace mach::memory::node_pool;
using namespace mach::nodes::wavetable;

constexpr uint32_t VALID_CAPACITY {4U};

struct TestNodePoolFixture {
    WavetableOscillator wavetable_osc;
    NodePool pool {VALID_CAPACITY};
};

TEST_CASE_FIXTURE(TestNodePoolFixture, "NodePool") {
    SUBCASE("returns a valid handle on successful acquire") {
        CHECK(pool.acquire<WavetableOscillator>(wavetable_osc).has_value());
    }

    SUBCASE("returns capacity exceeded") {
        for (uint32_t i {0U}; i < VALID_CAPACITY; ++i) {
            CHECK(pool.acquire<WavetableOscillator>(wavetable_osc).has_value());
        }
        auto expected_exceeded_result {pool.acquire<WavetableOscillator>(wavetable_osc)};
        CHECK(expected_exceeded_result.error() == PoolError::CAPACITY_EXCEEDED);
    }

    SUBCASE("successfully sets acquired node to active") {
        auto handle {pool.acquire<WavetableOscillator>(wavetable_osc).value()};
        CHECK(pool.activate(handle));
    }

    SUBCASE("successfully detects stale generation during activation") {
        auto handle {pool.acquire<WavetableOscillator>(wavetable_osc).value()};
        std::ignore = pool.activate(handle);
        std::ignore = pool.deactivate(handle);
        pool.recycle(handle);
        CHECK_FALSE(pool.activate(handle));
    }

    SUBCASE("sucessfully deactivates only on active nodes") {
        auto handle {pool.acquire<WavetableOscillator>(wavetable_osc).value()};
        CHECK_FALSE(pool.deactivate(handle)); // we just acquired

        std::ignore = pool.activate(handle);
        CHECK(pool.deactivate(handle));
        CHECK_FALSE(pool.deactivate(handle)); // we already deactivated
    }

    SUBCASE("bumps generation and clears slot on successfulrecycles") {
        auto handle {pool.acquire<WavetableOscillator>(wavetable_osc).value()};
        std::ignore = pool.activate(handle);
        std::ignore = pool.deactivate(handle);
        pool.recycle(handle);

        CHECK_FALSE(
            pool.activate(handle)); // we should not be able to activate, generation has bumped
        auto new_handle {pool.acquire<WavetableOscillator>(wavetable_osc).value()};
        CHECK_FALSE(new_handle == handle); // same handle means we have not bumped
    }

    SUBCASE("returns valid node on successful get") {
        auto handle {pool.acquire<WavetableOscillator>(wavetable_osc).value()};
        std::ignore = pool.activate(handle);
        CHECK(pool.get_node(handle).has_value());
    }

    SUBCASE("returns invalid handle on failed get") {
        SUBCASE("empty pool") {
            CHECK_FALSE(pool.get_node(0U).has_value());
        }
        SUBCASE("after recycling a node") {
            auto handle {pool.acquire<WavetableOscillator>(wavetable_osc).value()};
            std::ignore = pool.activate(handle);
            std::ignore = pool.deactivate(handle);
            pool.recycle(handle);
            CHECK_FALSE(pool.get_node(handle).has_value());
        }
    }

    SUBCASE("successfully calls set_param on active node") {
        auto handle {pool.acquire<WavetableOscillator>(wavetable_osc).value()};
        std::ignore = pool.activate(handle);
        auto maybe_node {pool.get_node(handle)};
        REQUIRE(maybe_node.has_value());
        constexpr float TEST_FREQUENCY {880.0F};
        std::visit([](auto& node) -> void { node.set_param({.param_id = 0U, .value = TEST_FREQUENCY}); },
                   *maybe_node.value());
        CHECK(pool.deactivate(handle));
    }

    SUBCASE("has successful happy path on acquire, activate, deactivate, recycle") {
        auto handle {pool.acquire<WavetableOscillator>(wavetable_osc).value()};
        CHECK(pool.activate(handle));
        CHECK(pool.get_node(handle).has_value());
        CHECK(pool.deactivate(handle));
        CHECK_FALSE(pool.get_node(handle).has_value());

        pool.recycle(handle);
        CHECK_FALSE(pool.get_node(handle).has_value());
    }
}
