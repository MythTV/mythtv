/* -*- Mode: C; tab-width: 4 -*- */
/* ifs --- modified iterated functions system */

//#if !defined( lint ) && !defined( SABER )
//static const char sccsid[] = "@(#)ifs.c	5.00 2002/04/11 baffe";
//#endif

/*-
 * Copyright (c) 1997 by Massimino Pascal <Pascal.Massimon@ens.fr>
 *
 * Permission to use, copy, modify, and distribute this software and its
 * documentation for any purpose and without fee is hereby granted,
 * provided that the above copyright notice appear in all copies and that
 * both that copyright notice and this permission notice appear in
 * supporting documentation.
 *
 * This file is provided AS IS with no warranties of any kind.  The author
 * shall have no liability with respect to the infringement of copyrights,
 * trade secrets or any patents by this file or any part thereof.  In no
 * event will the author be liable for any lost revenue or profits or
 * other special, indirect and consequential damages.
 *
 * If this mode is weird and you have an old MetroX server, it is buggy.
 * There is a free SuSE-enhanced MetroX X server that is fine.
 *
 * When shown ifs, Diana Rose (4 years old) said, "It looks like dancing."
 *
 * Revision History:
 * 11-Apr-2002: Make ifs.c system-indendant. (ifs.h added)
 * 01-Nov-2000: Allocation checks
 * 10-May-1997: jwz@jwz.org: turned into a standalone program.
 *              Made it render into an offscreen bitmap and then copy
 *              that onto the screen, to reduce flicker.
 */

//#ifdef STANDALONE

#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>

#include "ifs.h"
#include "goom_core.h"
#include "goom_tools.h"


static inline long LRAND()      { return static_cast<long>( RAND() & 0x7fffffff); }
static inline int  NRAND(int n) { return static_cast<int>( LRAND() % n ); }
static constexpr double MAXRAND { 2147483648.0 }; /* unsigned 1<<31 as a * * * * float */

/*****************************************************/

using DBL = double;
using F_PT = float;

/*****************************************************/

static constexpr uint8_t  FIX      { 12     };
static constexpr uint16_t UNIT     { 1<<FIX };
static constexpr size_t   MAX_SIMI { 6      };

static constexpr int8_t   MAX_DEPTH_2  { 10 };
static constexpr int8_t   MAX_DEPTH_3  {  6 };
static constexpr int8_t   MAX_DEPTH_4  {  4 };
static constexpr int8_t   MAX_DEPTH_5  {  2 };

static inline F_PT DBL_To_F_PT(DBL x)
{ return static_cast<F_PT>( static_cast<DBL>(UNIT) * x ); };

using SIMI = struct Similitude_Struct;
using FRACTAL = struct Fractal_Struct;

struct Similitude_Struct
{

	DBL     m_dCx, m_dCy;
	DBL     m_dR, m_dR2, m_dA, m_dA2;
	F_PT    m_fCt, m_fSt, m_fCt2, m_fSt2;
	F_PT    m_fCx, m_fCy;
	F_PT    m_fR, m_fR2;
};

using SimiData = std::array<SIMI,5 * MAX_SIMI>;

struct Fractal_Struct
{

	size_t   m_nbSimi;
	SimiData m_components;
	int     m_depth, m_col;
	int     m_count, m_speed;
	int     m_width, m_height, m_lx, m_ly;
	DBL     m_rMean, m_drMean, m_dr2Mean;
	int     m_curPt, m_maxPt;

	IFSPoint *m_buffer1, *m_buffer2;
//      Pixmap      dbuf;
//      GC          dbuf_gc;
};

static FRACTAL *Root = (FRACTAL *) nullptr, *Cur_F;

/* Used by the Trace recursive method */
IFSPoint *Buf;
static int Cur_Pt;

/*****************************************************/

static  DBL
Gauss_Rand (DBL c, DBL A, DBL S)
{
	DBL y = (DBL) LRAND () / MAXRAND;
	y = A * (1.0 - exp (-y * y * S)) / (1.0 - exp (-S));
	if (NRAND (2))
		return (c + y);
	return (c - y);
}

static  DBL
Half_Gauss_Rand (DBL c, DBL A, DBL S)
{
	DBL y = (DBL) LRAND () / MAXRAND;
	y = A * (1.0 - exp (-y * y * S)) / (1.0 - exp (-S));
	return (c + y);
}

static void
Random_Simis (FRACTAL * F, SimiData &simi_set, int offset, int count)
{
	SIMI * Cur = &simi_set[offset];
	while (count--) {
		Cur->m_dCx = Gauss_Rand (0.0, .8, 4.0);
		Cur->m_dCy = Gauss_Rand (0.0, .8, 4.0);
		Cur->m_dR = Gauss_Rand (F->m_rMean, F->m_drMean, 3.0);
		Cur->m_dR2 = Half_Gauss_Rand (0.0, F->m_dr2Mean, 2.0);
		Cur->m_dA = Gauss_Rand (0.0, 360.0, 4.0) * (M_PI / 180.0);
		Cur->m_dA2 = Gauss_Rand (0.0, 360.0, 4.0) * (M_PI / 180.0);
		Cur++;
	}
}

static void
free_ifs_buffers (FRACTAL * Fractal)
{
	if (Fractal->m_buffer1 != nullptr) {
		free ((void *) Fractal->m_buffer1);
		Fractal->m_buffer1 = (IFSPoint *) nullptr;
	}
	if (Fractal->m_buffer2 != nullptr) {
		free ((void *) Fractal->m_buffer2);
		Fractal->m_buffer2 = (IFSPoint *) nullptr;
	}
}


static void
free_ifs (FRACTAL * Fractal)
{
	free_ifs_buffers (Fractal);
}

/***************************************************************/

void
init_ifs (int width, int height)
{
//      printf ("initing ifs\n");

	if (Root == nullptr) {
		Root = (FRACTAL *) malloc (sizeof (FRACTAL));
		if (Root == nullptr)
			return;
		Root->m_buffer1 = (IFSPoint *) nullptr;
		Root->m_buffer2 = (IFSPoint *) nullptr;
	}
	FRACTAL *Fractal = Root;

//      fprintf (stderr,"--ifs freeing ex-buffers\n");
	free_ifs_buffers (Fractal);
//      fprintf (stderr,"--ifs ok\n");

	int i = (NRAND (4)) + 2;					/* Number of centers */
	switch (i) {
	case 3:
		Fractal->m_depth = MAX_DEPTH_3;
		Fractal->m_rMean = .6;
		Fractal->m_drMean = .4;
		Fractal->m_dr2Mean = .3;
		break;

	case 4:
		Fractal->m_depth = MAX_DEPTH_4;
		Fractal->m_rMean = .5;
		Fractal->m_drMean = .4;
		Fractal->m_dr2Mean = .3;
		break;

	case 5:
		Fractal->m_depth = MAX_DEPTH_5;
		Fractal->m_rMean = .5;
		Fractal->m_drMean = .4;
		Fractal->m_dr2Mean = .3;
		break;

	default:
	case 2:
		Fractal->m_depth = MAX_DEPTH_2;
		Fractal->m_rMean = .7;
		Fractal->m_drMean = .3;
		Fractal->m_dr2Mean = .4;
		break;
	}
//      fprintf( stderr, "N=%d\n", i );
	Fractal->m_nbSimi = i;
	Fractal->m_maxPt = Fractal->m_nbSimi - 1;
	for (i = 0; i <= Fractal->m_depth + 2; ++i)
		Fractal->m_maxPt *= Fractal->m_nbSimi;

	Fractal->m_buffer1 = (IFSPoint *) calloc (Fractal->m_maxPt, sizeof (IFSPoint));
	if (Fractal->m_buffer1 == nullptr) {
		free_ifs (Fractal);
		return;
	}
	Fractal->m_buffer2 = (IFSPoint *) calloc (Fractal->m_maxPt, sizeof (IFSPoint));
	if (Fractal->m_buffer2 == nullptr) {
		free_ifs (Fractal);
		return;
	}

//      printf ("--ifs setting params\n");
	Fractal->m_speed = 6;
	Fractal->m_width = width;			/* modif by JeKo */
	Fractal->m_height = height;			/* modif by JeKo */
	Fractal->m_curPt = 0;
	Fractal->m_count = 0;
	Fractal->m_lx = (Fractal->m_width - 1) / 2;
	Fractal->m_ly = (Fractal->m_height - 1) / 2;
	Fractal->m_col = goom_rand () % (width * height);	/* modif by JeKo */

	Random_Simis (Fractal, Fractal->m_components, 0, 5 * MAX_SIMI);

	/* 
	 * #ifndef NO_DBUF
	 * if (Fractal->dbuf != None)
	 * XFreePixmap(display, Fractal->dbuf);
	 * Fractal->dbuf = XCreatePixmap(display, window,
	 * Fractal->m_width, Fractal->m_height, 1);
	 * * Allocation checked *
	 * if (Fractal->dbuf != None) {
	 * XGCValues   gcv;
	 * 
	 * gcv.foreground = 0;
	 * gcv.background = 0;
	 * gcv.graphics_exposures = False;
	 * gcv.function = GXcopy;
	 * 
	 * if (Fractal->dbuf_gc != None)
	 * XFreeGC(display, Fractal->dbuf_gc);
	 * if ((Fractal->dbuf_gc = XCreateGC(display, Fractal->dbuf,
	 * GCForeground | GCBackground | GCGraphicsExposures | GCFunction,
	 * &gcv)) == None) {
	 * XFreePixmap(display, Fractal->dbuf);
	 * Fractal->dbuf = None;
	 * } else {
	 * XFillRectangle(display, Fractal->dbuf,
	 * Fractal->dbuf_gc, 0, 0, Fractal->m_width, Fractal->m_height);
	 * XSetBackground(display, gc, MI_BLACK_PIXEL(mi));
	 * XSetFunction(display, gc, GXcopy);
	 * }
	 * }
	 * #endif
	 */
	// MI_CLEARWINDOW(mi);

	/* don't want any exposure events from XCopyPlane */
	// XSetGraphicsExposures(display, gc, False);

}


/***************************************************************/

/* Should be taken care of already... but just in case */
#if !defined( __GNUC__ ) && !defined(__cplusplus) && !defined(c_plusplus)
#undef inline
#define inline									/* */
#endif
static inline void
Transform (SIMI * Simi, F_PT xo, F_PT yo, F_PT * x, F_PT * y)
{
	xo = xo - Simi->m_fCx;
	xo = (xo * Simi->m_fR) / UNIT;
	yo = yo - Simi->m_fCy;
	yo = (yo * Simi->m_fR) / UNIT;

	F_PT xx = xo - Simi->m_fCx;
	xx = (xx * Simi->m_fR2) / UNIT;
	F_PT yy = -yo - Simi->m_fCy;
	yy = (yy * Simi->m_fR2) / UNIT;

	*x =
		((xo * Simi->m_fCt - yo * Simi->m_fSt + xx * Simi->m_fCt2 - yy * Simi->m_fSt2) / UNIT ) + Simi->m_fCx;
	*y =
		((xo * Simi->m_fSt + yo * Simi->m_fCt + xx * Simi->m_fSt2 + yy * Simi->m_fCt2) / UNIT ) + Simi->m_fCy;
}

/***************************************************************/

static void
Trace (FRACTAL * F, F_PT xo, F_PT yo)
{
	F_PT    x = NAN;
	F_PT    y = NAN;

	SIMI *Cur = (Cur_F->m_components).data();
	for (int i = Cur_F->m_nbSimi; i != 0; --i, Cur++) {
		Transform (Cur, xo, yo, &x, &y);

		Buf->x = F->m_lx + ((x * F->m_lx) / (UNIT*2) );
		Buf->y = F->m_ly - ((y * F->m_ly) / (UNIT*2) );
		Buf++;

		Cur_Pt++;

		if (F->m_depth && (((x - xo) / 16) != 0.0F) && (((y - yo) / 16) != 0.0F)) {
			F->m_depth--;
			Trace (F, x, y);
			F->m_depth++;
		}
	}
}

static void
Draw_Fractal ( void /* ModeInfo * mi */ )
{
	FRACTAL *F = Root;
	int     i = 0;
	SIMI   *Cur = nullptr;
	SIMI   *Simi = nullptr;

	for (Cur = (F->m_components).data(), i = F->m_nbSimi; i; --i, Cur++) {
		Cur->m_fCx = DBL_To_F_PT (Cur->m_dCx);
		Cur->m_fCy = DBL_To_F_PT (Cur->m_dCy);

		Cur->m_fCt = DBL_To_F_PT (cos (Cur->m_dA));
		Cur->m_fSt = DBL_To_F_PT (sin (Cur->m_dA));
		Cur->m_fCt2 = DBL_To_F_PT (cos (Cur->m_dA2));
		Cur->m_fSt2 = DBL_To_F_PT (sin (Cur->m_dA2));

		Cur->m_fR = DBL_To_F_PT (Cur->m_dR);
		Cur->m_fR2 = DBL_To_F_PT (Cur->m_dR2);
	}


	Cur_Pt = 0;
	Cur_F = F;
	Buf = F->m_buffer2;
	for (Cur = (F->m_components).data(), i = F->m_nbSimi; i; --i, Cur++) {
		F_PT xo = Cur->m_fCx;
		F_PT yo = Cur->m_fCy;
		int j = 0;
		for (Simi = (F->m_components).data(), j = F->m_nbSimi; j; --j, Simi++) {
			F_PT x = NAN;
			F_PT y = NAN;
			if (Simi == Cur)
				continue;
			Transform (Simi, xo, yo, &x, &y);
			Trace (F, x, y);
		}
	}

	/* Erase previous */

/*	if (F->m_curPt) {
		XSetForeground(display, gc, MI_BLACK_PIXEL(mi));
		if (F->dbuf != None) {
			XSetForeground(display, F->dbuf_gc, 0);
*/
	/* XDrawPoints(display, F->dbuf, F->dbuf_gc, F->m_buffer1, F->m_curPt, * * * * 
	 * CoordModeOrigin); */
/*			XFillRectangle(display, F->dbuf, F->dbuf_gc, 0, 0,
				       F->m_width, F->m_height);
		} else
			XDrawPoints(display, window, gc, F->m_buffer1, F->m_curPt, CoordModeOrigin);
	}
	if (MI_NPIXELS(mi) < 2)
		XSetForeground(display, gc, MI_WHITE_PIXEL(mi));
	else
		XSetForeground(display, gc, MI_PIXEL(mi, F->Col % MI_NPIXELS(mi)));
	if (Cur_Pt) {
		if (F->dbuf != None) {
			XSetForeground(display, F->dbuf_gc, 1);
			XDrawPoints(display, F->dbuf, F->dbuf_gc, F->m_buffer2, Cur_Pt,
				    CoordModeOrigin);
		} else
			XDrawPoints(display, window, gc, F->m_buffer2, Cur_Pt, CoordModeOrigin);
	}
	if (F->dbuf != None)
		XCopyPlane(display, F->dbuf, window, gc, 0, 0, F->m_width, F->m_height, 0, 0, 1);
*/

	F->m_curPt = Cur_Pt;
	Buf = F->m_buffer1;
	F->m_buffer1 = F->m_buffer2;
	F->m_buffer2 = Buf;
}


IFSPoint *
draw_ifs ( /* ModeInfo * mi */ int *nbPoints)
{
	if (Root == nullptr)
		return nullptr;
	FRACTAL *F = Root; // [/*MI_SCREEN(mi)*/0];
	if (F->m_buffer1 == nullptr)
		return nullptr;

	DBL u = (DBL) (F->m_count) * (DBL) (F->m_speed) / 1000.0;
	DBL uu = u * u;
	DBL v = 1.0 - u;
	DBL vv = v * v;
	DBL u0 = vv * v;
	DBL u1 = 3.0 * vv * u;
	DBL u2 = 3.0 * v * uu;
	DBL u3 = u * uu;

	SIMI *S  = (F->m_components).data();
	SIMI *S1 = &F->m_components[1 * F->m_nbSimi];
	SIMI *S2 = &F->m_components[2 * F->m_nbSimi];
	SIMI *S3 = &F->m_components[3 * F->m_nbSimi];
	SIMI *S4 = &F->m_components[4 * F->m_nbSimi];

	for (int i = F->m_nbSimi; i; --i, S++, S1++, S2++, S3++, S4++) {
		S->m_dCx = (u0 * S1->m_dCx) + (u1 * S2->m_dCx) + (u2 * S3->m_dCx) + (u3 * S4->m_dCx);
		S->m_dCy = (u0 * S1->m_dCy) + (u1 * S2->m_dCy) + (u2 * S3->m_dCy) + (u3 * S4->m_dCy);
		S->m_dR  = (u0 * S1->m_dR)  + (u1 * S2->m_dR)  + (u2 * S3->m_dR)  + (u3 * S4->m_dR);
		S->m_dR2 = (u0 * S1->m_dR2) + (u1 * S2->m_dR2) + (u2 * S3->m_dR2) + (u3 * S4->m_dR2);
		S->m_dA  = (u0 * S1->m_dA)  + (u1 * S2->m_dA)  + (u2 * S3->m_dA)  + (u3 * S4->m_dA);
		S->m_dA2 = (u0 * S1->m_dA2) + (u1 * S2->m_dA2) + (u2 * S3->m_dA2) + (u3 * S4->m_dA2);
	}

	// MI_IS_DRAWN(mi) = True;

	Draw_Fractal ( /* mi */ );

	if (F->m_count >= 1000 / F->m_speed) {
		S  = (F->m_components).data();
		S1 = &F->m_components[1 * F->m_nbSimi];
		S2 = &F->m_components[2 * F->m_nbSimi];
		S3 = &F->m_components[3 * F->m_nbSimi];
		S4 = &F->m_components[4 * F->m_nbSimi];

		for (int i = F->m_nbSimi; i; --i, S++, S1++, S2++, S3++, S4++) {
			S2->m_dCx = (2.0 * S4->m_dCx) - S3->m_dCx;
			S2->m_dCy = (2.0 * S4->m_dCy) - S3->m_dCy;
			S2->m_dR  = (2.0 * S4->m_dR)  - S3->m_dR;
			S2->m_dR2 = (2.0 * S4->m_dR2) - S3->m_dR2;
			S2->m_dA  = (2.0 * S4->m_dA)  - S3->m_dA;
			S2->m_dA2 = (2.0 * S4->m_dA2) - S3->m_dA2;

			*S1 = *S4;
		}
		Random_Simis (F, F->m_components, 3 * F->m_nbSimi, F->m_nbSimi);

		Random_Simis (F, F->m_components, 4 * F->m_nbSimi, F->m_nbSimi);

		F->m_count = 0;
	}
	else
	{
		F->m_count++;
	}

	F->m_col++;

	/* #1 code added by JeKo */
	(*nbPoints) = Cur_Pt;
	return F->m_buffer2;
	/* #1 end */
}


/***************************************************************/

void
release_ifs ()
{
	if (Root != nullptr) {
		free_ifs(Root);
		free ((void *) Root);
		Root = (FRACTAL *) nullptr;
	}
}
