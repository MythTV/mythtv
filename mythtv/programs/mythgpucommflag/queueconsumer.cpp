#include "mythlogging.h"
#include "packetqueue.h"
#include "resultslist.h"
#include "queueconsumer.h"

extern "C" {
#include "libavutil/avutil.h"
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
}


void QueueConsumer::run(void)
{
    RunProlog();
    LOG(VB_GENERAL, LOG_INFO, QString("Starting %1").arg(m_name));

    if (!Initialize())
        return;

    while(!m_done || !m_inQ->isEmpty())
    {
        Packet *packet = m_inQ->dequeue(-1);
        if (!packet)
            continue;

        // Send it to the code to process
        ProcessPacket(packet);

        // Free up the buffer.
        delete packet;
    }

    LOG(VB_GENERAL, LOG_INFO, QString("Ending %1").arg(m_name));
    RunEpilog();
}


int64_t QueueConsumer::NormalizeTimecode(int64_t timecode)
{
    int64_t start_pts = 0;

    if (m_stream->start_time != (int64_t)AV_NOPTS_VALUE)
        start_pts = m_stream->start_time;

    int64_t pts = timecode - start_pts;

    pts = av_rescale(pts, 1000000 * m_timebase.num, m_timebase.den);

#ifdef DEBUG_TIMESTAMP
    LOG(VB_GENERAL, LOG_INFO, QString("Timecode %1, Start %2, Normalized %3 us")
        .arg(timecode) .arg(start_pts) .arg(pts));
#endif
    return pts;
}

int64_t QueueConsumer::NormalizeDuration(int64_t duration)
{
    int64_t retval;

    retval = av_rescale(duration, m_timebase.num * 1000000, m_timebase.den);

#ifdef DEBUG_TIMESTAMP
    LOG(VB_GENERAL, LOG_INFO, QString("Duration %1, Normalized %2 us")
        .arg(duration) .arg(retval));
#endif
    return retval;
}


/*
 * vim:ts=4:sw=4:ai:et:si:sts=4
 */
