//avcodec include
#include "avcodec.h"
#include "dsputil.h"
#include "mpegvideo.h"

#include "via_mpegsdk.h"
#include "viaslice.h"

int VIA_field_start(MpegEncContext*s, AVCodecContext *avctx)
{
    via_slice_state_t *slicestate;
    VIAMPGSURFACE VIAMPGSurface;

    slicestate = (via_slice_state_t *)s->current_picture.data[0];
    assert(slicestate != NULL);

    VIAMPGSurface.dwPictureType = s->pict_type;
    VIAMPGSurface.dwDecodePictStruct = s->picture_structure;
    VIAMPGSurface.dwDecodeBuffIndex = slicestate->image_number;

    int i,j;

    VIAMPGSurface.dwQMatrixChanged = 1;

    for(i=0,j=0;i<64;i+=4,j++){
        VIAMPGSurface.dwQMatrix[0][j] =
             (s->intra_matrix[i] & 0xff) |
             ((s->intra_matrix[i+1] & 0xff) << 8)|
             ((s->intra_matrix[i+2] & 0xff) << 16)|
             ((s->intra_matrix[i+3] & 0xff) << 24);
    }

    for(i=0,j=0;i<64;i+=4,j++){
        VIAMPGSurface.dwQMatrix[1][j] =
            (s->inter_matrix[i] & 0xff) |
            ((s->inter_matrix[i+1] & 0xff) << 8)|
            ((s->inter_matrix[i+2] & 0xff) << 16)|
            ((s->inter_matrix[i+3] & 0xff) << 24);
    }

    s->mb_width = (s->width + 15) / 16;
    s->mb_height = (s->codec_id == CODEC_ID_MPEG2VIDEO && 
                    !s->progressive_sequence) ?
                      2 * ((s->height + 31) / 32) : (s->height + 15) / 16;

    VIAMPGSurface.dwAlternateScan = s->alternate_scan;
    VIAMPGSurface.dwMBwidth = s->width >> 4;

    if (s->codec_id == CODEC_ID_MPEG2VIDEO)
        VIAMPGSurface.dwMpeg2 = 1;
    else
        VIAMPGSurface.dwMpeg2 = 0;

    VIAMPGSurface.dwTopFirst = s->top_field_first;
    VIAMPGSurface.dwFramePredDct = s->frame_pred_frame_dct;
    VIAMPGSurface.dwMBAmax = s->mb_width * s->mb_height;
    VIAMPGSurface.dwIntravlc = s->intra_vlc_format;
    VIAMPGSurface.dwDcPrec = s->intra_dc_precision;
    VIAMPGSurface.dwQscaleType = s->q_scale_type;
    VIAMPGSurface.dwConcealMV = s->concealment_motion_vectors;

    // XXX: not sure on this
    VIAMPGSurface.dwSecondField = s->first_field;

    // FIXME: fill in from last, next picture_ptr
    VIAMPGSurface.dwOldRefFrame = 0;
    VIAMPGSurface.dwRefFrame =  0;

    switch (s->pict_type){
        case B_TYPE:
            slicestate = (via_slice_state_t *)s->next_picture.data[0];
            assert(slicestate);
            VIAMPGSurface.dwRefFrame = slicestate->image_number;
        case P_TYPE:
            slicestate = (via_slice_state_t *)s->last_picture.data[0];
            assert(slicestate);
            VIAMPGSurface.dwOldRefFrame = slicestate->image_number;
            break;
        default:
            break;
    }
    
    VIAMPGSurface.FVMVRange = s->mpeg_f_code[0][1] - 1;
    VIAMPGSurface.FHMVRange = s->mpeg_f_code[0][0] - 1;
    VIAMPGSurface.BVMVRange = s->mpeg_f_code[1][1] - 1;
    VIAMPGSurface.BHMVRange = s->mpeg_f_code[1][0] - 1;

    VIABeginPicture(&VIAMPGSurface);

    return 0;
}

void VIA_field_end(MpegEncContext *s)
{
    s->error_count = 0;
}

static int length_to_next_start(uint8_t *pbuf_ptr, int buf_size)
{
    uint8_t *buf_ptr;
    unsigned int state = 0xFFFFFFFF, v;
    int val;

    buf_ptr = pbuf_ptr;
    while (buf_ptr < pbuf_ptr + buf_size) {
       v = *buf_ptr++;
       if (state == 0x000001) {
            state = ((state << 8) | v) & 0xffffff;
            val = state;
            goto found;
        }
        state = ((state << 8) | v) & 0xffffff;
    }
    val = -1;
 found:
    return buf_ptr - pbuf_ptr;
}

int VIA_decode_slice(MpegEncContext *s, int start_code, uint8_t *buffer, 
                     int buf_size)
{
    via_slice_state_t  *slicestate;
    int slicelen = length_to_next_start(buffer, buf_size) - 4;

    slicestate = (via_slice_state_t *)s->current_picture.data[0];
    slicestate->slice_data = buffer;
    slicestate->slice_datalen = slicelen;
    slicestate->code = start_code;
    slicestate->maxcode = s->mb_height;

    if (start_code == s->mb_height)
        slicestate->slice_datalen += 4;

    ff_draw_horiz_band(s, 0, 0);

    return slicelen;
}

