#ifndef VIDEOVISUAL_DEFS_H
#define VIDEOVISUAL_DEFS_H

#include <algorithm>
#include <cmath>

class LogScale
{
  public:
    explicit LogScale(int maxscale = 0, int maxrange = 0)
    {
        setMax(maxscale, maxrange);
    }

    int scale() const { return m_s; }
    int range() const { return m_r; }

    void setMax(int maxscale, int maxrange)
    {
        if (maxscale == 0 || maxrange == 0)
            return;

        m_s = maxscale;
        m_r = maxrange;

        auto domain = (long double) maxscale;
        auto drange  = (long double) maxrange;
        long double x  = 1.0L;
        long double dx = 1.0L;
        long double e4 = 1.0E-8L;

        m_indices.clear();
        m_indices.resize(maxrange, 0);

        // initialize log scale
        for (uint i = 0; i < 10000 && (std::abs(dx) > e4); i++)
        {
            long double t = std::log((domain + x) / x);
            long double y = (x * t) - drange;
            long double yy = t - (domain / (x + domain));
            dx = y / yy;
            x -= dx;
        }

        double alpha = x;
        for (int i = 1; i < (int) domain; i++)
        {
            int scaled = (int) floor(0.5 + (alpha * log((double(i) + alpha) / alpha)));
            scaled = std::max(scaled, 1);
            m_indices[scaled - 1] = std::max(m_indices[scaled - 1], i);
        }
    }

    int operator[](int index) const
    {
        return m_indices[index];
    }


  private:
    std::vector<int> m_indices;
    int  m_s       {0};
    int  m_r       {0};
};

static inline void stereo16_from_stereopcm8(short *l,
                        short *r,
                        const uchar *c,
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
                         const short *s,
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
                    const uchar *c,
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
                     const short *s,
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

#endif // VIDEOVISUAL_DEFS_H
