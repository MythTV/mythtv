#ifndef MYTHTV_BSWAP_H
#define MYTHTV_BSWAP_H

#include <stdint.h> /* uint32_t */

#if HAVE_BYTESWAP_H
#  include <byteswap.h> /* bswap_16|32|64 */
#elif HAVE_SYS_ENDIAN_H
#  include <sys/endian.h>
#elif CONFIG_DARWIN
#  include <libkern/OSByteOrder.h> 
#  define bswap_16(x) OSSwapInt16(x)
#  define bswap_32(x) OSSwapInt32(x)
#  define bswap_64(x) OSSwapInt64(x)
#else
#  error Byte swapping functions not defined for this platform
#endif

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
