#include <stdlib.h>

#include "v3d.h"
#include "surf3d.h"
#include "goom_tools.h"
#include "goomconfig.h"

#define D 256.0F

#define nbgrid 6
#define definitionx 15
#define definitionz 45

static float cycle = 0.0F;
static grid3d *grille[nbgrid];
static float *vals;

/* Prototypes to keep gcc from spewing warnings */
void tentacle_free (void);
void tentacle_new (void);
void tentacle_update(int *buf, int *back, int W, int H, short data[2][512], float rapport, int drawit);

void tentacle_free (void) {
        int tmp;
	free (vals);
        for (tmp=0;tmp<nbgrid;tmp++) {
                grid3d_free(&(grille[tmp]));
        }
}

void tentacle_new (void) {
	int tmp;

	v3d center = {0,-17.0,0};
	vals = (float*)malloc ((definitionx+20)*sizeof(float));
	
	for (tmp=0;tmp<nbgrid;tmp++) {
		// Pseudo-random is good enough. Don't need a true random.
		// NOLINTNEXTLINE(cert-msc30-c,cert-msc50-cpp)
		int z = 45+rand()%30;
		// NOLINTNEXTLINE(cert-msc30-c,cert-msc50-cpp)
		int x = 85+rand()%5;
		center.z = z;
		// NOLINTNEXTLINE(cert-msc30-c,cert-msc50-cpp)
		grille[tmp] = grid3d_new (x,definitionx,z,definitionz+rand()%10,center);
		center.y += 8;
	}
}

static inline unsigned char
lighten (unsigned char value, float power)
{
	int     val = value;
	float   t = (float) val * log10f(power) / 2.0F;

	if (t > 0) {
		val = (int) t; // (32.0F * log (t));
		if (val > 255)
			val = 255;
		if (val < 0)
			val = 0;
		return val;
	}
        return 0;
}

static void
lightencolor (int *col, float power)
{
	unsigned char *color;

	color = (unsigned char *) col;
	*color = lighten (*color, power);
	color++;
	*color = lighten (*color, power);
	color++;
	*color = lighten (*color, power);
	color++;
	*color = lighten (*color, power);
}

// retourne x>>s , en testant le signe de x
#define ShiftRight(_x,_s) (((_x)<0) ? -(-(_x)>>(_s)) : ((_x)>>(_s)))

static
int evolutecolor (unsigned int src,unsigned int dest, unsigned int mask, unsigned int incr) {
	int color = src & (~mask);
	src &= mask;
	dest &= mask;

	if ((src!=mask)
			&&(src<dest))
		src += incr;

	if (src>dest)
		src -= incr;
	return (src&mask)|color;
}

static void pretty_move (float lcycle, float *dist,float *dist2, float *rotangle) {
	static float s_distT = 10.0F;
	static float s_distT2 = 0.0F;
	static float s_rot = 0.0F; // entre 0 et 2 * M_PI
	static int s_happens = 0;
	float tmp;
        static int s_rotation = 0;
	static int s_lock = 0;

	if (s_happens)
		s_happens -= 1;
	else if (s_lock == 0) {
		s_happens = iRAND(200)?0:100+iRAND(60);
		s_lock = s_happens * 3 / 2;
	}
	else s_lock --;
//	happens = 1;
	
	tmp = s_happens?8.0F:0;
	*dist2 = s_distT2 = (tmp + 15.0F*s_distT2)/16.0F;

	tmp = 30+D-90.0F*(1.0F+sinf(lcycle*19/20));
	if (s_happens)
		tmp *= 0.6F;

	*dist = s_distT = (tmp + 3.0F*s_distT)/4.0F;

	if (!s_happens){
		tmp = M_PI_F*sinf(lcycle)/32+3*M_PI_F/2;
	}
	else {
		s_rotation = iRAND(500)?s_rotation:iRAND(2);
		if (s_rotation)
			lcycle *= 2.0F*M_PI_F;
		else
			lcycle *= -1.0F*M_PI_F;
		tmp = lcycle - (M_PI_F*2.0F) * floorf(lcycle/(M_PI_F*2.0F));
	}
	
	if (fabsf(tmp-s_rot) > fabsf(tmp-(s_rot+2.0F*M_PI_F))) {
		s_rot = (tmp + 15.0F*(s_rot+2*M_PI_F)) / 16.0F;
		if (s_rot>2.0F*M_PI_F)
			s_rot -= 2.0F*M_PI_F;
		*rotangle = s_rot;
	}
	else if (fabsf(tmp-s_rot) > fabsf(tmp-(s_rot-2.0F*M_PI_F))) {
		s_rot = (tmp + 15.0F*(s_rot-2.0F*M_PI_F)) / 16.0F;
		if (s_rot<0.0F)
			s_rot += 2.0F*M_PI_F;
		*rotangle = s_rot;
	}
	else
		*rotangle = s_rot = (tmp + 15.0F*s_rot) / 16.0F;
}

void tentacle_update(int *buf, int *back, int W, int H, short data[2][512], float rapport, int drawit) {
	static int s_colors[] = {
		(0x18<<(ROUGE*8))|(0x4c<<(VERT*8))|(0x2f<<(BLEU*8)),
		(0x48<<(ROUGE*8))|(0x2c<<(VERT*8))|(0x6f<<(BLEU*8)),
		(0x58<<(ROUGE*8))|(0x3c<<(VERT*8))|(0x0f<<(BLEU*8))};
	
	static int s_col = (0x28<<(ROUGE*8))|(0x2c<<(VERT*8))|(0x5f<<(BLEU*8));
	static int s_dstCol = 0;
	static float s_lig = 1.15F;
	static float s_ligs = 0.1F;

	int color;
	int colorlow;

	float dist;
	float dist2;
	float rotangle;

	if ((!drawit) && (s_ligs>0.0F))
		s_ligs = -s_ligs;

	s_lig += s_ligs;

	if (s_lig > 1.01F) {
		if ((s_lig>10.0F) | (s_lig<1.1F)) s_ligs = -s_ligs;
		
		if ((s_lig<6.3F)&&(iRAND(30)==0))
			s_dstCol=iRAND(3);

		s_col = evolutecolor(s_col,s_colors[s_dstCol],0xff,0x01);
		s_col = evolutecolor(s_col,s_colors[s_dstCol],0xff00,0x0100);
		s_col = evolutecolor(s_col,s_colors[s_dstCol],0xff0000,0x010000);
		s_col = evolutecolor(s_col,s_colors[s_dstCol],0xff000000,0x01000000);
		
		color = s_col;
		colorlow = s_col;
		
		lightencolor(&color,s_lig * 2.0F + 2.0F);
		lightencolor(&colorlow,(s_lig/3.0F)+0.67F);

		rapport = 1.0F + 2.0F * (rapport - 1.0F);
                rapport *= 1.2F;
		if (rapport > 1.12F)
			rapport = 1.12F;
		
		pretty_move (cycle,&dist,&dist2,&rotangle);

		for (int tmp=0;tmp<nbgrid;tmp++) {
			for (int tmp2=0;tmp2<definitionx;tmp2++) {
				float val = (float)(ShiftRight(data[0][iRAND(511)],10)) * rapport;
				vals[tmp2] = val;
			}
			
			grid3d_update (grille[tmp],rotangle, vals, dist2);
		}
		cycle+=0.01F;
		for (int tmp=0;tmp<nbgrid;tmp++)
			grid3d_draw (grille[tmp],color,colorlow,dist,buf,back,W,H);
	}
	else {
		s_lig = 1.05F;
		if (s_ligs < 0.0F)
			s_ligs = -s_ligs;
		pretty_move (cycle,&dist,&dist2,&rotangle);
		cycle+=0.1F;
		if (cycle > 1000)
			cycle = 0;
	}
}
