#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "surf3d.h"

void grid3d_free(grid3d **grid)
{
    free ((*grid)->surf.vertex);
    free ((*grid)->surf.svertex);
    free (*grid);
    *grid = nullptr;
}

grid3d *grid3d_new (int sizex, int defx, int sizez, int defz, v3d center) {
	int x = defx;
	int y = defz;
	auto *g = (grid3d*)malloc (sizeof(grid3d));
	surf3d *s = &(g->surf);
	s->nbvertex = x*y;
	s->vertex = (v3d*)malloc (sizeof(v3d)*x*y);
	s->svertex = (v3d*)malloc(sizeof(v3d)*x*y);
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
			s->vertex[x+(defx*y)].x = (x-defx/2.0F)*sizex/defx;
			s->vertex[x+(defx*y)].y = 0;
			s->vertex[x+(defx*y)].z = (y-defz/2.0F)*sizez/defz;
		}
	}
	return g;
}

//#undef HAVE_MMX
#include "drawmethods.h"

void surf3d_draw (surf3d *s, int color, int dist, int *buf, int *back, int W,int H) {
	v2d v2 {};
	
	for (int i=0;i<s->nbvertex;i++) {
		V3D_TO_V2D(s->svertex[i],v2,W,H,dist);
		int *p1 = buf + v2.x + (v2.y*static_cast<ptrdiff_t>(W));
		[[maybe_unused]] int *p2 = back + v2.x + (v2.y*static_cast<ptrdiff_t>(W));
		if ((v2.x>=0) && (v2.y>=0) && (v2.x<W) && (v2.y<H)) {
			*p1 = color;
		}
	}
}

void grid3d_draw (grid3d *g, int color, int colorlow,
									int dist, int *buf, int *back, int W,int H) {
	v2d v2  {};
	v2d v2x {};
	
	for (int x=0;x<g->defx;x++) {
		V3D_TO_V2D(g->surf.svertex[x],v2x,W,H,dist);

		for (int z=1;z<g->defz;z++) {
			V3D_TO_V2D(g->surf.svertex[(z*g->defx) + x],v2,W,H,dist);
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
	float cosa = NAN;
	float sina = NAN;
	SINCOS(angle,sina,cosa);
	for (int i=0;i<s->nbvertex;i++) {
		Y_ROTATE_V3D(s->vertex[i],s->svertex[i],cosa,sina);
	}
}

void surf3d_translate (surf3d *s) {
	for (int i=0;i<s->nbvertex;i++) {
		TRANSLATE_V3D(s->center,s->svertex[i]);
	}
}

void grid3d_update (grid3d *g, float angle, const float *vals, float dist) {
	float cosa = NAN;
	float sina = NAN;
	surf3d *s = &(g->surf);
	v3d cam = s->center;
	cam.z += dist;

	SINCOS((angle/4.3F),sina,cosa);
	cam.y += sina*2.0F;
	SINCOS(angle,sina,cosa);

	if (g->mode==0) {
		if (vals)
			for (int i=0;i<g->defx;i++)
				s->vertex[i].y = (s->vertex[i].y*0.2F) + (vals[i]*0.8F);

		for (int i=g->defx;i<s->nbvertex;i++) {
			s->vertex[i].y *= 0.255F;
			s->vertex[i].y += (s->vertex[i-g->defx].y * 0.777F);
		}
	}

	for (int i=0;i<s->nbvertex;i++) {
		Y_ROTATE_V3D(s->vertex[i],s->svertex[i],cosa,sina);
		TRANSLATE_V3D(cam,s->svertex[i]);
	}
}
