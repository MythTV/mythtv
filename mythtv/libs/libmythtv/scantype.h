#ifndef SCAN_TYPE_H_
#define SCAN_TYPE_H_

#include <cstdint>

enum class SCAN_t : uint8_t {
    UNKNOWN_SCAN,
    INTERLACED,
    PROGRESSIVE,
    VARIABLE
};

#endif // SCAN_TYPE_H_
