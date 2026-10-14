#include <algorithm>
#include <cstdint>

#include "zoom_filters.h"

#include "libmythtv/sse2.h"

#ifdef Q_PROCESSOR_X86
static   constexpr uint8_t  BUFFPOINTNB   { 16     };
//static constexpr uint16_t BUFFPOINTMASK { 0xffff };
//static constexpr uint8_t  BUFFINCR      { 0xff   };


//static constexpr uint8_t sqrtperte { 16 };
// faire : a % sqrtperte <=> a & pertemask
static constexpr uint8_t PERTEMASK { 0xf };
// faire : a / sqrtperte <=> a >> PERTEDEC
static constexpr uint8_t PERTEDEC { 4 };

void zoom_filter_sse2(int prevX, int prevY,
                      const unsigned int *expix1, unsigned int *expix2,//NOLINT(readability-non-const-parameter)
                      const sintvec& brutS,
                      const sintvec& brutD,
                      int buffratio,
                      const GoomCoefficients &precalCoef)
{
    unsigned int ax = (prevX-1)<<PERTEDEC;
    unsigned int ay = (prevY-1)<<PERTEDEC;
    
    int bufsize = prevX * prevY;

    
    for (int loop=0; loop<bufsize; loop++)
    {
        int pos = 0;
        int coeffs = 0;
    
        int myPos = loop << 1;
        int myPos2 = myPos + 1;
        int brutSmypos = brutS[myPos];
        
        int px = brutSmypos +
             (((brutD[myPos] - brutSmypos)*buffratio) >> BUFFPOINTNB);

        brutSmypos = brutS[myPos2];
        int py = brutSmypos +
             (((brutD[myPos2] - brutSmypos)*buffratio) >> BUFFPOINTNB);

        px = std::max(px, 0);
        py = std::max(py, 0);

        if ((py>=(int)ay) || (px>=(int)ax)) 
        {
            pos=coeffs=0;
        }
        else {
            pos = ((px >> PERTEDEC) + (prevX * (py >> PERTEDEC)));
            /* coef en modulo 15 */
            coeffs = precalCoef [px & PERTEMASK][py & PERTEMASK];
        }

        int posplusprevX = pos + prevX;

        __asm__ volatile (
            "pxor       %%xmm7, %%xmm7\n\t"
            "movd       %1,     %%xmm6\n\t"
        /* recuperation des deux premiers pixels dans mm0 et mm1 */
            "movq       %2,     %%xmm0\n\t" /* b1-v1-r1-a1-b2-v2-r2-a2 */
            "movq       %%xmm0, %%xmm1\n\t" /* b1-v1-r1-a1-b2-v2-r2-a2 */

        /* depackage du premier pixel */
            "punpcklbw  %%xmm7, %%xmm0\n\t" /* 00-b2-00-v2-00-r2-00-a2 */
            "movq       %%xmm6, %%xmm5\n\t" /* xx-xx-xx-xx-c4-c3-c2-c1 */

        /* depackage du 2ieme pixel */
            "punpckhbw  %%xmm7, %%xmm1\n\t" /* 00-b1-00-v1-00-r1-00-a1 */

        /* extraction des coefficients... */
            "punpcklbw  %%xmm5, %%xmm6\n\t" /* c4-c4-c3-c3-c2-c2-c1-c1 */
            "movq       %%xmm6, %%xmm4\n\t" /* c4-c4-c3-c3-c2-c2-c1-c1 */
            "movq       %%xmm6, %%xmm5\n\t" /* c4-c4-c3-c3-c2-c2-c1-c1 */
            "punpcklbw  %%xmm5, %%xmm6\n\t" /* c2-c2-c2-c2-c1-c1-c1-c1 */
            "punpckhbw  %%xmm5, %%xmm4\n\t" /* c4-c4-c4-c4-c3-c3-c3-c3 */
        
            "movq       %%xmm6, %%xmm3\n\t" /* c2-c2-c2-c2-c1-c1-c1-c1 */
            "punpcklbw  %%xmm7, %%xmm6\n\t" /* 00-c1-00-c1-00-c1-00-c1 */
            "punpckhbw  %%xmm7, %%xmm3\n\t" /* 00-c2-00-c2-00-c2-00-c2 */

        /* multiplication des pixels par les coefficients */
            "pmullw     %%xmm6, %%xmm0\n\t" /* c1*b2-c1*v2-c1*r2-c1*a2 */
            "pmullw     %%xmm3, %%xmm1\n\t" /* c2*b1-c2*v1-c2*r1-c2*a1 */
            "paddw      %%xmm1, %%xmm0\n\t"
        
        /* ...extraction des 2 derniers coefficients */
            "movq       %%xmm4, %%xmm5\n\t" /* c4-c4-c4-c4-c3-c3-c3-c3 */
            "punpcklbw  %%xmm7, %%xmm4\n\t" /* 00-c3-00-c3-00-c3-00-c3 */
            "punpckhbw  %%xmm7, %%xmm5\n\t" /* 00-c4-00-c4-00-c4-00-c4 */
        
        /* ajouter la longueur de ligne a esi */
        /* recuperation des 2 derniers pixels */
            "movq       %3,     %%xmm1\n\t"
            "movq       %%xmm1, %%xmm2\n\t"
        
        /* depackage des pixels */
            "punpcklbw  %%xmm7, %%xmm1\n\t"
            "punpckhbw  %%xmm7, %%xmm2\n\t"
        
        /* multiplication pas les coeffs */
            "pmullw     %%xmm4, %%xmm1\n\t"
            "pmullw     %%xmm5, %%xmm2\n\t"
        
        /* ajout des valeurs obtenues de iso8859-15 à la valeur finale */
            "paddw      %%xmm1, %%xmm0\n\t"
            "paddw      %%xmm2, %%xmm0\n\t"
        
        /* division par 256 = 16+16+16+16, puis repackage du pixel final */
            "psrlw      %4,     %%xmm0\n\t"
            "packuswb   %%xmm7, %%xmm0\n\t"
            "movd       %%xmm0, %0\n\t"
            : "=m" (expix2[loop])
            : "m" (coeffs), "m" (expix1[pos]), "m" (expix1[posplusprevX]), "i" (8)
            );
    }
}
#else // !defined(Q_PROCESSOR_X86)
void zoom_filter_sse2([[maybe_unused]] int prevX,
                      [[maybe_unused]] int prevY,
                      [[maybe_unused]] const unsigned int *expix1,
                      [[maybe_unused]] unsigned int *expix2,
                      [[maybe_unused]] const sintvec& brutS,
                      [[maybe_unused]] const sintvec& brutD,
                      [[maybe_unused]] int buffratio,
                      [[maybe_unused]] const GoomCoefficients &precalCoef)
{
}
#endif
