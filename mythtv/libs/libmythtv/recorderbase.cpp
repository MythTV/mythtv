#include <iostream>
using namespace std;

#include "recorderbase.h"
#include "tv_rec.h"
#include "mythcontext.h"
#include "RingBuffer.h"
#include "programinfo.h"
#include "recordingprofile.h"
#include "util.h"

#define TVREC_CARDNUM \
        ((tvrec != NULL) ? QString::number(tvrec->GetCaptureCardNum()) : "NULL")

#define LOC      QString("RecBase(%1:%2): ") \
                 .arg(TVREC_CARDNUM).arg(videodevice)
#define LOC_WARN QString("RecBase(%1:%2) Warning: ") \
                 .arg(TVREC_CARDNUM).arg(videodevice)
#define LOC_ERR  QString("RecBase(%1:%2) Error: ") \
                 .arg(TVREC_CARDNUM).arg(videodevice)

RecorderBase::RecorderBase(TVRec *rec)
    : tvrec(rec), ringBuffer(NULL), weMadeBuffer(true), videocodec("rtjpeg"),
      audiodevice("/dev/dsp"), videodevice("/dev/video"), vbidevice("/dev/vbi"),
      vbimode(0), ntsc(true), ntsc_framerate(true), video_frame_rate(29.97),
      curRecording(NULL), request_pause(false), paused(false),
      nextRingBuffer(NULL), nextRecording(NULL),
      positionMapType(MARK_GOP_BYFRAME), positionMapLock(false)
{
    QMutexLocker locker(&avcodeclock);
    avcodec_init(); // init CRC's
}

RecorderBase::~RecorderBase(void)
{
    if (weMadeBuffer && ringBuffer)
    {
        delete ringBuffer;
        ringBuffer = NULL;
    }
    if (curRecording)
    {
        delete curRecording;
        curRecording = NULL;
    }
}

void RecorderBase::SetRingBuffer(RingBuffer *rbuf)
{
    if (print_verbose_messages & VB_RECORD)
    {
        QString msg("");
        if (rbuf)
            msg = " '" + rbuf->GetFilename() + "'";
        VERBOSE(VB_RECORD,  LOC + "SetRingBuffer("<<rbuf<<")"<<msg);
    }
    ringBuffer = rbuf;
    weMadeBuffer = false;
}

void RecorderBase::SetRecording(const ProgramInfo *pginfo)
{
    if (pginfo)
        VERBOSE(VB_RECORD, LOC + "SetRecording(" << pginfo
                << QString(") title(%1)").arg(pginfo->title));
    else
        VERBOSE(VB_RECORD, LOC + "SetRecording(0x0)");

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
    if (name == "videocodec")
        videocodec = value;
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
    VERBOSE(VB_IMPORTANT, LOC_ERR +
            QString("SetOption(): Unknown int option: %1: %2")
            .arg(name).arg(value));
}

void RecorderBase::SetIntOption(RecordingProfile *profile, const QString &name)
{
    const Setting *setting = profile->byName(name);
    if (setting)
        SetOption(name, setting->getValue().toInt());
    else
        VERBOSE(VB_IMPORTANT, LOC_ERR + QString(
                    "SetIntOption(...%1): Option not in profile.").arg(name));
}

void RecorderBase::SetStrOption(RecordingProfile *profile, const QString &name)
{
    const Setting *setting = profile->byName(name);
    if (setting)
        SetOption(name, setting->getValue());
    else
        VERBOSE(VB_IMPORTANT, LOC_ERR + QString(
                    "SetStrOption(...%1): Option not in profile.").arg(name));
}

/** \fn RecorderBase::WaitForPause(int)
 *  \brief WaitForPause blocks until StartRecording() is actually paused,
 *         or timeout milliseconds elapse.
 *  \param timeout number of milliseconds to wait defaults to 1000.
 *  \return true iff pause happened within timeout period.
 */
bool RecorderBase::WaitForPause(int timeout)
{
    MythTimer t;
    t.start();

    while (true)
    {
        int wait = timeout - t.elapsed();

        if (wait <= 0)
            return IsPaused();
        else if (IsPaused())
            return true;

        pauseWait.wait(wait);
    }
}

/** \fn RecorderBase::PauseAndWait(int)
 *  \brief If request_pause is true Paused and blocks up to timeout 
 *         milliseconds.
 *  \param timeout number of milliseconds to wait defaults to 100.
 *  \return true if recorder is paused.
 */
bool RecorderBase::PauseAndWait(int timeout)
{
    if (request_pause)
    {
        if (!paused)
        {
            paused = true;
            pauseWait.wakeAll();
            if (tvrec)
                tvrec->RecorderPaused();
        }
        unpauseWait.wait(timeout);
    }
    if (!request_pause)
        paused = false;
    return paused;
}

void RecorderBase::CheckForRingBufferSwitch(void)
{
    nextRingBufferLock.lock();

    bool rb_changed = false;

    if (nextRingBuffer)
    {
        FinishRecording();
        ResetForNewFile();

        if (weMadeBuffer && ringBuffer)
            delete ringBuffer;
        SetRingBuffer(nextRingBuffer);
        nextRingBuffer = NULL;

        ProgramInfo *oldrec = curRecording;
        curRecording        = nextRecording;
        nextRecording       = NULL;
        if (oldrec)
            delete oldrec;
        rb_changed = true;

        StartNewFile();
    }
    nextRingBufferLock.unlock();

    if (rb_changed && tvrec)
        tvrec->RingBufferChanged(ringBuffer, curRecording);
}

/** \fn RecorderBase::SavePositionMap(bool)
 *  \brief This saves the postition map delta to the database if force
 *         is true or there are 30 frames in the map or there are five
 *         frames in the map with less than 30 frames in the non-delta
 *         position map.
 *  \param force If true this forces a DB sync.
 */
void RecorderBase::SavePositionMap(bool force)
{
    bool needToSave = force;
    positionMapLock.lock();

    // save on every 5th key frame if in the first few frames of a recording
    needToSave |= (positionMap.size() < 30) && (positionMap.size() % 5 == 1);
    // save every 30th key frame later on
    needToSave |= positionMapDelta.size() >= 30;

    if (curRecording && needToSave)
    {
        // copy the delta map because most times we are called it will be in
        // another thread and we don't want to lock the main recorder thread
        // which is populating the delta map
        QMap<long long, long long> deltaCopy(positionMapDelta);
        positionMapDelta.clear();
        positionMapLock.unlock();

        curRecording->SetPositionMapDelta(deltaCopy, positionMapType);

        if (ringBuffer)
            curRecording->SetFilesize(ringBuffer->GetWritePosition());
    }
    else
        positionMapLock.unlock();
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
