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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
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

//#include "xvmc_debug.h"

#undef fprintf

static inline xvmc_render_state_t *render_state(const MpegEncContext *s);

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
//        printf("s->pblocks[%d]=%p ,s->block=%p cbp=%d\n",i,s->pblocks[i],s->block,cbp);
    }
}

XvMCSurface* findPastSurface(MpegEncContext *s, xvmc_render_state_t *render) {
    Picture *lastp = s->last_picture_ptr;
    xvmc_render_state_t *last = NULL;
    
    if (NULL!=lastp) {
        last = (xvmc_render_state_t*)(lastp->data[2]);
        if (B_TYPE==last->pict_type)
            fprintf(stderr, "Past frame is a B frame in findPastSurface, this is bad.\n");
        //assert(B_TYPE!=last->pict_type);
    }

    if (NULL==last) 
        if (!s->first_field)
            last = render; // predict second field from the first
        else
            return 0;
    
    if (last->magic != MP_XVMC_RENDER_MAGIC) 
        return 0;

    return (last->state & MP_XVMC_STATE_PREDICTION) ? last->p_surface : 0;
}

XvMCSurface* findFutureSurface(MpegEncContext *s) {
    Picture *nextp = s->next_picture_ptr;
    xvmc_render_state_t *next = NULL;
    
    if (NULL!=nextp) {
        next = (xvmc_render_state_t*)(nextp->data[2]);
        if (B_TYPE==next->pict_type)
            fprintf(stderr, "Next frame is a B frame in findFutureSurface, this is bad.\n");
        //assert(B_TYPE!=next->pict_type);
    }

    assert(NULL!=next);

    if (next->magic != MP_XVMC_RENDER_MAGIC)
        return 0;

    return (next->state & MP_XVMC_STATE_PREDICTION) ? next->p_surface : 0;
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
                fprintf(stderr, "error: decoding B frame and past frame is null!");
            else if (!render->p_future_surface)
                fprintf(stderr, "error: decoding B frame and future frame is null!");
            else return 0;
            return 0; // pretend things are ok even if they aren't
        case  P_TYPE:
            render->p_past_surface = findPastSurface(s, render);
            if (!render->p_past_surface)
                fprintf(stderr, "error: decoding P frame and past frame is null!");
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

void XVMC_decode_mb(MpegEncContext *s){
XvMCMacroBlock * mv_block;
xvmc_render_state_t * render;
int i,cbp,blocks_per_mb;

const int mb_xy = s->mb_y * s->mb_stride + s->mb_x;


    if(s->encoding){
        av_log(s->avctx, AV_LOG_ERROR, "XVMC doesn't support encoding!!!\n");
	av_abort();
    }

   //from MPV_decode_mb(),
    /* update DC predictors for P macroblocks */
    if (!s->mb_intra) {
        s->last_dc[0] =
        s->last_dc[1] =
        s->last_dc[2] =  128 << s->intra_dc_precision;
    }

   //MC doesn't skip blocks
    s->mb_skiped = 0;


   // do I need to export quant when I could not perform postprocessing?
   // anyway, it doesn't hurrt
    s->current_picture.qscale_table[mb_xy] = s->qscale;

//START OF XVMC specific code
    render = render_state(s);

    //take the next free macroblock
    mv_block = &render->mv_blocks[render->start_mv_blocks_num + 
                                   render->filled_mv_blocks_num ];

// memset(mv_block,0,sizeof(XvMCMacroBlock));

    mv_block->x = s->mb_x;
    mv_block->y = s->mb_y;
    if (0) {
        static unsigned char orig=0xff;
        unsigned char newc = s->picture_structure;
        if (orig!=newc)
            fprintf(stderr, "XVMC_DCT_TYPE_FRAME/FIELD: %i\n", (int) newc);
        orig=newc;
    }

    int frametype = (PICT_FRAME==s->picture_structure) ? XVMC_DCT_TYPE_FRAME : XVMC_DCT_TYPE_FIELD;
    mv_block->dct_type = s->interlaced_dct;
    /*
      Quote from XvMC standard:
    dct_type -  This field indicates whether frame pictures are frame DCT
                coded or field DCT coded. ie XVMC_DCT_TYPE_FIELD or
                XVMC_DCT_TYPE_FRAME.
    */

    mv_block->macroblock_type = XVMC_MB_TYPE_INTRA;
    if (!s->mb_intra) {
        mv_block->macroblock_type = XVMC_MB_TYPE_PATTERN;
        if (s->mv_dir & MV_DIR_FORWARD) {
            mv_block->macroblock_type|= XVMC_MB_TYPE_MOTION_FORWARD;
            //pmv[n][dir][xy]=mv[dir][n][xy]
            //PMV[hozontal|vertical][forward|backward][first|second vector]
            mv_block->PMV[0][0][0] = s->mv[0][0][0];
            mv_block->PMV[0][0][1] = s->mv[0][0][1];
            mv_block->PMV[1][0][0] = s->mv[0][1][0];
            mv_block->PMV[1][0][1] = s->mv[0][1][1];
        }
        if(s->mv_dir & MV_DIR_BACKWARD){
            mv_block->macroblock_type|=XVMC_MB_TYPE_MOTION_BACKWARD;
            mv_block->PMV[0][1][0] = s->mv[1][0][0];
            mv_block->PMV[0][1][1] = s->mv[1][0][1];
            mv_block->PMV[1][1][0] = s->mv[1][1][0];
            mv_block->PMV[1][1][1] = s->mv[1][1][1];
        }

        int mvfs=0;
        //set correct field referenses
        if(s->mv_type == MV_TYPE_FIELD || s->mv_type == MV_TYPE_16X8){
            if (s->field_select[0][0]) mvfs |= XVMC_SELECT_FIRST_FORWARD;
            if (s->field_select[1][0]) mvfs |= XVMC_SELECT_FIRST_BACKWARD;
            if (s->field_select[0][1]) mvfs |= XVMC_SELECT_SECOND_FORWARD;
            if (s->field_select[1][1]) mvfs |= XVMC_SELECT_SECOND_BACKWARD;
        }
        mv_block->motion_vertical_field_select = mvfs;
    } //!intra
    
    int mt = XVMC_PREDICTION_FRAME;
    if (XVMC_DCT_TYPE_FRAME==frametype) {
        switch (s->mv_type) {
        case MV_TYPE_16X16: mt=XVMC_PREDICTION_FRAME;      break;
        case MV_TYPE_FIELD: mt=XVMC_PREDICTION_FIELD; 
            mv_block->PMV[0][0][1]<<=1;
            mv_block->PMV[1][0][1]<<=1;
            mv_block->PMV[0][1][1]<<=1;
            mv_block->PMV[1][1][1]<<=1;
            break;
        case MV_TYPE_DMV:   mt=XVMC_PREDICTION_DUAL_PRIME;
            mv_block->PMV[0][0][0] = s->mv[0][0][0]; // top    from top
            mv_block->PMV[0][1][0] = s->mv[0][0][0]; // bottom from bottom
            mv_block->PMV[1][0][0] = s->mv[0][2][0]; // top    from bottom
            mv_block->PMV[1][1][0] = s->mv[0][3][0]; // bottom from top
            mv_block->PMV[0][0][1] = s->mv[0][0][1]<<1;
            mv_block->PMV[0][1][1] = s->mv[0][0][1]<<1;
            mv_block->PMV[1][0][1] = s->mv[0][2][1]<<1;
            mv_block->PMV[1][1][1] = s->mv[0][3][1]<<1;
            break;
        case MV_TYPE_16X8:  fprintf(stderr, "XVMC: Field motion vector type with frame picture!\n");
            break;
        case MV_TYPE_8X8:   fprintf(stderr, "XVMC: H263/MPEG4 motion vector type in MPEG2 decoder!\n"); 
            break;
        default:            fprintf(stderr, "XVMC: Unknown motion vector type!\n");
        }
    } else {
        switch (s->mv_type) {
        case MV_TYPE_16X8:  mt=XVMC_PREDICTION_16x8;       break;
        case MV_TYPE_FIELD: mt=XVMC_PREDICTION_FIELD;      break;
        case MV_TYPE_DMV:   mt=XVMC_PREDICTION_DUAL_PRIME;
            //pmv[n][dir][xy]=mv[dir][n][xy]
            //PMV[hozontal|vertical][forward|backward][first|second vector]
            mv_block->PMV[0][1][0] = s->mv[0][2][0];//dmv00
            mv_block->PMV[0][1][1] = s->mv[0][2][1];//dmv01
            break;
        case MV_TYPE_16X16: fprintf(stderr, "XVMC: Frame motion vector type with field picture!\n");
            break;
        case MV_TYPE_8X8:   fprintf(stderr, "XVMC: H263/MPEG4 motion vector type in MPEG2 decoder!\n"); 
            break;
        default:            fprintf(stderr, "XVMC: Unknown motion vector type!\n");
        }
    }
    mv_block->motion_type = mt;


    // Handle data blocks


    // Offset in units of 128 bytes from start of block array to this macroblocks DCT blocks begin
    mv_block->index = render->next_free_data_block_num;

    if (s->flags & CODEC_FLAG_GRAY) {
        if (s->mb_intra) { //intra frames are always full chroma block
            blocks_per_mb = 6;
            for (i=4; i<blocks_per_mb; i++) {
                memset(s->pblocks[i],0,sizeof(short)*8*8); //so we need to clear them
                if(!render->unsigned_intra)
                    s->pblocks[i][0] = 1<<10;
            }
        } else blocks_per_mb = 4; //Luminance blocks only
    } else {
        blocks_per_mb = 6; // 4 luminance + 2 color blocks in macroblock
        if( s->chroma_format >= 2){
           blocks_per_mb = 4 + (1 << (s->chroma_format));
        }
    }

    if ((mv_block->macroblock_type & XVMC_MB_TYPE_PATTERN) || 
        (mv_block->macroblock_type & XVMC_MB_TYPE_INTRA)) {

        // calculate cbp
        cbp = 0;
        for(i=0; i<blocks_per_mb; i++) {
            cbp+= cbp;
            if (s->block_last_index[i] >= 0)
                cbp++;
        }

        if (s->flags & CODEC_FLAG_GRAY && !s->mb_intra)
            cbp &= 0xf << (blocks_per_mb - 4);

        mv_block->coded_block_pattern = cbp;

        // ????
        if (0==cbp) mv_block->macroblock_type &= ~XVMC_MB_TYPE_PATTERN;
    }

    for (i=0; i<blocks_per_mb; i++){
        if (s->block_last_index[i] >= 0){
            // i do not have unsigned_intra MOCO to test, hope it is OK
            if ( (s->mb_intra) && (render->idct || (!render->idct && !render->unsigned_intra)) )
                s->pblocks[i][0]-=1<<10;
            if(!render->idct){
                s->dsp.idct(s->pblocks[i]);
                //!!TODO!clip!!!
            }
//copy blocks only if the codec doesn't support pblocks reordering
            if(s->avctx->xvmc_acceleration == 1){
                memcpy(&render->data_blocks[(render->next_free_data_block_num)*64],
                        s->pblocks[i],sizeof(short)*8*8);
            }else{
/*              if(s->pblocks[i] != &render->data_blocks[
                        (render->next_free_data_block_num)*64]){
                   printf("ERROR mb(%d,%d) s->pblocks[i]=%p data_block[]=%p\n",
                   s->mb_x,s->mb_y, s->pblocks[i], 
                   &render->data_blocks[(render->next_free_data_block_num)*64]);
                }*/
            }
            render->next_free_data_block_num++;
        }
    }
    render->filled_mv_blocks_num++;

    assert(render->filled_mv_blocks_num <= render->total_number_of_mv_blocks);
    assert(render->next_free_data_block_num <= render->total_number_of_data_blocks);


    if(render->filled_mv_blocks_num >= render->total_number_of_mv_blocks)
        ff_draw_horiz_band(s,0,0);

// DumpRenderInfo(render);
// DumpMBlockInfo(mv_block);

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

#endif
