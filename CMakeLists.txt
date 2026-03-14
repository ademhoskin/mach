cmake_minimum_required(VERSION 3.28)
project(mach_core LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

# --- miniaudio (single-header, no CMake targets) ---
FetchContent_Declare(
  miniaudio
  GIT_REPOSITORY https://github.com/mackron/miniaudio.git
  GIT_TAG        0.11.25
  GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(miniaudio)

# --- doctest ---
FetchContent_Declare(
  doctest
  GIT_REPOSITORY https://github.com/doctest/doctest.git
  GIT_TAG        v2.4.12
  GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(doctest)

# --- Python ---
find_package(Python 3.14 COMPONENTS Interpreter Development.Module REQUIRED)

# --- nanobind ---
FetchContent_Declare(
  nanobind
  GIT_REPOSITORY https://github.com/wjakob/nanobind.git
  GIT_TAG        v2.12.0
  GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(nanobind)

# --- Core C++ library ---
add_library(mach_core STATIC core/engine/engine.cpp)
target_include_directories(mach_core PUBLIC ${CMAKE_SOURCE_DIR} ${miniaudio_SOURCE_DIR})
target_compile_options(mach_core PRIVATE -O3 -march=native -Wall -Wextra)

# --- Placeholder test runner ---
add_executable(mach_tests tests/main.cpp)
target_link_libraries(mach_tests PRIVATE mach_core doctest::doctest)
target_compile_options(mach_tests PRIVATE -O0 -g -Wall -Wextra -Wno-#warnings)

# --- nanobind extension ---
nanobind_add_module(_mach_ext STABLE_ABI python/mach/bindings.cpp)
target_link_libraries(_mach_ext PRIVATE mach_core)
target_compile_options(_mach_ext PRIVATE -O3 -march=native -Wall -Wextra)

# Drop the .so directly into python/mach/ so `import mach` works without installing
set_target_properties(_mach_ext PROPERTIES
  LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/python/mach"
)
