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


/*
 * vim:ts=4:sw=4:ai:et:si:sts=4
 */
