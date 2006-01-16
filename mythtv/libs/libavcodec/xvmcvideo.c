/*
 * XVideo Motion Compensation
 * Copyright (c) 2003 Ivan Kalvachev
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include <limits.h>

//avcodec include
#include "avcodec.h"
#include "dsputil.h"
#include "mpegvideo.h"

#undef NDEBUG
#include <assert.h>
#include <stdio.h>

#ifdef USE_FASTMEMCPY
#include "fastmemcpy.h"
#endif

#ifdef HAVE_XVMC

//X11 includes are in the xvmc_render.h
//by replacing it with none-X one
//XvMC emulation could be performed

#include "xvmc_render.h"
#define DEBUG_PICTURE_STRUCTURE_CHANGES

#ifndef FALSE
#define FALSE 0
#endif
#ifndef TRUE
#define TRUE 1
#endif

#define BLKS_SIZE (sizeof(short)*64)

// if chroma_format < 2, 4 luminance + 2 color blocks in macroblock
static const int chromaBits[4] = { 6, 6, 4+1<<CHROMA_422, 4+1<<CHROMA_444 };
static const int mv_ffmpeg_to_xvmc[8];
static const char* mv_ffmpeg_to_string[8];

static inline xvmc_render_state_t *render_state(const MpegEncContext *s);
static inline int calc_cbp(MpegEncContext *s);
static void set_block_pattern(const MpegEncContext *, XvMCMacroBlock *);
static void setup_pmv(MpegEncContext *, XvMCMacroBlock *);
static void setup_pmv_frame(MpegEncContext *, XvMCMacroBlock *);
static void setup_pmv_field(MpegEncContext *, XvMCMacroBlock *);
static inline XvMCMacroBlock *macroblock(const xvmc_render_state_t *,
                                            int x, int y, int dct);
static inline void setup_context(MpegEncContext *s);
static inline void DBG_printPictureStructureChanges(int picture_structure);
static inline void handle_intra_block(const MpegEncContext *, const XvMCMacroBlock *,
                                      xvmc_render_state_t *render);
static inline void handle_p_b_block(const MpegEncContext *, const XvMCMacroBlock *,
                                    xvmc_render_state_t *render);

#include "xvmccommon.c"

//set s->block
void XVMC_init_block(MpegEncContext *s)
{
    xvmc_render_state_t *render = render_state(s);
    s->block= (DCTELEM*) (render->data_blocks+(render->next_free_data_block_num)*64);
}

void XVMC_pack_pblocks(MpegEncContext *s, int cbp){
int i,j;
const int mb_block_count = 4+(1<<s->chroma_format);

    j=0;
    cbp<<= 12-mb_block_count;
    for(i=0; i<mb_block_count; i++){
        if (cbp & (1<<11)) {
           s->pblocks[i] = (short *)(&s->block[(j++)]);
        }else{
           s->pblocks[i] = NULL;
        }
        cbp+=cbp;
    }
}

//these functions should be called on every new field or/and frame
//They should be safe if they are called few times for same field!
int XVMC_field_start(MpegEncContext*s, AVCodecContext *avctx){
xvmc_render_state_t * render,* last, * next;

    assert(avctx != NULL);

    render = (xvmc_render_state_t*)s->current_picture.data[2];
    assert(render != NULL);
    if( (render == NULL) || (render->magic != MP_XVMC_RENDER_MAGIC) )
        return -1;//make sure that this is render packet

    render->picture_structure = s->picture_structure;
    if (render->picture_structure==PICT_FRAME)
        render->flags = XVMC_FRAME_PICTURE;
    else if (render->picture_structure==PICT_TOP_FIELD)
        render->flags = XVMC_TOP_FIELD;
    else if (render->picture_structure==PICT_BOTTOM_FIELD)
        render->flags = XVMC_BOTTOM_FIELD;
    if (!s->first_field) 
        render->flags |= XVMC_SECOND_FIELD;

//make sure that all data is drawn by XVMC_end_frame
    assert(render->filled_mv_blocks_num==0);

    render->p_future_surface = NULL;
    render->p_past_surface = NULL;

    render->pict_type=s->pict_type; // for later frame dropping use
    switch(s->pict_type){
        case  I_TYPE:
            return 0;// no prediction from other frames
        case  B_TYPE:
            render->p_past_surface = findPastSurface(s, render);
            render->p_future_surface = findFutureSurface(s);
            if (!render->p_past_surface)
                av_log(s->avctx, AV_LOG_ERROR, "XvMC: Error, decoding "
                       "B frame and past frame is null!\n");
            else if (!render->p_future_surface)
                av_log(s->avctx, AV_LOG_ERROR, "XvMC: Error, decoding "
                       "B frame and future frame is null!\n");
            else return 0;
            return 0; // pretend things are ok even if they aren't
        case  P_TYPE:
            render->p_past_surface = findPastSurface(s, render);
            if (!render->p_past_surface)
                av_log(s->avctx, AV_LOG_ERROR, "XvMC: error, decoding "
                       "P frame and past frame is null!\n");
            else return 0;
            return 0; // pretend things are ok even if they aren't
     }

return -1;
}

void XVMC_field_end(MpegEncContext *s)
{
    if (render_state(s)->filled_mv_blocks_num > 0)
        ff_draw_horiz_band(s,0,0);
}

void XVMC_decode_mb(MpegEncContext *s)
{
    xvmc_render_state_t * render;
    XvMCMacroBlock * mv_block;
  
    setup_context(s);

    render = render_state(s);
    mv_block = macroblock(render, s->mb_x, s->mb_y, s->interlaced_dct);

    DBG_printPictureStructureChanges(render->picture_structure);

    if (s->mb_intra)
    { // Intra Block
        mv_block->macroblock_type = XVMC_MB_TYPE_INTRA;
        mv_block->motion_type = 0;
        handle_intra_block(s, mv_block, render);
    }
    else
    { // Prediction Block
        if (PICT_FRAME==render->picture_structure)
            setup_pmv_frame(s, mv_block); 
        else
            setup_pmv_field(s, mv_block);
        handle_p_b_block(s, mv_block, render);
    } // (!s->mb_intra)
    render->filled_mv_blocks_num++;
  
    assert(render->filled_mv_blocks_num <= render->total_number_of_mv_blocks);
    assert(render->next_free_data_block_num <= render->total_number_of_data_blocks);
  
    // Once all XvMC blocks filled send them to the video card.
    if (render->filled_mv_blocks_num >= render->total_number_of_mv_blocks)
        ff_draw_horiz_band(s,0,0);
}

// The remainder of this file is all helper functions

static inline xvmc_render_state_t *render_state(const MpegEncContext *s)
{
    xvmc_render_state_t *render = (xvmc_render_state_t*)s->current_picture.data[2];
    assert(render!=NULL);
    assert(render->magic==MP_XVMC_RENDER_MAGIC);
    assert(render->mv_blocks);
    return render;
}

static inline int calc_cbp(MpegEncContext *s)
{
    int cbp, cbp1;

    cbp   = (s->block_last_index[0] >= 0) ? (1 << (5)) : 0;
    cbp1  = (s->block_last_index[1] >= 0) ? (1 << (4)) : 0;
    cbp  |= (s->block_last_index[2] >= 0) ? (1 << (3)) : 0;
    cbp1 |= (s->block_last_index[3] >= 0) ? (1 << (2)) : 0;
    cbp  |= cbp1;

    if (s->flags & CODEC_FLAG_GRAY)
        return cbp; // 4 block for grayscale one done

    cbp |= (((s->block_last_index[4] >= 0) ? 1 << (1) : 0) | 
            ((s->block_last_index[5] >= 0) ? 1 << (0) : 0));

    if (s->chroma_format >= 2) {
        if (s->chroma_format == 2)
        { /* CHROMA_422 */
            cbp |= (((s->block_last_index[4] >= 0) ? 1 << (6+1) : 0) |
                    ((s->block_last_index[5] >= 0) ? 1 << (6+0) : 0));
        }
        else
        { /* CHROMA_444 */
            cbp  |= (s->block_last_index[ 6] >= 0) ? 1 << (6+5) : 0;
            cbp1  = (s->block_last_index[ 7] >= 0) ? 1 << (6+3) : 0;
            cbp  |= (s->block_last_index[ 8] >= 0) ? 1 << (6+4) : 0;
            cbp1 |= (s->block_last_index[ 9] >= 0) ? 1 << (6+2) : 0;
            cbp  |= (s->block_last_index[10] >= 0) ? 1 << (6+1) : 0;
            cbp1 |= (s->block_last_index[11] >= 0) ? 1 << (6+0) : 0;
            cbp  |= cbp1;
        }
    }
    return cbp;
}

static void set_block_pattern(const MpegEncContext *s, XvMCMacroBlock *mv_block)
{
    mv_block->coded_block_pattern = calc_cbp(s);
    if (!mv_block->coded_block_pattern)
        mv_block->macroblock_type &= ~XVMC_MB_TYPE_PATTERN;
}

static inline void setup_context(MpegEncContext *s)
{
    if (s->encoding)
    {
        av_log(s->avctx, AV_LOG_ERROR, "XVMC doesn't support encoding!!!\n");
        return -1;
    }
    // Do I need to export quant when I could not perform postprocessing?
    // Anyway, it doesn't hurt.
    s->current_picture.qscale_table[s->mb_y * s->mb_stride + s->mb_x] = s->qscale;
    s->mb_skipped = 0; //MC doesn't skip blocks
}

static inline XvMCMacroBlock *macroblock(const xvmc_render_state_t *render,
                                         int x, int y, int dct)
{
    // Take the next free macroblock
    int index = render->start_mv_blocks_num + render->filled_mv_blocks_num;
    XvMCMacroBlock *mv_block = &render->mv_blocks[index];

    mv_block->x = x;
    mv_block->y = y;
    mv_block->dct_type = dct;

    // Offset in units of 128 bytes from start of block 
    // array to this macroblocks DCT blocks begin.
    mv_block->index = render->next_free_data_block_num;
    return mv_block;
}

static inline void DBG_printPictureStructureChanges(int picture_structure)
{
#ifdef DEBUG_PICTURE_STRUCTURE_CHANGES
    static int oldframe=0xff;
    int frame = (PICT_FRAME==picture_structure)? 1 : 0;
    if (oldframe != frame)
    {
        av_log(NULL, AV_LOG_ERROR, "XvMC: picture structure %s\n",
               frame ? "FRAME" : "FIELD");
        oldframe = frame;
    }
#endif
}

// I've explicitly moved test variables outside of the loop so
// that good code is generated even with bad compilers.
static inline void pack_flip_block(const MpegEncContext *s, 
                                   xvmc_render_state_t *render,
                                   int blocks_per_mb,
                                   int pack, int flip)
{
    int i;
    if (pack)
    {
        // pack blocks only if the codec doesn't support pblocks reordering
        if (flip)
        {
            for (i=0; i<blocks_per_mb; i++)
                if (s->block_last_index[i] >= 0)
                {
                    s->pblocks[i][0]-=1<<10;
                    unsigned int indx = (render->next_free_data_block_num)*64;
                    memcpy(&render->data_blocks[indx], s->pblocks[i], BLKS_SIZE);
                    render->next_free_data_block_num++;
                }
        }
        else
        {
            for (i=0; i<blocks_per_mb; i++)
                if (s->block_last_index[i] >= 0)
                {
                    unsigned int indx = (render->next_free_data_block_num)*64;
                    memcpy(&render->data_blocks[indx], s->pblocks[i], BLKS_SIZE);
                    render->next_free_data_block_num++;
                }
        }
    }
    else
    { // nvidia fx path
        int ndbn=0;
        if (flip)
        {
            for (i=0; i<blocks_per_mb; i++)
            {
                if (s->block_last_index[i] >= 0)
                {
                    s->pblocks[i][0] -= 1<<10;
                    ndbn++;
                }
            }
        }
        else
            for (i=0; i<blocks_per_mb; i++)
                ndbn += (s->block_last_index[i] >= 0)? 1 : 0;
        render->next_free_data_block_num += ndbn;
    }
}

// Arrange the data for the card, for Motion Vector only cards
static inline void pack_flip_idct_block(const MpegEncContext *s, 
                                        xvmc_render_state_t *render,
                                        int blocks_per_mb,
                                        int pack, int flip)
{
    int i;

    if (flip) {
        for (i=0; i<blocks_per_mb; i++)
        {
            if (s->block_last_index[i] >= 0)
            {
                s->pblocks[i][0] -= 1<<10;
                s->dsp.idct(s->pblocks[i]); //!!TODO!clip!!!
                if (pack)
                { // codec doesn't support pblocks reordering...
                    unsigned int indx = (render->next_free_data_block_num)*64;
                    memcpy(&render->data_blocks[indx], s->pblocks[i], BLKS_SIZE);
                }
                render->next_free_data_block_num++;
            }
        }
    }
    else
    {
        for (i=0; i<blocks_per_mb; i++)
        {
            if (s->block_last_index[i] >= 0)
            {
                s->dsp.idct(s->pblocks[i]); //!!TODO!clip!!!
                if (pack)
                { // codec doesn't support pblocks reordering...
                    unsigned int indx = (render->next_free_data_block_num)*64;
                    memcpy(&render->data_blocks[indx], s->pblocks[i], BLKS_SIZE);
                }
                render->next_free_data_block_num++;
            }
        }
    }
}

static inline void clear_chroma(MpegEncContext *s, 
                                int blocks_per_mb,
                                int flip)
{
    int i;
    // Intra frames always have a full chroma block, so we need to clear them.
    if (flip)
        for (i=4; i<blocks_per_mb; i++)
        {
            memset(s->pblocks[i], 0, BLKS_SIZE);
            s->pblocks[i][0] = 1<<10;
        }
    else
        for (i=4; i<blocks_per_mb; i++)
            memset(s->pblocks[i], 0, BLKS_SIZE);
}

static inline void handle_intra_block(const MpegEncContext *s, 
                                      const XvMCMacroBlock *mv_block,
                                      xvmc_render_state_t *render)
{
    const int pack = (1 == s->avctx->xvmc_acceleration);
    const int flip = !render->unsigned_intra;
    int cb = chromaBits[s->chroma_format];

    if (s->flags & CODEC_FLAG_GRAY)
        clear_chroma(s, (cb = 6), !flip);

    set_block_pattern(s, mv_block);

    if (render->idct) 
        pack_flip_block(s, render, cb, pack, TRUE/*flip*/);
    else
        pack_flip_idct_block(s, render, cb, pack, flip);
}

static inline void handle_p_b_block(const MpegEncContext *s, 
                                    const XvMCMacroBlock *mv_block,
                                    xvmc_render_state_t *render)
{
    const int pack = (1 == s->avctx->xvmc_acceleration);
    int cb = (s->flags & CODEC_FLAG_GRAY)? 4 : chromaBits[s->chroma_format];

    set_block_pattern(s, mv_block);

    if (render->idct) 
        pack_flip_block(s, render, cb, pack, FALSE/*flip*/);
    else
        pack_flip_idct_block(s, render, cb, pack, FALSE/*flip*/);
}

static void setup_pmv(MpegEncContext *s, XvMCMacroBlock *mv_block)
{
    mv_block->macroblock_type = XVMC_MB_TYPE_PATTERN;
    mv_block->macroblock_type |= 
        (s->mv_dir & MV_DIR_FORWARD) ? XVMC_MB_TYPE_MOTION_FORWARD   : 0;
    mv_block->macroblock_type |= 
        (s->mv_dir & MV_DIR_BACKWARD) ? XVMC_MB_TYPE_MOTION_BACKWARD : 0;

    //pmv[horz|vert][forward|back][1st vec|2nd vec]=mv[f|b][h|v|?|?][f|s]
    mv_block->PMV[0][0][0] = s->mv[0][0][0]; //fw
    mv_block->PMV[0][0][1] = s->mv[0][0][1]; //fw
    mv_block->PMV[0][1][0] = s->mv[1][0][0]; //BW
    mv_block->PMV[0][1][1] = s->mv[1][0][1]; //BW
    mv_block->PMV[1][0][0] = s->mv[0][1][0]; //fw
    mv_block->PMV[1][0][1] = s->mv[0][1][1]; //fw
    mv_block->PMV[1][1][0] = s->mv[1][1][0]; //BW
    mv_block->PMV[1][1][1] = s->mv[1][1][1]; //BW 
}

static void setup_pmv_frame(MpegEncContext *s, 
                            XvMCMacroBlock *mv_block)
{
    assert(s->mv_type>=0 && s->mv_type<=MV_TYPE_DMV);
    mv_block->motion_type = mv_ffmpeg_to_xvmc[s->mv_type];
    // Update DC predictors for P macroblocks
    s->last_dc[0] = s->last_dc[1] = s->last_dc[2] = 
        128 << s->intra_dc_precision;
  
    mv_block->macroblock_type = XVMC_MB_TYPE_PATTERN;
    mv_block->macroblock_type |= 
        (s->mv_dir & MV_DIR_FORWARD) ? XVMC_MB_TYPE_MOTION_FORWARD   : 0;
    mv_block->macroblock_type |= 
        (s->mv_dir & MV_DIR_BACKWARD) ? XVMC_MB_TYPE_MOTION_BACKWARD : 0;

    if (MV_TYPE_FIELD<=s->mv_type)
    {
        if (MV_TYPE_FIELD==s->mv_type)
        {
            //pmv[horz|vert][forward|back][1st vec|2nd vec]=mv[f|b][h|v|?|?][f|s]
            int mvfs0 = (s->field_select[0][0]) ? XVMC_SELECT_FIRST_FORWARD   : 0;
            mv_block->PMV[0][0][0] = s->mv[0][0][0];
            mv_block->PMV[0][0][1] = s->mv[0][0][1]<<1;
            int mvfs1 = (s->field_select[0][1]) ? XVMC_SELECT_SECOND_FORWARD  : 0;
            mv_block->PMV[0][1][0] = s->mv[1][0][0];
            mv_block->PMV[0][1][1] = s->mv[1][0][1]<<1;
            mvfs0    |= (s->field_select[1][0]) ? XVMC_SELECT_FIRST_BACKWARD  : 0;
            mv_block->PMV[1][0][0] = s->mv[0][1][0];
            mv_block->PMV[1][0][1] = s->mv[0][1][1]<<1;
            mvfs1    |= (s->field_select[1][1]) ? XVMC_SELECT_SECOND_BACKWARD : 0;
            mv_block->PMV[1][1][0] = s->mv[1][1][0];
            mv_block->PMV[1][1][1] = s->mv[1][1][1]<<1;
            mv_block->motion_vertical_field_select = mvfs0 | mvfs1;
        }
        else if (MV_TYPE_DMV==s->mv_type)
        {
            mv_block->PMV[0][0][0] = s->mv[0][0][0]; // top    from top
            mv_block->PMV[0][0][1] = s->mv[0][0][1]<<1;
            mv_block->PMV[0][1][0] = s->mv[0][0][0]; // bottom from bottom
            mv_block->PMV[0][1][1] = s->mv[0][0][1]<<1;
            mv_block->PMV[1][0][0] = s->mv[0][2][0]; // top    from bottom
            mv_block->PMV[1][0][1] = s->mv[0][2][1]<<1;
            mv_block->PMV[1][1][0] = s->mv[0][3][0]; // bottom from top
            mv_block->PMV[1][1][1] = s->mv[0][3][1]<<1;
        }
        else
            av_log(s->avctx, AV_LOG_ERROR, "XvMC: Unexpected movement type\n");
    }
    else
    {
        // Just in case set all basic vectors
        // and set correct field referenses.
        setup_pmv(s, mv_block);
    }

    // Error conditions
    if (MV_TYPE_16X8 == s->mv_type)
    {
        av_log(s->avctx, AV_LOG_ERROR,
               "XvMC: Field motion vector type with frame picture!\n");
        if (s->mv_type == MV_TYPE_16X8)
        {
            int mvfs0 = (s->field_select[0][0]) ? XVMC_SELECT_FIRST_FORWARD   : 0;
            int mvfs1 = (s->field_select[0][1]) ? XVMC_SELECT_SECOND_FORWARD  : 0;
            mvfs0    |= (s->field_select[1][0]) ? XVMC_SELECT_FIRST_BACKWARD  : 0;
            mvfs1    |= (s->field_select[1][1]) ? XVMC_SELECT_SECOND_BACKWARD : 0;
            mv_block->motion_vertical_field_select = mvfs0 | mvfs1;
        }
    }
    if (-1 == mv_block->motion_type)
    {
        if (MV_TYPE_8X8==s->mv_type)
            av_log(s->avctx, AV_LOG_ERROR,
                   "XvMC: H263/MPEG4 motion vectortype in MPEG2 decoder!\n");
        else
            av_log(s->avctx, AV_LOG_ERROR,
                   "XvMC: Unknown motion vector type!\n");
        mv_block->motion_type=XVMC_PREDICTION_FRAME;
    }
}

static void setup_pmv_field(MpegEncContext *s,
                            XvMCMacroBlock *mv_block)
{
    assert(s->mv_type>=0 && s->mv_type<=MV_TYPE_DMV);
    mv_block->motion_type = mv_ffmpeg_to_xvmc[s->mv_type];

    // Update DC predictors for P macroblocks
    s->last_dc[0] = s->last_dc[1] = s->last_dc[2] = 
        128 << s->intra_dc_precision;
    setup_pmv(s, mv_block);

    if (MV_TYPE_DMV==s->mv_type)
    {
        // I don't have a stream that tests this, so... -- dtk
        //pmv[horz|vert][forward|back][1st vec|2nd vec]=mv[f|b][h|v|?|?][f|s]
        mv_block->PMV[0][1][0] = s->mv[0][2][0]; //dmv00
        mv_block->PMV[0][1][1] = s->mv[0][2][1]; //dmv01
    }
    else
    {
        // Set correct field referenses
        if (s->mv_type == MV_TYPE_FIELD || s->mv_type == MV_TYPE_16X8)
        {
            int mvfs0 = (s->field_select[0][0]) ? XVMC_SELECT_FIRST_FORWARD   : 0;
            int mvfs1 = (s->field_select[0][1]) ? XVMC_SELECT_SECOND_FORWARD  : 0;
            mvfs0    |= (s->field_select[1][0]) ? XVMC_SELECT_FIRST_BACKWARD  : 0;
            mvfs1    |= (s->field_select[1][1]) ? XVMC_SELECT_SECOND_BACKWARD : 0;
            mv_block->motion_vertical_field_select = mvfs0 | mvfs1;
        }
    }

    // Handle error condition
    if (-1==mv_block->motion_type)
    {
        if (MV_TYPE_8X8==s->mv_type) 
            av_log(s->avctx, AV_LOG_ERROR,
                   "XvMC: H263/MPEG4 motion vector type in MPEG2 decoder!\n");
        else
            av_log(s->avctx, AV_LOG_ERROR,
                   "XvMC: Unknown motion vector type!\n");
        mv_block->motion_type = XVMC_PREDICTION_FRAME;
    }
}

static const int mv_ffmpeg_to_xvmc[8] =
{
    XVMC_PREDICTION_FRAME,      /* MV_TYPE_16X16 (1 vector)                        */
    -1,                         /* MV_TYPE_8X8   (4 vectors, h263, MPEG4 4MV)      */
    XVMC_PREDICTION_16x8,       /* MV_TYPE_16X8  (2 vectors, one per 16x8 block)   */
    XVMC_PREDICTION_FIELD,      /* MV_TYPE_FIELD (2 vectors, one per field)        */
    XVMC_PREDICTION_DUAL_PRIME, /* MV_TYPE_DMV   (2 vec, MPEG2 Dual Prime Vectors) */
    -1,
    -1,
    -1
};

static const char* mv_ffmpeg_to_string[8] =
{
    "XVMC_PREDICTION_FRAME",      /* MV_TYPE_16X16 (1 vector)                        */
    "MV_TYPE_8x8 -- unsupported", /* MV_TYPE_8X8   (4 vectors, h263, MPEG4 4MV)      */
    "XVMC_PREDICTION_16x8",       /* MV_TYPE_16X8  (2 vectors, one per 16x8 block)   */
    "XVMC_PREDICTION_FIELD",      /* MV_TYPE_FIELD (2 vectors, one per field)        */
    "XVMC_PREDICTION_DUAL_PRIME", /* MV_TYPE_DMV   (2 vec, MPEG2 Dual Prime Vectors) */
    "MV_??? 5 unknown",
    "MV_??? 6 unknown",
    "MV_??? 7 unknown"
};

#endif
