#pragma once

#include "core/engine/engine.hpp"

#include <nanobind/nanobind.h>

#include <stdexcept>
#include <string>
#include <unordered_map>

namespace nb = nanobind;

struct NodeHandle {
    mach::engine::AudioEngine::NodeHandleID   id;
    mach::engine::AudioEngine*                engine; // non-owning, engine outlives handles
    std::unordered_map<std::string, uint32_t> param_map; // name → param_id
};

template<typename... Ts>
struct Overloaded : Ts... {
    using Ts::operator()...;
};

inline void throw_engine_error(mach::engine::EngineError err) {
    switch (err) {
        case mach::engine::EngineError::POOL_CAPACITY_EXCEEDED:
            throw std::runtime_error("pool capacity exceeded");
        case mach::engine::EngineError::COMMAND_QUEUE_FULL:
            throw std::runtime_error("command queue full");
    }
}

void register_enums(nb::module_& m);
void register_node_handle(nb::module_& m);
void register_engine(nb::module_& m);
