#include "surf3d.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

void grid3d_free(grid3d **grid)
{
    free ((*grid)->surf.vertex);
    free ((*grid)->surf.svertex);
    free (*grid);
    *grid = NULL;
}

grid3d *grid3d_new (int sizex, int defx, int sizez, int defz, v3d center) {
	int x = defx;
	int y = defz;
	grid3d *g = (grid3d*)malloc (sizeof(grid3d));
	surf3d *s = &(g->surf);
	s->nbvertex = x*y;
	s->vertex = (v3d*)malloc (x*y*sizeof(v3d));
	s->svertex = (v3d*)malloc(x*y*sizeof(v3d));
	s->center = center;
	
	g->defx=defx;
	g->sizex=sizex;
	g->defz=defz;
	g->sizez=sizez;
	g->mode=0;

	while (y) {
		--y;
		x = defx;
		while (x) {
			--x;
			s->vertex[x+defx*y].x = (float)(x-defx/2)*sizex/defx;
			s->vertex[x+defx*y].y = 0;
			s->vertex[x+defx*y].z = (float)(y-defz/2)*sizez/defz;
		}
	}
	return g;
}

//#undef HAVE_MMX
#include "drawmethods.h"

void surf3d_draw (surf3d *s, int color, int dist, int *buf, int *back, int W,int H) {
	int i;
	int *p1;
	int *p2;
	v2d v2;
	
	for (i=0;i<s->nbvertex;i++) {
		V3D_TO_V2D(s->svertex[i],v2,W,H,dist);
		p1 = buf + v2.x + (v2.y*W);
		p2 = back + v2.x + (v2.y*W);
		if ((v2.x>=0) && (v2.y>=0) && (v2.x<W) && (v2.y<H)) {
			*p1 = color;
		}
	}

	/* Squelch a gcc warning */
	(void)p2;
}

void grid3d_draw (grid3d *g, int color, int colorlow,
									int dist, int *buf, int *back, int W,int H) {
	int x;
//	int *p1;
//	int *p2;
	v2d v2,v2x;
	
	for (x=0;x<g->defx;x++) {
		int z;
		V3D_TO_V2D(g->surf.svertex[x],v2x,W,H,dist);

		for (z=1;z<g->defz;z++) {
			V3D_TO_V2D(g->surf.svertex[z*g->defx + x],v2,W,H,dist);
			if (((v2.x != -666) || (v2.y!=-666))
					&& ((v2x.x != -666) || (v2x.y!=-666))) {
				draw_line(buf,v2x.x,v2x.y,v2.x,v2.y, colorlow, W, H);
				draw_line(back,v2x.x,v2x.y,v2.x,v2.y, color, W, H);
				DRAWMETHOD_DONE();
			}
			v2x = v2;
		}
	}
}

void surf3d_rotate (surf3d *s, float angle) {
	int i;
	float cosa;
	float sina;
	SINCOS(angle,sina,cosa);
	for (i=0;i<s->nbvertex;i++) {
		Y_ROTATE_V3D(s->vertex[i],s->svertex[i],cosa,sina);
	}
}

void surf3d_translate (surf3d *s) {
	int i;
	for (i=0;i<s->nbvertex;i++) {
		TRANSLATE_V3D(s->center,s->svertex[i]);
	}
}

void grid3d_update (grid3d *g, float angle, float *vals, float dist) {
	int i;
	float cosa;
	float sina;
	surf3d *s = &(g->surf);
	v3d cam = s->center;
	cam.z += dist;

	SINCOS((angle/4.3f),sina,cosa);
	cam.y += sina*2.0f;
	SINCOS(angle,sina,cosa);

	if (g->mode==0) {
		if (vals)
			for (i=0;i<g->defx;i++)
				s->vertex[i].y = s->vertex[i].y*0.2 + vals[i]*0.8;

		for (i=g->defx;i<s->nbvertex;i++) {
			s->vertex[i].y *= 0.255f;
			s->vertex[i].y += (s->vertex[i-g->defx].y * 0.777f);
		}
	}

	for (i=0;i<s->nbvertex;i++) {
		Y_ROTATE_V3D(s->vertex[i],s->svertex[i],cosa,sina);
		TRANSLATE_V3D(cam,s->svertex[i]);
	}
}
