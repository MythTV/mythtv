#ifndef OSDSURFACE_H_
#define OSDSURFACE_H_

#include <qregion.h>

static inline unsigned char blendColorsAlpha(int src, int dest, int alpha)
{
    int tmp1, tmp2;

    tmp1 = (src - dest) * alpha;
    tmp2 = dest + ((tmp1 + (tmp1 >> 8) + 0x80) >> 8);
    return tmp2 & 0xff;
}

class OSDSurface
{
  public:
    OSDSurface(int w, int h)
    {
        yuvbuffer = new unsigned char[w * h * 3 / 2];
        y = yuvbuffer;
        u = yuvbuffer + w * h;
        v = u + w * h / 4;
        alpha = new unsigned char[w * h];

        width = w;
        height = h;

        size = width * height;

        for (int i = 0; i < 256; i++)
        {
            for (int j = 0; j < 256; j++)
            {
                int divisor = (i + (j * (255 - i)) / 255);
                if (divisor > 0)
                    pow_lut[i][j] = (i * 255) / divisor;
                else
                    pow_lut[i][j] = 0;
            }
        }

        Clear();
    }

   ~OSDSurface()
    {
        delete [] yuvbuffer;
        delete [] alpha;
    }

    void Clear(void)
    {
        memset(y, 0, size);
        memset(u, 127, size / 4);
        memset(v, 127, size / 4);
        memset(alpha, 0, size);
        usedRegions = QRegion();
    }

    bool IntersectsDrawn(QRect &newrect)
    {
        QMemArray<QRect> rects = usedRegions.rects();
        QMemArray<QRect>::Iterator it = rects.begin();
        for (; it != rects.end(); ++it)
            if (newrect.intersects(*it))
                return true;
        return false;
    }

    void AddRect(QRect &newrect)
    {
        usedRegions = usedRegions.unite(newrect);
    }

    bool Changed(void) { return changed; }
    void SetChanged(bool change) { changed = change; }

    unsigned char *yuvbuffer;

    // just pointers into yuvbuffer
    unsigned char *y;
    unsigned char *u;
    unsigned char *v;

    unsigned char *alpha;

    int width;
    int height;
    int size;

    QRegion usedRegions;

    unsigned char pow_lut[256][256];

    bool changed;
};

#endif
