#pragma once

#include "core/engine/engine.hpp"

#include <nanobind/nanobind.h>

#include <stdexcept>
#include <string>
#include <unordered_map>

/**
 * @file bind_common.hpp
 * @brief Shared types and helpers used across all nanobind binding translation units.
 *
 * @details Defines `NodeHandle` — the Python-facing proxy for a live DSP node — and
 *          `throw_engine_error()`, which converts `EngineError` values into Python
 *          exceptions. Every `bind_*.cpp` file includes this header.
 */

namespace nb = nanobind;

/**
 * @brief Python-facing proxy for a live DSP node.
 *
 * @details Python never holds a pointer into the C++ node pool directly. Instead it
 *          holds a `NodeHandle` which bundles:
 *          - `id` — the opaque `NodeHandleID` (uint64_t) used for all engine calls.
 *          - `engine` — a non-owning back-pointer to the `AudioEngine` that owns the
 *            node. The engine always outlives any `NodeHandle` the user holds.
 *          - `param_map` — a name→ID lookup table built once at `add_node()` time from
 *            `Node::get_params()`, so Python can use string names for parameters.
 *
 * @note **Thread Safety:** Python/Main Thread only. Never passed to the audio thread.
 */
struct NodeHandle {
    mach::engine::AudioEngine::NodeHandleID   id;        ///< Opaque engine-side node handle.
    mach::engine::AudioEngine*                engine;    ///< Non-owning; engine outlives handles.
    std::unordered_map<std::string, uint32_t> param_map; ///< Parameter name → param_id.
};

/**
 * @brief Variadic helper for `std::visit` over multiple lambdas.
 *
 * @tparam Ts Lambda types to compose into a single overload set.
 */
template<typename... Ts>
struct Overloaded : Ts... {
    using Ts::operator()...;
};

/**
 * @brief Converts an `EngineError` to a Python `RuntimeError` and throws it.
 *
 * @details Called by every nanobind lambda that receives a `std::unexpected` result.
 *          Never returns normally.
 *
 * @param err The engine error to translate.
 * @throws std::runtime_error always.
 *
 * @note **Thread Safety:** Python/Main Thread. GIL must be held (nanobind context).
 */
inline void throw_engine_error(mach::engine::EngineError err) {
    switch (err) {
        case mach::engine::EngineError::POOL_CAPACITY_EXCEEDED:
            throw std::runtime_error("pool capacity exceeded");
        case mach::engine::EngineError::COMMAND_QUEUE_FULL:
            throw std::runtime_error("command queue full");
        case mach::engine::EngineError::INVALID_PARAMETER:
            throw std::runtime_error("invalid parameter");
    }
}

void register_enums(nb::module_& m);
void register_node_handle(nb::module_& m);
void register_engine(nb::module_& m);
