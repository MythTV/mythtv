#include "osdsurface.h"
#include "dithertable.h"

#include <iostream>
using namespace std;

#ifdef MMX

#include "config.h"
#include "dsputil.h"
#include "i386/mmx.h"

#ifdef ARCH_X86_64
#  define REG_b "rbx"
#  define REG_S "rsi"
#else
#  define REG_b "ebx"
#  define REG_S "esi"
#endif

/* ebx saving is necessary for PIC. gcc seems unable to see it alone */
#define cpuid(index,eax,ebx,ecx,edx)\
    __asm __volatile\
        ("mov %%"REG_b", %%"REG_S"\n\t"\
         "cpuid\n\t"\
         "xchg %%"REG_b", %%"REG_S\
         : "=a" (eax), "=S" (ebx),\
           "=c" (ecx), "=d" (edx)\
         : "0" (index));

#if 0
/** Function to test if multimedia instructions are supported...  */
int mm_support(void)
{
    int rval = 0, max_ext_level=0, ext_caps=0;
    long a, c; // long's needed for x86_64 compatibility
    int eax, ebx, ecx, edx;

    __asm__ __volatile__ (
                          /* See if CPUID instruction is supported ... */
                          /* ... Get copies of EFLAGS into eax and ecx */
                          "pushf\n\t"
                          "pop %0\n\t"
                          "mov %0, %1\n\t"

                          /* ... Toggle the ID bit in one copy and store */
			  /*     to the EFLAGS reg */
                          "xor $0x200000, %0\n\t"
                          "push %0\n\t"
                          "popf\n\t"

                          /* ... Get the (hopefully modified) EFLAGS */
                          "pushf\n\t"
                          "pop %0\n\t"
                          : "=a" (a), "=c" (c)
                          :
                          : "cc"
                          );

    if (a == c)
      return 0; /* CPUID not supported */

    /* x86-64, C3, Tranmeta Crusoe test */
    cpuid(0x80000000, max_ext_level, ebx, ecx, edx);
    if ((uint)max_ext_level >= 0x80000001)
    {
        cpuid(0x80000001, eax, ebx, ecx, ext_caps);
        if (ext_caps & (1<<31))
            rval |= MM_3DNOW;
        if (ext_caps & (1<<30))
            rval |= MM_3DNOWEXT;
        if (ext_caps & (1<<23))
            rval |= MM_MMX;
    }

    cpuid(0, eax, ebx, ecx, edx);

    if (ebx == 0x756e6547 &&
        edx == 0x49656e69 &&
        ecx == 0x6c65746e) {

      /* intel */
        cpuid(1, eax, ebx, ecx, edx);
        if ((edx & 0x00800000) == 0)
            return 0;
        rval = MM_MMX;
        if (edx & 0x02000000)
            rval |= MM_MMXEXT | MM_SSE;
        if (edx & 0x04000000)
            rval |= MM_SSE2;
        return rval;
    } else if (ebx == 0x68747541 &&
               edx == 0x69746e65 &&
               ecx == 0x444d4163) {
      /* AMD */
      if (ext_caps & (1<<22))
            rval |= MM_MMXEXT;
    } else if (ebx == 0x746e6543 &&
               edx == 0x48727561 &&
               ecx == 0x736c7561) {  /*  "CentaurHauls" */
      /* VIA C3 */
      if (ext_caps & (1<<24))
            rval |= MM_MMXEXT;
    } else if (ebx == 0x69727943 &&
               edx == 0x736e4978 &&
               ecx == 0x64616574) {
      /* Cyrix Section */
      /* See if extended CPUID level 80000001 is supported */
      /* The value of CPUID/80000001 for the 6x86MX is undefined
           according to the Cyrix CPU Detection Guide (Preliminary
           Rev. 1.01 table 1), so we'll check the value of eax for
           CPUID/0 to see if standard CPUID level 2 is supported.
           According to the table, the only CPU which supports level
           2 is also the only one which supports extended CPUID levels.
      */
        if (eax < 2)
            return rval;
        if (ext_caps & (1<<24))
            rval |= MM_MMXEXT;
    }
    return rval;
}
#endif

QString cpuid2str(int mm_flags)
{
    QString str;
    if (mm_flags & MM_MMX)
        str+="MMX, ";
    if (mm_flags & MM_3DNOW)
        str+="3DNow, ";
    if (mm_flags & MM_MMXEXT)
        str+="MMXEXT, ";
    if (mm_flags & MM_SSE)
        str+="SSE, ";
    if (mm_flags & MM_SSE2)
        str+"SSE2, ";
    if (mm_flags & MM_3DNOWEXT)
        str+"3DNowEXT, ";
    if (str.length()>=2)
        str = str.left(str.length()-2);
    return str;
}
#endif

OSDSurface::OSDSurface(int w, int h)
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
    memset(y, 0, size);
    memset(u, 127, size / 4);
    memset(v, 127, size / 4);
    memset(alpha, 0, size);
    usedRegions = QRegion();
}

void OSDSurface::ClearUsed(void)
{
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
    QMemArray<QRect> rects = usedRegions.rects();
    QMemArray<QRect>::Iterator it = rects.begin();
    for (; it != rects.end(); ++it)
        if (newrect.intersects(*it))
            return true;
    return false;
}

void OSDSurface::AddRect(QRect &newrect)
{
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

blendtoyv12_8_fun blendtoyv12_8_init(OSDSurface *surface)
{
    (void)surface;
#ifdef MMX
    if (surface->usemmx)
        return blendalpha8_mmx;
#endif
    return blendalpha8_c;
}

static inline void blendtoargb_8_c(OSDSurface *surf, unsigned char *src,
                                   unsigned char *usrc, unsigned char *vsrc,
                                   unsigned char *alpha, unsigned char *dest)
{
    int r, g, b, y0;
    int cb, cr, r_add, g_add, b_add;
    unsigned char *cm = surf->cm;

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
static inline void blendtoargb_8_mmx(OSDSurface * /*surf*/, unsigned char *src,
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

blendtoargb_8_fun blendtoargb_8_init(OSDSurface *surface)
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

dithertoia44_8_fun dithertoia44_8_init(OSDSurface* /*surface*/)
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


