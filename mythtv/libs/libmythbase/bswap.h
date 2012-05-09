#ifndef MYTHTV_BSWAP_H
#define MYTHTV_BSWAP_H

#include <stdint.h> /* uint32_t */

#if HAVE_BYTESWAP_H
#  include <byteswap.h> /* bswap_16|32|64 */
#elif HAVE_SYS_ENDIAN_H
#  include <sys/endian.h>
#  if !defined(bswap_16) && defined(bswap16)
#    define bswap_16(x) bswap16(x)
#  endif
#  if !defined(bswap_32) && defined(bswap32)
#    define bswap_32(x) bswap32(x)
#  endif
#  if !defined(bswap_64) && defined(bswap64)
#    define bswap_64(x) bswap64(x)
#  endif
#elif CONFIG_DARWIN
#  include <libkern/OSByteOrder.h> 
#  define bswap_16(x) OSSwapInt16(x)
#  define bswap_32(x) OSSwapInt32(x)
#  define bswap_64(x) OSSwapInt64(x)
#elif HAVE_BIGENDIAN
#  error Byte swapping functions not defined for this platform
#endif

#ifdef bswap_32
// TODO: Any reason we choose not to use bswap_64 for this?
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
#endif

#endif /* ndef MYTHTV_BSWAP_H */
