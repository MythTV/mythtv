#include <stdint.h>

#include <algorithm> // for min
using namespace std;

#include "NuppelVideoRecorder.h"
#include "firewirerecorder.h"
#include "recordingprofile.h"
#include "firewirechannel.h"
#include "importrecorder.h"
#include "dummychannel.h"
#include "hdhrrecorder.h"
#include "iptvrecorder.h"
#include "mpegrecorder.h"
#include "recorderbase.h"
#include "asirecorder.h"
#include "dvbrecorder.h"
#include "hdhrchannel.h"
#include "iptvchannel.h"
#include "mythverbose.h"
#include "programinfo.h"
#include "asichannel.h"
#include "dtvchannel.h"
#include "dvbchannel.h"
#include "v4lchannel.h"
#include "ringbuffer.h"
#include "cardutil.h"
#include "tv_rec.h"
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
    : tvrec(rec),               ringBuffer(NULL),
      weMadeBuffer(true),       videocodec("rtjpeg"),
      ntsc(true),               ntsc_framerate(true),
      video_frame_rate(29.97),
      m_videoAspect(0),         m_videoHeight(0),
      m_videoWidth(0),          m_frameRate(0.0),
      curRecording(NULL),
      request_pause(false),     paused(false),
      request_recording(false), recording(false),
      nextRingBuffer(NULL),     nextRecording(NULL),
      positionMapType(MARK_GOP_BYFRAME)
{
    QMutexLocker locker(avcodeclock);
    avcodec_init(); // init CRC's
}

RecorderBase::~RecorderBase(void)
{
    if (weMadeBuffer && ringBuffer)
    {
        delete ringBuffer;
        ringBuffer = NULL;
    }
    SetRecording(NULL);
}

void RecorderBase::SetRingBuffer(RingBuffer *rbuf)
{
    if (VERBOSE_LEVEL_CHECK(VB_RECORD))
    {
        QString msg("");
        if (rbuf)
            msg = " '" + rbuf->GetFilename() + "'";
        VERBOSE(VB_RECORD,  LOC + QString("SetRingBuffer(0x%1)")
                .arg((uint64_t)rbuf,0,16) + msg);
    }
    ringBuffer = rbuf;
    weMadeBuffer = false;
}

void RecorderBase::SetRecording(const ProgramInfo *pginfo)
{
    if (pginfo)
        VERBOSE(VB_RECORD, LOC + QString("SetRecording(0x%1) title(%2)")
                .arg((uint64_t)pginfo,0,16).arg(pginfo->GetTitle()));
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
    else if (name == "videodevice")
        videodevice = value;
    else if (name == "tvformat")
    {
        ntsc = false;
        if (value.toLower() == "ntsc" || value.toLower() == "ntsc-jp")
        {
            ntsc = true;
            SetFrameRate(29.97);
        }
        else if (value.toLower() == "pal-m")
            SetFrameRate(29.97);
        else if (value.toLower() == "atsc")
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
    else
    {
        VERBOSE(VB_GENERAL, LOC_WARN +
                QString("SetOption(%1,%2): Option not recognized")
                .arg(name).arg(value));
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

/** \brief StopRecording() signals to the StartRecording() function that
 *         it should stop recording and exit cleanly.
 *
 *   This function should block until StartRecording() has finished up.
 */
void RecorderBase::StopRecording(void)
{
    QMutexLocker locker(&pauseLock);
    request_recording = false;
    unpauseWait.wakeAll();
    while (recording)
    {
        recordingWait.wait(&pauseLock, 100);
        if (request_recording)
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR +
                    "Programmer Error: StartRecording called while we were in StopRecording");
            request_recording = false;
        }
    }
}

/// \brief Tells whether the StartRecorder() loop is running.
bool RecorderBase::IsRecording(void)
{
    QMutexLocker locker(&pauseLock);
    return recording;
}

/// \brief Tells us if StopRecording() has been called.
bool RecorderBase::IsRecordingRequested(void)
{
    QMutexLocker locker(&pauseLock);
    return request_recording;
}

/** \brief Pause tells StartRecording() to pause, it should not block.
 *
 *   Once paused the recorder calls tvrec->RecorderPaused().
 *
 *  \param clear if true any generated timecodes should be reset.
 *  \sa Unpause(), WaitForPause()
 */
void RecorderBase::Pause(bool clear)
{
    (void) clear;
    QMutexLocker locker(&pauseLock);
    request_pause = true;
}

/** \brief Unpause tells StartRecording() to unpause.
 *  This is an asynchronous call it should not wait block waiting
 *  for the command to be processed.
 */
void RecorderBase::Unpause(void)
{
    QMutexLocker locker(&pauseLock);
    request_pause = false;
    unpauseWait.wakeAll();
}

/// \brief Returns true iff recorder is paused.
bool RecorderBase::IsPaused(bool holding_lock) const
{
    if (!holding_lock)
        pauseLock.lock();
    bool ret = paused;
    if (!holding_lock)
        pauseLock.unlock();
    return ret;
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

    QMutexLocker locker(&pauseLock);
    while (!IsPaused(true) && request_pause)
    {
        int wait = timeout - t.elapsed();
        if (wait <= 0)
            return false;
        pauseWait.wait(&pauseLock, wait);
    }
    return true;
}

/** \fn RecorderBase::PauseAndWait(int)
 *  \brief If request_pause is true, sets pause and blocks up to
 *         timeout milliseconds or until unpaused, whichever is
 *         sooner.
 *
 *  This is the where we actually do the pausing. For most recorders
 *  that need to do something special on pause, this is the method
 *  to overide.
 *
 *  \param timeout number of milliseconds to wait defaults to 100.
 *  \return true if recorder is paused.
 */
bool RecorderBase::PauseAndWait(int timeout)
{
    QMutexLocker locker(&pauseLock);
    if (request_pause)
    {
        if (!IsPaused(true))
        {
            paused = true;
            pauseWait.wakeAll();
            if (tvrec)
                tvrec->RecorderPaused();
        }

        unpauseWait.wait(&pauseLock, timeout);
    }

    if (!request_pause && IsPaused(true))
    {
        paused = false;
        unpauseWait.wakeAll();
    }

    return IsPaused(true);
}

void RecorderBase::CheckForRingBufferSwitch(void)
{
    nextRingBufferLock.lock();

    bool rb_changed = false;

    if (nextRingBuffer)
    {
        FinishRecording();
        ResetForNewFile();

        m_videoAspect = m_videoWidth = m_videoHeight = 0;
        m_frameRate = 0.0;

        SetRingBuffer(nextRingBuffer);
        SetRecording(nextRecording);

        nextRingBuffer = NULL;
        nextRecording = NULL;

        rb_changed = true;

        StartNewFile();
    }
    nextRingBufferLock.unlock();

    if (rb_changed && tvrec)
        tvrec->RingBufferChanged(ringBuffer, curRecording);
}

int64_t RecorderBase::GetKeyframePosition(uint64_t desired) const
{
    QMutexLocker locker(&positionMapLock);
    long long ret = -1;

    if (positionMap.empty())
        return ret;

    // find closest exact or previous keyframe position...
    frm_pos_map_t::const_iterator it = positionMap.lowerBound(desired);
    if (it == positionMap.end())
        ret = *positionMap.begin();
    else if (it.key() == desired)
        ret = *it;
    else if (--it != positionMap.end())
        ret = *it;

    return ret;
}

bool RecorderBase::GetKeyframePositions(
    int64_t start, int64_t end, frm_pos_map_t &map) const
{
    map.clear();

    QMutexLocker locker(&positionMapLock);
    if (positionMap.empty())
        return true;

    frm_pos_map_t::const_iterator it = positionMap.lowerBound(start);
    end = (end < 0) ? INT64_MAX : end;
    for (; (it != positionMap.end()) &&
             (it.key() <= (uint64_t)end); ++it)
        map[it.key()] = *it;

    VERBOSE(VB_IMPORTANT, LOC +
            QString("GetKeyframePositions(%1,%2,#%3) out of %4")
            .arg(start).arg(end).arg(map.size()).arg(positionMap.size()));

    return true;
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

    uint delta_size = positionMapDelta.size();
    uint pm_elapsed = positionMapTimer.elapsed();
    // save on every 1.5 seconds if in the first few frames of a recording
    needToSave |= (positionMap.size() < 30) &&
        (delta_size >= 1) && (pm_elapsed >= 1500);
    // save every 10 seconds later on
    needToSave |= (delta_size >= 1) && (pm_elapsed >= 10000);

    if (curRecording && needToSave)
    {
        positionMapTimer.start();
        if (delta_size)
        {
            // copy the delta map because most times we are called it will be in
            // another thread and we don't want to lock the main recorder thread
            // which is populating the delta map
            frm_pos_map_t deltaCopy(positionMapDelta);
            positionMapDelta.clear();
            positionMapLock.unlock();

            curRecording->SavePositionMapDelta(deltaCopy, positionMapType);
        }
        else
        {
            positionMapLock.unlock();
        }

        if (ringBuffer)
        {
            curRecording->SaveFilesize(ringBuffer->GetWritePosition());
        }
    }
    else
    {
        positionMapLock.unlock();
    }
}

void RecorderBase::AspectChange(uint aspect, long long frame)
{
    MarkTypes mark = MARK_ASPECT_4_3;
    uint customAspect = 0;
    if (aspect == ASPECT_1_1 && m_videoHeight)
    {
        mark = MARK_ASPECT_CUSTOM;
        customAspect = m_videoWidth * 1000000 / m_videoHeight;
    }
    if (aspect == ASPECT_4_3)
        mark = MARK_ASPECT_4_3;
    if (aspect == ASPECT_16_9)
        mark = MARK_ASPECT_16_9;
    if (aspect == ASPECT_2_21_1)
        mark = MARK_ASPECT_2_21_1;
    if (aspect > ASPECT_CUSTOM)
    {
        mark = MARK_ASPECT_CUSTOM;
        customAspect = aspect;
    }

    if (curRecording)
        curRecording->SaveAspect(frame, mark, customAspect);
}

void RecorderBase::ResolutionChange(uint width, uint height, long long frame)
{
    if (curRecording)
        curRecording->SaveResolution(frame, width, height);
}

void RecorderBase::FrameRateChange(uint framerate, long long frame)
{
    if (curRecording)
        curRecording->SaveFrameRate(frame, framerate);
}

void RecorderBase::SetDuration(uint64_t duration)
{
    if (curRecording)
        curRecording->SaveTotalDuration(duration);
}



RecorderBase *RecorderBase::CreateRecorder(
    TVRec                  *tvrec,
    ChannelBase            *channel,
    const RecordingProfile &profile,
    const GeneralDBOptions &genOpt,
    const DVBDBOptions     &dvbOpt)
{
    if (!channel)
        return NULL;

    RecorderBase *recorder = NULL;
    if (genOpt.cardtype == "MPEG")
    {
#ifdef USING_IVTV
        recorder = new MpegRecorder(tvrec);
#endif // USING_IVTV
    }
    else if (genOpt.cardtype == "HDPVR")
    {
#ifdef USING_HDPVR
        recorder = new MpegRecorder(tvrec);
#endif // USING_HDPVR
    }
    else if (genOpt.cardtype == "FIREWIRE")
    {
#ifdef USING_FIREWIRE
        recorder = new FirewireRecorder(
            tvrec, dynamic_cast<FirewireChannel*>(channel));
#endif // USING_FIREWIRE
    }
    else if (genOpt.cardtype == "HDHOMERUN")
    {
#ifdef USING_HDHOMERUN
        recorder = new HDHRRecorder(
            tvrec, dynamic_cast<HDHRChannel*>(channel));
        recorder->SetOption("wait_for_seqstart", genOpt.wait_for_seqstart);
#endif // USING_HDHOMERUN
    }
    else if (genOpt.cardtype == "DVB")
    {
#ifdef USING_DVB
        recorder = new DVBRecorder(
            tvrec, dynamic_cast<DVBChannel*>(channel));
        recorder->SetOption("wait_for_seqstart", genOpt.wait_for_seqstart);
        recorder->SetOption("dvb_on_demand",     dvbOpt.dvb_on_demand);
#endif // USING_DVB
    }
    else if (genOpt.cardtype == "FREEBOX")
    {
#ifdef USING_IPTV
        recorder = new IPTVRecorder(
            tvrec, dynamic_cast<IPTVChannel*>(channel));
        recorder->SetOption("mrl", genOpt.videodev);
#endif // USING_IPTV
    }
    else if (genOpt.cardtype == "ASI")
    {
#ifdef USING_ASI
        recorder = new ASIRecorder(
            tvrec, dynamic_cast<ASIChannel*>(channel));
        recorder->SetOption("wait_for_seqstart", genOpt.wait_for_seqstart);
#endif // USING_ASI
    }
    else if (genOpt.cardtype == "IMPORT")
    {
        recorder = new ImportRecorder(tvrec);
    }
    else if (genOpt.cardtype == "DEMO")
    {
#ifdef USING_IVTV
        recorder = new MpegRecorder(tvrec);
#else
        recorder = new ImportRecorder(tvrec);
#endif
    }
    else if (CardUtil::IsV4L(genOpt.cardtype))
    {
#ifdef USING_V4L
        // V4L/MJPEG/GO7007 from here on
        recorder = new NuppelVideoRecorder(tvrec, channel);
        recorder->SetOption("skipbtaudio", genOpt.skip_btaudio);
#endif // USING_V4L
    }

    if (recorder)
    {
        recorder->SetOptionsFromProfile(
            const_cast<RecordingProfile*>(&profile),
            genOpt.videodev, genOpt.audiodev, genOpt.vbidev);
        // Override the samplerate defined in the profile if this card
        // was configured with a fixed rate.
        if (genOpt.audiosamplerate)
            recorder->SetOption("samplerate", genOpt.audiosamplerate);
    }
    else
    {
        QString msg = "Need %1 recorder, but compiled without %2 support!";
        msg = msg.arg(genOpt.cardtype).arg(genOpt.cardtype);
        VERBOSE(VB_IMPORTANT, "RecorderBase::CreateRecorder() Error, " + msg);
    }

    return recorder;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
