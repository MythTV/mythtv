#include <stdlib.h>

#include "v3d.h"
#include "surf3d.h"
#include "goom_tools.h"
#include "goomconfig.h"

#define D 256.0f

#define nbgrid 6
#define definitionx 15
#define definitionz 45

static float cycle = 0.0f;
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
		int x,z;
		z = 45+rand()%30;
		x = 85+rand()%5;
		center.z = z;
		grille[tmp] = grid3d_new (x,definitionx,z,definitionz+rand()%10,center);
		center.y += 8;
	}
}

static inline unsigned char
lighten (unsigned char value, float power)
{
	int     val = value;
	float   t = (float) val * log10(power) / 2.0;

	if (t > 0) {
		val = (int) t; // (32.0f * log (t));
		if (val > 255)
			val = 255;
		if (val < 0)
			val = 0;
		return val;
	}
	else {
		return 0;
	}
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
#define ShiftRight(_x,_s) ((_x<0) ? -(-_x>>_s) : (_x>>_s))

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

static void pretty_move (float cycle, float *dist,float *dist2, float *rotangle) {
	static float distt = 10.0f;
	static float distt2 = 0.0f;
	static float rot = 0.0f; // entre 0 et 2 * M_PI
	static int happens = 0;
	float tmp;
	static int rotation = 0;
	static int lock = 0;

	if (happens)
		happens -= 1;
	else if (lock == 0) {
		happens = iRAND(200)?0:100+iRAND(60);
		lock = happens * 3 / 2;
	}
	else lock --;
//	happens = 1;
	
	tmp = happens?8.0f:0;
	*dist2 = distt2 = (tmp + 15.0f*distt2)/16.0f;

	tmp = 30+D-90.0f*(1.0f+sin(cycle*19/20));
	if (happens)
		tmp *= 0.6f;

	*dist = distt = (tmp + 3.0f*distt)/4.0f;

	if (!happens){
		tmp = M_PI*sin(cycle)/32+3*M_PI/2;
	}
	else {
		rotation = iRAND(500)?rotation:iRAND(2);
		if (rotation)
			cycle *= 2.0f*M_PI;
		else
			cycle *= -1.0f*M_PI;
		tmp = cycle - (M_PI*2.0) * floor(cycle/(M_PI*2.0));
	}
	
	if (abs(tmp-rot) > abs(tmp-(rot+2.0*M_PI))) {
		rot = (tmp + 15.0f*(rot+2*M_PI)) / 16.0f;
		if (rot>2.0*M_PI)
			rot -= 2.0*M_PI;
		*rotangle = rot;
	}
	else if (abs(tmp-rot) > abs(tmp-(rot-2.0*M_PI))) {
		rot = (tmp + 15.0f*(rot-2.0*M_PI)) / 16.0f;
		if (rot<0.0f)
			rot += 2.0*M_PI;
		*rotangle = rot;
	}
	else
		*rotangle = rot = (tmp + 15.0f*rot) / 16.0f;
}

void tentacle_update(int *buf, int *back, int W, int H, short data[2][512], float rapport, int drawit) {
	int tmp;
	int tmp2;
	
	static int colors[] = {
		(0x18<<(ROUGE*8))|(0x4c<<(VERT*8))|(0x2f<<(BLEU*8)),
		(0x48<<(ROUGE*8))|(0x2c<<(VERT*8))|(0x6f<<(BLEU*8)),
		(0x58<<(ROUGE*8))|(0x3c<<(VERT*8))|(0x0f<<(BLEU*8))};
	
	static int col = (0x28<<(ROUGE*8))|(0x2c<<(VERT*8))|(0x5f<<(BLEU*8));
	static int dstcol = 0;
	static float lig = 1.15f;
	static float ligs = 0.1f;

	int color;
	int colorlow;

	float dist,dist2,rotangle;

	if ((!drawit) && (ligs>0.0f))
		ligs = -ligs;

	lig += ligs;

	if (lig > 1.01f) {
		if ((lig>10.0f) | (lig<1.1f)) ligs = -ligs;
		
		if ((lig<6.3f)&&(iRAND(30)==0))
			dstcol=iRAND(3);

		col = evolutecolor(col,colors[dstcol],0xff,0x01);
		col = evolutecolor(col,colors[dstcol],0xff00,0x0100);
		col = evolutecolor(col,colors[dstcol],0xff0000,0x010000);
		col = evolutecolor(col,colors[dstcol],0xff000000,0x01000000);
		
		color = col;
		colorlow = col;
		
		lightencolor(&color,lig * 2.0f + 2.0f);
		lightencolor(&colorlow,(lig/3.0f)+0.67f);

		rapport = 1.0f + 2.0f * (rapport - 1.0f);
                rapport *= 1.2f;
		if (rapport > 1.12f)
			rapport = 1.12f;
		
		pretty_move (cycle,&dist,&dist2,&rotangle);

		for (tmp=0;tmp<nbgrid;tmp++) {
			for (tmp2=0;tmp2<definitionx;tmp2++) {
				float val = (float)(ShiftRight(data[0][iRAND(511)],10)) * rapport;
				vals[tmp2] = val;
			}
			
			grid3d_update (grille[tmp],rotangle, vals, dist2);
		}
		cycle+=0.01f;
		for (tmp=0;tmp<nbgrid;tmp++)
			grid3d_draw (grille[tmp],color,colorlow,dist,buf,back,W,H);
	}
	else {
		lig = 1.05f;
		if (ligs < 0.0f)
			ligs = -ligs;
		pretty_move (cycle,&dist,&dist2,&rotangle);
		cycle+=0.1f;
		if (cycle > 1000)
			cycle = 0;
	}
}
