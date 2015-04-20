#include <stdint.h>

#include <algorithm> // for min
using namespace std;

#include "NuppelVideoRecorder.h"
#include "firewirerecorder.h"
#include "recordingprofile.h"
#include "firewirechannel.h"
#include "importrecorder.h"
#include "cetonrecorder.h"
#include "dummychannel.h"
#include "hdhrrecorder.h"
#include "iptvrecorder.h"
#include "mpegrecorder.h"
#include "recorderbase.h"
#include "cetonchannel.h"
#include "asirecorder.h"
#include "dvbrecorder.h"
#include "ExternalRecorder.h"
#include "hdhrchannel.h"
#include "iptvchannel.h"
#include "mythsystemevent.h"
#include "mythlogging.h"
#include "programinfo.h"
#include "asichannel.h"
#include "dtvchannel.h"
#include "dvbchannel.h"
#include "v4lchannel.h"
#include "ExternalChannel.h"
#include "ringbuffer.h"
#include "cardutil.h"
#include "tv_rec.h"
#include "mythdate.h"

#define TVREC_CARDNUM \
        ((tvrec != NULL) ? QString::number(tvrec->GetCaptureCardNum()) : "NULL")

#define LOC QString("RecBase[%1](%2): ") \
            .arg(TVREC_CARDNUM).arg(videodevice)

const uint RecorderBase::kTimeOfLatestDataIntervalTarget = 5000;

RecorderBase::RecorderBase(TVRec *rec)
    : tvrec(rec),               ringBuffer(NULL),
      weMadeBuffer(true),
      m_containerFormat(formatUnknown),
      m_primaryVideoCodec(AV_CODEC_ID_NONE),
      m_primaryAudioCodec(AV_CODEC_ID_NONE),
      videocodec("rtjpeg"),
      ntsc(true),               ntsc_framerate(true),
      video_frame_rate(29.97),
      m_videoAspect(0),         m_videoHeight(0),
      m_videoWidth(0),          m_frameRate(0),
      curRecording(NULL),
      request_pause(false),     paused(false),
      request_recording(false), recording(false),
      nextRingBuffer(NULL),     nextRecording(NULL),
      positionMapType(MARK_GOP_BYFRAME),
      estimatedProgStartMS(0), lastSavedKeyframe(0), lastSavedDuration(0)
{
    ClearStatistics();
    QMutexLocker locker(avcodeclock);
#if 0
    avcodec_init(); // init CRC's
#endif
}

RecorderBase::~RecorderBase(void)
{
    if (weMadeBuffer && ringBuffer)
    {
        delete ringBuffer;
        ringBuffer = NULL;
    }
    SetRecording(NULL);
    if (nextRingBuffer)
    {
        delete nextRingBuffer;
        nextRingBuffer = NULL;
    }
    if (nextRecording)
    {
        delete nextRecording;
        nextRecording = NULL;
    }
}

void RecorderBase::SetRingBuffer(RingBuffer *rbuf)
{
    if (VERBOSE_LEVEL_CHECK(VB_RECORD, LOG_INFO))
    {
        QString msg("");
        if (rbuf)
            msg = " '" + rbuf->GetFilename() + "'";
        LOG(VB_RECORD, LOG_INFO, LOC + QString("SetRingBuffer(0x%1)")
                .arg((uint64_t)rbuf,0,16) + msg);
    }
    ringBuffer = rbuf;
    weMadeBuffer = false;
}

void RecorderBase::SetRecording(const RecordingInfo *pginfo)
{
    if (pginfo)
        LOG(VB_RECORD, LOG_INFO, LOC + QString("SetRecording(0x%1) title(%2)")
                .arg((uint64_t)pginfo,0,16).arg(pginfo->GetTitle()));
    else
        LOG(VB_RECORD, LOG_INFO, LOC + "SetRecording(0x0)");

    ProgramInfo *oldrec = curRecording;
    if (pginfo)
    {
        // NOTE: RecorderBase and TVRec do not share a single RecordingInfo
        //       instance which may lead to the possibility that changes made
        //       in the database by one are overwritten by the other
        curRecording = new RecordingInfo(*pginfo);
        // Compute an estimate of the actual progstart delay for setting the
        // MARK_UTIL_PROGSTART mark.  We can't reliably use
        // curRecording->GetRecordingStartTime() because the scheduler rounds it
        // to the nearest minute, so we use the current time instead.
        estimatedProgStartMS =
            MythDate::current().msecsTo(curRecording->GetScheduledStartTime());
        RecordingFile *recFile = curRecording->GetRecordingFile();
        recFile->m_containerFormat = m_containerFormat;
        recFile->Save();
    }
    else
        curRecording = NULL;

    if (oldrec)
        delete oldrec;
}

void RecorderBase::SetNextRecording(const RecordingInfo *ri, RingBuffer *rb)
{
    LOG(VB_RECORD, LOG_INFO, LOC + QString("SetNextRecording(0x%1, 0x%2)")
        .arg(reinterpret_cast<intptr_t>(ri),0,16)
        .arg(reinterpret_cast<intptr_t>(rb),0,16));

    // First we do some of the time consuming stuff we can do now
    SavePositionMap(true);
    if (ringBuffer)
    {
        ringBuffer->WriterFlush();
        if (curRecording)
            curRecording->SaveFilesize(ringBuffer->GetRealFileSize());
    }

    // Then we set the next info
    QMutexLocker locker(&nextRingBufferLock);
    if (nextRecording)
    {
        delete nextRecording;
        nextRecording = NULL;
    }
    if (ri)
        nextRecording = new RecordingInfo(*ri);

    if (nextRingBuffer)
        delete nextRingBuffer;
    nextRingBuffer = rb;
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
        LOG(VB_GENERAL, LOG_WARNING, LOC +
            QString("SetOption(%1,%2): Option not recognized")
                .arg(name).arg(value));
    }
}

void RecorderBase::SetOption(const QString &name, int value)
{
    LOG(VB_GENERAL, LOG_ERR, LOC +
        QString("SetOption(): Unknown int option: %1: %2")
            .arg(name).arg(value));
}

void RecorderBase::SetIntOption(RecordingProfile *profile, const QString &name)
{
    const Setting *setting = profile->byName(name);
    if (setting)
        SetOption(name, setting->getValue().toInt());
    else
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("SetIntOption(...%1): Option not in profile.").arg(name));
}

void RecorderBase::SetStrOption(RecordingProfile *profile, const QString &name)
{
    const Setting *setting = profile->byName(name);
    if (setting)
        SetOption(name, setting->getValue());
    else
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("SetStrOption(...%1): Option not in profile.").arg(name));
}

/** \brief StopRecording() signals to the recorder that
 *         it should stop recording and exit cleanly.
 *
 *   This function should block until recorder has finished up.
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
            LOG(VB_GENERAL, LOG_ERR, LOC +
                "Programmer Error: Recorder started while we were in "
                "StopRecording");
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

/** \brief Pause tells recorder to pause, it should not block.
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

/** \brief Unpause tells recorder to unpause.
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
 *  \brief WaitForPause blocks until recorder is actually paused,
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

    RecordingQuality *recq = NULL;

    if (nextRingBuffer)
    {
        FinishRecording();

        recq = GetRecordingQuality(NULL);

        ResetForNewFile();

        m_videoAspect = m_videoWidth = m_videoHeight = 0;
        m_frameRate = FrameRate(0);

        SetRingBuffer(nextRingBuffer);
        SetRecording(nextRecording);

        nextRingBuffer = NULL;
        nextRecording = NULL;

        StartNewFile();
    }
    nextRingBufferLock.unlock();

    if (recq && tvrec)
        tvrec->RingBufferChanged(ringBuffer, curRecording, recq);
}

void RecorderBase::SetRecordingStatus(RecStatus::Type status,
                                      const QString& file, int line)
{
    if (curRecording && curRecording->GetRecordingStatus() != status)
    {
        LOG(VB_RECORD, LOG_INFO,
            QString("Modifying recording status from %1 to %2 at %3:%4")
            .arg(RecStatus::toString(curRecording->GetRecordingStatus(), kSingleRecord))
            .arg(RecStatus::toString(status, kSingleRecord)).arg(file).arg(line));

        curRecording->SetRecordingStatus(status);

        if (status == RecStatus::Failing)
        {
            curRecording->SaveVideoProperties(VID_DAMAGED, VID_DAMAGED);
            SendMythSystemRecEvent("REC_FAILING", curRecording);
        }

        MythEvent me(QString("UPDATE_RECORDING_STATUS %1 %2 %3 %4 %5")
                    .arg(curRecording->GetInputID())
                    .arg(curRecording->GetChanID())
                    .arg(curRecording->GetScheduledStartTime(MythDate::ISODate))
                    .arg(status)
                    .arg(curRecording->GetRecordingEndTime(MythDate::ISODate)));
        gCoreContext->dispatch(me);
    }
}

void RecorderBase::ClearStatistics(void)
{
    QMutexLocker locker(&statisticsLock);
    timeOfFirstData = QDateTime();
    timeOfFirstDataIsSet.fetchAndStoreRelaxed(0);
    timeOfLatestData = QDateTime();
    timeOfLatestDataCount.fetchAndStoreRelaxed(0);
    timeOfLatestDataPacketInterval.fetchAndStoreRelaxed(2000);
    recordingGaps.clear();
}

void RecorderBase::FinishRecording(void)
{
    if (curRecording)
    {
        if (m_primaryVideoCodec == AV_CODEC_ID_H264)
            curRecording->SaveVideoProperties(VID_AVC, VID_AVC);

        RecordingFile *recFile = curRecording->GetRecordingFile();
        if (recFile)
        {
            // Container
            recFile->m_containerFormat = m_containerFormat;

            // Video
            recFile->m_videoCodec = ff_codec_id_string(m_primaryVideoCodec);
            switch (curRecording->QueryAverageAspectRatio())
            {
                case MARK_ASPECT_1_1 :
                    recFile->m_videoAspectRatio = 1.0;
                    break;
                case MARK_ASPECT_4_3:
                    recFile->m_videoAspectRatio = 1.33333333333;
                    break;
                case MARK_ASPECT_16_9:
                    recFile->m_videoAspectRatio = 1.77777777777;
                    break;
                case MARK_ASPECT_2_21_1:
                    recFile->m_videoAspectRatio = 2.21;
                    break;
                default:
                    recFile->m_videoAspectRatio = (double)m_videoAspect / 1000000.0;
                    break;
            }
            QSize resolution(curRecording->QueryAverageWidth(),
                            curRecording->QueryAverageHeight());
            recFile->m_videoResolution = resolution;
            recFile->m_videoFrameRate = (double)curRecording->QueryAverageFrameRate() / 1000.0;

            // Audio
            recFile->m_audioCodec = ff_codec_id_string(m_primaryAudioCodec);

            recFile->Save();
        }
        else
            LOG(VB_GENERAL, LOG_CRIT, "RecordingFile object is NULL. No video file metadata can be stored");

        SavePositionMap(true, true); // Save Position Map only, not file size

        if (ringBuffer)
            curRecording->SaveFilesize(ringBuffer->GetRealFileSize());
    }

    LOG(VB_GENERAL, LOG_NOTICE, QString("Finished Recording: "
                                        "Container: %7 "
                                        "Video Codec: %1 (%2x%3 A/R: %4 %5fps) "
                                        "Audio Codec: %6")
                                        .arg(avcodec_get_name(m_primaryVideoCodec))
                                        .arg(m_videoWidth)
                                        .arg(m_videoHeight)
                                        .arg(m_videoAspect)
                                        .arg(GetFrameRate())
                                        .arg(avcodec_get_name(m_primaryAudioCodec))
                                        .arg(RecordingFile::AVContainerToString(m_containerFormat)));
}

RecordingQuality *RecorderBase::GetRecordingQuality(
    const RecordingInfo *r) const
{
    QMutexLocker locker(&statisticsLock);
    if (r && curRecording &&
        (r->MakeUniqueKey() == curRecording->MakeUniqueKey()))
    {
        curRecording->SetDesiredStartTime(r->GetDesiredStartTime());
        curRecording->SetDesiredEndTime(r->GetDesiredEndTime());
    }
    return new RecordingQuality(
        curRecording, recordingGaps,
        timeOfFirstData, timeOfLatestData);
}

long long RecorderBase::GetKeyframePosition(long long desired) const
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
    long long start, long long end, frm_pos_map_t &map) const
{
    map.clear();

    QMutexLocker locker(&positionMapLock);
    if (positionMap.empty())
        return true;

    frm_pos_map_t::const_iterator it = positionMap.lowerBound(start);
    end = (end < 0) ? INT64_MAX : end;
    for (; (it != positionMap.end()) &&
             (it.key() <= end); ++it)
        map[it.key()] = *it;

    LOG(VB_GENERAL, LOG_DEBUG, LOC +
        QString("GetKeyframePositions(%1,%2,#%3) out of %4")
            .arg(start).arg(end).arg(map.size()).arg(positionMap.size()));

    return true;
}

bool RecorderBase::GetKeyframeDurations(
    long long start, long long end, frm_pos_map_t &map) const
{
    map.clear();

    QMutexLocker locker(&positionMapLock);
    if (durationMap.empty())
        return true;

    frm_pos_map_t::const_iterator it = durationMap.lowerBound(start);
    end = (end < 0) ? INT64_MAX : end;
    for (; (it != durationMap.end()) &&
             (it.key() <= end); ++it)
        map[it.key()] = *it;

    LOG(VB_GENERAL, LOG_DEBUG, LOC +
        QString("GetKeyframeDurations(%1,%2,#%3) out of %4")
            .arg(start).arg(end).arg(map.size()).arg(durationMap.size()));

    return true;
}

/** \fn RecorderBase::SavePositionMap(bool)
 *  \brief This saves the postition map delta to the database if force
 *         is true or there are 30 frames in the map or there are five
 *         frames in the map with less than 30 frames in the non-delta
 *         position map.
 *  \param force If true this forces a DB sync.
 */
void RecorderBase::SavePositionMap(bool force, bool finished)
{
    bool needToSave = force;
    positionMapLock.lock();

    bool has_delta = !positionMapDelta.empty();
    // set pm_elapsed to a fake large value if the timer hasn't yet started
    uint pm_elapsed = (positionMapTimer.isRunning()) ?
        positionMapTimer.elapsed() : ~0;
    // save on every 1.5 seconds if in the first few frames of a recording
    needToSave |= (positionMap.size() < 30) &&
        has_delta && (pm_elapsed >= 1500);
    // save every 10 seconds later on
    needToSave |= has_delta && (pm_elapsed >= 10000);
    // Assume that durationMapDelta is the same size as
    // positionMapDelta and implicitly use the same logic about when
    // to same durationMapDelta.

    if (curRecording && needToSave)
    {
        positionMapTimer.start();
        if (has_delta)
        {
            // copy the delta map because most times we are called it will be in
            // another thread and we don't want to lock the main recorder thread
            // which is populating the delta map
            frm_pos_map_t deltaCopy(positionMapDelta);
            positionMapDelta.clear();
            frm_pos_map_t durationDeltaCopy(durationMapDelta);
            durationMapDelta.clear();
            positionMapLock.unlock();

            curRecording->SavePositionMapDelta(deltaCopy, positionMapType);
            curRecording->SavePositionMapDelta(durationDeltaCopy,
                                               MARK_DURATION_MS);

            TryWriteProgStartMark(durationDeltaCopy);
        }
        else
        {
            positionMapLock.unlock();
        }

        if (ringBuffer && !finished) // Finished Recording will update the final size for us
        {
            curRecording->SaveFilesize(ringBuffer->GetWritePosition());
        }
    }
    else
    {
        positionMapLock.unlock();
    }
}

void RecorderBase::TryWriteProgStartMark(const frm_pos_map_t &durationDeltaCopy)
{
    // Note: all log strings contain "progstart mark" for searching.
    if (estimatedProgStartMS <= 0)
    {
        // Do nothing because no progstart mark is needed.
        LOG(VB_RECORD, LOG_DEBUG,
            QString("No progstart mark needed because delta=%1")
            .arg(estimatedProgStartMS));
        return;
    }
    frm_pos_map_t::const_iterator last_it = durationDeltaCopy.end();
    --last_it;
    long long bookmarkFrame = 0;
    LOG(VB_RECORD, LOG_DEBUG,
        QString("durationDeltaCopy.begin() = (%1,%2)")
        .arg(durationDeltaCopy.begin().key())
        .arg(durationDeltaCopy.begin().value()));
    if (estimatedProgStartMS > *last_it)
    {
        // Do nothing because we haven't reached recstartts yet.
        LOG(VB_RECORD, LOG_DEBUG,
            QString("No progstart mark yet because estimatedProgStartMS=%1 "
                    "and *last_it=%2")
            .arg(estimatedProgStartMS).arg(*last_it));
    }
    else if (lastSavedDuration <= estimatedProgStartMS &&
             estimatedProgStartMS < *durationDeltaCopy.begin())
    {
        // Set progstart mark @ lastSavedKeyframe
        LOG(VB_RECORD, LOG_DEBUG,
            QString("Set progstart mark=%1 because %2<=%3<%4")
            .arg(lastSavedKeyframe).arg(lastSavedDuration)
            .arg(estimatedProgStartMS).arg(*durationDeltaCopy.begin()));
        bookmarkFrame = lastSavedKeyframe;
    }
    else if (*durationDeltaCopy.begin() <= estimatedProgStartMS &&
             estimatedProgStartMS < *last_it)
    {
        frm_pos_map_t::const_iterator upper_it = durationDeltaCopy.begin();
        for (; upper_it != durationDeltaCopy.end(); ++upper_it)
        {
            if (*upper_it > estimatedProgStartMS)
            {
                --upper_it;
                // Set progstart mark @ upper_it.key()
                LOG(VB_RECORD, LOG_DEBUG,
                    QString("Set progstart mark=%1 because "
                            "estimatedProgStartMS=%2 and upper_it.value()=%3")
                    .arg(upper_it.key()).arg(estimatedProgStartMS)
                    .arg(upper_it.value()));
                bookmarkFrame = upper_it.key();
                break;
            }
        }
    }
    else
    {
        // do nothing
        LOG(VB_RECORD, LOG_DEBUG, "No progstart mark due to fallthrough");
    }
    if (bookmarkFrame)
    {
        frm_dir_map_t progStartMap;
        progStartMap[bookmarkFrame] = MARK_UTIL_PROGSTART;
        curRecording->SaveMarkupMap(progStartMap, MARK_UTIL_PROGSTART);
    }
    lastSavedKeyframe = last_it.key();
    lastSavedDuration = last_it.value();
    LOG(VB_RECORD, LOG_DEBUG,
        QString("Setting lastSavedKeyframe=%1 lastSavedDuration=%2 "
                "for progstart mark calculations")
        .arg(lastSavedKeyframe).arg(lastSavedDuration));
}

void RecorderBase::AspectChange(uint aspect, long long frame)
{
    MarkTypes mark = MARK_ASPECT_4_3;
    uint customAspect = 0;
    if (aspect == ASPECT_1_1 || aspect >= ASPECT_CUSTOM)
    {
        if (aspect > 0x0F)
            customAspect = aspect;
        else if (m_videoWidth && m_videoHeight)
            customAspect = m_videoWidth * 1000000 / m_videoHeight;

        mark = (customAspect) ? MARK_ASPECT_CUSTOM : mark;
    }
    if (aspect == ASPECT_4_3)
        mark = MARK_ASPECT_4_3;
    if (aspect == ASPECT_16_9)
        mark = MARK_ASPECT_16_9;
    if (aspect == ASPECT_2_21_1)
        mark = MARK_ASPECT_2_21_1;

    // Populate the recordfile table as early as possible, the best
    // value will be determined when the recording completes.
    if (curRecording && curRecording->GetRecordingFile() &&
        curRecording->GetRecordingFile()->m_videoAspectRatio == 0.0)
    {
        RecordingFile *recFile = curRecording->GetRecordingFile();
        switch (m_videoAspect)
        {
            case ASPECT_1_1 :
                recFile->m_videoAspectRatio = 1.0;
                break;
            case ASPECT_4_3:
                recFile->m_videoAspectRatio = 1.33333333333;
                break;
            case ASPECT_16_9:
                recFile->m_videoAspectRatio = 1.77777777777;
                break;
            case ASPECT_2_21_1:
                recFile->m_videoAspectRatio = 2.21;
                break;
            default:
                recFile->m_videoAspectRatio = (double)m_videoAspect / 1000000.0;
                break;
        }
        recFile->Save();
    }

    if (curRecording)
        curRecording->SaveAspect(frame, mark, customAspect);
}

void RecorderBase::ResolutionChange(uint width, uint height, long long frame)
{
    if (curRecording)
    {
        // Populate the recordfile table as early as possible, the best value
        // value will be determined when the recording completes.
        if (curRecording && curRecording->GetRecordingFile() &&
            curRecording->GetRecordingFile()->m_videoResolution.isNull())
        {
            curRecording->GetRecordingFile()->m_videoResolution = QSize(width, height);
            curRecording->GetRecordingFile()->Save();
        }
        curRecording->SaveResolution(frame, width, height);
    }
}

void RecorderBase::FrameRateChange(uint framerate, long long frame)
{
    if (curRecording)
    {
        // Populate the recordfile table as early as possible, the average
        // value will be determined when the recording completes.
        if (!curRecording->GetRecordingFile()->m_videoFrameRate)
        {
            curRecording->GetRecordingFile()->m_videoFrameRate = (double)framerate / 1000.0;
            curRecording->GetRecordingFile()->Save();
        }
        curRecording->SaveFrameRate(frame, framerate);
    }
}

void RecorderBase::VideoCodecChange(AVCodecID vCodec)
{
    if (curRecording && curRecording->GetRecordingFile())
    {
        curRecording->GetRecordingFile()->m_videoCodec = ff_codec_id_string(vCodec);
        curRecording->GetRecordingFile()->Save();
    }
}

void RecorderBase::AudioCodecChange(AVCodecID aCodec)
{
    if (curRecording && curRecording->GetRecordingFile())
    {
        curRecording->GetRecordingFile()->m_audioCodec = ff_codec_id_string(aCodec);
        curRecording->GetRecordingFile()->Save();
    }
}

void RecorderBase::SetDuration(uint64_t duration)
{
    if (curRecording)
        curRecording->SaveTotalDuration(duration);
}

void RecorderBase::SetTotalFrames(uint64_t total_frames)
{
    if (curRecording)
        curRecording->SaveTotalFrames(total_frames);
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
    else if (genOpt.cardtype == "CETON")
    {
#ifdef USING_CETON
        recorder = new CetonRecorder(
            tvrec, dynamic_cast<CetonChannel*>(channel));
        recorder->SetOption("wait_for_seqstart", genOpt.wait_for_seqstart);
#endif // USING_CETON
    }
    else if (genOpt.cardtype == "DVB")
    {
#ifdef USING_DVB
        recorder = new DVBRecorder(
            tvrec, dynamic_cast<DVBChannel*>(channel));
        recorder->SetOption("wait_for_seqstart", genOpt.wait_for_seqstart);
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
#ifdef USING_V4L2
        // V4L/MJPEG/GO7007 from here on
        recorder = new NuppelVideoRecorder(tvrec, channel);
        recorder->SetOption("skipbtaudio", genOpt.skip_btaudio);
#endif // USING_V4L2
    }
    else if (genOpt.cardtype == "EXTERNAL")
    {
        recorder = new ExternalRecorder(tvrec,
                                dynamic_cast<ExternalChannel*>(channel));
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
        LOG(VB_GENERAL, LOG_ERR,
            "RecorderBase::CreateRecorder() Error, " + msg);
    }

    return recorder;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
