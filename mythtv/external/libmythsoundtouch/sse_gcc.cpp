// SSE2 versions of the expensive routines for float samples

#include "STTypes.h"
#include "TDStretch.h"
#include "FIRFilter.h"
#include "inttypes.h"

using namespace soundtouch;

double TDStretchSSE3::calcCrossCorrMulti(const float *mPos, const float *cPos) const
{
    double corr = 0;
    int count = overlapLength * channels;
    int loops = count >> 4;
    int i = loops << 4;
    const float *mp = mPos;
    const float *cp = cPos;

    __asm__ volatile (
        "xorpd      %%xmm7, %%xmm7      \n\t"
        "1:                             \n\t"
        "movups       (%1), %%xmm0      \n\t"
        "movups     16(%1), %%xmm1      \n\t"
        "mulps      (%2),   %%xmm0      \n\t"
        "movups     32(%1), %%xmm2      \n\t"
        "addps      %%xmm0, %%xmm7      \n\t"
        "mulps      16(%2), %%xmm1      \n\t"
        "movups     48(%1), %%xmm3      \n\t"
        "mulps      32(%2), %%xmm2      \n\t"
        "addps      %%xmm1, %%xmm7      \n\t"
        "mulps      48(%2), %%xmm3      \n\t"
        "addps      %%xmm2, %%xmm7      \n\t"
        "add        $64,    %1          \n\t"
        "add        $64,    %2          \n\t"
        "addps      %%xmm3, %%xmm7      \n\t"
        "sub        $1,     %%ecx       \n\t"
        "jnz        1b                  \n\t"
        "haddps     %%xmm7, %%xmm7      \n\t"
        "cvtps2pd   %%xmm7, %%xmm7      \n\t"
        "haddpd     %%xmm7, %%xmm7      \n\t"
        "movsd      %%xmm7, %0          \n\t"
        :"=m"(corr),"+r"(mp), "+r"(cp)
        :"c"(loops)
    );

    for (; i < count; i++)
        corr += *mp++ * *cp++;

    return corr;
}

double TDStretchSSE2::calcCrossCorrMulti(const float *mPos, const float *cPos) const
{
    double corr = 0;
    int count = overlapLength * channels;
    int loops = count >> 4;
    int i = loops << 4;
    const float *mp = mPos;
    const float *cp = cPos;

    __asm__ volatile (
        "xorpd      %%xmm7, %%xmm7      \n\t"
        "1:                             \n\t"
        "movups       (%1), %%xmm0      \n\t"
        "movups     16(%1), %%xmm1      \n\t"
        "mulps      (%2),   %%xmm0      \n\t"
        "movups     32(%1), %%xmm2      \n\t"
        "addps      %%xmm0, %%xmm7      \n\t"
        "mulps      16(%2), %%xmm1      \n\t"
        "movups     48(%1), %%xmm3      \n\t"
        "mulps      32(%2), %%xmm2      \n\t"
        "addps      %%xmm1, %%xmm7      \n\t"
        "mulps      48(%2), %%xmm3      \n\t"
        "addps      %%xmm2, %%xmm7      \n\t"
        "add        $64,    %1          \n\t"
        "add        $64,    %2          \n\t"
        "addps      %%xmm3, %%xmm7      \n\t"
        "sub        $1,     %%ecx       \n\t"
        "jnz        1b                  \n\t"
        "movaps     %%xmm7, %%xmm6      \n\t"
        "shufps     $0x4e,  %%xmm7, %%xmm6  \n\t"
        "addps      %%xmm6, %%xmm7      \n\t"
        "cvtps2pd   %%xmm7, %%xmm7      \n\t"
        "movapd     %%xmm7, %%xmm6      \n\t"
        "shufpd     $0x01,  %%xmm7, %%xmm6  \n\t"
        "addpd      %%xmm6, %%xmm7      \n\t"
        "movsd      %%xmm7, %0          \n\t"
        :"=m"(corr),"+r"(mp), "+r"(cp)
        :"c"(loops)
    );

    for (; i < count; i++)
        corr += *mp++ * *cp++;

    return corr;
}

double TDStretchSSE3::calcCrossCorrStereo(const float *mPos, const float *cPos) const
{
    double corr = 0;
    int count = overlapLength <<1;
    int loops = count >> 4;
    int i = loops << 4;
    const float *mp = mPos;
    const float *cp = cPos;

    __asm__ volatile (
        "xorpd      %%xmm7, %%xmm7      \n\t"
        "1:                             \n\t"
        "movups       (%1), %%xmm0      \n\t"
        "movups     16(%1), %%xmm1      \n\t"
        "mulps      (%2),   %%xmm0      \n\t"
        "movups     32(%1), %%xmm2      \n\t"
        "addps      %%xmm0, %%xmm7      \n\t"
        "mulps      16(%2), %%xmm1      \n\t"
        "movups     48(%1), %%xmm3      \n\t"
        "mulps      32(%2), %%xmm2      \n\t"
        "addps      %%xmm1, %%xmm7      \n\t"
        "mulps      48(%2), %%xmm3      \n\t"
        "addps      %%xmm2, %%xmm7      \n\t"
        "add        $64,    %1          \n\t"
        "add        $64,    %2          \n\t"
        "addps      %%xmm3, %%xmm7      \n\t"
        "sub        $1,     %%ecx       \n\t"
        "jnz        1b                  \n\t"
        "haddps     %%xmm7, %%xmm7      \n\t"
        "cvtps2pd   %%xmm7, %%xmm7      \n\t"
        "haddpd     %%xmm7, %%xmm7      \n\t"
        "movsd      %%xmm7, %0          \n\t"
        :"=m"(corr),"+r"(mp), "+r"(cp)
        :"c"(loops)
    );

    for (; i < count; i += 2)
        corr += (mp[i] * cp[i] + mp[i + 1] * cp[i + 1]);

    return corr;
}

double TDStretchSSE2::calcCrossCorrStereo(const float *mPos, const float *cPos) const
{
    double corr = 0;
    int count = overlapLength <<1;
    int loops = count >> 4;
    int i = loops << 4;
    const float *mp = mPos;
    const float *cp = cPos;

    __asm__ volatile (
        "xorpd      %%xmm7, %%xmm7      \n\t"
        "1:                             \n\t"
        "movups       (%1), %%xmm0      \n\t"
        "movups     16(%1), %%xmm1      \n\t"
        "mulps      (%2),   %%xmm0      \n\t"
        "movups     32(%1), %%xmm2      \n\t"
        "addps      %%xmm0, %%xmm7      \n\t"
        "mulps      16(%2), %%xmm1      \n\t"
        "movups     48(%1), %%xmm3      \n\t"
        "mulps      32(%2), %%xmm2      \n\t"
        "addps      %%xmm1, %%xmm7      \n\t"
        "mulps      48(%2), %%xmm3      \n\t"
        "addps      %%xmm2, %%xmm7      \n\t"
        "add        $64,    %1          \n\t"
        "add        $64,    %2          \n\t"
        "addps      %%xmm3, %%xmm7      \n\t"
        "sub        $1,     %%ecx       \n\t"
        "jnz        1b                  \n\t"
        "movaps     %%xmm7, %%xmm6      \n\t"
        "shufps     $0x4e,  %%xmm7, %%xmm6  \n\t"
        "addps      %%xmm6, %%xmm7      \n\t"
        "cvtps2pd   %%xmm7, %%xmm7      \n\t"
        "movapd     %%xmm7, %%xmm6      \n\t"
        "shufpd     $0x01,  %%xmm7, %%xmm6  \n\t"
        "addpd      %%xmm6, %%xmm7      \n\t"
        "movsd      %%xmm7, %0          \n\t"
        :"=m"(corr),"+r"(mp), "+r"(cp)
        :"c"(loops)
    );

    for (; i < count; i += 2)
        corr += (mp[i] * cp[i] + mp[i + 1] * cp[i + 1]);

    return corr;
}

void TDStretchSSE2::overlapMulti(float *output, const float *input) const
{

    float *o = output;
    const float *i = input;
    const float *m = pMidBuffer;

    if (channels > 4)
        __asm__ volatile (
            "cvtsi2ss   %%ecx,  %%xmm7      \n\t"
            "shl        $2,     %4          \n\t"
            "punpckldq  %%xmm7, %%xmm7      \n\t"
            "xorpd      %%xmm6, %%xmm6      \n\t"
            "punpckldq  %%xmm7, %%xmm7      \n\t"
            "rcpps      %%xmm7, %%xmm1      \n\t"
            "mulps      %%xmm1, %%xmm7      \n\t"
            "1:                             \n\t"
            "movups     (%1),   %%xmm2      \n\t"
            "movups     16(%1), %%xmm4      \n\t"
            "mulps      %%xmm6, %%xmm2      \n\t"
            "movups     (%2),   %%xmm3      \n\t"
            "movups     16(%2), %%xmm5      \n\t"
            "mulps      %%xmm7, %%xmm3      \n\t"
            "add        %4,     %1          \n\t"
            "mulps      %%xmm6, %%xmm4      \n\t"
            "addps      %%xmm2, %%xmm3      \n\t"
            "mulps      %%xmm7, %%xmm5      \n\t"
            "movups     %%xmm3, (%3)        \n\t"
            "addps      %%xmm4, %%xmm5      \n\t"
            "add        %4,     %2          \n\t"
            "movups     %%xmm5, 16(%3)      \n\t"
            "addps      %%xmm1, %%xmm6      \n\t"
            "add        %4,     %3          \n\t"
            "subps      %%xmm1, %%xmm7      \n\t"
            "sub        $1,     %%ecx       \n\t"
            "jnz        1b                  \n\t"
            :
            :"c"(overlapLength),"r"(i),"r"(m),"r"(o),"r"((long)channels)
        );
    else
        __asm__ volatile (
            "cvtsi2ss   %%ecx, %%xmm7      \n\t"
            "shl        $2, %4              \n\t"
            "shr        %%ecx               \n\t"
            "punpckldq  %%xmm7, %%xmm7      \n\t"
            "xorpd      %%xmm6, %%xmm6      \n\t"
            "punpckldq  %%xmm7, %%xmm7      \n\t"
            "rcpps      %%xmm7, %%xmm1      \n\t"
            "mulps      %%xmm1, %%xmm7      \n\t"
            "1:                             \n\t"
            "movups     (%1),   %%xmm2      \n\t"
            "movups     16(%1), %%xmm4      \n\t"
            "mulps      %%xmm6, %%xmm2      \n\t"
            "movups     (%2),   %%xmm3      \n\t"
            "movups     16(%2), %%xmm5      \n\t"
            "mulps      %%xmm7, %%xmm3      \n\t"
            "addps      %%xmm1, %%xmm6      \n\t"
            "add        %4,     %1          \n\t"
            "addps      %%xmm2, %%xmm3      \n\t"
            "add        %4,     %2          \n\t"
            "subps      %%xmm1, %%xmm7      \n\t"
            "movups     %%xmm3, (%3)        \n\t"
            "add        %4,     %3          \n\t"
            "mulps      %%xmm6, %%xmm4      \n\t"
            "add        %4,     %1          \n\t"
            "mulps      %%xmm7, %%xmm5      \n\t"
            "addps      %%xmm1, %%xmm6      \n\t"
            "add        %4,     %2          \n\t"
            "addps      %%xmm4, %%xmm5      \n\t"
            "subps      %%xmm1, %%xmm7      \n\t"
            "movups     %%xmm5, (%3)        \n\t"
            "add        %4,     %3          \n\t"
            "sub        $1,     %%ecx       \n\t"
            "jnz        1b                  \n\t"
            :
            :"c"(overlapLength),"r"(i),"r"(m),"r"(o),"r"((long)channels)
        );
}

void TDStretchSSE2::overlapStereo(float *output, const float *input) const
{
    float *o = output;
    const float *i = input;
    const float *m = pMidBuffer;

    __asm__ volatile (
        "cvtsi2ss   %%ecx, %%xmm7       \n\t"
        "shr        %%ecx               \n\t"
        "xorpd      %%xmm6, %%xmm6      \n\t"
        "punpckldq  %%xmm7, %%xmm7      \n\t"
        "rcpps      %%xmm7, %%xmm1      \n\t"
        "mulps      %%xmm1, %%xmm7      \n\t"
        "1:                             \n\t"
        "movups     (%1),  %%xmm2       \n\t"
        "movups     8(%1), %%xmm4       \n\t"
        "mulps      %%xmm6, %%xmm2      \n\t"
        "movups     (%2),  %%xmm3       \n\t"
        "movups     8(%2), %%xmm5       \n\t"
        "mulps      %%xmm7, %%xmm3      \n\t"
        "addps      %%xmm1, %%xmm6      \n\t"
        "addps      %%xmm2, %%xmm3      \n\t"
        "subps      %%xmm1, %%xmm7      \n\t"
        "movlps     %%xmm3, (%3)        \n\t"
        "add        $8,    %3           \n\t"
        "mulps      %%xmm6, %%xmm4      \n\t"
        "add        $16,   %1           \n\t"
        "mulps      %%xmm7, %%xmm5      \n\t"
        "addps      %%xmm1, %%xmm6      \n\t"
        "add        $16,   %2           \n\t"
        "addps      %%xmm4, %%xmm5      \n\t"
        "subps      %%xmm1, %%xmm7      \n\t"
        "movlps     %%xmm5, (%3)        \n\t"
        "add        $8,    %3           \n\t"
        "sub        $1,    %%ecx        \n\t"
        "jnz        1b                  \n\t"
        :
        :"c"(overlapLength),"r"(i),"r"(m),"r"(o)
    );
}

FIRFilterSSE2::FIRFilterSSE2() : FIRFilter()
{
    filterCoeffsAlign = NULL;
    filterCoeffsUnalign = NULL;
}

FIRFilterSSE2::~FIRFilterSSE2()
{
    delete[] filterCoeffsUnalign;
    filterCoeffsAlign = NULL;
    filterCoeffsUnalign = NULL;
}


void FIRFilterSSE2::setCoefficients(const float *coeffs, uint newLen, uint uRDF)
{
    uint i;
    FIRFilter::setCoefficients(coeffs, newLen, uRDF);

    // Ensure that filter coeffs array is aligned to 16-byte boundary
    delete[] filterCoeffsUnalign;
    filterCoeffsUnalign = new float[2 * newLen + 16];
    filterCoeffsAlign = (float *)(((ulong)filterCoeffsUnalign + 15) & -16);

    float fdiv = (float)resultDivider;

    for (i = 0; i < newLen; i++)
    {
        filterCoeffsAlign[2 * i + 0] =
        filterCoeffsAlign[2 * i + 1] = coeffs[i + 0] / fdiv;
    }
}

uint FIRFilterSSE2::evaluateFilterStereo(float *dest, const float *src, const uint numSamples) const
{
    uint count = (numSamples - length) & -2;

    for (int i = 0; i < count; i += 2)
    {
        __asm__ volatile(
            "xorpd      %%xmm6, %%xmm6          \n\t"
            "xorpd      %%xmm7, %%xmm7          \n\t"
            "1:                                 \n\t"
            "movups     (%1),   %%xmm1          \n\t"
            "movups     8(%1),  %%xmm2          \n\t"
            "mulps      (%2),   %%xmm1          \n\t"
            "movups     16(%1), %%xmm3          \n\t"
            "mulps      (%2),   %%xmm2          \n\t"
            "addps      %%xmm1, %%xmm6          \n\t"
            "movups     24(%1), %%xmm4          \n\t"
            "addps      %%xmm2, %%xmm7          \n\t"
            "mulps      16(%2), %%xmm3          \n\t"
            "movups     32(%1), %%xmm1          \n\t"
            "mulps      16(%2), %%xmm4          \n\t"
            "addps      %%xmm3, %%xmm6          \n\t"
            "movups     40(%1), %%xmm2          \n\t"
            "addps      %%xmm4, %%xmm7          \n\t"
            "mulps      32(%2), %%xmm1          \n\t"
            "movups     48(%1), %%xmm3          \n\t"
            "mulps      32(%2), %%xmm2          \n\t"
            "addps      %%xmm1, %%xmm6          \n\t"
            "movups     56(%1), %%xmm4          \n\t"
            "addps      %%xmm2, %%xmm7          \n\t"
            "mulps      48(%2), %%xmm3          \n\t"
            "add        $64,    %1              \n\t"
            "mulps      48(%2), %%xmm4          \n\t"
            "addps      %%xmm3, %%xmm6          \n\t"
            "add        $64,    %2              \n\t"
            "addps      %%xmm4, %%xmm7          \n\t"
            "sub        $1,     %%ecx           \n\t"
            "jnz        1b                      \n\t"
            "movhlps    %%xmm6, %%xmm0          \n\t"
            "movlhps    %%xmm7, %%xmm0          \n\t"
            "shufps     $0xe4,  %%xmm7, %%xmm6  \n\t"
            "addps      %%xmm0, %%xmm6          \n\t"
            "movups     %%xmm6, (%0)            \n\t"
            :
            :"r"(dest),"r"(src),"r"(filterCoeffsAlign),"c"(length>>3)
        );
        src  += 4;
        dest += 4;
    }

    return count;
}
