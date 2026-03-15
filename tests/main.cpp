#define DOCTEST_CONFIG_IMPLEMENT
#include <doctest/doctest.h>

auto main(int argc, char** argv) -> int {
    doctest::Context ctx {argc, argv};
    ctx.setOption("success", true);

    return ctx.run();
}
