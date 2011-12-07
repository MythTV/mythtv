#include <strings.h>

#include "mythlogging.h"
#include "packetqueue.h"
#include "resultslist.h"
#include "audiopacket.h"
#include "audioconsumer.h"
#include "audioprocessor.h"

extern "C" {
#include "libavutil/avutil.h"
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
}

AudioConsumer::AudioConsumer(PacketQueue *inQ, ResultsList *outL,
                             OpenCLDevice *dev) : 
    QueueConsumer(inQ, outL, dev, "AudioConsumer") 
{
    m_audioSamples = (int16_t *)av_mallocz(AVCODEC_MAX_AUDIO_FRAME_SIZE *
                                           sizeof(int32_t));
    InitAudioProcessors();
    if (m_dev)
        m_proclist = openCLAudioProcessorList;
    else
        m_proclist = softwareAudioProcessorList;

    m_context = avcodec_alloc_context();
}

void AudioConsumer::ProcessPacket(Packet *packet)
{
    LOG(VB_GENERAL, LOG_INFO, "Audio Packet");

    // Decode the packet to PCM
    AVStream *curstream = packet->m_stream;
    AVPacket *pkt = packet->m_pkt;
    QMutex   *avcodeclock = packet->m_mutex;

    if (!m_opened)
    {
        QMutexLocker lock(avcodeclock);
        m_codec = avcodec_find_decoder(curstream->codec->codec_id);
        if (avcodec_open(m_context, m_codec) < 0)
            return;

        m_stream = curstream;

        memcpy(&m_timebase, &curstream->time_base, sizeof(m_timebase));
        LOG(VB_GENERAL, LOG_NOTICE, QString("Audio: num = %1, den = %2")
            .arg(m_timebase.num) .arg(m_timebase.den));
        m_opened = true;
    }

    int ret             = 0;
    int dataSize        = 0;
    int frames          = -1;
    int rate;
    int64_t pts;

    pts = (long long)(av_q2d(curstream->time_base) * pkt->pts * 1000);
    if (pkt->dts != (int64_t)AV_NOPTS_VALUE)
        pts = (long long)(av_q2d(curstream->time_base) * pkt->dts * 1000);

    AVPacket tmp_pkt;
    av_init_packet(&tmp_pkt);
    tmp_pkt.data = pkt->data;
    tmp_pkt.size = pkt->size;

    while (tmp_pkt.size > 0)
    {
        dataSize = AVCODEC_MAX_AUDIO_FRAME_SIZE;

        avcodeclock->lock();
        ret = avcodec_decode_audio3(m_context, m_audioSamples, &dataSize,
                                    &tmp_pkt);
        frames = dataSize / (m_context->channels *
                 av_get_bits_per_sample_fmt(m_context->sample_fmt)>>3);
        rate = m_context->sample_rate;
        avcodeclock->unlock();

        if (ret < 0)
        {
            LOG(VB_GENERAL, LOG_ERR, "Unknown audio decoding error");
            return;
        }

        if (dataSize > 0)
        {
            int64_t temppts = pts;

#if 0
            LOG(VB_GENERAL, LOG_INFO,
                QString("Channels: %1, Size: %2, Rate: %3") .arg(ctx->channels)
                .arg(av_get_bits_per_sample_fmt(ctx->sample_fmt))
                .arg(ctx->sample_rate));
#endif

            // Process each frame
            ProcessFrame(m_audioSamples, dataSize, frames, pts, rate, pkt->pos);

            pts += (long long) ((double)(frames * 1000) / rate);

            LOG(VB_TIMESTAMP, LOG_DEBUG,  QString("audio timecode %1 %2 %3 %4")
                    .arg(pkt->pts).arg(pkt->dts).arg(temppts).arg(pts));
        }

        tmp_pkt.data += ret;
        tmp_pkt.size -= ret;
    }
}
    

void AudioConsumer::ProcessFrame(int16_t *samples, int size, int frames,
                                 int64_t pts, int rate, uint64_t pos)
{
    LOG(VB_GENERAL, LOG_INFO, QString("Audio Frame: %1 samples (%2 size)")
        .arg(frames).arg(size));

    int duration = (int)((double)(frames * 1000000) / (rate * 1.0));
    AudioPacket *audioPacket = NULL;

    // Push PCM frame to GPU/CPU Processing memory
    if (m_dev)
    {
        // Push PCM frame to GPU
        audioPacket = new AudioPacket(m_dev, samples, size, frames);
    }

    // Loop through the list of detection routines
    for (AudioProcessorList::iterator it = m_proclist->begin();
         it != m_proclist->end(); ++it)
    {
        AudioProcessor *proc = *it;

        // Run the routine in GPU/CPU & pull results
        FlagResults *result = proc->m_func(m_dev, samples, size, frames, pts,
                                           rate);

        // Toss the results onto the results list
        if (result)
        {
            static AVRational realTimeBase = { 1, 1000 };
            LOG(VB_GENERAL, LOG_INFO, "Audio Finding found");
            pts = av_rescale_q(pts, realTimeBase, m_timebase);
            result->m_timestamp = NormalizeTimecode(pts);;
            result->m_duration = duration;
            LOG(VB_GENERAL, LOG_INFO, result->toString());
            m_outL->append(result);
        }
    }

    if (m_dev)
    {
        // Free the frame in GPU/CPU memory if not needed
        delete audioPacket;
    }
}


/*
 * vim:ts=4:sw=4:ai:et:si:sts=4
 */
