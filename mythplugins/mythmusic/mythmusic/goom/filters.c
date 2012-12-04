/* filter.c version 0.7
* contient les filtres applicable a un buffer
* creation : 01/10/2000
*  -ajout de sinFilter()
*  -ajout de zoomFilter()
*  -copie de zoomFilter() en zoomFilterRGB(), gérant les 3 couleurs
*  -optimisation de sinFilter (utilisant une table de sin)
*	-asm
*	-optimisation de la procedure de génération du buffer de transformation
*		la vitesse est maintenant comprise dans [0..128] au lieu de [0..100]
*/

//#define _DEBUG_PIXEL;

#include "filters.h"
#include "graphic.h"
#include "goom_tools.h"
#include <stdlib.h>
#include <math.h>
#include <stdio.h>
#include <inttypes.h>

#ifdef MMX
#define USE_ASM
#endif
#ifdef POWERPC
#define USE_ASM
#endif

#define EFFECT_DISTORS 4
#define EFFECT_DISTORS_SL 2

extern volatile guint32 resolx;
extern volatile guint32 c_resoly;

void c_zoom (unsigned int *expix1, unsigned int *expix2, unsigned int prevX, unsigned int prevY, signed int *brutS, signed int *brutD);

#ifdef MMX

void    zoom_filter_xmmx (int prevX, int prevY, unsigned int *expix1, unsigned int *expix2, int *brutS, int *brutD, int buffratio, int precalCoef[16][16]);
int 	zoom_filter_xmmx_supported (void);
void    zoom_filter_mmx (int prevX, int prevY, unsigned int *expix1, unsigned int *expix2, int *brutS, int *brutD, int buffratio, int precalCoef[16][16]);
int 	zoom_filter_mmx_supported (void);

static int zf_use_xmmx = 0;
static int zf_use_mmx = 0;

static void select_zoom_filter (void) {
	static int firsttime = 1;
	if (firsttime){
		if (zoom_filter_xmmx_supported()) {
			zf_use_xmmx = 1;
			printf ("Extended MMX detected. Using the fastest method !\n");
		}
		else if (zoom_filter_mmx_supported()) {
			zf_use_mmx = 1;
			printf ("MMX detected. Using fast method !\n");
		}
		else {
			printf ("Too bad ! No MMX detected.\n");
		}
		firsttime = 0;
	}
}

#else /* MMX */

static void select_zoom_filter () {
	static int firsttime = 1;
	if (firsttime) {
		printf ("No MMX support compiled in\n");
		firsttime = 0;
	}
}

#endif /* MMX */


guint32 mmx_zoom_size;

#ifdef USE_ASM

#ifdef POWERPC
#include "altivec.h"
extern unsigned int useAltivec;
extern const void ppc_zoom (unsigned int *frompixmap, unsigned int *topixmap, unsigned int sizex, unsigned int sizey, unsigned int *brutS, unsigned int *brutD, unsigned int buffratio, int precalCoef[16][16]);

#endif /* PowerPC */

#endif /* ASM */


unsigned int *coeffs = 0, *freecoeffs = 0;

signed int *brutS = 0, *freebrutS = 0;	// source
signed int *brutD = 0, *freebrutD = 0;	// dest
signed int *brutT = 0, *freebrutT = 0;	// temp (en cours de génération)

// TODO : virer
guint32 *expix1 = 0;						// pointeur exporte vers p1
guint32 *expix2 = 0;						// pointeur exporte vers p2
// fin TODO

guint32 zoom_width;

unsigned int     prevX = 0, prevY = 0;

static int sintable[0x10000];
static int vitesse = 127;
static char theMode = AMULETTE_MODE;
static int waveEffect = 0;
static int hypercosEffect = 0;
static int vPlaneEffect = 0;
static int hPlaneEffect = 0;
static char noisify = 2;
static int middleX, middleY;

//static unsigned char sqrtperte = 16 ;

/** modif by jeko : fixedpoint : buffration = (16:16) (donc 0<=buffration<=2^16) */
//static int buffratio = 0;
int     buffratio = 0;

#define BUFFPOINTNB 16
#define BUFFPOINTMASK 0xffff
#define BUFFINCR 0xff

#define sqrtperte 16
// faire : a % sqrtperte <=> a & pertemask
#define PERTEMASK 0xf
// faire : a / sqrtperte <=> a >> PERTEDEC
#define PERTEDEC 4

static int *firedec = 0;


// retourne x>>s , en testant le signe de x
#define ShiftRight(_x,_s) (((_x)<0) ? -(-(_x)>>(_s)) : ((_x)>>(_s)))

/** modif d'optim by Jeko : precalcul des 4 coefs résultant des 2 pos */
int     precalCoef[16][16];

/* Prototypes to keep gcc from spewing warnings */
void generatePrecalCoef (void);
void calculatePXandPY (int x, int y, int *px, int *py);
void setPixelRGB (Uint * buffer, Uint x, Uint y, Color c);
void setPixelRGB_ (Uint * buffer, Uint x, Color c);
inline void getPixelRGB (Uint * buffer, Uint x, Uint y, Color * c);
void getPixelRGB_ (Uint * buffer, Uint x, Color * c);

void
generatePrecalCoef ()
{
	static int firstime = 1;

	if (firstime) {
		int     coefh, coefv;

		firstime = 0;

		for (coefh = 0; coefh < 16; coefh++) {

			for (coefv = 0; coefv < 16; coefv++) {
				int     i;
				int     diffcoeffh;
				int     diffcoeffv;

				diffcoeffh = sqrtperte - coefh;
				diffcoeffv = sqrtperte - coefv;

				// coeffs[myPos] = ((px >> PERTEDEC) + prevX * (py >> PERTEDEC)) <<
				// 2;
				if (!(coefh || coefv))
					i = 255;
				else {
					int     i1, i2, i3, i4;

					i1 = diffcoeffh * diffcoeffv;
					i2 = coefh * diffcoeffv;
					i3 = diffcoeffh * coefv;
					i4 = coefh * coefv;
					if (i1)
						i1--;
					if (i2)
						i2--;
					if (i3)
						i3--;
					if (i4)
						i4--;

					i = (i1) | (i2 << 8) | (i3 << 16) | (i4 << 24);
				}
				precalCoef[coefh][coefv] = i;
			}
		}
	}
}

/*
 calculer px et py en fonction de x,y,middleX,middleY et theMode
 px et py indique la nouvelle position (en sqrtperte ieme de pixel)
 (valeur * 16)
 */
/*inline*/ void
calculatePXandPY (int x, int y, int *px, int *py)
{
	if (theMode == WATER_MODE) {
		static int wave = 0;
		static int wavesp = 0;
		int     yy;

		yy = y + RAND () % 4 - RAND () % 4 + wave / 10;
		if (yy < 0)
			yy = 0;
		if (yy >= (int)c_resoly)
			yy = c_resoly - 1;

		*px = (x << 4) + firedec[yy] + (wave / 10);
		*py = (y << 4) + 132 - ((vitesse < 131) ? vitesse : 130);

		wavesp += RAND () % 3 - RAND () % 3;
		if (wave < -10)
			wavesp += 2;
		if (wave > 10)
			wavesp -= 2;
		wave += (wavesp / 10) + RAND () % 3 - RAND () % 3;
		if (wavesp > 100)
			wavesp = (wavesp * 9) / 10;
	}
	else {
		int     dist = 0, vx9, vy9;
	  int     vx, vy;
		int     ppx, ppy;
		int     fvitesse = vitesse << 4;

		if (noisify) {
			x += RAND () % noisify - RAND () % noisify;
			y += RAND () % noisify - RAND () % noisify;
		}
		vx = (x - middleX) << 9;
		vy = (y - middleY) << 9;

		if (hPlaneEffect)
			vx += hPlaneEffect * (y - middleY);
		// else vx = (x - middleX) << 9 ;

		if (vPlaneEffect)
			vy += vPlaneEffect * (x - middleX);
		// else vy = (y - middleY) << 9 ;

		if (waveEffect) {
			fvitesse *=
				1024 +
				ShiftRight (sintable
										[(unsigned short) (dist * 0xffff + EFFECT_DISTORS)], 6);
			fvitesse /= 1024;
		}

		if (hypercosEffect) {
			vx += ShiftRight (sintable[(-vy + dist) & 0xffff], 1);
			vy += ShiftRight (sintable[(vx + dist) & 0xffff], 1);
		}

		vx9 = ShiftRight (vx, 9);
		vy9 = ShiftRight (vy, 9);
		dist = vx9 * vx9 + vy9 * vy9;

		switch (theMode) {
		case WAVE_MODE:
			fvitesse *=
				1024 +
				ShiftRight (sintable
										[(unsigned short) (dist * 0xffff * EFFECT_DISTORS)], 6);
			fvitesse>>=10;///=1024;
			break;
		case CRYSTAL_BALL_MODE:
			fvitesse += (dist >> (10-EFFECT_DISTORS_SL));
			break;
		case AMULETTE_MODE:
			fvitesse -= (dist >> (4 - EFFECT_DISTORS_SL));
			break;
		case SCRUNCH_MODE:
			fvitesse -= (dist >> (10 - EFFECT_DISTORS_SL));
			break;
		case HYPERCOS1_MODE:
			vx = vx + ShiftRight (sintable[(-vy + dist) & 0xffff], 1);
			vy = vy + ShiftRight (sintable[(vx + dist) & 0xffff], 1);
			break;
		case HYPERCOS2_MODE:
			vx =
				vx + ShiftRight (sintable[(-ShiftRight (vy, 1) + dist) & 0xffff], 0);
			vy =
				vy + ShiftRight (sintable[(ShiftRight (vx, 1) + dist) & 0xffff], 0);
			fvitesse = 128 << 4;
			break;
		case YONLY_MODE:
			fvitesse *= 1024 + ShiftRight (sintable[vy & 0xffff], 6);
			fvitesse >>= 10;
			break;
		case SPEEDWAY_MODE:
			fvitesse -= (ShiftRight(vy,10-EFFECT_DISTORS_SL));
			break;
		}

		if (fvitesse < -3024)
			fvitesse = -3024;

		if (vx < 0)									// pb avec decalage sur nb negatif
			ppx = -(-(vx * fvitesse) >> 16);
		/* 16 = 9 + 7 (7 = nb chiffre virgule de vitesse * (v = 128 => immobile)
		 * * * * * 9 = nb chiffre virgule de vx) */
		else
			ppx = ((vx * fvitesse) >> 16);

		if (vy < 0)
			ppy = -(-(vy * fvitesse) >> 16);
		else
			ppy = ((vy * fvitesse) >> 16);

		*px = (middleX << 4) + ppx;
		*py = (middleY << 4) + ppy;
	}
}

//#define _DEBUG

/*inline*/ void
setPixelRGB (Uint * buffer, Uint x, Uint y, Color c)
{
	// buffer[ y*WIDTH + x ] = (c.r<<16)|(c.v<<8)|c.b
#ifdef _DEBUG_PIXEL
	if (x + y * resolx >= resolx * resoly) {
		fprintf (stderr, "setPixel ERROR : hors du tableau... %i, %i\n", x, y);
		// exit (1) ;
	}
#endif

	buffer[y * resolx + x] =
		(c.b << (BLEU * 8)) | (c.v << (VERT * 8)) | (c.r << (ROUGE * 8));
}


/*inline*/ void
setPixelRGB_ (Uint * buffer, Uint x, Color c)
{
#ifdef _DEBUG
	if (x >= resolx * c_resoly) {
		printf ("setPixel ERROR : hors du tableau... %i\n", x);
		// exit (1) ;
	}
#endif

	buffer[x] = (c.r << (ROUGE * 8)) | (c.v << (VERT * 8)) | c.b << (BLEU * 8);
}



inline void
getPixelRGB (Uint * buffer, Uint x, Uint y, Color * c)
{
//    register unsigned char *tmp8;
	unsigned int i;

#ifdef _DEBUG
	if (x + y * resolx >= resolx * c_resoly) {
		printf ("getPixel ERROR : hors du tableau... %i, %i\n", x, y);
		// exit (1) ;
	}
#endif

	/* ATTENTION AU PETIT INDIEN  */
	i = *(buffer + (x + y * resolx));
	c->b = (i >> (BLEU * 8)) & 0xff;
	c->v = (i >> (VERT * 8)) & 0xff;
	c->r = (i >> (ROUGE * 8)) & 0xff;
}


/*inline*/ void
getPixelRGB_ (Uint * buffer, Uint x, Color * c)
{
	register unsigned char *tmp8;

#ifdef _DEBUG
	if (x >= resolx * c_resoly) {
		printf ("getPixel ERROR : hors du tableau... %i\n", x);
		// exit (1) ;
	}
#endif

#ifdef __BIG_ENDIAN__
	c->b = *(unsigned char *) (tmp8 = (unsigned char *) (buffer + x));
	c->r = *(unsigned char *) (++tmp8);
	c->v = *(unsigned char *) (++tmp8);
	c->b = *(unsigned char *) (++tmp8);

#else
	/* ATTENTION AU PETIT INDIEN  */
	c->b = *(unsigned char *) (tmp8 = (unsigned char *) (buffer + x));
	c->v = *(unsigned char *) (++tmp8);
	c->r = *(unsigned char *) (++tmp8);
	// *c = (Color) buffer[x+y*WIDTH] ;
#endif
}


void c_zoom (unsigned int *expix1, unsigned int *expix2, unsigned int prevX, unsigned int prevY, signed int *brutS, signed int *brutD)
{
	int     myPos, myPos2;
	Color   couleur;
//	unsigned int coefv, coefh;

	unsigned int ax = (prevX - 1) << PERTEDEC, ay = (prevY - 1) << PERTEDEC;

	int     bufsize = prevX * prevY * 2;
	int     bufwidth = prevX;

	expix1[0]=expix1[prevX-1]=expix1[prevX*prevY-1]=expix1[prevX*prevY-prevX]=0;

	for (myPos = 0; myPos < bufsize; myPos += 2) {
		Color   col1, col2, col3, col4;
		int     c1, c2, c3, c4, px, py;
		int     pos;
		int     coeffs;

		int     brutSmypos = brutS[myPos];

		myPos2 = myPos + 1;

		px = brutSmypos + (((brutD[myPos] - brutSmypos) * buffratio) >> BUFFPOINTNB);
		brutSmypos = brutS[myPos2];
		py = brutSmypos + (((brutD[myPos2] - brutSmypos) * buffratio) >> BUFFPOINTNB);

                if (px < 0)
                    px = 0;
                if (py < 0)
                    py = 0;

		pos = ((px >> PERTEDEC) + prevX * (py >> PERTEDEC));
		// coef en modulo 15
		coeffs = precalCoef[px & PERTEMASK][py & PERTEMASK];

		if ((py >= (int)ay) || (px >= (int)ax)) {
			pos = coeffs = 0;
		}
		
		getPixelRGB_ (expix1, pos, &col1);
		getPixelRGB_ (expix1, pos + 1, &col2);
		getPixelRGB_ (expix1, pos + bufwidth, &col3);
		getPixelRGB_ (expix1, pos + bufwidth + 1, &col4);

		c1 = coeffs;
		c2 = (c1 & 0x0000ff00) >> 8;
		c3 = (c1 & 0x00ff0000) >> 16;
		c4 = (c1 & 0xff000000) >> 24;
		c1 = c1 & 0xff;

		couleur.r = col1.r * c1 + col2.r * c2 + col3.r * c3 + col4.r * c4;
		if (couleur.r > 5)
			couleur.r -= 5;
		couleur.r >>= 8;

		couleur.v = col1.v * c1 + col2.v * c2 + col3.v * c3 + col4.v * c4;
		if (couleur.v > 5)
			couleur.v -= 5;
		couleur.v >>= 8;

		couleur.b = col1.b * c1 + col2.b * c2 + col3.b * c3 + col4.b * c4;
		if (couleur.b > 5)
			couleur.b -= 5;
		couleur.b >>= 8;

		setPixelRGB_ (expix2, myPos >> 1, couleur);
	}
}

#ifdef USE_ASM
void setAsmUse (int useIt);
int getAsmUse (void);

static int use_asm = 1;
void
setAsmUse (int useIt)
{
	use_asm = useIt;
}

int
getAsmUse ()
{
	return use_asm;
}
#endif

/*===============================================================*/
void
zoomFilterFastRGB (Uint * pix1, Uint * pix2, ZoomFilterData * zf, Uint resx, Uint resy, int switchIncr, float switchMult)
{
	register Uint x, y;

	static char reverse = 0;			// vitesse inversé..(zoom out)
	static unsigned char pertedec = 8;
	static char firstTime = 1;

#define INTERLACE_INCR 16
#define INTERLACE_ADD 9
#define INTERLACE_AND 0xf
	static int interlace_start = -2;

	expix1 = pix1;
	expix2 = pix2;

	/** changement de taille **/
	if ((prevX != resx) || (prevY != resy)) {
		prevX = resx;
		prevY = resy;

		if (brutS)
			free (freebrutS);
		brutS = 0;
		if (brutD)
			free (freebrutD);
		brutD = 0;
		if (brutT)
			free (freebrutT);
		brutT = 0;

		middleX = resx / 2;
		middleY = resy - 1;
		firstTime = 1;
		if (firedec)
			free (firedec);
		firedec = 0;
	}

	if (interlace_start != -2)
		zf = NULL;

	/** changement de config **/
	if (zf) {
		reverse = zf->reverse;
		vitesse = zf->vitesse;
		if (reverse)
			vitesse = 256 - vitesse;
		pertedec = zf->pertedec;
		middleX = zf->middleX;
		middleY = zf->middleY;
		theMode = zf->mode;
		hPlaneEffect = zf->hPlaneEffect;
		vPlaneEffect = zf->vPlaneEffect;
		waveEffect = zf->waveEffect;
		hypercosEffect = zf->hypercosEffect;
		noisify = zf->noisify;
	}

	/* Silence a gcc warning */
	(void)pertedec;

	/** generation d'un effet **/
	if (firstTime || zf) {

		// generation d'une table de sinus
		if (firstTime) {
			unsigned short us;
			int     yofs;

			firstTime = 0;
			generatePrecalCoef ();
			select_zoom_filter ();

			freebrutS = (signed int *) calloc (resx * resy * 2 + 128, sizeof(unsigned int));
			brutS = (signed int *) ((1 + ((uintptr_t) (freebrutS)) / 128) * 128);

			freebrutD = (signed int *) calloc (resx * resy * 2 + 128, sizeof(unsigned int));
			brutD = (signed int *) ((1 + ((uintptr_t) (freebrutD)) / 128) * 128);

			freebrutT = (signed int *) calloc (resx * resy * 2 + 128, sizeof(unsigned int));
			brutT = (signed int *) ((1 + ((uintptr_t) (freebrutT)) / 128) * 128);

			/** modif here by jeko : plus de multiplications **/
			{
				int     yperte = 0;

				for (y = 0, yofs = 0; y < resy; y++, yofs += resx) {
					int     xofs = yofs << 1;
					int     xperte = 0;

					for (x = 0; x < resx; x++) {
						brutS[xofs++] = xperte;
						brutS[xofs++] = yperte;
						xperte += sqrtperte;
					}
					yperte += sqrtperte;
				}
				buffratio = 0;
			}

			for (us = 0; us < 0xffff; us++) {
				sintable[us] = (int) (1024 * sin ((double) us * 360 / (sizeof (sintable) / sizeof (sintable[0]) - 1) * 3.141592 / 180) + .5);
			}

			{
				int     loopv;
				firedec = (int *) malloc (prevY * sizeof (int));

				for (loopv = prevY; loopv != 0;) {
					static int decc = 0;
					static int spdc = 0;
					static int accel = 0;

					loopv--;
					firedec[loopv] = decc;
					decc += spdc / 10;
					spdc += RAND () % 3 - RAND () % 3;

					if (decc > 4)
						spdc -= 1;
					if (decc < -4)
						spdc += 1;

					if (spdc > 30)
						spdc = spdc - RAND () % 3 + accel / 10;
					if (spdc < -30)
						spdc = spdc + RAND () % 3 + accel / 10;

					if (decc > 8 && spdc > 1)
						spdc -= RAND () % 3 - 2;

					if (decc < -8 && spdc < -1)
						spdc += RAND () % 3 + 2;

					if (decc > 8 || decc < -8)
						decc = decc * 8 / 9;

					accel += RAND () % 2 - RAND () % 2;
					if (accel > 20)
						accel -= 2;
					if (accel < -20)
						accel += 2;
				}
			}
		}

        interlace_start = 0;
        }
		// generation du buffer de trans
        if (interlace_start==-1) {

			/* sauvegarde de l'etat actuel dans la nouvelle source */

			y = prevX * prevY * 2;
			for (x = 0; x < y; x += 2) {
				int     brutSmypos = brutS[x];
				int     x2 = x + 1;
				
				brutS[x] =
					brutSmypos + (((brutD[x] - brutSmypos) * buffratio) >> BUFFPOINTNB);
				brutSmypos = brutS[x2];
				brutS[x2] =
					brutSmypos +
					(((brutD[x2] - brutSmypos) * buffratio) >> BUFFPOINTNB);
			}
			buffratio = 0;
        }
	
        if (interlace_start==-1) {
            signed int * tmp;
            tmp = brutD;
            brutD=brutT;
            brutT=tmp;
            tmp = freebrutD;
            freebrutD=freebrutT;
            freebrutT=tmp;
            interlace_start = -2;
        }

	if (interlace_start>=0) {
            int maxEnd = (interlace_start+INTERLACE_INCR);
		/* creation de la nouvelle destination */
		for (y = (Uint)interlace_start; (y < (Uint)prevY) && (y < (Uint)maxEnd); y++) {
			Uint premul_y_prevX = y * prevX * 2;
			for (x = 0; x < prevX; x++) {
				int     px, py;
				
				calculatePXandPY (x, y, &px, &py);
				
				brutT[premul_y_prevX] = px;
				brutT[premul_y_prevX + 1] = py;
				premul_y_prevX += 2;
			}
		}
		interlace_start += INTERLACE_INCR;
		if (y >= prevY-1) interlace_start = -1;
	}

	if (switchIncr != 0) {
		buffratio += switchIncr;
		if (buffratio > BUFFPOINTMASK)
			buffratio = BUFFPOINTMASK;
	}

	if (switchMult != 1.0f) {
		buffratio =
			(int) ((float) BUFFPOINTMASK * (1.0f - switchMult) +
						 (float) buffratio * switchMult);
	}

	zoom_width = prevX;
	mmx_zoom_size = prevX * prevY;

#ifdef USE_ASM
#ifdef MMX
	if (zf_use_xmmx)
		zoom_filter_xmmx (prevX, prevY,expix1, expix2,
											brutS, brutD, buffratio, precalCoef);
	else if (zf_use_mmx)
		zoom_filter_mmx (prevX, prevY,expix1, expix2,
										 brutS, brutD, buffratio, precalCoef);
	else c_zoom (expix1, expix2, prevX, prevY, brutS, brutD);
#endif

#ifdef POWERPC
	ppc_zoom (expix1, expix2, prevX, prevY, brutS, brutD, buffratio,precalCoef);
#endif
#else
	c_zoom (expix1, expix2, prevX, prevY, brutS, brutD);
#endif
}

void
pointFilter (Uint * pix1, Color c, float t1, float t2, float t3, float t4, Uint cycle)
{
	Uint    x = (Uint) ((int) (resolx/2) + (int) (t1 * cos ((float) cycle / t3)));
	Uint    y = (Uint) ((int) (c_resoly/2) + (int) (t2 * sin ((float) cycle / t4)));

	if ((x > 1) && (y > 1) && (x < resolx - 2) && (y < c_resoly - 2)) {
		setPixelRGB (pix1, x + 1, y, c);
		setPixelRGB (pix1, x, y + 1, c);
		setPixelRGB (pix1, x + 1, y + 1, WHITE);
		setPixelRGB (pix1, x + 2, y + 1, c);
		setPixelRGB (pix1, x + 1, y + 2, c);
	}
}
