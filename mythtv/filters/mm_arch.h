/* mm_arch.h - Multi-media CPU acceleration for several architectures */

#ifndef MM_ALTIVEC
  #define MM_ALTIVEC 0x0001
#endif

#ifdef MMX
  #include "i386/mmx.h"
#else
  #define emms()    ;

  /* Only i386 and ppc code in libavcodec define mm_support() */
  #ifndef ARCH_POWERPC
    #define mm_support()    0;
  #endif
#endif
