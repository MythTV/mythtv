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

    slicestate->progressive_sequence = s->progressive_sequence;
    slicestate->top_field_first = s->top_field_first;

    VIAMPGSurface.dwPictureType = s->pict_type;
    VIAMPGSurface.dwDecodePictStruct = s->picture_structure;
    VIAMPGSurface.dwDecodeBuffIndex = slicestate->image_number;

    int i,j;

    for (i=0, j=0; j < 16; i += 4, j += 2)
    {
        VIAMPGSurface.dwQMatrix[0][j+1] =
          (s->intra_matrix[ s->intra_scantable.permutated[i+0] ] >> 8) << 0 |
          (s->intra_matrix[ s->intra_scantable.permutated[i+1] ] >> 8) << 8 |
          (s->intra_matrix[ s->intra_scantable.permutated[i+2] ] >> 8) << 16 |
          (s->intra_matrix[ s->intra_scantable.permutated[i+3] ] >> 8) << 24;

        VIAMPGSurface.dwQMatrix[0][j+0] =
          (s->intra_matrix[ s->intra_scantable.permutated[i+0] ] & 0xff) << 0 |
          (s->intra_matrix[ s->intra_scantable.permutated[i+1] ] & 0xff) << 8 |
          (s->intra_matrix[ s->intra_scantable.permutated[i+2] ] & 0xff) << 16 |
          (s->intra_matrix[ s->intra_scantable.permutated[i+3] ] & 0xff) << 24;
    }

    for (i = 0, j = 0; j < 16; i += 4, j += 2)
    {
        VIAMPGSurface.dwQMatrix[1][j+1] =
          (s->inter_matrix[ s->inter_scantable.permutated[i+0] ] >> 8) << 0 |
          (s->inter_matrix[ s->inter_scantable.permutated[i+1] ] >> 8) << 8 |
          (s->inter_matrix[ s->inter_scantable.permutated[i+2] ] >> 8) << 16 |
          (s->inter_matrix[ s->inter_scantable.permutated[i+3] ] >> 8) << 24;

        VIAMPGSurface.dwQMatrix[1][j+0] =
          (s->inter_matrix[ s->inter_scantable.permutated[i+0] ] & 0xff) << 0 |
          (s->inter_matrix[ s->inter_scantable.permutated[i+1] ] & 0xff) << 8 |
          (s->inter_matrix[ s->inter_scantable.permutated[i+2] ] & 0xff) << 16 |
          (s->inter_matrix[ s->inter_scantable.permutated[i+3] ] & 0xff) << 24;
    }

    VIAMPGSurface.dwQMatrixChanged = 1;

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
    VIAMPGSurface.dwSecondField = s->first_field?0:1;

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

    VIAMPGSurface.dwMPEGProgressiveMode = s->progressive_sequence?
                                          VIA_PROGRESSIVE:VIA_NON_PROGRESSIVE;

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

    buf_ptr = pbuf_ptr;
    while (buf_ptr < pbuf_ptr + buf_size) {
       v = *buf_ptr++;
       if (state == 0x000001) {
            state = ((state << 8) | v) & 0xffffff;
            goto found;
        }
        state = ((state << 8) | v) & 0xffffff;
    }
    return -1;
 found:
    return buf_ptr - pbuf_ptr;
}

#define SLICE_MIN_START_CODE   0x00000101
#define SLICE_MAX_START_CODE   0x000001af
int VIA_decode_slice(MpegEncContext *s, int mb_y, uint8_t *buffer,
                     int buf_size)
{
    int slicelen = length_to_next_start(buffer, buf_size) - 4;
    if (slicelen < 0)
    {
        if (mb_y == s->mb_height - 1)
            slicelen = buf_size;
        else
            return -1;
    }

    via_slice_state_t *slicestate;
    slicestate = (via_slice_state_t *)s->current_picture.data[0];
    slicestate->slice_data = buffer;
    slicestate->slice_datalen = slicelen;
    slicestate->code = SLICE_MIN_START_CODE + mb_y;
    slicestate->maxcode = SLICE_MIN_START_CODE + s->mb_height - 1;

    if (slicestate->code == slicestate->maxcode)
        slicestate->slice_datalen += 4;

    ff_draw_horiz_band(s, 0, 0);

    return slicelen;
}

