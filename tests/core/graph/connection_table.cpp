#include "core/graph/connection_table.hpp"

#include <vector>

#include <doctest/doctest.h>

using namespace mach::graph;

constexpr auto TEST_CAPACITY {8UZ};

struct TestConnectionTableFixture {
    ConnectionTable table {TEST_CAPACITY};
};

TEST_CASE_FIXTURE(TestConnectionTableFixture, "ConnectionTable") {
    SUBCASE("add returns true on success") {
        CHECK(table.add(1ULL, 2ULL));
    }

    SUBCASE("add returns false when full") {
        for (std::size_t i {0}; i < TEST_CAPACITY; ++i) {
            REQUIRE(table.add(i, i + 100));
        }
        CHECK_FALSE(table.add(99ULL, 100ULL));
    }

    SUBCASE("remove returns true for existing connection") {
        REQUIRE(table.add(1ULL, 2ULL));
        CHECK(table.remove(1ULL, 2ULL));
    }

    SUBCASE("remove returns false for non-existent connection") {
        CHECK_FALSE(table.remove(1ULL, 2ULL));
    }

    SUBCASE("remove frees a slot for new connections") {
        for (std::size_t i {0}; i < TEST_CAPACITY; ++i) {
            REQUIRE(table.add(i, i + 100));
        }
        CHECK_FALSE(table.add(99ULL, 100ULL));

        REQUIRE(table.remove(0ULL, 100ULL));
        CHECK(table.add(99ULL, 100ULL));
    }

    SUBCASE("remove_all_for clears all connections involving a node") {
        REQUIRE(table.add(1ULL, 10ULL));
        REQUIRE(table.add(2ULL, 10ULL));
        REQUIRE(table.add(10ULL, 3ULL));
        REQUIRE(table.add(4ULL, 5ULL));

        table.remove_all_for(10ULL);

        // only the 4->5 connection should remain
        std::vector<Connection> remaining;
        table.for_each_connection(
            [&](const Connection& conn) -> void { remaining.push_back(conn); });

        CHECK(remaining.size() == 1);
        CHECK(remaining[0].source == 4ULL);
        CHECK(remaining[0].dest == 5ULL);
    }

    SUBCASE("for_each_connection iterates all connections") {
        REQUIRE(table.add(1ULL, 2ULL));
        REQUIRE(table.add(3ULL, 4ULL));
        REQUIRE(table.add(5ULL, 6ULL));

        std::size_t count {0};
        table.for_each_connection(
            [&](const Connection& /*conn*/) -> void { ++count; });

        CHECK(count == 3);
    }

    SUBCASE("for_each_connection on empty table is a no-op") {
        std::size_t count {0};
        table.for_each_connection(
            [&](const Connection& /*conn*/) -> void { ++count; });

        CHECK(count == 0);
    }
}
