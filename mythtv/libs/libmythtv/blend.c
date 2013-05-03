#include "blend.h"

#ifdef MMX

#include "ffmpeg-mmx.h"

static const mmx_t mm_cpool[] =
{
    { .uw = {128, 128, 128, 128} },
    { .uw = {255, 255, 255, 255} },
    { .uw = {514, 514, 514, 514} },
    { .uw = {1, 1, 1, 1} },
    { .uw = {257, 257, 257, 257} }
};

void blendregion_mmx (uint8_t * ysrc, uint8_t * usrc, uint8_t * vsrc,
                      uint8_t * asrc, int srcstrd,
                      uint8_t * ydst, uint8_t * udst, uint8_t * vdst,
                      uint8_t * adst, int dststrd,
                      int width, int height, int alphamod, int dochroma,
                      int16_t rec_lut[256], uint8_t pow_lut[256][256])
{
    int x, y, i, alpha, newalpha;
    mmx_t amod = { .uw = {alphamod, alphamod, alphamod, alphamod} };
    int16_t wbuf[8];

    (void) pow_lut;
    for (y = 0; y < height; y++)
    {
        for (x = 0; x + 7 < width; x += 8)
        {
            movq_m2r (asrc[x], mm0);
            movq_m2r (adst[x], mm2);
            movq_m2r (mm_cpool[0], mm6);
            pxor_r2r (mm7, mm7);
            movq_m2r (amod, mm5);
            movq_m2r (mm_cpool[1], mm4);
            movq_r2r (mm0, mm1);
            movq_r2r (mm2, mm3);

            punpcklbw_r2r (mm7, mm0);
            punpckhbw_r2r (mm7, mm1);
            punpcklbw_r2r (mm7, mm2);
            punpckhbw_r2r (mm7, mm3);

            pmullw_r2r (mm5, mm0); /* asrc *= amod */
            pmullw_r2r (mm5, mm1);

            movq_r2r (mm4, mm5);

            paddw_r2r (mm6, mm0); /* asrc += 0x80 */
            paddw_r2r (mm6, mm1);

            psubw_r2r (mm2, mm4); /* 255 - adst */
            psubw_r2r (mm3, mm5);

            psrlw_i2r (8, mm0); /* asrc >>= 8 */
            psrlw_i2r (8, mm1);

            movq_m2r (mm_cpool[3], mm6); /* load more constants */
            movq_m2r (mm_cpool[2], mm7);

            pmullw_r2r (mm0, mm4); /* (255 - adst) * asrc */
            pmullw_r2r (mm1, mm5);

            paddw_r2r (mm6, mm4); /* +1 and >>1 for fixup */
            paddw_r2r (mm6, mm5);

            psrlw_i2r (1, mm4); /* because MMX lacks unsigned mult */
            psrlw_i2r (1, mm5);
            
            pmulhw_r2r (mm7, mm4); /* (255 - adst) * asrc / 255 */
            pmulhw_r2r (mm7, mm5);

            movq_m2r (mm_cpool[1], mm6); /* another constant preload */

            paddw_r2r (mm2, mm4); /* add asrc for new adst value */
            paddw_r2r (mm3, mm5);

            movq_r2r (mm6, mm7); /* copy 255 const to mm7 */

            packuswb_r2r (mm5, mm4); /* pack new adst to byte */

            psubw_r2r (mm0, mm6); /* 255 - asrc */
            psubw_r2r (mm1, mm7);

            movq_r2m (mm4, adst[x]); /* store new adst value */
            movq_m2r (mm_cpool[3], mm5); /* constant preloads */
            movq_m2r (mm_cpool[2], mm4);

            pmullw_r2r (mm2, mm6); /* (255 - asrc) * adst */
            pmullw_r2r (mm3, mm7);

            paddw_r2r (mm5, mm6); /* add one */
            paddw_r2r (mm5, mm7);

            psrlw_i2r (1, mm6); /* rotate right one */
            psrlw_i2r (1, mm7);

            pmulhw_r2r (mm4, mm6); /* (255 - asrc) * adst / 255 */
            pmulhw_r2r (mm4, mm7);

            paddw_r2r (mm0, mm6); /* and add asrc */
            paddw_r2r (mm1, mm7);

            movq_m2r (ysrc[x], mm2); /* preload some luma data */
            movq_m2r (ydst[x], mm4);
            
            movq_r2m (mm6, wbuf[0]); /* write "div" to memory */
            movq_r2m (mm7, wbuf[4]); /* for nasty LUT op */
            movq_m2r (mm_cpool[4], mm7); /* 257 const */ 
            pxor_r2r (mm6, mm6);

            movq_r2r (mm2, mm3);
            movq_r2r (mm4, mm5);

            for (i = 0; i < 8; i++)
                wbuf[i] = rec_lut[wbuf[i]];

            pmullw_m2r (wbuf[0], mm0); /* divide by multiplying */
            pmullw_m2r (wbuf[4], mm1);

            punpcklbw_r2r (mm6, mm2);
            punpckhbw_r2r (mm6, mm3);
            punpcklbw_r2r (mm6, mm4);
            punpckhbw_r2r (mm6, mm5);

            psrlw_i2r (7, mm0); /* newalpha */
            psrlw_i2r (7, mm1);

            psubw_r2r (mm4, mm2); /* ysrc - ydst */
            psubw_r2r (mm5, mm3);

            pmullw_r2r (mm7, mm0); /* newalpha * (2^16 / 255) */
            pmullw_r2r (mm7, mm1);

            movq_m2r (mm_cpool[3], mm7);

            psllw_i2r (2, mm2); /* luma diff << 1 */
            psllw_i2r (2, mm3);

            psrlw_i2r (1, mm0); /* (newalpha * (2^16 / 255)) >> 1 */
            psrlw_i2r (1, mm1); /* saves a load of fixup pain to do it this way */

            pmulhw_r2r (mm0, mm2); /* mutiply, now with FREE truncation! */
            pmulhw_r2r (mm1, mm3); /* that's right, absolutely FREE!!! */

            paddw_r2r (mm7, mm2);
            paddw_r2r (mm7, mm3);

            psraw_i2r (1, mm2);
            psraw_i2r (1, mm3);

            paddw_r2r (mm4, mm2); /* add to ydst */
            paddw_r2r (mm5, mm3);

            packuswb_r2r (mm3, mm2); /* pack it up */

            movq_r2m (mm2, ydst[x]); /* save blended luma */
            if ((y & 1) == 0 && dochroma) /* do chroma each even line */
            {
                pslld_i2r (16, mm0); /* could do this with mask logic */
                movd_m2r (usrc[x>>1], mm2);
                pslld_i2r (16, mm1); /* but shifts are cheap */
                movd_m2r (vsrc[x>>1], mm3);
                psrld_i2r (16, mm0); /* shift again */
                movd_m2r (udst[x>>1], mm4);
                psrld_i2r (16, mm1);
                movd_m2r (vdst[x>>1], mm5);


                punpcklbw_r2r (mm6, mm2);
                punpcklbw_r2r (mm6, mm3);
                punpcklbw_r2r (mm6, mm4);
                punpcklbw_r2r (mm6, mm5);

                packssdw_r2r (mm1, mm0); /* pack even columns of alpha */

                psubw_r2r (mm4, mm2);
                psubw_r2r (mm5, mm3);
                /* argh, wish I had something to do here */
                psllw_i2r (2, mm2);
                psllw_i2r (2, mm3);
                
                pmulhw_r2r (mm0, mm2);
                pmulhw_r2r (mm0, mm3);
                /* doubly so here, pmul* is slow on P4 */
                paddw_r2r (mm7, mm2); /* round */
                paddw_r2r (mm7, mm3);

                psraw_i2r (1, mm2);
                psraw_i2r (1, mm3);

                paddw_r2r (mm4, mm2);
                paddw_r2r (mm5, mm3);

                packuswb_r2r (mm7, mm2); /* bah, more stalling */
                packuswb_r2r (mm7, mm3); /* mm7? we only need low DWORD */

                movd_r2m (mm2, udst[x>>1]);
                movd_r2m (mm3, vdst[x>>1]);
            }
        }
        for (/*x*/; x < width; x++)
        {
            /* The above is known to round differently than the C code in
             * blendregion.  To prevent cleanup columns from showing this
             * difference, we use a C implementation of the MMX blend */
            alpha = (asrc[x] * alphamod + 0x80) >> 8;
            newalpha = ((((((255 - alpha) * adst[x]) + 1) >> 1) * 514) >> 16) + alpha;
            newalpha = (((alpha * rec_lut[newalpha]) >> 7) * 257) >> 1;
            adst[x] = ((((((255 - adst[x]) * alpha) + 1) >> 1) * 514) >> 16) + adst[x];
            ydst[x] = (((((ysrc[x] - ydst[x]) << 2) * newalpha) + 65536) >> 17) + ydst[x];
            if (((y & 1) | (x & 1)) == 0 && dochroma)
            {
                udst[x>>1] = (((((usrc[x>>1] - udst[x>>1]) << 2) * newalpha) + 65536) >> 17) + udst[x>>1];
                vdst[x>>1] = (((((vsrc[x>>1] - vdst[x>>1]) << 2) * newalpha) + 65536) >> 17) + vdst[x>>1];
            }
        }
                
        ysrc += srcstrd;
        asrc += srcstrd;
        ydst += dststrd;
        adst += dststrd;

        if ((y & 1) == 0 && dochroma)
        {
            usrc += srcstrd >> 1;
            vsrc += srcstrd >> 1;
            udst += dststrd >> 1;
            vdst += dststrd >> 1;
        }
    }
    emms();
}

void blendcolumn2_mmx (uint8_t * ysrc1, uint8_t * usrc1, uint8_t * vsrc1,
                       uint8_t * asrc1, int srcstrd1,
                       uint8_t * ysrc2, uint8_t * usrc2, uint8_t * vsrc2,
                       uint8_t * asrc2, int srcstrd2,
                       uint8_t * mask,
                       uint8_t * ydst, uint8_t * udst, uint8_t * vdst,
                       uint8_t * adst, int dststrd,
                       int width, int height, int alphamod, int dochroma,
                       int16_t rec_lut[256], uint8_t pow_lut[256][256])
{
    int x, y, i, alpha, newalpha;
    mmx_t amod = { .uw = {alphamod, alphamod, alphamod, alphamod} };
    mmx_t maskm;
    mmx_t ysrc1m;
    mmx_t uvsrc1m;
    mmx_t asrc1m;
    mmx_t ysrc2m;
    mmx_t uvsrc2m;
    mmx_t asrc2m;
    uint8_t * ysrc, * asrc, * usrc, * vsrc;
    int16_t wbuf[8];

    (void) pow_lut;
    for (y = 0; y < height; y++)
    {
        ysrc1m.ub[0] = *ysrc1;
        ysrc1m.ub[1] = *ysrc1;
        ysrc1m.ub[2] = *ysrc1;
        ysrc1m.ub[3] = *ysrc1;
        ysrc1m.ub[4] = *ysrc1;
        ysrc1m.ub[5] = *ysrc1;
        ysrc1m.ub[6] = *ysrc1;
        ysrc1m.ub[7] = *ysrc1;
        ysrc2m.ub[0] = *ysrc2;
        ysrc2m.ub[1] = *ysrc2;
        ysrc2m.ub[2] = *ysrc2;
        ysrc2m.ub[3] = *ysrc2;
        ysrc2m.ub[4] = *ysrc2;
        ysrc2m.ub[5] = *ysrc2;
        ysrc2m.ub[6] = *ysrc2;
        ysrc2m.ub[7] = *ysrc2;
        asrc1m.ub[0] = *asrc1;
        asrc1m.ub[1] = *asrc1;
        asrc1m.ub[2] = *asrc1;
        asrc1m.ub[3] = *asrc1;
        asrc1m.ub[4] = *asrc1;
        asrc1m.ub[5] = *asrc1;
        asrc1m.ub[6] = *asrc1;
        asrc1m.ub[7] = *asrc1;
        asrc2m.ub[0] = *asrc2;
        asrc2m.ub[1] = *asrc2;
        asrc2m.ub[2] = *asrc2;
        asrc2m.ub[3] = *asrc2;
        asrc2m.ub[4] = *asrc2;
        asrc2m.ub[5] = *asrc2;
        asrc2m.ub[6] = *asrc2;
        asrc2m.ub[7] = *asrc2;
        uvsrc1m.ub[0] = *usrc1;
        uvsrc1m.ub[1] = *usrc1;
        uvsrc1m.ub[2] = *usrc1;
        uvsrc1m.ub[3] = *usrc1;
        uvsrc1m.ub[4] = *vsrc1;
        uvsrc1m.ub[5] = *vsrc1;
        uvsrc1m.ub[6] = *vsrc1;
        uvsrc1m.ub[7] = *vsrc1;
        uvsrc2m.ub[0] = *usrc2;
        uvsrc2m.ub[1] = *usrc2;
        uvsrc2m.ub[2] = *usrc2;
        uvsrc2m.ub[3] = *usrc2;
        uvsrc2m.ub[4] = *vsrc2;
        uvsrc2m.ub[5] = *vsrc2;
        uvsrc2m.ub[6] = *vsrc2;
        uvsrc2m.ub[7] = *vsrc2;
        for (x = 0; x + 7 < width; x += 8)
        {
            movq_m2r (mask[x], mm3);
            movq_m2r (asrc1m, mm0);
            pxor_r2r (mm7, mm7);
            movq_m2r (asrc2m, mm1);
            movq_m2r (adst[x], mm2);
            pcmpeqb_r2r (mm7, mm3);
            movq_m2r (mm_cpool[0], mm6);
            pand_r2r (mm3, mm1);
            movq_r2m (mm3, maskm);
            movq_m2r (amod, mm5);
            pandn_r2r (mm0, mm3);
            movq_m2r (mm_cpool[1], mm4);
            por_r2r (mm3, mm1);
            movq_r2r (mm1, mm0);
            movq_r2r (mm2, mm3);

            punpcklbw_r2r (mm7, mm0);
            punpckhbw_r2r (mm7, mm1);
            punpcklbw_r2r (mm7, mm2);
            punpckhbw_r2r (mm7, mm3);

            pmullw_r2r (mm5, mm0); /* asrc *= amod */
            pmullw_r2r (mm5, mm1);

            movq_r2r (mm4, mm5);

            paddw_r2r (mm6, mm0); /* asrc += 0x80 */
            paddw_r2r (mm6, mm1);

            psubw_r2r (mm2, mm4); /* 255 - adst */
            psubw_r2r (mm3, mm5);

            psrlw_i2r (8, mm0); /* asrc >>= 8 */
            psrlw_i2r (8, mm1);

            movq_m2r (mm_cpool[3], mm6); /* load more constants */
            movq_m2r (mm_cpool[2], mm7);

            pmullw_r2r (mm0, mm4); /* (255 - adst) * asrc */
            pmullw_r2r (mm1, mm5);

            paddw_r2r (mm6, mm4); /* +1 and >>1 for fixup */
            paddw_r2r (mm6, mm5);

            psrlw_i2r (1, mm4); /* because MMX lacks unsigned mult */
            psrlw_i2r (1, mm5);
            
            pmulhw_r2r (mm7, mm4); /* (255 - adst) * asrc / 255 */
            pmulhw_r2r (mm7, mm5);

            movq_m2r (mm_cpool[1], mm6); /* another constant preload */

            paddw_r2r (mm2, mm4); /* add asrc for new adst value */
            paddw_r2r (mm3, mm5);

            movq_r2r (mm6, mm7); /* copy 255 const to mm7 */

            packuswb_r2r (mm5, mm4); /* pack new adst to byte */

            psubw_r2r (mm0, mm6); /* 255 - asrc */
            psubw_r2r (mm1, mm7);

            movq_r2m (mm4, adst[x]); /* store new adst value */
            movq_m2r (mm_cpool[3], mm5); /* constant preloads */
            movq_m2r (mm_cpool[2], mm4);

            pmullw_r2r (mm2, mm6); /* (255 - asrc) * adst */
            pmullw_r2r (mm3, mm7);

            paddw_r2r (mm5, mm6); /* add one */
            paddw_r2r (mm5, mm7);

            psrlw_i2r (1, mm6); /* rotate right one */
            psrlw_i2r (1, mm7);

            pmulhw_r2r (mm4, mm6); /* (255 - asrc) * adst / 255 */
            pmulhw_r2r (mm4, mm7);

            paddw_r2r (mm0, mm6); /* and add asrc */
            paddw_r2r (mm1, mm7);

            movq_m2r (maskm, mm5);
            movq_m2r (ysrc1m, mm2); /* preload some luma data */
            movq_m2r (ysrc2m, mm3);
            movq_m2r (ydst[x], mm4);
            
            movq_r2m (mm6, wbuf[0]); /* write "div" to memory */
            pxor_r2r (mm6, mm6);
            movq_r2m (mm7, wbuf[4]); /* for nasty LUT op */
            movq_m2r (mm_cpool[4], mm7); /* 257 const */ 
            pand_r2r (mm5, mm3);
            pandn_r2r (mm2, mm5);

            for (i = 0; i < 8; i++)
                wbuf[i] = rec_lut[wbuf[i]];

            por_r2r (mm5, mm3);


            pmullw_m2r (wbuf[0], mm0); /* divide by multiplying */
            movq_r2r (mm3, mm2); /* trying so very hard not to stall here */
            movq_r2r (mm4, mm5);
            pmullw_m2r (wbuf[4], mm1);

            punpcklbw_r2r (mm6, mm2);
            punpckhbw_r2r (mm6, mm3);
            punpcklbw_r2r (mm6, mm4);
            punpckhbw_r2r (mm6, mm5);

            psrlw_i2r (7, mm0); /* newalpha */
            psrlw_i2r (7, mm1);

            psubw_r2r (mm4, mm2); /* ysrc - ydst */
            psubw_r2r (mm5, mm3);

            pmullw_r2r (mm7, mm0); /* newalpha * (2^16 / 255) */
            pmullw_r2r (mm7, mm1);

            movq_m2r (mm_cpool[3], mm7);

            psllw_i2r (2, mm2); /* luma diff << 1 */
            psllw_i2r (2, mm3);

            psrlw_i2r (1, mm0); /* (newalpha * (2^16 / 255)) >> 1 */
            psrlw_i2r (1, mm1); /* saves a load of fixup pain to do it this way */

            pmulhw_r2r (mm0, mm2); /* mutiply, now with FREE truncation! */
            pmulhw_r2r (mm1, mm3); /* that's right, absolutely FREE!!! */

            paddw_r2r (mm7, mm2);
            paddw_r2r (mm7, mm3);

            psraw_i2r (1, mm2);
            psraw_i2r (1, mm3);

            paddw_r2r (mm4, mm2); /* add to ydst */
            paddw_r2r (mm5, mm3);

            packuswb_r2r (mm3, mm2); /* pack it up */

            movq_r2m (mm2, ydst[x]); /* save blended luma */
            if ((y & 1) == 0 && dochroma) /* do chroma each even line */
            {
                pslld_i2r (16, mm0); /* could do this with mask logic */
                movq_m2r (maskm, mm6); /* reload mask */
                pslld_i2r (16, mm1); /* but shifts are cheap */
                psllw_i2r (8, mm6);
                movq_m2r (uvsrc1m, mm2);
                psrlw_i2r (8, mm6);
                movq_m2r (uvsrc2m, mm3);
                packuswb_r2r (mm6, mm6); /* pack even columns of mask */
                psrld_i2r (16, mm0); /* shift again */
                psrld_i2r (16, mm1);

                pand_r2r (mm6, mm3);
                pandn_r2r (mm2, mm6);
                movd_m2r (udst[x>>1], mm4);
                por_r2r (mm6, mm3);
                movd_m2r (vdst[x>>1], mm5);
                
                movq_r2r (mm3, mm2);
                pxor_r2r (mm6, mm6);
                packssdw_r2r (mm1, mm0); /* pack even columns of alpha */
                psrlq_i2r (32, mm3);

                punpcklbw_r2r (mm6, mm2);
                punpcklbw_r2r (mm6, mm3);
                punpcklbw_r2r (mm6, mm4);
                punpcklbw_r2r (mm6, mm5);


                psubw_r2r (mm4, mm2);
                psubw_r2r (mm5, mm3);
                /* argh, wish I had something to do here */
                psllw_i2r (2, mm2);
                psllw_i2r (2, mm3);
                
                pmulhw_r2r (mm0, mm2);
                pmulhw_r2r (mm0, mm3);
                /* doubly so here, pmul* is slow on P4 */
                paddw_r2r (mm7, mm2); /* round */
                paddw_r2r (mm7, mm3);

                psraw_i2r (1, mm2);
                psraw_i2r (1, mm3);

                paddw_r2r (mm4, mm2);
                paddw_r2r (mm5, mm3);

                packuswb_r2r (mm7, mm2); /* bah, more stalling */
                packuswb_r2r (mm7, mm3); /* mm7? we only need low DWORD */

                movd_r2m (mm2, udst[x>>1]);
                movd_r2m (mm3, vdst[x>>1]);
            }
        }
        for (/*x*/; x < width; x++)
        {
            /* The above is known to round differently than the C code in
             * blendregion.  To prevent cleanup columns from showing this
             * difference, we use a C implementation of the MMX blend */
            if (mask[x])
            {
                ysrc = ysrc1;
                usrc = usrc1;
                vsrc = vsrc1;
                asrc = asrc1;
            }
            else
            {
                ysrc = ysrc2;
                usrc = usrc2;
                vsrc = vsrc2;
                asrc = asrc2;
            }
            alpha = (*asrc * alphamod + 0x80) >> 8;
            newalpha = ((((((255 - alpha) * adst[x]) + 1) >> 1) * 514) >> 16) + alpha;
            newalpha = (((alpha * rec_lut[newalpha]) >> 7) * 257) >> 1;
            adst[x] = ((((((255 - adst[x]) * alpha) + 1) >> 1) * 514) >> 16) + adst[x];
            ydst[x] = (((((*ysrc - ydst[x]) << 2) * newalpha) + 65536) >> 17) + ydst[x];
            if (((y & 1) | (x & 1)) == 0 && dochroma)
            {
                udst[x>>1] = (((((*usrc - udst[x>>1]) << 2) * newalpha) + 65536) >> 17) + udst[x>>1];
                vdst[x>>1] = (((((*vsrc - vdst[x>>1]) << 2) * newalpha) + 65536) >> 17) + vdst[x>>1];
            }
        }
                
        ysrc1 += srcstrd1;
        asrc1 += srcstrd1;
        ysrc2 += srcstrd2;
        asrc2 += srcstrd2;
        ydst += dststrd;
        adst += dststrd;

        if ((y & 1) == 0 && dochroma)
        {
            usrc1 += srcstrd1 >> 1;
            vsrc1 += srcstrd1 >> 1;
            usrc2 += srcstrd2 >> 1;
            vsrc2 += srcstrd2 >> 1;
            udst += dststrd >> 1;
            vdst += dststrd >> 1;
        }
    }
    emms();
}

void blendcolor_mmx (uint8_t ysrc, uint8_t usrc, uint8_t vsrc,
                      uint8_t * asrc, int srcstrd,
                      uint8_t * ydst, uint8_t * udst, uint8_t * vdst,
                      uint8_t * adst, int dststrd,
                      int width, int height, int alphamod, int dochroma,
                      int16_t rec_lut[256], uint8_t pow_lut[256][256])
{
    int x, y, i, alpha, newalpha;
    mmx_t amod = { .uw = {alphamod, alphamod, alphamod, alphamod} };
    mmx_t ysrcm = { .uw = {ysrc, ysrc, ysrc, ysrc} };
    mmx_t usrcm = { .uw = {usrc, usrc, usrc, usrc} };
    mmx_t vsrcm = { .uw = {vsrc, vsrc, vsrc, vsrc} };
    int16_t wbuf[8];

    (void) pow_lut;
    for (y = 0; y < height; y++)
    {
        for (x = 0; x + 7 < width; x += 8)
        {
            movq_m2r (asrc[x], mm0);
            movq_m2r (adst[x], mm2);
            movq_m2r (mm_cpool[0], mm6);
            pxor_r2r (mm7, mm7);
            movq_m2r (amod, mm5);
            movq_m2r (mm_cpool[1], mm4);
            movq_r2r (mm0, mm1);
            movq_r2r (mm2, mm3);

            punpcklbw_r2r (mm7, mm0);
            punpckhbw_r2r (mm7, mm1);
            punpcklbw_r2r (mm7, mm2);
            punpckhbw_r2r (mm7, mm3);

            pmullw_r2r (mm5, mm0); /* asrc *= amod */
            pmullw_r2r (mm5, mm1);

            movq_r2r (mm4, mm5);

            paddw_r2r (mm6, mm0); /* asrc += 0x80 */
            paddw_r2r (mm6, mm1);

            psubw_r2r (mm2, mm4); /* 255 - adst */
            psubw_r2r (mm3, mm5);

            psrlw_i2r (8, mm0); /* asrc >>= 8 */
            psrlw_i2r (8, mm1);

            movq_m2r (mm_cpool[3], mm6); /* load more constants */
            movq_m2r (mm_cpool[2], mm7);

            pmullw_r2r (mm0, mm4); /* (255 - adst) * asrc */
            pmullw_r2r (mm1, mm5);

            paddw_r2r (mm6, mm4); /* +1 and >>1 for fixup */
            paddw_r2r (mm6, mm5);

            psrlw_i2r (1, mm4); /* because MMX lacks unsigned mult */
            psrlw_i2r (1, mm5);
            
            pmulhw_r2r (mm7, mm4); /* (255 - adst) * asrc / 255 */
            pmulhw_r2r (mm7, mm5);

            movq_m2r (mm_cpool[1], mm6); /* another constant preload */

            paddw_r2r (mm2, mm4); /* add asrc for new adst value */
            paddw_r2r (mm3, mm5);

            movq_r2r (mm6, mm7); /* copy 255 const to mm7 */

            packuswb_r2r (mm5, mm4); /* pack new adst to byte */

            psubw_r2r (mm0, mm6); /* 255 - asrc */
            psubw_r2r (mm1, mm7);

            movq_r2m (mm4, adst[x]); /* store new adst value */
            movq_m2r (mm_cpool[3], mm5); /* constant preloads */
            movq_m2r (mm_cpool[2], mm4);

            pmullw_r2r (mm2, mm6); /* (255 - asrc) * adst */
            pmullw_r2r (mm3, mm7);

            paddw_r2r (mm5, mm6); /* add one */
            paddw_r2r (mm5, mm7);

            psrlw_i2r (1, mm6); /* rotate right one */
            psrlw_i2r (1, mm7);

            pmulhw_r2r (mm4, mm6); /* (255 - asrc) * adst / 255 */
            pmulhw_r2r (mm4, mm7);

            paddw_r2r (mm0, mm6); /* and add asrc */
            paddw_r2r (mm1, mm7);

            movq_m2r (ysrcm, mm2); /* preload some luma data */
            movq_m2r (ydst[x], mm4);
            
            movq_r2m (mm6, wbuf[0]); /* write "div" to memory */
            movq_r2m (mm7, wbuf[4]); /* for nasty LUT op */
            movq_m2r (mm_cpool[4], mm7); /* 257 const */ 
            pxor_r2r (mm6, mm6);

            movq_r2r (mm2, mm3);
            movq_r2r (mm4, mm5);

            for (i = 0; i < 8; i++)
                wbuf[i] = rec_lut[wbuf[i]];

            pmullw_m2r (wbuf[0], mm0); /* divide by multiplying */
            pmullw_m2r (wbuf[4], mm1);

            punpcklbw_r2r (mm6, mm4);
            punpckhbw_r2r (mm6, mm5);

            psrlw_i2r (7, mm0); /* newalpha */
            psrlw_i2r (7, mm1);

            psubw_r2r (mm4, mm2); /* ysrc - ydst */
            psubw_r2r (mm5, mm3);

            pmullw_r2r (mm7, mm0); /* newalpha * (2^16 / 255) */
            pmullw_r2r (mm7, mm1);

            movq_m2r (mm_cpool[3], mm7);

            psllw_i2r (2, mm2); /* luma diff << 1 */
            psllw_i2r (2, mm3);

            psrlw_i2r (1, mm0); /* (newalpha * (2^16 / 255)) >> 1 */
            psrlw_i2r (1, mm1); /* saves a load of fixup pain to do it this way */

            pmulhw_r2r (mm0, mm2); /* mutiply, now with FREE truncation! */
            pmulhw_r2r (mm1, mm3); /* that's right, absolutely FREE!!! */

            paddw_r2r (mm7, mm2);
            paddw_r2r (mm7, mm3);

            psraw_i2r (1, mm2);
            psraw_i2r (1, mm3);

            paddw_r2r (mm4, mm2); /* add to ydst */
            paddw_r2r (mm5, mm3);

            packuswb_r2r (mm3, mm2); /* pack it up */

            movq_r2m (mm2, ydst[x]); /* save blended luma */
            if ((y & 1) == 0 && dochroma) /* do chroma each even line */
            {
                pslld_i2r (16, mm0); /* could do this with mask logic */
                movq_m2r (usrcm, mm2);
                pslld_i2r (16, mm1); /* but shifts are cheap */
                movq_m2r (vsrcm, mm3);
                psrld_i2r (16, mm0); /* shift again */
                movd_m2r (udst[x>>1], mm4);
                psrld_i2r (16, mm1);
                movd_m2r (vdst[x>>1], mm5);

                punpcklbw_r2r (mm6, mm4);
                punpcklbw_r2r (mm6, mm5);

                packssdw_r2r (mm1, mm0); /* pack even columns of alpha */

                psubw_r2r (mm4, mm2);
                psubw_r2r (mm5, mm3);
                /* argh, wish I had something to do here */
                psllw_i2r (2, mm2);
                psllw_i2r (2, mm3);
                
                pmulhw_r2r (mm0, mm2);
                pmulhw_r2r (mm0, mm3);
                /* doubly so here, pmul* is slow on P4 */
                paddw_r2r (mm7, mm2); /* round */
                paddw_r2r (mm7, mm3);

                psraw_i2r (1, mm2);
                psraw_i2r (1, mm3);

                paddw_r2r (mm4, mm2);
                paddw_r2r (mm5, mm3);

                packuswb_r2r (mm7, mm2); /* bah, more stalling */
                packuswb_r2r (mm7, mm3); /* mm7? we only need low DWORD */

                movd_r2m (mm2, udst[x>>1]);
                movd_r2m (mm3, vdst[x>>1]);
            }
        }
        for (/*x*/; x < width; x++)
        {
            /* The above is known to round differently than the C code in
             * blendregion.  To prevent cleanup columns from showing this
             * difference, we use a C implementation of the MMX blend */
            alpha = (asrc[x] * alphamod + 0x80) >> 8;
            newalpha = ((((((255 - alpha) * adst[x]) + 1) >> 1) * 514) >> 16) + alpha;
            newalpha = (((alpha * rec_lut[newalpha]) >> 7) * 257) >> 1;
            adst[x] = ((((((255 - adst[x]) * alpha) + 1) >> 1) * 514) >> 16) + adst[x];
            ydst[x] = (((((ysrc - ydst[x]) << 2) * newalpha) + 65536) >> 17) + ydst[x];
            if (((y & 1) | (x & 1)) == 0 && dochroma)
            {
                udst[x>>1] = (((((usrc - udst[x>>1]) << 2) * newalpha) + 65536) >> 17) + udst[x>>1];
                vdst[x>>1] = (((((vsrc - vdst[x>>1]) << 2) * newalpha) + 65536) >> 17) + vdst[x>>1];
            }
        }
                
        asrc += srcstrd;
        ydst += dststrd;
        adst += dststrd;

        if ((y & 1) == 0 && dochroma)
        {
            udst += dststrd >> 1;
            vdst += dststrd >> 1;
        }
    }
    emms();
}

void blendconst_mmx (uint8_t ysrc, uint8_t usrc, uint8_t vsrc,
                      uint8_t asrc,
                      uint8_t * ydst, uint8_t * udst, uint8_t * vdst,
                      uint8_t * adst, int dststrd,
                      int width, int height, int dochroma,
                      int16_t rec_lut[256], uint8_t pow_lut[256][256])
{
    int x, y, i, alpha, newalpha;
    mmx_t ysrcm = { .uw = {ysrc, ysrc, ysrc, ysrc} };
    mmx_t usrcm = { .uw = {usrc, usrc, usrc, usrc} };
    mmx_t vsrcm = { .uw = {vsrc, vsrc, vsrc, vsrc} };
    mmx_t asrcm = { .uw = {asrc, asrc, asrc, asrc} };
    int16_t wbuf[8];

    (void) pow_lut;
    for (y = 0; y < height; y++)
    {
        for (x = 0; x + 7 < width; x += 8)
        {
            movq_m2r (asrcm, mm0);
            movq_m2r (adst[x], mm2);
            movq_m2r (mm_cpool[0], mm6);
            pxor_r2r (mm7, mm7);
            movq_m2r (mm_cpool[1], mm4);
            movq_r2r (mm0, mm1);
            movq_r2r (mm2, mm3);

            punpcklbw_r2r (mm7, mm2);
            punpckhbw_r2r (mm7, mm3);

            movq_r2r (mm4, mm5);

            psubw_r2r (mm2, mm4); /* 255 - adst */
            psubw_r2r (mm3, mm5);

            movq_m2r (mm_cpool[3], mm6); /* load more constants */
            movq_m2r (mm_cpool[2], mm7);

            pmullw_r2r (mm0, mm4); /* (255 - adst) * asrc */
            pmullw_r2r (mm1, mm5);

            paddw_r2r (mm6, mm4); /* +1 and >>1 for fixup */
            paddw_r2r (mm6, mm5);

            psrlw_i2r (1, mm4); /* because MMX lacks unsigned mult */
            psrlw_i2r (1, mm5);
            
            pmulhw_r2r (mm7, mm4); /* (255 - adst) * asrc / 255 */
            pmulhw_r2r (mm7, mm5);

            movq_m2r (mm_cpool[1], mm6); /* another constant preload */

            paddw_r2r (mm2, mm4); /* add asrc for new adst value */
            paddw_r2r (mm3, mm5);

            movq_r2r (mm6, mm7); /* copy 255 const to mm7 */

            packuswb_r2r (mm5, mm4); /* pack new adst to byte */

            psubw_r2r (mm0, mm6); /* 255 - asrc */
            psubw_r2r (mm1, mm7);

            movq_r2m (mm4, adst[x]); /* store new adst value */
            movq_m2r (mm_cpool[3], mm5); /* constant preloads */
            movq_m2r (mm_cpool[2], mm4);

            pmullw_r2r (mm2, mm6); /* (255 - asrc) * adst */
            pmullw_r2r (mm3, mm7);

            paddw_r2r (mm5, mm6); /* add one */
            paddw_r2r (mm5, mm7);

            psrlw_i2r (1, mm6); /* rotate right one */
            psrlw_i2r (1, mm7);

            pmulhw_r2r (mm4, mm6); /* (255 - asrc) * adst / 255 */
            pmulhw_r2r (mm4, mm7);

            paddw_r2r (mm0, mm6); /* and add asrc */
            paddw_r2r (mm1, mm7);

            movq_m2r (ysrcm, mm2); /* preload some luma data */
            movq_m2r (ydst[x], mm4);
            
            movq_r2m (mm6, wbuf[0]); /* write "div" to memory */
            movq_r2m (mm7, wbuf[4]); /* for nasty LUT op */
            movq_m2r (mm_cpool[4], mm7); /* 257 const */ 
            pxor_r2r (mm6, mm6);

            movq_r2r (mm2, mm3);
            movq_r2r (mm4, mm5);

            for (i = 0; i < 8; i++)
                wbuf[i] = rec_lut[wbuf[i]];

            pmullw_m2r (wbuf[0], mm0); /* divide by multiplying */
            pmullw_m2r (wbuf[4], mm1);

            punpcklbw_r2r (mm6, mm4);
            punpckhbw_r2r (mm6, mm5);

            psrlw_i2r (7, mm0); /* newalpha */
            psrlw_i2r (7, mm1);

            psubw_r2r (mm4, mm2); /* ysrc - ydst */
            psubw_r2r (mm5, mm3);

            pmullw_r2r (mm7, mm0); /* newalpha * (2^16 / 255) */
            pmullw_r2r (mm7, mm1);

            movq_m2r (mm_cpool[3], mm7);

            psllw_i2r (2, mm2); /* luma diff << 1 */
            psllw_i2r (2, mm3);

            psrlw_i2r (1, mm0); /* (newalpha * (2^16 / 255)) >> 1 */
            psrlw_i2r (1, mm1); /* saves a load of fixup pain to do it this way */

            pmulhw_r2r (mm0, mm2); /* mutiply, now with FREE truncation! */
            pmulhw_r2r (mm1, mm3); /* that's right, absolutely FREE!!! */

            paddw_r2r (mm7, mm2);
            paddw_r2r (mm7, mm3);

            psraw_i2r (1, mm2);
            psraw_i2r (1, mm3);

            paddw_r2r (mm4, mm2); /* add to ydst */
            paddw_r2r (mm5, mm3);

            packuswb_r2r (mm3, mm2); /* pack it up */

            movq_r2m (mm2, ydst[x]); /* save blended luma */
            if ((y & 1) == 0 && dochroma) /* do chroma each even line */
            {
                pslld_i2r (16, mm0); /* could do this with mask logic */
                movq_m2r (usrcm, mm2);
                pslld_i2r (16, mm1); /* but shifts are cheap */
                movq_m2r (vsrcm, mm3);
                psrld_i2r (16, mm0); /* shift again */
                movd_m2r (udst[x>>1], mm4);
                psrld_i2r (16, mm1);
                movd_m2r (vdst[x>>1], mm5);

                punpcklbw_r2r (mm6, mm4);
                punpcklbw_r2r (mm6, mm5);

                packssdw_r2r (mm1, mm0); /* pack even columns of alpha */

                psubw_r2r (mm4, mm2);
                psubw_r2r (mm5, mm3);
                /* argh, wish I had something to do here */
                psllw_i2r (2, mm2);
                psllw_i2r (2, mm3);
                
                pmulhw_r2r (mm0, mm2);
                pmulhw_r2r (mm0, mm3);
                /* doubly so here, pmul* is slow on P4 */
                paddw_r2r (mm7, mm2); /* round */
                paddw_r2r (mm7, mm3);

                psraw_i2r (1, mm2);
                psraw_i2r (1, mm3);

                paddw_r2r (mm4, mm2);
                paddw_r2r (mm5, mm3);

                packuswb_r2r (mm7, mm2); /* bah, more stalling */
                packuswb_r2r (mm7, mm3); /* mm7? we only need low DWORD */

                movd_r2m (mm2, udst[x>>1]);
                movd_r2m (mm3, vdst[x>>1]);
            }
        }
        for (/*x*/; x < width; x++)
        {
            /* The above is known to round differently than the C code in
             * blendregion.  To prevent cleanup columns from showing this
             * difference, we use a C implementation of the MMX blend */
            alpha = asrc;
            newalpha = ((((((255 - alpha) * adst[x]) + 1) >> 1) * 514) >> 16) + alpha;
            newalpha = (((alpha * rec_lut[newalpha]) >> 7) * 257) >> 1;
            adst[x] = ((((((255 - adst[x]) * alpha) + 1) >> 1) * 514) >> 16) + adst[x];
            ydst[x] = (((((ysrc - ydst[x]) << 2) * newalpha) + 65536) >> 17) + ydst[x];
            if (((y & 1) | (x & 1)) == 0 && dochroma)
            {
                udst[x>>1] = (((((usrc - udst[x>>1]) << 2) * newalpha) + 65536) >> 17) + udst[x>>1];
                vdst[x>>1] = (((((vsrc - vdst[x>>1]) << 2) * newalpha) + 65536) >> 17) + vdst[x>>1];
            }
        }
                
        ydst += dststrd;
        adst += dststrd;

        if ((y & 1) == 0 && dochroma)
        {
            udst += dststrd >> 1;
            vdst += dststrd >> 1;
        }
    }
    emms();
}

void blendcolumn_mmx (uint8_t * ysrc, uint8_t * usrc, uint8_t * vsrc,
                      uint8_t * asrc, int srcstrd,
                      uint8_t * ydst, uint8_t * udst, uint8_t * vdst,
                      uint8_t * adst, int dststrd,
                      int width, int height, int alphamod, int dochroma,
                      int16_t rec_lut[256], uint8_t pow_lut[256][256])
{
    int x, y, i, alpha, newalpha;
    mmx_t amod = { .uw = {alphamod, alphamod, alphamod, alphamod} };
    mmx_t ysrcm;
    mmx_t usrcm;
    mmx_t vsrcm;
    mmx_t asrcm;
    int16_t wbuf[8];

    (void) pow_lut;
    for (y = 0; y < height; y++)
    {
        /* preload the MMX values for the column, as packed unsigned words */
        ysrcm.uw[0] = *ysrc;
        ysrcm.uw[1] = *ysrc;
        ysrcm.uw[2] = *ysrc;
        ysrcm.uw[3] = *ysrc;
        asrcm.uw[0] = *asrc;
        asrcm.uw[1] = *asrc;
        asrcm.uw[2] = *asrc;
        asrcm.uw[3] = *asrc;
        /* this only need to be done every other line for U/V, but doing it
         * conditionally slows things down, and it's safe to do it
         * unconditionally */
        usrcm.uw[0] = *usrc;
        usrcm.uw[1] = *usrc;
        usrcm.uw[2] = *usrc;
        usrcm.uw[3] = *usrc;
        vsrcm.uw[0] = *vsrc;
        vsrcm.uw[1] = *vsrc;
        vsrcm.uw[2] = *vsrc;
        vsrcm.uw[3] = *vsrc;
        for (x = 0; x + 7 < width; x += 8)
        {
            movq_m2r (asrcm, mm0);
            movq_m2r (adst[x], mm2);
            movq_m2r (mm_cpool[0], mm6);
            pxor_r2r (mm7, mm7);
            movq_m2r (amod, mm5);
            movq_m2r (mm_cpool[1], mm4);
            movq_r2r (mm0, mm1);
            movq_r2r (mm2, mm3);

            punpcklbw_r2r (mm7, mm2);
            punpckhbw_r2r (mm7, mm3);

            pmullw_r2r (mm5, mm0); /* asrc *= amod */
            pmullw_r2r (mm5, mm1);

            movq_r2r (mm4, mm5);

            paddw_r2r (mm6, mm0); /* asrc += 0x80 */
            paddw_r2r (mm6, mm1);

            psubw_r2r (mm2, mm4); /* 255 - adst */
            psubw_r2r (mm3, mm5);

            psrlw_i2r (8, mm0); /* asrc >>= 8 */
            psrlw_i2r (8, mm1);

            movq_m2r (mm_cpool[3], mm6); /* load more constants */
            movq_m2r (mm_cpool[2], mm7);

            pmullw_r2r (mm0, mm4); /* (255 - adst) * asrc */
            pmullw_r2r (mm1, mm5);

            paddw_r2r (mm6, mm4); /* +1 and >>1 for fixup */
            paddw_r2r (mm6, mm5);

            psrlw_i2r (1, mm4); /* because MMX lacks unsigned mult */
            psrlw_i2r (1, mm5);
            
            pmulhw_r2r (mm7, mm4); /* (255 - adst) * asrc / 255 */
            pmulhw_r2r (mm7, mm5);

            movq_m2r (mm_cpool[1], mm6); /* another constant preload */

            paddw_r2r (mm2, mm4); /* add asrc for new adst value */
            paddw_r2r (mm3, mm5);

            movq_r2r (mm6, mm7); /* copy 255 const to mm7 */

            packuswb_r2r (mm5, mm4); /* pack new adst to byte */

            psubw_r2r (mm0, mm6); /* 255 - asrc */
            psubw_r2r (mm1, mm7);

            movq_r2m (mm4, adst[x]); /* store new adst value */
            movq_m2r (mm_cpool[3], mm5); /* constant preloads */
            movq_m2r (mm_cpool[2], mm4);

            pmullw_r2r (mm2, mm6); /* (255 - asrc) * adst */
            pmullw_r2r (mm3, mm7);

            paddw_r2r (mm5, mm6); /* add one */
            paddw_r2r (mm5, mm7);

            psrlw_i2r (1, mm6); /* rotate right one */
            psrlw_i2r (1, mm7);

            pmulhw_r2r (mm4, mm6); /* (255 - asrc) * adst / 255 */
            pmulhw_r2r (mm4, mm7);

            paddw_r2r (mm0, mm6); /* and add asrc */
            paddw_r2r (mm1, mm7);

            movq_m2r (ysrcm, mm2); /* preload some luma data */
            movq_m2r (ydst[x], mm4);
            
            movq_r2m (mm6, wbuf[0]); /* write "div" to memory */
            movq_r2m (mm7, wbuf[4]); /* for nasty LUT op */
            movq_m2r (mm_cpool[4], mm7); /* 257 const */ 
            pxor_r2r (mm6, mm6);

            movq_r2r (mm2, mm3);
            movq_r2r (mm4, mm5);

            for (i = 0; i < 8; i++)
                wbuf[i] = rec_lut[wbuf[i]];

            pmullw_m2r (wbuf[0], mm0); /* divide by multiplying */
            pmullw_m2r (wbuf[4], mm1);

            punpcklbw_r2r (mm6, mm4);
            punpckhbw_r2r (mm6, mm5);

            psrlw_i2r (7, mm0); /* newalpha */
            psrlw_i2r (7, mm1);

            psubw_r2r (mm4, mm2); /* ysrc - ydst */
            psubw_r2r (mm5, mm3);

            pmullw_r2r (mm7, mm0); /* newalpha * (2^16 / 255) */
            pmullw_r2r (mm7, mm1);

            movq_m2r (mm_cpool[3], mm7);

            psllw_i2r (2, mm2); /* luma diff << 1 */
            psllw_i2r (2, mm3);

            psrlw_i2r (1, mm0); /* (newalpha * (2^16 / 255)) >> 1 */
            psrlw_i2r (1, mm1); /* saves a load of fixup pain to do it this way */

            pmulhw_r2r (mm0, mm2); /* mutiply, now with FREE truncation! */
            pmulhw_r2r (mm1, mm3); /* that's right, absolutely FREE!!! */

            paddw_r2r (mm7, mm2);
            paddw_r2r (mm7, mm3);

            psraw_i2r (1, mm2);
            psraw_i2r (1, mm3);

            paddw_r2r (mm4, mm2); /* add to ydst */
            paddw_r2r (mm5, mm3);

            packuswb_r2r (mm3, mm2); /* pack it up */

            movq_r2m (mm2, ydst[x]); /* save blended luma */
            if ((y & 1) == 0 && dochroma) /* do chroma each even line */
            {
                pslld_i2r (16, mm0); /* could do this with mask logic */
                movq_m2r (usrcm, mm2);
                pslld_i2r (16, mm1); /* but shifts are cheap */
                movq_m2r (vsrcm, mm3);
                psrld_i2r (16, mm0); /* shift again */
                movd_m2r (udst[x>>1], mm4);
                psrld_i2r (16, mm1);
                movd_m2r (vdst[x>>1], mm5);

                punpcklbw_r2r (mm6, mm4);
                punpcklbw_r2r (mm6, mm5);

                packssdw_r2r (mm1, mm0); /* pack even columns of alpha */

                psubw_r2r (mm4, mm2);
                psubw_r2r (mm5, mm3);
                /* argh, wish I had something to do here */
                psllw_i2r (2, mm2);
                psllw_i2r (2, mm3);
                
                pmulhw_r2r (mm0, mm2);
                pmulhw_r2r (mm0, mm3);
                /* doubly so here, pmul* is slow on P4 */
                paddw_r2r (mm7, mm2); /* round */
                paddw_r2r (mm7, mm3);

                psraw_i2r (1, mm2);
                psraw_i2r (1, mm3);

                paddw_r2r (mm4, mm2);
                paddw_r2r (mm5, mm3);

                packuswb_r2r (mm7, mm2); /* bah, more stalling */
                packuswb_r2r (mm7, mm3); /* mm7? we only need low DWORD */

                movd_r2m (mm2, udst[x>>1]);
                movd_r2m (mm3, vdst[x>>1]);
            }
        }
        for (/*x*/; x < width; x++)
        {
            /* The above is known to round differently than the C code in
             * blendregion.  To prevent cleanup columns from showing this
             * difference, we use a C implementation of the MMX blend */
            alpha = (*asrc * alphamod + 0x80) >> 8;
            newalpha = ((((((255 - alpha) * adst[x]) + 1) >> 1) * 514) >> 16) + alpha;
            newalpha = (((alpha * rec_lut[newalpha]) >> 7) * 257) >> 1;
            adst[x] = ((((((255 - adst[x]) * alpha) + 1) >> 1) * 514) >> 16) + adst[x];
            ydst[x] = (((((*ysrc - ydst[x]) << 2) * newalpha) + 65536) >> 17) + ydst[x];
            if (((y & 1) | (x & 1)) == 0 && dochroma)
            {
                udst[x>>1] = (((((*usrc - udst[x>>1]) << 2) * newalpha) + 65536) >> 17) + udst[x>>1];
                vdst[x>>1] = (((((*vsrc - vdst[x>>1]) << 2) * newalpha) + 65536) >> 17) + vdst[x>>1];
            }
        }
                
        ysrc += srcstrd;
        asrc += srcstrd;
        ydst += dststrd;
        adst += dststrd;

        if ((y & 1) == 0 && dochroma)
        {
            usrc += srcstrd >> 1;
            vsrc += srcstrd >> 1;
            udst += dststrd >> 1;
            vdst += dststrd >> 1;
        }
    }
    emms();
}
#endif /*MMX*/

void blendregion (uint8_t * ysrc, uint8_t * usrc, uint8_t * vsrc,
                  uint8_t * asrc, int srcstrd,
                  uint8_t * ydst, uint8_t * udst, uint8_t * vdst,
                  uint8_t * adst, int dststrd,
                  int width, int height, int alphamod, int dochroma,
                  int16_t rec_lut[256], uint8_t pow_lut[256][256])
{
    int newalpha, alpha;
    int x, y;

    (void) rec_lut;
    for (y = 0; y < height; y++)
    {
        if ((y & 1) || dochroma == 0)
        {
            for (x = 0; x < width; x++)
            {
                alpha = ((asrc[x] * alphamod) + 0x80) >> 8;

                newalpha = pow_lut [alpha][adst[x]] * 257;
                adst[x] = ((255 - adst[x]) * alpha) / 255 + adst[x];
                ydst[x] += (((ysrc[x] - ydst[x]) * newalpha) + 32768) >> 16;
            }
            ysrc += srcstrd;
            asrc += srcstrd;
            ydst += dststrd;
            adst += dststrd;
        }
        else
        {
            for (x = 0; x < width; x++)
            {
                alpha = ((asrc[x] * alphamod) + 0x80) >> 8;

                newalpha = pow_lut [alpha][adst[x]] * 257;
                adst[x] = ((255 - adst[x]) * alpha) / 255 + adst[x];
                ydst[x] += (((ysrc[x] - ydst[x]) * newalpha) + 32768) >> 16;
                if ((x & 1) == 0)
                {
                    udst[x>>1] += (((usrc[x>>1] - udst[x>>1]) * newalpha) + 32768) >> 16;
                    vdst[x>>1] += (((vsrc[x>>1] - vdst[x>>1]) * newalpha) + 32768) >> 16;
                }
            }
            ysrc += srcstrd;
            asrc += srcstrd;
            ydst += dststrd;
            adst += dststrd;
            usrc += srcstrd >> 1;
            vsrc += srcstrd >> 1;
            udst += dststrd >> 1;
            vdst += dststrd >> 1;
        }
    }
}

void blendcolumn2 (uint8_t * ysrc1, uint8_t * usrc1, uint8_t * vsrc1,
                   uint8_t * asrc1, int srcstrd1,
                   uint8_t * ysrc2, uint8_t * usrc2, uint8_t * vsrc2,
                   uint8_t * asrc2, int srcstrd2,
                   uint8_t * mask,
                   uint8_t * ydst, uint8_t * udst, uint8_t * vdst,
                   uint8_t * adst, int dststrd,
                   int width, int height, int alphamod, int dochroma,
                   int16_t rec_lut[256], uint8_t pow_lut[256][256])
{
    int newalpha, alpha;
    int x, y;
    uint8_t * ysrc, * usrc, * vsrc, * asrc;

    (void) rec_lut;
    for (y = 0; y < height; y++)
    {
        if ((y & 1) || dochroma == 0)
        {
            for (x = 0; x < width; x++)
            {
                if (mask[x])
                {
                    ysrc = ysrc1;
                    asrc = asrc2;
                }
                else
                {
                    ysrc = ysrc2;
                    asrc = asrc2;
                }
                alpha = ((*asrc * alphamod) + 0x80) >> 8;

                newalpha = pow_lut [alpha][adst[x]] * 257;
                adst[x] = ((255 - adst[x]) * alpha) / 255 + adst[x];
                ydst[x] += (((*ysrc - ydst[x]) * newalpha) + 32768) >> 16;
            }
            ysrc1 += srcstrd1;
            asrc1 += srcstrd1;
            ysrc2 += srcstrd2;
            asrc2 += srcstrd2;
            ydst += dststrd;
            adst += dststrd;
        }
        else
        {
            for (x = 0; x < width; x++)
            {
                if (mask[x])
                {
                    ysrc = ysrc1;
                    usrc = usrc1;
                    vsrc = vsrc1;
                    asrc = asrc1;
                }
                else
                {
                    ysrc = ysrc2;
                    usrc = usrc2;
                    vsrc = vsrc2;
                    asrc = asrc2;
                }
                alpha = ((*asrc * alphamod) + 0x80) >> 8;

                newalpha = pow_lut [alpha][adst[x]] * 257;
                adst[x] = ((255 - adst[x]) * alpha) / 255 + adst[x];
                ydst[x] += (((*ysrc - ydst[x]) * newalpha) + 32768) >> 16;
                if ((x & 1) == 0)
                {
                    udst[x>>1] += (((*usrc - udst[x>>1]) * newalpha) + 32768) >> 16;
                    vdst[x>>1] += (((*vsrc - vdst[x>>1]) * newalpha) + 32768) >> 16;
                }
            }
            ysrc1 += srcstrd1;
            asrc1 += srcstrd1;
            ysrc2 += srcstrd2;
            asrc2 += srcstrd2;
            ydst += dststrd;
            adst += dststrd;
            usrc1 += srcstrd1 >> 1;
            vsrc1 += srcstrd1 >> 1;
            usrc2 += srcstrd2 >> 1;
            vsrc2 += srcstrd2 >> 1;
            udst += dststrd >> 1;
            vdst += dststrd >> 1;
        }
    }
}

void blendcolor (uint8_t ysrc, uint8_t usrc, uint8_t vsrc,
                 uint8_t * asrc, int srcstrd,
                 uint8_t * ydst, uint8_t * udst, uint8_t * vdst,
                 uint8_t * adst, int dststrd,
                 int width, int height, int alphamod, int dochroma,
                 int16_t rec_lut[256], uint8_t pow_lut[256][256])
{
    int newalpha, alpha;
    int x, y;

    (void) rec_lut;
    for (y = 0; y < height; y++)
    {
        if ((y & 1) || dochroma == 0)
        {
            for (x = 0; x < width; x++)
            {
                alpha = ((asrc[x] * alphamod) + 0x80) >> 8;

                newalpha = pow_lut [alpha][adst[x]] * 257;
                adst[x] = ((255 - adst[x]) * alpha) / 255 + adst[x];
                ydst[x] += (((ysrc - ydst[x]) * newalpha) + 32768) >> 16;
            }
            asrc += srcstrd;
            ydst += dststrd;
            adst += dststrd;
        }
        else
        {
            for (x = 0; x < width; x++)
            {
                alpha = ((asrc[x] * alphamod) + 0x80) >> 8;

                newalpha = pow_lut [alpha][adst[x]] * 257;
                adst[x] = ((255 - adst[x]) * alpha) / 255 + adst[x];
                ydst[x] += (((ysrc - ydst[x]) * newalpha) + 32768) >> 16;
                if ((x & 1) == 0)
                {
                    udst[x>>1] += (((usrc - udst[x>>1]) * newalpha) + 32768) >> 16;
                    vdst[x>>1] += (((vsrc - vdst[x>>1]) * newalpha) + 32768) >> 16;
                }
            }
            asrc += srcstrd;
            ydst += dststrd;
            adst += dststrd;
            udst += dststrd >> 1;
            vdst += dststrd >> 1;
        }
    }
}

void blendconst (uint8_t ysrc, uint8_t usrc, uint8_t vsrc,
                 uint8_t asrc,
                 uint8_t * ydst, uint8_t * udst, uint8_t * vdst,
                 uint8_t * adst, int dststrd,
                 int width, int height, int dochroma,
                 int16_t rec_lut[256], uint8_t pow_lut[256][256])
{
    int newalpha, alpha;
    int x, y;

    (void) rec_lut;
    for (y = 0; y < height; y++)
    {
        if ((y & 1) || dochroma == 0)
        {
            for (x = 0; x < width; x++)
            {
                alpha = asrc;

                newalpha = pow_lut [alpha][adst[x]] * 257;
                adst[x] = ((255 - adst[x]) * alpha) / 255 + adst[x];
                ydst[x] += (((ysrc - ydst[x]) * newalpha) + 32768) >> 16;
            }
            ydst += dststrd;
            adst += dststrd;
        }
        else
        {
            for (x = 0; x < width; x++)
            {
                alpha = asrc;

                newalpha = pow_lut [alpha][adst[x]] * 257;
                adst[x] = ((255 - adst[x]) * alpha) / 255 + adst[x];
                ydst[x] += (((ysrc - ydst[x]) * newalpha) + 32768) >> 16;
                if ((x & 1) == 0)
                {
                    udst[x>>1] += (((usrc - udst[x>>1]) * newalpha) + 32768) >> 16;
                    vdst[x>>1] += (((vsrc - vdst[x>>1]) * newalpha) + 32768) >> 16;
                }
            }
            ydst += dststrd;
            adst += dststrd;
            udst += dststrd >> 1;
            vdst += dststrd >> 1;
        }
    }
}

void blendcolumn (uint8_t * ysrc, uint8_t * usrc, uint8_t * vsrc,
                  uint8_t * asrc, int srcstrd,
                  uint8_t * ydst, uint8_t * udst, uint8_t * vdst,
                  uint8_t * adst, int dststrd,
                  int width, int height, int alphamod, int dochroma,
                  int16_t rec_lut[256], uint8_t pow_lut[256][256])
{
    int newalpha, alpha;
    int x, y;

    (void) rec_lut;
    for (y = 0; y < height; y++)
    {
        if ((y & 1) || dochroma == 0)
        {
            for (x = 0; x < width; x++)
            {
                alpha = ((*asrc * alphamod) + 0x80) >> 8;

                newalpha = pow_lut [alpha][adst[x]] * 257;
                adst[x] = ((255 - adst[x]) * alpha) / 255 + adst[x];
                ydst[x] += (((*ysrc - ydst[x]) * newalpha) + 32768) >> 16;
            }
            ysrc += srcstrd;
            asrc += srcstrd;
            ydst += dststrd;
            adst += dststrd;
        }
        else
        {
            for (x = 0; x < width; x++)
            {
                alpha = ((*asrc * alphamod) + 0x80) >> 8;

                newalpha = pow_lut [alpha][adst[x]] * 257;
                adst[x] = ((255 - adst[x]) * alpha) / 255 + adst[x];
                ydst[x] += (((*ysrc - ydst[x]) * newalpha) + 32768) >> 16;
                if ((x & 1) == 0)
                {
                    udst[x>>1] += (((*usrc - udst[x>>1]) * newalpha) + 32768) >> 16;
                    vdst[x>>1] += (((*vsrc - vdst[x>>1]) * newalpha) + 32768) >> 16;
                }
            }
            ysrc += srcstrd;
            asrc += srcstrd;
            ydst += dststrd;
            adst += dststrd;
            usrc += srcstrd >> 1;
            vsrc += srcstrd >> 1;
            udst += dststrd >> 1;
            vdst += dststrd >> 1;
        }
    }
}
