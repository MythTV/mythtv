#include "privatedecoder_mpeg2.h"
#include "mythverbose.h"

#define LOC QString("MPEG2Dec: ")

void PrivateDecoderMPEG2::GetDecoders(render_opts &opts)
{
    opts.decoders->append("libmpeg2");
    (*opts.equiv_decoders)["libmpeg2"].append("nuppel");
    (*opts.equiv_decoders)["libmpeg2"].append("ffmpeg");
    (*opts.equiv_decoders)["libmpeg2"].append("dummy");
}

PrivateDecoderMPEG2::PrivateDecoderMPEG2() : PrivateDecoder(), mpeg2dec(NULL)
{
}

PrivateDecoderMPEG2::~PrivateDecoderMPEG2()
{
    if (mpeg2dec)
        mpeg2_close(mpeg2dec);
    mpeg2dec = NULL;
    ClearFrames();
}

bool PrivateDecoderMPEG2::Init(const QString &decoder,
                               bool no_hardware_decode,
                               AVCodecContext *avctx)
{
    if (!((decoder == "libmpeg2") &&
        (CODEC_IS_MPEG(avctx->codec_id))))
        return false;

    if (mpeg2dec)
        mpeg2_close(mpeg2dec);
    ClearFrames();
    mpeg2dec = mpeg2_init();
    if (mpeg2dec)
    {
        VERBOSE(VB_PLAYBACK, LOC + "Using libmpeg2 for video decoding");
        return true;
    }
    return false;
}

bool PrivateDecoderMPEG2::Reset(void)
{
    if (mpeg2dec)
        mpeg2_reset(mpeg2dec, 0);
    ClearFrames();
    return true;
}

int  PrivateDecoderMPEG2::GetFrame(AVStream *stream,
                                   AVFrame *picture,
                                   int *got_picture_ptr,
                                   AVPacket *pkt)
{
    AVCodecContext *avctx = stream->codec;
    *got_picture_ptr = 0;
    const mpeg2_info_t *info = mpeg2_info(mpeg2dec);
    mpeg2_buffer(mpeg2dec, pkt->data, pkt->data + pkt->size);
    while (1)
    {
        switch (mpeg2_parse(mpeg2dec))
        {
            case STATE_SEQUENCE:
                // libmpeg2 needs three buffers to do its work.
                // We set up two prediction buffers here, from
                // the set of available video frames.
                mpeg2_custom_fbuf(mpeg2dec, 1);
                for (int i = 0; i < 2; i++)
                {
                    avctx->get_buffer(avctx, picture);
                    mpeg2_set_buf(mpeg2dec, picture->data, picture->opaque);
                }
                break;
            case STATE_PICTURE:
                // This sets up the third buffer for libmpeg2.
                // We use up one of the three buffers for each
                // frame shown. The frames get released once
                // they are drawn (outside this routine).
                avctx->get_buffer(avctx, picture);
                mpeg2_set_buf(mpeg2dec, picture->data, picture->opaque);
                break;
            case STATE_BUFFER:
                // We're finished with the buffer...
                if (partialFrames.size())
                {
                    AVFrame *frm = partialFrames.dequeue();
                    *got_picture_ptr = 1;
                    *picture = *frm;
                    delete frm;
#if 0
                    QString msg("");
                    AvFormatDecoder *nd = (AvFormatDecoder *)(avctx->opaque);
                    if (nd && nd->GetNVP() && nd->GetNVP()->getVideoOutput())
                        msg = nd->GetNVP()->getVideoOutput()->GetFrameStatus();

                    VERBOSE(VB_IMPORTANT, "ret frame: "<<picture->opaque
                            <<"           "<<msg);
#endif
                }
                return pkt->size;
            case STATE_INVALID:
                // This is the error state. The decoder must be
                // reset on an error.
                Reset();
                return -1;

            case STATE_SLICE:
            case STATE_END:
            case STATE_INVALID_END:
                if (info->display_fbuf)
                {
                    bool exists = false;
                    avframe_q::iterator it = partialFrames.begin();
                    for (; it != partialFrames.end(); ++it)
                        if ((*it)->opaque == info->display_fbuf->id)
                            exists = true;

                    if (!exists)
                    {
                        AVFrame *frm = new AVFrame();
                        frm->data[0] = info->display_fbuf->buf[0];
                        frm->data[1] = info->display_fbuf->buf[1];
                        frm->data[2] = info->display_fbuf->buf[2];
                        frm->data[3] = NULL;
                        frm->opaque  = info->display_fbuf->id;
                        frm->type    = FF_BUFFER_TYPE_USER;
                        frm->top_field_first =
                            !!(info->display_picture->flags &
                               PIC_FLAG_TOP_FIELD_FIRST);
                        frm->interlaced_frame =
                            !(info->display_picture->flags &
                              PIC_FLAG_PROGRESSIVE_FRAME);
                        frm->repeat_pict =
                            !!(info->display_picture->flags &
#if CONFIG_LIBMPEG2EXTERNAL
                               PIC_FLAG_REPEAT_FIRST_FIELD);
#else
                               PIC_FLAG_REPEAT_FIELD);
#endif
                        partialFrames.enqueue(frm);

                    }
                }
                if (info->discard_fbuf)
                {
                    bool exists = false;
                    avframe_q::iterator it = partialFrames.begin();
                    for (; it != partialFrames.end(); ++it)
                    {
                        if ((*it)->opaque == info->discard_fbuf->id)
                        {
                            exists = true;
                            (*it)->data[3] = (unsigned char*) 1;
                        }
                    }

                    if (!exists)
                    {
                        AVFrame frame;
                        frame.opaque = info->discard_fbuf->id;
                        frame.type   = FF_BUFFER_TYPE_USER;
                        avctx->release_buffer(avctx, &frame);
                    }
                }
                break;
            default:
                break;
        }
    }
}

void PrivateDecoderMPEG2::ClearFrames(void)
{
    avframe_q::iterator it = partialFrames.begin();
    for (; it != partialFrames.end(); ++it)
        delete (*it);
    partialFrames.clear();
}
