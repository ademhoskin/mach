#pragma once

#include "core/ipc/spsc_queue.hpp"
#include "core/memory/node_pool.hpp"

#include <functional>
#include <semaphore>
#include <stop_token>
#include <thread>

namespace mach::janitor {
class JanitorThread {
public:
    using NodeHandleID = memory::node_pool::NodePool::NodeHandleID;

    explicit JanitorThread(memory::node_pool::NodePool& pool,
                           std::size_t dead_queue_capacity) noexcept
        : pool_ {pool}, dead_node_queue_ {dead_queue_capacity},
          thread_ {run_thread, std::ref(*this)} {};

    // NOTE: thread is joined in destructor
    ~JanitorThread() noexcept {
        wake_signal_.release();
        NodeHandleID handle {};
        while (dead_node_queue_.try_pop(handle)) {
            pool_.recycle(handle);
        }
    };

    JanitorThread(const JanitorThread&) = delete;
    auto operator=(const JanitorThread&) -> JanitorThread& = delete;
    JanitorThread(JanitorThread&&) noexcept = delete;
    auto operator=(JanitorThread&&) noexcept -> JanitorThread& = delete;

    [[nodiscard]] auto enqueue_dead_node(NodeHandleID handle) noexcept -> bool {
        if (!dead_node_queue_.try_push(handle)) {
            return false;
        }
        wake_signal_.release();
        return true;
    }

private:
    static void run_thread(const std::stop_token& stop_token,
                           JanitorThread& self) noexcept {
        while (!stop_token.stop_requested()) {
            self.wake_signal_.acquire();
            NodeHandleID handle {};
            if (self.dead_node_queue_.try_pop(handle)) {
                self.pool_.recycle(handle);
            }
        }
    }

    memory::node_pool::NodePool& pool_;
    ipc::SPSCQueue<NodeHandleID> dead_node_queue_;
    std::counting_semaphore<> wake_signal_ {0};
    std::jthread thread_;
};

} // namespace mach::janitor
