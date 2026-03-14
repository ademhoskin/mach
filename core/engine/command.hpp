#pragma once

#include <cstdint>
#include <variant>

namespace mach {
using NodeId = uint64_t;

struct AddNodePayload {
    uint32_t node_id;
    uint32_t node_type;
};

struct RemoveNodePayload {
    uint32_t node_id;
};

struct SetNodeParamPayload {
    uint32_t node_id;
    uint32_t param_id;
    float value;
};

using CommandPayload = std::variant<AddNodePayload, RemoveNodePayload, SetNodeParamPayload>;

} // namespace mach
