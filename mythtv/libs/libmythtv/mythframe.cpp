//
//  mythframe.cpp
//  MythTV
//
//  Created by Jean-Yves Avenard on 10/06/2014.
//  Copyright (c) 2014 Bubblestuff Pty Ltd. All rights reserved.
//
// derived from copy.c: Fast YV12/NV12 copy from VLC project
// portion of SSE Code Copyright (C) 2010 Laurent Aimar

/******************************************************************************
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#include "mythframe.h"

#if ARCH_X86

static int has_sse2     = -1;
static int has_sse3     = -1;
static int has_sse4     = -1;

#ifdef _WIN32
//  Windows
#define cpuid    __cpuid

#else
inline void cpuid(int CPUInfo[4],int InfoType)
{
    __asm__ __volatile__ (
    // pic requires to save ebx/rbx
#if ARCH_X86_64
    "push       %%rbx\n"
#else
    "push       %%ebx\n"
#endif
    "cpuid\n"
    "movl %%ebx ,%[ebx]\n"
#if ARCH_X86_64
    "pop        %%rbx\n"
#else
    "pop        %%ebx\n"
#endif
    :"=a" (CPUInfo[0]),
    [ebx] "=r"(CPUInfo[1]),
    "=c" (CPUInfo[2]),
    "=d" (CPUInfo[3]) :
    "a" (InfoType)
    );
}
#endif

static inline bool sse2_check()
{
    if (has_sse2 != -1)
    {
        return has_sse2;
    }

    int info[4];
    cpuid(info, 0);
    int nIds = info[0];

    //  Detect Features
    if (nIds >= 0x00000001)
    {
        cpuid(info,0x00000001);
        has_sse2  = (info[3] & ((int)1 << 26)) != 0;
        has_sse3  = (info[2] & ((int)1 <<  0)) != 0;
        has_sse4  = (info[2] & ((int)1 << 19)) != 0;
    }
    else
    {
        has_sse2  = 0;
        has_sse3  = 0;
        has_sse4  = 0;
    }
    return has_sse2;
}

static inline bool sse3_check()
{
    if (has_sse3 != -1)
    {
        return has_sse3;
    }

    sse2_check();

    return has_sse3;
}

static inline void SSE_splitplanes(uint8_t* dstu, int dstu_pitch,
                                   uint8_t* dstv, int dstv_pitch,
                                   const uint8_t* src, int src_pitch,
                                   int width, int height)
{
    const uint8_t shuffle[] = { 0, 2, 4, 6, 8, 10, 12, 14,
                                1, 3, 5, 7, 9, 11, 13, 15 };
    const uint8_t mask[] = { 0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00,
                             0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00 };
    const bool sse3 = sse3_check();

    asm volatile ("mfence");

#define LOAD64A \
    "movdqa  0(%[src]), %%xmm0\n" \
    "movdqa 16(%[src]), %%xmm1\n" \
    "movdqa 32(%[src]), %%xmm2\n" \
    "movdqa 48(%[src]), %%xmm3\n"

#define LOAD64U \
    "movdqu  0(%[src]), %%xmm0\n" \
    "movdqu 16(%[src]), %%xmm1\n" \
    "movdqu 32(%[src]), %%xmm2\n" \
    "movdqu 48(%[src]), %%xmm3\n"

#define STORE2X32 \
    "movq   %%xmm0,   0(%[dst1])\n" \
    "movq   %%xmm1,   8(%[dst1])\n" \
    "movhpd %%xmm0,   0(%[dst2])\n" \
    "movhpd %%xmm1,   8(%[dst2])\n" \
    "movq   %%xmm2,  16(%[dst1])\n" \
    "movq   %%xmm3,  24(%[dst1])\n" \
    "movhpd %%xmm2,  16(%[dst2])\n" \
    "movhpd %%xmm3,  24(%[dst2])\n"

    for (int y = 0; y < height; y++)
    {
        int x = 0;

        if (((uintptr_t)src & 0xf) == 0)
        {
            if (sse3)
            {
                for (; x < (width & ~31); x += 32)
                {
                    asm volatile (
                        "movdqu (%[shuffle]), %%xmm7\n"
                        LOAD64A
                        "pshufb  %%xmm7, %%xmm0\n"
                        "pshufb  %%xmm7, %%xmm1\n"
                        "pshufb  %%xmm7, %%xmm2\n"
                        "pshufb  %%xmm7, %%xmm3\n"
                        STORE2X32
                        : : [dst1]"r"(&dstu[x]), [dst2]"r"(&dstv[x]), [src]"r"(&src[2*x]), [shuffle]"r"(shuffle) : "memory", "xmm0", "xmm1", "xmm2", "xmm3", "xmm7");
                }
            }
            else
            {
                for (; x < (width & ~31); x += 32)
                {
                    asm volatile (
                        "movdqu (%[mask]), %%xmm7\n"
                        LOAD64A
                        "movdqa   %%xmm0, %%xmm4\n"
                        "movdqa   %%xmm1, %%xmm5\n"
                        "movdqa   %%xmm2, %%xmm6\n"
                        "psrlw    $8,     %%xmm0\n"
                        "psrlw    $8,     %%xmm1\n"
                        "pand     %%xmm7, %%xmm4\n"
                        "pand     %%xmm7, %%xmm5\n"
                        "pand     %%xmm7, %%xmm6\n"
                        "packuswb %%xmm4, %%xmm0\n"
                        "packuswb %%xmm5, %%xmm1\n"
                        "pand     %%xmm3, %%xmm7\n"
                        "psrlw    $8,     %%xmm2\n"
                        "psrlw    $8,     %%xmm3\n"
                        "packuswb %%xmm6, %%xmm2\n"
                        "packuswb %%xmm7, %%xmm3\n"
                        STORE2X32
                        : : [dst2]"r"(&dstu[x]), [dst1]"r"(&dstv[x]), [src]"r"(&src[2*x]), [mask]"r"(mask) : "memory", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7");
                }
            }
        }
        else
        {
            if (sse3)
            {
                for (; x < (width & ~31); x += 32)
                {
                    asm volatile (
                        "movdqu (%[shuffle]), %%xmm7\n"
                        LOAD64U
                        "pshufb  %%xmm7, %%xmm0\n"
                        "pshufb  %%xmm7, %%xmm1\n"
                        "pshufb  %%xmm7, %%xmm2\n"
                        "pshufb  %%xmm7, %%xmm3\n"
                        STORE2X32
                        : : [dst1]"r"(&dstu[x]), [dst2]"r"(&dstv[x]), [src]"r"(&src[2*x]), [shuffle]"r"(shuffle) : "memory", "xmm0", "xmm1", "xmm2", "xmm3", "xmm7");
                }
            }
            else
            {
                for (; x < (width & ~31); x += 32)
                {
                    asm volatile (
                        "movdqu (%[mask]), %%xmm7\n"
                        LOAD64U
                        "movdqu   %%xmm0, %%xmm4\n"
                        "movdqu   %%xmm1, %%xmm5\n"
                        "movdqu   %%xmm2, %%xmm6\n"
                        "psrlw    $8,     %%xmm0\n"
                        "psrlw    $8,     %%xmm1\n"
                        "pand     %%xmm7, %%xmm4\n"
                        "pand     %%xmm7, %%xmm5\n"
                        "pand     %%xmm7, %%xmm6\n"
                        "packuswb %%xmm4, %%xmm0\n"
                        "packuswb %%xmm5, %%xmm1\n"
                        "pand     %%xmm3, %%xmm7\n"
                        "psrlw    $8,     %%xmm2\n"
                        "psrlw    $8,     %%xmm3\n"
                        "packuswb %%xmm6, %%xmm2\n"
                        "packuswb %%xmm7, %%xmm3\n"
                        STORE2X32
                        : : [dst2]"r"(&dstu[x]), [dst1]"r"(&dstv[x]), [src]"r"(&src[2*x]), [mask]"r"(mask) : "memory", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7");
                }
            }
        }

        for (; x < width; x++)
        {
            dstu[x] = src[2*x+0];
            dstv[x] = src[2*x+1];
        }
        src  += src_pitch;
        dstu += dstu_pitch;
        dstv += dstv_pitch;
    }
    asm volatile ("mfence");

#undef STORE2X32
#undef LOAD64U
#undef LOAD64A
}
#endif /* ARCH_X86 */

static inline void copyplane(uint8_t* dst, int dst_pitch,
                             const uint8_t* src, int src_pitch,
                             int width, int height)
{
    for (int y = 0; y < height; y++)
    {
        memcpy(dst, src, width);
        src += src_pitch;
        dst += dst_pitch;
    }
}

static void splitplanes(uint8_t* dstu, int dstu_pitch,
                        uint8_t* dstv, int dstv_pitch,
                        const uint8_t* src, int src_pitch,
                        int width, int height)
{
    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            dstu[x] = src[2*x+0];
            dstv[x] = src[2*x+1];
        }
        src  += src_pitch;
        dstu += dstu_pitch;
        dstv += dstv_pitch;
    }
}

void framecopy(VideoFrame* dst, const VideoFrame* src, bool useSSE)
{
    VideoFrameType codec = dst->codec;
    if (!(dst->codec == src->codec ||
          (src->codec == FMT_NV12 && dst->codec == FMT_YV12)))
        return;

    dst->interlaced_frame = src->interlaced_frame;
    dst->repeat_pict      = src->repeat_pict;
    dst->top_field_first  = src->top_field_first;

    if (FMT_YV12 == codec)
    {
        int width   = src->width;
        int height  = src->height;
        int dwidth  = dst->width;
        int dheight = dst->height;

        if (src->codec == FMT_NV12 &&
            height == dheight && width == dwidth)
        {
            copyplane(dst->buf + dst->offsets[0], dst->pitches[0],
                      src->buf + src->offsets[0], src->pitches[0],
                      width, height);
#if ARCH_X86
            if (useSSE && sse2_check())
            {
                SSE_splitplanes(dst->buf + dst->offsets[1], dst->pitches[1],
                                dst->buf + dst->offsets[2], dst->pitches[2],
                                src->buf + src->offsets[1], src->pitches[1],
                                (width+1) / 2, (height+1) / 2);
                asm volatile ("emms");
                return;
            }
#endif
            splitplanes(dst->buf + dst->offsets[1], dst->pitches[1],
                        dst->buf + dst->offsets[2], dst->pitches[2],
                        src->buf + src->offsets[1], src->pitches[1],
                        (width+1) / 2, (height+1) / 2);
            return;
        }

        if (height == dheight && width == dwidth &&
            dst->pitches[0] != src->pitches[0])
        {
            // We have a different stride between the two frames
            // drop the garbage data
            copyplane(dst->buf + dst->offsets[0], dst->pitches[0],
                      src->buf + src->offsets[0], src->pitches[0],
                      width, height);
            copyplane(dst->buf + dst->offsets[1], dst->pitches[1],
                      src->buf + src->offsets[1], src->pitches[1],
                      (width+1) / 2, (height+1) / 2);
            copyplane(dst->buf + dst->offsets[2], dst->pitches[2],
                      src->buf + src->offsets[2], src->pitches[2],
                      (width+1) / 2, (height+1) / 2);
            return;
        }

        int height0 = (dst->height < src->height) ? dst->height : src->height;
        int height1 = (height0+1) >> 1;
        int height2 = (height0+1) >> 1;
        int pitch0  = ((dst->pitches[0] < src->pitches[0]) ?
                       dst->pitches[0] : src->pitches[0]);
        int pitch1  = ((dst->pitches[1] < src->pitches[1]) ?
                       dst->pitches[1] : src->pitches[1]);
        int pitch2  = ((dst->pitches[2] < src->pitches[2]) ?
                       dst->pitches[2] : src->pitches[2]);

        memcpy(dst->buf + dst->offsets[0],
               src->buf + src->offsets[0], pitch0 * height0);
        memcpy(dst->buf + dst->offsets[1],
               src->buf + src->offsets[1], pitch1 * height1);
        memcpy(dst->buf + dst->offsets[2],
               src->buf + src->offsets[2], pitch2 * height2);
    }
}
