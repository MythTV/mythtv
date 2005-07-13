#ifndef GOOM_MMX_H

#include <mythtv/mythconfig.h>
#include <mythtv/ffmpeg/mmx.h>

int mm_support(void);

#define MM_MMX    0x0001 /* standard MMX */
#define MM_3DNOW  0x0004 /* AMD 3DNOW */
#define MM_MMXEXT 0x0002 /* SSE integer functions or AMD MMX ext */
#define MM_SSE    0x0008 /* SSE functions */
#define MM_SSE2   0x0010 /* PIV SSE2 functions */
#define MM_3DNOWEXT  0x0020 /* AMD 3DNowExt */

#endif
