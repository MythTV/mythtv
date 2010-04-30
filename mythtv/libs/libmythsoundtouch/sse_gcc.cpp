// SSE2 version of the expensive routines for 16 bit integer samples

#include "STTypes.h"
#include "TDStretch.h"
#include "inttypes.h"

using namespace soundtouch;

static uint64_t ones = 0x0001ffff0001ffffULL;
static uint64_t sadd = 0x000001ff000001ffULL;

long TDStretchSSE2::calcCrossCorrMulti(const short *mPos, const short *cPos) const
{
    long corr = 0;
    // Need 16-byte align for int out[4], but gcc bug #16660 prevents use of
    //  attribute(aligned()), so align it ourselves. Fixed in gcc >4.4 r138335
    int i, x[8]; 
    int *out=(int *)(((uintptr_t)&x+15) & ~(uintptr_t)0xf);
    int count = (overlapLength * channels) - channels;
    long loops = count >> 5;
    long remainder = count - (loops<<5);
    const short *mp = mPos;
    const short *cp = cPos;

    mp += channels;
    cp += channels;

    __asm__ volatile (
        "xorps      %%xmm5, %%xmm5      \n\t"
        "movd       %4, %%xmm7          \n\t"
        "1:                             \n\t"
        "movupd     (%1), %%xmm0        \n\t"
        "movupd     (%2), %%xmm1        \n\t"
        "movupd     16(%1), %%xmm2      \n\t"
        "pmaddwd    %%xmm0, %%xmm1      \n\t"
        "movupd     32(%1), %%xmm4      \n\t"
        "movupd     48(%1), %%xmm6      \n\t"
        "psrad      %%xmm7, %%xmm1      \n\t"
        "movupd     16(%2), %%xmm3      \n\t"
        "paddd      %%xmm1, %%xmm5      \n\t"
        "movupd     32(%2), %%xmm0      \n\t"
        "pmaddwd    %%xmm2, %%xmm3      \n\t"
        "movupd     48(%2), %%xmm1      \n\t"
        "pmaddwd    %%xmm4, %%xmm0      \n\t"
        "psrad      %%xmm7, %%xmm3      \n\t"
        "pmaddwd    %%xmm6, %%xmm1      \n\t"
        "psrad      %%xmm7, %%xmm0      \n\t"
        "paddd      %%xmm3, %%xmm5      \n\t"
        "psrad      %%xmm7, %%xmm1      \n\t"
        "paddd      %%xmm0, %%xmm5      \n\t"
        "add        $64, %1             \n\t"
        "paddd      %%xmm1, %%xmm5      \n\t"
        "add        $64, %2             \n\t"
        "sub        $1, %%ecx           \n\t"
        "jnz        1b                  \n\t"
        "movdqa     %%xmm5, %0          \n\t"
        :"=m"(out[0]),"+r"(mp), "+r"(cp)
        :"c"(loops), "m"(overlapDividerBits)
    );

    corr = out[0] + out[1] + out[2] + out[3];

    for (i = 0; i < remainder; i++)
        corr += (mPos[i] * cPos[i]) >> overlapDividerBits;

    return corr;
}

long TDStretchSSE2::calcCrossCorrStereo(const short *mPos, const short *cPos) const
{
    long corr = 0;
    // Need 16-byte align for int out[4], but gcc bug #16660 prevents use of
    //  attribute(aligned()), so align it ourselves. Fixed in gcc >4.4 r138335
    int i, x[8]; 
    int *out=(int *)(((uintptr_t)&x+15) & ~(uintptr_t)0xf);
    int count = (overlapLength<<1) - 2;
    long loops = count >> 5;
    long remainder = count - (loops<<5);
    const short *mp = mPos;
    const short *cp = cPos;

    mp += 2;
    cp += 2;

    __asm__ volatile (
        "xorps      %%xmm5, %%xmm5      \n\t"
        "movd       %4, %%xmm7          \n\t"
        "1:                             \n\t"
        "movupd     (%1), %%xmm0        \n\t"
        "movupd     (%2), %%xmm1        \n\t"
        "movupd     16(%1), %%xmm2      \n\t"
        "pmaddwd    %%xmm0, %%xmm1      \n\t"
        "movupd     32(%1), %%xmm4      \n\t"
        "movupd     48(%1), %%xmm6      \n\t"
        "psrad      %%xmm7, %%xmm1      \n\t"
        "movupd     16(%2), %%xmm3      \n\t"
        "paddd      %%xmm1, %%xmm5      \n\t"
        "movupd     32(%2), %%xmm0      \n\t"
        "pmaddwd    %%xmm2, %%xmm3      \n\t"
        "movupd     48(%2), %%xmm1      \n\t"
        "pmaddwd    %%xmm4, %%xmm0      \n\t"
        "psrad      %%xmm7, %%xmm3      \n\t"
        "pmaddwd    %%xmm6, %%xmm1      \n\t"
        "psrad      %%xmm7, %%xmm0      \n\t"
        "paddd      %%xmm3, %%xmm5      \n\t"
        "psrad      %%xmm7, %%xmm1      \n\t"
        "paddd      %%xmm0, %%xmm5      \n\t"
        "add        $64, %1             \n\t"
        "paddd      %%xmm1, %%xmm5      \n\t"
        "add        $64, %2             \n\t"
        "sub        $1, %%ecx           \n\t"
        "jnz        1b                  \n\t"
        "movdqa     %%xmm5, %0          \n\t"
        :"=m"(out[0]),"+r"(mp),"+r"(cp)
        :"c"(loops), "m"(overlapDividerBits)
    );

    corr = out[0] + out[1] + out[2] + out[3];

    for (i = 0; i < remainder; i += 2)
        corr += (mPos[i] * cPos[i] +
                 mPos[i+1] * cPos[i+1]) >> overlapDividerBits;

    return corr;
}

void TDStretchSSE2::overlapMulti(short *output, const short *input) const
{

    short *o = output;
    const short *i = input;
    const short *m = pMidBuffer;
    long ch = (long)channels;

    __asm__ volatile (
        "movd       %%ecx, %%xmm0       \n\t"
        "shl        %6                  \n\t"
        "punpckldq  %%xmm0, %%xmm0      \n\t"
        "movq       %2, %%xmm2          \n\t"
        "punpckldq  %%xmm0, %%xmm0      \n\t"
        "movq       %1, %%xmm1          \n\t"
        "punpckldq  %%xmm2, %%xmm2      \n\t"
        "xorpd      %%xmm7, %%xmm7      \n\t"
        "punpckldq  %%xmm1, %%xmm1      \n\t"
        "1:                             \n\t"
        "movdqu     (%3), %%xmm3        \n\t"
        "movdqu     (%4), %%xmm4        \n\t"
        "movdqa     %%xmm4, %%xmm5      \n\t"
        "punpcklwd  %%xmm3, %%xmm4      \n\t"
        "add        %6, %3              \n\t"
        "punpckhwd  %%xmm3, %%xmm5      \n\t"
        "pmaddwd    %%xmm0, %%xmm4      \n\t"
        "movdqa     %%xmm7, %%xmm3      \n\t"
        "pcmpgtd    %%xmm4, %%xmm3      \n\t"
        "pand       %%xmm1, %%xmm3      \n\t"
        "add        %6, %4              \n\t"
        "paddd      %%xmm3, %%xmm4      \n\t"
        "pmaddwd    %%xmm0, %%xmm5      \n\t"
        "movdqa     %%xmm7, %%xmm3      \n\t"
        "pcmpgtd    %%xmm5, %%xmm3      \n\t"
        "pand       %%xmm1, %%xmm3      \n\t"
        "psrad      $9, %%xmm4          \n\t"
        "paddd      %%xmm3, %%xmm5      \n\t"
        "psrad      $9, %%xmm5          \n\t"
        "packssdw   %%xmm5, %%xmm4      \n\t"
        "paddw      %%xmm2, %%xmm0      \n\t"
        "movdqu     %%xmm4, (%5)        \n\t"
        "add        %6, %5              \n\t"
        "sub        $1, %%ecx           \n\t"
        "jnz        1b                  \n\t"
        ::"c"(overlapLength),"m"(sadd),"m"(ones),"r"(i),"r"(m),"r"(o),"r"(ch)
        :"memory"
    );
}

void TDStretchSSE2::overlapStereo(short *output, const short *input) const
{
    short *o = output;
    const short *i = input;
    const short *m = pMidBuffer;

    __asm__ volatile (
        "movd       %%ecx, %%mm0        \n\t"
        "pxor       %%mm7, %%mm7        \n\t"
        "punpckldq  %%mm0, %%mm0        \n\t"
        "shr        %%ecx               \n\t"
        "movq       %%mm0, %%mm6        \n\t"
        "movq       %2, %%mm2           \n\t"
        "paddw      %%mm2, %%mm6        \n\t"
        "movq       %1, %%mm1           \n\t"
        "paddw      %%mm2, %%mm2        \n\t" 
        "1:                             \n\t"
        "movq       (%4), %%mm4         \n\t"
        "movq       (%3), %%mm3         \n\t"
        "movq       %%mm4, %%mm5        \n\t"
        "punpcklwd  %%mm3, %%mm4        \n\t"
        "add        $8, %3              \n\t"
        "pmaddwd    %%mm0, %%mm4        \n\t"
        "punpckhwd  %%mm3, %%mm5        \n\t"
        "movq       %%mm7, %%mm3        \n\t"
        "pcmpgtd    %%mm4, %%mm3        \n\t"
        "pand       %%mm1, %%mm3        \n\t"
        "pmaddwd    %%mm6, %%mm5        \n\t"
        "paddd      %%mm3, %%mm4        \n\t"
        "movq       %%mm7, %%mm3        \n\t"
        "psrad      $9, %%mm4           \n\t"
        "pcmpgtd    %%mm5, %%mm3        \n\t"
        "pand       %%mm1, %%mm3        \n\t"
        "paddd      %%mm3, %%mm5        \n\t"
        "add        $8, %4              \n\t"
        "psrad      $9, %%mm5           \n\t"
        "paddw      %%mm2, %%mm0        \n\t"
        "packssdw   %%mm5, %%mm4        \n\t"
        "paddw      %%mm2, %%mm6        \n\t"
        "movq       %%mm4, (%5)         \n\t"
        "add        $8, %5              \n\t"
        "sub        $1, %%ecx           \n\t"
        "jnz        1b                  \n\t"
        "emms                           \n\t"
        ::"c"(overlapLength),"m"(sadd),"m"(ones),"r"(i),"r"(m),"r"(o)
        :"memory"
    );
}
