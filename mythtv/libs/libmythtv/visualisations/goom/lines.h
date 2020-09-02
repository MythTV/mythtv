/*
 *  lines.h
 *  iGoom
 *
 *  Created by guillaum on Tue Aug 14 2001.
 *  Copyright (c) 2001 ios. All rights reserved.
 */

#include "graphic.h"
#include "goomconfig.h"
#include "goom_core.h"

struct GMUnitPointer
{
	float   x;
	float   y;
	float   angle;
};

// tableau de points
struct GMLine
{
	GMUnitPointer *points;
	GMUnitPointer *points2;
	int     IDdest;
	float   param;
	float   amplitudeF;
	float   amplitude;

	int     nbPoints;
	guint32 color;
	guint32 color2;

	int     screenX;
	int     screenY;

	float   power;
	float   powinc;
};

// les ID possibles
#define GML_CIRCLE 0
// (param = radius)

#define GML_HLINE 1
// (param = y)

#define GML_VLINE 2
// (param = x)

// les modes couleur possible (si tu mets un autre c'est noir)

#define GML_BLEUBLANC 0
#define GML_RED 1
#define GML_ORANGE_V 2
#define GML_ORANGE_J 3
#define GML_VERT 4
#define GML_BLEU 5
#define GML_BLACK 6

/* construit un effet de line (une ligne horitontale pour commencer) */
GMLine *goom_lines_init (int rx, int ry, int IDsrc, float paramS, int coulS, int IDdest, float paramD, int coulD);

void    goom_lines_switch_to (GMLine * gml, int IDdest, float param, float amplitude, int col);

void    goom_lines_set_res (GMLine * gml, int rx, int ry);

void    goom_lines_free (GMLine ** gml);

void    goom_lines_draw (GMLine * line, const GoomSingleData& data, unsigned int *p);
