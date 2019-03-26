// Copyright (c) 2000-2001 Brad Hughes <bhughes@trolltech.com>
//
// Use, modification and distribution is allowed without limitation,
// warranty, or liability of any kind.
//

#ifndef INLINES_H
#define INLINES_H

#include "config.h"

// *fast* convenience functions

static inline void stereo16_from_stereopcm8(short *l,
					    short *r,
					    uchar *c,
					    long cnt)
{
    while (cnt >= 4L) {
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
	cnt -= 4L;
    }

    if (cnt > 0L) {
	l[0] = c[0];
	r[0] = c[1];
	if (cnt > 1L) {
	    l[1] = c[2];
	    r[1] = c[3];
	    if (cnt > 2L) {
		l[2] = c[4];
		r[2] = c[5];
	    }
	}
    }
}


static inline void stereo16_from_stereopcm16(short *l,
					     short *r,
					     short *s,
					     long cnt)
{
    while (cnt >= 4L) {
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
	cnt -= 4L;
    }

    if (cnt > 0L) {
	l[0] = s[0];
	r[0] = s[1];
	if (cnt > 1L) {
	    l[1] = s[2];
	    r[1] = s[3];
	    if (cnt > 2L) {
		l[2] = s[4];
		r[2] = s[5];
	    }
	}
    }
}


static inline void stereo16_from_stereopcm32(short *l,
                         short *r,
                         int *s,
                         long cnt)
{
    while (cnt--) {
  *l++ = (short)(*s++ >> 16);
  *r++ = (short)(*s++ >> 16);
    }
}


static inline void stereo16_from_stereopcmfloat(short *l,
                         short *r,
                         float *s,
                         long cnt)
{
    while (cnt--) {
  *l++ = (short)(*s++ * 32767.0F);
  *r++ = (short)(*s++ * 32767.0F);
    }
}


static inline void mono16_from_monopcm8(short *l,
					uchar *c,
					long cnt)
{
    while (cnt >= 4L) {
	l[0] = c[0];
	l[1] = c[1];
	l[2] = c[2];
	l[3] = c[3];
	l += 4;
	c += 4;
	cnt -= 4L;
    }

    if (cnt > 0L) {
	l[0] = c[0];
	if (cnt > 1L) {
	    l[1] = c[1];
	    if (cnt > 2L) {
		l[2] = c[2];
	    }
	}
    }
}


static inline void mono16_from_monopcm16(short *l,
					 short *s,
					 long cnt)
{
    while (cnt >= 4L) {
	l[0] = s[0];
	l[1] = s[1];
	l[2] = s[2];
	l[3] = s[3];
	l += 4;
	s += 4;
	cnt -= 4L;
    }

    if (cnt > 0L) {
	l[0] = s[0];
	if (cnt > 1L) {
	    l[1] = s[1];
	    if (cnt > 2L) {
		l[2] = s[2];
	    }
	}
    }
}


static inline void mono16_from_monopcm32(short *l,
                         int *s,
                         long cnt)
{
    while (cnt--)
  *l++ = (short)(*s++ >> 16);
}


static inline void mono16_from_monopcmfloat(short *l,
                         float *s,
                         long cnt)
{
    while (cnt--)
  *l++ = (short)(*s++ * 32767.0F);
}


#if FFTW3_SUPPORT
static inline void fast_short_set(short *p,
				  short v,
				  long c)
{
    while (c >= 4L) {
	p[0] = v;
	p[1] = v;
	p[2] = v;
	p[3] = v;
	p += 4;
	c -= 4L;
    }

    if (c > 0L) {
	p[0] = v;
	if (c > 1L) {
	    p[1] = v;
	    if (c > 2L) {
		p[2] = v;
	    }
	}
    }
}


static inline void fast_real_set_from_short(fftw_real *d,
					    short *s,
					    long c)
{
    while (c >= 4L) {
	d[0] = fftw_real(s[0]);
	d[1] = fftw_real(s[1]);
	d[2] = fftw_real(s[2]);
	d[3] = fftw_real(s[3]);
	d += 4;
	s += 4;
	c -= 4L;
    }

    if (c > 0L) {
	d[0] = fftw_real(s[0]);
	if (c > 1L) {
	    d[1] = fftw_real(s[1]);
	    if (c > 2L) {
		d[2] = fftw_real(s[2]);
	    }
	}
    }
}

static inline void fast_reals_set(fftw_real *p1,
				  fftw_real *p2,
				  fftw_real v,
				  long c)
{
    while (c >= 4L) {
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
	c -= 4L;
    }

    if (c > 0L) {
	p1[0] = v;
	p2[0] = v;
	if (c > 1L) {
	    p1[1] = v;
	    p2[1] = v;
	    if (c > 2L) {
		p1[2] = v;
		p2[2] = v;
	    }
	}
    }
}
#endif // FFTW3_SUPPORT

#endif // INLINES_H
