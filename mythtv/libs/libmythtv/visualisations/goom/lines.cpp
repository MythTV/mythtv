/*
 *  lines.c
 */

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

#include "lines.h"
#include "drawmethods.h"
#include "goom_core.h"
#include "goom_tools.h"

extern unsigned int resolx, c_resoly;

static inline unsigned char
lighten (unsigned char value, float power)
{
	int     val = value;
	float   t = (float) val * log10f(power) / 2.0F;

	if (t > 0) {
		val = (int) t; // (32.0F * log (t));
		return std::min(val, 255);
	}
        return 0;
}

static void
lightencolor (int *col, float power)
{
	auto *color = (unsigned char *) col;
	*color = lighten (*color, power);
	color++;
	*color = lighten (*color, power);
	color++;
	*color = lighten (*color, power);
	color++;
	*color = lighten (*color, power);
}

static void
genline (int id, float param, GMUnitPointer * l, int rx, int ry)
{
	switch (id) {
	case GML_HLINE:
		for (int i = 0; i < 512; i++) {
			l[i].x = ((float) i * rx) / 512.0F;
			l[i].y = param;
			l[i].angle = M_PI_F / 2.0F;
		}
		return;
	case GML_VLINE:
		for (int i = 0; i < 512; i++) {
			l[i].y = ((float) i * ry) / 512.0F;
			l[i].x = param;
			l[i].angle = 0.0F;
		}
		return;
	case GML_CIRCLE:
		for (int i = 0; i < 512; i++) {
			l[i].angle = 2.0F * M_PI_F * (float) i / 512.0F;
			float cosa = param * cosf (l[i].angle);
			float sina = param * sinf (l[i].angle);
			l[i].x = ((float) rx / 2.0F) + cosa;
			l[i].y = ((float) ry / 2.0F) + sina;
		}
		return;
	}
}

static guint32 getcouleur (int mode)
{
	switch (mode) {
	case GML_RED:
		return (230 << (ROUGE * 8)) | (120 << (VERT * 8)) | (10 << (BLEU * 8));
	case GML_ORANGE_J:
		return (120 << (VERT * 8)) | (252 << (ROUGE * 8)) | (10 << (BLEU * 8));
	case GML_ORANGE_V:
		return (160 << (VERT * 8)) | (236 << (ROUGE * 8)) | (40 << (BLEU * 8));
	case GML_BLEUBLANC:
		return (40 << (BLEU * 8)) | (220 << (ROUGE * 8)) | (140 << (VERT * 8));
	case GML_VERT:
		return (200 << (VERT * 8)) | (80 << (ROUGE * 8)) | (10 << (BLEU * 8));
	case GML_BLEU:
		return (250 << (BLEU * 8)) | (30 << (VERT * 8)) | (80 << (ROUGE * 8));
	case GML_BLACK:
		return 0x5 << (BLEU * 8);
	}
	return 0;
}

void
goom_lines_set_res (GMLine * gml, int rx, int ry)
{
	if (gml != nullptr) {
		//int     i;

		gml->screenX = rx;
		gml->screenY = ry;

		genline (gml->IDdest, gml->param, gml->points2, rx, ry);
	}
}


static void
goom_lines_move (GMLine * l)
{
	for (int i = 0; i < 512; i++) {
		l->points[i].x = (l->points2[i].x + 39.0F * l->points[i].x) / 40.0F;
		l->points[i].y = (l->points2[i].y + 39.0F * l->points[i].y) / 40.0F;
		l->points[i].angle =
			(l->points2[i].angle + 39.0F * l->points[i].angle) / 40.0F;
	}

	auto *c1 = (unsigned char *) &l->color;
	auto *c2 = (unsigned char *) &l->color2;
	for (int i = 0; i < 4; i++) {
		int cc1 = *c1;
		int cc2 = *c2;
		*c1 = (unsigned char) ((cc1 * 63 + cc2) >> 6);
		++c1;
		++c2;
	}

	l->power += l->powinc;
	if (l->power < 1.1F) {
		l->power = 1.1F;
		l->powinc = (float) (iRAND (20) + 10) / 300.0F;
	}
	if (l->power > 17.5F) {
		l->power = 17.5F;
		l->powinc = -(float) (iRAND (20) + 10) / 300.0F;
	}

	l->amplitude = (99.0F * l->amplitude + l->amplitudeF) / 100.0F;
}

void
goom_lines_switch_to (GMLine * gml, int IDdest,
											float param, float amplitude, int col)
{
	genline (IDdest, param, gml->points2, gml->screenX, gml->screenY);
	gml->IDdest = IDdest;
	gml->param = param;
	gml->amplitudeF = amplitude;
	gml->color2 = getcouleur (col);
//  printf ("couleur %d : %x\n",col,gml->color2);
}

GMLine *
goom_lines_init (int rx, int ry,
								 int IDsrc, float paramS, int coulS,
								 int IDdest, float paramD, int coulD)
{
	//int     i;
	//unsigned char *color;
	//unsigned char power = 4;

	auto *l = (GMLine *) malloc (sizeof (GMLine));

	l->points = (GMUnitPointer *) malloc (512 * sizeof (GMUnitPointer));
	l->points2 = (GMUnitPointer *) malloc (512 * sizeof (GMUnitPointer));
	l->nbPoints = 512;

	l->IDdest = IDdest;
	l->param = paramD;
	
	l->amplitude = l->amplitudeF = 1.0F;

	genline (IDsrc, paramS, l->points, rx, ry);
	genline (IDdest, paramD, l->points2, rx, ry);

	l->color = getcouleur (coulS);
	l->color2 = getcouleur (coulD);

	l->screenX = rx;
	l->screenY = ry;

	l->power = 0.0F;
	l->powinc = 0.01F;

	goom_lines_switch_to (l, IDdest, paramD, 1.0F, coulD);

	return l;
}

void
goom_lines_free (GMLine ** l)
{
	free ((*l)->points);
	free ((*l)->points2);
	free (*l);
	*l = nullptr;
}

void
goom_lines_draw (GMLine * line, const GoomSingleData& data, unsigned int *p)
{
	if (line != nullptr) {
		guint32 color = line->color;
		GMUnitPointer *pt = &(line->points[0]);

		float   cosa = cosf (pt->angle) / 1000.0F;
		float   sina = sinf (pt->angle) / 1000.0F;

		lightencolor ((int *)&color, line->power);

		int x1 = (int) (pt->x + (cosa * line->amplitude * data[0]));
		int y1 = (int) (pt->y + (sina * line->amplitude * data[0]));

		for (int i = 1; i < 512; i++) {
			pt = &(line->points[i]);

			cosa = cosf (pt->angle) / 1000.0F;
			sina = sinf (pt->angle) / 1000.0F;

			int x2 = (int) (pt->x + (cosa * line->amplitude * data[i]));
			int y2 = (int) (pt->y + (sina * line->amplitude * data[i]));

			draw_line ((int *)p, x1, y1, x2, y2, color, line->screenX, line->screenY);
			DRAWMETHOD_DONE ();

			x1 = x2;
			y1 = y2;
		}
		goom_lines_move (line);
	}
}
