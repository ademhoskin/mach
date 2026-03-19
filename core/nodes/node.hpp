#pragma once

#include <concepts>
#include <cstdint>
#include <span>

namespace mach::nodes {

struct NodeParamUpdate {
    uint32_t param_id;
    float value;
};

template<typename Node>
concept DSPNode = requires(Node node, float sample_rate, NodeParamUpdate update) {
    { node.set_sample_rate(sample_rate) } noexcept -> std::same_as<void>;
    { node.set_param(update) } noexcept -> std::same_as<void>;
};

template<typename Node>
concept GeneratorNode = DSPNode<Node> && requires(Node node, std::span<float> output) {
    { node.render_frame(output) } noexcept -> std::same_as<void>;
    Node::FREQ_PARAM_ID;
    Node::AMP_PARAM_ID;
};

} // namespace mach::nodes
