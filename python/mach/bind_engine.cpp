#include "bind_common.hpp"

#include "core/common/time.hpp"
#include "core/engine/engine.hpp"
#include "core/nodes/wavetable/wavetable_osc.hpp"

#include <nanobind/stl/string.h>
#include <nanobind/stl/variant.h>

/**
 * @file bind_engine.cpp
 * @brief Registers time types, engine init params, and the `Engine` class with Python.
 *
 * @details This is the largest binding file. It exposes:
 *          - `Samples`, `Seconds`, `Beats` — time offset types.
 *          - `mach.note.*` submodule — named musical duration constants.
 *          - `EngineInitParams` — keyword-argument engine configuration.
 *          - `Engine` — the main Python API surface.
 *
 * @note GIL handling: `play()` and `stop()` use `nb::call_guard<nb::gil_scoped_release>`
 *       so the GIL is released for the duration of the blocking miniaudio call.
 *       `sleep()` releases the GIL manually inside its lambda. All other methods release
 *       the GIL after argument extraction (implicit in nanobind for non-Python objects).
 */

using namespace mach;
using namespace mach::engine;
using namespace mach::nodes::wavetable;

/**
 * @brief Registers all engine-related types into the nanobind module.
 *
 * @param m The `_mach_ext` module object.
 *
 * @note **Thread Safety:** Python/Main Thread — called once during module
 *       initialisation.
 */
void register_engine(nb::module_& m) {
    /**
     * @brief Python binding for `mach::Samples`.
     * @details Constructible as `mach.Samples(n)`. `count` is read-write.
     */
    nb::class_<Samples>(m, "Samples")
        .def(nb::init<uint64_t>())
        .def_rw("count", &Samples::count);

    /**
     * @brief Python binding for `mach::Seconds`.
     * @details Constructible as `mach.Seconds(s)`. `value` is read-write.
     */
    nb::class_<Seconds>(m, "Seconds")
        .def(nb::init<double>())
        .def_rw("value", &Seconds::value);

    /**
     * @brief Python binding for `mach::Beats`.
     * @details Constructible as `mach.Beats(b)`. `value` is read-write.
     *          1.0 = one quarter note. See `mach.note.*` for named constants.
     */
    nb::class_<Beats>(m, "Beats")
        .def(nb::init<double>())
        .def_rw("value", &Beats::value);

    /**
     * @brief Submodule exposing named musical duration constants.
     * @details All values are `mach::Beats` instances. Example:
     *          `engine.sleep(mach.note.QUARTER)`.
     */
    auto note_mod = m.def_submodule("note", "musical note duration constants");
    note_mod.attr("WHOLE")                = note::WHOLE;
    note_mod.attr("DOTTED_WHOLE")         = note::DOTTED_WHOLE;
    note_mod.attr("HALF")                 = note::HALF;
    note_mod.attr("DOTTED_HALF")          = note::DOTTED_HALF;
    note_mod.attr("QUARTER")              = note::QUARTER;
    note_mod.attr("DOTTED_QUARTER")       = note::DOTTED_QUARTER;
    note_mod.attr("EIGHTH")               = note::EIGHTH;
    note_mod.attr("DOTTED_EIGHTH")        = note::DOTTED_EIGHTH;
    note_mod.attr("SIXTEENTH")            = note::SIXTEENTH;
    note_mod.attr("DOTTED_SIXTEENTH")     = note::DOTTED_SIXTEENTH;
    note_mod.attr("THIRTY_SECOND")        = note::THIRTY_SECOND;
    note_mod.attr("DOTTED_THIRTY_SECOND") = note::DOTTED_THIRTY_SECOND;
    note_mod.attr("SIXTY_FOURTH")         = note::SIXTY_FOURTH;
    note_mod.attr("DOTTED_SIXTY_FOURTH")  = note::DOTTED_SIXTY_FOURTH;

    /**
     * @brief Python binding for `mach::engine::EngineInitParams`.
     * @details All fields are read-write. Typical Python usage:
     * @code
     * params = mach.EngineInitParams()
     * params.sample_rate = 48000
     * params.bpm = 140.0
     * engine = mach.Engine(params)
     * @endcode
     */
    nb::class_<EngineInitParams>(m, "EngineInitParams")
        .def(nb::init<>())
        .def_rw("sample_rate",        &EngineInitParams::sample_rate)
        .def_rw("block_size",         &EngineInitParams::block_size)
        .def_rw("max_node_pool_size", &EngineInitParams::max_node_pool_size)
        .def_rw("bpm",               &EngineInitParams::bpm);

    nb::class_<AudioEngine>(m, "Engine")
        .def(nb::init<const EngineInitParams&>())
        /**
         * @brief Adds a named node type and returns its `NodeHandle`.
         *
         * @details Currently supports `"wavetable_oscillator"`. The returned handle's
         *          `param_map` is pre-populated from `Node::get_params()`.
         *
         * @throws mach.EngineError on pool or queue exhaustion, or unknown node type.
         */
        .def("add_node",
             [](AudioEngine& engine, const std::string& node_type) -> NodeHandle {
                 NodeHandle handle;
                 handle.engine = &engine;

                 if (node_type == "wavetable_oscillator") {
                     auto result = engine.add_node<WavetableOscillator>();
                     if (!result) {
                         throw_engine_error(result.error());
                     }
                     handle.id = result.value();
                     for (const auto& desc : WavetableOscillator::get_params()) {
                         handle.param_map.emplace(std::string(desc.name),
                                                  static_cast<uint32_t>(desc.param_id));
                     }
                     return handle;
                 }

                 throw std::runtime_error("unknown node type: " + node_type);
             })
        /**
         * @brief Queues removal of the given node.
         * @throws mach.EngineError if the SPSC queue is full.
         */
        .def("remove_node",
             [](AudioEngine& engine, const NodeHandle& handle) -> void {
                 auto result = engine.remove_node(handle.id);
                 if (!result) {
                     throw_engine_error(result.error());
                 }
             })
        /**
         * @brief Queues a connection from `source` to `dest`.
         * @throws mach.EngineError if the SPSC queue is full.
         */
        .def("connect",
             [](AudioEngine& engine, const NodeHandle& source,
                const NodeHandle& dest) -> void {
                 auto result = engine.connect(source.id, dest.id);
                 if (!result) {
                     throw_engine_error(result.error());
                 }
             })
        /**
         * @brief Queues removal of the edge between `source` and `dest`.
         * @throws mach.EngineError if the SPSC queue is full.
         */
        .def("disconnect",
             [](AudioEngine& engine, const NodeHandle& source,
                const NodeHandle& dest) -> void {
                 auto result = engine.disconnect(source.id, dest.id);
                 if (!result) {
                     throw_engine_error(result.error());
                 }
             })
        /**
         * @brief Returns the `NodeHandle` for the singleton `MasterOutput` node.
         * @details The returned handle has an empty `param_map` (MasterOutput has no
         *          parameters). Use it as the `dest` argument to `connect()`.
         */
        .def("get_master_output",
             [](AudioEngine& engine) -> NodeHandle {
                 NodeHandle handle;
                 handle.engine = &engine;
                 handle.id = engine.get_master_output();
                 return handle;
             })
        /**
         * @brief Applies an immediate BPM change (queued via SPSC).
         * @throws mach.EngineError if bpm is out of range or the queue is full.
         */
        .def("set_bpm",
             [](AudioEngine& engine, double bpm) -> void {
                 auto result = engine.set_bpm(bpm);
                 if (!result) {
                     throw_engine_error(result.error());
                 }
             })
        /**
         * @brief Schedules a BPM change at the given `TimeSpec` offset.
         * @throws mach.EngineError if the SPSC queue is full.
         */
        .def("schedule_set_bpm",
             [](AudioEngine& engine, double bpm, const TimeSpec& time) -> void {
                 auto result = engine.schedule(
                     commands::detail::SetBpmPayload {.bpm = bpm}, time);
                 if (!result) {
                     throw_engine_error(result.error());
                 }
             })
        /**
         * @brief Schedules a parameter update on `handle` at the given `TimeSpec` offset.
         *
         * @details Accepts `float`, `int`, or a nanobind enum as `value`.
         * @throws mach.EngineError if the param name is unknown or the queue is full.
         */
        .def("schedule_set_param",
             [](AudioEngine& engine, const NodeHandle& handle,
                const std::string& name, nb::object value,
                const TimeSpec& time) -> void {
                 if (!handle.param_map.contains(name)) {
                     throw std::runtime_error("unknown param: " + name);
                 }
                 float coerced;
                 if (nb::isinstance<nb::float_>(value)) {
                     coerced = nb::cast<float>(value);
                 } else if (nb::isinstance<nb::int_>(value)) {
                     coerced = static_cast<float>(nb::cast<int>(value));
                 } else if (nb::hasattr(value, "value")) {
                     coerced = static_cast<float>(
                         nb::cast<int>(nb::getattr(value, "value")));
                 } else {
                     throw std::runtime_error("param value must be numeric");
                 }
                 auto result = engine.schedule(
                     commands::detail::SetNodeParamPayload {
                         .node_id = handle.id,
                         .update = {.param_id = handle.param_map.at(name),
                                    .value = coerced}},
                     time);
                 if (!result) {
                     throw_engine_error(result.error());
                 }
             })
        /**
         * @brief Blocks until the given `TimeSpec` duration has elapsed.
         * @details GIL is released for the duration of the sleep.
         */
        .def("sleep",
             [](AudioEngine& engine, const TimeSpec& time) -> void {
                 nb::gil_scoped_release release;
                 engine.sleep(time);
             })
        /// @brief Starts the audio callback. GIL released for the duration.
        .def("play", &AudioEngine::play, nb::call_guard<nb::gil_scoped_release>())
        /// @brief Stops the audio callback and flushes pending commands. GIL released.
        .def("stop", &AudioEngine::stop, nb::call_guard<nb::gil_scoped_release>());
}
