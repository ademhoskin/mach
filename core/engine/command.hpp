#pragma once

#include <cstdint>
#include <variant>

namespace mach::engine {
using NodeId = uint64_t;

struct AddNodePayload {
    NodeId node_id;
    uint32_t node_type;
};

struct RemoveNodePayload {
    NodeId node_id;
};

struct SetNodeParamPayload {
    NodeId node_id;
    uint32_t param_id;
    float value;
};

using CommandPayload = std::variant<AddNodePayload, RemoveNodePayload, SetNodeParamPayload>;

} // namespace mach::engine
