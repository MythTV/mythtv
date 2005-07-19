#include <iostream>
#include <pthread.h>
#include <cstdio>
#include <cstdlib>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <ctime>
#include <qregexp.h>
#include "videodev_myth.h"

extern "C" {
#include <inttypes.h>
#ifdef USING_IVTV_HEADER
#include <linux/ivtv.h>
#else
#include "ivtv_myth.h"
#endif
}

using namespace std;

#include "mpegrecorder.h"
#include "RingBuffer.h"
#include "mythcontext.h"
#include "programinfo.h"
#include "recordingprofile.h"

extern "C" {
#include "../libavcodec/avcodec.h"
#include "../libavformat/avformat.h"
}

const int MpegRecorder::audRateL1[] = { 32, 64, 96, 128, 160, 192, 224, 
                                        256, 288, 320, 352, 384, 416, 448, 0 };
const int MpegRecorder::audRateL2[] = { 32, 48, 56, 64, 80, 96, 112, 128, 
                                        160, 192, 224, 256, 320, 384, 0 };
const char* MpegRecorder::streamType[] = { "MPEG-2 PS", "MPEG-2 TS", 
                                           "MPEG-1 VCD", "PES AV", "", "PES V", 
                                           "", "PES A", "", "", "DVD", "VCD",
                                           "SVCD", "DVD-Special 1", 
                                           "DVD-Special 2", 0 };
const char* MpegRecorder::aspectRatio[] = { "Square", "4:3", "16:9", 
                                            "2.21:1", 0 };

MpegRecorder::MpegRecorder()
            : RecorderBase()
{
    errored = false;
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

    bitrate = 4500;
    maxbitrate = 6000;
    streamtype = 0;
    aspectratio = 2;
    audtype = 2;
    audsamplerate = 48000;
    audbitratel1 = 14;
    audbitratel2 = 14;
    audvolume = 80;

    deviceIsMpegFile = false;
    bufferSize = 256000;
}

MpegRecorder::~MpegRecorder()
{
    if (chanfd >= 0)
        close(chanfd);
    if (readfd >= 0)
        close(readfd);
}

void MpegRecorder::SetOption(const QString &opt, int value)
{
    bool found = false;
    if (opt == "width")
        width = value;
    else if (opt == "height")
        height = value;
    else if (opt == "mpeg2bitrate")
        bitrate = value;
    else if (opt == "mpeg2maxbitrate")
        maxbitrate = value;
    else if (opt == "samplerate")
        audsamplerate = value;
    else if (opt == "mpeg2audbitratel1")
    {
        for (int i = 0; audRateL1[i] != 0; i++)
            if (audRateL1[i] == value)
            {
                audbitratel1 = i + 1;
                found = true;
                break;
            }
        if (! found)
            cerr << "Audiorate(L1): " << value << " is invalid\n";

    }
    else if (opt == "mpeg2audbitratel2")
    {
        for (int i = 0; audRateL2[i] != 0; i++)
            if (audRateL2[i] == value)
            {
                audbitratel2 = i + 1;
                found = true;
                break;
            }
        if (! found)
            cerr << "Audiorate(L2): " << value << " is invalid\n";
    }
    else if (opt == "mpeg2audvolume")
        audvolume = value;
    else
        RecorderBase::SetOption(opt, value);
}

void MpegRecorder::SetOption(const QString &opt, const QString &value)
{
    if (opt == "mpeg2streamtype")
    {
        bool found = false;
        for (unsigned int i = 0; i < sizeof(streamType) / sizeof(char*); i++)
        {
            if (QString(streamType[i]) == value)
            {
                streamtype = i;
                found = true;
                break;
            }
        }

        if (!found)
            cerr << "MPEG2 stream type: " << value << " is invalid\n";
    }
    else if (opt == "mpeg2aspectratio")
    {
        bool found = false;
        for (int i = 0; aspectRatio[i] != 0; i++)
        {
            if (QString(aspectRatio[i]) == value)
            {
                aspectratio = i + 1;
                found = true;
                break;
            }
        }

        if (!found)
            cerr << "MPEG2 Aspect-ratio: " << value << " is invalid\n";
    }
    else if (opt == "mpeg2audtype")
    {
        if (value == "Layer I")
            audtype = 1;
        else if (value == "Layer II")
            audtype = 2;
        else
            cerr << "MPEG2 layer: " << value << " is invalid\n";
    }
    else
        RecorderBase::SetOption(opt, value);

}

void MpegRecorder::SetOptionsFromProfile(RecordingProfile *profile, 
                                         const QString &videodev, 
                                         const QString &audiodev,
                                         const QString &vbidev, int ispip)
{
    (void)audiodev;
    (void)vbidev;

    if (videodev.lower().left(5) == "file:")
    {
        deviceIsMpegFile = true;
        bufferSize = 64000;
        QString newVideoDev = videodev;
        newVideoDev.replace(QRegExp("^file:", false), QString(""));
        SetOption("videodevice", newVideoDev);
    }
    else
    {
        SetOption("videodevice", videodev);
    }

    SetOption("tvformat", gContext->GetSetting("TVFormat"));
    SetOption("vbiformat", gContext->GetSetting("VbiFormat"));

    SetIntOption(profile, "mpeg2bitrate");
    SetIntOption(profile, "mpeg2maxbitrate");
    SetOption("mpeg2streamtype",
              profile->byName("mpeg2streamtype")->getValue());
    SetOption("mpeg2aspectratio",
              profile->byName("mpeg2aspectratio")->getValue());

    SetIntOption(profile, "samplerate");
    SetOption("mpeg2audtype", profile->byName("mpeg2audtype")->getValue());
    SetIntOption(profile, "mpeg2audbitratel1");
    SetIntOption(profile, "mpeg2audbitratel2");
    SetIntOption(profile, "mpeg2audvolume");

    SetIntOption(profile, "width");
    SetIntOption(profile, "height");

    if (ispip)
    {
        SetOption("width", 320);
        SetOption("height", 240);
        SetOption("mpeg2bitrate", 1000);
        SetOption("mpeg2maxbitrate", 2000);
    }
}

bool MpegRecorder::OpenMpegFileAsInput(void)
{
    chanfd = readfd = open(videodevice.ascii(), O_RDONLY);

    if (readfd < 0)
    {
        cerr << "Can't open MPEG File: " << videodevice << endl;
        perror("open mpeg file:");
        return false;
    }
    return true;
}

bool MpegRecorder::OpenV4L2DeviceAsInput(void)
{
    chanfd = open(videodevice.ascii(), O_RDWR);
    if (chanfd < 0)
    {
        cerr << "Can't open video device: " << videodevice << endl;
        perror("open video:");
        return false;
    }

    struct v4l2_format vfmt;
    memset(&vfmt, 0, sizeof(vfmt));

    vfmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if (ioctl(chanfd, VIDIOC_G_FMT, &vfmt) < 0)
    {
        cerr << "Error getting format\n";
        perror("VIDIOC_G_FMT:");
        return false;
    }

    vfmt.fmt.pix.width = width;
    vfmt.fmt.pix.height = height;

    if (ioctl(chanfd, VIDIOC_S_FMT, &vfmt) < 0)
    {
        cerr << "Error setting format\n";
        perror("VIDIOC_S_FMT:");
        return false;
    }

    struct ivtv_ioctl_codec ivtvcodec;
    memset(&ivtvcodec, 0, sizeof(ivtvcodec));

    if (ioctl(chanfd, IVTV_IOC_G_CODEC, &ivtvcodec) < 0)
    {
        cerr << "Error getting codec params\n";
        perror("IVTV_IOC_G_CODEC:");
        return false;
    }

    // only 48kHz works properly.
    int audio_rate = 1;

/*
    int audio_rate;
    if (audsamplerate == 44100)
        audio_rate = 0;
    else if (audsamplerate == 48000)
        audio_rate = 1;
    // 32kHz doesn't seem to work, let's force 44.1kHz instead.
    else if (audsamplerate == 32000)
        audio_rate = 2;
    else
    {
        cerr << "Error setting audio sample rate\n";
        cerr << audsamplerate << " is not a valid sampling rate\n";
        return;
    }
*/

    ivtvcodec.aspect = aspectratio;
    if (audtype == 2)
    {
        if (audbitratel2 < 10)
            audbitratel2 = 10;
        ivtvcodec.audio_bitmask = audio_rate + (audtype << 2) + 
                                  (audbitratel2 << 4);
    }
    else
    {
        if (audbitratel1 < 6)
            audbitratel1 = 6;
        ivtvcodec.audio_bitmask = audio_rate + (audtype << 2) + 
                                  (audbitratel1 << 4);
    }

    ivtvcodec.bitrate = bitrate * 1000;
    ivtvcodec.bitrate_peak = maxbitrate * 1000;

    if (ivtvcodec.bitrate > ivtvcodec.bitrate_peak)
        ivtvcodec.bitrate = ivtvcodec.bitrate_peak;

    // framerate (1 = 25fps, 0 = 30fps)
    ivtvcodec.framerate = (!ntsc_framerate);
    ivtvcodec.stream_type = streamtype;

    if (ioctl(chanfd, IVTV_IOC_S_CODEC, &ivtvcodec) < 0)
    {
        cerr << "Error setting codec params\n";
        perror("IVTV_IOC_S_CODEC:");
        return false;
    }

    struct v4l2_control ctrl;
    ctrl.id = V4L2_CID_AUDIO_VOLUME;
    ctrl.value = 65536 / 100 *audvolume;

    if (ioctl(chanfd, VIDIOC_S_CTRL, &ctrl) < 0)
    {
        cerr << "Error setting codec params\n";
        perror("VIDIOC_S_CTRL:");
        return false;
    }

    struct ivtv_sliced_vbi_format vbifmt;
    if (vbimode) {
        memset(&vbifmt, 0, sizeof(struct ivtv_sliced_vbi_format));
        vbifmt.service_set = (1==vbimode) ? VBI_TYPE_TELETEXT : VBI_TYPE_CC;
        if ((ioctl(chanfd, IVTV_IOC_S_VBI_MODE, &vbifmt) < 0) || 
            (ioctl(chanfd, IVTV_IOC_S_VBI_EMBED, &vbifmt) < 0)) 
        {
            VERBOSE(VB_IMPORTANT, QString("Can't enable VBI recording"));
            perror("vbi");
        }
        ioctl(chanfd, IVTV_IOC_G_VBI_MODE, &vbifmt);
        VERBOSE(VB_RECORD, QString("VBI service:%1, packet size:%2, io size:%3").
                arg(vbifmt.service_set).arg(vbifmt.packet_size).arg(vbifmt.io_size));
    }

    readfd = open(videodevice.ascii(), O_RDWR);
    if (readfd < 0)
    {
        cerr << "Can't open video device: " << videodevice << endl;
        perror("open video:");
        return false;
    }
    return true;
}

bool MpegRecorder::Open(void)
{
    if (deviceIsMpegFile)
        return OpenMpegFileAsInput();
    else
        return OpenV4L2DeviceAsInput();
}

void MpegRecorder::StartRecording(void)
{
    if (!Open())
    {
	errored = true;
	return;
    }

    if (!SetupRecording())
    {
        VERBOSE(VB_IMPORTANT, "Error initializing recording");
	errored = true;
	return;
    }

    encoding = true;
    recording = true;
    unsigned char *buffer = new unsigned char[bufferSize + 1];
    int ret;

    QTime elapsedTimer;
    float elapsed;

    if (deviceIsMpegFile)
        elapsedTimer.start();

    while (encoding)
    {
        if (paused)
        {
            mainpaused = true;
            pauseWait.wakeAll();

            if ((!deviceIsMpegFile) &&
                (readfd >= 0))
            {
                close(readfd);
                readfd = -1;
            }
            usleep(50);
            continue;
        }

        if ((deviceIsMpegFile) && (framesWritten))
        {
            elapsed = (elapsedTimer.elapsed() / 1000.0) + 1;
            while ((framesWritten / elapsed) > 30)
            {
                usleep(50000);
                elapsed = (elapsedTimer.elapsed() / 1000.0) + 1;
            }
        }

        if (readfd < 0)
            readfd = open(videodevice.ascii(), O_RDWR);

        ret = read(readfd, buffer, bufferSize);

        if ((ret == 0) &&
            (deviceIsMpegFile))
        {
            close(readfd);
            readfd = open(videodevice.ascii(), O_RDONLY);

            if (readfd >= 0)
                ret = read(readfd, buffer, bufferSize);
            if (ret <= 0)
            {
                encoding = false;
                continue;
            }
        }
        else if (ret < 0)
        {
            cerr << "error reading from: " << videodevice << endl;
            perror("read");
            continue;
        }
        else if (ret > 0)
            ProcessData(buffer, ret);
    }

    FinishRecording();

    delete[] buffer;
    recording = false;
}

int MpegRecorder::GetVideoFd(void)
{
    return chanfd;
}

// start common code to the dvbrecorder class.

static int mpg_write_packet(void *opaque, uint8_t *buf, int buf_size)
{
    (void)opaque;
    (void)buf;
    (void)buf_size;
    return 0;
}

static int mpg_read_packet(void *opaque, uint8_t *buf, int buf_size)
{
    (void)opaque;
    (void)buf;
    (void)buf_size;
    return 0;
}

static offset_t mpg_seek_packet(void *opaque, int64_t offset, int whence)
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

    ic = av_alloc_format_context();
    if (!ic)
    {
        cerr << "Couldn't allocate context\n";
        return false;
    }

    QString filename = "blah.mpg";
    char *cfilename = (char *)filename.ascii();
    AVFormatParameters params;

    ic->pb.buffer_size = bufferSize + 1;
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

    ic->build_index = 0;

    return true;
}

void MpegRecorder::FinishRecording(void)
{
    if (curRecording)
    {
        curRecording->SetFilesize(ringBuffer->GetRealFileSize());
        if (positionMapDelta.size())
        {
            curRecording->SetPositionMapDelta(positionMapDelta, MARK_GOP_START);
            positionMapDelta.clear();
        }
    }
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
    unsigned char *bufptr = buf;
    unsigned int state = 0xFFFFFFFF, v = 0;

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
        if (pkt.size > 0 && curstream->codec->codec_type == CODEC_TYPE_VIDEO)
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
                startpos += pkt.pos;

                long long keyCount = frameNum / keyframedist;

                if (!positionMap.contains(keyCount))
                {
                    positionMapDelta[keyCount] = startpos;
                    positionMap[keyCount] = startpos;

                    if (curRecording &&
                        ((positionMapDelta.size() % 30) == 0))
                    {
                        curRecording->SetPositionMapDelta(positionMapDelta,
                                MARK_GOP_START);
                        curRecording->SetFilesize(startpos);
                        positionMapDelta.clear();
                    }
                }
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
    errored = false;
    AVPacketList *pktl = NULL;
    while ((pktl = ic->packet_buffer))
    {
        ic->packet_buffer = pktl->next;
        av_free_packet(&pktl->pkt);
        av_free(pktl);
    }

    ic->pb.pos = 0;
    ic->pb.buf_ptr = ic->pb.buffer;
    ic->pb.buf_end = ic->pb.buffer;

    framesWritten = 0;

    positionMap.clear();
    positionMapDelta.clear();

    if (curRecording)
    {
        curRecording->ClearPositionMap(MARK_GOP_START);
    }
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

void MpegRecorder::WaitForPause(void)
{
    if (!mainpaused)
        if (!pauseWait.wait(1000))
            cerr << "Waited too long for recorder to pause\n";
}

bool MpegRecorder::IsRecording(void)
{
    return recording;
}

long long MpegRecorder::GetFramesWritten(void)
{
    return framesWritten;
}

long long MpegRecorder::GetKeyframePosition(long long desired)
{
    long long ret = -1;

    if (positionMap.find(desired) != positionMap.end())
        ret = positionMap[desired];

    return ret;
}
