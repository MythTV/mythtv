#include <iostream>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <time.h>
#include "videodev_myth.h"

using namespace std;

#include "mpegrecorder.h"
#include "RingBuffer.h"
#include "mythcontext.h"
#include "programinfo.h"

extern "C" {
#include "../libavcodec/avcodec.h"
#include "../libavformat/avformat.h"
extern AVInputFormat mpegps_demux;
}

MpegRecorder::MpegRecorder()
{
    paused = false;
    mainpaused = false;
    recording = false;

    framesWritten = 0;

    chanfd = -1; 
    readfd = -1;

    width = 720;
    height = 480;

    keyframedist = 15;
    gopset = false;
}

MpegRecorder::~MpegRecorder()
{
    if (chanfd > 0)
        close(chanfd);
    if (readfd > 0)
        close(readfd);
}

void MpegRecorder::SetOption(const QString &opt, int value)
{
    if (opt == "width")
        width = value;
    else if (opt == "height")
        height = value;
    else
        RecorderBase::SetOption(opt, value);
}

void MpegRecorder::StartRecording(void)
{
    chanfd = open(videodevice.ascii(), O_RDWR);
    if (chanfd <= 0)
    {
        cerr << "Can't open video device: " << videodevice << endl;
        perror("open video:");
        return;
    }

    struct v4l2_format vfmt;
    memset(&vfmt, 0, sizeof(vfmt));

    if (ioctl(chanfd, VIDIOC_G_FMT, &vfmt) < 0)
    {
        cerr << "Error getting format\n";
        perror("VIDIOC_G_FMT:");
        return;
    }

    vfmt.fmt.pix.width = width;
    vfmt.fmt.pix.height = height;

    if (ioctl(chanfd, VIDIOC_S_FMT, &vfmt) < 0)
    {
        cerr << "Error setting format\n";
        perror("VIDIOC_S_FMT:");
        return;
    }

    readfd = open(videodevice.ascii(), O_RDWR);
    if (readfd <= 0)
    {
        cerr << "Can't open video device: " << videodevice << endl;
        perror("open video:");
        return;
    }

    unsigned char buffer[256001];
    int ret;

    if (!SetupRecording())
    {
        cerr << "Error initializing recording\n";
        return;
    }

    encoding = true;
    recording = true;

    while (encoding)
    {
        if (paused)
        {
            if (readfd > 0)
            {
                close(readfd);
                readfd = -1;
            }
            mainpaused = true;
            usleep(50);
            continue;
        }

        if (readfd < 0)
            readfd = open(videodevice.ascii(), O_RDWR);

        ret = read(readfd, buffer, 256000);

        if (ret < 0)
        {
            cerr << "error reading from: " << videodevice << endl;
            perror("read");
            continue;
        }
        else if (ret > 0)
            ProcessData(buffer, ret);
    }

    FinishRecording();

    recording = false;
}

int MpegRecorder::GetVideoFd(void)
{
    return chanfd;
}

// start common code to the dvbrecorder class.

static void mpg_write_packet(void *opaque, uint8_t *buf, int buf_size)
{
    (void)opaque;
    (void)buf;
    (void)buf_size;
}

static int mpg_read_packet(void *opaque, uint8_t *buf, int buf_size)
{
    (void)opaque;
    (void)buf;
    (void)buf_size;
    return 0;
}

static int mpg_seek_packet(void *opaque, int64_t offset, int whence)
{
    (void)opaque;
    (void)offset;
    (void)whence;
    return 0;
}

bool MpegRecorder::SetupRecording(void)
{
    AVInputFormat *fmt = &mpegps_demux;
    fmt->flags |= AVFMT_NOFILE;

    ic = (AVFormatContext *)av_mallocz(sizeof(AVFormatContext));
    if (!ic)
    {
        cerr << "Couldn't allocate context\n";
        return false;
    }

    QString filename = "blah.mpg";
    char *cfilename = (char *)filename.ascii();
    AVFormatParameters params;

    ic->pb.buffer_size = 256001;
    ic->pb.buffer = NULL;
    ic->pb.buf_ptr = NULL;
    ic->pb.write_flag = 0;
    ic->pb.buf_end = NULL;
    ic->pb.opaque = this;
    ic->pb.read_packet = mpg_read_packet;
    ic->pb.write_packet = mpg_write_packet;
    ic->pb.seek = mpg_seek_packet;
    ic->pb.pos = 0;
    ic->pb.must_flush = 0;
    ic->pb.eof_reached = 0;
    ic->pb.is_streamed = 0;
    ic->pb.max_packet_size = 0;

    int err = av_open_input_file(&ic, cfilename, fmt, 0, &params);
    if (err < 0)
    {
        cerr << "Couldn't initialize decocder\n";
        return false;
    }    

    return true;
}

void MpegRecorder::FinishRecording(void)
{
    if (curRecording)
    {
        pthread_mutex_lock(db_lock);
        MythContext::KickDatabase(db_conn);
        curRecording->SetPositionMap(positionMap, MARK_GOP_START, db_conn);
        pthread_mutex_unlock(db_lock);
    }
}

void MpegRecorder::ChangeDeinterlacer(int deint_mode)
{
    (void)deint_mode;
}

void MpegRecorder::SetVideoFilters(QString &filters)
{
    (void)filters;
}

void MpegRecorder::Initialize(void)
{
}

#define GOP_START     0x000001B8
#define PICTURE_START 0x00000100
#define SLICE_MIN     0x00000101
#define SLICE_MAX     0x000001af

bool MpegRecorder::PacketHasHeader(unsigned char *buf, int len,
                                   unsigned int startcode)
{
    unsigned char *bufptr;
    unsigned int state = 0xFFFFFFFF, v;

    bufptr = buf;

    while (bufptr < buf + len)
    {
        v = *bufptr++;
        if (state == 0x000001)
        {
            state = ((state << 8) | v) & 0xFFFFFF;
            if (state >= SLICE_MIN && state <= SLICE_MAX)
                return false;
            if (state == startcode)
                return true;
        }
        state = ((state << 8) | v) & 0xFFFFFF;
    }

    return false;
}

void MpegRecorder::ProcessData(unsigned char *buffer, int len)
{
    AVPacket pkt;

    ic->pb.buffer = buffer;
    ic->pb.buf_ptr = ic->pb.buffer;
    ic->pb.buf_end = ic->pb.buffer + len;
    ic->pb.eof_reached = 0;
    ic->pb.pos = len;

    while (ic->pb.eof_reached == 0)
    {
        if (av_read_packet(ic, &pkt) < 0)
            break;
        
        if (pkt.stream_index > ic->nb_streams)
        {
            cerr << "bad stream\n";
            av_free_packet(&pkt);
            continue;
        }

        AVStream *curstream = ic->streams[pkt.stream_index];
        if (pkt.size > 0 && curstream->codec.codec_type == CODEC_TYPE_VIDEO)
        {
            if (PacketHasHeader(pkt.data, pkt.size, PICTURE_START))
            {
                framesWritten++;
            }

            if (PacketHasHeader(pkt.data, pkt.size, GOP_START))
            {
                int frameNum = framesWritten - 1;

                if (!gopset && frameNum > 0)
                {
                    keyframedist = frameNum;
                    gopset = true;
                }

                long long startpos = ringBuffer->GetFileWritePosition();
                startpos += pkt.startpos;

                positionMap[frameNum / keyframedist] = startpos;
            }
        }

        av_free_packet(&pkt);
    }

    ringBuffer->Write(buffer, len);
}

void MpegRecorder::StopRecording(void)
{
    encoding = false;
}

void MpegRecorder::Reset(void)
{
    AVPacketList *pktl = NULL;
    while ((pktl = ic->packet_buffer))
    {
        ic->packet_buffer = pktl->next;
        av_free(pktl);
    }

    ic->pb.pos = 0;
    ic->pb.buf_ptr = ic->pb.buffer;
    ic->pb.buf_end = ic->pb.buffer;

    framesWritten = 0;

    positionMap.clear();
}

void MpegRecorder::Pause(bool clear)
{
    cleartimeonpause = clear;
    mainpaused = false;
    paused = true;
}

void MpegRecorder::Unpause(void)
{
    paused = false;
}

bool MpegRecorder::GetPause(void)
{
    return mainpaused;
}

bool MpegRecorder::IsRecording(void)
{
    return recording;
}

long long MpegRecorder::GetFramesWritten(void)
{
    return framesWritten;
}

void MpegRecorder::TransitionToFile(const QString &lfilename)
{
    ringBuffer->TransitionToFile(lfilename);
}

void MpegRecorder::TransitionToRing(void)
{
    if (curRecording)
    {
        pthread_mutex_lock(db_lock);
        MythContext::KickDatabase(db_conn);
        curRecording->SetPositionMap(positionMap, MARK_GOP_START, db_conn);
        pthread_mutex_unlock(db_lock);

        delete curRecording;
        curRecording = NULL;
    }

    ringBuffer->TransitionToRing();
}

long long MpegRecorder::GetKeyframePosition(long long desired)
{
    long long ret = -1;

    if (positionMap.find(desired) != positionMap.end())
        ret = positionMap[desired];

    return ret;
}

void MpegRecorder::GetBlankFrameMap(QMap<long long, int> &blank_frame_map)
{
    (void)blank_frame_map;
}
