// SSE2 version of the expensive routines for 16 bit integer samples
#include "STTypes.h"
#include "TDStretch.h"
using namespace std;
using namespace soundtouch;

#ifdef ALLOW_SSE
long TDStretchSSE::calcCrossCorrMulti(const short *mPos, const short *cPos) const
{
	
    long corr = 0;
    int i, out[4];
    int count = (overlapLength * channels) - channels;
    long loops = count >> 5;
    long remainder = count - (loops<<5);

    mPos += channels;
    cPos += channels;

    asm(
        "xorps      %%xmm8, %%xmm8      \n\t"
        "movd       %4, %%xmm9          \n\t"
        "1:                             \n\t"
        "movupd     (%1), %%xmm0        \n\t"
        "movupd     16(%1), %%xmm2      \n\t"
        "movupd     32(%1), %%xmm4      \n\t"
        "movupd     48(%1), %%xmm6      \n\t"
        "movupd     (%2), %%xmm1        \n\t"
        "movupd     16(%2), %%xmm3      \n\t"
        "movupd     32(%2), %%xmm5      \n\t"
        "movupd     48(%2), %%xmm7      \n\t"
        "pmaddwd    %%xmm0, %%xmm1      \n\t"
        "pmaddwd    %%xmm2, %%xmm3      \n\t"
        "pmaddwd    %%xmm4, %%xmm5      \n\t"
        "pmaddwd    %%xmm6, %%xmm7      \n\t"
        "psrad      %%xmm9, %%xmm1      \n\t"
        "psrad      %%xmm9, %%xmm3      \n\t"
        "paddd      %%xmm1, %%xmm8      \n\t"
        "psrad      %%xmm9, %%xmm5      \n\t"
        "paddd      %%xmm3, %%xmm8      \n\t"
        "psrad      %%xmm9, %%xmm7      \n\t"
        "add        $64, %1             \n\t"
        "paddd      %%xmm5, %%xmm8      \n\t"
        "add        $64, %2             \n\t"
        "paddd      %%xmm7, %%xmm8      \n\t"
        "loop       1b                  \n\t"
        "movdqa     %%xmm8, %0          \n\t"
        :"=m"(out)
        :"r"(mPos), "r"(cPos), "c"(loops), "r"(overlapDividerBits)
    );

    corr = out[0] + out[1] + out[2] + out[3];

    mPos += loops<<5;
    cPos += loops<<5;

    for (i = 0; i < remainder; i++)
        corr += (mPos[i] * cPos[i]) >> overlapDividerBits;

    return corr;

}

long TDStretchSSE::calcCrossCorrStereo(const short *mPos, const short *cPos) const
{
	
    long corr = 0;
    int i, out[4];
    int count = (overlapLength<<1) - 2;
    long loops = count >> 5;
    long remainder = count - (loops<<5);

    mPos += 2;
    cPos += 2;

    asm(
        "xorps      %%xmm8, %%xmm8      \n\t"
        "movd       %4, %%xmm9          \n\t"
        "1:                             \n\t"
        "movupd     (%1), %%xmm0        \n\t"
        "movupd     16(%1), %%xmm2      \n\t"
        "movupd     32(%1), %%xmm4      \n\t"
        "movupd     48(%1), %%xmm6      \n\t"
        "movupd     (%2), %%xmm1        \n\t"
        "movupd     16(%2), %%xmm3      \n\t"
        "movupd     32(%2), %%xmm5      \n\t"
        "movupd     48(%2), %%xmm7      \n\t"
        "pmaddwd    %%xmm0, %%xmm1      \n\t"
        "pmaddwd    %%xmm2, %%xmm3      \n\t"
        "pmaddwd    %%xmm4, %%xmm5      \n\t"
        "pmaddwd    %%xmm6, %%xmm7      \n\t"
        "psrad      %%xmm9, %%xmm1      \n\t"
        "psrad      %%xmm9, %%xmm3      \n\t"
        "paddd      %%xmm1, %%xmm8      \n\t"
        "psrad      %%xmm9, %%xmm5      \n\t"
        "paddd      %%xmm3, %%xmm8      \n\t"
        "psrad      %%xmm9, %%xmm7      \n\t"
        "add        $64, %1             \n\t"
        "paddd      %%xmm5, %%xmm8      \n\t"
        "add        $64, %2             \n\t"
        "paddd      %%xmm7, %%xmm8      \n\t"
        "loop       1b                  \n\t"
        "movdqa     %%xmm8, %0          \n\t"
        :"=m"(out)
        :"r"(mPos), "r"(cPos), "c"(loops), "r"(overlapDividerBits)
    );

    corr = out[0] + out[1] + out[2] + out[3];

    mPos += loops<<5;
    cPos += loops<<5;

    for (i = 0; i < remainder; i += 2)
        corr += (mPos[i] * cPos[i] +
                 mPos[i+1] * cPos[i+1]) >> overlapDividerBits;

    return corr;

}

void TDStretchSSE::overlapMulti(short *output, const short *input) const
{
    unsigned long ones = 0x0001ffff0001ffffUL;
    
    asm(
        "movd       %%ecx, %%xmm0       \n\t"
        "punpckldq  %%xmm0, %%xmm0      \n\t"
        "shl        %6                  \n\t"
        "punpckldq  %%xmm0, %%xmm0      \n\t"
        "movd       %1, %%xmm1          \n\t"
        "movq       %2, %%xmm2          \n\t"
        "punpckldq  %%xmm2, %%xmm2      \n\t"
        "1:                             \n\t"
        "movdqu     (%3), %%xmm3        \n\t"
        "movdqu     (%4), %%xmm4        \n\t"
        "movdqu     %%xmm4, %%xmm5      \n\t"
        "punpcklwd  %%xmm3, %%xmm4      \n\t"
        "punpckhwd  %%xmm3, %%xmm5      \n\t"
        "add        %6, %3              \n\t"
        "pmaddwd    %%xmm0, %%xmm4      \n\t"
        "pmaddwd    %%xmm0, %%xmm5      \n\t"
        "psrad      %%xmm1, %%xmm4      \n\t"
        "psrad      %%xmm1, %%xmm5      \n\t"
        "add        %6, %4              \n\t"
        "packssdw   %%xmm5, %%xmm4      \n\t"
        "movdqu     %%xmm4, (%5)        \n\t"
        "add        %6, %5              \n\t"
        "paddw      %%xmm2, %%xmm0      \n\t"
        "loop       1b                  \n\t"
        ::"c"(overlapLength),"r"(overlapDividerBits),
        "r"(ones),"r"(input),"r"(pMidBuffer),"r"(output),
	"r"((long)channels)
    );
}

void TDStretchSSE::overlapStereo(short *output, const short *input) const
{
    // 4 bytes per sample - use MMX
    unsigned long ones = 0x0001ffff0001ffffUL;

    asm(
        "movd       %%ecx, %%mm0        \n\t"
        "shr        $1, %%ecx           \n\t"
        "punpckldq  %%mm0, %%mm0        \n\t"
        "movq       %1, %%mm1           \n\t"
        "movq       %%mm0, %%mm6        \n\t"
        "movq       %2, %%mm2           \n\t"
        "paddw      %%mm2, %%mm6        \n\t"
        "paddw      %%mm2, %%mm2        \n\t" 
        "1:                             \n\t"
        "movq       (%3), %%mm3         \n\t"
        "movq       (%4), %%mm4         \n\t"
        "movq       %%mm4, %%mm5        \n\t"
        "punpcklwd  %%mm3, %%mm4        \n\t"
        "punpckhwd  %%mm3, %%mm5        \n\t"
        "add        $8, %3              \n\t"
        "pmaddwd    %%mm0, %%mm4        \n\t"
        "pmaddwd    %%mm6, %%mm5        \n\t"
        "psrad      %%mm1, %%mm4        \n\t"
        "psrad      %%mm1, %%mm5        \n\t"
        "add        $8, %4              \n\t"
        "packssdw   %%mm5, %%mm4        \n\t"
        "movq       %%mm4, (%5)         \n\t"
        "add        $8, %5              \n\t"
        "paddw      %%mm2, %%mm0        \n\t"
        "paddw      %%mm2, %%mm6        \n\t"
        "loop       1b                  \n\t"
        "emms                           \n\t"
        ::"c"(overlapLength),"r"((long)overlapDividerBits),
        "r"(ones),"r"(input),"r"(pMidBuffer),"r"(output)
    );

}
#endif // ALLOW_SSE
