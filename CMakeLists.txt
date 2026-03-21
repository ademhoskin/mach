cmake_minimum_required(VERSION 3.28)
project(mach_core LANGUAGES C CXX)

# enforce clang / clang-cl
if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  message(FATAL_ERROR "mach requires clang or clang-cl (found ${CMAKE_CXX_COMPILER_ID})")
endif()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# platform-aware flags
if(MSVC)
  # clang-cl
  set(MACH_OPT_FLAGS /O2 /W4)
  set(MACH_DEBUG_FLAGS /Od /Zi /W4)
else()
  # clang++
  set(MACH_OPT_FLAGS -O3 -march=native -Wall -Wextra)
  set(MACH_DEBUG_FLAGS -O0 -g -Wall -Wextra)
endif()

include(FetchContent)

FetchContent_Declare(
  miniaudio
  GIT_REPOSITORY https://github.com/mackron/miniaudio.git
  GIT_TAG        0.11.25
  GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(miniaudio)

FetchContent_Declare(
  doctest
  GIT_REPOSITORY https://github.com/doctest/doctest.git
  GIT_TAG        v2.4.12
  GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(doctest)

find_package(Python 3.14 COMPONENTS Interpreter Development.Module REQUIRED)

FetchContent_Declare(
  nanobind
  GIT_REPOSITORY https://github.com/wjakob/nanobind.git
  GIT_TAG        v2.12.0
  GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(nanobind)

file(GLOB_RECURSE MACH_CORE_SOURCES "core/**/*.cpp" "core/**/*.c")
add_library(mach_core STATIC ${MACH_CORE_SOURCES})
target_include_directories(mach_core PUBLIC ${CMAKE_SOURCE_DIR} ${miniaudio_SOURCE_DIR})
target_compile_options(mach_core PRIVATE ${MACH_OPT_FLAGS})
set_target_properties(mach_core PROPERTIES POSITION_INDEPENDENT_CODE ON)

if(NOT WIN32)
  target_link_libraries(mach_core PUBLIC dl m pthread)
endif()

file(GLOB_RECURSE MACH_TEST_SOURCES "tests/**/*.cpp")
add_executable(mach_tests tests/main.cpp ${MACH_TEST_SOURCES})
target_link_libraries(mach_tests PRIVATE mach_core doctest::doctest)
target_compile_options(mach_tests PRIVATE ${MACH_DEBUG_FLAGS})
target_include_directories(mach_tests SYSTEM PRIVATE
  ${doctest_SOURCE_DIR}/doctest
)

add_executable(mach_bench bench/render_bench.cpp)
target_link_libraries(mach_bench PRIVATE mach_core)
target_compile_options(mach_bench PRIVATE ${MACH_OPT_FLAGS})
target_include_directories(mach_bench PRIVATE ${CMAKE_SOURCE_DIR})

nanobind_add_module(_mach_ext STABLE_ABI
    python/mach/bindings.cpp
    python/mach/bind_enums.cpp
    python/mach/bind_node_handle.cpp
    python/mach/bind_engine.cpp
)
target_link_libraries(_mach_ext PRIVATE mach_core)
target_compile_options(_mach_ext PRIVATE ${MACH_OPT_FLAGS})

set_target_properties(_mach_ext PROPERTIES
  LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/python/mach"
)
