#include "bind_common.hpp"

#include "core/engine/engine.hpp"
#include "core/nodes/wavetable/wavetable_osc.hpp"

#include <nanobind/stl/string.h>

using namespace mach::engine;
using namespace mach::nodes::wavetable;

void register_engine(nb::module_& m) {
    nb::class_<EngineInitParams>(m, "EngineInitParams")
        .def(nb::init<>())
        .def_rw("sample_rate",        &EngineInitParams::sample_rate)
        .def_rw("block_size",         &EngineInitParams::block_size)
        .def_rw("max_node_pool_size", &EngineInitParams::max_node_pool_size);

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
        .def("play", &AudioEngine::play, nb::call_guard<nb::gil_scoped_release>())
        .def("stop", &AudioEngine::stop, nb::call_guard<nb::gil_scoped_release>());
}
