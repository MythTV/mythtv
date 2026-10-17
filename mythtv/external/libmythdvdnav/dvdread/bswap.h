/*
 * Copyright (C) 2000, 2001 Billy Biggs <vektor@dumbterm.net>,
 *                          Håkan Hjort <d95hjort@dtek.chalmers.se>
 *
 * This file is part of libdvdread.
 *
 * libdvdread is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * libdvdread is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with libdvdread; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef LIBDVDREAD_BSWAP_H
#define LIBDVDREAD_BSWAP_H

#if defined(WORDS_BIGENDIAN)
/* All bigendian systems are fine, just ignore the swaps. */
#define B2N_16(x) (void)(x)
#define B2N_32(x) (void)(x)
#define B2N_64(x) (void)(x)

#elif defined(__GNUC__) || defined(__clang__)

#define B2N_16(x) ((x) = __builtin_bswap16(x))
#define B2N_32(x) ((x) = __builtin_bswap32(x))
#define B2N_64(x) ((x) = __builtin_bswap64(x))

#elif defined(_MSC_VER)

#include <stdlib.h>

#define B2N_16(x) ((x) = _byteswap_ushort(x))
#define B2N_32(x) ((x) = _byteswap_ulong(x))
#define B2N_64(x) ((x) = _byteswap_uint64(x))

#else

#include <stdint.h>

#define BSWAP16(x) (((x) << 8 & 0xff00) | ((x) >> 8 & 0x00ff))
#define BSWAP32(x) (BSWAP16(x) << 16 | BSWAP16((x) >> 16))
#define BSWAP64(x) (BSWAP32(x) << 32 | BSWAP32((x) >> 32))

static inline uint16_t dvdread_bswap16(uint16_t x) { return BSWAP16(x); }
static inline uint32_t dvdread_bswap32(uint32_t x) { return BSWAP32(x); }
static inline uint64_t dvdread_bswap64(uint64_t x) { return BSWAP64(x); }

#define B2N_16(x) ((x) = dvdread_bswap16(x))
#define B2N_32(x) ((x) = dvdread_bswap32(x))
#define B2N_64(x) ((x) = dvdread_bswap64(x))

#undef BSWAP16
#undef BSWAP32
#undef BSWAP64

#endif

#endif /* LIBDVDREAD_BSWAP_H */
