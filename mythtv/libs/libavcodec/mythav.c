#include "mythav.h"
#include "dsputil.h"
#include "mpegvideo.h"

void mythav_set_last_picture(AVCodecContext *avctx, UINT8 *buf,
                             int width, int height)
{
    switch (avctx->codec_id)
    {
        case CODEC_ID_MPEG4:
        case CODEC_ID_H263:
        case CODEC_ID_MSMPEG4V1:
        case CODEC_ID_MSMPEG4V2:
        case CODEC_ID_MSMPEG4V3:
        case CODEC_ID_WMV1:
        case CODEC_ID_WMV2:
        case CODEC_ID_H263I:
        {
            MpegEncContext *s = avctx->priv_data;
            s->next_picture[0] = buf;
            s->next_picture[1] = buf + width * height;
            s->next_picture[2] = s->next_picture[1] + width * height / 4;
            break;
        }
        default: break;
    }
}
