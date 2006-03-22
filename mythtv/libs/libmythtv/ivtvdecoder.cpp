#include <iostream>
#include <cassert>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>

using namespace std;

#include "mythconfig.h"
#include "ivtvdecoder.h"
#include "RingBuffer.h"
#include "NuppelVideoPlayer.h"
#include "remoteencoder.h"
#include "programinfo.h"
#include "mythcontext.h"
#include "mythdbcon.h"

#include "videoout_ivtv.h"
#include "videodev_myth.h"

#define LOC QString("IVD: ")
#define LOC_ERR QString("IVD Error: ")

//#define EXTRA_DEBUG

DevInfoMap IvtvDecoder::devInfo;
QMutex     IvtvDecoder::devInfoLock;
const uint IvtvDecoder::vidmax  = 131072; // must be a power of 2

IvtvDecoder::IvtvDecoder(NuppelVideoPlayer *parent, ProgramInfo *pginfo)
    : DecoderBase(parent, pginfo),

      validvpts(false),       gotvideo(false),
      gopset(false),          needPlay(false),
      mpeg_state(0xffffffff),

      prevgoppos(0),          firstgoppos(0),

      vidbuf(new unsigned char[vidmax]),

      vidread(0),             vidwrite(0),
      vidfull(0),             vidframes(0),
      frame_decoded(0),       videoPlayed(0),
      lastStartFrame(0),      laststartpos(0),

      nexttoqueue(1),         lastdequeued(0)
{
    lastResetTime.start();
    fps = 29.97f;
    lastKey = 0;
}

IvtvDecoder::~IvtvDecoder()
{
    if (vidbuf)
        delete[] vidbuf;
}

void IvtvDecoder::SeekReset(long long newkey, uint skipframes,
                            bool needFlush, bool discardFrames)
{
    VERBOSE(VB_PLAYBACK, LOC +
            QString("SeekReset(%1, %2, %3 flush, %4 discard)")
            .arg(newkey).arg(skipframes)
            .arg((needFlush) ? "do" : "don't")
            .arg((discardFrames) ? "do" : "don't"));

    DecoderBase::SeekReset(newkey, skipframes, needFlush, discardFrames);

    skipframes = (exactseeks) ? skipframes : 0;
    vidread = vidwrite = vidfull = 0;
    mpeg_state = 0xffffffff;
    ateof = false;

    framesRead = newkey;
    framesPlayed = newkey;

    VideoOutput *vo = GetNVP()->getVideoOutput();
    VideoOutputIvtv *videoout = (VideoOutputIvtv*) vo;

    if (needFlush)
    {
        needPlay = true;
        lastStartFrame = newkey;
        nexttoqueue = 1;
        lastdequeued = 0;
        vidframes = 0;
        queuedlist.clear();

        videoout->Stop(false /* hide */);
        videoout->Flush();

        videoout->Start(0, skipframes + 5 /* muted frames */);

        if (GetNVP()->GetFFRewSkip() != 1)
            videoout->Play();
        else
        {
            if (GetNVP()->GetPause())
            {
                videoout->Pause();
                do
                    ReadWrite(1);
                while (videoout->GetFramesPlayed() < 1 && !ateof);
                StepFrames(newkey, skipframes);
            }
            else
            {
                videoout->Play();
                bool done = false;
                while (!done)
                {
                    ReadWrite(1);
                    done  = ateof;
                    done |= (uint)videoout->GetFramesPlayed() > skipframes;
                    done |= !skipframes;
                }
            }
        }

        // Note: decoderbase::DoRewdind/DoFastforward will add 1 to
        // framesPlayed so we shouldn't do it here.
        framesPlayed = newkey + skipframes;
        videoPlayed = framesPlayed+1;
    }
    else
    {
        // Kluge!  Apparently the decoder won't honor a speed setting
        // unless it's given enough data right away.  We can't always
        // do that so do again here.
        if (needPlay && videoout->GetFramesPlayed())
        {
            videoout->Play();
            needPlay = false;
        }
    }
}

bool IvtvDecoder::GetDeviceWorks(QString dev)
{
    QMutexLocker locker(&devInfoLock);
    DevInfoMap::const_iterator it = devInfo.find(dev);
    if (it != devInfo.end())
        return (*it).works;
    return false;
}

bool IvtvDecoder::GetDeviceNTSC(QString dev)
{
    QMutexLocker locker(&devInfoLock);
    DevInfoMap::const_iterator it = devInfo.find(dev);
    if (it != devInfo.end())
        return (*it).ntsc;
    return true;
}

void IvtvDecoder::SetDeviceInfo(QString dev, bool works, bool ntsc)
{
    QString tmpStr = QDeepCopy<QString>(dev);
    DeviceInfo tmp;
    tmp.works = works;
    tmp.ntsc  = ntsc;

    QMutexLocker locker(&devInfoLock);
    devInfo[tmpStr] = tmp;
}

/** \fn IvtvDecoder::CheckDevice(void)
 *  \brief Checks the validity of the ivtv output device.
 *
 *   If this returns successfully once. The is cached so that CanHandle()
 *   can be called on RingBuffer changes in the NVP (for LiveTV support).
 */
bool IvtvDecoder::CheckDevice(void)
{
    if (!gContext->GetNumSetting("PVR350OutputEnable", 0))
        return false;

    QString videodev = gContext->GetSetting("PVR350VideoDev");

    if (GetDeviceWorks(videodev))
        return true;

    int testfd = open(videodev.ascii(), O_RDWR);
    if (testfd < 0)
        return false;

    bool ok = false;

    struct v4l2_capability vcap;
    bzero(&vcap, sizeof(vcap));
    if (ioctl(testfd, VIDIOC_QUERYCAP, &vcap) < 0)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR +
                QString("Failed to query capabilities of '%1'")
                .arg(videodev) + ENO);
    }
    else
    {
        if (vcap.version < 0x00000109)
            VERBOSE(VB_IMPORTANT, QString("IVTV driver is version %1, "
                                          "version %2 (or later) is required")
                                          .arg(vcap.version, 0, 16)
                                          .arg(0x00000109, 0, 16));
        else if (vcap.capabilities & V4L2_CAP_VIDEO_OUTPUT)
            ok = true;
    }

    v4l2_std_id std = V4L2_STD_NTSC;
    bool ntsc = true;

    if (ioctl(testfd, VIDIOC_G_STD, &std) < 0)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR +
                QString("Failed to determine video standard used by '%1'.")
                .arg(videodev) + ENO);
    }
    else
    {
        if (std & V4L2_STD_625_50)
            ntsc = false;
    }    

    close(testfd);

    if (!ok)
        return false;

    SetDeviceInfo(videodev, true, ntsc);
    return true;
}

bool IvtvDecoder::CanHandle(char testbuf[kDecoderProbeBufferSize], 
                            const QString &filename, int testbufsize)
{
    if (!CheckDevice())
        return false;

    avcodeclock.lock();
    av_register_all();
    avcodeclock.unlock();

    AVProbeData probe;

    probe.filename = (char *)(filename.ascii());
    probe.buf = (unsigned char *)testbuf;
    probe.buf_size = testbufsize;

    AVInputFormat *fmt = av_probe_input_format(&probe, true);

    if (!strcmp(fmt->name, "mpeg"))
        return true;
    return false;
}

int IvtvDecoder::OpenFile(RingBuffer *rbuffer, bool novideo,
                          char testbuf[kDecoderProbeBufferSize],
                          int)
{
    (void)novideo;
    (void)testbuf;

    ringBuffer = rbuffer;

    GetNVP()->ForceVideoOutputType(kVideoOutput_IVTV);

    keyframedist = -1;
    positionMapType = MARK_UNSET;

    QString videodev = gContext->GetSetting("PVR350VideoDev");
    bool    ntsc     = GetDeviceNTSC(videodev);

    GetNVP()->SetVideoParams(720 /*width*/, (ntsc) ? 480 : 576 /*height*/,
                             (ntsc) ? 29.97f : 25.0f, keyframedist, 1.33);

    fps = (ntsc) ? 29.97f : 25.0f; // save for later length calculations

    ringBuffer->UpdateRawBitrate(8000);

    if (m_playbackinfo || livetv || watchingrecording)
    {
        recordingHasPositionMap = SyncPositionMap();
        if (recordingHasPositionMap && !livetv && !watchingrecording)
        {
            hasFullPositionMap = true;
            gopset = true;
        }
    }

    if (!recordingHasPositionMap)
    {
        int bitrate = 8000;
        float bytespersec = (float)bitrate / 8 / 2;
        float secs = ringBuffer->GetRealFileSize() * 1.0 / bytespersec;

        GetNVP()->SetFileLength((int)(secs), (int)(secs * fps));
    }

    if (hasFullPositionMap)
    {
        VERBOSE(VB_PLAYBACK, "Position map found");
    }
    else if (recordingHasPositionMap)
    {
        VERBOSE(VB_PLAYBACK, "Partial position map found");
    }

    return hasFullPositionMap;
}

#define VID_START     0x000001e0
#define VID_END       0x000001ef

#define SEQ_START     0x000001b3
#define GOP_START     0x000001b8
#define PICTURE_START 0x00000100
#define SLICE_MIN     0x00000101
#define SLICE_MAX     0x000001af

int IvtvDecoder::MpegPreProcessPkt(unsigned char *buf, int len, 
                                   long long startpos, long stopframe)
{
    unsigned char *bufptr = buf;
    unsigned int v = 0;
    
    while (bufptr < buf + len)
    {
        v = *bufptr++;

        if (mpeg_state == 0x000001)
        {
            mpeg_state = ((mpeg_state << 8) | v) & 0xFFFFFF;
            if (mpeg_state >= SLICE_MIN && mpeg_state <= SLICE_MAX)
                continue;

            if (mpeg_state >= VID_START && mpeg_state <= VID_END)
            {
                laststartpos = (bufptr - buf) + startpos - 4;
                continue;
            }

            switch (mpeg_state)
            {
                case GOP_START:
                {
                    int frameNum = framesRead;

                    if (!gopset && frameNum > 0)
                    {
                        if ((firstgoppos > 0) && (keyframedist != 1))
                        {
                            keyframedist = frameNum - firstgoppos;

                            if ((keyframedist == 15) ||
                                (keyframedist == 12))
                                positionMapType = MARK_GOP_START;
                            else
                                positionMapType = MARK_GOP_BYFRAME;

                            gopset = true;
                            GetNVP()->SetKeyframeDistance(keyframedist);
                            VERBOSE(VB_PLAYBACK,
                                    QString("keyframedist changed to %1")
                                    .arg(keyframedist));
                        }
                        else
                            firstgoppos = frameNum;
                    }

                    lastKey = frameNum;
                    if (!hasFullPositionMap)
                    {
                        long long last_frame = 0;
                        if (!m_positionMap.empty())
                            last_frame =
                                m_positionMap[m_positionMap.size() - 1].index;
                        if (keyframedist > 1)
                            last_frame *= keyframedist;
                        if (framesRead > last_frame && keyframedist > 0)
                        {       
                            if (m_positionMap.capacity() ==
                                    m_positionMap.size())
                                m_positionMap.reserve(m_positionMap.size() + 60);
                            PosMapEntry entry = {lastKey / keyframedist,
                                                 lastKey, laststartpos};
                            m_positionMap.push_back(entry);
                        }

                        if ((framesRead > 150) &&
                            (!recordingHasPositionMap) &&
                            (!livetv))
                        {
                            int bitrate = (int)((laststartpos * 8 * fps) /
                                             (framesRead - 1));
                            float bytespersec = (float)bitrate / 8;
                            float secs = ringBuffer->GetRealFileSize() * 1.0 /
                                             bytespersec;
                            GetNVP()->SetFileLength((int)(secs),
                                                    (int)(secs * fps));
                        }

                    }

                    break;
                }
                case PICTURE_START:
                {
                    if (bufptr + 1 >= buf + len)
                        VERBOSE(VB_IMPORTANT, "ivtv picture start overflow, "
                                "please inform mythtv-dev@mythtv.org");
                    int type = (bufptr[1] >> 3) & 7;
                    if (type >= 1 && type <= 3)
                    {
                        if ((long)framesRead < stopframe)
                        {
                            queuedlist.push_back(IvtvQueuedFrame(++vidframes, 
                                                                ++framesRead));
                            //cout << "queued: r=" << vidframes
                            //     << ", p=" << framesRead << endl;
                        }
                        else
                        {
                            ++framesRead;
                            //cout << "mpeg stopped at " << framesRead 
                            //     << ", " << stopframe<< endl;
                            int res = bufptr - buf - 4;
                            if (res < 0)
                            {
                                VERBOSE(VB_IMPORTANT, 
                                        "ivtv picture start underflow, "
                                        "please inform mythtv-dev@mythtv.org");
                                res = 0;
                            }
                            return res;
                        }
                    }
                    break;
                }
                default:
                    break;
            }
            continue;
        }
        mpeg_state = ((mpeg_state << 8) | v) & 0xFFFFFF;
    }

    return bufptr - buf;
}

bool IvtvDecoder::ReadWrite(int onlyvideo, long stopframe)
{
    if (ateof)
        return false;

    gotvideo = false;
    frame_decoded = 0;

    int count, total = 0;
    bool canwrite = false;

    VideoOutputIvtv *videoout = (VideoOutputIvtv*) GetNVP()->getVideoOutput();

    if (onlyvideo < 0)
    {
        vidread = vidwrite = vidfull = 0;
    }
    else if (vidfull || vidread != vidwrite)
    {
        int ret = videoout->Poll(50);

        if (ret==0)
            VERBOSE(VB_PLAYBACK, LOC + "write0 !canwrite");

        canwrite = ret > 0;
        if (canwrite && vidwrite >= vidread)
        {
            count = vidmax - vidwrite;
            count = videoout->WriteBuffer(&vidbuf[vidwrite], count);
            if (count < 0)
            {
                ateof = true;
                VERBOSE(VB_PLAYBACK, LOC + "write1 ateof");
            }
            else if (count > 0)
            {
                vidwrite = (vidwrite + count) & (vidmax - 1);
                vidfull = 0;
                total += count;
#ifdef EXTRA_DEBUG
                VERBOSE(VB_PLAYBACK, LOC +
                        QString("write1 cnt(%1) rd(%2) wr(%3) full(%4)")
                        .arg(count, 5).arg(vidread, 5).arg(vidwrite, 5)
                        .arg(vidfull));
#endif // EXTRA_DEBUG
            }
        }

        if (canwrite && vidwrite < vidread)
        {
            count = vidread - vidwrite;
            count = videoout->WriteBuffer(&vidbuf[vidwrite], count);
            if (count < 0)
            {
                ateof = true;
                VERBOSE(VB_PLAYBACK, LOC + "write2 ateof");
            }
            else if (count > 0)
            {
                vidwrite = (vidwrite + count) & (vidmax - 1);
                vidfull = 0;
                total += count;
#ifdef EXTRA_DEBUG
                VERBOSE(VB_PLAYBACK, LOC +
                        QString("write2 cnt(%1) rd(%2) wr(%3) full(%4)")
                        .arg(count, 5).arg(vidread, 5).arg(vidwrite, 5)
                        .arg(vidfull));
#endif // EXTRA_DEBUG
            }
        }
    }

    int size = 0;
    if ((long)framesRead <= stopframe && (!vidfull || vidread != vidwrite))
    {
        long long startpos = ringBuffer->GetReadPosition();
        if (waitingForChange)
        {
#ifdef EXTRA_DEBUG
            VERBOSE(VB_PLAYBACK,
                    "startpos: "<<startpos
                    <<" readAdjust: "<<readAdjust);
#endif // EXTRA_DEBUG

            if (startpos + 4 >= readAdjust)
            {
                FileChanged();
                startpos = ringBuffer->GetReadPosition();
            }
        }

        if (vidread >= vidwrite)
        {
            size  = vidmax - vidread;
            count = ringBuffer->Read(&vidbuf[vidread], size);
            if (count > 0)
            {
                count = MpegPreProcessPkt(&vidbuf[vidread], count, 
                                          startpos, stopframe);
                vidread = (vidread + count) & (vidmax - 1);
                vidfull = (vidread == vidwrite);
                total += count;
#ifdef EXTRA_DEBUG
                VERBOSE(VB_PLAYBACK, LOC +
                        QString("read1  cnt(%1) rd(%2) wr(%3) full(%4)")
                        .arg(count, 5).arg(vidread, 5).arg(vidwrite, 5)
                        .arg(vidfull));
#endif // EXTRA_DEBUG
            }
        }

        if (vidread < vidwrite)
        {
            size  = vidwrite - vidread;
            count = ringBuffer->Read(&vidbuf[vidread], size);
            if (count > 0)
            {
                count = MpegPreProcessPkt(&vidbuf[vidread], count, 
                                          startpos, stopframe);
                vidread = (vidread + count) & (vidmax - 1);
                vidfull = (vidread == vidwrite);
                total += count;
#ifdef EXTRA_DEBUG
                VERBOSE(VB_PLAYBACK, LOC +
                        QString("read2  cnt(%1) rd(%2) wr(%3) full(%4)")
                        .arg(count, 5).arg(vidread, 5).arg(vidwrite, 5)
                        .arg(vidfull));
#endif // EXTRA_DEBUG
            }
        }

        if (total == 0 && (vidread != vidwrite) && canwrite)
        {
            ateof = true;
#ifdef EXTRA_DEBUG
            VERBOSE(VB_PLAYBACK, LOC +
                    QString("read3  cnt(%1) rd(%2) wr(%3) full(%4) size(%5)")
                    .arg(count, 5).arg(vidread, 5).arg(vidwrite, 5)
                    .arg(vidfull).arg(size) + " ateof");
#endif // EXTRA_DEBUG
        }

        bool was_set = needReset;
        needReset = !total && !canwrite;
        if (needReset && !was_set)
            needResetTimer.start();
    }

#ifdef EXTRA_DEBUG
    if (needReset)
    {
        VERBOSE(VB_PLAYBACK, LOC + "needReset "<<needResetTimer.elapsed()
                <<" livetv("<<(bool)GetNVP()->GetTVChain()<<")");
        usleep(50000);
    }
#endif // EXTRA_DEBUG

    // If hit the EOF and are watching an old recording, set ateof.
    // This lets us exit out of old recording quicker.
    ateof |= needReset && (size > 0) && !(bool)GetNVP()->GetTVChain();

    if (needReset && needResetTimer.elapsed() > 1000)
    {
        // Abort the reset itself if the we just did a reset, to avoid looping.
        if (lastResetTime.elapsed() < 1200)
        {
            VERBOSE(VB_IMPORTANT, LOC + "RESET -- aborted "
                    <<lastResetTime.elapsed());
            ateof = true;
            return false;
        }
        needReset = false;
        lastResetTime.start();
        VERBOSE(VB_IMPORTANT, LOC + "*********************************");
        VERBOSE(VB_IMPORTANT, LOC + "RESET -- begin");
        lastdequeued = 0;
        vidframes = 0;
        queuedlist.clear();
        videoout->Stop(false /* hide */);
        videoout->Flush();
        videoout->Start(0 /*GOP offset*/, 0 /* muted frames */);
        videoout->Play();
        VERBOSE(VB_IMPORTANT, LOC + "RESET -- end");
        VERBOSE(VB_IMPORTANT, LOC + "*********************************");
    }

    if ((long)framesRead <= stopframe)
        return (total > 0);
    else
        return (vidfull || vidread != vidwrite);
}

bool IvtvDecoder::GetFrame(int onlyvideo)
{
    long long last_read = framesRead;

    if (GetNVP()->GetFFRewSkip() == 1 || onlyvideo < 0)
        ReadWrite(onlyvideo, LONG_MAX);
    else
    {
        long stopframe = framesRead + 1;
        while (ReadWrite(onlyvideo, stopframe))
            ;
    }

    if (ateof && !GetNVP()->GetEditMode())
    {
        VERBOSE(VB_PLAYBACK, LOC + QString("NVP::SetEof() at frame %1")
                .arg(framesRead));
        GetNVP()->SetEof();
    }

    framesPlayed = framesRead - 1;

    return (framesRead != last_read);
}

bool IvtvDecoder::DoFastForward(long long desiredFrame, bool doflush)
{
    long long number = desiredFrame - videoPlayed;

    if (GetNVP()->GetPause() && number < keyframedist)
    {
        StepFrames(videoPlayed, number+1);
        framesPlayed = desiredFrame + 1;
        videoPlayed = framesPlayed;
        GetNVP()->SetFramesPlayed(videoPlayed);
        return !ateof;
    }

    return DecoderBase::DoFastForward(desiredFrame, doflush);
}

void IvtvDecoder::UpdateFramesPlayed(void)
{
    VideoOutputIvtv *videoout = (VideoOutputIvtv*)(GetNVP()->getVideoOutput());

    int rawframes = videoout->GetFramesPlayed();

    if (rawframes < lastdequeued)
    {
        VERBOSE(VB_IMPORTANT, QString("IVTV rawframes decreased! "
                                      "Did the decoder reset?"));
    }
    else
    {
        while (rawframes != lastdequeued)
        {
            if (queuedlist.empty())
            {
                VERBOSE(VB_IMPORTANT, QString("IVTV framelist is empty!"));
                break;
            }
            lastdequeued = queuedlist.front().raw;
            videoPlayed = queuedlist.front().actual;
            queuedlist.pop_front();
            //cout << "                        "
            //     << "dequeued: r=" << lastdequeued 
            //     << ", p=" << videoPlayed << endl;
        }
    }

    GetNVP()->SetFramesPlayed(videoPlayed);
}

bool IvtvDecoder::StepFrames(long long start, long long count)
{
    VideoOutputIvtv *videoout = (VideoOutputIvtv*)(GetNVP()->getVideoOutput());

    long long step, cur = 0, last = start;

    //fprintf(stderr, "stepping %d from %d\n", count, last);

    for (step = 0; step < count && !ateof; step++)
    {
        while (ReadWrite(1))
            ;

        //fprintf(stderr, "    step %d at %d\n", step, last);
        videoout->Step();
        usleep(1000);

        int tries;
        const int maxtries = 500;
        const int restep = 25;

        for (tries = 0; tries < maxtries && !ateof; tries++)
        {
            while (ReadWrite(1))
                ;

            cur = lastStartFrame + videoout->GetFramesPlayed();
            if (cur > last)
                break;

            if (tries && !(tries % restep))
            {
                videoout->Pause();
                VERBOSE(VB_IMPORTANT, QString("  extra step %1 at %2")
                                              .arg(step)
                                              .arg(last));
                videoout->Step();
                usleep(1000);
            }
        }

        videoout->Pause();

        //fprintf(stderr, "        %d tries to %d\n", tries, cur);

        if (tries >= maxtries)
        {
            VERBOSE(VB_IMPORTANT, QString("IvtvDecoder timed out while "
                                          "stepping, giving up"));
            break;
        }
        
        last = cur;
    }

    //fprintf(stderr, "    stepped to %d\n", cur);

    return true;
}

