#include <algorithm>
#include <thread> // for sleep_for

#include "PrePostRollFlagger.h"

// Qt headers
#include <QCoreApplication>

// MythTV headers
#include "libmythbase/mythcorecontext.h"
#include "libmythbase/mythlogging.h"
#include "libmythbase/programinfo.h"
#include "libmythtv/mythcommflagplayer.h"

PrePostRollFlagger::PrePostRollFlagger(SkipType commDetectMethod,
                            bool showProgress, bool fullSpeed,
                            MythCommFlagPlayer *player,
                            const QDateTime& startedAt_in,
                            const QDateTime& stopsAt_in,
                            const QDateTime& recordingStartedAt_in,
                            const QDateTime& recordingStopsAt_in):
    ClassicCommDetector( commDetectMethod,  showProgress,  fullSpeed,
        player,          startedAt_in,      stopsAt_in,
        recordingStartedAt_in,              recordingStopsAt_in)
{
}

void PrePostRollFlagger::Init()
{
    ClassicCommDetector::Init();
}

bool PrePostRollFlagger::go()
{
    int secsSince = 0;
    int requiredBuffer = 120;
    int requiredHeadStart = requiredBuffer;
    bool wereRecording = m_stillRecording;

    secsSince = m_startedAt.secsTo(MythDate::current());
    while (m_stillRecording && (secsSince < requiredHeadStart))
    {
        emit statusUpdate(QCoreApplication::translate("(mythcommflag)",
            "Waiting to pass preroll + head start"));

        emit breathe();
        if (m_bStop)
            return false;

        std::this_thread::sleep_for(5s);
        secsSince = m_startedAt.secsTo(MythDate::current());
    }

    if (m_player->OpenFile() < 0)
        return false;

    Init();


    // Don't bother flagging short ~realtime recordings
    if ((wereRecording) && (!m_stillRecording) && (secsSince < requiredHeadStart))
        return false;

    m_aggressiveDetection =
        gCoreContext->GetBoolSetting("AggressiveCommDetect", true);

    if (!m_player->InitVideo())
    {
        LOG(VB_GENERAL, LOG_ERR,
            "NVP: Unable to initialize video for FlagCommercials.");
        return false;
    }

    emit breathe();
    if (m_bStop)
        return false;

    QElapsedTimer flagTime;
    flagTime.start();

    if (m_recordingStopsAt < MythDate::current() )
        m_myTotalFrames = m_player->GetTotalFrameCount();
    else
        m_myTotalFrames = (long long)(m_player->GetFrameRate() *
                        (m_recordingStartedAt.secsTo(m_recordingStopsAt)));



    if (m_showProgress)
    {
        if (m_myTotalFrames)
            std::cerr << "  0%/      ";
        else
            std::cerr << "     0/      ";
        std::cerr.flush();
    }

    float aspect = m_player->GetVideoAspect();

    SetVideoParams(aspect);

    emit breathe();

    long long stopFrame = m_preRoll + (m_fps * 120); //look up to 2 minutes past
    long long framesToProcess = 0;
    if(m_preRoll)
        framesToProcess += stopFrame;
    if(m_postRoll)
        //guess two minutes before
        framesToProcess += m_myTotalFrames - m_postRoll + (m_fps * 120);


    long long framesProcessed = 0;
    if(m_preRoll > 0)
    {
        //check from preroll after
        LOG(VB_COMMFLAG, LOG_INFO,
            QString("Finding closest after preroll(%1-%2)")
                .arg(m_preRoll).arg(stopFrame));

        m_closestAfterPre = findBreakInrange(m_preRoll, stopFrame, framesToProcess,
                                           framesProcessed, flagTime, false);

        LOG(VB_COMMFLAG, LOG_INFO, QString("Closest after preroll: %1")
                .arg(m_closestAfterPre));


        //check before preroll
        long long startFrame = 0;
        if(m_closestAfterPre)
            startFrame = m_preRoll - (m_closestAfterPre - m_preRoll) - 1;

        LOG(VB_COMMFLAG, LOG_INFO, QString("Finding before preroll (%1-%2)")
                .arg(startFrame).arg(m_preRoll));
        m_closestBeforePre = findBreakInrange(startFrame, m_preRoll,
                                            framesToProcess, framesProcessed,
                                            flagTime, true);
        LOG(VB_COMMFLAG, LOG_INFO, QString("Closest before preroll: %1")
                .arg(m_closestBeforePre));

        if(m_closestBeforePre || m_closestAfterPre)
            emit gotNewCommercialBreakList();

        // for better processing percent
        framesToProcess -= (stopFrame - framesProcessed);

    }

    if(m_stillRecording)
    {
        while (MythDate::current() <= m_recordingStopsAt)
        {
            emit breathe();
            if (m_bStop)
                return false;
            emit statusUpdate(QCoreApplication::translate("(mythcommflag)",
                "Waiting for recording to finish"));
            std::this_thread::sleep_for(5s);
        }
        m_stillRecording = false;
         m_myTotalFrames = m_player->GetTotalFrameCount();
    }

    if(m_postRoll > 0)
    {
        //check from preroll after
        long long postRollStartLoc = m_myTotalFrames - m_postRoll;
        LOG(VB_COMMFLAG, LOG_INFO,
            QString("Finding closest after postroll(%1-%2)")
                .arg(postRollStartLoc).arg(m_myTotalFrames));
        m_closestAfterPost = findBreakInrange(postRollStartLoc, m_myTotalFrames,
                                            framesToProcess, framesProcessed,
                                            flagTime, false);
        LOG(VB_COMMFLAG, LOG_INFO, QString("Closest after postRoll: %1")
                .arg(m_closestAfterPost));

        //check before preroll
        long long startFrame = 0;
        if(m_closestAfterPost)
            startFrame = postRollStartLoc
                         - (m_closestAfterPost - postRollStartLoc) - 1;

        LOG(VB_COMMFLAG, LOG_INFO,
            QString("finding closest before preroll(%1-%2)")
                .arg(startFrame).arg(postRollStartLoc));
        m_closestBeforePost = findBreakInrange(startFrame, postRollStartLoc,
                                             framesToProcess, framesProcessed,
                                             flagTime, true);
        LOG(VB_COMMFLAG, LOG_INFO, QString("Closest before postroll: %1")
                .arg(m_closestBeforePost));

//      framesToProcess = framesProcessed;
    }

    if (m_showProgress)
    {
        //float elapsed = flagTime.elapsed() / 1000.0;

        //float flagFPS = (elapsed > 0.0F) ? (framesProcessed / elapsed) : 0.0F;

        if (m_myTotalFrames)
            std::cerr << "\b\b\b\b\b\b      \b\b\b\b\b\b";
        else
            std::cerr << "\b\b\b\b\b\b\b\b\b\b\b\b\b             "
                         "\b\b\b\b\b\b\b\b\b\b\b\b\b";
        std::cerr.flush();
    }

    return true;
}


long long PrePostRollFlagger::findBreakInrange(long long startFrame,
                                               long long stopFrame,
                                               long long totalFrames,
                                               long long &framesProcessed,
                                               QElapsedTimer&flagTime, bool findLast)
{
    int requiredBuffer = 30;
    int prevpercent = -1;

    if(startFrame > 0)
        startFrame--;
    else
        startFrame = 0;

    m_player->DiscardVideoFrame(m_player->GetRawVideoFrame(0));

    long long tmpStartFrame = startFrame;
    MythVideoFrame* f = m_player->GetRawVideoFrame(tmpStartFrame);
    float aspect = m_player->GetVideoAspect();
    long long currentFrameNumber = f->m_frameNumber;
    LOG(VB_COMMFLAG, LOG_INFO, QString("Starting with frame %1")
            .arg(currentFrameNumber));
    m_player->DiscardVideoFrame(f);

    long long foundFrame = 0;

    while (m_player->GetEof() == kEofStateNone)
    {
        std::chrono::microseconds startTime {0us};
        if (m_stillRecording)
            startTime = nowAsDuration<std::chrono::microseconds>();

        MythVideoFrame* currentFrame = m_player->GetRawVideoFrame();
        currentFrameNumber = currentFrame->m_frameNumber;

        if(currentFrameNumber % 1000 == 0)
        {
            LOG(VB_COMMFLAG, LOG_INFO, QString("Processing frame %1")
                    .arg(currentFrameNumber));
        }

        if(currentFrameNumber > stopFrame || (!findLast && foundFrame))
        {
            m_player->DiscardVideoFrame(currentFrame);
            break;
        }

        float newAspect = currentFrame->m_aspect;
        if (newAspect != aspect)
        {
            SetVideoParams(aspect);
            aspect = newAspect;
        }

        if (((currentFrameNumber % 500) == 0) ||
            (((currentFrameNumber % 100) == 0) &&
             (m_stillRecording)))
        {
            emit breathe();
            if (m_bStop)
            {
                m_player->DiscardVideoFrame(currentFrame);
                return 0;
            }
        }

        while (m_bPaused)
        {
            emit breathe();
            std::this_thread::sleep_for(1s);
        }

        // sleep a little so we don't use all cpu even if we're niced
        if (!m_fullSpeed && !m_stillRecording)
            std::this_thread::sleep_for(10ms);

        if (((currentFrameNumber % 500) == 0) ||
            ((m_showProgress || m_stillRecording) &&
             ((currentFrameNumber % 100) == 0)))
        {
            float elapsed = flagTime.elapsed() / 1000.0F;

            float flagFPS = 0.0F;
            if (elapsed != 0.0F)
                flagFPS = framesProcessed / elapsed;

            int percentage = 0;
            if (stopFrame)
                percentage = framesProcessed * 100 / totalFrames;
            percentage = std::min(percentage, 100);

            if (m_showProgress)
            {
                if (stopFrame)
                {
                    QString tmp = QString("\b\b\b\b\b\b\b\b\b\b\b%1%/%2fps")
                        .arg(percentage, 3).arg((int)flagFPS, 3);
                    QByteArray ba = tmp.toLatin1();
                    std::cerr << ba.constData() << std::flush;
                }
                else
                {
                    QString tmp = QString("\b\b\b\b\b\b\b\b\b\b\b\b\b%1/%2fps")
                        .arg(currentFrameNumber, 6).arg((int)flagFPS, 3);
                    QByteArray ba = tmp.toLatin1();
                    std::cerr << ba.constData() << std::flush;
                }
                std::cerr.flush();
            }

            if (stopFrame)
            {
                emit statusUpdate(QCoreApplication::translate("(mythcommflag)",
                    "%1% Completed @ %2 fps.")
                        .arg(percentage).arg(flagFPS));
            }
            else
            {
                emit statusUpdate(QCoreApplication::translate("(mythcommflag)",
                    "%1 Frames Completed @ %2 fps.")
                        .arg((long)currentFrameNumber).arg(flagFPS));
            }

            if (percentage % 10 == 0 && prevpercent != percentage)
            {
                prevpercent = percentage;
                LOG(VB_GENERAL, LOG_INFO, QString("%1%% Completed @ %2 fps.")
                    .arg(percentage) .arg(flagFPS));
            }
        }

        ProcessFrame(currentFrame, currentFrameNumber);

        if (m_frameInfo[currentFrameNumber].flagMask &
           (COMM_FRAME_SCENE_CHANGE | COMM_FRAME_BLANK))
        {
            foundFrame = currentFrameNumber;
        }

        if (m_stillRecording)
        {
            int secondsRecorded =
                m_recordingStartedAt.secsTo(MythDate::current());
            int secondsFlagged = (int)(framesProcessed / m_fps);
            int secondsBehind = secondsRecorded - secondsFlagged;
            auto usecPerFrame = floatusecs(1000000.0F / m_player->GetFrameRate());

            auto endTime = nowAsDuration<std::chrono::microseconds>();

            floatusecs usecSleep = usecPerFrame - (endTime - startTime);

            if (secondsBehind > requiredBuffer)
            {
                if (m_fullSpeed)
                    usecSleep = 0us;
                else
                    usecSleep = usecSleep * 0.25;
            }
            else if (secondsBehind < requiredBuffer)
            {
                usecSleep = usecPerFrame * 1.5;
            }

            if (usecSleep > 0us)
                std::this_thread::sleep_for(usecSleep);
        }

        m_player->DiscardVideoFrame(currentFrame);
        framesProcessed++;
    }
    return foundFrame;
}


void PrePostRollFlagger::GetCommercialBreakList(frm_dir_map_t &marks)
{
    LOG(VB_COMMFLAG, LOG_INFO, "PrePostRollFlagger::GetCommBreakMap()");
    marks.clear();

    long long end = 0;
    if(m_closestAfterPre && m_closestBeforePre)
    {
        //choose closest
        if(m_closestAfterPre - m_preRoll < m_preRoll - m_closestBeforePre)
            end = m_closestAfterPre;
        else
            end = m_closestBeforePre;
    }
    else if(m_closestBeforePre)
    {
        end = m_closestBeforePre;
    }
    else if(m_closestAfterPre)
    {
        end = m_closestAfterPre;
    }
    else
    {
        end = m_preRoll;
    }

    if(end)
    {
        marks[0] = MARK_COMM_START;
        marks[end] = MARK_COMM_END;
    }

    long long start = 0;
    if(m_closestAfterPost && m_closestBeforePost)
    {
        //choose closest
        if(m_closestAfterPost - m_postRoll < m_postRoll - m_closestBeforePost)
            start = m_closestAfterPost;
        else
            start = m_closestBeforePost;
    }
    else if(m_closestBeforePost)
    {
        start = m_closestBeforePost;
    }
    else if(m_closestAfterPost)
    {
        start = m_closestAfterPost;
    }
    else if(m_postRoll)
    {
        start = m_myTotalFrames - m_postRoll;
    }

    if(start)
    {
        marks[start] = MARK_COMM_START;
        marks[m_myTotalFrames] = MARK_COMM_END;
    }
}
