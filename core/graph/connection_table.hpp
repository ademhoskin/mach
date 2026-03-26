#pragma once

#include "core/memory/node_pool.hpp"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <span>

namespace mach::graph {

using NodeHandleID = memory::node_pool::NodePool::NodeHandleID;

struct Connection {
    NodeHandleID source;
    NodeHandleID dest;
};

class ConnectionTable {
public:
    explicit ConnectionTable(std::size_t capacity)
        : connections_ {std::make_unique<Connection[]>(capacity)}, CAPACITY {capacity} {}

    [[nodiscard]] auto add(NodeHandleID source, NodeHandleID dest) noexcept -> bool {
        if (count_ >= CAPACITY) {
            return false;
        }
        connections_[count_] = {.source = source, .dest = dest};
        ++count_;
        return true;
    }

    auto remove(NodeHandleID source, NodeHandleID dest) noexcept -> bool {
        auto conns {std::span {connections_.get(), count_}};
        for (auto&& [idx, conn] : std::views::enumerate(conns)) {
            if (conn.source == source && conn.dest == dest) {
                conn = connections_[count_ - 1];
                --count_;
                return true;
            }
        }
        return false;
    }

    void remove_all_for(NodeHandleID node_id) noexcept {
        std::size_t i {0};
        while (i < count_) {
            if (connections_[i].source == node_id
                || connections_[i].dest == node_id) {
                connections_[i] = connections_[count_ - 1];
                --count_;
            } else {
                ++i;
            }
        }
    }

    template<typename F>
    void for_each_connection(F&& func) const noexcept {
        auto conns {std::span {connections_.get(), count_}};
        for (const auto& conn : conns) {
            func(conn);
        }
    }

private:
    std::unique_ptr<Connection[]> connections_; // NOLINT
    const std::size_t CAPACITY;                 // NOLINT
    std::size_t count_ {0};
};

} // namespace mach::graph
