#include "bind_common.hpp"

#include "core/common/time.hpp"
#include "core/engine/engine.hpp"
#include "core/nodes/wavetable/wavetable_osc.hpp"

#include <nanobind/stl/string.h>
#include <nanobind/stl/variant.h>

using namespace mach;
using namespace mach::engine;
using namespace mach::nodes::wavetable;

void register_engine(nb::module_& m) {
    nb::class_<Samples>(m, "Samples")
        .def(nb::init<uint64_t>())
        .def_rw("count", &Samples::count);

    nb::class_<Seconds>(m, "Seconds")
        .def(nb::init<double>())
        .def_rw("value", &Seconds::value);

    nb::class_<Beats>(m, "Beats")
        .def(nb::init<double>())
        .def_rw("value", &Beats::value);

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

    nb::class_<EngineInitParams>(m, "EngineInitParams")
        .def(nb::init<>())
        .def_rw("sample_rate",        &EngineInitParams::sample_rate)
        .def_rw("block_size",         &EngineInitParams::block_size)
        .def_rw("max_node_pool_size", &EngineInitParams::max_node_pool_size)
        .def_rw("bpm",               &EngineInitParams::bpm);

    nb::class_<AudioEngine>(m, "Engine")
        .def(nb::init<const EngineInitParams&>())
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
        .def("remove_node",
             [](AudioEngine& engine, const NodeHandle& handle) -> void {
                 auto result = engine.remove_node(handle.id);
                 if (!result) {
                     throw_engine_error(result.error());
                 }
             })
        .def("connect",
             [](AudioEngine& engine, const NodeHandle& source,
                const NodeHandle& dest) -> void {
                 auto result = engine.connect(source.id, dest.id);
                 if (!result) {
                     throw_engine_error(result.error());
                 }
             })
        .def("disconnect",
             [](AudioEngine& engine, const NodeHandle& source,
                const NodeHandle& dest) -> void {
                 auto result = engine.disconnect(source.id, dest.id);
                 if (!result) {
                     throw_engine_error(result.error());
                 }
             })
        .def("get_master_output",
             [](AudioEngine& engine) -> NodeHandle {
                 NodeHandle handle;
                 handle.engine = &engine;
                 handle.id = engine.get_master_output();
                 return handle;
             })
        .def("set_bpm",
             [](AudioEngine& engine, double bpm) -> void {
                 auto result = engine.set_bpm(bpm);
                 if (!result) {
                     throw_engine_error(result.error());
                 }
             })
        .def("schedule_set_bpm",
             [](AudioEngine& engine, double bpm, const TimeSpec& time) -> void {
                 auto result = engine.schedule(
                     commands::detail::SetBpmPayload {.bpm = bpm}, time);
                 if (!result) {
                     throw_engine_error(result.error());
                 }
             })
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
        .def("sleep",
             [](AudioEngine& engine, const TimeSpec& time) -> void {
                 nb::gil_scoped_release release;
                 engine.sleep(time);
             })
        .def("play", &AudioEngine::play, nb::call_guard<nb::gil_scoped_release>())
        .def("stop", &AudioEngine::stop, nb::call_guard<nb::gil_scoped_release>());
}
