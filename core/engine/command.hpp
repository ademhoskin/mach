#pragma once

#include "core/nodes/node.hpp"

#include <cstdint>
#include <variant>

namespace mach::engine {

using NodeId = uint64_t;
struct AddNodePayload {
    NodeId node_id;
};

struct RemoveNodePayload {
    NodeId node_id;
};

struct SetNodeParamPayload {
    NodeId node_id;
    nodes::NodeParamUpdate update;
};

using CommandPayload = std::variant<AddNodePayload, RemoveNodePayload, SetNodeParamPayload>;

} // namespace mach::engine
