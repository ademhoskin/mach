#pragma once

#include <concepts>
#include <span>

namespace mach::nodes {

template<typename Node>
concept DSPNode = requires(Node node, float sample_rate) {
    { node.set_sample_rate(sample_rate) } noexcept -> std::same_as<void>;
};

template<typename Node>
concept GeneratorNode =
    DSPNode<Node> && requires(Node node, std::span<float> output, float sample_rate) {
        { node.render_frame(output) } noexcept -> std::same_as<void>;
    };

template<typename Node>
concept TunableGeneratorNode = GeneratorNode<Node> && requires(Node node, float frequency) {
    { node.set_frequency(frequency) } noexcept -> std::same_as<void>;
};

} // namespace mach::nodes
