#include "mythlogging.h"
#include "packetqueue.h"
#include "resultslist.h"
#include "audioconsumer.h"

extern "C" {
#include "libavutil/avutil.h"
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
}

void AudioConsumer::ProcessPacket(Packet *packet)
{
    LOG(VB_GENERAL, LOG_INFO, "Audio Packet");

    // Decode the packet to PCM
    AVStream *curstream = packet->m_stream;
    AVPacket *pkt = packet->m_pkt;
    QMutex   *avcodeclock = packet->m_mutex;

    AVCodecContext *ctx = curstream->codec;
    int ret             = 0;
    int dataSize        = 0;
    int frames          = -1;
    int64_t pts;

    pts = (long long)(av_q2d(curstream->time_base) * pkt->pts * 1000);
    if (pkt->dts != (int64_t)AV_NOPTS_VALUE)
        pts = (long long)(av_q2d(curstream->time_base) * pkt->dts * 1000);

    AVPacket tmp_pkt;
    tmp_pkt.data = pkt->data;
    tmp_pkt.size = pkt->size;

    while (tmp_pkt.size > 0)
    {
        dataSize = AVCODEC_MAX_AUDIO_FRAME_SIZE;
        avcodeclock->lock();
        ret = avcodec_decode_audio3(ctx, m_audioSamples, &dataSize, &tmp_pkt);
        avcodeclock->unlock();
        if (ret < 0)
        {
            LOG(VB_GENERAL, LOG_ERR, "Unknown audio decoding error");
            return;
        }

        if (dataSize > 0)
        {
            int64_t temppts = pts;

            frames = dataSize / (ctx->channels *
                 av_get_bits_per_sample_fmt(ctx->sample_fmt)>>3);
            // Process each frame
            ProcessFrame(m_audioSamples, dataSize, frames, pts);
            pts += (long long) ((double)(frames * 1000) / ctx->sample_rate);

            LOG(VB_TIMESTAMP, LOG_DEBUG,  QString("audio timecode %1 %2 %3 %4")
                    .arg(pkt->pts).arg(pkt->dts).arg(temppts).arg(pts));
        }

        tmp_pkt.data += ret;
        tmp_pkt.size -= ret;
    }
}
    

void AudioConsumer::ProcessFrame(int16_t *samples, int size, int frames,
                                 int64_t pts)
{
    LOG(VB_GENERAL, LOG_INFO, QString("Audio Frame: %1 samples (%2 size)")
        .arg(frames).arg(size));
    // Push PCM frame to GPU/CPU Processing memory

    // Loop through the list of detection routines
        // Run the routine in GPU/CPU
        // Pull the results
        // Toss the results onto the results list

    // Free the frame in GPU/CPU memory if not needed
}

/*
 * vim:ts=4:sw=4:ai:et:si:sts=4
 */
