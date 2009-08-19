/*
 * copyright (c) 2006 Michael Niedermayer <michaelni@gmx.at>
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

/**
 * @file libavutil/internal.h
 * common internal API header
 */

#ifndef AVUTIL_DECLARE_ALIGNED_H
#define AVUTIL_DECLARE_ALIGNED_H

#include "common.h"

#ifndef attribute_align_arg
#if (!defined(__ICC) || __ICC > 1110) && AV_GCC_VERSION_AT_LEAST(4,2)
#    define attribute_align_arg __attribute__((force_align_arg_pointer))
#else
#    define attribute_align_arg
#endif
#endif

#if defined(__ICC) || defined(__SUNPRO_C)
    #define DECLARE_ALIGNED(n,t,v)      t v __attribute__ ((aligned (n)))
    #define DECLARE_ASM_CONST(n,t,v)    const t __attribute__ ((aligned (n))) v
#elif defined(__GNUC__)
    #define DECLARE_ALIGNED(n,t,v)      t v __attribute__ ((aligned (n)))
    #define DECLARE_ASM_CONST(n,t,v)    static const t v attribute_used __attribute__ ((aligned (n)))
#elif defined(_MSC_VER)
    #define DECLARE_ALIGNED(n,t,v)      __declspec(align(n)) t v
    #define DECLARE_ASM_CONST(n,t,v)    __declspec(align(n)) static const t v
#elif HAVE_INLINE_ASM
    #error The asm code needs alignment, but we do not know how to do it for this compiler.
#else
    #define DECLARE_ALIGNED(n,t,v)      t v
    #define DECLARE_ASM_CONST(n,t,v)    static const t v
#endif


#else /* AVUTIL_DECLARE_ALIGNED_H */
#error libavutil/declare_aligned.h include more than once
#endif /* AVUTIL_DECLARE_ALIGNED_H */
