#include "core/engine/engine.hpp"
#include "core/nodes/wavetable/wavetable_osc.hpp"

#include <nanobind/nanobind.h>
#include <nanobind/stl/string.h>
namespace nb = nanobind;
using namespace mach::engine;
using namespace mach::nodes::wavetable;

NB_MODULE(_mach_ext, mach) { // NOLINT
    mach.doc() = "mach audio engine";

    // NOLINTNEXTLINE we register the exception in Python, no need to assign to lval
    nb::exception<std::runtime_error>(mach, "EngineError");

    const auto THROW_ENGINE_ERROR = [](EngineError err) -> void {
        switch (err) {
            case EngineError::POOL_CAPACITY_EXCEEDED:
                throw std::runtime_error("pool capacity exceeded");
            case EngineError::COMMAND_QUEUE_FULL:
                throw std::runtime_error("command queue full");
        }
    };

    nb::class_<EngineInitParams>(mach, "EngineInitParams")
        .def(nb::init<>())
        .def_rw("sample_rate", &EngineInitParams::sample_rate)
        .def_rw("block_size", &EngineInitParams::block_size)
        .def_rw("max_node_pool_size", &EngineInitParams::max_node_pool_size);

    nb::class_<AudioEngine>(mach, "AudioEngine")
        .def(nb::init<const EngineInitParams&>())
        .def("add_node",
             [&THROW_ENGINE_ERROR](AudioEngine& engine, const std::string& node_type)
                 -> AudioEngine::NodeHandleID {
                 if (node_type == "wavetable_oscillator") {
                     auto result {engine.add_node<WavetableOscillator>()};
                     if (!result) {
                         THROW_ENGINE_ERROR(result.error());
                     }
                     return result.value();
                 }
                 throw std::runtime_error("Unknown node type");
             })
        .def("remove_node",
             [&THROW_ENGINE_ERROR](AudioEngine& engine, uint64_t handle) -> void {
                 auto result {engine.remove_node(handle)};
                 if (!result) {
                     THROW_ENGINE_ERROR(result.error());
                 }
             })
        .def("set_node_parameter",
             [&THROW_ENGINE_ERROR](AudioEngine& engine, uint64_t handle,
                                   uint32_t param_id, float value) -> void {
                 auto result {engine.set_node_parameter(handle, param_id, value)};
                 if (!result) {
                     THROW_ENGINE_ERROR(result.error());
                 }
             })
        .def("play", &AudioEngine::play, nb::call_guard<nb::gil_scoped_release>())
        .def("stop", &AudioEngine::stop, nb::call_guard<nb::gil_scoped_release>());
}
