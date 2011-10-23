#include <QMutex>

#include <iostream>
using namespace std;

#include "gpuavdecoder.h"
#include "gpuplayer.h"
#include "packetqueue.h"
#include "mythlogging.h"

extern "C" {
#include "libavcodec/avcodec.h"
}

void enqueue(void *q, AVStream *stream, AVPacket *pkt, QMutex *mutex);

void enqueue(void *q, AVStream *stream, AVPacket *pkt, QMutex *mutex)
{
    PacketQueue *queue = (PacketQueue *)q;
    if (queue)
        queue->enqueue(stream, pkt, mutex);
}

bool GPUPlayer::ProcessFile(bool showPercentage, StatusCallback StatusCB,
                            void *cbData, PacketQueue *audioQ,
                            PacketQueue *videoQ)
{
    int percentage = 0;
    uint64_t myFramesPlayed = 0;

    killdecoder = false;
    framesPlayed = 0;
    using_null_videoout = true;

    if (OpenFile() < 0)
        return false;

    SetPlaying(true);

    if (!InitVideo())
    {
        LOG(VB_GENERAL, LOG_ERR, "Unable to initialize video");
        SetPlaying(false);
        return false;
    }

    GPUAvDecoder *decoder = (GPUAvDecoder *)GetDecoder();

    // Hook up queue callbacks
    decoder->SetAudioCB(enqueue, audioQ);
    decoder->SetVideoCB(enqueue, videoQ);

    ClearAfterSeek();

    MythTimer flagTime, ui_timer, inuse_timer;
    flagTime.start();
    ui_timer.start();
    inuse_timer.start();

    DecoderGetFrame(kDecodeNothing,true);

    if (showPercentage)
        cout << "\r                         \r" << flush;

    int prevperc = -1;
    while (!GetEof())
    {
        if (inuse_timer.elapsed() > 2534)
        {
            inuse_timer.restart();
            player_ctx->LockPlayingInfo(__FILE__, __LINE__);
            if (player_ctx->playingInfo)
                player_ctx->playingInfo->UpdateInUseMark();
            player_ctx->UnlockPlayingInfo(__FILE__, __LINE__);
        }

        if (ui_timer.elapsed() > 98)
        {
            ui_timer.restart();

            if (totalFrames)
            {
                float elapsed = flagTime.elapsed() * 0.001f;
                int flagFPS = (elapsed > 0.0f) ?
                    (int)(myFramesPlayed / elapsed) : 0;

                percentage = myFramesPlayed * 100 / totalFrames;
                if (StatusCB)
                    (*StatusCB)(percentage, cbData);

                if (showPercentage)
                {
                    QString str = QString("\r%1%/%2fps  \r")
                        .arg(percentage,3).arg(flagFPS,5);
                    cout << qPrintable(str) << flush;
                }
                else if (percentage % 10 == 0 && prevperc != percentage)
                {
                    prevperc = percentage;
                    LOG(VB_GENERAL, LOG_INFO, QString("Progress %1% @ %2fps")
                        .arg(percentage,3).arg(flagFPS,5));
                }
            }
            else 
            {
                if (showPercentage)
                {
                    QString str = QString("\r%1  \r").arg(myFramesPlayed,6);
                    cout << qPrintable(str) << flush;
                }
                else if (myFramesPlayed % 1000 == 0)
                {
                    LOG(VB_GENERAL, LOG_INFO, QString("Frames processed %1")
                        .arg(myFramesPlayed));
                }
            }
        }

        if (DecoderGetFrame(kDecodeNothing,true))
            myFramesPlayed = decoder->GetFramesRead();
    }

    if (showPercentage)
        cout << "\r                         \r" << flush;

    SetPlaying(false);
    killdecoder = true;

    return true;
}

/*
 * vim:ts=4:sw=4:ai:et:si:sts=4
 */
