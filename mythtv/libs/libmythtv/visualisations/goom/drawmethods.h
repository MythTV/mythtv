#ifndef DRAWMETHODS_H
#define DRAWMETHODS_H

#include "goomconfig.h"
#include "libmythbase/mythconfig.h"

#define DRAWMETHOD_NORMAL(adr,col) {*(adr) = (col);}

#if HAVE_MMX
#include "mmx.h"

#define DRAWMETHOD_PLUS(_dest,_col) \
{\
movd_m2r (_dest, mm0); \
paddusb_m2r (_col, mm0); \
movd_r2m (mm0, _dest); \
}

#else
#define DRAWMETHOD_PLUS(_dest,_col) \
{\
	unsigned char *dra = (unsigned char*)&(_dest);\
	unsigned char *cra = (unsigned char*)&(_col);\
	for (int i = 0; i < 4; i++) {\
		dra[i] += cra[i];\
		if (dra[i] < cra[i]) dra[i] = 255U;\
	}\
}
#endif

#define DRAWMETHOD_OR(adr,col) {*(adr)|=(col);}

#if HAVE_MMX
#define DRAWMETHOD_DONE() {__asm__ __volatile__ ("emms");}
#else
#define DRAWMETHOD_DONE() {}
#endif

#ifndef DRAWMETHOD
#define DRAWMETHOD DRAWMETHOD_PLUS(*p,col)

static void draw_line (int *data, int x1, int y1, int x2, int y2, int col, int screenx, int screeny) {
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

#endif // DRAWMETHODS_H
