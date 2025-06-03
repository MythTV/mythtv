#include "audiooutpututil.h"

#include <cstdint>
#include <limits> // workaround QTBUG-90395

#include <QtGlobal>

#include "libmythbase/mythlogging.h"

#include "audioconvert.h"
#include "mythaverror.h"
#include "mythavframe.h"

extern "C" {
#include "libavcodec/avcodec.h"
}

#define LOC QString("AOUtil: ")

/**
 * DecodeAudio
 * Decode an audio packet, and compact it if data is planar
 * Return negative error code if an error occurred during decoding
 * or the number of bytes consumed from the input AVPacket
 * data_size contains the size of decoded data copied into buffer
 */
int AudioOutputUtil::DecodeAudio(AVCodecContext *ctx,
                                 uint8_t *buffer, int &data_size,
                                 const AVPacket *pkt)
{
    MythAVFrame frame;
    bool got_frame = false;

    data_size = 0;
    if (!frame)
    {
        return AVERROR(ENOMEM);
    }

//  SUGGESTION
//  Now that avcodec_decode_audio4 is deprecated and replaced
//  by 2 calls (receive frame and send packet), this could be optimized
//  into separate routines or separate threads.
//  Also now that it always consumes a whole buffer some code
//  in the caller may be able to be optimized.
    int ret = avcodec_receive_frame(ctx,frame);
    if (ret == 0)
        got_frame = true;
    if (ret == AVERROR(EAGAIN))
        ret = 0;
    if (ret == 0)
        ret = avcodec_send_packet(ctx, pkt);
    if (ret == AVERROR(EAGAIN))
        ret = 0;
    else if (ret < 0)
    {
        std::string error;
        LOG(VB_AUDIO, LOG_ERR, LOC +
            QString("audio decode error: %1 (%2)")
            .arg(av_make_error_stdstring(error, ret))
            .arg(got_frame));
        return ret;
    }
    else
    {
        ret = pkt->size;
    }

    if (!got_frame)
    {
        LOG(VB_AUDIO, LOG_DEBUG, LOC +
            QString("audio decode, no frame decoded (%1)").arg(ret));
        return ret;
    }

    auto format = (AVSampleFormat)frame->format;

    data_size = frame->nb_samples * frame->ch_layout.nb_channels * av_get_bytes_per_sample(format);

    if (av_sample_fmt_is_planar(format))
    {
        AudioConvert::InterleaveSamples(
            AudioOutputSettings::AVSampleFormatToFormat(format, ctx->bits_per_raw_sample),
                          frame->ch_layout.nb_channels, buffer, (const uint8_t **)frame->extended_data,
                          data_size);
    }
    else
    {
        // data is already compacted... simply copy it
        memcpy(buffer, frame->extended_data[0], data_size);
    }

    return ret;
}
