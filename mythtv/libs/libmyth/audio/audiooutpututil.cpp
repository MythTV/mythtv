#include <math.h>

#include "mythconfig.h"
#include "audiooutpututil.h"
#include <sys/types.h>
#include <inttypes.h>
#if HAVE_BYTESWAP_H
#include <byteswap.h>
#elif HAVE_SYS_ENDIAN_H
#include <sys/endian.h>
#elif CONFIG_DARWIN
#include <libkern/OSByteOrder.h>
#define bswap_16 OSSwapInt16
#define bswap_32 OSSwapInt32
#elif HAVE_BIGENDIAN
#error No bswap functions defined
#endif

#define LOC QString("AOUtil: ")

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

static inline float clipcheck(float f) {
    if (f > 1.0f) f = 1.0f;
    else if (f < -1.0f) f = -1.0f;
    return f;
}

/*
 All toFloat variants require 16 byte aligned input and output buffers on x86
 The SSE code processes 16 bytes at a time and leaves any remainder for the C
 - there is no remainder in practice */

static int toFloat8(float *out, uchar *in, int len)
{
    int i = 0;
    float f = 1.0f / ((1<<7) - 1);

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
            "movdqa     (%1), %%xmm1        \n\t"
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
            "movaps     %%xmm4, (%0)        \n\t"
            "cvtdq2ps   %%xmm1, %%xmm1      \n\t"
            "mulps      %%xmm7, %%xmm6      \n\t"
            "movaps     %%xmm5, 16(%0)      \n\t"
            "mulps      %%xmm7, %%xmm1      \n\t"
            "movaps     %%xmm6, 32(%0)      \n\t"
            "add        $16,    %1          \n\t"
            "movaps     %%xmm1, 48(%0)      \n\t"
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

static int fromFloat8(uchar *out, float *in, int len)
{
    int i = 0;
    float f = (1<<7) - 1;

#if ARCH_X86
    if (sse_check() && len >= 16 && ((unsigned long)out & 0xf) == 0)
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
        *out++ = lrintf(clipcheck(*in++) * f) + 0x80;
    return len;
}

static int toFloat16(float *out, short *in, int len)
{
    int i = 0;
    float f = 1.0f / ((1<<15) - 1);

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
            "movdqa     (%1),   %%xmm1      \n\t"
            "xorpd      %%xmm3, %%xmm3      \n\t"
            "punpcklwd  %%xmm1, %%xmm2      \n\t"
            "movdqa     16(%1), %%xmm4      \n\t"
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
            "movaps     %%xmm2, (%0)        \n\t"
            "cvtdq2ps   %%xmm6, %%xmm6      \n\t"
            "mulps      %%xmm7, %%xmm5      \n\t"
            "movaps     %%xmm3, 16(%0)      \n\t"
            "mulps      %%xmm7, %%xmm6      \n\t"
            "movaps     %%xmm5, 32(%0)      \n\t"
            "add        $32, %1             \n\t"
            "movaps     %%xmm6, 48(%0)      \n\t"
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

static int fromFloat16(short *out, float *in, int len)
{
    int i = 0;
    float f = (1<<15) - 1;

#if ARCH_X86
    if (sse_check() && len >= 16 && ((unsigned long)out & 0xf) == 0)
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
        *out++ = lrintf(clipcheck(*in++) * f);
    return len << 1;
}

static int toFloat32(AudioFormat format, float *out, int *in, int len)
{
    int i = 0;
    int bits = AudioOutputSettings::FormatToBits(format);
    float f = 1.0f / ((uint)(1<<(bits-1)) - 128);
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
            "movdqa     (%1),   %%xmm1      \n\t"
            "movdqa     16(%1), %%xmm2      \n\t"
            "psrad      %%xmm6, %%xmm1      \n\t"
            "movdqa     32(%1), %%xmm3      \n\t"
            "cvtdq2ps   %%xmm1, %%xmm1      \n\t"
            "psrad      %%xmm6, %%xmm2      \n\t"
            "movdqa     48(%1), %%xmm4      \n\t"
            "cvtdq2ps   %%xmm2, %%xmm2      \n\t"
            "psrad      %%xmm6, %%xmm3      \n\t"
            "mulps      %%xmm7, %%xmm1      \n\t"
            "psrad      %%xmm6, %%xmm4      \n\t"
            "cvtdq2ps   %%xmm3, %%xmm3      \n\t"
            "movaps     %%xmm1, (%0)        \n\t"
            "mulps      %%xmm7, %%xmm2      \n\t"
            "cvtdq2ps   %%xmm4, %%xmm4      \n\t"
            "movaps     %%xmm2, 16(%0)      \n\t"
            "mulps      %%xmm7, %%xmm3      \n\t"
            "mulps      %%xmm7, %%xmm4      \n\t"
            "movaps     %%xmm3, 32(%0)      \n\t"
            "add        $64,    %1          \n\t"
            "movaps     %%xmm4, 48(%0)      \n\t"
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

static int fromFloat32(AudioFormat format, int *out, float *in, int len)
{
    int i = 0;
    int bits = AudioOutputSettings::FormatToBits(format);
    float f = (uint)(1<<(bits-1)) - 128;
    int shift = 32 - bits;

    if (format == FORMAT_S24LSB)
        shift = 0;

#if ARCH_X86
    if (sse_check() && len >= 16 && ((unsigned long)out & 0xf) == 0)
    {
        float o = 1, mo = -1;
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
    for (;i < len;i++)
        *out++ = lrintf(clipcheck(*in++) * f) << shift;
    return len << 2;
}

static int fromFloatFLT(float *out, float *in, int len)
{
    int i = 0;

#if ARCH_X86
    if (sse_check() && len >= 16 && ((unsigned long)in & 0xf) == 0)
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
 * Returns true if platform has an FPU.
 * for the time being, this test is limited to testing if SSE2 is supported
 */
bool AudioOutputUtil::has_hardware_fpu()
{
#if ARCH_X86
    return sse_check();
#else
    return false;
#endif
}

/**
 * Convert integer samples to floats
 *
 * Consumes 'bytes' bytes from in and returns the numer of bytes written to out
 */
int AudioOutputUtil::toFloat(AudioFormat format, void *out, void *in,
                             int bytes)
{
    if (bytes <= 0)
        return 0;

    switch (format)
    {
        case FORMAT_U8:
            return toFloat8((float *)out,  (uchar *)in, bytes);
        case FORMAT_S16:
            return toFloat16((float *)out, (short *)in, bytes >> 1);
        case FORMAT_S24:
        case FORMAT_S24LSB:
        case FORMAT_S32:
            return toFloat32(format, (float *)out, (int *)in, bytes >> 2);
        case FORMAT_FLT:
            memcpy(out, in, bytes);
            return bytes;
    }

    return 0;
}

/**
 * Convert float samples to integers
 *
 * Consumes 'bytes' bytes from in and returns the numer of bytes written to out
 */
int AudioOutputUtil::fromFloat(AudioFormat format, void *out, void *in,
                               int bytes)
{
    if (bytes <= 0)
        return 0;

    switch (format)
    {
        case FORMAT_U8:
            return fromFloat8((uchar *)out, (float *)in, bytes >> 2);
        case FORMAT_S16:
            return fromFloat16((short *)out, (float *)in, bytes >> 2);
        case FORMAT_S24:
        case FORMAT_S24LSB:
        case FORMAT_S32:
            return fromFloat32(format, (int *)out, (float *)in, bytes >> 2);
        case FORMAT_FLT:
            return fromFloatFLT((float *)out, (float *)in, bytes >> 2);
    }

    return 0;
}

/**
 * Convert a mono stream to stereo by copying and interleaving samples
 */
void AudioOutputUtil::MonoToStereo(void *dst, void *src, int samples)
{
    float *d = (float *)dst;
    float *s = (float *)src;
    for (int i = 0; i < samples; i++)
    {
        *d++ = *s;
        *d++ = *s++;
    }
}

/**
 * Adjust the volume of samples
 *
 * Makes a crude attempt to normalise the relative volumes of
 * PCM from mythmusic, PCM from video and upmixed AC-3
 */
void AudioOutputUtil::AdjustVolume(void *buf, int len, int volume,
                                   bool music, bool upmix)
{
    float g     = volume / 100.0f;
    float *fptr = (float *)buf;
    int samples = len >> 2;
    int i       = 0;

    // Should be exponential - this'll do
    g *= g;

    // Try to ~ match stereo volume when upmixing
    if (upmix)
        g *= 1.5f;

    // Music is relatively loud
    if (music)
        g *= 0.4f;

    if (g == 1.0f)
        return;

#if ARCH_X86
    if (sse_check() && samples >= 16)
    {
        int loops = samples >> 4;
        i = loops << 4;

        __asm__ volatile (
            "movss      %2, %%xmm0          \n\t"
            "punpckldq  %%xmm0, %%xmm0      \n\t"
            "punpckldq  %%xmm0, %%xmm0      \n\t"
            "1:                             \n\t"
            "movups     (%0), %%xmm1        \n\t"
            "movups     16(%0), %%xmm2      \n\t"
            "mulps      %%xmm0, %%xmm1      \n\t"
            "movups     32(%0), %%xmm3      \n\t"
            "mulps      %%xmm0, %%xmm2      \n\t"
            "movups     48(%0), %%xmm4      \n\t"
            "mulps      %%xmm0, %%xmm3      \n\t"
            "movups     %%xmm1, (%0)        \n\t"
            "mulps      %%xmm0, %%xmm4      \n\t"
            "movups     %%xmm2, 16(%0)      \n\t"
            "movups     %%xmm3, 32(%0)      \n\t"
            "movups     %%xmm4, 48(%0)      \n\t"
            "add        $64,    %0          \n\t"
            "sub        $1, %%ecx           \n\t"
            "jnz        1b                  \n\t"
            :"+r"(fptr)
            :"c"(loops),"m"(g)
        );
    }
#endif //ARCH_X86
    for (; i < samples; i++)
        *fptr++ *= g;
}

template <class AudioDataType>
void _MuteChannel(AudioDataType *buffer, int channels, int ch, int frames)
{
    AudioDataType *s1 = buffer + ch;
    AudioDataType *s2 = buffer - ch + 1;

    for (int i = 0; i < frames; i++)
    {
        *s1 = *s2;
        s1 += channels;
        s2 += channels;
    }
}

/**
 * Mute individual channels through mono->stereo duplication
 *
 * Mute given channel (left or right) by copying right or left
 * channel over.
 */
void AudioOutputUtil::MuteChannel(int obits, int channels, int ch,
                                  void *buffer, int bytes)
{
    int frames = bytes / ((obits >> 3) * channels);

    if (obits == 8)
        _MuteChannel((uchar *)buffer, channels, ch, frames);
    else if (obits == 16)
        _MuteChannel((short *)buffer, channels, ch, frames);
    else
        _MuteChannel((int *)buffer, channels, ch, frames);
}

#if HAVE_BIGENDIAN
#define LE_SHORT(v)      bswap_16(v)
#define LE_INT(v)        bswap_32(v)
#else
#define LE_SHORT(v)      (v)
#define LE_INT(v)        (v)
#endif

char *AudioOutputUtil::GeneratePinkFrames(char *frames, int channels,
                                          int channel, int count, int bits)
{
    pink_noise_t pink;

    initialize_pink_noise(&pink, bits);

    double   res;
    int32_t  ires;
    int16_t *samp16 = (int16_t*) frames;
    int32_t *samp32 = (int32_t*) frames;

    while (count-- > 0)
    {
        for(int chn = 0 ; chn < channels; chn++)
        {
            if (chn==channel)
            {
                res = generate_pink_noise_sample(&pink) * 0x03fffffff; /* Don't use MAX volume */
                ires = res;
                if (bits == 16)
                    *samp16++ = LE_SHORT(ires >> 16);
                else
                    *samp32++ = LE_INT(ires);
            }
            else
            {
                if (bits == 16)
                    *samp16++ = 0;
                else
                    *samp32++ = 0;
            }
        }
    }    
    return frames;
}
  
