/******************************************************************************
 * Copyright (C) 2004 Beam Ltd
 * This file is part of MythTv.
 * This file contains free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software    
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA
 *
 * Author: Terry Barnaby
 * Description:  This file contains the Video Codec interface for the XvMC 
 *     VLD X-Windows extension that supports the Hardware MPEG decoding 
 *     engine in the Via Unichrome Chipset.
 * Date: 3/9/04
 *
 ******************************************************************************
*/
#include "avcodec.h"
#include "dsputil.h"
#include "mpegvideo.h"
#include "xvmc_render.h"

#include "xvmccommon.c"

int XVMC_VLD_field_start(MpegEncContext* s, AVCodecContext* avctx)
{
    xvmc_render_state_t*    render;
    XvMCMpegControl     binfo;
    XvMCQMatrix      qmatrix;
    int          i;

    memset(&binfo, 0, sizeof(binfo));
    memset(&qmatrix, 0, sizeof(qmatrix));

    render = (xvmc_render_state_t *)s->current_picture.data[2];
    assert(render != NULL);

    render->picture_structure = s->picture_structure;
    if (render->picture_structure == PICT_FRAME)
        render->flags = XVMC_FRAME_PICTURE;
    else if (render->picture_structure == PICT_TOP_FIELD)
        render->flags = XVMC_TOP_FIELD;
    else if (render->picture_structure == PICT_BOTTOM_FIELD)
        render->flags = XVMC_BOTTOM_FIELD;

    if (!s->first_field)
        render->flags |= XVMC_SECOND_FIELD;

    render->p_future_surface = NULL;
    render->p_past_surface = NULL;

    render->pict_type = s->pict_type; // for later frame dropping use

    switch(s->pict_type)
    {
    case  I_TYPE:
        // no prediction from other frames
        break;

    case  B_TYPE:
        render->p_past_surface = findPastSurface(s, render);
        render->p_future_surface = findFutureSurface(s);
        if (!render->p_past_surface)
            av_log(avctx, AV_LOG_ERROR, "error: decoding B frame and past frame is null!");
        else if (!render->p_future_surface)
            av_log(avctx, AV_LOG_ERROR, "error: decoding B frame and future frame is null!");
        break;
    case  P_TYPE:
        render->p_past_surface = findPastSurface(s, render);
        if (!render->p_past_surface)
            av_log(avctx, AV_LOG_ERROR, "error: decoding P frame and past frame is null!");
        break;
    }

    for (i = 0; i < 64; i++)
    {
        if (s->alternate_scan)
        {
            /* Not sure if this is correct. If MPEG video streams have their
             * QMatrix encoded in the alternative_scan mode rather than zig_zag,
             * when the IDCT's are sent in alternative_scan mode, then we
             * should re-zigzag and then de-alternative_scan the QMatrix data
             * in intra_matrix and inter_matirx. The standard libavcodec always
             * de-zigzags the incoming MPeg QMatrix.
             */
            qmatrix.intra_quantiser_matrix[i] = s->intra_matrix[s->dsp.idct_permutation[i]];
            qmatrix.non_intra_quantiser_matrix[i] = s->inter_matrix[s->dsp.idct_permutation[i]];
            qmatrix.chroma_intra_quantiser_matrix[i] = s->chroma_intra_matrix[s->dsp.idct_permutation[i]];
            qmatrix.chroma_non_intra_quantiser_matrix[i] = s->chroma_inter_matrix[s->dsp.idct_permutation[i]];
        }
        else
        {
            qmatrix.intra_quantiser_matrix[i] = s->intra_matrix[s->dsp.idct_permutation[i]];
            qmatrix.non_intra_quantiser_matrix[i] = s->inter_matrix[s->dsp.idct_permutation[i]];
            qmatrix.chroma_intra_quantiser_matrix[i] = s->chroma_intra_matrix[s->dsp.idct_permutation[i]];
            qmatrix.chroma_non_intra_quantiser_matrix[i] = s->chroma_inter_matrix[s->dsp.idct_permutation[i]];
        }
    }

    qmatrix.load_intra_quantiser_matrix = 1;
    qmatrix.load_non_intra_quantiser_matrix = 1;
    qmatrix.load_chroma_intra_quantiser_matrix = 1;
    qmatrix.load_chroma_non_intra_quantiser_matrix = 1;

    binfo.flags = 0;
    if (s->alternate_scan)
        binfo.flags |= XVMC_ALTERNATE_SCAN;
    if (s->top_field_first)
        binfo.flags |= XVMC_TOP_FIELD_FIRST;
    if (s->frame_pred_frame_dct)
        binfo.flags |= XVMC_PRED_DCT_FRAME;
    else
        binfo.flags |= XVMC_PRED_DCT_FIELD;

    if (s->intra_vlc_format)
        binfo.flags |= XVMC_INTRA_VLC_FORMAT;
    if (!s->first_field && !s->progressive_sequence)
        binfo.flags |= XVMC_SECOND_FIELD;
    if (s->q_scale_type)
        binfo.flags |= XVMC_Q_SCALE_TYPE;
    if (s->concealment_motion_vectors)
        binfo.flags |= XVMC_CONCEALMENT_MOTION_VECTORS;
    if (s->progressive_sequence)
        binfo.flags |= XVMC_PROGRESSIVE_SEQUENCE;

    binfo.picture_structure = s->picture_structure;
    switch (s->pict_type)
    {
    case I_TYPE:    binfo.picture_coding_type = XVMC_I_PICTURE;     break;
    case P_TYPE:    binfo.picture_coding_type = XVMC_P_PICTURE;     break;
    case B_TYPE:    binfo.picture_coding_type = XVMC_B_PICTURE;     break;
    default:    av_log(avctx, AV_LOG_ERROR, "%s: Unknown picture coding type: %d\n", __FUNCTION__, s->pict_type);
    }

    binfo.intra_dc_precision = s->intra_dc_precision;;

    if (s->codec_id == CODEC_ID_MPEG2VIDEO)
        binfo.mpeg_coding = 2;
    else
        binfo.mpeg_coding = 1;

#ifdef ZAP
    /* Don't know if these are needed ??? */
    s->mb_width = (s->width + 15) / 16;
    s->mb_height = (s->codec_id == CODEC_ID_MPEG2VIDEO && !s->progressive_sequence) ?
        2 * ((s->height + 31) / 32) : (s->height + 15) / 16;
#endif

    binfo.FVMV_range = (s->mpeg_f_code[0][1] - 1);
    binfo.FHMV_range = (s->mpeg_f_code[0][0] - 1);
    binfo.BVMV_range = (s->mpeg_f_code[1][1] - 1);
    binfo.BHMV_range = (s->mpeg_f_code[1][0] - 1);

    Status status;

    status = XvMCLoadQMatrix(render->disp, render->ctx, &qmatrix);
    if (status)
        av_log(avctx, AV_LOG_ERROR, "XvMCLoadQMatrix: Error: %d\n", status);

    status = XvMCBeginSurface(render->disp, render->ctx, render->p_surface, 
                              render->p_past_surface, render->p_future_surface,
                              &binfo);
    if (status)
        av_log(avctx, AV_LOG_ERROR, "XvMCBeginSurface: Error: %d\n", status);

    return 0;
}

void XVMC_VLD_field_end(MpegEncContext* s)
{
    xvmc_render_state_t*    render;

    render = (xvmc_render_state_t *)s->current_picture.data[2];
    assert(render != NULL);

    XvMCFlushSurface(render->disp, render->p_surface);
    XvMCSyncSurface(render->disp, render->p_surface);

    s->error_count = 0;
}

static int length_to_next_start(uint8_t* pbuf_ptr, int buf_size)
{
    uint8_t*    buf_ptr;
    unsigned int    state = 0xFFFFFFFF, v;

    buf_ptr = pbuf_ptr;
    while (buf_ptr < pbuf_ptr + buf_size)
    {
        v = *buf_ptr++;
        if (state == 0x000001) {
            return buf_ptr - pbuf_ptr - 4;
        }
        state = ((state << 8) | v) & 0xffffff;
    }
    return -1;
}

#define SLICE_MIN_START_CODE   0x00000101
#define SLICE_MAX_START_CODE   0x000001af


int XVMC_VLD_decode_slice(MpegEncContext* s, int mb_y, uint8_t* buffer, int buf_size)
{
    int slicelen = length_to_next_start(buffer, buf_size);
    xvmc_render_state_t*    render;

    if (slicelen < 0)
    {
        if ((mb_y == s->mb_height - 1) || 
            (!s->progressive_sequence && (mb_y == ((s->mb_height >> 1) -1))))
            slicelen = buf_size;
        else
            return -1;
    }

    render = (xvmc_render_state_t*)s->current_picture.data[2];
    render->slice_code = SLICE_MIN_START_CODE + mb_y;
    render->slice_data = buffer;
    render->slice_datalen = slicelen;

    ff_draw_horiz_band(s, 0, 0);

    return slicelen;
}

