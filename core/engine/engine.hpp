#pragma once

#include "core/common/constants.hpp"
#include "core/common/time.hpp"
#include "core/engine/commands.hpp"
#include "core/graph/connection_table.hpp"
#include "core/ipc/spsc_queue.hpp"
#include "core/memory/node_pool.hpp"
#include "core/scheduler/scheduler.hpp"

#include <cassert>
#include <cstdint>
#include <expected>

#include <miniaudio.h>

/**
 * @file engine.hpp
 * @brief Top-level audio engine: owns all subsystems and drives the miniaudio callback.
 *
 * @details `AudioEngine` is the single entry point for Python. It owns the node pool,
 *          SPSC command queue, EDF scheduler, Janitor thread, connection table, and the
 *          miniaudio device handle. Python interacts exclusively through `NodeHandleID`
 *          values — opaque uint64_t handles that encode a slot index and generation tag.
 *
 *          **Data flow per block:**
 *          1. `audio_callback()` pops all pending `ScheduledCommandPayload` items from
 *             the SPSC queue and feeds them to the EDF scheduler.
 *          2. `EDFScheduler::process_block()` fires commands whose deadline has passed.
 *          3. The output buffer is zeroed.
 *          4. For each connection, the source generator renders into a stack-allocated
 *             scratch buffer; the sink mixes it into the hardware output buffer.
 *          5. `current_sample_` is incremented by `frame_count`.
 */

/// @brief Top-level AudioEngine and engine configuration types.
namespace mach::engine {

/**
 * @brief Construction parameters for `AudioEngine`.
 *
 * @note Passed by value from Python via nanobind. All fields have defaults so Python
 *       can construct with keyword arguments.
 */
struct EngineInitParams {
    uint32_t    sample_rate;        ///< Device sample rate in Hz.
    std::size_t block_size;         ///< Frames per audio callback.
    uint32_t    max_node_pool_size; ///< Maximum live nodes.
    double      bpm;                ///< Initial tempo in BPM.
};

/**
 * @brief Error codes returned by engine operations via `std::expected`.
 */
enum class EngineError : uint8_t {
    POOL_CAPACITY_EXCEEDED, ///< Node pool has no free slots.
    COMMAND_QUEUE_FULL,     ///< SPSC queue is full; the caller should retry.
    INVALID_PARAMETER,      ///< A parameter value was outside its valid range.
};

/**
 * @brief Headless audio engine managing the full DSP pipeline.
 *
 * @details Not copyable or movable — owns a live `ma_device` and background threads.
 *          Python holds a single `AudioEngine` instance for the lifetime of a session.
 */
class AudioEngine {
public:
    /**
     * @brief Constructs the engine, initialises the miniaudio device, and starts the
     *        Janitor thread.
     *
     * @details Sizes the node pool, SPSC queue, EDF heap, and connection table from
     *          `params.max_node_pool_size`. Activates the `MasterOutput` node
     *          synchronously (no SPSC push needed for the initial sink). Calls
     *          `std::terminate()` if miniaudio device init fails.
     *
     * @param params Engine configuration.
     *
     * @note **Thread Safety:** Python/Main Thread.
     */
    explicit AudioEngine(const EngineInitParams& params);

    /**
     * @brief Uninitialises the miniaudio device.
     *
     * @details Call `stop()` before destruction to drain the command queue cleanly.
     *          The Janitor `std::jthread` is joined automatically by its destructor.
     *
     * @note **Thread Safety:** Python/Main Thread.
     */
    ~AudioEngine() noexcept;

    AudioEngine(const AudioEngine&) = delete;
    auto operator=(const AudioEngine&) -> AudioEngine& = delete;
    AudioEngine(AudioEngine&&) = delete;
    auto operator=(AudioEngine&&) -> AudioEngine& = delete;

    /// @brief Opaque node handle — `bit_cast` of a `(slot_idx, generation)` pair.
    using NodeHandleID = memory::node_pool::NodePool::NodeHandleID;

    /**
     * @brief Acquires a pool slot, constructs a `Node`, and queues its activation.
     *
     * @details Steps:
     *          1. `node_pool_.acquire<Node>()` — constructs the node in `ACQUIRED`.
     *          2. `node.set_sample_rate(sample_rate_)` — before audio thread sees it.
     *          3. Pushes `AddNodePayload` (deadline = 0) to the SPSC queue. On queue
     *             full, rolls back via `abandon_acquired_node()` and hands the handle
     *             to the Janitor.
     *
     * @tparam Node Must satisfy `DSPNode` and be constructible from `Args...`.
     * @param args  Forwarded to `Node`'s constructor.
     * @return `NodeHandleID` on success.
     * @retval `EngineError::POOL_CAPACITY_EXCEEDED` if the node pool is full.
     * @retval `EngineError::COMMAND_QUEUE_FULL` if the SPSC queue could not accept the
     *         activation command (node is rolled back).
     *
     * @note **Thread Safety:** Python/Main Thread.
     */
    template<typename Node, typename... Args>
        requires(nodes::DSPNode<Node> && std::constructible_from<Node, Args...>)
    auto add_node(Args&&... args) noexcept -> std::expected<NodeHandleID, EngineError> {
        auto acquire_result {node_pool_.acquire<Node>(std::forward<Args>(args)...)};
        if (!acquire_result) {
            return std::unexpected<EngineError>(EngineError::POOL_CAPACITY_EXCEEDED);
        }

        auto handle {acquire_result.value()};

        auto acquired_node {node_pool_.get_node(handle)};
        std::visit([this](auto& node) -> void { node.set_sample_rate(sample_rate_); },
                   *acquired_node.value());

        if (!command_queue_.try_push({.command = commands::detail::AddNodePayload {
                                             .node_id = handle},
                                         .deadline_abs_sample = 0})) {
            [[maybe_unused]] auto abandon_result {
                node_pool_.abandon_acquired_node(handle)};
            assert(abandon_result);
            janitor_.enqueue_dead_node(handle);
            return std::unexpected<EngineError>(EngineError::COMMAND_QUEUE_FULL);
        }
        return handle;
    };

    /**
     * @brief Queues removal of an active node.
     *
     * @details Pushes a `RemoveNodePayload` (deadline = 0). The audio thread disconnects
     *          all edges, abandons the slot, and hands the handle to the Janitor.
     *
     * @param handle Opaque handle of the node to remove.
     * @return `{}` on success.
     * @retval `EngineError::COMMAND_QUEUE_FULL` if the SPSC queue is full.
     *
     * @note **Thread Safety:** Python/Main Thread.
     */
    auto remove_node(const NodeHandleID& handle) noexcept
        -> std::expected<void, EngineError>;

    /**
     * @brief Queues an immediate parameter update.
     *
     * @details Pushes `SetNodeParamPayload` (deadline = 0). Takes effect in the next
     *          block. For sample-accurate scheduling use `schedule()`.
     *
     * @param handle   Opaque node handle.
     * @param param_id Parameter ID from `Node::get_params()`.
     * @param value    New value, encoded as float.
     * @return `{}` on success.
     * @retval `EngineError::COMMAND_QUEUE_FULL` if the SPSC queue is full.
     *
     * @note **Thread Safety:** Python/Main Thread.
     */
    auto set_node_parameter(NodeHandleID handle, uint32_t param_id, float value) noexcept
        -> std::expected<void, EngineError>;

    /**
     * @brief Queues a connection between a generator and a sink node.
     *
     * @param source Generator node handle.
     * @param dest   Sink node handle.
     * @return `{}` on success.
     * @retval `EngineError::COMMAND_QUEUE_FULL` if the SPSC queue is full.
     *
     * @note **Thread Safety:** Python/Main Thread.
     */
    auto connect(NodeHandleID source, NodeHandleID dest) noexcept
        -> std::expected<void, EngineError>;

    /**
     * @brief Queues removal of the edge between `source` and `dest`.
     *
     * @param source Generator node handle.
     * @param dest   Sink node handle.
     * @return `{}` on success.
     * @retval `EngineError::COMMAND_QUEUE_FULL` if the SPSC queue is full.
     *
     * @note **Thread Safety:** Python/Main Thread.
     */
    auto disconnect(NodeHandleID source, NodeHandleID dest) noexcept
        -> std::expected<void, EngineError>;

    /**
     * @brief Queues an immediate tempo change.
     *
     * @param bpm New tempo in beats per minute. Must be in (0, 1000].
     * @return `{}` on success.
     * @retval `EngineError::INVALID_PARAMETER` if `bpm <= 0` or `bpm > 1000`.
     * @retval `EngineError::COMMAND_QUEUE_FULL` if the SPSC queue is full.
     *
     * @note **Thread Safety:** Python/Main Thread.
     */
    auto set_bpm(double bpm) noexcept -> std::expected<void, EngineError>;

    /**
     * @brief Queues a command for sample-accurate execution at the given `TimeSpec`.
     *
     * @details Converts `time` to an absolute sample deadline using the current
     *          `current_sample_` and `bpm_` snapshots (`memory_order_relaxed`), then
     *          pushes a `ScheduledCommandPayload` to the SPSC queue.
     *
     * @param cmd  The command payload to schedule.
     * @param time Offset from now as `Samples`, `Seconds`, or `Beats`.
     * @return `{}` on success.
     * @retval `EngineError::COMMAND_QUEUE_FULL` if the SPSC queue is full.
     *
     * @note **Thread Safety:** Python/Main Thread.
     */
    auto schedule(commands::detail::CommandPayload cmd, TimeSpec time) noexcept
        -> std::expected<void, EngineError>;

    /**
     * @brief Returns the handle of the singleton `MasterOutput` node.
     *
     * @details Created and activated during construction; never removed. Pass this as
     *          the `dest` argument to `connect()`.
     *
     * @return Pre-activated `MasterOutput` handle.
     *
     * @note **Thread Safety:** Any thread. Immutable after construction.
     */
    [[nodiscard]] auto get_master_output() const noexcept -> NodeHandleID;

    /**
     * @brief Blocks the calling thread until the given time has elapsed.
     *
     * @details - `Samples`: polls `current_sample_` in a 1 ms sleep loop.
     *          - `Seconds`: delegates to `std::this_thread::sleep_for`.
     *          - `Beats`: polls `steady_clock` in a 1 ms loop, consuming
     *            `remaining_beats` using live `bpm_` reads.
     *
     * @param time How long to block.
     *
     * @note **Thread Safety:** Python/Main Thread. The nanobind binding releases the GIL
     *       before calling this function.
     */
    void sleep(TimeSpec time) noexcept;

    /**
     * @brief Starts the miniaudio audio callback.
     *
     * @details Calls `std::terminate()` on failure.
     *
     * @note **Thread Safety:** Python/Main Thread. GIL released by nanobind binding.
     */
    void play() noexcept;

    /**
     * @brief Stops the audio callback and flushes remaining commands.
     *
     * @details After `ma_device_stop()`:
     *          1. Drains all remaining SPSC items into the EDF scheduler.
     *          2. Calls `process_block()` with `block_size = 0` to dispatch all
     *             immediate commands (deadline < current_sample_).
     *
     * @note **Thread Safety:** Python/Main Thread. GIL released by nanobind binding.
     */
    void stop() noexcept;

private:
    /**
     * @brief miniaudio device callback — the real-time audio thread entry point.
     *
     * @details Matches the `ma_device_data_proc` signature. Steps per call:
     *          1. Drain SPSC → EDF scheduler.
     *          2. `process_block()` to fire due commands.
     *          3. Zero the output buffer.
     *          4. Per connection: render source into a stack-allocated scratch buffer
     *             (`std::array<float, 8192>`), mix into the hardware output.
     *          5. Increment `current_sample_` by `frame_count` (`relaxed`).
     *
     * @param device      miniaudio device; `pUserData` holds the `AudioEngine*`.
     * @param output      Hardware output buffer (f32, interleaved stereo).
     * @param input       Unused (playback-only device).
     * @param frame_count Frames requested by the audio hardware.
     *
     * @note **Thread Safety:** Audio Thread. Real-time Safe.
     */
    static void audio_callback(ma_device* device, void* output, const void* input,
                               ma_uint32 frame_count);

    memory::node_pool::NodePool                               node_pool_;
    ipc::SPSCQueue<commands::detail::ScheduledCommandPayload> command_queue_;
    uint32_t                                                  sample_rate_;
    std::size_t                                               block_size_;

    ma_device device_ {};

    scheduler::EDFScheduler event_scheduler_;
    janitor::JanitorThread  janitor_;
    graph::ConnectionTable  connection_table_;
    NodeHandleID            master_output_id_;

    /// @brief Monotonically increasing sample counter. Written only by the audio thread;
    ///        read by the Python thread in `sleep()` and `schedule()`.
    ///        `relaxed` ordering: slight staleness in deadline calculations is acceptable.
    std::atomic<uint64_t> current_sample_ {0ULL};

    /// @brief Current tempo in BPM. Written via SPSC command (`set_bpm`) on the audio
    ///        thread; read with `relaxed` ordering on both threads.
    std::atomic<double> bpm_ {constants::DEFAULT_BPM}; // NOLINT
};

} // namespace mach::engine
