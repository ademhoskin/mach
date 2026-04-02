#include "bind_common.hpp"

#include <nanobind/nanobind.h>

/**
 * @file bindings.cpp
 * @brief nanobind module entry point for the `_mach_ext` extension.
 *
 * @details Registers all Python-visible types and exceptions by delegating to the
 *          per-subsystem `register_*` functions. Import order matters: enums must be
 *          registered before `NodeHandle` (which references `Waveform`), and both
 *          before `Engine`.
 *
 * @note **Thread Safety:** Python/Main Thread — called once at `import _mach_ext`.
 */

NB_MODULE(_mach_ext, m) { // NOLINT
    m.doc() = "mach audio engine";

    /// @brief Maps `std::runtime_error` thrown by engine operations to a Python
    ///        `EngineError` exception class, preserving the message string.
    // NOLINTNEXTLINE
    nb::exception<std::runtime_error>(m, "EngineError");

    register_enums(m);
    register_node_handle(m);
    register_engine(m);
}
