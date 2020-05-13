#ifndef MYTHTV_BSWAP_H
#define MYTHTV_BSWAP_H

#include <cstdint> /* uint32_t */

#define bswap_dbl(x) bswap_64(x)

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

#endif /* ndef MYTHTV_BSWAP_H */
