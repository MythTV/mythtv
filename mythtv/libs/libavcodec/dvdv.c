/*
 * dvdv.c - Mac Hardware MPEG2 decoding.
 *
 * Based on modifications to ffmpeg's libavcodec/mpeg12.c by John Dagliesh.
 * See http://www.defyne.org/dvb/accellent.html
 *
 * Added to the MythTV project's copy of libavcodec by Nigel Pearson.
 */

#include "avcodec.h"
#include "mpegvideo.h"
#include "dvdv.h"

void DVDV_init_block(MpegEncContext *s)
{
    DVDV_MBInfo * mb = DVDV_MB(s);
    int           ret;
 
    for (ret=0; ret<6; ret++)
        mb->num_coded_elts[ret] = 0;
}

void DVDV_decode_mb(MpegEncContext *s)
{
    DVDV_MBInfo * mb = DVDV_MB(s);

    mb->mb_type = 0;
    if (!s->mb_intra)
    {
        if (s->mv_dir & MV_DIR_FORWARD)
        {
            mb->mb_type |= 1;
            mb->frwd_mv[0] = s->mv[0][0][0];
            mb->frwd_mv[1] = s->mv[0][0][1];
            mb->frwd_mv_field[0] = s->mv[0][1][0];
            mb->frwd_mv_field[1] = s->mv[0][1][1];
        }
        if (s->mv_dir & MV_DIR_BACKWARD)
        {
            mb->mb_type |= 2;
            mb->back_mv[0] = s->mv[1][0][0];
            mb->back_mv[1] = s->mv[1][0][1];
            mb->back_mv_field[0] = s->mv[1][1][0];
            mb->back_mv_field[1] = s->mv[1][1][1];
        }
        if (s->mv_type != MV_TYPE_16X16)
        {
            mb->mb_type |= 4;
            mb->field_select[0] = s->field_select[0][0];
            mb->field_select[1] = s->field_select[1][0];
            mb->field_select[2] = s->field_select[0][1];
            mb->field_select[3] = s->field_select[1][1];
        }
    }
    mb->ildct = s->interlaced_dct;
    DVDV_MB(s)++;

    // side effects of MPV_decode_mb:
    if (!s->mb_intra)
    {
        s->last_dc[0] =
        s->last_dc[1] =
        s->last_dc[2] =  128 << s->intra_dc_precision;
    }
    s->mb_skipped = 0;
}
