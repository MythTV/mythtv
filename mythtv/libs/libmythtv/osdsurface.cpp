#include "osdsurface.h"
#include "dithertable.h"

#include <algorithm>

#ifdef MMX

extern "C" {
#include "config.h"
#include "dsputil.h"
#include "i386/mmx.h"
}

#endif

OSDSurface::OSDSurface(int w, int h)
{
    yuvbuffer = new unsigned char[w * (h + 2) * 3 / 2];
    y = yuvbuffer;
    u = yuvbuffer + w * h;
    v = u + w * h / 4;
    alpha = new unsigned char[w * (h + 2)];

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

    for (int i = 0; i < 256; i++)
        cropTbl[i + MAX_NEG_CROP] = i;
    for (int i = 0; i < MAX_NEG_CROP; i++)
    {
        cropTbl[i] = 0;
        cropTbl[i + MAX_NEG_CROP + 256] = 255;
    }

    cm = cropTbl + MAX_NEG_CROP;

    Clear();

    usemmx = false;

    blendregionfunc = &blendregion;
    blendcolumn2func = &blendcolumn2;
    blendcolumnfunc = &blendcolumn;
    blendcolorfunc = &blendcolor;
    blendconstfunc = &blendconst;
#if defined(MMX)
    usemmx = (mm_support() & MM_MMX);
    if (usemmx)
    {
        rec_lut[0] = 0;
        for (int i = 1; i < 256; i++)
            rec_lut[i] = ((255 << 7) + (i >> 1)) / i;
        blendregionfunc = &blendregion_mmx;
        blendcolumn2func = &blendcolumn2_mmx;
        blendcolumnfunc = &blendcolumn_mmx;
        blendcolorfunc = &blendcolor_mmx;
        blendconstfunc = &blendconst_mmx;
    }
#endif
    revision = 0;
}

OSDSurface::~OSDSurface()
{
    delete [] yuvbuffer;
    delete [] alpha;
}

void OSDSurface::Clear(void)
{
    QMutexLocker lock(&usedRegionsLock);
    memset(y, 0, size);
    memset(u, 127, size / 4);
    memset(v, 127, size / 4);
    memset(alpha, 0, size);
    usedRegions = QRegion();
}

void OSDSurface::ClearUsed(void)
{
    QMutexLocker lock(&usedRegionsLock);
    QMemArray<QRect> rects = usedRegions.rects();
    QMemArray<QRect>::Iterator it = rects.begin();
    QRect drawRect;
    int startcol, startline, endcol, endline, cwidth, uvcwidth;
    int yoffset;

    for (; it != rects.end(); ++it)
    {
        drawRect = *it;

        startcol = drawRect.left();
        startline = drawRect.top();
        endcol = drawRect.right();
        endline = drawRect.bottom();

        cwidth = drawRect.width();
        uvcwidth = cwidth / 2;

        if (startline < 0) startline = 0;
        if (endline >= height) endline = height - 1;
        if (startcol < 0) endcol = 0;
        if (endcol >= width) endcol = width - 1;

        for (int line = startline; line <= endline; line++)
        {
            yoffset = line * width;

            memset(y + yoffset + startcol, 0, cwidth);
            memset(alpha + yoffset + startcol, 0, cwidth);
//            if ((line & 2) == 0)
            {
                memset(u + yoffset / 4 + startcol / 2, 127, uvcwidth);
                memset(v + yoffset / 4 + startcol / 2, 127, uvcwidth);
            }
        }
    }

    usedRegions = QRegion();
}

bool OSDSurface::IntersectsDrawn(QRect &newrect)
{
    QMutexLocker lock(&usedRegionsLock);
    QMemArray<QRect> rects = usedRegions.rects();
    QMemArray<QRect>::Iterator it = rects.begin();
    for (; it != rects.end(); ++it)
        if (newrect.intersects(*it))
            return true;
    return false;
}

void OSDSurface::AddRect(QRect &newrect)
{
    QMutexLocker lock(&usedRegionsLock);
    usedRegions = usedRegions.unite(newrect);
}

///////////////////////////////////////////////////////////////////////////
// Helper functions
///////////////////////////////////////////////////////////////////////////

static inline void blendalpha8_c(unsigned char *src, unsigned char *dest,
                                 unsigned char *alpha, bool uvplane)
{
    int mult = uvplane ? 2 : 1;

    for (int i = 0; i < 8; i++)
         dest[i] = blendColorsAlpha(src[i], dest[i], alpha[i * mult]);
}

#ifdef MMX
// cribbed from Jason Tackaberry's vf_bmovl2 source
static inline void blendalpha8_mmx(unsigned char *src, unsigned char *dest,
                                   unsigned char *alpha, bool uvplane)
{
    static mmx_t mmx_80w = {0x0080008000800080LL};
    static mmx_t mmx_fs  = {0xffffffffffffffffLL};
    static mmx_t mmx_ff00 = {0x00ff00ff00ff00ffLL};

    movq_m2r(*dest, mm0);
    movq_r2r(mm0, mm1);
    movq_m2r(mmx_fs, mm2);
    movq_m2r(mmx_80w, mm4);
    pxor_r2r(mm7, mm7);

    if (uvplane)
    {
        movq_m2r(*alpha, mm5);
        movq_m2r(*(alpha+8), mm6);

        pand_m2r(mmx_ff00, mm5);
        pand_m2r(mmx_ff00, mm6);

        packuswb_r2r(mm6, mm5);
    }
    else
        movq_m2r(*alpha, mm5);

    psubw_r2r(mm5, mm2);
    punpcklbw_r2r(mm7, mm0);
    punpckhbw_r2r(mm7, mm1);

    movq_r2r(mm2, mm3);
    punpcklbw_r2r(mm7, mm2);
    punpckhbw_r2r(mm7, mm3);

    pmullw_r2r(mm2, mm0);
    pmullw_r2r(mm3, mm1);

    paddw_r2r(mm4, mm0);
    paddw_r2r(mm4, mm1);

    movq_r2r(mm0, mm2);
    movq_r2r(mm1, mm3);

    psrlw_i2r(8, mm0);
    psrlw_i2r(8, mm1);

    paddw_r2r(mm2, mm0);
    paddw_r2r(mm3, mm1);

    psrlw_i2r(8, mm0);
    psrlw_i2r(8, mm1);

    movq_m2r(*src, mm2);
    movq_r2r(mm2, mm3);

    punpcklbw_r2r(mm7, mm2);
    punpckhbw_r2r(mm7, mm3);

    movq_r2r(mm5, mm6);

    punpcklbw_r2r(mm7, mm5);
    punpckhbw_r2r(mm7, mm6);

    pmullw_r2r(mm5, mm2);
    pmullw_r2r(mm6, mm3);

    paddw_r2r(mm4, mm2);
    paddw_r2r(mm4, mm3);

    movq_r2r(mm2, mm4);
    movq_r2r(mm3, mm5);

    psrlw_i2r(8, mm2);
    psrlw_i2r(8, mm3);

    paddw_r2r(mm4, mm2);
    paddw_r2r(mm5, mm3);

    psrlw_i2r(8, mm2);
    psrlw_i2r(8, mm3);

    paddw_r2r(mm2, mm0);
    paddw_r2r(mm3, mm1);

    packuswb_r2r(mm1, mm0);

    movq_r2m(mm0, *dest);

    emms();
}
#endif

blendtoyv12_8_fun blendtoyv12_8_init(const OSDSurface *surface)
{
    (void)surface;
#ifdef MMX
    if (surface->usemmx)
        return blendalpha8_mmx;
#endif
    return blendalpha8_c;
}

static inline void blendtoargb_8_c(const OSDSurface *surf, unsigned char *src,
                                   unsigned char *usrc, unsigned char *vsrc,
                                   unsigned char *alpha, unsigned char *dest)
{
    int r, g, b, y0;
    int cb, cr, r_add, g_add, b_add;
    const unsigned char *cm = surf->cm;

    int i, j;

    for (i = 0, j = 0; i < 8; i++)
    {
        YUV_TO_RGB1(usrc[j], vsrc[j]);
        YUV_TO_RGB2(r, g, b, src[i]);
        RGBA_OUT(&(dest[i * 4]), r, g, b, alpha[i]);
        if (i % 2 == 0)
            j++;
    }
}

#ifdef MMX
#define movntq(src, dest) movq_r2m(src, dest);
static inline void blendtoargb_8_mmx(const OSDSurface * /*surf*/, unsigned char *src,
                                     unsigned char *usrc, unsigned char *vsrc,
                                     unsigned char *alpha, unsigned char *dest)
{
    static mmx_t mmx_80w = {0x0080008000800080LL};
    static mmx_t mmx_U_green = {0xf37df37df37df37dLL};
    static mmx_t mmx_U_blue = {0x4093409340934093LL};
    static mmx_t mmx_V_red = {0x3312331233123312LL};
    static mmx_t mmx_V_green = {0xe5fce5fce5fce5fcLL};
    static mmx_t mmx_10w = {0x1010101010101010LL};
    static mmx_t mmx_00ffw = {0x00ff00ff00ff00ffLL};
    static mmx_t mmx_Y_coeff = {0x253f253f253f253fLL};

    movd_m2r(*usrc, mm0);
    movd_m2r(*vsrc, mm1);
    movq_m2r(*src, mm6);
    pxor_r2r(mm4, mm4);

    punpcklbw_r2r(mm4, mm0);           // mm0 = u3 u2 u1 u0
    punpcklbw_r2r(mm4, mm1);           // mm1 = v3 v2 v1 v0
    psubsw_m2r(mmx_80w, mm0);          // u -= 128
    psubsw_m2r(mmx_80w, mm1);          // v -= 128
    psllw_i2r(3, mm0);                 // promote precision
    psllw_i2r(3, mm1);                 // promote precision
    movq_r2r(mm0, mm2);                // mm2 = u3 u2 u1 u0
    movq_r2r(mm1, mm3);                // mm3 = v3 v2 v1 v0
    pmulhw_m2r(mmx_U_green, mm2);      // mm2 = u * u_green
    pmulhw_m2r(mmx_V_green, mm3);      // mm3 = v * v_green
    pmulhw_m2r(mmx_U_blue, mm0);       // mm0 = chroma_b
    pmulhw_m2r(mmx_V_red, mm1);        // mm1 = chroma_r
    paddsw_r2r(mm3, mm2);              // mm2 = chroma_g

    psubusb_m2r(mmx_10w, mm6);         // Y -= 16
    movq_r2r(mm6, mm7);                // mm7 = Y7 Y6 Y5 Y4 Y3 Y2 Y1 Y0
    pand_m2r(mmx_00ffw, mm6);          // mm6 =    Y6    Y4    Y2    Y0
    psrlw_i2r(8, mm7);                 // mm7 =    Y7    Y5    Y3    Y1
    psllw_i2r(3, mm6);                 // promote precision
    psllw_i2r(3, mm7);                 // promote precision
    pmulhw_m2r(mmx_Y_coeff, mm6);      // mm6 = luma_rgb even
    pmulhw_m2r(mmx_Y_coeff, mm7);      // mm7 = luma_rgb odd

    /*
     * Do the addition part of the conversion for even and odd pixels
     * register usage:
     * mm0 -> Cblue, mm1 -> Cred, mm2 -> Cgreen even pixels
     * mm3 -> Cblue, mm4 -> Cred, mm5 -> Cgreen odd  pixels
     * mm6 -> Y even, mm7 -> Y odd
     */

    movq_r2r(mm0, mm3);                // mm3 = chroma_b
    movq_r2r(mm1, mm4);                // mm4 = chroma_r
    movq_r2r(mm2, mm5);                // mm5 = chroma_g
    paddsw_r2r(mm6, mm0);              // mm0 = B6 B4 B2 B0
    paddsw_r2r(mm7, mm3);              // mm3 = B7 B5 B3 B1
    paddsw_r2r(mm6, mm1);              // mm1 = R6 R4 R2 R0
    paddsw_r2r(mm7, mm4);              // mm4 = R7 R5 R3 R1
    paddsw_r2r(mm6, mm2);              // mm2 = G6 G4 G2 G0
    paddsw_r2r(mm7, mm5);              // mm5 = G7 G5 G3 G1
    packuswb_r2r(mm0, mm0);            // saturate to 0-255
    packuswb_r2r(mm1, mm1);            // saturate to 0-255
    packuswb_r2r(mm2, mm2);            // saturate to 0-255
    packuswb_r2r(mm3, mm3);            // saturate to 0-255
    packuswb_r2r(mm4, mm4);            // saturate to 0-255
    packuswb_r2r(mm5, mm5);            // saturate to 0-255
    punpcklbw_r2r(mm3, mm0);           // mm0 = B7 B6 B5 B4 B3 B2 B1 B0
    punpcklbw_r2r(mm4, mm1);           // mm1 = R7 R6 R5 R4 R3 R2 R1 R0
    punpcklbw_r2r(mm5, mm2);           // mm2 = G7 G6 G5 G4 G3 G2 G1 G0

    movq_m2r(*alpha, mm3);

    movq_r2r(mm0, mm6);
    movq_r2r(mm1, mm7);
    movq_r2r(mm0, mm4);
    movq_r2r(mm1, mm5);
    punpcklbw_r2r(mm2, mm6);
    punpcklbw_r2r(mm3, mm7);
    punpcklwd_r2r(mm7, mm6);
    movntq(mm6, *dest);
    movq_r2r(mm0, mm6);
    punpcklbw_r2r(mm2, mm6);
    punpckhwd_r2r(mm7, mm6);
    movntq(mm6, *(dest+8));
    punpckhbw_r2r(mm2, mm4);
    punpckhbw_r2r(mm3, mm5);
    punpcklwd_r2r(mm5, mm4);
    movntq(mm4, *(dest+16));
    movq_r2r(mm0, mm4);
    punpckhbw_r2r(mm2, mm4);
    punpckhwd_r2r(mm5, mm4);
    movntq(mm4, *(dest+24));

    emms();
}
#endif

blendtoargb_8_fun blendtoargb_8_init(const OSDSurface *surface)
{
    (void)surface;
#ifdef MMX
    if (surface->usemmx)
        return blendtoargb_8_mmx;
#endif
    return blendtoargb_8_c;
}

struct dither8_context
{
    int ashift;
    int amask;
    int ishift;
    int imask;
#ifdef MMX
    mmx_t amask_mmx;
    mmx_t imask_mmx;
#endif
};

static inline void dithertoia44_8_c(unsigned char *src, unsigned char *dest,
                                    unsigned char *alpha,
                                    const unsigned char *dmp, int xpos,
                                    dither8_context *context)
{
    int grey;
    for (int i = 0; i < 8; i++)
    {
        grey = src[i] + ((dmp[((xpos + i) & (DM_WIDTH - 1))] << 2) >> 4);
        grey = (grey - (grey >> 4)) >> 4;

        dest[i] = (((alpha[i] >> 4) << context->ashift) & context->amask) |
                  (((grey) << context->ishift) & context->imask);
    }
}

#ifdef MMX
static inline void dithertoia44_8_mmx(unsigned char *src, unsigned char *dest,
                                      unsigned char *alpha,
                                      const unsigned char *dmp, int xpos,
                                      dither8_context *context)
{
    unsigned char tmp2[8];

    dithertoia44_8_c(src, tmp2, alpha, dmp, xpos, context);
  
    unsigned char tmp[8];

    tmp[0] = dmp[((xpos+0) & (DM_WIDTH - 1))];
    tmp[1] = dmp[((xpos+1) & (DM_WIDTH - 1))];
    tmp[2] = dmp[((xpos+2) & (DM_WIDTH - 1))];
    tmp[3] = dmp[((xpos+3) & (DM_WIDTH - 1))];
    tmp[4] = dmp[((xpos+4) & (DM_WIDTH - 1))];
    tmp[5] = dmp[((xpos+5) & (DM_WIDTH - 1))];
    tmp[6] = dmp[((xpos+6) & (DM_WIDTH - 1))];
    tmp[7] = dmp[((xpos+7) & (DM_WIDTH - 1))];

    movq_m2r(*src, mm0);
    movq_r2r(mm0, mm1);
    movq_m2r(*tmp, mm2);
    movq_r2r(mm2, mm3);
    movq_m2r(*alpha, mm4);
    movq_m2r(context->amask_mmx, mm5);
    movq_m2r(context->imask_mmx, mm6);

    pxor_r2r(mm7, mm7);

    punpcklbw_r2r(mm7, mm0);
    punpckhbw_r2r(mm7, mm1);

    punpcklbw_r2r(mm7, mm2);
    punpckhbw_r2r(mm7, mm3);

    psllw_i2r(2, mm2);
    psllw_i2r(2, mm3);

    psrlw_i2r(4, mm2);
    psrlw_i2r(4, mm3);

    paddw_r2r(mm0, mm2);
    paddw_r2r(mm1, mm3);

    movq_r2r(mm2, mm0);
    movq_r2r(mm3, mm1);

    psrlw_i2r(4, mm0);
    psrlw_i2r(4, mm1);

    psubw_r2r(mm0, mm2);
    psubw_r2r(mm1, mm3);

    psrlw_i2r(4, mm2);
    psrlw_i2r(4, mm3);

    packuswb_r2r(mm3, mm2);

    psllw_i2r(4, mm2);

    pand_r2r(mm6, mm2);

    psrlw_i2r(4, mm4);

    psllw_i2r(0, mm4);

    pand_r2r(mm5, mm4);

    por_r2r(mm4, mm2);

    movq_r2m(mm2, *dest);

    emms();
}
#endif

dithertoia44_8_fun dithertoia44_8_init(const OSDSurface* /*surface*/)
{
#ifdef MMX
// mmx version seems to be about the same speed, no reason to use it.
//    if (surface->usemmx)
//        return dithertoia44_8_mmx;
#endif
    return dithertoia44_8_c;
}

#ifdef MMX
static mmx_t mask_0f = {0x0f0f0f0f0f0f0f0fLL};
static mmx_t mask_f0 = {0xf0f0f0f0f0f0f0f0LL};
#endif

dither8_context *init_dithertoia44_8_context(bool first)
{
    dither8_context *context = new dither8_context;
    if (first)
    {
        context->ashift = 0;
        context->amask = 0x0f;
        context->ishift = 4;
        context->imask = 0xf0;
#ifdef MMX
        context->amask_mmx = mask_0f;
        context->imask_mmx = mask_f0;
#endif
    }
    else
    {
        context->ashift = 4;
        context->amask = 0xf0;
        context->ishift = 0;
        context->imask = 0x0f;
#ifdef MMX
        context->amask_mmx = mask_f0;
        context->imask_mmx = mask_0f;
#endif
    }

    return context;
}

void delete_dithertoia44_8_context(dither8_context *context)
{
    delete context;
}

/** \fn OSDSurface::BlendToYV12(unsigned char *) const
 *  \brief Alpha blends OSDSurface to yuv buffer of the same size.
 *  \param yuvptr Pointer to YUV buffer to blend OSD to.
 */
void OSDSurface::BlendToYV12(unsigned char *yuvptr) const
{
    QMutexLocker lock(&usedRegionsLock);
    const OSDSurface *surface = this;
    blendtoyv12_8_fun blender = blendtoyv12_8_init(surface);

    unsigned char *uptrdest = yuvptr + surface->width * surface->height;
    unsigned char *vptrdest = uptrdest + surface->width * surface->height / 4;

    QMemArray<QRect> rects = surface->usedRegions.rects();
    QMemArray<QRect>::Iterator it = rects.begin();
    for (; it != rects.end(); ++it)
    {
        QRect drawRect = *it;

        int startcol, startline, endcol, endline;
        startcol = drawRect.left();
        startline = drawRect.top();
        endcol = drawRect.right();
        endline = drawRect.bottom();

        if (startline < 0) startline = 0;
        if (endline >= height) endline = height - 1;
        if (startcol < 0) endcol = 0;
        if (endcol >= width) endcol = width - 1;

        unsigned char *src, *usrc, *vsrc;
        unsigned char *dest, *udest, *vdest;
        unsigned char *alpha;

        int yoffset;

        for (int y = startline; y <= endline; y++)
        {
            yoffset = y * surface->width;

            src = surface->y + yoffset + startcol;
            dest = yuvptr + yoffset + startcol;
            alpha = surface->alpha + yoffset + startcol;

            for (int x = startcol; x <= endcol; x++)
            {
                if (x + 8 >= endcol)
                {
                    if (*alpha != 0)
                        *dest = blendColorsAlpha(*src, *dest, *alpha);
                    src++;
                    dest++;
                    alpha++;
                }
                else
                {
                    blender(src, dest, alpha, false);
                    src += 8;
                    dest += 8;
                    alpha += 8;
                    x += 7;
                }
            }

            alpha = surface->alpha + yoffset + startcol;

            if (y % 2 == 0)
            {
                usrc = surface->u + yoffset / 4 + startcol / 2;
                udest = uptrdest + yoffset / 4 + startcol / 2;

                vsrc = surface->v + yoffset / 4 + startcol / 2;
                vdest = vptrdest + yoffset / 4 + startcol / 2;

                for (int x = startcol; x <= endcol; x += 2)
                {
                    alpha = surface->alpha + yoffset + x;

                    if (x + 16 >= endcol)
                    {
                        if (*alpha != 0)
                        {
                            *udest = blendColorsAlpha(*usrc, *udest, *alpha);
                            *vdest = blendColorsAlpha(*vsrc, *vdest, *alpha);
                        }

                        usrc++;
                        udest++;
                        vsrc++;
                        vdest++;
                    }
                    else
                    {
                        blender(usrc, udest, alpha, true);
                        blender(vsrc, vdest, alpha, true);
                        usrc += 8;
                        udest += 8;
                        vsrc += 8;
                        vdest += 8;
                        x += 14;
                    }
                }
            }
        }
    }
}

static void BlendToBlack(unsigned char *argbptr, uint width, uint outheight)
{
    unsigned int i, argb, a, r, g, b;
    for (i = 0; i < width * outheight; i++)
    {
        argb = ((uint*)argbptr)[i];
        a = (argb >> 24);
        if (argb && a < 250)
        {
            r = (argb >> 16) & 0xff;
            g = (argb >> 8) & 0xff;
            b = (argb) & 0xff;

            RGBA_OUT(&argbptr[i * 4], (a * r) >> 8, (a * g) >> 8, 
                     (a * b) >> 8, 128);
        }
    }
}

/** \fn OSDSurface::BlendToARGB(unsigned char *,uint,uint,bool,uint) const
 *  \brief Alpha blends OSDSurface to ARGB buffer.
 *
 *  \todo Currently blend_to_black is implemented as a post process
 *        on the whole frame, it would make sense to make this more
 *        efficient by integrating it into the process.
 *
 *  \param stride         Length of each line in output buffer in bytes
 *  \param outheight      Number of lines in output buffer
 *  \param blend_to_black Uses Alpha to blend buffer to black
 *  \param threshold      Alpha threshold above which to perform blending
 */
void OSDSurface::BlendToARGB(unsigned char *argbptr, uint stride, 
                             uint outheight, bool blend_to_black,
                             uint threshold) const
{
    QMutexLocker lock(&usedRegionsLock);
    const OSDSurface *surface = this;
    blendtoargb_8_fun blender = blendtoargb_8_init(surface);
    const unsigned char *cm = surface->cm;

    if (blend_to_black)
        bzero(argbptr, stride * outheight);

    QMemArray<QRect> rects = surface->usedRegions.rects();
    QMemArray<QRect>::Iterator it = rects.begin();
    for (; it != rects.end(); ++it)
    {
        QRect drawRect = *it;

        int startcol  = std::max(drawRect.left(),   0);
        int startline = std::max(drawRect.top(),    0);
        int endcol    = std::min(drawRect.right(),  width  - 1);
        int endline   = std::min(drawRect.bottom(), height - 1);

        unsigned char *src, *usrcbase, *vsrcbase, *usrc, *vsrc;
        unsigned char *dest;
        unsigned char *alpha;

        int yoffset;
        int destyoffset;

        int cb, cr, r_add, g_add, b_add;
        int r, g, b, y0;

        for (int y = startline; y <= endline; y++)
        {
            yoffset = y * surface->width;
            destyoffset = y * stride;

            src = surface->y + yoffset + startcol;
            dest = argbptr + destyoffset + startcol * 4;
            alpha = surface->alpha + yoffset + startcol;

            usrcbase = surface->u + (y / 2) * (surface->width / 2);
            vsrcbase = surface->v + (y / 2) * (surface->width / 2);

            for (int x = startcol; x <= endcol; x++)
            {
                usrc = usrcbase + x / 2;
                vsrc = vsrcbase + x / 2;

                if (((x + 8) >= endcol) || threshold)
                {
                    if (*alpha > threshold)
                    {
                        YUV_TO_RGB1(*usrc, *vsrc);
                        YUV_TO_RGB2(r, g, b, *src);
                        RGBA_OUT(dest, r, g, b, *alpha);
                    }
                    src++;
                    alpha++;
                    dest += 4;
                }
                else
                {
                    blender(surface, src, usrc, vsrc, alpha, dest);
                    src += 8;
                    dest += 32;
                    alpha += 8;
                    x += 7;
                } 
            }
        }
    }
    if (blend_to_black)
        BlendToBlack(argbptr, stride>>2, outheight);
}

/** \fn OSDSurface::DitherToI44(unsigned char*,bool,uint,uint) const
 *  \brief Copies and converts OSDSurface to either a greyscale
 *         IA44 or AI44 buffer.
 *  \sa DitherToIA44(unsigned char*,uint,uint) const,
 *      DitherToAI44(unsigned char*,uint,uint) const.
 *
 *  \param outbuf Output buffer
 *  \param ifirst If true output buffer is AI44, otherwise it is IA44
 *  \param stride Length of each line in output buffer in bytes
 *  \param outheight Number of lines in output buffer
 */
void OSDSurface::DitherToI44(unsigned char *outbuf, bool ifirst,
                             uint stride, uint outheight) const
{
    QMutexLocker lock(&usedRegionsLock);
    const OSDSurface *surface = this;
    int ashift = ifirst ? 0 : 4;
    int amask = ifirst ? 0x0f : 0xf0;

    int ishift = ifirst ? 4 : 0;
    int imask = ifirst ? 0xf0 : 0x0f; 

    dithertoia44_8_fun ditherer = dithertoia44_8_init(surface);
    dither8_context *dcontext = init_dithertoia44_8_context(ifirst);

    bzero(outbuf, stride * outheight);

    QMemArray<QRect> rects = surface->usedRegions.rects();
    QMemArray<QRect>::Iterator it = rects.begin();
    for (; it != rects.end(); ++it)
    {
        QRect drawRect = *it;

        int startcol, startline, endcol, endline;
        startcol = drawRect.left();
        startline = drawRect.top();
        endcol = drawRect.right();
        endline = drawRect.bottom();

        if (startline < 0) startline = 0;
        if (endline >= height) endline = height - 1;
        if (startcol < 0) endcol = 0;
        if (endcol >= width) endcol = width - 1;

        unsigned char *src;
        unsigned char *dest;
        unsigned char *alpha;

        const unsigned char *dmp;

        int yoffset;
        int destyoffset;

        int grey;

        for (int y = startline; y <= endline; y++)
        {
            yoffset = y * surface->width;
            destyoffset = y * stride;

            src = surface->y + yoffset + startcol;
            dest = outbuf + destyoffset + startcol;
            alpha = surface->alpha + yoffset + startcol;

            dmp = DM[(y) & (DM_HEIGHT - 1)];

            for (int x = startcol; x <= endcol; x++)
            {
                if (x + 8 >= endcol)
                {
                    if (*alpha != 0)
                    {
                        grey = *src + ((dmp[(x & (DM_WIDTH - 1))] << 2) >> 4);
                        grey = (grey - (grey >> 4)) >> 4;

                        *dest = (((*alpha >> 4) << ashift) & amask) |
                                (((grey) << ishift) & imask);
                    }
                    else
                        *dest = 0;

                    src++;
                    dest++;
                    alpha++;
                }
                else
                {
                    ditherer(src, dest, alpha, dmp, x, dcontext);
                    src += 8;
                    dest += 8;
                    alpha += 8;
                    x += 7;
                }
            }
        }
    }

    delete_dithertoia44_8_context(dcontext);
}

/** \fn OSDSurface::DitherToIA44(unsigned char*,uint,uint) const
 *  \brief Copies and converts OSDSurface to a greyscale IA44 buffer.
 *
 *  \param outbuf Output buffer
 *  \param stride Length of each line in output buffer in bytes
 *  \param outheight Number of lines in output buffer
 *  \sa DitherToI44(unsigned char*,bool,uint,uint) const,
 *      DitherToAI44(unsigned char*,uint,uint) const.
 */
void OSDSurface::DitherToIA44(unsigned char* outbuf,
                              uint stride, uint outheight) const
{
    DitherToI44(outbuf, false, stride, outheight);
}

/** \fn OSDSurface::DitherToAI44(unsigned char*,uint,uint) const
 *  \brief Copies and converts OSDSurface to a greyscale AI44 buffer.
 *
 *  \param outbuf Output buffer
 *  \param stride Length of each line in output buffer in bytes
 *  \param outheight Number of lines in output buffer
 *  \sa DitherToI44(unsigned char*,bool,uint,uint) const,
 *      DitherToIA44(unsigned char*,uint,uint) const.
 */
void OSDSurface::DitherToAI44(unsigned char* outbuf,
                              uint stride, uint outheight) const
{
    DitherToI44(outbuf, true, stride, outheight);
}
