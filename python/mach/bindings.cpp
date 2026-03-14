#include <nanobind/nanobind.h>

namespace nb = nanobind;

NB_MODULE(_mach_ext, m) {
    m.doc() = "mach audio engine";
}
