#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "../config.h"
#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif

#include <inttypes.h>

#include "goom_core.h"
#include "goom_tools.h"
#include "filters.h"
#include "lines.h"
#include "ifs.h"
#include "tentacle3d.h"
//#include "gfontlib.h"

//#define VERBOSE

#ifdef VERBOSE
#include <stdio.h>
#endif

#define STOP_SPEED 128

#define TIME_BTW_CHG 300

/**-----------------------------------------------------**
 **  SHARED DATA                                        **
 **-----------------------------------------------------**/
static guint32 *pixel;
static guint32 *back;
static guint32 *p1, *p2, *tmp;
static guint32 cycle;

typedef struct {
	int drawIFS;
	int drawPoints;
	int drawTentacle;

	int drawScope;
	int farScope;

	int rangemin;
	int rangemax;
} GoomState;

#define STATES_NB 8
#define STATES_RANGEMAX 510
GoomState states[STATES_NB] = {
	{1,0,0,1,4, 000, 100},
	{1,0,0,1,1, 101, 140}, // turned on drawScope
	{1,0,0,1,2, 141, 200},
	{0,1,0,1,2, 201, 260},
	{0,1,0,1,0, 261, 330},
	{0,1,1,1,4, 331, 400},
	{0,0,1,1,5, 401, 450}, // turned on drawScope
        {0,0,1,1,1, 451, 510}
};

GoomState *curGState = states+4;

guint32 resolx, resoly, buffsize, c_black_height = 0, c_offset = 0, c_resoly = 0;	/* avec prise en compte de ca */

// effet de ligne..
static GMLine *gmline1 = NULL;
static GMLine *gmline2 = NULL;

void    choose_a_goom_line (float *param1, float *param2, int *couleur, int *mode, float *amplitude, int far);

void goom_init (guint32 resx, guint32 resy, int cinemascope) {
#ifdef VERBOSE
	printf ("GOOM: init (%d, %d);\n", resx, resy);
#endif
	if (cinemascope)
		c_black_height = resy / 5;
	else
		c_black_height = 0;

	resolx = resx;
	resoly = resy;
	buffsize = resx * resy;

	c_offset = c_black_height * resx;
	c_resoly = resy - c_black_height * 2;

	pixel = (guint32 *) malloc (buffsize * sizeof (guint32) + 128);
	back = (guint32 *) malloc (buffsize * sizeof (guint32) + 128);
	//RAND_INIT ();
        srand ((uintptr_t) pixel);
        if (!rand_tab) rand_tab = (int *) malloc (NB_RAND * sizeof(int)) ;
        rand_pos = 1 ;
        while (rand_pos != 0) rand_tab [rand_pos++] = rand () ;
                
	cycle = 0;

	p1 = (guint32 *) ((1 + ((uintptr_t) (pixel)) / 128) * 128);
	p2 = (guint32 *) ((1 + ((uintptr_t) (back)) / 128) * 128);

	init_ifs (resx, c_resoly);
	gmline1 = goom_lines_init (resx, c_resoly, GML_HLINE, c_resoly, GML_BLACK, GML_CIRCLE, 0.4f * (float) c_resoly, GML_VERT);
	gmline2 = goom_lines_init (resx, c_resoly, GML_HLINE, 0, GML_BLACK, GML_CIRCLE, 0.2f * (float) c_resoly, GML_RED);

//	gfont_load ();

	tentacle_new ();
}


void goom_set_resolution (guint32 resx, guint32 resy, int cinemascope) {
	free (pixel);
	free (back);

	if (cinemascope)
		c_black_height = resy / 8;
	else
		c_black_height = 0;

	c_offset = c_black_height * resx;
	c_resoly = resy - c_black_height * 2;

	resolx = resx;
	resoly = resy;
	buffsize = resx * resy;

	pixel = (guint32 *) malloc (buffsize * sizeof (guint32) + 128);
	memset (pixel, 0, buffsize * sizeof (guint32) + 128);
	back = (guint32 *) malloc (buffsize * sizeof (guint32) + 128);
	memset (back, 0,  buffsize * sizeof (guint32) + 128);
	p1 = (guint32 *) ((1 + ((uintptr_t) (pixel)) / 128) * 128);
	p2 = (guint32 *) ((1 + ((uintptr_t) (back)) / 128) * 128);

	init_ifs (resx, c_resoly);
	goom_lines_set_res (gmline1, resx, c_resoly);
	goom_lines_set_res (gmline2, resx, c_resoly);
}


guint32 * goom_update (gint16 data[2][512], int forceMode) {
	static int lockvar = 0;				// pour empecher de nouveaux changements
	static int goomvar = 0;				// boucle des gooms
	static int totalgoom = 0;			// nombre de gooms par seconds
	static int agoom = 0;					// un goom a eu lieu..      
	static int abiggoom = 0;					// un big goom a eu lieu..      
	static int loopvar = 0;				// mouvement des points
	static int speedvar = 0;			// vitesse des particules

	// duree de la transition entre afficher les lignes ou pas
#define DRAWLINES 80
	static int lineMode = DRAWLINES;	// l'effet lineaire a dessiner
	static int nombreCDDC = 0;		// nombre de Cycle Depuis Dernier Changement
	guint32 *return_val;
	guint32 pointWidth;
	guint32 pointHeight;
	int     incvar;								// volume du son
	static int accelvar=0;							// acceleration des particules
	int     i;
	float   largfactor;						// elargissement de l'intervalle d'évolution
	static int stop_lines = 0;

	// des points
	static int ifs_incr = 1;			// dessiner l'ifs (0 = non: > = increment)
	static int decay_ifs = 0;			// disparition de l'ifs
	static int recay_ifs = 0;			// dédisparition de l'ifs

#define SWITCHMULT (29.0f/30.0f)
#define SWITCHINCR 0x7f
	static float switchMult = 1.0f;
	static int switchIncr = SWITCHINCR;

	static char goomlimit = 2;		// sensibilité du goom
	static ZoomFilterData zfd = {
		127, 8, 16,
		1, 1, 0, NORMAL_MODE,
		0, 0, 0, 0, 0
	};

	ZoomFilterData *pzfd;

	/* test if the config has changed, update it if so */
	pointWidth = (resolx * 2) / 5;
	pointHeight = ((c_resoly) * 2) / 5;

	/* ! etude du signal ... */
	incvar = 0;
	for (i = 0; i < 512; i++) {
		if (incvar < data[0][i])
			incvar = data[0][i];
	}

	i = accelvar;
	accelvar = incvar / 1000;

	if (speedvar > 5) {
		accelvar--;
		if (speedvar > 20)
			accelvar--;
		if (speedvar > 40)
			speedvar = 40;
	}
	accelvar--;

	i = accelvar - i;
	if (i<0) i=-i;

	speedvar += (speedvar + i/2);
	speedvar /= 2;
	if ((speedvar) && (cycle%9==0)) {
		speedvar -= 1;
	}
	if ((speedvar) && (cycle%5==0)) {
		speedvar = (speedvar*7)/8;
	}

	if (speedvar < 0)
		speedvar = 0;
	if (speedvar > 50)
		speedvar = 50;


	/* ! calcul du deplacement des petits points ... */

	largfactor = ((float) speedvar / 40.0f + (float) incvar / 50000.0f) / 1.5f;
	if (largfactor > 1.5f)
		largfactor = 1.5f;

	decay_ifs--;
	if (decay_ifs > 0)
		ifs_incr += 2;
	if (decay_ifs == 0)
		ifs_incr = 0;


	if (recay_ifs) {
		ifs_incr -= 2;
		recay_ifs--;
		if ((recay_ifs == 0)&&(ifs_incr<=0))
			ifs_incr = 1;
	}

	if (ifs_incr > 0)
		ifs_update (p1 + c_offset, p2 + c_offset, resolx, c_resoly, ifs_incr);
	
	if (curGState->drawPoints) {
		for (i = 1; i * 15 <= speedvar + 15; i++) {
			loopvar += speedvar*2/3 + 1;

			pointFilter (p1 + c_offset, YELLOW, ((pointWidth - 6.0f) * largfactor + 5.0f), ((pointHeight - 6.0f) * largfactor + 5.0f), i * 152.0f, 128.0f, loopvar + i * 2032);
			pointFilter (p1 + c_offset, ORANGE, ((pointWidth / 2) * largfactor) / i + 10.0f * i, ((pointHeight / 2) * largfactor) / i + 10.0f * i, 96.0f, i * 80.0f, loopvar / i);
			pointFilter (p1 + c_offset, VIOLET, ((pointHeight / 3 + 5.0f) * largfactor) / i + 10.0f * i, ((pointHeight / 3 + 5.0f) * largfactor) / i + 10.0f * i, i + 122.0f, 134.0f, loopvar / i);
			pointFilter (p1 + c_offset, BLACK, ((pointHeight / 3) * largfactor + 20.0f), ((pointHeight / 3) * largfactor + 20.0f), 58.0f, i * 66.0f, loopvar / i); 
			pointFilter (p1 + c_offset, WHITE, (pointHeight * largfactor + 10.0f * i) / i, (pointHeight * largfactor + 10.0f * i) / i, 66.0f, 74.0f, loopvar + i * 500); }
	}

	// par défaut pas de changement de zoom
	pzfd = NULL;

	/* 
	 * Test forceMode
	 */
#ifdef VERBOSE
	if (forceMode != 0) {
		printf ("forcemode = %d\n", forceMode);
	}
#endif


	// diminuer de 1 le temps de lockage
	// note pour ceux qui n'ont pas suivis : le lockvar permet d'empecher un
	// changement d'etat du plugins juste apres un autre changement d'etat. oki 
	// 
	// ?
	if (--lockvar < 0)
		lockvar = 0;

	// temps du goom
	if (--agoom < 0)
		agoom = 0;

	// temps du goom
	if (--abiggoom < 0)
		abiggoom = 0;

	if ((!abiggoom) && (speedvar > 4) && (goomlimit > 4) &&
			((accelvar > goomlimit*9/8+7)||(accelvar < -goomlimit*9/8-7))) {
		int size,i;
		static int couleur =
			 (0xc0<<(ROUGE*8))
			|(0xc0<<(VERT*8))
			|(0xf0<<(BLEU*8))
			|(0xf0<<(ALPHA*8));
		abiggoom = 100;
		size = resolx*c_resoly;
		for (i=0;i<size;i++)
			(p1+c_offset)[i] = (~(p1+c_offset)[i]) | couleur;
	}

	// on verifie qu'il ne se pas un truc interressant avec le son.
	if ((accelvar > goomlimit) || (accelvar < -goomlimit) || (forceMode > 0)
			|| (nombreCDDC > TIME_BTW_CHG)) {

//        if (nombreCDDC > 300) {
//        }

		// UN GOOM !!! YAHOO !
		totalgoom++;
		agoom = 20;									// mais pdt 20 cycles, il n'y en aura plus.

		// changement eventuel de mode
		if (iRAND(16) == 0)
		switch (iRAND (32)) {
		case 0:
		case 10:
			zfd.hypercosEffect = iRAND (2);
		case 13:
		case 20:
		case 21:
			zfd.mode = WAVE_MODE;
			zfd.reverse = 0;
			zfd.waveEffect = (iRAND (3) == 0);
			if (iRAND (2))
				zfd.vitesse = (zfd.vitesse + 127) >> 1;
			break;
		case 1:
		case 11:
			zfd.mode = CRYSTAL_BALL_MODE;
			zfd.waveEffect = 0;
			zfd.hypercosEffect = 0;
			break;
		case 2:
		case 12:
			zfd.mode = AMULETTE_MODE;
			zfd.waveEffect = 0;
			zfd.hypercosEffect = 0;
			break;
		case 3:
			zfd.mode = WATER_MODE;
			zfd.waveEffect = 0;
			zfd.hypercosEffect = 0;
			break;
		case 4:
		case 14:
			zfd.mode = SCRUNCH_MODE;
			zfd.waveEffect = 0;
			zfd.hypercosEffect = 0;
			break;
		case 5:
  	case 15:
		case 22:
			zfd.mode = HYPERCOS1_MODE;
			zfd.waveEffect = 0;
			zfd.hypercosEffect = (iRAND (3) == 0);
			break;
		case 6:
		case 16:
			zfd.mode = HYPERCOS2_MODE;
			zfd.waveEffect = 0;
			zfd.hypercosEffect = 0;
			break;
		case 7:
		case 17:
			zfd.mode = CRYSTAL_BALL_MODE;
			zfd.waveEffect = (iRAND (4) == 0);
			zfd.hypercosEffect = iRAND (2);
			break;
		case 8:
		case 18:
		case 19:
			zfd.mode = SCRUNCH_MODE;
			zfd.waveEffect = 1;
			zfd.hypercosEffect = 1;
			break;
	  case 29:
	 	case 30:
			zfd.mode = YONLY_MODE;
			break;
	  case 31:
	 	case 32:
			zfd.mode = SPEEDWAY_MODE;
			break;
		default:
			zfd.mode = NORMAL_MODE;
			zfd.waveEffect = 0;
			zfd.hypercosEffect = 0;
		}
	}
	
	// tout ceci ne sera fait qu'en cas de non-blocage
	if (lockvar == 0) {
		// reperage de goom (acceleration forte de l'acceleration du volume)
		// -> coup de boost de la vitesse si besoin..
		if ((accelvar > goomlimit) || (accelvar < -goomlimit)) {
			static int rndn = 0 ,i;
			static int blocker = 0;
			goomvar++;

			/* SELECTION OF THE GOOM STATE */
			if ((!blocker)&&(iRAND(3))) {
				rndn = iRAND(STATES_RANGEMAX);
				blocker = 3;
			}
			else if (blocker) blocker--;

			for (i=0;i<STATES_NB;i++)
				if ((rndn >= states[i].rangemin)
						&& (rndn <= states[i].rangemax))
					curGState = states+i;

			if ((curGState->drawIFS) && (ifs_incr<=0)) {
				recay_ifs = 5;
				ifs_incr = 11;
			}

			if ((!curGState->drawIFS) && (ifs_incr>0) && (decay_ifs<=0))
				decay_ifs = 100;

			if (!curGState->drawScope)
				stop_lines = 0xf000 & 5;
			
			if (!curGState->drawScope) {
				stop_lines = 0;			
				lineMode = DRAWLINES;
			}

			// if (goomvar % 1 == 0)
			{
				guint32 vtmp;
				guint32 newvit;

				lockvar = 50;
				newvit = STOP_SPEED + 1 - ((float)4.0f * log10((double)speedvar+1));
				// retablir le zoom avant..
				if ((zfd.reverse) && (!(cycle % 13)) && (rand () % 5 == 0)) {
					zfd.reverse = 0;
					zfd.vitesse = STOP_SPEED - 2;
					lockvar = 75;
				}
				if (iRAND (10) == 0) {
					zfd.reverse = 1;
					lockvar = 100;
				}

				if (iRAND (10) == 0)
					zfd.vitesse = STOP_SPEED - 1;
				if (iRAND (12) == 0)
					zfd.vitesse = STOP_SPEED + 1;

				// changement de milieu..
				switch (iRAND (25)) {
				case 0:
				case 3:
				case 6:
					zfd.middleY = c_resoly - 1;
					zfd.middleX = resolx / 2;
					break;
				case 1:
				case 4:
					zfd.middleX = resolx - 1;
					break;
				case 2:
				case 5:
					zfd.middleX = 1;
					break;
				default:
					zfd.middleY = c_resoly / 2;
					zfd.middleX = resolx / 2;
				}

				if ((zfd.mode == WATER_MODE)
						|| (zfd.mode == YONLY_MODE)
						|| (zfd.mode == AMULETTE_MODE)) {
					zfd.middleX = resolx / 2;
					zfd.middleY = c_resoly / 2;
				}

				switch (vtmp = (iRAND (15))) {
				case 0:
                                    
					zfd.vPlaneEffect = iRAND (3) - iRAND (3);
					zfd.hPlaneEffect = iRAND (3) - iRAND (3);
					break;
				case 3:
					zfd.vPlaneEffect = 0;
					zfd.hPlaneEffect = iRAND (8) - iRAND (8);
					break;
				case 4:
				case 5:
				case 6:
				case 7:
					zfd.vPlaneEffect = iRAND (5) - iRAND (5);
					zfd.hPlaneEffect = -zfd.vPlaneEffect;
					break;
				case 8:
					zfd.hPlaneEffect = 5 + iRAND (8);
					zfd.vPlaneEffect = -zfd.hPlaneEffect;
					break;
				case 9:
					zfd.vPlaneEffect = 5 + iRAND (8);
					zfd.hPlaneEffect = -zfd.hPlaneEffect;
					break;
				case 13:
					zfd.hPlaneEffect = 0;
					zfd.vPlaneEffect = iRAND (10) - iRAND (10);
					break;
				case 14:
					zfd.hPlaneEffect = iRAND (10) - iRAND (10);
					zfd.vPlaneEffect = iRAND (10) - iRAND (10);
					break;
				default:
					if (vtmp < 10) {
						zfd.vPlaneEffect = 0;
						zfd.hPlaneEffect = 0;
					}
				}

				if (iRAND (5) != 0)
					zfd.noisify = 0;
				else {
					zfd.noisify = iRAND (2) + 1;
					lockvar *= 2;
				}

				if (zfd.mode == AMULETTE_MODE) {
					zfd.vPlaneEffect = 0;
					zfd.hPlaneEffect = 0;
					zfd.noisify = 0;
				}

				if ((zfd.middleX == 1) || (zfd.middleX == (int)resolx - 1)) {
					zfd.vPlaneEffect = 0;
					zfd.hPlaneEffect = iRAND (2) ? 0 : zfd.hPlaneEffect;
				}

				if (newvit < (guint32)zfd.vitesse)	// on accelere
				{
					pzfd = &zfd;
					if (((newvit < STOP_SPEED - 7) &&
							 (zfd.vitesse < STOP_SPEED - 6) &&
							 (cycle % 3 == 0)) || (iRAND (40) == 0)) {
						zfd.vitesse = STOP_SPEED - iRAND (2) + iRAND (2);
						zfd.reverse = !zfd.reverse;
					}
					else {
						zfd.vitesse = (newvit + zfd.vitesse * 7) / 8;
					}
					lockvar += 50;
				}
			}

			if (lockvar > 150) {
				switchIncr = SWITCHINCR;
				switchMult = 1.0f;
			}
		}
		// mode mega-lent
		if (iRAND (700) == 0) {
			pzfd = &zfd;
			zfd.vitesse = STOP_SPEED - 1;
			zfd.pertedec = 8;
			zfd.sqrtperte = 16;
			goomvar = 1;
			lockvar += 50;
			switchIncr = SWITCHINCR;
			switchMult = 1.0f;
		}
	}

	/*
	 * gros frein si la musique est calme
	 */
	if ((speedvar < 1) && (zfd.vitesse < STOP_SPEED - 4) && (cycle % 16 == 0)) {
		pzfd = &zfd;
		zfd.vitesse += 3;
		zfd.pertedec = 8;
		zfd.sqrtperte = 16;
		goomvar = 0;
	}

	/*
	 * baisser regulierement la vitesse...
	 */
	if ((cycle % 73 == 0) && (zfd.vitesse < STOP_SPEED - 5)) {
		pzfd = &zfd;
		zfd.vitesse++;
	}

	/*
	 * arreter de decrémenter au bout d'un certain temps
	 */
	if ((cycle % 101 == 0) && (zfd.pertedec == 7)) {
		pzfd = &zfd;
		zfd.pertedec = 8;
		zfd.sqrtperte = 16;
	}

	/*
	 * Permet de forcer un effet.
	 */
	if ((forceMode > 0) && (forceMode <= NB_FX)) {
		pzfd = &zfd;
		pzfd->mode = forceMode - 1;
	}

	if (forceMode == -1) {
		pzfd = NULL;
	}

	/*
	 * Changement d'effet de zoom !
	 */
	if (pzfd != NULL) {
		static int exvit = 128;
		int     dif;

		nombreCDDC = 0;

		switchIncr = SWITCHINCR;

		dif = zfd.vitesse - exvit;
		if (dif < 0)
			dif = -dif;

		if (dif > 2) {
			switchIncr *= (dif + 2) / 2;
		}
		exvit = zfd.vitesse;
		switchMult = 1.0f;

		if (((accelvar > goomlimit) && (totalgoom < 2)) || (forceMode > 0)) {
			switchIncr = 0;
			switchMult = SWITCHMULT;
		}
	}
	else {
		if (nombreCDDC > TIME_BTW_CHG) {
			pzfd = &zfd;
			nombreCDDC = 0;
		}
		else
			nombreCDDC++;
	}

#ifdef VERBOSE
	if (pzfd) {
		printf ("GOOM: pzfd->mode = %d\n", pzfd->mode);
	}
#endif

	// Zoom here !
	zoomFilterFastRGB (p1 + c_offset, p2 + c_offset, pzfd, resolx, c_resoly, switchIncr, switchMult);

	/*
	 * Affichage tentacule
	 */

	if (goomlimit!=0)
        tentacle_update((gint32*)(p2 + c_offset), (gint32*)(p1 + c_offset), resolx, c_resoly, data, (float)accelvar/goomlimit, curGState->drawTentacle);
	else
        tentacle_update((gint32*)(p2 + c_offset), (gint32*)(p1 + c_offset), resolx, c_resoly, data,0.0f, curGState->drawTentacle);

/*
	{
		static char title[1024];
		static int displayTitle = 0;
		char    text[255];

		if (fps > 0) {
			int i;
			if (speedvar>0) {
				for (i=0;i<speedvar;i++)
					text[i]='*';
				text[i]=0;
				goom_draw_text (p1 + c_offset,resolx,c_resoly, 10, 50, text, 1.0f, 0);
			}
			if (accelvar>0) {
				for (i=0;i<accelvar;i++) {
					if (i==goomlimit)
						text[i]='o';
					else
						text[i]='*';
				}
				text[i]=0;
				goom_draw_text (p1 + c_offset,resolx,c_resoly, 10, 62, text, 1.0f, 0);
			}
			if (agoom==20)
				goom_draw_text (p1 + c_offset,resolx,c_resoly,10, 80, "GOOM",1.0f, 0);
			else if (agoom)
				goom_draw_text (p1 + c_offset,resolx,c_resoly,10, 80, "goom",1.0f, 0);
			if (abiggoom==200)
				goom_draw_text (p1 + c_offset,resolx,c_resoly,10, 100, "BGOOM",1.0f, 0);
			else if (abiggoom)
				goom_draw_text (p1 + c_offset,resolx,c_resoly,10, 100, "bgoom",1.0f, 0);
		}

		update_message (message);

		if (fps > 0) {
			sprintf (text, "%2.f fps", fps);
			goom_draw_text (p1 + c_offset,resolx,c_resoly, 10, 24, text, 1, 0);
		}

		if (songTitle != NULL) {
			sprintf (title, songTitle);	// la flemme d'inclure string.h :)
			displayTitle = 200;
		}

		if (displayTitle) {
			goom_draw_text (p1 + c_offset,resolx,c_resoly, resolx / 2, c_resoly / 2 + 7, title, ((float) (190 - displayTitle) / 10.0f), 1);
			displayTitle--;
			if (displayTitle < 4)
				goom_draw_text (p2 + c_offset,resolx,c_resoly, resolx / 2, c_resoly / 2 + 7, title, ((float) (190 - displayTitle) / 10.0f), 1);
		}
	}
*/

	/*
	 * arret demande
	 */
	if ((stop_lines & 0xf000)||(!curGState->drawScope)) {
		float   param1, param2, amplitude;
		int     couleur;
		int     mode;
		
		choose_a_goom_line (&param1, &param2, &couleur, &mode, &amplitude,1);
		couleur = GML_BLACK;
		
		goom_lines_switch_to (gmline1, mode, param1, amplitude, couleur);
		goom_lines_switch_to (gmline2, mode, param2, amplitude, couleur);
		stop_lines &= 0x0fff;
	}
	
	/*
	 * arret aleatore.. changement de mode de ligne..
	 */
	if (lineMode != DRAWLINES) {
		lineMode--;
		if (lineMode == -1)
			lineMode = 0;
	}
	else
		if ((cycle%80==0)&&(iRAND(5)==0)&&lineMode)
			lineMode--;

	if ((cycle % 120 == 0)
			&& (iRAND (4) == 0)
			&& (curGState->drawScope)) {
		if (lineMode == 0)
			lineMode = DRAWLINES;
		else if (lineMode == DRAWLINES) {
			float   param1, param2, amplitude;
			int     couleur1,couleur2;
			int     mode;

			lineMode--;
			choose_a_goom_line (&param1, &param2, &couleur1, &mode, &amplitude,stop_lines);

			couleur2 = 5-couleur1;
			if (stop_lines) {
				stop_lines--;
				if (iRAND(2))
					couleur2=couleur1 = GML_BLACK;
			}

			goom_lines_switch_to (gmline1, mode, param1, amplitude, couleur1);
			goom_lines_switch_to (gmline2, mode, param2, amplitude, couleur2);
		}
	}

	/*
	 * si on est dans un goom : afficher les lignes...
	 */
	if ((lineMode != 0) || (agoom > 15)) {
		gmline2->power = gmline1->power;

		goom_lines_draw (gmline1, data[0], p2 + c_offset);
		goom_lines_draw (gmline2, data[1], p2 + c_offset);

		if (((cycle % 121) == 9) && (iRAND (3) == 1)
				&& ((lineMode == 0) || (lineMode == DRAWLINES))) {
			float   param1, param2, amplitude;
			int     couleur1,couleur2;
			int     mode;

			choose_a_goom_line (&param1, &param2, &couleur1, &mode, &amplitude, stop_lines);
			couleur2 = 5-couleur1;
			
			if (stop_lines) {
				stop_lines--;
				if (iRAND(2))
					couleur2=couleur1 = GML_BLACK;
			}
			goom_lines_switch_to (gmline1, mode, param1, amplitude, couleur1);
			goom_lines_switch_to (gmline2, mode, param2, amplitude, couleur2);
		}
	}

	return_val = p1;
	tmp = p1;
	p1 = p2;
	p2 = tmp;

	// affichage et swappage des buffers..
	cycle++;

	// toute les 2 secondes : vérifier si le taux de goom est correct
	// et le modifier sinon..
	if (!(cycle % 64)) {
		if (speedvar<1)
			goomlimit /= 2;
		if (totalgoom > 4) {
			goomlimit++;
		}
		if (totalgoom > 7) {
			goomlimit*=4/3;
			goomlimit+=2;
		}
		if ((totalgoom == 0) && (goomlimit > 1))
			goomlimit--;
		if ((totalgoom == 1) && (goomlimit > 1))
			goomlimit--;
		totalgoom = 0;
	}
	return return_val;
}

void goom_close () {
	if (pixel != NULL)
		free (pixel);
	if (back != NULL)
		free (back);
	pixel = back = NULL;
	RAND_CLOSE ();
	release_ifs ();
	goom_lines_free (&gmline1);
	goom_lines_free (&gmline2);
	tentacle_free();
}


void choose_a_goom_line (float *param1, float *param2, int *couleur, int *mode, float *amplitude, int far) {
	*mode = iRAND (3);
	*amplitude = 1.0f;
	switch (*mode) {
	case GML_CIRCLE:
		if (far) {
			*param1 = *param2 = 0.47f;
			*amplitude = 0.8f;
			break;
		}
		if (iRAND (3) == 0) {
			*param1 = *param2 = 0;
			*amplitude = 3.0f;
		}
		else if (iRAND (2)) {
			*param1 = 0.40f * c_resoly;
			*param2 = 0.22f * c_resoly;
		}
		else {
			*param1 = *param2 = c_resoly * 0.35;
		}
		break;
	case GML_HLINE:
		if (iRAND (4) || far) {
			*param1 = c_resoly / 7;
			*param2 = 6.0f * c_resoly / 7.0f;
		}
		else {
			*param1 = *param2 = c_resoly / 2.0f;
			*amplitude = 2.0f;
		}
		break;
	case GML_VLINE:
		if (iRAND (3) || far) {
			*param1 = resolx / 7.0f;
			*param2 = 6.0f * resolx / 7.0f;
		}
		else {
			*param1 = *param2 = resolx / 2.0f;
			*amplitude = 1.5f;
		}
		break;
	}

	*couleur = iRAND (6);
}

/*
void goom_set_font (int ***chars, int *width, int *height) {
    if (chars == NULL)
        return ;
}
*/

/*
void update_message (char *message) {

	static int nbl;
	static char msg2 [0x800];
	static int affiche = 0;
	static int longueur;
	int fin = 0;

	if (message) {
		int i=1,j=0;
		sprintf (msg2,message);
		for (j=0;msg2[j];j++)
			if (msg2[j]=='\n')
				i++;
		nbl = i;
		affiche = resoly + nbl * 25 + 105;
		longueur = strlen (msg2);
	}
	if (affiche) {
		int i = 0;
		char *msg=malloc(longueur+1);
		char *ptr = msg;
		int pos;
		float ecart;
		message = msg;
		sprintf (msg,msg2);
		
		while (!fin) {
			while (1) {
				if (*ptr == 0) {
					fin = 1;
					break;
				}
				if (*ptr == '\n') {
					*ptr = 0;
					break;
				}
				++ptr;
			}
			pos = affiche - (nbl-i)*25;
			pos += 6.0*(cos((double)pos/20.0));
			pos -= 80;
			ecart = (1.0+2.0*sin((double)pos/20.0));
			if ((fin) && (2 * pos < (int)resoly))
				pos = (int)resoly / 2;
			pos += 7;

			goom_draw_text(p1 + c_offset,resolx,c_resoly, resolx/2, pos, message, ecart, 1);
			message = ++ptr;
			i++;
		}
		affiche --;
		free (msg);
	}
}
*/
