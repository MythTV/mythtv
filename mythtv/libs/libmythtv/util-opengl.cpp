// -*- Mode: c++ -*-

#include <string.h>
#include <stdint.h>
#include <QSize>
#include "compat.h"
#include "util-opengl.h"

#ifdef MMX
extern "C" {
#include "ffmpeg-mmx.h"
}

static const mmx_t mmx_1s = {0xffffffffffffffffULL};

static inline void mmx_pack_alpha1s_high(const uint8_t *y1, const uint8_t *y2)
{
    movq_m2r (mmx_1s, mm4);
    punpckhbw_m2r (*y1, mm4);
    movq_m2r (mmx_1s, mm7);
    punpckhbw_m2r (*y2, mm7);
}

static inline void mmx_pack_alpha1s_low(const uint8_t *y1, const uint8_t *y2)
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

static inline void mmx_pack_easy(uint8_t *dest, const uint8_t *y)
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

static const mmx_t mmx_0s = {0x0000000000000000LL};
static const mmx_t mmx_round  = {0x0002000200020002LL};

static inline void mmx_interp_start(const uint8_t *left, const uint8_t *right)
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

static inline void mmx_pack_chroma(const uint8_t *u, const uint8_t *v)
{
    movd_m2r (*u,  mm2);
    movd_m2r (*v,  mm3);
    punpcklbw_r2r (mm2, mm2);
    punpcklbw_r2r (mm3, mm3);
}
#endif // MMX

static inline void c_interp(unsigned dest[4], unsigned a, unsigned b,
                            unsigned c, unsigned d)
{
    unsigned int tmp = a;
    tmp *= 3;
    tmp += 2;
    tmp += c;
    dest[0] = tmp >> 2;

    tmp = b;
    tmp *= 3;
    tmp += 2;
    tmp += d;
    dest[1] = tmp >> 2;

    tmp = c;
    tmp *= 3;
    tmp += 2;
    tmp += a;
    dest[2] = tmp >> 2;

    tmp = d;
    tmp *= 3;
    tmp += 2;
    tmp += b;
    dest[3] = tmp >> 2;
}

#ifdef __GNUC__
#define MYTH_PACKED __attribute__((packed))
#else
#define MYTH_PACKED
#endif
static inline unsigned c_pack2(uint8_t dest[],
    unsigned v, unsigned u, unsigned y1, unsigned y2)
{
    struct pack
    {
        uint8_t v1, a1, u1, y1;
        uint8_t v2, a2, u2, y2;
    } MYTH_PACKED tmp = {v, 0xff, u, y1, v, 0xff, u, y2};

    *reinterpret_cast< struct pack * >(dest) = tmp;

    return sizeof tmp;
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
                         unsigned char *dest,
                         const int offsets[3],
                         const int pitches[3],
                         const QSize &size)
{
    const int width = size.width();
    int height = size.height();

    if (height % 4 || width % 2)
        return;

    const uint bgra_width   = width << 2;
    const uint chroma_width = width >> 1;
          uint ywrap        = (pitches[0] << 1) - width;
    const uint uwrap        = (pitches[1] << 1) - chroma_width;
    const uint vwrap        = (pitches[2] << 1) - chroma_width;

    const uint8_t *ypt_1    = static_cast< const uint8_t * >(source) + offsets[0];
    const uint8_t *ypt_2    = ypt_1 + pitches[0];
    const uint8_t *ypt_3    = ypt_1 + (pitches[0] * (height - 2));
    const uint8_t *ypt_4    = ypt_3 + pitches[0];

    const uint8_t *u1       = static_cast< const uint8_t * >(source) + offsets[1];
    const uint8_t *v1       = static_cast< const uint8_t * >(source) + offsets[2];
    const uint8_t *u2       = u1 + pitches[1];
    const uint8_t *v2       = v1 + pitches[2];
    const uint8_t *u3       = u1 + (pitches[1] * ((height - 4) >> 1));
    const uint8_t *v3       = v1 + (pitches[2] * ((height - 4) >> 1));
    const uint8_t *u4       = u3 + pitches[1];
    const uint8_t *v4       = v3 + pitches[2];

    uint8_t *dst_1          = static_cast< uint8_t * >(dest);
    uint8_t *dst_3          = dst_1 + (bgra_width * (height - 2));

    // Allocate a 4 line packed data buffer
    // NB dest is probably graphics memory so access may be slow
    const uint bufsize = bgra_width * 4;
    uint8_t *buf = new uint8_t[bufsize];

    uint8_t *b1 = buf;
    uint8_t *b2 = b1 + bgra_width;
    const uint len = bgra_width * 2; // 2 line buffer size
    uint8_t *b3 = buf + len;
    uint8_t *b4 = b3 + bgra_width;

#ifdef MMX

    if (!(width % 8))
    {
        // pack first 2 and last 2 rows
        for (int col = 0; col < width; col += 8)
        {
            mmx_pack_chroma(u1, v1);
            mmx_pack_easy(b1, ypt_1);
            mmx_pack_chroma(u2, v2);
            mmx_pack_easy(b2, ypt_2);
            mmx_pack_chroma(u3, v3);
            mmx_pack_easy(b3, ypt_3);
            mmx_pack_chroma(u4, v4);
            mmx_pack_easy(b4, ypt_4);

            b1 += 32; b2 += 32; b3 += 32; b4 += 32;
            ypt_1 += 8; ypt_2 += 8; ypt_3 += 8; ypt_4 += 8;
            u1   += 4; v1   += 4; u2   += 4; v2   += 4;
            u3   += 4; v3   += 4; u4   += 4; v4   += 4;
        }

        memcpy(dst_1, buf, len);
        dst_1 += len;
        memcpy(dst_3, buf + len, len);

        ypt_1 += ywrap; ypt_2 += ywrap;
        ypt_3 = ypt_2 + pitches[0];
        ypt_4 = ypt_3 + pitches[0];

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
            b1 = buf;
            b2 = b1 + bgra_width;
            b3 = b2 + bgra_width;
            b4 = b3 + bgra_width;

            for (int col = 0; col < width; col += 8)
            {
                mmx_interp_start(u1, u3); mmx_interp_endu();
                mmx_interp_start(v1, v3); mmx_interp_endv();
                mmx_pack_easy(b1, ypt_1);

                mmx_interp_start(u2, u4); mmx_interp_endu();
                mmx_interp_start(v2, v4); mmx_interp_endv();
                mmx_pack_easy(b2, ypt_2);

                mmx_interp_start(u3, u1); mmx_interp_endu();
                mmx_interp_start(v3, v1); mmx_interp_endv();
                mmx_pack_easy(b3, ypt_3);

                mmx_interp_start(u4, u2); mmx_interp_endu();
                mmx_interp_start(v4, v2); mmx_interp_endv();
                mmx_pack_easy(b4, ypt_4);

                b1 += 32; b2 += 32; b3 += 32; b4 += 32;
                ypt_1 += 8; ypt_2 += 8; ypt_3 += 8; ypt_4 += 8;
                u1   += 4; u2   += 4; u3   += 4; u4   += 4;
                v1   += 4; v2   += 4; v3   += 4; v4   += 4;
            }

            // Copy the packed data to dest
            memcpy(dst_1, buf, bufsize);
            dst_1 += bufsize;

            ypt_1 += ywrap; ypt_2 += ywrap; ypt_3 += ywrap; ypt_4 += ywrap;
            u1 += uwrap; v1 += vwrap; u2 += uwrap; v2 += vwrap;
            u3 += uwrap; v3 += vwrap; u4 += uwrap;v4 += vwrap;
        }

        emms();

        delete[] buf;
        return;
    }
#endif //MMX

    // pack first 2 and last 2 rows
    for (int col = 0; col < width; col += 2)
    {
        b1 += c_pack2(b1, *(v1++), *(u1++), ypt_1[col], ypt_1[col + 1]);
        b2 += c_pack2(b2, *(v2++), *(u2++), ypt_2[col], ypt_2[col + 1]);
        b3 += c_pack2(b3, *(v3++), *(u3++), ypt_3[col], ypt_3[col + 1]);
        b4 += c_pack2(b4, *(v4++), *(u4++), ypt_4[col], ypt_4[col + 1]);
    }

    // Copy the packed data to dest
    memcpy(dst_1, buf, len);
    dst_1 += len;
    memcpy(dst_3, buf + len, len);

    ywrap = pitches[0] << 2;

    ypt_1 += ywrap; ypt_2 += ywrap;
    ypt_3 = ypt_2 + pitches[0];
    ypt_4 = ypt_3 + pitches[0];

    u1 = static_cast< const uint8_t * >(source) + offsets[1];
    v1 = static_cast< const uint8_t * >(source) + offsets[2];
    u2 = u1 + pitches[1]; v2 = v1 + pitches[2];
    u3 = u2 + pitches[1]; v3 = v2 + pitches[2];
    u4 = u3 + pitches[1]; v4 = v3 + pitches[2];

    height -= 4;

    // pack main body
    for (int row = 0; row < height; row += 4)
    {
        b1 = buf;
        b2 = b1 + bgra_width;
        b3 = b2 + bgra_width;
        b4 = b3 + bgra_width;

        for (int col = 0; col < width; col += 2)
        {
            unsigned v[4], u[4];

            c_interp(v, *(v1++), *(v2++), *(v3++), *(v4++));
            c_interp(u, *(u1++), *(u2++), *(u3++), *(u4++));

            b1 += c_pack2(b1, v[0], u[0], ypt_1[col], ypt_1[col + 1]);
            b2 += c_pack2(b2, v[1], u[1], ypt_2[col], ypt_2[col + 1]);
            b3 += c_pack2(b3, v[2], u[2], ypt_3[col], ypt_3[col + 1]);
            b4 += c_pack2(b4, v[3], u[3], ypt_4[col], ypt_4[col + 1]);
        }

        // Copy the packed data to dest
        memcpy(dst_1, buf, bufsize);
        dst_1 += bufsize;

        ypt_1 += ywrap; ypt_2 += ywrap; ypt_3 += ywrap; ypt_4 += ywrap;
        u1 += uwrap; u2 += uwrap; u3 += uwrap; u4 += uwrap;
        v1 += vwrap; v2 += vwrap; v3 += vwrap; v4 += vwrap;
    }

    delete[] buf;
}
