#pragma once

#include "core/common/constants.hpp"
#include "core/nodes/node.hpp"

#include <array>
#include <cstdint>
#include <span>

namespace mach::nodes::master_output {

class MasterOutput {
public:
    explicit MasterOutput(
        uint32_t sample_rate = mach::constants::DEFAULT_SAMPLE_RATE) noexcept
        : sample_rate_ {sample_rate} {}

    void set_sample_rate(uint32_t sample_rate) noexcept { sample_rate_ = sample_rate; }

    // TODO: CURRENTLY NO OP
    void set_param(NodeParamUpdate /*update*/) noexcept {}

    [[nodiscard]] static constexpr auto get_params() noexcept
        -> std::array<ParamDescriptor, 0> {
        return {};
    }

    static void mix_to_output(const std::span<const float> INPUT,
                              std::span<float> output) noexcept {
        auto count {std::min(INPUT.size(), output.size())};
        for (std::size_t i {0}; i < count; ++i) {
            output[i] += INPUT[i];
        }
    }

private:
    uint32_t sample_rate_;
};

static_assert(SinkNode<MasterOutput>);

} // namespace mach::nodes::master_output
