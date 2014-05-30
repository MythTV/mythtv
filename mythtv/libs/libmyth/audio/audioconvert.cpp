
/*
 *  Class AudioConvert
 *  Created by Jean-Yves Avenard on 10/06/13.
 *
 *  Copyright (C) Bubblestuff Pty Ltd 2013
 *  Copyright (C) foobum@gmail.com 2010
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
 */

#include <math.h>
#include <sys/types.h>
#include <inttypes.h>

#include "mythconfig.h"
#include "mythlogging.h"
#include "audioconvert.h"

extern "C" {
#include "libavcodec/avcodec.h"
#include "libswresample/swresample.h"
}

#define LOC QString("AudioConvert: ")

#define ISALIGN(x) (((unsigned long)x & 0xf) == 0)

#if ARCH_X86
static int has_sse2 = -1;

// Check cpuid for SSE2 support on x86 / x86_64
static inline bool sse_check()
{
    if (has_sse2 != -1)
        return (bool)has_sse2;
    __asm__(
            // -fPIC - we may not clobber ebx/rbx
#if ARCH_X86_64
            "push       %%rbx               \n\t"
#else
            "push       %%ebx               \n\t"
#endif
            "mov        $1, %%eax           \n\t"
            "cpuid                          \n\t"
            "and        $0x4000000, %%edx   \n\t"
            "shr        $26, %%edx          \n\t"
#if ARCH_X86_64
            "pop        %%rbx               \n\t"
#else
            "pop        %%ebx               \n\t"
#endif
            :"=d"(has_sse2)
            ::"%eax","%ecx"
            );
    return (bool)has_sse2;
}
#endif //ARCH_x86

#if !HAVE_LRINTF
static av_always_inline av_const long int lrintf(float x)
{
    return (int)(rint(x));
}
#endif /* HAVE_LRINTF */

static inline float clipcheck(float f)
{
    if (f > 1.0f) f = 1.0f;
    else if (f < -1.0f) f = -1.0f;
    return f;
}

/*
 The SSE code processes 16 bytes at a time and leaves any remainder for the C
 */

static int toFloat8(float* out, const uchar* in, int len)
{
    int i = 0;
    float f = 1.0f / ((1<<7));

#if ARCH_X86
    if (sse_check() && len >= 16)
    {
        int loops = len >> 4;
        i = loops << 4;
        int a = 0x80808080;

        __asm__ volatile (
                          "movd       %3, %%xmm0          \n\t"
                          "movd       %4, %%xmm7          \n\t"
                          "punpckldq  %%xmm0, %%xmm0      \n\t"
                          "punpckldq  %%xmm7, %%xmm7      \n\t"
                          "punpckldq  %%xmm0, %%xmm0      \n\t"
                          "punpckldq  %%xmm7, %%xmm7      \n\t"
                          "1:                             \n\t"
                          "movdqu     (%1), %%xmm1        \n\t"
                          "xorpd      %%xmm2, %%xmm2      \n\t"
                          "xorpd      %%xmm3, %%xmm3      \n\t"
                          "psubb      %%xmm0, %%xmm1      \n\t"
                          "xorpd      %%xmm4, %%xmm4      \n\t"
                          "punpcklbw  %%xmm1, %%xmm2      \n\t"
                          "xorpd      %%xmm5, %%xmm5      \n\t"
                          "punpckhbw  %%xmm1, %%xmm3      \n\t"
                          "punpcklwd  %%xmm2, %%xmm4      \n\t"
                          "xorpd      %%xmm6, %%xmm6      \n\t"
                          "punpckhwd  %%xmm2, %%xmm5      \n\t"
                          "psrad      $24,    %%xmm4      \n\t"
                          "punpcklwd  %%xmm3, %%xmm6      \n\t"
                          "psrad      $24,    %%xmm5      \n\t"
                          "punpckhwd  %%xmm3, %%xmm1      \n\t"
                          "psrad      $24,    %%xmm6      \n\t"
                          "cvtdq2ps   %%xmm4, %%xmm4      \n\t"
                          "psrad      $24,    %%xmm1      \n\t"
                          "cvtdq2ps   %%xmm5, %%xmm5      \n\t"
                          "mulps      %%xmm7, %%xmm4      \n\t"
                          "cvtdq2ps   %%xmm6, %%xmm6      \n\t"
                          "mulps      %%xmm7, %%xmm5      \n\t"
                          "movups     %%xmm4, (%0)        \n\t"
                          "cvtdq2ps   %%xmm1, %%xmm1      \n\t"
                          "mulps      %%xmm7, %%xmm6      \n\t"
                          "movups     %%xmm5, 16(%0)      \n\t"
                          "mulps      %%xmm7, %%xmm1      \n\t"
                          "movups     %%xmm6, 32(%0)      \n\t"
                          "add        $16,    %1          \n\t"
                          "movups     %%xmm1, 48(%0)      \n\t"
                          "add        $64,    %0          \n\t"
                          "sub        $1, %%ecx           \n\t"
                          "jnz        1b                  \n\t"
                          :"+r"(out),"+r"(in)
                          :"c"(loops), "r"(a), "r"(f)
                          );
    }
#endif //ARCH_x86
    for (; i < len; i++)
        *out++ = (*in++ - 0x80) * f;
    return len << 2;
}

/*
 The SSE code processes 16 bytes at a time and leaves any remainder for the C
 - there is no remainder in practice */

static inline uchar clip_uchar(int a)
{
    if (a&(~0xFF)) return (-a)>>31;
    else           return a;
}

static int fromFloat8(uchar* out, const float* in, int len)
{
    int i = 0;
    float f = (1<<7);

#if ARCH_X86
    if (sse_check() && len >= 16)
    {
        int loops = len >> 4;
        i = loops << 4;
        int a = 0x80808080;

        __asm__ volatile (
                          "movd       %3, %%xmm0          \n\t"
                          "movd       %4, %%xmm7          \n\t"
                          "punpckldq  %%xmm0, %%xmm0      \n\t"
                          "punpckldq  %%xmm7, %%xmm7      \n\t"
                          "punpckldq  %%xmm0, %%xmm0      \n\t"
                          "punpckldq  %%xmm7, %%xmm7      \n\t"
                          "1:                             \n\t"
                          "movups     (%1), %%xmm1        \n\t"
                          "movups     16(%1), %%xmm2      \n\t"
                          "mulps      %%xmm7, %%xmm1      \n\t"
                          "movups     32(%1), %%xmm3      \n\t"
                          "mulps      %%xmm7, %%xmm2      \n\t"
                          "cvtps2dq   %%xmm1, %%xmm1      \n\t"
                          "movups     48(%1), %%xmm4      \n\t"
                          "mulps      %%xmm7, %%xmm3      \n\t"
                          "cvtps2dq   %%xmm2, %%xmm2      \n\t"
                          "mulps      %%xmm7, %%xmm4      \n\t"
                          "cvtps2dq   %%xmm3, %%xmm3      \n\t"
                          "packssdw   %%xmm2, %%xmm1      \n\t"
                          "cvtps2dq   %%xmm4, %%xmm4      \n\t"
                          "packssdw   %%xmm4, %%xmm3      \n\t"
                          "add        $64,    %1          \n\t"
                          "packsswb   %%xmm3, %%xmm1      \n\t"
                          "paddb      %%xmm0, %%xmm1      \n\t"
                          "movdqu     %%xmm1, (%0)        \n\t"
                          "add        $16,    %0          \n\t"
                          "sub        $1, %%ecx           \n\t"
                          "jnz        1b                  \n\t"
                          :"+r"(out),"+r"(in)
                          :"c"(loops), "r"(a), "r"(f)
                          );
    }
#endif //ARCH_x86
    for (;i < len; i++)
        *out++ = clip_uchar(lrintf(*in++ * f) + 0x80);
    return len;
}

static int toFloat16(float* out, const short* in, int len)
{
    int i = 0;
    float f = 1.0f / ((1<<15));

#if ARCH_X86
    if (sse_check() && len >= 16)
    {
        int loops = len >> 4;
        i = loops << 4;

        __asm__ volatile (
                          "movd       %3, %%xmm7          \n\t"
                          "punpckldq  %%xmm7, %%xmm7      \n\t"
                          "punpckldq  %%xmm7, %%xmm7      \n\t"
                          "1:                             \n\t"
                          "xorpd      %%xmm2, %%xmm2      \n\t"
                          "movdqu     (%1),   %%xmm1      \n\t"
                          "xorpd      %%xmm3, %%xmm3      \n\t"
                          "punpcklwd  %%xmm1, %%xmm2      \n\t"
                          "movdqu     16(%1), %%xmm4      \n\t"
                          "punpckhwd  %%xmm1, %%xmm3      \n\t"
                          "psrad      $16,    %%xmm2      \n\t"
                          "punpcklwd  %%xmm4, %%xmm5      \n\t"
                          "psrad      $16,    %%xmm3      \n\t"
                          "cvtdq2ps   %%xmm2, %%xmm2      \n\t"
                          "punpckhwd  %%xmm4, %%xmm6      \n\t"
                          "psrad      $16,    %%xmm5      \n\t"
                          "mulps      %%xmm7, %%xmm2      \n\t"
                          "cvtdq2ps   %%xmm3, %%xmm3      \n\t"
                          "psrad      $16,    %%xmm6      \n\t"
                          "mulps      %%xmm7, %%xmm3      \n\t"
                          "cvtdq2ps   %%xmm5, %%xmm5      \n\t"
                          "movups     %%xmm2, (%0)        \n\t"
                          "cvtdq2ps   %%xmm6, %%xmm6      \n\t"
                          "mulps      %%xmm7, %%xmm5      \n\t"
                          "movups     %%xmm3, 16(%0)      \n\t"
                          "mulps      %%xmm7, %%xmm6      \n\t"
                          "movups     %%xmm5, 32(%0)      \n\t"
                          "add        $32, %1             \n\t"
                          "movups     %%xmm6, 48(%0)      \n\t"
                          "add        $64, %0             \n\t"
                          "sub        $1, %%ecx           \n\t"
                          "jnz        1b                  \n\t"
                          :"+r"(out),"+r"(in)
                          :"c"(loops), "r"(f)
                          );
    }
#endif //ARCH_x86
    for (; i < len; i++)
        *out++ = *in++ * f;
    return len << 2;
}

static inline short clip_short(int a)
{
    if ((a+0x8000) & ~0xFFFF) return (a>>31) ^ 0x7FFF;
    else                      return a;
}

static int fromFloat16(short* out, const float* in, int len)
{
    int i = 0;
    float f = (1<<15);

#if ARCH_X86
    if (sse_check() && len >= 16)
    {
        int loops = len >> 4;
        i = loops << 4;

        __asm__ volatile (
                          "movd       %3, %%xmm7          \n\t"
                          "punpckldq  %%xmm7, %%xmm7      \n\t"
                          "punpckldq  %%xmm7, %%xmm7      \n\t"
                          "1:                             \n\t"
                          "movups     (%1), %%xmm1        \n\t"
                          "movups     16(%1), %%xmm2      \n\t"
                          "mulps      %%xmm7, %%xmm1      \n\t"
                          "movups     32(%1), %%xmm3      \n\t"
                          "mulps      %%xmm7, %%xmm2      \n\t"
                          "cvtps2dq   %%xmm1, %%xmm1      \n\t"
                          "movups     48(%1), %%xmm4      \n\t"
                          "mulps      %%xmm7, %%xmm3      \n\t"
                          "cvtps2dq   %%xmm2, %%xmm2      \n\t"
                          "mulps      %%xmm7, %%xmm4      \n\t"
                          "cvtps2dq   %%xmm3, %%xmm3      \n\t"
                          "cvtps2dq   %%xmm4, %%xmm4      \n\t"
                          "packssdw   %%xmm2, %%xmm1      \n\t"
                          "packssdw   %%xmm4, %%xmm3      \n\t"
                          "add        $64,    %1          \n\t"
                          "movdqu     %%xmm1, (%0)        \n\t"
                          "movdqu     %%xmm3, 16(%0)      \n\t"
                          "add        $32,    %0          \n\t"
                          "sub        $1, %%ecx           \n\t"
                          "jnz        1b                  \n\t"
                          :"+r"(out),"+r"(in)
                          :"c"(loops), "r"(f)
                          );
    }
#endif //ARCH_x86
    for (;i < len;i++)
        *out++ = clip_short(lrintf(*in++ * f));
    return len << 1;
}

static int toFloat32(AudioFormat format, float* out, const int* in, int len)
{
    int i = 0;
    int bits = AudioOutputSettings::FormatToBits(format);
    float f = 1.0f / ((uint)(1<<(bits-1)));
    int shift = 32 - bits;

    if (format == FORMAT_S24LSB)
        shift = 0;

#if ARCH_X86
    if (sse_check() && len >= 16)
    {
        int loops = len >> 4;
        i = loops << 4;

        __asm__ volatile (
                          "movd       %3, %%xmm7          \n\t"
                          "punpckldq  %%xmm7, %%xmm7      \n\t"
                          "movd       %4, %%xmm6          \n\t"
                          "punpckldq  %%xmm7, %%xmm7      \n\t"
                          "1:                             \n\t"
                          "movdqu     (%1),   %%xmm1      \n\t"
                          "movdqu     16(%1), %%xmm2      \n\t"
                          "psrad      %%xmm6, %%xmm1      \n\t"
                          "movdqu     32(%1), %%xmm3      \n\t"
                          "cvtdq2ps   %%xmm1, %%xmm1      \n\t"
                          "psrad      %%xmm6, %%xmm2      \n\t"
                          "movdqu     48(%1), %%xmm4      \n\t"
                          "cvtdq2ps   %%xmm2, %%xmm2      \n\t"
                          "psrad      %%xmm6, %%xmm3      \n\t"
                          "mulps      %%xmm7, %%xmm1      \n\t"
                          "psrad      %%xmm6, %%xmm4      \n\t"
                          "cvtdq2ps   %%xmm3, %%xmm3      \n\t"
                          "movups     %%xmm1, (%0)        \n\t"
                          "mulps      %%xmm7, %%xmm2      \n\t"
                          "cvtdq2ps   %%xmm4, %%xmm4      \n\t"
                          "movups     %%xmm2, 16(%0)      \n\t"
                          "mulps      %%xmm7, %%xmm3      \n\t"
                          "mulps      %%xmm7, %%xmm4      \n\t"
                          "movups     %%xmm3, 32(%0)      \n\t"
                          "add        $64,    %1          \n\t"
                          "movups     %%xmm4, 48(%0)      \n\t"
                          "add        $64,    %0          \n\t"
                          "sub        $1, %%ecx           \n\t"
                          "jnz        1b                  \n\t"
                          :"+r"(out),"+r"(in)
                          :"c"(loops), "r"(f), "r"(shift)
                          );
    }
#endif //ARCH_x86
    for (; i < len; i++)
        *out++ = (*in++ >> shift) * f;
    return len << 2;
}

static int fromFloat32(AudioFormat format, int* out, const float* in, int len)
{
    int i = 0;
    int bits = AudioOutputSettings::FormatToBits(format);
    float f = (uint)(1<<(bits-1));
    int shift = 32 - bits;

    if (format == FORMAT_S24LSB)
        shift = 0;

#if ARCH_X86
    if (sse_check() && len >= 16)
    {
        float o = 0.99999995, mo = -1;
        int loops = len >> 4;
        i = loops << 4;

        __asm__ volatile (
                          "movd       %3, %%xmm7          \n\t"
                          "movss      %4, %%xmm5          \n\t"
                          "punpckldq  %%xmm7, %%xmm7      \n\t"
                          "movss      %5, %%xmm6          \n\t"
                          "punpckldq  %%xmm5, %%xmm5      \n\t"
                          "punpckldq  %%xmm6, %%xmm6      \n\t"
                          "movd       %6, %%xmm0          \n\t"
                          "punpckldq  %%xmm7, %%xmm7      \n\t"
                          "punpckldq  %%xmm5, %%xmm5      \n\t"
                          "punpckldq  %%xmm6, %%xmm6      \n\t"
                          "1:                             \n\t"
                          "movups     (%1), %%xmm1        \n\t"
                          "movups     16(%1), %%xmm2      \n\t"
                          "minps      %%xmm5, %%xmm1      \n\t"
                          "movups     32(%1), %%xmm3      \n\t"
                          "maxps      %%xmm6, %%xmm1      \n\t"
                          "movups     48(%1), %%xmm4      \n\t"
                          "mulps      %%xmm7, %%xmm1      \n\t"
                          "minps      %%xmm5, %%xmm2      \n\t"
                          "cvtps2dq   %%xmm1, %%xmm1      \n\t"
                          "maxps      %%xmm6, %%xmm2      \n\t"
                          "pslld      %%xmm0, %%xmm1      \n\t"
                          "minps      %%xmm5, %%xmm3      \n\t"
                          "mulps      %%xmm7, %%xmm2      \n\t"
                          "movdqu     %%xmm1, (%0)        \n\t"
                          "cvtps2dq   %%xmm2, %%xmm2      \n\t"
                          "maxps      %%xmm6, %%xmm3      \n\t"
                          "minps      %%xmm5, %%xmm4      \n\t"
                          "pslld      %%xmm0, %%xmm2      \n\t"
                          "mulps      %%xmm7, %%xmm3      \n\t"
                          "maxps      %%xmm6, %%xmm4      \n\t"
                          "movdqu     %%xmm2, 16(%0)      \n\t"
                          "cvtps2dq   %%xmm3, %%xmm3      \n\t"
                          "mulps      %%xmm7, %%xmm4      \n\t"
                          "pslld      %%xmm0, %%xmm3      \n\t"
                          "cvtps2dq   %%xmm4, %%xmm4      \n\t"
                          "movdqu     %%xmm3, 32(%0)      \n\t"
                          "pslld      %%xmm0, %%xmm4      \n\t"
                          "add        $64,    %1          \n\t"
                          "movdqu     %%xmm4, 48(%0)      \n\t"
                          "add        $64,    %0          \n\t"
                          "sub        $1, %%ecx           \n\t"
                          "jnz        1b                  \n\t"
                          :"+r"(out), "+r"(in)
                          :"c"(loops), "r"(f), "m"(o), "m"(mo), "r"(shift)
                          );
    }
#endif //ARCH_x86
    uint range = 1<<(bits-1);
    for (; i < len; i++)
    {
        float valf = *in++;

        if (valf >= 1.0f)
        {
            *out++ = (range - 128) << shift;
            continue;
        }
        if (valf <= -1.0f)
        {
            *out++ = (-range) << shift;
            continue;
        }
        *out++ = lrintf(valf * f) << shift;
    }
    return len << 2;
}

static int fromFloatFLT(float* out, const float* in, int len)
{
    int i = 0;

#if ARCH_X86
    if (sse_check() && len >= 16)
    {
        int loops = len >> 4;
        float o = 1, mo = -1;
        i = loops << 4;

        __asm__ volatile (
                          "movss      %3, %%xmm6          \n\t"
                          "movss      %4, %%xmm7          \n\t"
                          "punpckldq  %%xmm6, %%xmm6      \n\t"
                          "punpckldq  %%xmm7, %%xmm7      \n\t"
                          "punpckldq  %%xmm6, %%xmm6      \n\t"
                          "punpckldq  %%xmm7, %%xmm7      \n\t"
                          "1:                             \n\t"
                          "movups     (%1), %%xmm1        \n\t"
                          "movups     16(%1), %%xmm2      \n\t"
                          "minps      %%xmm6, %%xmm1      \n\t"
                          "movups     32(%1), %%xmm3      \n\t"
                          "maxps      %%xmm7, %%xmm1      \n\t"
                          "minps      %%xmm6, %%xmm2      \n\t"
                          "movups     48(%1), %%xmm4      \n\t"
                          "maxps      %%xmm7, %%xmm2      \n\t"
                          "movups     %%xmm1, (%0)        \n\t"
                          "minps      %%xmm6, %%xmm3      \n\t"
                          "movups     %%xmm2, 16(%0)      \n\t"
                          "maxps      %%xmm7, %%xmm3      \n\t"
                          "minps      %%xmm6, %%xmm4      \n\t"
                          "movups     %%xmm3, 32(%0)      \n\t"
                          "maxps      %%xmm7, %%xmm4      \n\t"
                          "add        $64,    %1          \n\t"
                          "movups     %%xmm4, 48(%0)      \n\t"
                          "add        $64,    %0          \n\t"
                          "sub        $1, %%ecx           \n\t"
                          "jnz        1b                  \n\t"
                          :"+r"(out), "+r"(in)
                          :"c"(loops), "m"(o), "m"(mo)
                          );
    }
#endif //ARCH_x86
    for (;i < len;i++)
        *out++ = clipcheck(*in++);
    return len << 2;
}

/**
 * Convert integer samples to floats
 *
 * Consumes 'bytes' bytes from in and returns the numer of bytes written to out
 */
int AudioConvert::toFloat(AudioFormat format, void* out, const void* in,
                             int bytes)
{
    if (bytes <= 0)
        return 0;

    switch (format)
    {
        case FORMAT_U8:
            return toFloat8((float*)out,  (uchar*)in, bytes);
        case FORMAT_S16:
            return toFloat16((float*)out, (short*)in, bytes >> 1);
        case FORMAT_S24:
        case FORMAT_S24LSB:
        case FORMAT_S32:
            return toFloat32(format, (float*)out, (int*)in, bytes >> 2);
        case FORMAT_FLT:
            memcpy(out, in, bytes);
            return bytes;
        case FORMAT_NONE:
        default:
            return 0;
    }
}

/**
 * Convert float samples to integers
 *
 * Consumes 'bytes' bytes from in and returns the numer of bytes written to out
 */
int AudioConvert::fromFloat(AudioFormat format, void* out, const void* in,
                               int bytes)
{
    if (bytes <= 0)
        return 0;

    switch (format)
    {
        case FORMAT_U8:
            return fromFloat8((uchar*)out, (float*)in, bytes >> 2);
        case FORMAT_S16:
            return fromFloat16((short*)out, (float*)in, bytes >> 2);
        case FORMAT_S24:
        case FORMAT_S24LSB:
        case FORMAT_S32:
            return fromFloat32(format, (int*)out, (float*)in, bytes >> 2);
        case FORMAT_FLT:
            return fromFloatFLT((float*)out, (float*)in, bytes >> 2);
        case FORMAT_NONE:
        default:
            return 0;
    }
}

/////// AudioConvert Class

class AudioConvertInternal
{
public:
    AudioConvertInternal(AVSampleFormat in, AVSampleFormat out) :
    m_in(in), m_out(out)
    {
        char error[AV_ERROR_MAX_STRING_SIZE];
        m_swr = swr_alloc_set_opts(NULL,
                                   av_get_default_channel_layout(1),
                                   m_out,
                                   48000,
                                   av_get_default_channel_layout(1),
                                   m_in,
                                   48000,
                                   0, NULL);
        if (!m_swr)
        {
            LOG(VB_AUDIO, LOG_ERR, LOC + "error allocating resampler context");
            return;
        }
        /* initialize the resampling context */
        int ret = swr_init(m_swr);
        if (ret < 0)
        {
            LOG(VB_AUDIO, LOG_ERR, LOC +
                QString("error initializing resampler context (%1)")
                .arg(av_make_error_string(error, sizeof(error), ret)));
            swr_free(&m_swr);
            return;
        }
    }
    int Process(void* out, const void* in, int bytes)
    {
        if (!m_swr)
            return -1;

        uint8_t* outp[] = {(uint8_t*)out};
        const uint8_t* inp[]  = {(const uint8_t*)in};
        int samples = bytes / av_get_bytes_per_sample(m_in);
        int ret = swr_convert(m_swr,
                              outp, samples,
                              inp, samples);
        if (ret < 0)
            return ret;
        return ret * av_get_bytes_per_sample(m_out);
    }
    ~AudioConvertInternal()
    {
        if (m_swr)
        {
            swr_free(&m_swr);
        }
    }

    SwrContext* m_swr;
    AVSampleFormat m_in, m_out;
};


AudioConvert::AudioConvert(AudioFormat in, AudioFormat out) :
    m_ctx(NULL), m_in(in), m_out(out)
{
}

AudioConvert::~AudioConvert()
{
    delete m_ctx;
    m_ctx = NULL;
}

/**
 * Convert samples from one format to another
 *
 * Consumes 'bytes' bytes from in and returns the numer of bytes written to out
 * return negative number if error
 */
int AudioConvert::Process(void* out, const void* in, int bytes, bool noclip)
{
    if (bytes <= 0)
        return 0;
    if (m_out == FORMAT_NONE || m_in == FORMAT_NONE)
        return 0;

    if (noclip && m_in == m_out)
    {
        memcpy(out, in, bytes);
        return bytes;
    }

    /* use conversion routines to perform clipping on samples */
    if (m_in == FORMAT_FLT)
        return fromFloat(m_out, out, in, bytes);
    if (m_out == FORMAT_FLT)
        return toFloat(m_in, out, in, bytes);

    if (m_in == m_out)
    {
        memcpy(out, in, bytes);
        return bytes;
    }

    if (m_in  == FORMAT_S24 || m_in  == FORMAT_S24LSB ||
        m_out == FORMAT_S24 || m_out == FORMAT_S24LSB)
    {
        // FFmpeg can't handle those, so use float conversion intermediary
        if (AudioOutputSettings::SampleSize(m_out) == AudioOutputSettings::SampleSize(FORMAT_FLT))
        {
            // this can be done in place
            int s = toFloat(m_in, out, in, bytes);
            return fromFloat(m_out, out, out, s);
        }
        // this leave S24 -> U8/S16.
        // TODO: native handling of those ; use internal temp buffer in the mean time

        uint8_t     buffer[65536+15];
        uint8_t*    tmp = (uint8_t*)(((long)buffer + 15) & ~0xf);
        int left        = bytes;

        while (left > 0)
        {
            int s;

            if (left >= 65536)
            {
                s       = toFloat(m_in, tmp, in, 65536);
                in      = (void*)((long)in + s);
                out     = (void*)((long)out + fromFloat(m_out, out, tmp, s));
                left   -= 65536;
                continue;
            }
            s       = toFloat(m_in, tmp, in, left);
            in      = (void*)((long)in + s);
            out     = (void*)((long)out + fromFloat(m_out, out, tmp, s));
            left    = 0;
        }
        return bytes * AudioOutputSettings::SampleSize(m_out) / AudioOutputSettings::SampleSize(m_in);
    }

    // use FFmpeg conversion routine for S32<->S16, S32<->U8 and S16<->U8
    if (!m_ctx)
    {
        m_ctx = new AudioConvertInternal(AudioOutputSettings::FormatToAVSampleFormat(m_in),
                                         AudioOutputSettings::FormatToAVSampleFormat(m_out));
    }

    return m_ctx->Process(out, in, bytes);
}

/**
 * Convert a mono stream to stereo by copying and interleaving samples
 */
void AudioConvert::MonoToStereo(void* dst, const void* src, int samples)
{
    float* d = (float*)dst;
    float* s = (float*)src;
    for (int i = 0; i < samples; i++)
    {
        *d++ = *s;
        *d++ = *s++;
    }
}

template <class AudioDataType>
void _DeinterleaveSample(AudioDataType* out, const AudioDataType* in, int channels, int frames)
{
    AudioDataType* outp[8];

    for (int i = 0; i < channels; i++)
    {
        outp[i] = out + (i * frames);
    }

    for (int i = 0; i < frames; i++)
    {
        for (int j = 0; j < channels; j++)
        {
            *(outp[j]++) = *(in++);
        }
    }
}

/**
 * Deinterleave input samples
 * Deinterleave audio samples and compact them
 */
void AudioConvert::DeinterleaveSamples(AudioFormat format, int channels,
                                          uint8_t* output, const uint8_t* input,
                                          int data_size)
{
    if (channels == 1)
    {
        // If channel count is 1, planar and non-planar formats are the same
        memcpy(output, input, data_size);
        return;
    }

    int bits = AudioOutputSettings::FormatToBits(format);
    if (bits == 8)
    {
        _DeinterleaveSample((char*)output, (const char*)input, channels, data_size/sizeof(char)/channels);
    }
    else if (bits == 16)
    {
        _DeinterleaveSample((short*)output, (const short*)input, channels, data_size/sizeof(short)/channels);
    }
    else
    {
        _DeinterleaveSample((int*)output, (const int*)input, channels, data_size/sizeof(int)/channels);
    }
}

template <class AudioDataType>
void _InterleaveSample(AudioDataType* out, const AudioDataType* in, int channels, int frames,
                       const AudioDataType*  const* inp = NULL)
{
    const AudioDataType* my_inp[8];

    if (channels == 1)
    {
        //special case for mono
        memcpy(out, inp ? inp[0] : in, sizeof(AudioDataType) * frames);
        return;
    }

    if (!inp)
    {
        // We're given an array of int, calculate pointers to each row
        for (int i = 0; i < channels; i++)
        {
            my_inp[i] = in + (i * frames);
        }
    }
    else
    {
        for (int i = 0; i < channels; i++)
        {
            my_inp[i] = inp[i];
        }
    }

    for (int i = 0; i < frames; i++)
    {
        for (int j = 0; j < channels; j++)
        {
            *(out++) = *(my_inp[j]++);
        }
    }
}

/**
 * Interleave input samples
 * Planar audio is contained in array of pointers
 * Interleave audio samples (convert from planar format)
 */
void AudioConvert::InterleaveSamples(AudioFormat format, int channels,
                                        uint8_t* output, const uint8_t*  const* input,
                                        int data_size)
{
    int bits = AudioOutputSettings::FormatToBits(format);
    if (bits == 8)
    {
        _InterleaveSample((char*)output, (const char*)NULL, channels, data_size/sizeof(char)/channels,
                          (const char*  const*)input);
    }
    else if (bits == 16)
    {
        _InterleaveSample((short*)output, (const short*)NULL, channels, data_size/sizeof(short)/channels,
                          (const short*  const*)input);
    }
    else
    {
        _InterleaveSample((int*)output, (const int*)NULL, channels, data_size/sizeof(int)/channels,
                          (const int*  const*)input);
    }
}

/**
 * Interleave input samples
 * Interleave audio samples (convert from planar format)
 */
void AudioConvert::InterleaveSamples(AudioFormat format, int channels,
                                        uint8_t* output, const uint8_t* input,
                                        int data_size)
{
    int bits = AudioOutputSettings::FormatToBits(format);
    if (bits == 8)
    {
        _InterleaveSample((char*)output, (const char*)input, channels, data_size/sizeof(char)/channels);
    }
    else if (bits == 16)
    {
        _InterleaveSample((short*)output, (const short*)input, channels, data_size/sizeof(short)/channels);
    }
    else
    {
        _InterleaveSample((int*)output, (const int*)input, channels, data_size/sizeof(int)/channels);
    }
}

void AudioConvert::DeinterleaveSamples(int channels,
                                       uint8_t* output, const uint8_t* input,
                                       int data_size)
{
    DeinterleaveSamples(m_in, channels, output, input, data_size);
}

void AudioConvert::InterleaveSamples(int channels,
                                     uint8_t* output, const uint8_t*  const* input,
                                     int data_size)
{
    InterleaveSamples(m_in, channels, output, input, data_size);
}

void AudioConvert::InterleaveSamples(int channels,
                                     uint8_t* output, const uint8_t* input,
                                     int data_size)
{
    InterleaveSamples(m_in, channels, output, input, data_size);
}
