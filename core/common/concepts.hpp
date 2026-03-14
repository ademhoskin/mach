#include <cstddef>
namespace mach::concepts {

template<std::size_t N>
concept IsAPowerOfTwo = requires(N&(N - 1)) == 0;
}
