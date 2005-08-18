#include <iostream>
using namespace std;

#include "recorderbase.h"

#include "mythcontext.h"
#include "RingBuffer.h"
#include "programinfo.h"
#include "recordingprofile.h"

RecorderBase::RecorderBase(void)
    : ringBuffer(NULL), weMadeBuffer(true), codec("rtjpeg"),
      audiodevice("/dev/dsp"), videodevice("/dev/video"), vbidevice("/dev/vbi"),
      vbimode(0), ntsc(true), ntsc_framerate(true), video_frame_rate(29.97),
      curRecording(NULL), curChannelName("")
{
}

RecorderBase::~RecorderBase(void)
{
    if (weMadeBuffer && ringBuffer)
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
    VERBOSE(VB_RECORD, "SetRecording(0x"<<pginfo<<")");
    if (pginfo && pginfo->title)
        VERBOSE(VB_IMPORTANT, "Prog title: "<<pginfo->title);

    ProgramInfo *oldrec = curRecording;
    if (pginfo)
        curRecording = new ProgramInfo(*pginfo);
    else
        curRecording = NULL;

    if (oldrec)
        delete oldrec;
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
        ntsc = false;
        if (value.lower() == "ntsc" || value.lower() == "ntsc-jp")
        {
            ntsc = true;
            SetFrameRate(29.97);
        }
        else if (value.lower() == "pal-m")
            SetFrameRate(29.97);
        else if (value.lower() == "atsc")
        {
            // Here we set the TV format values for ATSC. ATSC isn't really 
            // NTSC, but users who configure a non-ATSC-recorder as ATSC
            // are far more likely to be using a mix of ATSC and NTSC than
            // a mix of ATSC and PAL or SECAM. The atsc recorder itself
            // does not care about these values, except in so much as tv_rec
            // cares anout video_frame_rate which should be neither 29.97 
            // nor 25.0, but based on the actual video.
            ntsc = true;
            SetFrameRate(29.97);
        }
        else
            SetFrameRate(25.00);
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
    VERBOSE(VB_IMPORTANT,
            QString("RecorderBase::SetOption(): Unknown int option: %1: %2")
            .arg(name).arg(value));
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
    SetOption(name, profile->byName(name)->getValue().toInt());
}

