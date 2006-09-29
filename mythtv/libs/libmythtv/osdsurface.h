#ifndef OSDSURFACE_H_
#define OSDSURFACE_H_

#include <qregion.h>
#include <qmutex.h>
#include "blend.h"

#define MAX_NEG_CROP 1024

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
    OSDSurface(int w, int h);
   ~OSDSurface();

    void Clear(void);
    void ClearUsed(void);
    bool IsClear();

    bool IntersectsDrawn(QRect &newrect);
    void AddRect(QRect &newrect);

    bool Changed(void) { return changed; }
    void SetChanged(bool change)
    {
        changed = change;
        if (change) 
            ++revision;
    }
    int GetRevision() { return revision; }

    void BlendToYV12(unsigned char *yptr,
                     unsigned char *uptr,
                     unsigned char *vptr,
                     int ystride, int ystride, int vstride) const;
    void BlendToARGB(unsigned char *argbptr,
                     uint stride, uint height, bool blendtoblack=false,
                     uint threshold = 0) const;
    void DitherToI44(unsigned char *outbuf, bool ifirst,
                     uint stride, uint height) const;
    void DitherToIA44(unsigned char* outbuf, uint stride, uint height) const;
    void DitherToAI44(unsigned char* outbuf, uint stride, uint height) const;

    int revision;

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
    mutable QMutex usedRegionsLock;

#ifdef MMX
    short int rec_lut[256];
#else
    short int * rec_lut;
#endif
    unsigned char pow_lut[256][256];

    blendregion_ptr blendregionfunc;
    blendcolumn2_ptr blendcolumn2func;
    blendcolumn_ptr blendcolumnfunc;
    blendcolor_ptr blendcolorfunc;
    blendconst_ptr blendconstfunc;

    bool changed;

    bool usemmx;

    unsigned char cropTbl[256 + 2 * MAX_NEG_CROP];
    unsigned char *cm;
};

typedef void (*blendtoyv12_8_fun)(unsigned char *src, unsigned char *dest,
                                  unsigned char *alpha, bool uvplane);

blendtoyv12_8_fun blendtoyv12_8_init(const OSDSurface *surface);

typedef void (*blendtoargb_8_fun)(const OSDSurface *surf, unsigned char *src, 
                                  unsigned char *usrc, unsigned char *vsrc, 
                                  unsigned char *alpha, unsigned char *dest);

blendtoargb_8_fun blendtoargb_8_init(const OSDSurface *surface);
           

struct dither8_context;

typedef void (*dithertoia44_8_fun)(unsigned char *src, unsigned char *dest,
                                   unsigned char *alpha, 
                                   const unsigned char *dmp, int xpos,
                                   dither8_context *context);

dithertoia44_8_fun dithertoia44_8_init(const OSDSurface *surface);
dither8_context *init_dithertoia44_8_context(bool first);
void delete_dithertoia44_8_context(dither8_context *context);

#define SCALEBITS 10
#define ONE_HALF  (1 << (SCALEBITS - 1))
#define FIX(x)    ((int) ((x) * (1<<SCALEBITS) + 0.5))

#define YUV_TO_RGB1(cb1, cr1)\
{\
    cb = ((int)cb1) - 128;\
    cr = ((int)cr1) - 128;\
    r_add = FIX(1.40200) * cr + ONE_HALF;\
    g_add = - FIX(0.34414) * cb - FIX(0.71414) * cr + ONE_HALF;\
    b_add = FIX(1.77200) * cb + ONE_HALF;\
}

#define YUV_TO_RGB2(r, g, b, y1)\
{\
    y0 = ((int)y1) << SCALEBITS;\
    r = cm[(y0 + r_add) >> SCALEBITS];\
    g = cm[(y0 + g_add) >> SCALEBITS];\
    b = cm[(y0 + b_add) >> SCALEBITS];\
}

#define RGBA_OUT(d, r, g, b, a)\
{\
    ((unsigned int *)(d))[0] =  ((a) << 24) | \
                                ((r) << 16) | \
                                ((g) << 8) | \
                                (b);\
}
                       
#endif
