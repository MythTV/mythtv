#include "videodecodebuffer.h"

#include "mythplayer.h"

#include <chrono> // for milliseconds
#include <thread> // for sleep_for

VideoDecodeBuffer::~VideoDecodeBuffer()
{
    m_runThread = false;
    m_frameWaitCond.wakeAll();

    while (m_isRunning)
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
}

void VideoDecodeBuffer::stop(void)
{
    m_runThread = false;
    m_frameWaitCond.wakeAll();

    while (m_isRunning)
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
}

void VideoDecodeBuffer::run()
{
    m_isRunning = true;
    while (m_runThread)
    {
        QMutexLocker locker(&m_queueLock);

        if (m_frameList.size() < m_maxFrames && !m_eof)
        {
            locker.unlock();

            DecodedFrameInfo tfInfo;
            tfInfo.frame = nullptr;
            tfInfo.didFF = 0;
            tfInfo.isKey = false;

            if (m_player->TranscodeGetNextFrame(tfInfo.didFF,
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
            return nullptr;

        m_frameWaitCond.wait(locker.mutex());

        if (m_frameList.isEmpty())
            return nullptr;
    }

    DecodedFrameInfo tfInfo = m_frameList.takeFirst();
    locker.unlock();
    m_frameWaitCond.wakeAll();

    didFF = tfInfo.didFF;
    isKey = tfInfo.isKey;

    return tfInfo.frame;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */

