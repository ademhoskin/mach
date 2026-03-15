#include "core/ipc/spsc_queue.hpp"

#include <doctest/doctest.h>

constexpr auto VALID_POWER_OF_TWO_SIZE {4UZ};
constexpr auto USABLE_CAPACITY {VALID_POWER_OF_TWO_SIZE - 1};

struct TestQueueFixture {
    mach::ipc::SPSCQueue<int, VALID_POWER_OF_TWO_SIZE> queue {};
};

TEST_CASE_FIXTURE(TestQueueFixture, "SPSCQueue") {
    SUBCASE("Pushes and pops same value (happy path)") {
        int val {};
        REQUIRE(queue.try_push(42) == true);
        REQUIRE(queue.try_pop(val) == true);
        CHECK(val == 42);
    }

    SUBCASE("returns false when trying to push on full queue") {
        for (auto i {0UZ}; i < USABLE_CAPACITY; ++i) {
            REQUIRE(queue.try_push(static_cast<int>(i)) == true);
        }
        CHECK(queue.try_push(42) == false);
    }

    SUBCASE("Pops from empty queue returns false") {
        int val {};
        CHECK(queue.try_pop(val) == false);
    }

    SUBCASE("has expected FIFO behavior") {
        int val {};
        REQUIRE(queue.try_push(42) == true);
        REQUIRE(queue.try_push(43) == true);
        REQUIRE(queue.try_pop(val) == true);
        CHECK(val == 42);
        REQUIRE(queue.try_pop(val) == true);
        CHECK(val == 43);
    }

    SUBCASE("indices wrap around correctly") {
        int val {};
        for (auto i {0UZ}; i < USABLE_CAPACITY; ++i) {
            REQUIRE(queue.try_push(static_cast<int>(i)) == true);
        }
        for (auto i {0UZ}; i < USABLE_CAPACITY; ++i) {
            REQUIRE(queue.try_pop(val) == true);
            CHECK(val == static_cast<int>(i));
        }
        for (auto i {0UZ}; i < USABLE_CAPACITY; ++i) {
            REQUIRE(queue.try_push(static_cast<int>(i)) == true);
        }
        for (auto i {0UZ}; i < USABLE_CAPACITY; ++i) {
            REQUIRE(queue.try_pop(val) == true);
            CHECK(val == static_cast<int>(i));
        }
    }
}
