/*
 *  lines.h
 *  iGoom
 *
 *  Created by guillaum on Tue Aug 14 2001.
 *  Copyright (c) 2001 ios. All rights reserved.
 */
#include <cstdint>

#include "graphic.h"
#include "goom_core.h"

struct GMUnitPointer
{
	float   x;
	float   y;
	float   angle;
};

// tableau de points
static const int LINENUMPOINTS { 512 };
using GMUnitArray = std::array<GMUnitPointer,LINENUMPOINTS>;
struct GMLine
{
	GMUnitArray points;
	GMUnitArray points2;
	int     IDdest;
	float   param;
	float   amplitudeF;
	float   amplitude;

	uint32_t color;
	uint32_t color2;

	int     screenX;
	int     screenY;

	float   power;
	float   powinc;
};

enum GML_TYPE : uint8_t {
// les ID possibles
    GML_CIRCLE = 0,
// (param = radius)

    GML_HLINE = 1,
// (param = y)

    GML_VLINE = 2,
// (param = x)
};

// les modes couleur possible (si tu mets un autre c'est noir)

enum GML_COLOR : uint8_t {
    GML_BLEUBLANC = 0,
    GML_RED = 1,
    GML_ORANGE_V = 2,
    GML_ORANGE_J = 3,
    GML_VERT = 4,
    GML_BLEU = 5,
    GML_BLACK = 6,
};

/* construit un effet de line (une ligne horitontale pour commencer) */
GMLine *goom_lines_init (int rx, int ry, int IDsrc, float paramS, int coulS, int IDdest, float paramD, int coulD);

void    goom_lines_switch_to (GMLine * gml, int IDdest, float param, float amplitude, int col);

void    goom_lines_set_res (GMLine * gml, int rx, int ry);

void    goom_lines_free (GMLine ** gml);

void    goom_lines_draw (GMLine * line, const GoomSingleData& data, unsigned int *p);
