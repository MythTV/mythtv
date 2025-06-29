#include "../config.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <random>

#include "goom_core.h"
#include "goom_tools.h"
#include "filters.h"
#include "lines.h"
#include "ifs.h"
#include "tentacle3d.h"
//#include "gfontlib.h"

//#define VERBOSE

static constexpr gint32  STOP_SPEED   { 128 };
static constexpr int16_t TIME_BTW_CHG { 300 };

/**-----------------------------------------------------**
 **  SHARED DATA                                        **
 **-----------------------------------------------------**/
static guint32 *pixel;
static guint32 *back;
static guint32 *p1, *p2, *tmp;
static guint32 cycle;

struct GoomState {
	int m_drawIfs;
	int m_drawPoints;
	int m_drawTentacle;

	int m_drawScope;
	int m_farScope;

	int m_rangeMin;
	int m_rangeMax;
};

static constexpr size_t   STATES_NB       {   8 };
static constexpr uint16_t STATES_RANGEMAX { 510 };
const std::array<const GoomState,STATES_NB> kStates {{
	{1,0,0,1,4, 000, 100},
	{1,0,0,1,1, 101, 140}, // turned on drawScope
	{1,0,0,1,2, 141, 200},
	{0,1,0,1,2, 201, 260},
	{0,1,0,1,0, 261, 330},
	{0,1,1,1,4, 331, 400},
	{0,0,1,1,5, 401, 450}, // turned on drawScope
        {0,0,1,1,1, 451, 510}
}};

const GoomState *curGState = &kStates[4];

guint32 resolx, resoly, buffsize, c_black_height = 0, c_offset = 0, c_resoly = 0;	/* avec prise en compte de ca */

// effet de ligne..
static GMLine *gmline1 = nullptr;
static GMLine *gmline2 = nullptr;

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
	c_resoly = resy - (c_black_height * 2);

	pixel = (guint32 *) malloc ((buffsize * sizeof (guint32)) + 128);
	back = (guint32 *) malloc ((buffsize * sizeof (guint32)) + 128);
	//RAND_INIT ();
	srand ((uintptr_t) pixel);
	if (!rand_tab) rand_tab = (int *) malloc (NB_RAND * sizeof(int)) ;
	for (size_t i = 0; i < NB_RAND; i++)
		rand_tab[i] = goom_rand();
	rand_pos = 0;
                
	cycle = 0;

	p1 = (guint32 *) ((1 + ((uintptr_t) (pixel)) / 128) * 128);
	p2 = (guint32 *) ((1 + ((uintptr_t) (back)) / 128) * 128);

	init_ifs (resx, c_resoly);
	gmline1 = goom_lines_init (resx, c_resoly, GML_HLINE, c_resoly, GML_BLACK, GML_CIRCLE, 0.4F * (float) c_resoly, GML_VERT);
	gmline2 = goom_lines_init (resx, c_resoly, GML_HLINE, 0, GML_BLACK, GML_CIRCLE, 0.2F * (float) c_resoly, GML_RED);

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
	c_resoly = resy - (c_black_height * 2);

	resolx = resx;
	resoly = resy;
	buffsize = resx * resy;

	pixel = (guint32 *) malloc ((buffsize * sizeof (guint32)) + 128);
	memset (pixel, 0, (buffsize * sizeof (guint32)) + 128);
	back = (guint32 *) malloc ((buffsize * sizeof (guint32)) + 128);
	memset (back, 0,  (buffsize * sizeof (guint32)) + 128);
	p1 = (guint32 *) ((1 + ((uintptr_t) (pixel)) / 128) * 128);
	p2 = (guint32 *) ((1 + ((uintptr_t) (back)) / 128) * 128);

	init_ifs (resx, c_resoly);
	goom_lines_set_res (gmline1, resx, c_resoly);
	goom_lines_set_res (gmline2, resx, c_resoly);
}


guint32 * goom_update (GoomDualData& data, int forceMode) {
	static int s_lockVar = 0;		// pour empecher de nouveaux changements
	static int s_totalGoom = 0;		// nombre de gooms par seconds
	static int s_aGoom = 0;			// un goom a eu lieu..
	static int s_aBigGoom = 0;		// un big goom a eu lieu..
	static int s_speedVar = 0;		// vitesse des particules

	// duree de la transition entre afficher les lignes ou pas
	static constexpr int DRAWLINES { 80 };
	static int s_lineMode = DRAWLINES;	// l'effet lineaire a dessiner
	static int s_nombreCddc = 0;		// nombre de Cycle Depuis Dernier Changement
	static int s_accelVar=0;		// acceleration des particules
	static int s_stopLines = 0;

	// des points
	static int s_ifsIncr = 1;		// dessiner l'ifs (0 = non: > = increment)
	static int s_decayIfs = 0;		// disparition de l'ifs
	static int s_recayIfs = 0;		// dédisparition de l'ifs

	static constexpr float SWITCHMULT { 29.0F/30.0F };
	static constexpr int   SWITCHINCR { 0x7f };
	static float s_switchMult = 1.0F;
	static int s_switchIncr = SWITCHINCR;

	static char s_goomLimit = 2;		// sensibilité du goom
	static ZoomFilterData s_zfd = {
		127, 8, 16,
		1, 1, false, NORMAL_MODE,
		0, 0, false, false, 0
	};

	ZoomFilterData *pzfd = nullptr;

	/* test if the config has changed, update it if so */
	guint32 pointWidth = (resolx * 2) / 5;
	guint32 pointHeight = ((c_resoly) * 2) / 5;

	/* ! etude du signal ... */
	int incvar = 0;				// volume du son
	for (int i = 0; i < 512; i++) {
		incvar = std::max<int>(incvar, data[0][i]);
	}

	int i = s_accelVar;
	s_accelVar = incvar / 1000;

	if (s_speedVar > 5) {
		s_accelVar--;
		if (s_speedVar > 20)
			s_accelVar--;
		s_speedVar = std::min(s_speedVar, 40);
	}
	s_accelVar--;

	i = s_accelVar - i;
	if (i<0) i=-i;

	s_speedVar += (s_speedVar + (i/2));
	s_speedVar /= 2;
	if ((s_speedVar) && (cycle%9==0)) {
		s_speedVar -= 1;
	}
	if ((s_speedVar) && (cycle%5==0)) {
		s_speedVar = (s_speedVar*7)/8;
	}

	s_speedVar = std::clamp(s_speedVar, 0, 50);


	/* ! calcul du deplacement des petits points ... */

        // largfactor: elargissement de l'intervalle d'évolution
	float largfactor = ((float) s_speedVar / 40.0F + (float) incvar / 50000.0F) / 1.5F;
	largfactor = std::min(largfactor, 1.5F);

	s_decayIfs--;
	if (s_decayIfs > 0)
		s_ifsIncr += 2;
	if (s_decayIfs == 0)
		s_ifsIncr = 0;


	if (s_recayIfs) {
		s_ifsIncr -= 2;
		s_recayIfs--;
		if ((s_recayIfs == 0)&&(s_ifsIncr<=0))
			s_ifsIncr = 1;
	}

	if (s_ifsIncr > 0)
		ifs_update (p1 + c_offset, p2 + c_offset, resolx, c_resoly, s_ifsIncr);
	
	if (curGState->m_drawPoints) {
		for (i = 1; i * 15 <= s_speedVar + 15; i++) {
			static int s_loopVar = 0; // mouvement des points
			s_loopVar += (s_speedVar*2/3) + 1;

			pointFilter (p1 + c_offset, YELLOW,
                                     (((pointWidth - 6.0F) * largfactor) + 5.0F),
                                     (((pointHeight - 6.0F) * largfactor) + 5.0F),
                                     i * 152.0F, 128.0F, s_loopVar + (i * 2032));
			pointFilter (p1 + c_offset, ORANGE,
                                     (((pointWidth  / 2.0F) * largfactor) / i) + (10.0F * i),
                                     (((pointHeight / 2.0F) * largfactor) / i) + (10.0F * i),
                                     96.0F, i * 80.0F, s_loopVar / i);
			pointFilter (p1 + c_offset, VIOLET,
                                     (((pointHeight / 3.0F + 5.0F) * largfactor) / i) + (10.0F * i),
                                     (((pointHeight / 3.0F + 5.0F) * largfactor) / i) + (10.0F * i),
                                     i + 122.0F, 134.0F, s_loopVar / i);
			pointFilter (p1 + c_offset, BLACK,
                                     (((pointHeight / 3.0F) * largfactor) + 20.0F),
                                     (((pointHeight / 3.0F) * largfactor) + 20.0F),
                                     58.0F, i * 66.0F, s_loopVar / i);
			pointFilter (p1 + c_offset, WHITE,
                                     (pointHeight * largfactor + 10.0F * i) / i,
                                     (pointHeight * largfactor + 10.0F * i) / i,
                                     66.0F, 74.0F, s_loopVar + (i * 500)); }
	}

	// par défaut pas de changement de zoom
	pzfd = nullptr;

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
	if (--s_lockVar < 0)
		s_lockVar = 0;

	// temps du goom
	if (--s_aGoom < 0)
		s_aGoom = 0;

	// temps du goom
	if (--s_aBigGoom < 0)
		s_aBigGoom = 0;

	if ((!s_aBigGoom) && (s_speedVar > 4) && (s_goomLimit > 4) &&
			((s_accelVar > (s_goomLimit*9/8)+7)||(s_accelVar < (-s_goomLimit*9/8)-7))) {
		static int s_couleur =
			 (0xc0<<(ROUGE*8))
			|(0xc0<<(VERT*8))
			|(0xf0<<(BLEU*8))
			|(0xf0<<(ALPHA*8));
		s_aBigGoom = 100;
		int size = resolx*c_resoly;
		for (int j=0;j<size;j++)
			(p1+c_offset)[j] = (~(p1+c_offset)[j]) | s_couleur;
	}

	// on verifie qu'il ne se pas un truc interressant avec le son.
	if ((s_accelVar > s_goomLimit) || (s_accelVar < -s_goomLimit) || (forceMode > 0)
			|| (s_nombreCddc > TIME_BTW_CHG)) {

//        if (nombreCDDC > 300) {
//        }

		// UN GOOM !!! YAHOO !
		s_totalGoom++;
		s_aGoom = 20;			// mais pdt 20 cycles, il n'y en aura plus.

		// changement eventuel de mode
		if (iRAND(16) == 0)
                {
                    switch (iRAND (32)) {
                    case 0:
                    case 10:
			s_zfd.hypercosEffect = iRAND (2);
			// Checked Fedora26 get-plugins-good sources.
			// No break statement there.
			[[fallthrough]];
                    case 13:
                    case 20:
                    case 21:
			s_zfd.mode = WAVE_MODE;
			s_zfd.reverse = false;
			s_zfd.waveEffect = (iRAND (3) == 0);
			if (iRAND (2))
                            s_zfd.vitesse = (s_zfd.vitesse + 127) >> 1;
			break;
                    case 1:
                    case 11:
			s_zfd.mode = CRYSTAL_BALL_MODE;
			s_zfd.waveEffect = false;
			s_zfd.hypercosEffect = false;
			break;
                    case 2:
                    case 12:
			s_zfd.mode = AMULETTE_MODE;
			s_zfd.waveEffect = false;
			s_zfd.hypercosEffect = false;
			break;
                    case 3:
			s_zfd.mode = WATER_MODE;
			s_zfd.waveEffect = false;
			s_zfd.hypercosEffect = false;
			break;
                    case 4:
                    case 14:
			s_zfd.mode = SCRUNCH_MODE;
			s_zfd.waveEffect = false;
			s_zfd.hypercosEffect = false;
			break;
                    case 5:
                    case 15:
                    case 22:
			s_zfd.mode = HYPERCOS1_MODE;
			s_zfd.waveEffect = false;
			s_zfd.hypercosEffect = (iRAND (3) == 0);
			break;
                    case 6:
                    case 16:
			s_zfd.mode = HYPERCOS2_MODE;
			s_zfd.waveEffect = false;
			s_zfd.hypercosEffect = false;
			break;
                    case 7:
                    case 17:
			s_zfd.mode = CRYSTAL_BALL_MODE;
			s_zfd.waveEffect = (iRAND (4) == 0);
			s_zfd.hypercosEffect = iRAND (2);
			break;
                    case 8:
                    case 18:
                    case 19:
			s_zfd.mode = SCRUNCH_MODE;
			s_zfd.waveEffect = true;
			s_zfd.hypercosEffect = true;
			break;
                    case 29:
                    case 30:
			s_zfd.mode = YONLY_MODE;
			break;
                    case 31:
                    case 32:
			s_zfd.mode = SPEEDWAY_MODE;
			break;
                    default:
			s_zfd.mode = NORMAL_MODE;
			s_zfd.waveEffect = false;
			s_zfd.hypercosEffect = false;
                    }
                }
	}
	
	// tout ceci ne sera fait qu'en cas de non-blocage
	if (s_lockVar == 0) {
		// reperage de goom (acceleration forte de l'acceleration du volume)
		// -> coup de boost de la vitesse si besoin..
		if ((s_accelVar > s_goomLimit) || (s_accelVar < -s_goomLimit)) {
			static int s_rndn = 0;
			static int s_blocker = 0;

			/* SELECTION OF THE GOOM STATE */
			if ((!s_blocker)&&(iRAND(3))) {
				s_rndn = iRAND(STATES_RANGEMAX);
				s_blocker = 3;
			}
			else if (s_blocker)
			{
			    s_blocker--;
			}

                        (void)s_rndn; // Used in the lambda. Quiet warning.
                        auto goodstate = [&](auto state)
                            { return (s_rndn >= state.m_rangeMin) &&
                                     (s_rndn <= state.m_rangeMax); };
                        const auto *it =
                            std::find_if(kStates.cbegin(), kStates.cend(), goodstate);
                        if (it != kStates.cend())
                            curGState = &(*it);

			if ((curGState->m_drawIfs) && (s_ifsIncr<=0)) {
				s_recayIfs = 5;
				s_ifsIncr = 11;
			}

			if ((!curGState->m_drawIfs) && (s_ifsIncr>0) && (s_decayIfs<=0))
				s_decayIfs = 100;

			if (!curGState->m_drawScope) {
				s_stopLines = 0;
				s_lineMode = DRAWLINES;
			}

			// if (goomvar % 1 == 0)
			{
				s_lockVar = 50;
				guint32 newvit = STOP_SPEED + 1 - (4.0F * log10f(s_speedVar+1));
				// retablir le zoom avant..
                                // Pseudo-random is good enough. Don't need a true random.
                                // NOLINTNEXTLINE(cert-msc30-c,cert-msc50-cpp)
				if ((s_zfd.reverse) && (!(cycle % 13)) && (rand () % 5 == 0)) {
					s_zfd.reverse = false;
					s_zfd.vitesse = STOP_SPEED - 2;
					s_lockVar = 75;
				}
				if (iRAND (10) == 0) {
					s_zfd.reverse = true;
					s_lockVar = 100;
				}

				if (iRAND (10) == 0)
					s_zfd.vitesse = STOP_SPEED - 1;
				if (iRAND (12) == 0)
					s_zfd.vitesse = STOP_SPEED + 1;

				// changement de milieu..
				switch (iRAND (25)) {
				case 0:
				case 3:
				case 6:
					s_zfd.middleY = c_resoly - 1;
					s_zfd.middleX = resolx / 2;
					break;
				case 1:
				case 4:
					s_zfd.middleX = resolx - 1;
					break;
				case 2:
				case 5:
					s_zfd.middleX = 1;
					break;
				default:
					s_zfd.middleY = c_resoly / 2;
					s_zfd.middleX = resolx / 2;
				}

				if ((s_zfd.mode == WATER_MODE)
						|| (s_zfd.mode == YONLY_MODE)
						|| (s_zfd.mode == AMULETTE_MODE)) {
					s_zfd.middleX = resolx / 2;
					s_zfd.middleY = c_resoly / 2;
				}

				guint32 vtmp = iRAND (15);
				switch (vtmp) {
				case 0:
                                    
                                        // NOLINTNEXTLINE(misc-redundant-expression)
					s_zfd.vPlaneEffect = iRAND (3) - iRAND (3);
                                        // NOLINTNEXTLINE(misc-redundant-expression)
					s_zfd.hPlaneEffect = iRAND (3) - iRAND (3);
					break;
				case 3:
					s_zfd.vPlaneEffect = 0;
                                        // NOLINTNEXTLINE(misc-redundant-expression)
					s_zfd.hPlaneEffect = iRAND (8) - iRAND (8);
					break;
				case 4:
				case 5:
				case 6:
				case 7:
                                        // NOLINTNEXTLINE(misc-redundant-expression)
					s_zfd.vPlaneEffect = iRAND (5) - iRAND (5);
					s_zfd.hPlaneEffect = -s_zfd.vPlaneEffect;
					break;
				case 8:
					s_zfd.hPlaneEffect = 5 + iRAND (8);
					s_zfd.vPlaneEffect = -s_zfd.hPlaneEffect;
					break;
				case 9:
					s_zfd.vPlaneEffect = 5 + iRAND (8);
					s_zfd.hPlaneEffect = -s_zfd.hPlaneEffect;
					break;
				case 13:
					s_zfd.hPlaneEffect = 0;
                                        // NOLINTNEXTLINE(misc-redundant-expression)
					s_zfd.vPlaneEffect = iRAND (10) - iRAND (10);
					break;
				case 14:
                                        // NOLINTNEXTLINE(misc-redundant-expression)
					s_zfd.hPlaneEffect = iRAND (10) - iRAND (10);
                                        // NOLINTNEXTLINE(misc-redundant-expression)
					s_zfd.vPlaneEffect = iRAND (10) - iRAND (10);
					break;
				default:
					if (vtmp < 10) {
						s_zfd.vPlaneEffect = 0;
						s_zfd.hPlaneEffect = 0;
					}
				}

				if (iRAND (5) != 0)
					s_zfd.noisify = 0;
				else {
					s_zfd.noisify = iRAND (2) + 1;
					s_lockVar *= 2;
				}

				if (s_zfd.mode == AMULETTE_MODE) {
					s_zfd.vPlaneEffect = 0;
					s_zfd.hPlaneEffect = 0;
					s_zfd.noisify = 0;
				}

				if ((s_zfd.middleX == 1) || (s_zfd.middleX == (int)resolx - 1)) {
					s_zfd.vPlaneEffect = 0;
					s_zfd.hPlaneEffect = iRAND (2) ? 0 : s_zfd.hPlaneEffect;
				}

				if (newvit < (guint32)s_zfd.vitesse)	// on accelere
				{
					pzfd = &s_zfd;
					if (((newvit < STOP_SPEED - 7) &&
							 (s_zfd.vitesse < STOP_SPEED - 6) &&
							 (cycle % 3 == 0)) || (iRAND (40) == 0)) {
						s_zfd.vitesse = STOP_SPEED - iRAND (2) + iRAND (2);
						s_zfd.reverse = !s_zfd.reverse;
					}
					else {
						s_zfd.vitesse = (newvit + s_zfd.vitesse * 7) / 8;
					}
					s_lockVar += 50;
				}
			}

			if (s_lockVar > 150) {
				s_switchIncr = SWITCHINCR;
				s_switchMult = 1.0F;
			}
		}
		// mode mega-lent
		if (iRAND (700) == 0) {
			pzfd = &s_zfd;
			s_zfd.vitesse = STOP_SPEED - 1;
			s_zfd.pertedec = 8;
			s_zfd.sqrtperte = 16;
			s_lockVar += 50;
			s_switchIncr = SWITCHINCR;
			s_switchMult = 1.0F;
		}
	}

	/*
	 * gros frein si la musique est calme
	 */
	if ((s_speedVar < 1) && (s_zfd.vitesse < STOP_SPEED - 4) && (cycle % 16 == 0)) {
		pzfd = &s_zfd;
		s_zfd.vitesse += 3;
		s_zfd.pertedec = 8;
		s_zfd.sqrtperte = 16;
	}

	/*
	 * baisser regulierement la vitesse...
	 */
	if ((cycle % 73 == 0) && (s_zfd.vitesse < STOP_SPEED - 5)) {
		pzfd = &s_zfd;
		s_zfd.vitesse++;
	}

	/*
	 * arreter de decrémenter au bout d'un certain temps
	 */
	if ((cycle % 101 == 0) && (s_zfd.pertedec == 7)) {
		pzfd = &s_zfd;
		s_zfd.pertedec = 8;
		s_zfd.sqrtperte = 16;
	}

	/*
	 * Permet de forcer un effet.
	 */
	if ((forceMode > 0) && (forceMode <= NB_FX)) {
		pzfd = &s_zfd;
		pzfd->mode = forceMode - 1;
	}

	if (forceMode == -1) {
		pzfd = nullptr;
	}

	/*
	 * Changement d'effet de zoom !
	 */
	if (pzfd != nullptr) {
		static int s_exvit = 128;

		s_nombreCddc = 0;

		s_switchIncr = SWITCHINCR;

		int dif = s_zfd.vitesse - s_exvit;
		if (dif < 0)
			dif = -dif;

		if (dif > 2) {
			s_switchIncr *= (dif + 2) / 2;
		}
		s_exvit = s_zfd.vitesse;
		s_switchMult = 1.0F;

		if (((s_accelVar > s_goomLimit) && (s_totalGoom < 2)) || (forceMode > 0)) {
			s_switchIncr = 0;
			s_switchMult = SWITCHMULT;
		}
	}
	else {
		if (s_nombreCddc > TIME_BTW_CHG) {
			pzfd = &s_zfd;
			s_nombreCddc = 0;
		}
		else
		{
			s_nombreCddc++;
		}
	}

#ifdef VERBOSE
	if (pzfd) {
		printf ("GOOM: pzfd->mode = %d\n", pzfd->mode);
	}
#endif

	// Zoom here !
	zoomFilterFastRGB (p1 + c_offset, p2 + c_offset, pzfd, resolx, c_resoly, s_switchIncr, s_switchMult);

	/*
	 * Affichage tentacule
	 */

	if (s_goomLimit!=0)
        tentacle_update((gint32*)(p2 + c_offset), (gint32*)(p1 + c_offset), resolx, c_resoly, data, (float)s_accelVar/s_goomLimit, curGState->m_drawTentacle);
	else
        tentacle_update((gint32*)(p2 + c_offset), (gint32*)(p1 + c_offset), resolx, c_resoly, data,0.0F, curGState->m_drawTentacle);

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
				goom_draw_text (p1 + c_offset,resolx,c_resoly, 10, 50, text, 1.0F, 0);
			}
			if (accelvar>0) {
				for (i=0;i<accelvar;i++) {
					if (i==goomlimit)
						text[i]='o';
					else
						text[i]='*';
				}
				text[i]=0;
				goom_draw_text (p1 + c_offset,resolx,c_resoly, 10, 62, text, 1.0F, 0);
			}
			if (agoom==20)
				goom_draw_text (p1 + c_offset,resolx,c_resoly,10, 80, "GOOM",1.0F, 0);
			else if (agoom)
				goom_draw_text (p1 + c_offset,resolx,c_resoly,10, 80, "goom",1.0F, 0);
			if (abiggoom==200)
				goom_draw_text (p1 + c_offset,resolx,c_resoly,10, 100, "BGOOM",1.0F, 0);
			else if (abiggoom)
				goom_draw_text (p1 + c_offset,resolx,c_resoly,10, 100, "bgoom",1.0F, 0);
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
			goom_draw_text (p1 + c_offset,resolx,c_resoly, resolx / 2, c_resoly / 2 + 7, title, ((float) (190 - displayTitle) / 10.0F), 1);
			displayTitle--;
			if (displayTitle < 4)
				goom_draw_text (p2 + c_offset,resolx,c_resoly, resolx / 2, c_resoly / 2 + 7, title, ((float) (190 - displayTitle) / 10.0F), 1);
		}
	}
*/

	/*
	 * arret demande
	 */
	if ((s_stopLines & 0xf000)||(!curGState->m_drawScope)) {
		float   param1 = NAN;
		float   param2 = NAN;
		float   amplitude = NAN;
		int     couleur = 0;
		int     mode = 0;
		
		choose_a_goom_line (&param1, &param2, &couleur, &mode, &amplitude,1);
		couleur = GML_BLACK;
		
		goom_lines_switch_to (gmline1, mode, param1, amplitude, couleur);
		goom_lines_switch_to (gmline2, mode, param2, amplitude, couleur);
		s_stopLines &= 0x0fff;
	}
	
	/*
	 * arret aleatore.. changement de mode de ligne..
	 */
	if (s_lineMode != DRAWLINES) {
		s_lineMode--;
		if (s_lineMode == -1)
			s_lineMode = 0;
	}
	else
		if ((cycle%80==0)&&(iRAND(5)==0)&&s_lineMode)
		{
			s_lineMode--;
		}

	if ((cycle % 120 == 0)
			&& (iRAND (4) == 0)
			&& (curGState->m_drawScope)) {
		if (s_lineMode == 0)
			s_lineMode = DRAWLINES;
		else if (s_lineMode == DRAWLINES) {
			float   param1 = NAN;
			float   param2 = NAN;
			float   amplitude = NAN;
			int     couleur1 = 0;
			int     couleur2 = 0;
			int     mode = 0;

			s_lineMode--;
			choose_a_goom_line (&param1, &param2, &couleur1, &mode, &amplitude,s_stopLines);

			couleur2 = 5-couleur1;
			if (s_stopLines) {
				s_stopLines--;
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
	if ((s_lineMode != 0) || (s_aGoom > 15)) {
		gmline2->power = gmline1->power;

		goom_lines_draw (gmline1, data[0], p2 + c_offset);
		goom_lines_draw (gmline2, data[1], p2 + c_offset);

		if (((cycle % 121) == 9) && (iRAND (3) == 1)
				&& ((s_lineMode == 0) || (s_lineMode == DRAWLINES))) {
			float   param1 = NAN;
			float   param2 = NAN;
			float   amplitude = NAN;
			int     couleur1 = 0;
			int     couleur2 = 0;
			int     mode = 0;

			choose_a_goom_line (&param1, &param2, &couleur1, &mode, &amplitude, s_stopLines);
			couleur2 = 5-couleur1;
			
			if (s_stopLines) {
				s_stopLines--;
				if (iRAND(2))
					couleur2=couleur1 = GML_BLACK;
			}
			goom_lines_switch_to (gmline1, mode, param1, amplitude, couleur1);
			goom_lines_switch_to (gmline2, mode, param2, amplitude, couleur2);
		}
	}

	guint32 *return_val = p1;
	tmp = p1;
	p1 = p2;
	p2 = tmp;

	// affichage et swappage des buffers..
	cycle++;

	// toute les 2 secondes : vérifier si le taux de goom est correct
	// et le modifier sinon..
	if (!(cycle % 64)) {
		if (s_speedVar<1)
			s_goomLimit /= 2;
		if (s_totalGoom > 4) {
			s_goomLimit++;
		}
		if (s_totalGoom > 7) {
			s_goomLimit*=4/3;
			s_goomLimit+=2;
		}
		if ((s_totalGoom == 0) && (s_goomLimit > 1))
			s_goomLimit--;
		if ((s_totalGoom == 1) && (s_goomLimit > 1))
			s_goomLimit--;
		s_totalGoom = 0;
	}
	return return_val;
}

void goom_close () {
	if (pixel != nullptr)
		free (pixel);
	if (back != nullptr)
		free (back);
	pixel = back = nullptr;
	RAND_CLOSE ();
	release_ifs ();
	goom_lines_free (&gmline1);
	goom_lines_free (&gmline2);
	tentacle_free();
}


void choose_a_goom_line (float *param1, float *param2, int *couleur, int *mode, float *amplitude, int far) {
	*mode = iRAND (3);
	*amplitude = 1.0F;
	switch (*mode) {
	case GML_CIRCLE:
		if (far) {
			*param1 = *param2 = 0.47F;
			*amplitude = 0.8F;
			break;
		}
		if (iRAND (3) == 0) {
			*param1 = *param2 = 0;
			*amplitude = 3.0F;
		}
		else if (iRAND (2)) {
			*param1 = 0.40F * c_resoly;
			*param2 = 0.22F * c_resoly;
		}
		else {
			*param1 = *param2 = c_resoly * 0.35F;
		}
		break;
	case GML_HLINE:
		if (iRAND (4) || far) {
			*param1 = c_resoly / 7.0F;
			*param2 = 6.0F * c_resoly / 7.0F;
		}
		else {
			*param1 = *param2 = c_resoly / 2.0F;
			*amplitude = 2.0F;
		}
		break;
	case GML_VLINE:
		if (iRAND (3) || far) {
			*param1 = resolx / 7.0F;
			*param2 = 6.0F * resolx / 7.0F;
		}
		else {
			*param1 = *param2 = resolx / 2.0F;
			*amplitude = 1.5F;
		}
		break;
	}

	*couleur = iRAND (6);
}

/*
void goom_set_font (int ***chars, int *width, int *height) {
    if (chars == nullptr)
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

guint32 goom_rand (void)
{
    static std::random_device rd;
    static std::mt19937 mt(rd());
    return mt();
}
