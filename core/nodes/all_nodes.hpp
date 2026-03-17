#include "core/nodes/node.hpp"
#include "core/nodes/wavetable/wavetable_osc.hpp"

#include <variant>

namespace mach::nodes {

template<typename... Ts>
    requires(DSPNode<Ts> && ...)
using NodeVariantFactory = std::variant<Ts...>;

// Add new nodes into this to be able to use in node pool
using AnyDSPNode = NodeVariantFactory<wavetable::WavetableOscillator>;

} // namespace mach::nodes
