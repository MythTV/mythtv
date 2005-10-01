#include <iostream>
#include <pthread.h>
#include <cstdio>
#include <cstdlib>
#include <cerrno>
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
    bufferSize = 4096;
}

MpegRecorder::~MpegRecorder()
{
    TeardownAll();
}

void MpegRecorder::deleteLater(void)
{
    TeardownAll();
    RecorderBase::deleteLater();
}

void MpegRecorder::TeardownAll(void)
{
    if (chanfd >= 0)
    {
        close(chanfd);
        chanfd = -1;
    }
    if (readfd >= 0)
    {
        close(readfd);
        readfd = -1;
    }
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

    if (ivtvcodec.framerate)
        keyframedist = 12;

    struct v4l2_control ctrl;
    ctrl.id = V4L2_CID_AUDIO_VOLUME;
    ctrl.value = 65536 / 100 *audvolume;

    if (ioctl(chanfd, VIDIOC_S_CTRL, &ctrl) < 0)
    {
        cerr << "Error setting codec params\n";
        perror("VIDIOC_S_CTRL:");
        return false;
    }

    if (vbimode) {
        struct ivtv_sliced_vbi_format vbifmt;
        memset(&vbifmt, 0, sizeof(struct ivtv_sliced_vbi_format));
        vbifmt.service_set = (1==vbimode) ? VBI_TYPE_TELETEXT : VBI_TYPE_CC;
        if (ioctl(chanfd, IVTV_IOC_S_VBI_MODE, &vbifmt) < 0) 
        {
            VERBOSE(VB_IMPORTANT, QString("Can't enable VBI recording"));
            perror("vbi");
        }

        int embedon = 1;
        if (ioctl(chanfd, IVTV_IOC_S_VBI_EMBED, &embedon) < 0)
        {
            VERBOSE(VB_IMPORTANT, QString("Can't enable VBI recording"));
            perror("vbi");
        }

        ioctl(chanfd, IVTV_IOC_G_VBI_MODE, &vbifmt);
        VERBOSE(VB_RECORD, QString("VBI service:%1, packet size:%2, io size:%3").
                arg(vbifmt.service_set).arg(vbifmt.packet_size).arg(vbifmt.io_size));
    }

    readfd = open(videodevice.ascii(), O_RDWR | O_NONBLOCK);
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

    MythTimer elapsedTimer;
    float elapsed;

    struct timeval tv;
    fd_set rdset;

    if (deviceIsMpegFile)
        elapsedTimer.start();

    while (encoding)
    {
        if (PauseAndWait(100))
            continue;

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

        tv.tv_sec = 5;
        tv.tv_usec = 0;
        FD_ZERO(&rdset);
        FD_SET(readfd, &rdset);

        switch (select(readfd + 1, &rdset, NULL, NULL, &tv))
        {
            case -1:
                if (errno == EINTR)
                    continue;
                perror("select");
                continue;
            case 0:
                printf("select timeout\n");
                continue;
           default: break;
        }

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
        else if (ret < 0 && errno != EAGAIN)
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

bool MpegRecorder::SetupRecording(void)
{
    leftovers = 0xFFFFFFFF;
    numgops = 0;
    lastseqstart = 0;
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

#define PACK_HEADER   0x000001BA
#define GOP_START     0x000001B8
#define SEQ_START     0x000001B3
#define SLICE_MIN     0x00000101
#define SLICE_MAX     0x000001af

void MpegRecorder::ProcessData(unsigned char *buffer, int len)
{
    unsigned char *bufptr = buffer;
    unsigned int state = leftovers, v = 0;

    while (bufptr < buffer + len)
    {
        v = *bufptr++;
        if (state == 0x000001)
        {
            state = ((state << 8) | v) & 0xFFFFFF;
            
            if (state == PACK_HEADER)
            {
                long long startpos = ringBuffer->GetFileWritePosition();
                startpos += bufptr - buffer - 4;
                lastpackheaderpos = startpos;
            }

            if (state == SEQ_START)
            {
                lastseqstart = lastpackheaderpos;
            }

            if (state == GOP_START && lastseqstart == lastpackheaderpos)
            {
                framesWritten = numgops * keyframedist;
                numgops++;

                if (!positionMap.contains(numgops))
                {
                    positionMapDelta[numgops] = lastpackheaderpos;
                    positionMap[numgops] = lastpackheaderpos;

                    if (curRecording && ((positionMapDelta.size() % 30) == 0))
                    {
                        curRecording->SetPositionMapDelta(positionMapDelta,
                                                          MARK_GOP_START);
                        curRecording->SetFilesize(lastpackheaderpos);
                        positionMapDelta.clear();
                    }
                }
            }
        }

        state = ((state << 8) | v) & 0xFFFFFF;
    }

    leftovers = state;

    ringBuffer->Write(buffer, len);
}

void MpegRecorder::StopRecording(void)
{
    encoding = false;
}

void MpegRecorder::Reset(void)
{
    errored = false;
    framesWritten = 0;
    numgops = 0;
    lastseqstart = 0;

    leftovers = 0xFFFFFFFF;

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
    paused = false;
    request_pause = true;
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
