// Copyright (c) 2000-2001 Brad Hughes <bhughes@trolltech.com>
//
// Use, modification and distribution is allowed without limitation,
// warranty, or liability of any kind.
//

#ifndef INLINES_H
#define INLINES_H

// *fast* convenience functions

static inline void stereo16_from_stereopcm8(short *l,
					    short *r,
					    const uchar *c,
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
					     const short *s,
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
					const uchar *c,
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
					 const short *s,
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

#endif // INLINES_H
