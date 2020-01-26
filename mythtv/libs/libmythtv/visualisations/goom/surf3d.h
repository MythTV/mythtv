#ifndef SURF3D_H
#define SURF3D_H

#include "v3d.h"

struct surf3d {
	v3d *vertex;
	v3d *svertex;
	int nbvertex;

	v3d center;
};

struct grid3d {
	surf3d surf;
	
	int defx;
	int sizex;
	int defz;
	int sizez;
	int mode;
};

/* hi-level */

/* works on grid3d */
grid3d *grid3d_new (int sizex, int defx, int sizez, int defz, v3d center);
void grid3d_free(grid3d **grid);
void grid3d_update (grid3d *g, float angle, const float *vals, float dist);

/* low level */
void surf3d_draw (surf3d *s, int color, int dist, int *buf, int *back, int W,int H);
void grid3d_draw (grid3d *g, int color, int colorlow, int dist, int *buf, int *back, int W,int H);
void surf3d_rotate (surf3d *s, float angle);
void surf3d_translate (surf3d *s);

#endif // SURF3D_H
