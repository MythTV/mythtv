#include <iostream>
#include <cassert>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>

using namespace std;

#include "ivtvdecoder.h"
#include "RingBuffer.h"
#include "NuppelVideoPlayer.h"
#include "remoteencoder.h"
#include "programinfo.h"
#include "mythcontext.h"
#include "mythdbcon.h"

#include "videoout_ivtv.h"
#include "videodev_myth.h"

bool IvtvDecoder::ntsc = true;

IvtvDecoder::IvtvDecoder(NuppelVideoPlayer *parent, ProgramInfo *pginfo)
           : DecoderBase(parent, pginfo)
{
    lastStartFrame = 0;
    videoPlayed = 0;

    gopset = false;
    firstgoppos = 0;
    prevgoppos = 0;

    fps = 29.97;
    validvpts = false;

    lastKey = 0;

    vidread = vidwrite = vidfull = 0;
    vidscan = vidpoll = vid2write = 0;
    vidanyframes = videndofframe = 0;
    startpos = 0;
    mpeg_state = 0xffffffff;
    framesScanned = 0;

    nexttoqueue = 1;
    lastdequeued = 0;
    queuedlist.clear();
}

IvtvDecoder::~IvtvDecoder()
{
}

void IvtvDecoder::SeekReset(long long newkey, int skipframes, bool needFlush)
{
    //fprintf(stderr, "seek reset frame = %llu, skip = %d, exact = %d\n", 
    //        newkey, skipframes, exactseeks);

    if (!exactseeks)
        skipframes = 0;

    vidread = vidwrite = vidfull = 0;
    vidscan = vidpoll = vid2write = 0;
    vidanyframes = videndofframe = 0;
    startpos = ringBuffer->GetTotalReadPosition();
    mpeg_state = 0xffffffff;
    ateof = false;

    framesRead = newkey;
    framesScanned = newkey;

    VideoOutputIvtv *videoout = 
        (VideoOutputIvtv *)m_parent->getVideoOutput();

    if (needFlush)
    {
        needPlay = true;
        lastStartFrame = newkey;
        nexttoqueue = 1;
        lastdequeued = 0;
        queuedlist.clear();

        videoout->Stop(false);
        videoout->Flush();

        videoout->Start(0, skipframes+5);

        if (m_parent->GetPause())
        {
            videoout->Pause();
            do {
                while (ReadWrite(1))
                    ;
            } while (videoout->GetFramesPlayed() < 1 && !ateof);
            StepFrames(newkey, skipframes);
        }
        else
        {
            videoout->Play();
            if (m_parent->GetFFRewSkip() == 1)
            {
                do {
                    while (ReadWrite(1))
                        ;
                } while (videoout->GetFramesPlayed() <= skipframes && !ateof);
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

bool IvtvDecoder::CanHandle(char testbuf[2048], const QString &filename)
{
    if (!gContext->GetNumSetting("PVR350OutputEnable", 0))
        return false;

    QString videodev = gContext->GetSetting("PVR350VideoDev");

    int testfd = open(videodev.ascii(), O_RDWR);
    if (testfd < 0)
        return false;

    bool ok = false;

    struct v4l2_capability vcap;
    memset(&vcap, 0, sizeof(vcap));
    if (ioctl(testfd, VIDIOC_QUERYCAP, &vcap) < 0)
        perror("VIDIOC_QUERYCAP");
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
    ntsc = true;

    if (ioctl(testfd, VIDIOC_G_STD, &std) < 0)
        perror("VIDIOC_G_STD");
    else
    {
        if (std & V4L2_STD_625_50)
            ntsc = false;
    }    

    close(testfd);

    if (!ok)
        return false;

    av_register_all();

    AVProbeData probe;

    probe.filename = (char *)(filename.ascii());
    probe.buf = (unsigned char *)testbuf;
    probe.buf_size = 2048;

    AVInputFormat *fmt = av_probe_input_format(&probe, true);

    if (!strcmp(fmt->name, "mpeg"))
        return true;
    return false;
}

int IvtvDecoder::OpenFile(RingBuffer *rbuffer, bool novideo,
                              char testbuf[2048])
{
    (void)novideo;
    (void)testbuf;

    ringBuffer = rbuffer;

    m_parent->ForceVideoOutputType(kVideoOutput_IVTV);

    keyframedist = -1;
    positionMapType = MARK_UNSET;

    int video_width = 720;
    int video_height = 480;
    if (!ntsc)
    {
        video_height = 576;
        fps = 25.00;
    }
    m_parent->SetVideoParams(video_width, video_height, 
                             fps, keyframedist, 1.33);
     
    ringBuffer->CalcReadAheadThresh(8000);

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

        m_parent->SetFileLength((int)(secs), (int)(secs * fps));
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

int IvtvDecoder::MpegPreProcessPkt(unsigned char *buf, int start, int len, 
                                    long long startpos)
{
    unsigned char *bufptr = buf + start;
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
                    int frameNum = framesScanned;

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
                            m_parent->SetKeyframeDistance(keyframedist);
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
                        if (framesScanned > last_frame && keyframedist > 0)
                        {       
                            if (m_positionMap.capacity() ==
                                    m_positionMap.size())
                                m_positionMap.reserve(m_positionMap.size() + 60);
                            PosMapEntry entry = {lastKey / keyframedist,
                                                 lastKey, laststartpos};
                            m_positionMap.push_back(entry);
                        }

                        if ((framesScanned > 150) &&
                            (!recordingHasPositionMap) &&
                            (!livetv))
                        {
                            int bitrate = (int)((laststartpos * 8 * fps) /
                                             (framesScanned - 1));
                            float bytespersec = (float)bitrate / 8;
                            float secs = ringBuffer->GetRealFileSize() * 1.0 /
                                             bytespersec;
                            m_parent->SetFileLength((int)(secs),
                                                    (int)(secs * fps));
                        }

                    }

                    break;
                }
                case PICTURE_START:
                {
                    int type = (bufptr[1] >> 3) & 7;
                    if (type >= 1 && type <= 3)
                    {
                        framesScanned++;
                        if (exitafterdecoded)
                            gotvideo = 1;
                        return bufptr - buf;
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

    return -1;
}

bool IvtvDecoder::ReadWrite(int onlyvideo)
{
    int n;

    if (ateof)
        return false;

    if (vid2write) {
        if (onlyvideo < 0)
            n = vid2write;
        else
        {
            VideoOutputIvtv *videoout = 
                (VideoOutputIvtv *)m_parent->getVideoOutput();

            if (vidpoll)
            {
                n = videoout->Poll(1);
                if (n <= 0)
                {
                    if (n < 0)
                        ateof = 1;
                    return false;
                }
            }

            n = videoout->WriteBuffer(vidbuf+vidwrite, vid2write);
            if (n < 0)
            {
                ateof = 1;
                return false;
            }
        }

        vidwrite += n;
        vid2write -= n;

        if (vid2write)
        {
            vidpoll = 1;
            return false;
        }
        else
        {
            vidpoll = 0;
            if (videndofframe)
            {
                if (vidanyframes)
                {
                    framesRead++;
                    return false;
                }
                vidanyframes = 1;
            }
            return true;
        }
    }

    if (vidread - vidwrite <= 4) {
        n = vidread - vidwrite;
        memcpy(vidbuf, vidbuf+vidread-n, n);
        startpos += vidread - n;
        vidscan = n;
        vidread = n;
        vidwrite = 0;
    }

    if (vidread < vidmax) {
        n = ringBuffer->Read(vidbuf+vidread, vidmax-vidread);
        if (n > 0)
            vidread += n;
        if (vidread == vidwrite) {
            ateof = 1;
            return false;
        }
    }

    if (videndofframe)
    {
        //cout << "queued: r=" << nexttoqueue 
        //     << ", p=" << framesScanned << endl;
        queuedlist.push_back(IvtvQueuedFrame(nexttoqueue++, framesScanned));
    }

    vidscan = MpegPreProcessPkt(vidbuf, vidscan, vidread, startpos);
    if (vidscan >= 0)
    {
        vid2write = vidscan - 4 - vidwrite;
        videndofframe = 1;
    }
    else
    {
        vidscan = vidread;
        if (vidread - vidwrite > 3)
        {
            vid2write = vidread - 3 - vidwrite;
            videndofframe = 0;
        }
        else
        {
            vid2write = vidread - vidwrite;
            videndofframe = 1;
        }
    }

    return true;
}

bool IvtvDecoder::GetFrame(int onlyvideo)
{
    long long last_read = framesRead;

    if (onlyvideo < 0 || m_parent->GetFFRewSkip() != 1)
    {
        do {
            ReadWrite(onlyvideo);
        } while (!ateof && framesRead == last_read);
    }
    else
    {
        while (ReadWrite(onlyvideo))
            ;
    }

    if (ateof && !m_parent->GetEditMode())
        m_parent->SetEof();

    framesPlayed = framesRead;

    return (framesRead != last_read);
}

bool IvtvDecoder::DoFastForward(long long desiredFrame, bool doflush)
{
    long long number = desiredFrame - videoPlayed;

    if (m_parent->GetPause() && number < keyframedist)
    {
        StepFrames(videoPlayed, number+1);
        framesPlayed = desiredFrame + 1;
        videoPlayed = framesPlayed;
        m_parent->SetFramesPlayed(videoPlayed);
        return !ateof;
    }

    return DecoderBase::DoFastForward(desiredFrame, doflush);
}

void IvtvDecoder::UpdateFramesPlayed(void)
{
    VideoOutputIvtv *videoout = (VideoOutputIvtv *)m_parent->getVideoOutput();

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

    m_parent->SetFramesPlayed(videoPlayed);
}

bool IvtvDecoder::StepFrames(int start, int count)
{
    VideoOutputIvtv *videoout = (VideoOutputIvtv *)m_parent->getVideoOutput();

    int step, cur = 0, last = start;

    //fprintf(stderr, "stepping %d from %d\n", count, last);

    for (step = 0; step < count && !ateof; step++)
    {
        while (ReadWrite(1))
            ;

        //fprintf(stderr, "    step %d at %d\n", step, last);
        videoout->Step();
        usleep(1000);

        int tries;
        const int maxtries = 500000;
        const int restep = 100000;

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

