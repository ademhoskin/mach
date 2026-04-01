#pragma once

#include "core/nodes/node.hpp"

#include <cstdint>
#include <type_traits>
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

struct DisconnectNodesPayload {
    NodeId source_id;
    NodeId dest_id;
};

struct SetBpmPayload {
    double bpm;
};

using CommandPayload =
    std::variant<AddNodePayload, RemoveNodePayload, SetNodeParamPayload,
                 ConnectNodesPayload, DisconnectNodesPayload, SetBpmPayload>;

struct ScheduledCommandPayload {
    CommandPayload command;
    uint64_t deadline_abs_sample;
};

static_assert(std::is_trivially_copyable_v<ScheduledCommandPayload>);

} // namespace mach::engine::commands::detail
