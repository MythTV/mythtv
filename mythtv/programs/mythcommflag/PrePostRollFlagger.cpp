#include <unistd.h>

#include "PrePostRollFlagger.h"

// MythTV headers
#include "mythcorecontext.h"
#include "programinfo.h"
#include "mythplayer.h"

PrePostRollFlagger::PrePostRollFlagger(SkipType commDetectMethod,
                            bool showProgress,bool fullSpeed,
                            MythPlayer* player,
                            const QDateTime& startedAt_in,
                            const QDateTime& stopsAt_in,
                            const QDateTime& recordingStartedAt_in,
                            const QDateTime& recordingStopsAt_in):
    ClassicCommDetector( commDetectMethod,  showProgress,  fullSpeed,
        player,          startedAt_in,      stopsAt_in,
        recordingStartedAt_in,              recordingStopsAt_in),
        myTotalFrames(0),                   closestAfterPre(0),
        closestBeforePre(0),                closestAfterPost(0),
        closestBeforePost(0)
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
    bool wereRecording = stillRecording;

    secsSince = startedAt.secsTo(MythDate::current());
    while (stillRecording && (secsSince < requiredHeadStart))
    {
        emit statusUpdate(QObject::tr("Waiting to pass preroll + head start"));

        emit breathe();
        if (m_bStop)
            return false;

        sleep(5);
        secsSince = startedAt.secsTo(MythDate::current());
    }

    if (player->OpenFile() < 0)
        return false;

    Init();


    // Don't bother flagging short ~realtime recordings
    if ((wereRecording) && (!stillRecording) && (secsSince < requiredHeadStart))
        return false;

    aggressiveDetection =
        gCoreContext->GetNumSetting("AggressiveCommDetect", 1);

    if (!player->InitVideo())
    {
        LOG(VB_GENERAL, LOG_ERR,
            "NVP: Unable to initialize video for FlagCommercials.");
        return false;
    }
    player->EnableSubtitles(false);

    emit breathe();
    if (m_bStop)
        return false;

    QTime flagTime;
    flagTime.start();

    if (recordingStopsAt < MythDate::current() )
        myTotalFrames = player->GetTotalFrameCount();
    else
        myTotalFrames = (long long)(player->GetFrameRate() *
                        (recordingStartedAt.secsTo(recordingStopsAt)));



    if (showProgress)
    {
        if (myTotalFrames)
            cerr << "  0%/      ";
        else
            cerr << "     0/      ";
        cerr.flush();
    }

    float aspect = player->GetVideoAspect();

    SetVideoParams(aspect);

    emit breathe();

    long long stopFrame = preRoll + fps * 120; //look up to 2 minutes past
    long long framesToProcess = 0;
    if(preRoll)
        framesToProcess += stopFrame;
    if(postRoll)
        //guess two minutes before
        framesToProcess += myTotalFrames - postRoll + fps * 120;


    long long framesProcessed = 0;
    if(preRoll > 0)
    {
        //check from preroll after
        LOG(VB_COMMFLAG, LOG_INFO,
            QString("Finding closest after preroll(%1-%2)")
                .arg(preRoll).arg(stopFrame));

        closestAfterPre = findBreakInrange(preRoll, stopFrame, framesToProcess,
                                           framesProcessed, flagTime, false);

        LOG(VB_COMMFLAG, LOG_INFO, QString("Closest after preroll: %1")
                .arg(closestAfterPre));


        //check before preroll
        long long startFrame = 0;
        if(closestAfterPre)
            startFrame = preRoll - (closestAfterPre - preRoll) - 1;

        LOG(VB_COMMFLAG, LOG_INFO, QString("Finding before preroll (%1-%2)")
                .arg(startFrame).arg(preRoll));
        closestBeforePre = findBreakInrange(startFrame, preRoll,
                                            framesToProcess, framesProcessed,
                                            flagTime, true);
        LOG(VB_COMMFLAG, LOG_INFO, QString("Closest before preroll: %1")
                .arg(closestBeforePre));

        if(closestBeforePre || closestAfterPre)
            emit gotNewCommercialBreakList();

        // for better processing percent
        framesToProcess -= (stopFrame - framesProcessed);

    }

    if(stillRecording)
    {
        while (MythDate::current() <= recordingStopsAt)
        {
            emit breathe();
            if (m_bStop)
                return false;
            emit statusUpdate(QObject::tr("Waiting for recording to finish"));
            sleep(5);
        }
        stillRecording = false;
         myTotalFrames = player->GetTotalFrameCount();
    }

    if(postRoll > 0)
    {
        //check from preroll after
        long long postRollStartLoc = myTotalFrames - postRoll;
        LOG(VB_COMMFLAG, LOG_INFO,
            QString("Finding closest after postroll(%1-%2)")
                .arg(postRollStartLoc).arg(myTotalFrames));
        closestAfterPost = findBreakInrange(postRollStartLoc, myTotalFrames,
                                            framesToProcess, framesProcessed,
                                            flagTime, false);
        LOG(VB_COMMFLAG, LOG_INFO, QString("Closest after postRoll: %1")
                .arg(closestAfterPost));

        //check before preroll
        long long startFrame = 0;
        if(closestAfterPost)
            startFrame = postRollStartLoc
                         - (closestAfterPost - postRollStartLoc) - 1;

        LOG(VB_COMMFLAG, LOG_INFO,
            QString("finding closest before preroll(%1-%2)")
                .arg(startFrame).arg(postRollStartLoc));
        closestBeforePost = findBreakInrange(startFrame, postRollStartLoc,
                                             framesToProcess, framesProcessed,
                                             flagTime, true);
        LOG(VB_COMMFLAG, LOG_INFO, QString("Closest before postroll: %1")
                .arg(closestBeforePost));

        framesToProcess = framesProcessed;
    }

    if (showProgress)
    {
        //float elapsed = flagTime.elapsed() / 1000.0;

        //float flagFPS = (elapsed > 0.0f) ? (framesProcessed / elapsed) : 0.0f;

        if (myTotalFrames)
            cerr << "\b\b\b\b\b\b      \b\b\b\b\b\b";
        else
            cerr << "\b\b\b\b\b\b\b\b\b\b\b\b\b             "
                    "\b\b\b\b\b\b\b\b\b\b\b\b\b";
        cerr.flush();
    }

    return true;
}


long long PrePostRollFlagger::findBreakInrange(long long startFrame,
                                               long long stopFrame,
                                               long long totalFrames,
                                               long long &framesProcessed,
                                             QTime &flagTime, bool findLast)
{
    float flagFPS;
    int requiredBuffer = 30;
    long long currentFrameNumber;
    int prevpercent = -1;

    if(startFrame > 0)
        startFrame--;
    else
        startFrame = 0;

    player->DiscardVideoFrame(player->GetRawVideoFrame(0));

    long long tmpStartFrame = startFrame;
    VideoFrame* f = player->GetRawVideoFrame(tmpStartFrame);
    float aspect = player->GetVideoAspect();
    currentFrameNumber = f->frameNumber;
    LOG(VB_COMMFLAG, LOG_INFO, QString("Starting with frame %1")
            .arg(currentFrameNumber));
    player->DiscardVideoFrame(f);

    long long foundFrame = 0;

    while (player->GetEof() == kEofStateNone)
    {
        struct timeval startTime;
        if (stillRecording)
            gettimeofday(&startTime, NULL);

        VideoFrame* currentFrame = player->GetRawVideoFrame();
        currentFrameNumber = currentFrame->frameNumber;

        if(currentFrameNumber % 1000 == 0)
        {
            LOG(VB_COMMFLAG, LOG_INFO, QString("Processing frame %1")
                    .arg(currentFrameNumber));
        }

        if(currentFrameNumber > stopFrame || (!findLast && foundFrame))
        {
            player->DiscardVideoFrame(currentFrame);
            break;
        }

        double newAspect = currentFrame->aspect;
        if (newAspect != aspect)
        {
            SetVideoParams(aspect);
            aspect = newAspect;
        }

        if (((currentFrameNumber % 500) == 0) ||
            (((currentFrameNumber % 100) == 0) &&
             (stillRecording)))
        {
            emit breathe();
            if (m_bStop)
            {
                player->DiscardVideoFrame(currentFrame);
                return false;
            }
        }

        while (m_bPaused)
        {
            emit breathe();
            sleep(1);
        }

        // sleep a little so we don't use all cpu even if we're niced
        if (!fullSpeed && !stillRecording)
            usleep(10000);

        if (((currentFrameNumber % 500) == 0) ||
            ((showProgress || stillRecording) &&
             ((currentFrameNumber % 100) == 0)))
        {
            float elapsed = flagTime.elapsed() / 1000.0;

            if (elapsed)
                flagFPS = framesProcessed / elapsed;
            else
                flagFPS = 0.0;

            int percentage;
            if (stopFrame)
                percentage = framesProcessed * 100 / totalFrames;
            else
                percentage = 0;

            if (percentage > 100)
                percentage = 100;

            if (showProgress)
            {
                if (stopFrame)
                {
                    QString tmp = QString("\b\b\b\b\b\b\b\b\b\b\b%1%/%2fps")
                        .arg(percentage, 3).arg((int)flagFPS, 3);
                    QByteArray ba = tmp.toAscii();
                    cerr << ba.constData() << flush;
                }
                else
                {
                    QString tmp = QString("\b\b\b\b\b\b\b\b\b\b\b\b\b%1/%2fps")
                        .arg(currentFrameNumber, 6).arg((int)flagFPS, 3);
                    QByteArray ba = tmp.toAscii();
                    cerr << ba.constData() << flush;
                }
                cerr.flush();
            }

            if (stopFrame)
                emit statusUpdate(QObject::tr("%1% Completed @ %2 fps.")
                                  .arg(percentage).arg(flagFPS));
            else
                emit statusUpdate(QObject::tr("%1 Frames Completed @ %2 fps.")
                                  .arg((long)currentFrameNumber).arg(flagFPS));

            if (percentage % 10 == 0 && prevpercent != percentage)
            {
                prevpercent = percentage;
                LOG(VB_GENERAL, LOG_INFO, QString("%1%% Completed @ %2 fps.")
                    .arg(percentage) .arg(flagFPS));
            }
        }

        ProcessFrame(currentFrame, currentFrameNumber);

        if(frameInfo[currentFrameNumber].flagMask &
           (COMM_FRAME_SCENE_CHANGE | COMM_FRAME_BLANK))
        {
            foundFrame = currentFrameNumber;
        }

        if (stillRecording)
        {
            int secondsRecorded =
                recordingStartedAt.secsTo(MythDate::current());
            int secondsFlagged = (int)(framesProcessed / fps);
            int secondsBehind = secondsRecorded - secondsFlagged;
            long usecPerFrame = (long)(1.0 / player->GetFrameRate() * 1000000);

            struct timeval endTime;
            gettimeofday(&endTime, NULL);

            long long usecSleep =
                      usecPerFrame -
                      (((endTime.tv_sec - startTime.tv_sec) * 1000000) +
                       (endTime.tv_usec - startTime.tv_usec));

            if (secondsBehind > requiredBuffer)
            {
                if (fullSpeed)
                    usecSleep = 0;
                else
                    usecSleep = (long)(usecSleep * 0.25);
            }
            else if (secondsBehind < requiredBuffer)
                usecSleep = (long)(usecPerFrame * 1.5);

            if (usecSleep > 0)
                usleep(usecSleep);
        }

        player->DiscardVideoFrame(currentFrame);
        framesProcessed++;
    }
    return foundFrame;
}


void PrePostRollFlagger::GetCommercialBreakList(frm_dir_map_t &marks)
{
    LOG(VB_COMMFLAG, LOG_INFO, "PrePostRollFlagger::GetCommBreakMap()");
    marks.clear();

    long long end = 0;
    if(closestAfterPre && closestBeforePre)
    {
        //choose closest
        if(closestAfterPre - preRoll < preRoll - closestBeforePre)
            end = closestAfterPre;
        else
            end = closestBeforePre;
    }
    else if(closestBeforePre)
        end = closestBeforePre;
    else if(closestAfterPre)
        end = closestAfterPre;
    else
        end = preRoll;

    if(end)
    {
        marks[0] = MARK_COMM_START;
        marks[end] = MARK_COMM_END;
    }

    long long start = 0;
    if(closestAfterPost && closestBeforePost)
    {
        //choose closest
        if(closestAfterPost - postRoll < postRoll - closestBeforePost)
            start = closestAfterPost;
        else
            start = closestBeforePost;
    }
    else if(closestBeforePost)
        start = closestBeforePost;
    else if(closestAfterPost)
        start = closestAfterPost;
    else if(postRoll)
        start = myTotalFrames - postRoll;

    if(start)
    {
        marks[start] = MARK_COMM_START;
        marks[myTotalFrames] = MARK_COMM_END;
    }
}
