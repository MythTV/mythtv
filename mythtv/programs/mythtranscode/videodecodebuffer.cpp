#include "videodecodebuffer.h"

VideoDecodeBuffer::VideoDecodeBuffer(MythPlayer *player, VideoOutput *videoout,
                                     bool cutlist, int size)
  : m_player(player),         m_videoOutput(videoout),
    m_honorCutlist(cutlist),
    m_eof(false),             m_maxFrames(size),
    m_runThread(true),        m_isRunning(false)
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
        if (m_frameList.size() < m_maxFrames && !m_eof)
        {
            DecodedFrameInfo tfInfo;
            tfInfo.frame = NULL;
            tfInfo.didFF = 0;
            tfInfo.isKey = false;

            if (m_player->TranscodeGetNextFrame(dm_iter, tfInfo.didFF,
                tfInfo.isKey, m_honorCutlist))
            {
                tfInfo.frame = m_videoOutput->GetLastDecodedFrame();

                QMutexLocker locker(&m_queueLock);
                m_frameList.append(tfInfo);
            }
            else
            {
                m_eof = true;
            }

            m_frameWaitCond.wakeAll();
        }
        else
        {
            m_frameWaitLock.lock();
            m_frameWaitCond.wait(&m_frameWaitLock);
            m_frameWaitLock.unlock();
        }
    }
    m_isRunning = false;
}

VideoFrame *VideoDecodeBuffer::GetFrame(int &didFF, bool &isKey)
{
    m_queueLock.lock();

    if (m_frameList.isEmpty())
    {
        m_queueLock.unlock();

        if (m_eof)
            return NULL;

        m_frameWaitLock.lock();
        m_frameWaitCond.wait(&m_frameWaitLock);
        m_frameWaitLock.unlock();

        if (m_frameList.isEmpty())
            return NULL;

        m_queueLock.lock();
    }

    DecodedFrameInfo tfInfo = m_frameList.takeFirst();
    m_queueLock.unlock();
    m_frameWaitCond.wakeAll();

    didFF = tfInfo.didFF;
    isKey = tfInfo.isKey;

    return tfInfo.frame;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */

