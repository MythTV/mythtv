#ifndef MYTHTV_BSWAP_H
#define MYTHTV_BSWAP_H

#include <stdint.h> /* uint32_t */
#include <byteswap.h> /* bswap_16|32|64 */

static __inline__ double bswap_dbl(double x)
{
    union {
        uint32_t l[2];
        double   d;
    } w, r;
    w.d = x;
    r.l[0] = bswap_32(w.l[1]);
    r.l[1] = bswap_32(w.l[0]);
    return r.d;
}

#endif /* ndef MYTHTV_BSWAP_H */
