#include <iostream>
using namespace std;

#include "recorderbase.h"

#include "RingBuffer.h"
#include "programinfo.h"
#include "recordingprofile.h"

RecorderBase::RecorderBase(void)
{
    ringBuffer = NULL;
    weMadeBuffer = true;

    codec = "rtjpeg";
    audiodevice = "/dev/dsp";
    videodevice = "/dev/video";
    vbidevice = "/dev/vbi";

    ntsc = 1;
    ntsc_framerate = 1;
    video_frame_rate = 29.97;
    vbimode = 0;

    curRecording = NULL;
    curChannelName = "";
}

RecorderBase::~RecorderBase(void)
{
    if (weMadeBuffer)
        delete ringBuffer;
    if (curRecording)
        delete curRecording;
}

void RecorderBase::SetRingBuffer(RingBuffer *rbuf)
{
    ringBuffer = rbuf;
    weMadeBuffer = false;
}

void RecorderBase::SetRecording(ProgramInfo *pginfo)
{
    if (curRecording)
    {
        delete curRecording;
        curRecording = NULL;
    }

    if (pginfo)
    {
        curRecording = new ProgramInfo(*pginfo);
    }
}

void RecorderBase::SetOption(const QString &name, const QString &value)
{
    if (name == "codec")
        codec = value;
    else if (name == "audiodevice")
        audiodevice = value;
    else if (name == "videodevice")
        videodevice = value;
    else if (name == "vbidevice")
        vbidevice = value;
    else if (name == "tvformat")
    {
        if (value.lower() == "ntsc" || value.lower() == "ntsc-jp")
        {
            ntsc = 1;
            ntsc_framerate = 1;
            video_frame_rate = 29.97;
        }
        else if (value.lower() == "pal-m")
        {
            ntsc = 0;
            ntsc_framerate = 1;
            video_frame_rate = 29.97;
        }
        else
        {
            ntsc = 0;
            ntsc_framerate = 0;
            video_frame_rate = 25.0;
        }
    }
    else if (name == "vbiformat")
    {
        if (value.lower() == "pal teletext")
            vbimode = 1;
        else if (value.lower().left(4) == "ntsc")
            vbimode = 2;
        else
            vbimode = 0;
    }
}

void RecorderBase::SetOption(const QString &name, int value)
{
    cerr << "Unknown int option: " << name << ": " << value << endl;
}

void RecorderBase::ChannelNameChanged(const QString& new_name)
{
    curChannelName = new_name;
}

QString RecorderBase::GetCurChannelName() const
{
    return curChannelName;
}

void RecorderBase::SetIntOption(RecordingProfile *profile, const QString &name)
{
    int value = profile->byName(name)->getValue().toInt();
    SetOption(name, value);
}

