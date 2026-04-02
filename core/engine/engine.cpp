#include "core/engine/engine.hpp"

#include "core/engine/commands.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <chrono>
#include <iostream>
#include <print>
#include <span>
#include <string>
#include <thread>


namespace mach::engine {

/**
 * @brief Constructs the engine, pre-allocates all subsystem memory, and opens the
 *        miniaudio playback device.
 *
 * @details Capacity sizing:
 *          - SPSC queue: `bit_ceil(max_node_pool_size * 4)` — headroom for rapid
 *            bursts of add/remove/connect commands.
 *          - EDF heap: same size as the SPSC queue.
 *          - Janitor dead queue: `bit_ceil(max_node_pool_size)`.
 *          - Connection table: `bit_ceil(max_node_pool_size * 16)`.
 *
 *          The `MasterOutput` node is acquired and activated here, bypassing the SPSC
 *          queue, because it must be live before `play()` is called.
 *
 * @note **Thread Safety:** Python/Main Thread.
 */
AudioEngine::AudioEngine(const EngineInitParams& params)
    : node_pool_ {params.max_node_pool_size},
      command_queue_ {
          std::bit_ceil(static_cast<std::size_t>(params.max_node_pool_size) * 4UZ)},
      sample_rate_ {params.sample_rate}, block_size_ {params.block_size},
      event_scheduler_ {
          std::bit_ceil(static_cast<std::size_t>(params.max_node_pool_size) * 4UZ)},
      janitor_ {node_pool_,
                std::bit_ceil(static_cast<std::size_t>(params.max_node_pool_size))},
      connection_table_ {
          std::bit_ceil(static_cast<std::size_t>(params.max_node_pool_size) * 16UZ)},
      bpm_ {params.bpm} {

    auto master_output {
        node_pool_.acquire<nodes::master_output::MasterOutput>(params.sample_rate)};
    assert(master);

    master_output_id_ = master_output.value();
    [[maybe_unused]] auto activated {node_pool_.activate(master_output_id_)};
    assert(activated);

    ma_device_config config {ma_device_config_init(ma_device_type_playback)};
    config.playback.format = ma_format_f32;
    config.playback.channels = 2;
    config.sampleRate = sample_rate_;
    config.dataCallback = audio_callback;
    config.pUserData = this;

    ma_result result = ma_device_init(nullptr, &config, &device_);
    if (result != MA_SUCCESS) {
        std::println(std::cerr, "Failed to initialize audio device!: {}",
                     std::to_string(result));
        std::terminate();
    }
}

/**
 * @note **Thread Safety:** Python/Main Thread. Call `stop()` first.
 */
AudioEngine::~AudioEngine() noexcept {
    ma_device_uninit(&device_);
}

/**
 * @note **Thread Safety:** Python/Main Thread.
 */
auto AudioEngine::remove_node(const AudioEngine::NodeHandleID& handle) noexcept
    -> std::expected<void, EngineError> {
    if (!command_queue_.try_push(
            {.command = commands::detail::RemoveNodePayload {.node_id = handle},
             .deadline_abs_sample = 0})) {
        // Propagate so controller knows to retry
        return std::unexpected<EngineError>(EngineError::COMMAND_QUEUE_FULL);
    }
    return {};
}

/**
 * @note **Thread Safety:** Python/Main Thread.
 */
auto AudioEngine::set_node_parameter(AudioEngine::NodeHandleID handle, uint32_t param_id,
                                     float value) noexcept
    -> std::expected<void, EngineError> {
    if (!command_queue_.try_push(
            {.command = commands::detail::SetNodeParamPayload {
                 .node_id = handle, .update = {.param_id = param_id, .value = value}},
             .deadline_abs_sample = 0})) {
        return std::unexpected<EngineError>(EngineError::COMMAND_QUEUE_FULL);
    }

    return {};
}

/**
 * @note **Thread Safety:** Python/Main Thread.
 */
auto AudioEngine::connect(NodeHandleID source, NodeHandleID dest) noexcept
    -> std::expected<void, EngineError> {
    if (!command_queue_.try_push(
            {.command = commands::detail::ConnectNodesPayload {
                 .source_id = source, .dest_id = dest},
             .deadline_abs_sample = 0})) {
        return std::unexpected<EngineError>(EngineError::COMMAND_QUEUE_FULL);
    }
    return {};
}

/**
 * @note **Thread Safety:** Python/Main Thread.
 */
auto AudioEngine::disconnect(NodeHandleID source, NodeHandleID dest) noexcept
    -> std::expected<void, EngineError> {
    if (!command_queue_.try_push(
            {.command = commands::detail::DisconnectNodesPayload {
                 .source_id = source, .dest_id = dest},
             .deadline_abs_sample = 0})) {
        return std::unexpected<EngineError>(EngineError::COMMAND_QUEUE_FULL);
    }
    return {};
}

/**
 * @note **Thread Safety:** Python/Main Thread.
 */
auto AudioEngine::set_bpm(double bpm) noexcept -> std::expected<void, EngineError> {
    if (bpm <= 0.0 || bpm > 1000.0) {
        return std::unexpected<EngineError>(EngineError::INVALID_PARAMETER);
    }
    if (!command_queue_.try_push(
            {.command = commands::detail::SetBpmPayload {.bpm = bpm},
             .deadline_abs_sample = 0})) {
        return std::unexpected<EngineError>(EngineError::COMMAND_QUEUE_FULL);
    };
    return {};
};

/**
 * @note **Thread Safety:** Python/Main Thread.
 */
auto AudioEngine::schedule(commands::detail::CommandPayload cmd, TimeSpec time) noexcept
    -> std::expected<void, EngineError> {
    uint64_t deadline {to_abs_sample(
        time, current_sample_.load(std::memory_order_relaxed), sample_rate_,
        bpm_.load(std::memory_order_relaxed))};
    if (!command_queue_.try_push({.command = cmd, .deadline_abs_sample = deadline})) {
        return std::unexpected(EngineError::COMMAND_QUEUE_FULL);
    }
    return {};
}

/**
 * @details Dispatches on the `TimeSpec` variant. `Beats` mode polls `steady_clock` in
 *          1 ms intervals, draining `remaining_beats` using a live `bpm_` read each
 *          iteration so tempo changes during a sleep are respected.
 *
 * @note **Thread Safety:** Python/Main Thread. GIL released by nanobind binding.
 */
void AudioEngine::sleep(TimeSpec time) noexcept {
    using namespace std::chrono;
    constexpr auto POLL_INTERVAL {1ms};

    std::visit(
        [&]<typename T>(T spec) {
            if constexpr (std::same_as<T, Samples>) {
                uint64_t target {current_sample_.load(std::memory_order_relaxed)
                                 + spec.count};
                while (current_sample_.load(std::memory_order_relaxed) < target) {
                    std::this_thread::sleep_for(POLL_INTERVAL);
                }
            } else if constexpr (std::same_as<T, Seconds>) {
                std::this_thread::sleep_for(duration<double>(spec.value));
            } else {
                double remaining_beats {spec.value};
                auto last {steady_clock::now()};
                while (remaining_beats > 0.0) {
                    std::this_thread::sleep_for(POLL_INTERVAL);
                    auto now {steady_clock::now()};
                    double elapsed_seconds {duration<double>(now - last).count()};
                    last = now;
                    double bpm {bpm_.load(std::memory_order_relaxed)};
                    remaining_beats -= elapsed_seconds * bpm / 60.0;
                }
            }
        },
        time);
}

/**
 * @note **Thread Safety:** Any thread (immutable after construction).
 */
auto AudioEngine::get_master_output() const noexcept -> NodeHandleID {
    return master_output_id_;
}

/**
 * @note **Thread Safety:** Python/Main Thread. GIL released by nanobind binding.
 */
void AudioEngine::play() noexcept {
    ma_result result = ma_device_start(&device_);
    if (result != MA_SUCCESS) {
        std::println(std::cerr, "Failed to start audio device!: {}",
                     std::to_string(result));
        std::terminate();
    }
}

/**
 * @note **Thread Safety:** Python/Main Thread. GIL released by nanobind binding.
 */
void AudioEngine::stop() noexcept {
    ma_result result = ma_device_stop(&device_);
    if (result != MA_SUCCESS) {
        std::println(std::cerr, "Failed to stop audio device!: {}",
                     std::to_string(result));
        std::terminate();
    }

    // drain remaining commands in command queue
    commands::detail::ScheduledCommandPayload cmd;
    while (command_queue_.try_pop(cmd)) {
        [[maybe_unused]] auto scheduled {
            event_scheduler_.schedule(cmd.command, cmd.deadline_abs_sample)};
        assert(scheduled);
    }

    event_scheduler_.process_block(current_sample_.load(std::memory_order_relaxed), 0,
                                   node_pool_, janitor_, connection_table_, bpm_);
}

/**
 * @details Per-block steps:
 *          1. Drain SPSC into the EDF heap (wait-free `try_pop` loop).
 *          2. `process_block()` — fire all commands due this block.
 *          3. Zero the output buffer.
 *          4. For each connection: render source generator into the scratch buffer,
 *             mix via sink into the hardware output buffer.
 *          5. Increment `current_sample_` by `frame_count` (`relaxed`).
 *
 *          The scratch buffer is a fixed-size `std::array<float, 8192>` on the stack —
 *          no heap allocation. It is zeroed before each generator render via
 *          `std::ranges::fill`.
 *
 * @note **Thread Safety:** Audio Thread (SCHED_FIFO priority 99). Real-time Safe.
 */
// NOLINTNEXTLINE we are matching miniaudio API
void AudioEngine::audio_callback(ma_device* device, void* output, const void* input,
                                 ma_uint32 frame_count) {
    static_cast<void>(input);

    auto* engine {static_cast<AudioEngine*>(device->pUserData)};
    auto output_buffer {
        std::span<float> {static_cast<float*>(output), frame_count * 2UZ}};

    commands::detail::ScheduledCommandPayload cmd;
    while (engine->command_queue_.try_pop(cmd)) {
        [[maybe_unused]] auto scheduled {
            engine->event_scheduler_.schedule(cmd.command, cmd.deadline_abs_sample)};
        assert(scheduled);
    }

    engine->event_scheduler_.process_block(engine->current_sample_, frame_count,
                                           engine->node_pool_, engine->janitor_,
                                           engine->connection_table_, engine->bpm_);

    std::ranges::fill(output_buffer, 0.0F);

    // scratch buffer for per-node rendering
    std::array<float, 8192> scratch {};
    auto scratch_span {std::span {scratch.data(), frame_count * 2UZ}};

    engine->connection_table_.for_each_connection([&](const graph::Connection& conn)
                                                      -> void {
        auto source {engine->node_pool_.get_node(conn.source)};
        auto dest {engine->node_pool_.get_node(conn.dest)};
        if (!source || !dest) {
            return;
        }

        std::ranges::fill(scratch_span, 0.0F);

        std::visit(
            [&](auto& src_node) -> void {
                if constexpr (nodes::GeneratorNode<std::decay_t<decltype(src_node)>>) {
                    src_node.render_frame(scratch_span);
                }
            },
            *source.value());

        std::visit(
            [&](auto& dst_node) -> void {
                if constexpr (nodes::SinkNode<std::decay_t<decltype(dst_node)>>) {
                    dst_node.mix_to_output(scratch_span, output_buffer);
                }
            },
            *dest.value());
    });

    engine->current_sample_.fetch_add(frame_count, std::memory_order_relaxed);
}

} // namespace mach::engine
