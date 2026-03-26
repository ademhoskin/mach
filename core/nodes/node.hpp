#pragma once

#include <concepts>
#include <cstdint>
#include <span>
#include <string_view>

namespace mach::nodes {

struct ParamDescriptor {
    uint64_t param_id;
    std::string_view name;
};

struct NodeParamUpdate {
    uint32_t param_id;
    float value;
};

template<typename Node>
concept DSPNode = requires(Node node, ParamDescriptor param, float sample_rate,
                           NodeParamUpdate update) {
    { node.set_sample_rate(sample_rate) } noexcept -> std::same_as<void>;
    { node.set_param(update) } noexcept -> std::same_as<void>;
    {
        Node::get_params()
    } noexcept -> std::convertible_to<std::span<const ParamDescriptor>>;
};

template<typename Node>
concept SinkNode =
    DSPNode<Node>
    && requires(Node node, const std::span<float> INPUT, std::span<float> output) {
           { node.mix_to_output(INPUT, output) } noexcept -> std::same_as<void>;
       };

template<typename Node>
concept GeneratorNode = DSPNode<Node> && requires(Node node, std::span<float> output) {
    { node.render_frame(output) } noexcept -> std::same_as<void>;
    { Node::FREQ_PARAM_ID } -> std::same_as<const uint32_t&>;
    { Node::AMP_PARAM_ID } -> std::same_as<const uint32_t&>;
};

} // namespace mach::nodes
