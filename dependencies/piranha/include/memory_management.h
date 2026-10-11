#ifndef PIRANHA_MEMORY_MANAGEMENT_H
#define PIRANHA_MEMORY_MANAGEMENT_H

#include <assert.h>
#include <cstdint>

namespace piranha {

    typedef std::uint64_t mem_size;

    constexpr mem_size KB = 1000;
    constexpr mem_size MB = 1000 * KB;
    constexpr mem_size GB = 1000 * MB;

#define CHECK_ALIGNMENT(pointer, required) assert((((std::uintptr_t)((char *)(pointer))) % (required)) == 0)

} /* namespace piranha */

#endif /* PIRANHA_MEMORY_MANAGEMENT_H */
