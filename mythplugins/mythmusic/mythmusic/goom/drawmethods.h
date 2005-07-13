#ifndef _DRAWMETHODS_H
#define _DRAWMETHODS_H

#include "goomconfig.h"

#define DRAWMETHOD_NORMAL(adr,col) {*(adr) = (col);}

#ifdef MMX
#include "mmx.h"

#define DRAWMETHOD_PLUS(_out,_backbuf,_col) \
{\
movd_m2r (_backbuf, mm0); \
paddusb_m2r (_col, mm0); \
movd_r2m (mm0, _out); \
}

#else
#define DRAWMETHOD_PLUS(_out,_backbuf,_col) \
{\
      int tra=0,i=0;\
      unsigned char *bra = (unsigned char*)&(_backbuf);\
      unsigned char *dra = (unsigned char*)&(_out);\
      unsigned char *cra = (unsigned char*)&(_col);\
      for (;i<4;i++) {\
				tra = *cra;\
				tra += *bra;\
				if (tra>255) tra=255;\
				*dra = tra;\
				++dra;++cra;++bra;\
			}\
}
#endif

#define DRAWMETHOD_OR(adr,col) {*(adr)|=(col);}

#ifdef MMX
#define DRAWMETHOD_DONE() {__asm__ __volatile__ ("emms");}
#else
#define DRAWMETHOD_DONE() {}
#endif

#ifndef DRAWMETHOD
#define DRAWMETHOD DRAWMETHOD_PLUS(*p,*p,col)

static void draw_line (int *data, int x1, int y1, int x2, int y2, int col, int screenx, int screeny) {
    int     x, y, dx, dy, yy, xx;	// am, tmp;
	int    *p;


	if ((y1 < 0) || (y2 < 0) || (x1 < 0) || (x2 < 0) || (y1 >= screeny) || (y2 >= screeny) || (x1 >= screenx) || (x2 >= screenx)) 
			return;
        
	dx = x2 - x1;
	dy = y2 - y1;
	if (x1 > x2) {
		int     tmp;

		tmp = x1;
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
		else {
			p = &(data[(screenx * y1) + x2]);
			for (x = x2; x <= x1; x++) {
				DRAWMETHOD;
				p++;
			}
			return;
		}
	}
	/* 1    */
	
	/* 2 */
	if (y2 > y1) {
		/* steep */
		if (dy > dx) {
			dx = ((dx << 16) / dy);
			x = x1 << 16;
			for (y = y1; y <= y2; y++) {
				xx = x >> 16;
				p = &(data[(screenx * y) + xx]);
				DRAWMETHOD;
				if (xx < (screenx - 1)) {
					p++;
				}
				x += dx;
			}
			return;
		}
		/* shallow */
		else {
			dy = ((dy << 16) / dx);
			y = y1 << 16;
			for (x = x1; x <= x2; x++) {
				yy = y >> 16;
				p = &(data[(screenx * yy) + x]);
				DRAWMETHOD;
				if (yy < (screeny - 1)) {
					p += screeny;
				}
				y += dy;
			}
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
				xx = x >> 16;
				p = &(data[(screenx * y) + xx]);
				DRAWMETHOD;
				if (xx < (screenx - 1)) {
					p--;
				}
				x += dx;
			}
			return;
		}
		/* shallow */
		else {
			dy = ((dy << 16) / dx);
			y = y1 << 16;
			for (x = x1; x <= x2; x++) {
				yy = y >> 16;
				p = &(data[(screenx * yy) + x]);
				DRAWMETHOD;
				if (yy < (screeny - 1)) {
					p += screeny;
				}
				y += dy;
			}
			return;
		}
	}
}
#endif

#endif
