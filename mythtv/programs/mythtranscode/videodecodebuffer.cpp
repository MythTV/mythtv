#include "videodecodebuffer.h"

#include "mythplayer.h"
#include "videooutbase.h"

VideoDecodeBuffer::VideoDecodeBuffer(MythPlayer *player, VideoOutput *videoout,
                                     bool cutlist, int size)
  : m_player(player),         m_videoOutput(videoout),
    m_honorCutlist(cutlist),  m_maxFrames(size),
    m_runThread(true),        m_isRunning(false),
    m_eof(false)
{

}

VideoDecodeBuffer::~VideoDecodeBuffer()
{
    m_runThread = false;
    m_frameWaitCond.wakeAll();

    while (m_isRunning)
        usleep(50000);
}

void VideoDecodeBuffer::stop(void)
{
    m_runThread = false;
    m_frameWaitCond.wakeAll();

    while (m_isRunning)
        usleep(50000);
}

void VideoDecodeBuffer::run()
{
    frm_dir_map_t::iterator dm_iter;

    m_isRunning = true;
    while (m_runThread)
    {
        QMutexLocker locker(&m_queueLock);

        if (m_frameList.size() < m_maxFrames && !m_eof)
        {
            locker.unlock();

            DecodedFrameInfo tfInfo;
            tfInfo.frame = NULL;
            tfInfo.didFF = 0;
            tfInfo.isKey = false;

            if (m_player->TranscodeGetNextFrame(dm_iter, tfInfo.didFF,
                tfInfo.isKey, m_honorCutlist))
            {
                tfInfo.frame = m_videoOutput->GetLastDecodedFrame();

                locker.relock();
                m_frameList.append(tfInfo);
            }
            else if (m_player->GetEof() != kEofStateNone)
            {
                locker.relock();
                m_eof = true;
            }
            else
                continue;

            m_frameWaitCond.wakeAll();
        }
        else
        {
            m_frameWaitCond.wait(locker.mutex());
        }
    }
    m_isRunning = false;
}

VideoFrame *VideoDecodeBuffer::GetFrame(int &didFF, bool &isKey)
{
    QMutexLocker locker(&m_queueLock);

    if (m_frameList.isEmpty())
    {
        if (m_eof)
            return NULL;

        m_frameWaitCond.wait(locker.mutex());

        if (m_frameList.isEmpty())
            return NULL;
    }

    DecodedFrameInfo tfInfo = m_frameList.takeFirst();
    locker.unlock();
    m_frameWaitCond.wakeAll();

    didFF = tfInfo.didFF;
    isKey = tfInfo.isKey;

    return tfInfo.frame;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */

