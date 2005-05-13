// Copyright (c) 2000-2001 Brad Hughes <bhughes@trolltech.com>
//
// Use, modification and distribution is allowed without limitation,
// warranty, or liability of any kind.
//

#ifndef INLINES_H
#define INLINES_H

#include "config.h"

// *fast* convenience functions

static inline void stereo16_from_stereopcm8(register short *l,
					    register short *r,
					    register uchar *c,
					    long cnt)
{
    while (cnt >= 4l) {
	l[0] = c[0];
	r[0] = c[1];
	l[1] = c[2];
	r[1] = c[3];
	l[2] = c[4];
	r[2] = c[5];
	l[3] = c[6];
	r[3] = c[7];
	l += 4;
	r += 4;
	c += 8;
	cnt -= 4l;
    }

    if (cnt > 0l) {
	l[0] = c[0];
	r[0] = c[1];
	if (cnt > 1l) {
	    l[1] = c[2];
	    r[1] = c[3];
	    if (cnt > 2l) {
		l[2] = c[4];
		r[2] = c[5];
	    }
	}
    }
}


static inline void stereo16_from_stereopcm16(register short *l,
					     register short *r,
					     register short *s,
					     long cnt)
{
    while (cnt >= 4l) {
	l[0] = s[0];
	r[0] = s[1];
	l[1] = s[2];
	r[1] = s[3];
	l[2] = s[4];
	r[2] = s[5];
	l[3] = s[6];
	r[3] = s[7];
	l += 4;
	r += 4;
	s += 8;
	cnt -= 4l;
    }

    if (cnt > 0l) {
	l[0] = s[0];
	r[0] = s[1];
	if (cnt > 1l) {
	    l[1] = s[2];
	    r[1] = s[3];
	    if (cnt > 2l) {
		l[2] = s[4];
		r[2] = s[5];
	    }
	}
    }
}


static inline void mono16_from_monopcm8(register short *l,
					register uchar *c,
					long cnt)
{
    while (cnt >= 4l) {
	l[0] = c[0];
	l[1] = c[1];
	l[2] = c[2];
	l[3] = c[3];
	l += 4;
	c += 4;
	cnt -= 4l;
    }

    if (cnt > 0l) {
	l[0] = c[0];
	if (cnt > 1l) {
	    l[1] = c[1];
	    if (cnt > 2l) {
		l[2] = c[2];
	    }
	}
    }
}


static inline void mono16_from_monopcm16(register short *l,
					 register short *s,
					 long cnt)
{
    while (cnt >= 4l) {
	l[0] = s[0];
	l[1] = s[1];
	l[2] = s[2];
	l[3] = s[3];
	l += 4;
	s += 4;
	cnt -= 4l;
    }

    if (cnt > 0l) {
	l[0] = s[0];
	if (cnt > 1l) {
	    l[1] = s[1];
	    if (cnt > 2l) {
		l[2] = s[2];
	    }
	}
    }
}

#if defined(FFTW3_SUPPORT) || defined(FFTW2_SUPPORT)

static inline void fast_short_set(register short *p,
				  short v,
				  long c)
{
    while (c >= 4l) {
	p[0] = v;
	p[1] = v;
	p[2] = v;
	p[3] = v;
	p += 4;
	c -= 4l;
    }

    if (c > 0l) {
	p[0] = v;
	if (c > 1l) {
	    p[1] = v;
	    if (c > 2l) {
		p[2] = v;
	    }
	}
    }
}


static inline void fast_real_set_from_short(register fftw_real *d,
					    register short *s,
					    long c)
{
    while (c >= 4l) {
	d[0] = fftw_real(s[0]);
	d[1] = fftw_real(s[1]);
	d[2] = fftw_real(s[2]);
	d[3] = fftw_real(s[3]);
	d += 4;
	s += 4;
	c -= 4l;
    }

    if (c > 0l) {
	d[0] = fftw_real(s[0]);
	if (c > 1l) {
	    d[1] = fftw_real(s[1]);
	    if (c > 2l) {
		d[2] = fftw_real(s[2]);
	    }
	}
    }
}

static inline void fast_reals_set(register fftw_real *p1,
				  register fftw_real *p2,
				  fftw_real v,
				  long c)
{
    while (c >= 4l) {
	p1[0] = v;
	p1[1] = v;
	p1[2] = v;
	p1[3] = v;
	p2[0] = v;
	p2[1] = v;
	p2[2] = v;
	p2[3] = v;
	p1 += 4;
	p2 += 4;
	c -= 4l;
    }

    if (c > 0l) {
	p1[0] = v;
	p2[0] = v;
	if (c > 1l) {
	    p1[1] = v;
	    p2[1] = v;
	    if (c > 2l) {
		p1[2] = v;
		p2[2] = v;
	    }
	}
    }
}

#endif //defined(FFTW3_SUPPORT) || defined(FFTW2_SUPPORT)

#endif // INLINES_H
