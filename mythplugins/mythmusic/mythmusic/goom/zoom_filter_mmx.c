#define BUFFPOINTNB 16
#define BUFFPOINTMASK 0xffff
#define BUFFINCR 0xff

#include "mmx.h"

#define sqrtperte 16
// faire : a % sqrtperte <=> a & pertemask
#define PERTEMASK 0xf
// faire : a / sqrtperte <=> a >> PERTEDEC
#define PERTEDEC 4

void    zoom_filter_mmx (int prevX, int prevY,

                 unsigned int *expix1, unsigned int *expix2,

                 int *brutS, int *brutD, int buffratio,

                 int precalCoef[16][16]);
int zoom_filter_mmx_supported(void);


int zoom_filter_mmx_supported(void) {
	return (mm_support()&0x1);
}

void zoom_filter_mmx (int prevX, int prevY,
					  unsigned int *expix1, unsigned int *expix2,
					  int *brutS, int *brutD, int buffratio,
					  int precalCoef[16][16])
{
  unsigned int ax = (prevX-1)<<PERTEDEC, ay = (prevY-1)<<PERTEDEC;
  
  int bufsize = prevX * prevY;
  int loop;

  __asm__ ("pxor %mm7,%mm7");
  
  for (loop=0; loop<bufsize; loop++)
	{
	  int px,py;
	  int pos;
	  int coeffs;

	  int myPos = loop << 1,
		myPos2 = myPos + 1;
	  int brutSmypos = brutS[myPos];
	  
	  px = brutSmypos + (((brutD[myPos] - brutSmypos)*buffratio) >> BUFFPOINTNB);
	  brutSmypos = brutS[myPos2];
	  py = brutSmypos + (((brutD[myPos2] - brutSmypos)*buffratio) >> BUFFPOINTNB);
	  
	  if ((py>=(int)ay) || (px>=(int)ax)) {
		pos=coeffs=0;
	  }
	  else {
		pos = ((px >> PERTEDEC) + prevX * (py >> PERTEDEC));
		// coef en modulo 15
		coeffs = precalCoef [px & PERTEMASK][py & PERTEMASK];
	  }

	  __asm__ __volatile__ ("
			   movd %%eax,%%mm6
			   ;// recuperation des deux premiers pixels dans mm0 et mm1
			   movq (%%edx,%%ebx,4), %%mm0
			   movq %%mm0, %%mm1	
			   
			   ;// depackage du premier pixel
			   punpcklbw %%mm7, %%mm0	
			   
			   movq %%mm6, %%mm5	
			   ;// depackage du 2ieme pixel
			   punpckhbw %%mm7, %%mm1
							   
			   ;// extraction des coefficients...
			   punpcklbw %%mm5, %%mm6
			   movq %%mm6, %%mm4
			   movq %%mm6, %%mm5
											   
			   punpcklbw %%mm5, %%mm6
			   punpckhbw %%mm5, %%mm4

			   movq %%mm6, %%mm3
			   punpcklbw %%mm7, %%mm6
			   punpckhbw %%mm7, %%mm3
	
			   ;// multiplication des pixels par les coefficients
			   pmullw %%mm6, %%mm0
			   pmullw %%mm3, %%mm1
			   paddw %%mm1, %%mm0
			   
			   ;// ...extraction des 2 derniers coefficients
			   movq %%mm4, %%mm5
			   punpcklbw %%mm7, %%mm4
			   punpckhbw %%mm7, %%mm5

			   /* ajouter la longueur de ligne a esi */
			   addl 8(%%ebp),%%ebx
	   
			   ;// recuperation des 2 derniers pixels
			   movq (%%edx,%%ebx,4), %%mm1
			   movq %%mm1, %%mm2
			
			   ;// depackage des pixels
			   punpcklbw %%mm7, %%mm1
			   punpckhbw %%mm7, %%mm2
			
			   ;// multiplication pas les coeffs
			   pmullw %%mm4, %%mm1
			   pmullw %%mm5, %%mm2
			   
			   ;// ajout des valeurs obtenues à la valeur finale
			   paddw %%mm1, %%mm0
			   paddw %%mm2, %%mm0
			   
			   ;// division par 256 = 16+16+16+16, puis repackage du pixel final
			   psrlw $8, %%mm0
			   packuswb %%mm7, %%mm0

			   movd %%mm0,%%eax
			   "
							:"=eax"(expix2[loop])
							:"ebx"(pos),"eax"(coeffs),"edx"(expix1)
							
				);
	  
//	  expix2[loop] = couleur;
	  
	  __asm__ __volatile__ ("emms");
	}
}
