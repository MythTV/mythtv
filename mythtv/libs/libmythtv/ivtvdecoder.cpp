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

IvtvDecoder::IvtvDecoder(NuppelVideoPlayer *parent, MythSqlDatabase *db,
                         ProgramInfo *pginfo)
           : DecoderBase(parent, db, pginfo)
{
    lastStartFrame = 0;

    keyframedist = 15;

    gopset = false;
    firstgoppos = 0;
    prevgoppos = 0;

    fps = 29.97;
    validvpts = false;

    lastKey = 0;
    rewindExtraFrame = true;

    prvpkt[0] = prvpkt[1] = prvpkt[2] = 0;

    vidread = vidwrite = vidfull = 0;

    positionMapType = MARK_GOP_START;
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
    ateof = false;

    framesRead = newkey;
    lastStartFrame = newkey;

    VideoOutputIvtv *videoout = (VideoOutputIvtv *)m_parent->getVideoOutput();

    videoout->Stop(false);
    videoout->Flush();

    videoout->Start(0, skipframes+5);
    videoout->Pause();

    while (!videoout->GetFramesPlayed() && !ateof)
        ReadWrite(1, 0);

    if (m_parent->GetPause())
        StepFrames(newkey, skipframes);
    else
        videoout->Play();

    framesPlayed = newkey + skipframes + 1;
    m_parent->SetFramesPlayed(framesPlayed);
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
            cerr << "ivtv driver is version " << hex << vcap.version <<
                ", version " << hex <<  0x00000109 << " (or later) " <<
                "is required" << endl;
        else if (vcap.capabilities & V4L2_CAP_VIDEO_OUTPUT)
            ok = true;
    }

    v4l2_std_id std = V4L2_STD_NTSC;
    ntsc = true;

    if (ioctl(testfd, VIDIOC_G_STD, &std) < 0)
        perror("VIDIOC_G_STD");
    else
    {
        if (std & V4L2_STD_PAL)
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

    int video_width = 720;
    int video_height = 480;
    if (!ntsc)
    {
        video_height = 576;
        fps = 25.00;
        keyframedist = 12;
    }
    m_parent->SetVideoParams(video_width, video_height, 
                             fps, keyframedist, 1.33);
     
    ringBuffer->CalcReadAheadThresh(8000);

    if ((m_playbackinfo && m_db) || livetv || watchingrecording)
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

void IvtvDecoder::MpegPreProcessPkt(unsigned char *buf, int len, 
                                    long long startpos)
{
    unsigned char *bufptr = buf;
    unsigned int state = 0xFFFFFFFF, v = 0;
    
    int prvcount = -1;

    while (bufptr < buf + len)
    {
        if (++prvcount < 3)
            v = prvpkt[prvcount];
        else
            v = *bufptr++;

        if (state == 0x000001)
        {
            state = ((state << 8) | v) & 0xFFFFFF;
            if (state >= SLICE_MIN && state <= SLICE_MAX)
                continue;

            if (state >= VID_START && state <= VID_END)
            {
                laststartpos = (bufptr - buf) + startpos - 4;
                continue;
            }

            switch (state)
            {
                case GOP_START:
                {
                    int frameNum = framesRead;

                    if (!gopset && frameNum > 0)
                    {
                        if (firstgoppos > 0)
                        {
                            keyframedist = frameNum - firstgoppos;
                            gopset = true;
                            m_parent->SetVideoParams(-1, -1, -1, keyframedist);
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
                        framesRead++;
                        if (exitafterdecoded)
                            gotvideo = 1;
                    }
                    break;
                }
                default:
                    break;
            }
            continue;
        }
        state = ((state << 8) | v) & 0xFFFFFF;
    }

    memcpy(prvpkt, buf + len - 3, 3);
}

bool IvtvDecoder::ReadWrite(int onlyvideo, int delay)
{
    if (ateof)
        return false;

    gotvideo = false;
    frame_decoded = 0;

    int count, total = 0;

    if (onlyvideo < 0)
        vidread = vidwrite = vidfull = 0;
    else if (vidfull || vidread != vidwrite)
    {
        VideoOutputIvtv *videoout = (VideoOutputIvtv *)m_parent->getVideoOutput();
        if (delay)
            videoout->Poll(delay);

        if (vidwrite >= vidread)
        {
            count = videoout->WriteBuffer(&vidbuf[vidwrite], vidmax - vidwrite);
            if (count < 0)
                ateof = true;
            else if (count > 0)
            {
                vidwrite = (vidwrite + count) & (vidmax - 1);
                vidfull = 0;
                total += count;
                //fprintf(stderr, "write1 = %d, %d, %d, %d\n", count, vidread, vidwrite, vidfull);
            }
        }

        if (vidwrite < vidread)
        {
            count = videoout->WriteBuffer(&vidbuf[vidwrite], vidread - vidwrite);
            if (count < 0)
                ateof = true;
            else if (count > 0)
            {
                vidwrite = (vidwrite + count) & (vidmax - 1);
                vidfull = 0;
                total += count;
                //fprintf(stderr, "write2 = %d, %d, %d, %d\n", count, vidread, vidwrite, vidfull);
            }
        }
    }

    if (!vidfull || vidread != vidwrite)
    {
        if (vidread >= vidwrite)
        {
            long long startpos = ringBuffer->GetTotalReadPosition();
            count = ringBuffer->Read(&vidbuf[vidread], vidmax - vidread);
            if (count > 0)
            {
                MpegPreProcessPkt(&vidbuf[vidread], count, startpos);
                vidread = (vidread + count) & (vidmax - 1);
                vidfull = (vidread == vidwrite);
                total += count;
                //fprintf(stderr, "read1 = %d, %d, %d, %d\n", count, vidread, vidwrite, vidfull);
            }
        }

        if (vidread < vidwrite)
        {
            long long startpos = ringBuffer->GetTotalReadPosition();
            count = ringBuffer->Read(&vidbuf[vidread], vidwrite - vidread);
            if (count > 0)
            {
                MpegPreProcessPkt(&vidbuf[vidread], count, startpos);
                vidread = (vidread + count) & (vidmax - 1);
                vidfull = (vidread == vidwrite);
                total += count;
                //fprintf(stderr, "read2 = %d, %d, %d, %d\n", count, vidread, vidwrite, vidfull);
            }
        }

        if (total == 0)
            ateof = true;
    }

    return (total > 0);
}

void IvtvDecoder::GetFrame(int onlyvideo)
{
    ReadWrite(onlyvideo, 1);
    UpdateFramesPlayed();

    if (ateof && !m_parent->GetEditMode())
        m_parent->SetEof();
}

bool IvtvDecoder::DoFastForward(long long desiredFrame)
{
    long long number = desiredFrame - framesPlayed;

    if (m_parent->GetPause() && number < 15)
    {
        StepFrames(framesPlayed, number+1);
        framesPlayed = desiredFrame + 1;
        m_parent->SetFramesPlayed(framesPlayed);
        return !ateof;
    }

    return DecoderBase::DoFastForward(desiredFrame);
}

void IvtvDecoder::UpdateFramesPlayed(void)
{
    VideoOutputIvtv *videoout = (VideoOutputIvtv *)m_parent->getVideoOutput();

    int newframes = videoout->GetFramesPlayed();

    if (lastStartFrame + newframes >= framesPlayed &&
        lastStartFrame + newframes <= framesRead)
        framesPlayed = lastStartFrame + newframes;
    else
        fprintf(stderr, "bad newframes: %d %lld %lld %lld\n",
                newframes, lastStartFrame, framesRead, framesPlayed);

    m_parent->SetFramesPlayed(framesPlayed);
}

bool IvtvDecoder::StepFrames(int start, int count)
{
    VideoOutputIvtv *videoout = (VideoOutputIvtv *)m_parent->getVideoOutput();

    int step, cur = 0, last = start;

    //fprintf(stderr, "stepping %d from %d\n", count, last);

    for (step = 0; step < count && !ateof; step++)
    {
        while (ReadWrite(1, 0))
            ;

        //fprintf(stderr, "    step %d at %d\n", step, last);
        videoout->Step();
        usleep(1000);

        int tries;
        const int maxtries = 500000;
        const int restep = 100000;

        for (tries = 0; tries < maxtries && !ateof; tries++)
        {
            while (ReadWrite(1, 0))
                ;

            cur = lastStartFrame + videoout->GetFramesPlayed();
            if (cur > last)
                break;

            if (tries && !(tries % restep))
            {
                videoout->Pause();
                fprintf(stderr, "        extra step %d at %d\n", step, last);
                videoout->Step();
                usleep(1000);
            }
        }

        videoout->Pause();

        //fprintf(stderr, "        %d tries to %d\n", tries, cur);

        if (tries >= maxtries)
        {
            cerr << "timed out while stepping, giving up" << endl;
            break;
        }
        
        last = cur;
    }

    //fprintf(stderr, "    stepped to %d\n", cur);

    return true;
}

