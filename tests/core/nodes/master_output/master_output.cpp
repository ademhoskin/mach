#include "core/nodes/master_output/master_output.hpp"

#include <algorithm>
#include <array>

#include <doctest/doctest.h>

using namespace mach::nodes::master_output;

constexpr auto TEST_BLOCK_SIZE {8UZ};

struct TestMasterOutputFixture {
    MasterOutput master;
};

TEST_CASE_FIXTURE(TestMasterOutputFixture, "MasterOutput") {
    SUBCASE("satisfies SinkNode concept") {
        static_assert(mach::nodes::SinkNode<MasterOutput>);
    }

    SUBCASE("mix_to_output sums input into output") {
        std::array<float, TEST_BLOCK_SIZE> input {};
        std::array<float, TEST_BLOCK_SIZE> output {};
        std::ranges::fill(input, 0.5F);
        std::ranges::fill(output, 0.25F);

        MasterOutput::mix_to_output(input, output);

        CHECK(std::ranges::all_of(output,
                                  [](float s) -> bool { return s == 0.75F; }));
    }

    SUBCASE("mix_to_output does not overwrite — it accumulates") {
        std::array<float, TEST_BLOCK_SIZE> input {};
        std::array<float, TEST_BLOCK_SIZE> output {};
        std::ranges::fill(input, 0.1F);
        std::ranges::fill(output, 0.0F);

        MasterOutput::mix_to_output(input, output);
        MasterOutput::mix_to_output(input, output);

        for (const auto& sample : output) {
            CHECK(sample == doctest::Approx(0.2F));
        }
    }

    SUBCASE("mix_to_output with empty spans is a no-op") {
        std::span<const float> empty_in {};
        std::span<float> empty_out {};
        MasterOutput::mix_to_output(empty_in, empty_out);
    }

    SUBCASE("mix_to_output uses minimum of input and output sizes") {
        std::array<float, 4> short_input {};
        std::array<float, TEST_BLOCK_SIZE> output {};
        std::ranges::fill(short_input, 1.0F);
        std::ranges::fill(output, 0.0F);

        MasterOutput::mix_to_output(short_input, output);

        for (std::size_t i {0}; i < 4; ++i) {
            CHECK(output[i] == 1.0F);
        }
        for (std::size_t i {4}; i < TEST_BLOCK_SIZE; ++i) {
            CHECK(output[i] == 0.0F);
        }
    }

    SUBCASE("set_sample_rate does not throw") {
        master.set_sample_rate(96000);
    }

    SUBCASE("get_params returns empty array") {
        CHECK(MasterOutput::get_params().empty());
    }
}
