#ifndef VIDEOVISUAL_DEFS_H
#define VIDEOVISUAL_DEFS_H

#include <cmath>

class LogScale
{
  public:
    LogScale(int maxscale = 0, int maxrange = 0)
    {
        setMax(maxscale, maxrange);
    }

   ~LogScale()
    {
        delete [] m_indices;
    }

    int scale() const { return m_s; }
    int range() const { return m_r; }

    void setMax(int maxscale, int maxrange)
    {
        if (maxscale == 0 || maxrange == 0)
            return;

        m_s = maxscale;
        m_r = maxrange;

        delete [] m_indices;

        double alpha;
        long double domain = (long double) maxscale;
        long double range  = (long double) maxrange;
        long double x  = 1.0;
        long double dx = 1.0;
        long double e4 = 1.0E-8;

        m_indices = new int[maxrange];
        for (int i = 0; i < maxrange; i++)
            m_indices[i] = 0;

        // initialize log scale
        for (uint i = 0; i < 10000 && (std::abs(dx) > e4); i++)
        {
            long double t = std::log((domain + x) / x);
            long double y = (x * t) - range;
            long double yy = t - (domain / (x + domain));
            dx = y / yy;
            x -= dx;
        }

        alpha = x;
        for (int i = 1; i < (int) domain; i++)
        {
            int scaled = (int) floor(0.5 + (alpha * log((double(i) + alpha) / alpha)));
            if (scaled < 1)
                scaled = 1;
            if (m_indices[scaled - 1] < i)
                m_indices[scaled - 1] = i;
        }
    }

    int operator[](int index) const
    {
        return m_indices[index];
    }


  private:
    int *m_indices {nullptr};
    int  m_s       {0};
    int  m_r       {0};
};

static inline void stereo16_from_stereopcm8(short *l,
                        short *r,
                        uchar *c,
                        long cnt)
{
    while (cnt >= 4L)
    {
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

    if (cnt > 0L)
    {
        l[0] = c[0];
        r[0] = c[1];
        if (cnt > 1L)
        {
            l[1] = c[2];
            r[1] = c[3];
            if (cnt > 2L)
            {
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
    while (cnt >= 4L)
    {
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

    if (cnt > 0L)
    {
        l[0] = s[0];
        r[0] = s[1];
        if (cnt > 1L)
        {
            l[1] = s[2];
            r[1] = s[3];
            if (cnt > 2L)
            {
                l[2] = s[4];
                r[2] = s[5];
            }
        }
    }
}

static inline void mono16_from_monopcm8(short *l,
                    uchar *c,
                    long cnt)
{
    while (cnt >= 4L)
    {
        l[0] = c[0];
        l[1] = c[1];
        l[2] = c[2];
        l[3] = c[3];
        l += 4;
        c += 4;
        cnt -= 4L;
    }

    if (cnt > 0L)
    {
        l[0] = c[0];
        if (cnt > 1L)
        {
            l[1] = c[1];
            if (cnt > 2L)
            {
            l[2] = c[2];
            }
        }
    }
}

static inline void mono16_from_monopcm16(short *l,
                     short *s,
                     long cnt)
{
    while (cnt >= 4L)
    {
        l[0] = s[0];
        l[1] = s[1];
        l[2] = s[2];
        l[3] = s[3];
        l += 4;
        s += 4;
        cnt -= 4L;
    }

    if (cnt > 0L)
    {
        l[0] = s[0];
        if (cnt > 1L)
        {
            l[1] = s[1];
            if (cnt > 2L)
            {
                l[2] = s[2];
            }
        }
    }
}

#ifdef FFTW3_SUPPORT

#include <complex>
extern "C" {
#include <fftw3.h>
#define myth_fftw_float double /* need to use different plan function to change */
#define fftw_real myth_fftw_float
#define myth_fftw_complex std::complex<myth_fftw_float>
#if (myth_fftw_float == double)
#define myth_fftw_complex_cast fftw_complex
#elif (myth_fftw_float == float)
#define myth_fftw_complex_cast fftwf_complex
#endif
}

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
#endif // VIDEOVISUAL_DEFS_H
