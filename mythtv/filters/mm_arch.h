/* mm_arch.h - Multi-media CPU acceleration for several architectures */

#include "libavutil/mem.h"
#include "libavutil/cpu.h"

/* That has "extern int mm_flags;", or "#define mm_flags 0" as a fallthru.  */
/* Our code always declares an int, and "int 0;" won't compile well, so ... */
#ifdef mm_flags
#undef mm_flags
#endif

#if HAVE_MMX 
  #include "libavutil/x86/asm.h"
#else 
  #define emms()    ; 
#endif
