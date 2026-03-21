#include "bind_common.hpp"

#include <nanobind/nanobind.h>

NB_MODULE(_mach_ext, m) { // NOLINT
    m.doc() = "mach audio engine";

    // NOLINTNEXTLINE
    nb::exception<std::runtime_error>(m, "EngineError");

    register_enums(m);
    register_node_handle(m);
    register_engine(m);
}
