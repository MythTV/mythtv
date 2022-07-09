// Std
#include <chrono>
#include <thread>

// MythTV
#include "mythtranscodeplayer.h"
#include "videodecodebuffer.h"

VideoDecodeBuffer::VideoDecodeBuffer(MythTranscodePlayer* Player, MythVideoOutput* Videoout,
                                     bool Cutlist, int Size)
  : m_player(Player),        m_videoOutput(Videoout),
    m_honorCutlist(Cutlist), m_maxFrames(Size)
{
}

VideoDecodeBuffer::~VideoDecodeBuffer()
{
    m_runThread = false;
    m_frameWaitCond.wakeAll();
    while (m_isRunning)
        std::this_thread::sleep_for(50ms);
}

void VideoDecodeBuffer::stop()
{
    m_runThread = false;
    m_frameWaitCond.wakeAll();
    while (m_isRunning)
        std::this_thread::sleep_for(50ms);
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

            DecodedFrameInfo frameinfo {};
            frameinfo.frame = nullptr;
            frameinfo.didFF = 0;
            frameinfo.isKey = false;

            if (m_player->TranscodeGetNextFrame(frameinfo.didFF, frameinfo.isKey, m_honorCutlist))
            {
                frameinfo.frame = m_videoOutput->GetLastDecodedFrame();
                locker.relock();
                m_frameList.append(frameinfo);
            }
            else if (m_player->GetEof() != kEofStateNone)
            {
                locker.relock();
                m_eof = true;
            }
            else
            {
                continue;
            }
            m_frameWaitCond.wakeAll();
        }
        else
        {
            m_frameWaitCond.wait(locker.mutex());
        }
    }
    m_isRunning = false;
}

MythVideoFrame *VideoDecodeBuffer::GetFrame(int &DidFF, bool &Key)
{
    QMutexLocker locker(&m_queueLock);

    if (m_frameList.isEmpty())
    {
        if (m_eof)
            return nullptr;

        m_frameWaitCond.wait(locker.mutex());
        // cppcheck-suppress knownConditionTrueFalse
        if (m_frameList.isEmpty())
            return nullptr;
    }

    DecodedFrameInfo tfInfo = m_frameList.takeFirst();
    locker.unlock();
    m_frameWaitCond.wakeAll();
    DidFF = tfInfo.didFF;
    Key = tfInfo.isKey;
    return tfInfo.frame;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */

