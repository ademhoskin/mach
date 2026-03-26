#pragma once

#include "core/nodes/node.hpp"

#include <cstdint>
#include <variant>

namespace mach::engine::commands::detail {

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

struct ConnectNodesPayload {
    NodeId source_id;
    NodeId dest_id;
};

using CommandPayload =
    std::variant<AddNodePayload, RemoveNodePayload, SetNodeParamPayload,
                 ConnectNodesPayload>;

struct ScheduledCommandPayload {
    CommandPayload command;
    uint64_t deadline_abs_sample;
};

} // namespace mach::engine::commands::detail
