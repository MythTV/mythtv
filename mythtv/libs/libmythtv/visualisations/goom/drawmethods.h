#ifndef DRAWMETHODS_H
#define DRAWMETHODS_H

#include "libmythtv/sse2.h"

#define DRAWMETHOD_NORMAL(adr,col) {*(adr) = (col);}
#define DRAWMETHOD_OR(adr,col) {*(adr)|=(col);}

#define DRAWMETHOD_PLUS_C(_dest,_col) \
{\
	unsigned char *dra = (unsigned char*)&(_dest);\
	unsigned char *cra = (unsigned char*)&(_col);\
	for (int i = 0; i < 4; i++) {\
		dra[i] += cra[i];\
		if (dra[i] < cra[i]) dra[i] = 255U;\
	}\
}

#ifdef Q_PROCESSOR_X86
#define DRAWMETHOD_PLUS(_dest,_col) \
	if (sse2_check()) \
	{ \
		__asm__ volatile ( \
			"movd 		%[color], 	%%xmm1	\n\t" \
			"movd 		%[in], 		%%xmm0	\n\t" \
			"paddusb 	%%xmm1, 	%%xmm0	\n\t" \
			"movd 		%%xmm0, 	%[out]	\n\t" \
			: [out] "=m" (_dest) \
			: [in] "m" (_dest), [color] "m" (_col)\
			: "xmm0", "xmm1"  \
			); \
	} \
	else \
	{ \
		DRAWMETHOD_PLUS_C(_dest, _col); \
	}
#else
#define DRAWMETHOD_PLUS(_dest,_col) DRAWMETHOD_PLUS_C((_dest),(_col))
#endif

#ifndef DRAWMETHOD
#define DRAWMETHOD DRAWMETHOD_PLUS(*p,col)

inline void draw_line (int *data, int x1, int y1, int x2, int y2, int col, int screenx, int screeny) {
    int     x = 0;	// am, tmp
    int     y = 0;
    int     dx = 0;
    int     dy = 0;
    int    *p = nullptr;

	if ((y1 < 0) || (y2 < 0) || (x1 < 0) || (x2 < 0) || (y1 >= screeny) || (y2 >= screeny) || (x1 >= screenx) || (x2 >= screenx)) 
			return;
        
	dx = x2 - x1;
	dy = y2 - y1;
	if (x1 > x2) {
		int tmp = x1;
		x1 = x2;
		x2 = tmp;
		tmp = y1;
		y1 = y2;
		y2 = tmp;
		dx = x2 - x1;
		dy = y2 - y1;
	}

	/* vertical line */
	if (dx == 0) {
		if (y1 < y2) {
			p = &(data[(screenx * y1) + x1]);
			for (y = y1; y <= y2; y++) {
				DRAWMETHOD;
				p += screenx;
			}
		}
		else {
			p = &(data[(screenx * y2) + x1]);
			for (y = y2; y <= y1; y++) {
				DRAWMETHOD;
				p += screenx;
			}
		}
		return;
	}
	/* horizontal line */
	if (dy == 0) {
		if (x1 < x2) {
			p = &(data[(screenx * y1) + x1]);
			for (x = x1; x <= x2; x++) {
				DRAWMETHOD;
				p++;
			}
			return;
		}
                p = &(data[(screenx * y1) + x2]);
                for (x = x2; x <= x1; x++) {
                    DRAWMETHOD;
                    p++;
                }
                return;
	}
	/* 1    */
	
	/* 2 */
	if (y2 > y1) {
		/* steep */
		if (dy > dx) {
			dx = ((dx << 16) / dy);
			x = x1 << 16;
			for (y = y1; y <= y2; y++) {
				int xx = x >> 16;
				p = &(data[(screenx * y) + xx]);
				DRAWMETHOD;
#if 0
				if (xx < (screenx - 1)) {
					p++;
				}
#endif
				x += dx;
			}
			return;
		}
		/* shallow */
                dy = ((dy << 16) / dx);
                y = y1 << 16;
                for (x = x1; x <= x2; x++) {
                    int yy = y >> 16;
                    p = &(data[(screenx * yy) + x]);
                    DRAWMETHOD;
                    y += dy;
                }
	}
	/* 2 */
	
	/* 1    */
	else {
		/* steep */
		if (-dy > dx) {
			dx = ((dx << 16) / -dy);
			x = (x1 + 1) << 16;
			for (y = y1; y >= y2; y--) {
				int xx = x >> 16;
				p = &(data[(screenx * y) + xx]);
				DRAWMETHOD;
#if 0
				if (xx < (screenx - 1)) {
					p--;
				}
#endif
				x += dx;
			}
			return;
		}
		/* shallow */
                dy = ((dy << 16) / dx);
                y = y1 << 16;
                for (x = x1; x <= x2; x++) {
                    int yy = y >> 16;
                    p = &(data[(screenx * yy) + x]);
                    DRAWMETHOD;
                    y += dy;
                }
                return;
	}
}
#endif

#undef DRAWMETHOD
#undef DRAWMETHOD_NORMAL
#undef DRAWMETHOD_OR
#undef DRAWMETHOD_PLUS
#undef DRAWMETHOD_PLUS_C

#endif // DRAWMETHODS_H
