// -*- Mode: c++ -*-

#include <stdint.h>
#include <QSize>
#include "compat.h"
#include "util-opengl.h"

#ifdef MMX
extern "C" {
#include "ffmpeg-mmx.h"
}

static mmx_t mmx_1s = {0xffffffffffffffffULL};

static inline void mmx_pack_alpha1s_high(uint8_t *y1, uint8_t *y2)
{
    movq_m2r (mmx_1s, mm4);
    punpckhbw_m2r (*y1, mm4);
    movq_m2r (mmx_1s, mm7);
    punpckhbw_m2r (*y2, mm7);
}

static inline void mmx_pack_alpha1s_low(uint8_t *y1, uint8_t *y2)
{
    movq_m2r (mmx_1s, mm4);
    punpcklbw_m2r (*y1, mm4);
    movq_m2r (mmx_1s, mm7);
    punpcklbw_m2r (*y2, mm7);
}

static inline void mmx_pack_middle(uint8_t *dest1, uint8_t *dest2)
{
    movq_r2r (mm3, mm5);
    punpcklbw_r2r (mm2, mm5);

    movq_r2r (mm5, mm6);
    punpcklbw_r2r (mm4, mm6);
    movq_r2m (mm6, *(dest1));

    movq_r2r (mm5, mm6);
    punpckhbw_r2r (mm4, mm6);
    movq_r2m (mm6, *(dest1 + 8));

    movq_r2r (mm5, mm6);
    punpcklbw_r2r (mm7, mm6);
    movq_r2m (mm6, *(dest2));

    movq_r2r (mm5, mm6);
    punpckhbw_r2r (mm7, mm6);
    movq_r2m (mm6, *(dest2 + 8));
}

static inline void mmx_pack_end(uint8_t *dest1, uint8_t *dest2)
{
    punpckhbw_r2r (mm2, mm3);

    movq_r2r (mm3, mm6);
    punpcklbw_r2r (mm4, mm6);
    movq_r2m (mm6, *(dest1 + 16));

    movq_r2r (mm3, mm6);
    punpckhbw_r2r (mm4, mm6);
    movq_r2m (mm6, *(dest1 + 24));

    movq_r2r (mm3, mm6);
    punpcklbw_r2r (mm7, mm6);
    movq_r2m (mm6, *(dest2 + 16));

    punpckhbw_r2r (mm7, mm3);
    movq_r2m (mm3, *(dest2 + 24));
}

static inline void mmx_pack_easy(uint8_t *dest, uint8_t *y)
{
    movq_m2r (mmx_1s, mm4);
    punpcklbw_m2r (*y, mm4);

    movq_r2r (mm3, mm5);
    punpcklbw_r2r (mm2, mm5);

    movq_r2r (mm5, mm6);
    punpcklbw_r2r (mm4, mm6);
    movq_r2m (mm6, *(dest));

    movq_r2r (mm5, mm6);
    punpckhbw_r2r (mm4, mm6);
    movq_r2m (mm6, *(dest + 8));

    movq_m2r (mmx_1s, mm4);
    punpckhbw_m2r (*y, mm4);

    punpckhbw_r2r (mm2, mm3);

    movq_r2r (mm3, mm6);
    punpcklbw_r2r (mm4, mm6);
    movq_r2m (mm6, *(dest + 16));

    punpckhbw_r2r (mm4, mm3);
    movq_r2m (mm3, *(dest + 24));
}

static mmx_t mmx_0s = {0x0000000000000000LL};
static mmx_t mmx_round  = {0x0002000200020002LL};

static inline void mmx_interp_start(uint8_t *left, uint8_t *right)
{
    movd_m2r  (*left, mm5);
    punpcklbw_m2r (mmx_0s, mm5);

    movq_r2r  (mm5, mm4);
    paddw_r2r (mm4, mm4);
    paddw_r2r (mm5, mm4);
    paddw_m2r (mmx_round, mm4);

    movd_m2r  (*right, mm5);
    punpcklbw_m2r (mmx_0s, mm5);
    paddw_r2r (mm5, mm4);

    psrlw_i2r (2, mm4);
}

static inline void mmx_interp_endu(void)
{
    movq_r2r  (mm4, mm2);
    psllw_i2r (8, mm2);
    paddb_r2r (mm4, mm2);
}

static inline void mmx_interp_endv(void)
{
    movq_r2r  (mm4, mm3);
    psllw_i2r (8, mm3);
    paddb_r2r (mm4, mm3);
}

static inline void mmx_pack_chroma(uint8_t *u, uint8_t *v)
{
    movd_m2r (*u,  mm2);
    movd_m2r (*v,  mm3);
    punpcklbw_r2r (mm2, mm2);
    punpcklbw_r2r (mm3, mm3);
}
#endif // MMX

static inline void c_interp(uint8_t *dest, uint8_t *a, uint8_t *b,
                            uint8_t *c, uint8_t *d)
{
    unsigned int tmp = (unsigned int) *a;
    tmp *= 3;
    tmp += 2;
    tmp += (unsigned int) *c;
    dest[0] = (uint8_t) (tmp >> 2);

    tmp = (unsigned int) *b;
    tmp *= 3;
    tmp += 2;
    tmp += (unsigned int) *d;
    dest[1] = (uint8_t) (tmp >> 2);

    tmp = (unsigned int) *c;
    tmp *= 3;
    tmp += 2;
    tmp += (unsigned int) *a;
    dest[2] = (uint8_t) (tmp >> 2);

    tmp = (unsigned int) *d;
    tmp *= 3;
    tmp += 2;
    tmp += (unsigned int) *b;
    dest[3] = (uint8_t) (tmp >> 2);
}

void pack_yv12progressive(const unsigned char *source,
                          const unsigned char *dest,
                          const int *offsets, const int *pitches,
                          const QSize &size)
{
    const int width = size.width();
    const int height = size.height();

    if (height % 2 || width % 2)
        return;

#ifdef MMX
    int residual  = width % 8;
    int mmx_width = width - residual;
    int c_start_w = mmx_width;
#else
    int residual  = 0;
    int c_start_w = 0;
#endif

    uint bgra_width  = width << 2;
    uint chroma_width = width >> 1;

    uint y_extra     = (pitches[0] << 1) - width + residual;
    uint u_extra     = pitches[1] - chroma_width + (residual >> 1);
    uint v_extra     = pitches[2] - chroma_width + (residual >> 1);
    uint d_extra     = bgra_width + (residual << 2);

    uint8_t *ypt_1   = (uint8_t *)source + offsets[0];
    uint8_t *ypt_2   = ypt_1 + pitches[0];
    uint8_t *upt     = (uint8_t *)source + offsets[1];
    uint8_t *vpt     = (uint8_t *)source + offsets[2];
    uint8_t *dst_1   = (uint8_t *) dest;
    uint8_t *dst_2   = dst_1 + bgra_width;

#ifdef MMX
    for (int row = 0; row < height; row += 2)
    {
        for (int col = 0; col < mmx_width; col += 8)
        {
            mmx_pack_chroma(upt,  vpt);
            mmx_pack_alpha1s_low(ypt_1, ypt_2);
            mmx_pack_middle(dst_1, dst_2);
            mmx_pack_alpha1s_high(ypt_1, ypt_2);
            mmx_pack_end(dst_1, dst_2);

            dst_1 += 32; dst_2 += 32;
            ypt_1 += 8;  ypt_2 += 8;
            upt   += 4;  vpt   += 4;

        }
        ypt_1 += y_extra; ypt_2 += y_extra;
        upt   += u_extra; vpt   += v_extra;
        dst_1 += d_extra; dst_2 += d_extra;
    }

    emms();

    if (residual)
    {
        y_extra     = (pitches[0] << 1) - width + mmx_width;
        u_extra     = pitches[1] - chroma_width + (mmx_width >> 1);
        v_extra     = pitches[2] - chroma_width + (mmx_width >> 1);
        d_extra     = bgra_width + (mmx_width << 2);

        ypt_1   = (uint8_t *)source + offsets[0] + mmx_width;
        ypt_2   = ypt_1 + pitches[0];
        upt     = (uint8_t *)source + offsets[1] + (mmx_width>>1);
        vpt     = (uint8_t *)source + offsets[2] + (mmx_width>>1);
        dst_1   = (uint8_t *) dest + (mmx_width << 2);
        dst_2   = dst_1 + bgra_width;
    }
    else
    {
        return;
    }
#endif //MMX

    for (int row = 0; row < height; row += 2)
    {
        for (int col = c_start_w; col < width; col += 2)
        {
            *(dst_1++) = *vpt; *(dst_2++) = *vpt;
            *(dst_1++) = 255;  *(dst_2++) = 255;
            *(dst_1++) = *upt; *(dst_2++) = *upt;
            *(dst_1++) = *(ypt_1++);
            *(dst_2++) = *(ypt_2++);

            *(dst_1++) = *vpt; *(dst_2++) = *(vpt++);
            *(dst_1++) = 255;  *(dst_2++) = 255;
            *(dst_1++) = *upt; *(dst_2++) = *(upt++);
            *(dst_1++) = *(ypt_1++);
            *(dst_2++) = *(ypt_2++);
        }
        ypt_1   += y_extra; ypt_2   += y_extra;
        upt     += u_extra; vpt     += v_extra;
        dst_1   += d_extra; dst_2   += d_extra;
    }
}

void pack_yv12interlaced(const unsigned char *source,
                         const unsigned char *dest,
                         const int *offsets,
                         const int *pitches,
                         const QSize &size)
{
    int width = size.width();
    int height = size.height();

    if (height % 4 || width % 2)
        return;

    uint bgra_width  = width << 2;
    uint dwrap  = (bgra_width << 2) - bgra_width;
    uint chroma_width = width >> 1;
    uint ywrap     = (pitches[0] << 1) - width;
    uint uwrap     = (pitches[1] << 1) - chroma_width;
    uint vwrap     = (pitches[2] << 1) - chroma_width;

    uint8_t *ypt_1   = (uint8_t *)source + offsets[0];
    uint8_t *ypt_2   = ypt_1 + pitches[0];
    uint8_t *ypt_3   = ypt_1 + (pitches[0] * (height - 2));
    uint8_t *ypt_4   = ypt_3 + pitches[0];

    uint8_t *u1     = (uint8_t *)source + offsets[1];
    uint8_t *v1     = (uint8_t *)source + offsets[2];
    uint8_t *u2     = u1 + pitches[1]; uint8_t *v2     = v1 + pitches[2];
    uint8_t *u3     = u1 + (pitches[1] * ((height - 4) >> 1));
    uint8_t *v3     = v1 + (pitches[2] * ((height - 4) >> 1));
    uint8_t *u4     = u3 + pitches[1]; uint8_t *v4     = v3 + pitches[2];

    uint8_t *dst_1   = (uint8_t *) dest;
    uint8_t *dst_2   = dst_1 + bgra_width;
    uint8_t *dst_3   = dst_1 + (bgra_width * (height - 2));
    uint8_t *dst_4   = dst_3 + bgra_width;

#ifdef MMX

    if (!(width % 8))
    {
        // pack first 2 and last 2 rows
        for (int col = 0; col < width; col += 8)
        {
            mmx_pack_chroma(u1, v1);
            mmx_pack_easy(dst_1, ypt_1);
            mmx_pack_chroma(u2, v2);
            mmx_pack_easy(dst_2, ypt_2);
            mmx_pack_chroma(u3, v3);
            mmx_pack_easy(dst_3, ypt_3);
            mmx_pack_chroma(u4, v4);
            mmx_pack_easy(dst_4, ypt_4);

            dst_1 += 32; dst_2 += 32; dst_3 += 32; dst_4 += 32;
            ypt_1 += 8; ypt_2 += 8; ypt_3 += 8; ypt_4 += 8;
            u1   += 4; v1   += 4; u2   += 4; v2   += 4;
            u3   += 4; v3   += 4; u4   += 4; v4   += 4;
        }

        ypt_1 += ywrap; ypt_2 += ywrap;
        dst_1 += bgra_width; dst_2 += bgra_width;

        ypt_3 = ypt_2 + pitches[0];
        ypt_4 = ypt_3 + pitches[0];
        dst_3 = dst_2 + bgra_width;
        dst_4 = dst_3 + bgra_width;

        ywrap = (pitches[0] << 2) - width;

        u1 = (uint8_t *)source + offsets[1];
        v1 = (uint8_t *)source + offsets[2];
        u2 = u1 + pitches[1]; v2 = v1 + pitches[2];
        u3 = u2 + pitches[1]; v3 = v2 + pitches[2];
        u4 = u3 + pitches[1]; v4 = v3 + pitches[2];

        height -= 4;

        // pack main body
        for (int row = 0 ; row < height; row += 4)
        {
            for (int col = 0; col < width; col += 8)
            {
                mmx_interp_start(u1, u3); mmx_interp_endu();
                mmx_interp_start(v1, v3); mmx_interp_endv();
                mmx_pack_easy(dst_1, ypt_1);

                mmx_interp_start(u2, u4); mmx_interp_endu();
                mmx_interp_start(v2, v4); mmx_interp_endv();
                mmx_pack_easy(dst_2, ypt_2);

                mmx_interp_start(u3, u1); mmx_interp_endu();
                mmx_interp_start(v3, v1); mmx_interp_endv();
                mmx_pack_easy(dst_3, ypt_3);

                mmx_interp_start(u4, u2); mmx_interp_endu();
                mmx_interp_start(v4, v2); mmx_interp_endv();
                mmx_pack_easy(dst_4, ypt_4);

                dst_1 += 32; dst_2 += 32; dst_3 += 32; dst_4 += 32;
                ypt_1 += 8; ypt_2 += 8; ypt_3 += 8; ypt_4 += 8;
                u1   += 4; u2   += 4; u3   += 4; u4   += 4;
                v1   += 4; v2   += 4; v3   += 4; v4   += 4;
            }

            ypt_1 += ywrap; ypt_2 += ywrap; ypt_3 += ywrap; ypt_4 += ywrap;
            dst_1 += dwrap; dst_2 += dwrap; dst_3 += dwrap; dst_4 += dwrap;
            u1 += uwrap; v1 += vwrap; u2 += uwrap; v2 += vwrap;
            u3 += uwrap; v3 += vwrap; u4 += uwrap;v4 += vwrap;
        }

        emms();

        return;
    }
#endif //MMX

    // pack first 2 and last 2 rows
    for (int col = 0; col < width; col += 2)
    {
        *(dst_1++) = *v1; *(dst_2++) = *v2; *(dst_3++) = *v3; *(dst_4++) = *v4;
        *(dst_1++) = 255; *(dst_2++) = 255; *(dst_3++) = 255; *(dst_4++) = 255;
        *(dst_1++) = *u1; *(dst_2++) = *u2; *(dst_3++) = *u3; *(dst_4++) = *u4;
        *(dst_1++) = *(ypt_1++); *(dst_2++) = *(ypt_2++);
        *(dst_3++) = *(ypt_3++); *(dst_4++) = *(ypt_4++);

        *(dst_1++) = *(v1++); *(dst_2++) = *(v2++);
        *(dst_3++) = *(v3++); *(dst_4++) = *(v4++);
        *(dst_1++) = 255; *(dst_2++) = 255; *(dst_3++) = 255; *(dst_4++) = 255;
        *(dst_1++) = *(u1++); *(dst_2++) = *(u2++);
        *(dst_3++) = *(u3++); *(dst_4++) = *(u4++);
        *(dst_1++) = *(ypt_1++); *(dst_2++) = *(ypt_2++);
        *(dst_3++) = *(ypt_3++); *(dst_4++) = *(ypt_4++);
    }

    ypt_1 += ywrap; ypt_2 += ywrap;
    dst_1 += bgra_width; dst_2 += bgra_width;

    ypt_3 = ypt_2 + pitches[0];
    ypt_4 = ypt_3 + pitches[0];
    dst_3 = dst_2 + bgra_width;
    dst_4 = dst_3 + bgra_width;

    ywrap = (pitches[0] << 2) - width;

    u1 = (uint8_t *)source + offsets[1];
    v1 = (uint8_t *)source + offsets[2];
    u2 = u1 + pitches[1]; v2 = v1 + pitches[2];
    u3 = u2 + pitches[1]; v3 = v2 + pitches[2];
    u4 = u3 + pitches[1]; v4 = v3 + pitches[2];

    height -= 4;

    uint8_t v[4], u[4];

    // pack main body
    for (int row = 0; row < height; row += 4)
    {
        for (int col = 0; col < width; col += 2)
        {
            c_interp(v, v1, v2, v3, v4);
            c_interp(u, u1, u2, u3, u4);

            *(dst_1++) = v[0]; *(dst_2++) = v[1];
            *(dst_3++) = v[2]; *(dst_4++) = v[3];
            *(dst_1++) = 255; *(dst_2++) = 255; *(dst_3++) = 255; *(dst_4++) = 255;
            *(dst_1++) = u[0]; *(dst_2++) = u[1];
            *(dst_3++) = u[2]; *(dst_4++) = u[3];
            *(dst_1++) = *(ypt_1++); *(dst_2++) = *(ypt_2++);
            *(dst_3++) = *(ypt_3++); *(dst_4++) = *(ypt_4++);

            *(dst_1++) = v[0]; *(dst_2++) = v[1];
            *(dst_3++) = v[2]; *(dst_4++) = v[3];
            *(dst_1++) = 255; *(dst_2++) = 255; *(dst_3++) = 255; *(dst_4++) = 255;
            *(dst_1++) = u[0]; *(dst_2++) = u[1];
            *(dst_3++) = u[2]; *(dst_4++) = u[3];
            *(dst_1++) = *(ypt_1++); *(dst_2++) = *(ypt_2++);
            *(dst_3++) = *(ypt_3++); *(dst_4++) = *(ypt_4++);

            v1++; v2++; v3++; v4++;
            u1++; u2++; u3++; u4++;
        }
        ypt_1 += ywrap; ypt_2 += ywrap; ypt_3 += ywrap; ypt_4 += ywrap;
        u1 += uwrap; u2 += uwrap; u3 += uwrap; u4 += uwrap;
        v1 += vwrap; v2 += vwrap; v3 += vwrap; v4 += vwrap;
        dst_1 += dwrap; dst_2 += dwrap; dst_3 += dwrap; dst_4 += dwrap;
    }
}
