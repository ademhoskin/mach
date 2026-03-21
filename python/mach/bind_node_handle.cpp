#include "bind_common.hpp"

#include <nanobind/stl/string.h>
#include <nanobind/stl/unordered_map.h>

void register_node_handle(nb::module_& m) {
    nb::class_<NodeHandle>(m, "NodeHandle")
        .def("__setitem__",
             [](NodeHandle& handle, const std::string& name, nb::object value) -> void {
                 if (!handle.param_map.contains(name)) {
                     throw std::runtime_error("unknown param: " + name);
                 }
                 float coerced;
                 if (nb::isinstance<nb::float_>(value)) {
                     coerced = nb::cast<float>(value);
                 } else if (nb::isinstance<nb::int_>(value)) {
                     coerced = static_cast<float>(nb::cast<int>(value));
                 } else if (nb::hasattr(value, "value")) {
                     // nanobind enums expose their underlying value via .value
                     coerced = static_cast<float>(nb::cast<int>(nb::getattr(value, "value")));
                 } else {
                     throw std::runtime_error("param value must be numeric");
                 }
                 auto result = handle.engine->set_node_parameter(
                     handle.id, handle.param_map.at(name), coerced);
                 if (!result) {
                     throw_engine_error(result.error());
                 }
             })
        .def("params",
             [](const NodeHandle& handle) -> std::unordered_map<std::string, uint32_t> {
                 return handle.param_map;
             });
}
