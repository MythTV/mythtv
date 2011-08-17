#ifdef MMX
//#include "mmx.h"
#include "libavutil/cpu.h"

int zoom_filter_sse2_supported () 
{
    return (av_get_cpu_flags() & AV_CPU_FLAG_SSE2);
}

void zoom_filter_sse2(int prevX, int prevY, unsigned int *expix1, 
                      unsigned int *expix2, int *lbruS, int *lbruD, 
                      int buffratio, int precalCoef[16][16])
{
    int pos1x, pos1y, pos2x, pos2y;
    int coeff1, coeff2;
    long pos1, pos2;
    unsigned long count, cnt;
    unsigned int bufsize = prevX * prevY;
    unsigned int ax = (prevX - 1) << 4, ay = (prevY - 1) << 4; 
    
    asm volatile (
        "pxor       %%xmm5, %%xmm5    \n\t"
        ::
    );

    for(count = 0; count < bufsize; count += 2)
    {
        cnt = count << 1;

        asm volatile (
            "movdqu     %4, %%xmm0      \n\t" // lbruS2y lbruS2x lbruS1y lbruS1x
            "movd       %6, %%xmm2      \n\t"
            "movdqu     %5, %%xmm1      \n\t" // lbruD2y lbruD2x lbruD1y lbruD1x
            "punpckldq  %%xmm2, %%xmm2  \n\t"
            "psubd      %%xmm0, %%xmm1  \n\t" // lbruD - lbruS
            "punpckldq  %%xmm2, %%xmm2  \n\t"
            "pmullw     %%xmm2, %%xmm1  \n\t" // * buffratio
            "psrld      $16, %%xmm1     \n\t" // >> 16
            "paddd      %%xmm1, %%xmm0  \n\t"
            "movdqa     %%xmm0, %%xmm1  \n\t"
            "pcmpgtd    %%xmm5, %%xmm0  \n\t"
            "pand       %%xmm0, %%xmm1  \n\t"
            "movd       %%xmm1, %0      \n\t"
            "psrldq     $4, %%xmm1      \n\t"
            "movd       %%xmm1, %1      \n\t"
            "psrldq     $4, %%xmm1      \n\t"
            "movd       %%xmm1, %2      \n\t"
            "psrldq     $4, %%xmm1      \n\t"
            "movd       %%xmm1, %3      \n\t"     
            :"=r"(pos1x),"=r"(pos1y),"=r"(pos2x),"=r"(pos2y)
            :"m"(lbruS[cnt]),"m"(lbruD[cnt]),"r"(buffratio)
        );
        
        coeff1 = precalCoef[pos1x & 0xf][pos1y & 0xf];
        coeff2 = precalCoef[pos2x & 0xf][pos2y & 0xf];
        pos1 = (pos1x >> 4) + prevX * (pos1y >> 4);
        pos2 = (pos2x >> 4) + prevX * (pos2y >> 4);
        
        if ((pos1y >= (int)ay) || (pos1x >= (int)ax))
	    pos1 = coeff1 = 0; 
        if ((pos2y >= (int)ay) || (pos2x >= (int)ax))
	    pos2 = coeff2 = 0;
        
        asm volatile (
            "movd       %0, %%xmm0      \n\t"
            "movd       %1, %%xmm1      \n\t"
            "punpcklwd  %%xmm0, %%xmm0  \n\t"
            "punpcklwd  %%xmm1, %%xmm1  \n\t"
            "movq       (%2,%3,4),%%xmm2\n\t"
            "punpcklbw  %%xmm0, %%xmm1  \n\t"
            "movq       (%2,%4,4),%%xmm3\n\t"
            "punpckldq  %%xmm2, %%xmm3  \n\t"
            "movdqa     %%xmm1, %%xmm0  \n\t"
            "movdqa     %%xmm3, %%xmm4  \n\t"
            "punpcklbw  %%xmm5, %%xmm3  \n\t"
            "punpcklbw  %%xmm1, %%xmm1  \n\t"
            "punpckhbw  %%xmm5, %%xmm4  \n\t"
            "punpcklbw  %%xmm1, %%xmm1  \n\t"
            "add        %5, %3          \n\t"
            "movdqa     %%xmm1, %%xmm2  \n\t"
            "punpckhbw  %%xmm5, %%xmm1  \n\t"
            "add        %5, %4          \n\t"
            "punpcklbw  %%xmm5, %%xmm2  \n\t"
            "pmullw     %%xmm1, %%xmm4  \n\t"
            "pmullw     %%xmm2, %%xmm3  \n\t"
            "movq       (%2,%3,4),%%xmm2\n\t"
            "paddw      %%xmm3, %%xmm4  \n\t"
            "movq       (%2,%4,4),%%xmm3\n\t"
            "punpckldq  %%xmm2, %%xmm3  \n\t"
            "punpckhbw  %%xmm0, %%xmm0  \n\t"
            "movdqa     %%xmm3, %%xmm1  \n\t"
            "punpcklbw  %%xmm5, %%xmm3  \n\t"
            "punpckhbw  %%xmm0, %%xmm0  \n\t"
            "punpckhbw  %%xmm5, %%xmm1  \n\t"
            "movdqa     %%xmm0, %%xmm2  \n\t"
            "punpcklbw  %%xmm5, %%xmm0  \n\t"
            "punpckhbw  %%xmm5, %%xmm2  \n\t"
            "pmullw     %%xmm0, %%xmm3  \n\t"
            "pmullw     %%xmm2, %%xmm1  \n\t"
            "paddw      %%xmm3, %%xmm4  \n\t"
            "paddw      %%xmm1, %%xmm4  \n\t" 
            "psrlw      $8, %%xmm4      \n\t"
            "packuswb   %%xmm5, %%xmm4  \n\t"
            "movq       %%xmm4,(%6,%7,4)\n\t"
            ::"m"(coeff1),"m"(coeff2),"r"(expix1),"r"(pos1),"r"(pos2),
            "r"((long)prevX),"r"(expix2),"r"(count)
        );
    }
}
        
#else 

int zoom_filter_sse2_supported () {
	return 0;
}
void zoom_filter_sse2(int prevX, int prevY, unsigned int *expix1, 
                      unsigned int *expix2, int *lbruS, int *lbruD, 
                      int buffratio, int precalCoef[16][16])
{
    (void) prevX;     (void) prevY;
    (void) expix1;    (void) expix2;
    (void) lbruS;     (void) lbruD;
    (void) buffratio; (void) precalCoef;
	return;
}

#endif // MMX
