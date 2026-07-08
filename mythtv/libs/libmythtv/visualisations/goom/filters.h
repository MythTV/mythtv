#ifndef FILTERS_H
#define FILTERS_H

#include <cmath>
#include <cstdint>

#include "graphic.h"

struct ZoomFilterData
{
	int     vitesse;		/* 128 = vitesse nule... * * 256 = en arriere
                                         * hyper vite.. * * 0 = en avant hype vite. */
	unsigned char pertedec;
	unsigned char sqrtperte;
	int     middleX, middleY;	/* milieu de l'effet */
	bool    reverse;		/* inverse la vitesse */
	char    mode;			/* type d'effet à appliquer (cf les #define) */
	/** @since June 2001 */
	int     hPlaneEffect;		/* deviation horitontale */
	int     vPlaneEffect;		/* deviation verticale */
	/** @since April 2002 */
	bool    waveEffect;		/* applique une "surcouche" de wave effect */
	bool    hypercosEffect;		/* applique une "surcouche de hypercos effect */

	char    noisify;		/* ajoute un bruit a la transformation */
};

enum FILTER_MODE : uint8_t {
    NORMAL_MODE       = 0,
    WAVE_MODE         = 1,
    CRYSTAL_BALL_MODE = 2,
    SCRUNCH_MODE      = 3,
    AMULETTE_MODE     = 4,
    WATER_MODE        = 5,
    HYPERCOS1_MODE    = 6,
    HYPERCOS2_MODE    = 7,
    YONLY_MODE        = 8,
    SPEEDWAY_MODE     = 9,
};

void    pointFilter (uint32_t * pix1, Color c, float t1, float t2, float t3, float t4, uint32_t cycle);

/* filtre de zoom :
 * le contenu de pix1 est copie dans pix2.
 * zf : si non NULL, configure l'effet.
 * resx,resy : taille des buffers.
 */

void    zoomFilterFastRGB (uint32_t * pix1, uint32_t * pix2, ZoomFilterData * zf, uint32_t resx, uint32_t resy, int switchIncr, float switchMult);

#endif
