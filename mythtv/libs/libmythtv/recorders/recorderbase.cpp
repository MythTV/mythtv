#include <algorithm> // for min
#include <cstdint>

#include "libmythbase/mythdate.h"
#include "libmythbase/mythlogging.h"
#include "libmythbase/programinfo.h"

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
#include "asichannel.h"
#include "dtvchannel.h"
#include "dvbchannel.h"
#include "satipchannel.h"
#include "satiprecorder.h"
#include "ExternalChannel.h"
#include "io/mythmediabuffer.h"
#include "cardutil.h"
#include "tv_rec.h"
#if CONFIG_LIBMP3LAME
#include "NuppelVideoRecorder.h"
#endif
#if CONFIG_V4L2
#include "v4l2encrecorder.h"
#include "v4lchannel.h"
#endif

#define TVREC_CARDNUM \
        ((m_tvrec != nullptr) ? QString::number(m_tvrec->GetInputId()) : "NULL")

#define LOC QString("RecBase[%1](%2): ") \
            .arg(TVREC_CARDNUM, m_videodevice)

RecorderBase::RecorderBase(TVRec *rec)
    : m_tvrec(rec)
{
    RecorderBase::ClearStatistics();
}

RecorderBase::~RecorderBase(void)
{
    if (m_weMadeBuffer && m_ringBuffer)
    {
        delete m_ringBuffer;
        m_ringBuffer = nullptr;
    }
    SetRecording(nullptr);
    if (m_nextRingBuffer)
    {
        QMutexLocker locker(&m_nextRingBufferLock);
        delete m_nextRingBuffer;
        m_nextRingBuffer = nullptr;
    }
    if (m_nextRecording)
    {
        delete m_nextRecording;
        m_nextRecording = nullptr;
    }
}

void RecorderBase::SetRingBuffer(MythMediaBuffer *Buffer)
{
    if (VERBOSE_LEVEL_CHECK(VB_RECORD, LOG_INFO))
    {
        QString msg("");
        if (Buffer)
            msg = " '" + Buffer->GetFilename() + "'";
        LOG(VB_RECORD, LOG_INFO, LOC + QString("SetRingBuffer(0x%1)")
                .arg((uint64_t)Buffer,0,16) + msg);
    }
    m_ringBuffer = Buffer;
    m_weMadeBuffer = false;
}

void RecorderBase::SetRecording(const RecordingInfo *pginfo)
{
    if (pginfo)
    {
        LOG(VB_RECORD, LOG_INFO, LOC + QString("SetRecording(0x%1) title(%2)")
                .arg((uint64_t)pginfo,0,16).arg(pginfo->GetTitle()));
    }
    else
    {
        LOG(VB_RECORD, LOG_INFO, LOC + "SetRecording(0x0)");
    }

    ProgramInfo *oldrec = m_curRecording;
    if (pginfo)
    {
        // NOTE: RecorderBase and TVRec do not share a single RecordingInfo
        //       instance which may lead to the possibility that changes made
        //       in the database by one are overwritten by the other
        m_curRecording = new RecordingInfo(*pginfo);
        // Compute an estimate of the actual progstart delay for setting the
        // MARK_UTIL_PROGSTART mark.  We can't reliably use
        // m_curRecording->GetRecordingStartTime() because the scheduler rounds it
        // to the nearest minute, so we use the current time instead.
        m_estimatedProgStartMS =
            MythDate::current().msecsTo(m_curRecording->GetScheduledStartTime());
        RecordingFile *recFile = m_curRecording->GetRecordingFile();
        recFile->m_containerFormat = m_containerFormat;
        recFile->Save();
    }
    else
    {
        m_curRecording = nullptr;
    }

    delete oldrec;
}

void RecorderBase::SetNextRecording(const RecordingInfo *ri, MythMediaBuffer *Buffer)
{
    LOG(VB_RECORD, LOG_INFO, LOC + QString("SetNextRecording(0x%1, 0x%2)")
        .arg(reinterpret_cast<intptr_t>(ri),0,16)
        .arg(reinterpret_cast<intptr_t>(Buffer),0,16));

    // First we do some of the time consuming stuff we can do now
    SavePositionMap(true);
    if (m_ringBuffer)
    {
        m_ringBuffer->WriterFlush();
        if (m_curRecording)
            m_curRecording->SaveFilesize(m_ringBuffer->GetRealFileSize());
    }

    // Then we set the next info
    QMutexLocker locker(&m_nextRingBufferLock);
    if (m_nextRecording)
    {
        delete m_nextRecording;
        m_nextRecording = nullptr;
    }
    if (ri)
        m_nextRecording = new RecordingInfo(*ri);

    delete m_nextRingBuffer;
    m_nextRingBuffer = Buffer;
}

void RecorderBase::SetOption(const QString &name, const QString &value)
{
    if (name == "videocodec")
        m_videocodec = value;
    else if (name == "videodevice")
        m_videodevice = value;
    else if (name == "tvformat")
    {
        m_ntsc = false;
        if (value.toLower() == "ntsc" || value.toLower() == "ntsc-jp")
        {    // NOLINT(bugprone-branch-clone)
            m_ntsc = true;
            SetFrameRate(29.97);
        }
        else if (value.toLower() == "pal-m")
        {
            SetFrameRate(29.97);
        }
        else if (value.toLower() == "atsc")
        {
            // Here we set the TV format values for ATSC. ATSC isn't really
            // NTSC, but users who configure a non-ATSC-recorder as ATSC
            // are far more likely to be using a mix of ATSC and NTSC than
            // a mix of ATSC and PAL or SECAM. The atsc recorder itself
            // does not care about these values, except in so much as tv_rec
            // cares about m_videoFrameRate which should be neither 29.97
            // nor 25.0, but based on the actual video.
            m_ntsc = true;
            SetFrameRate(29.97);
        }
        else
        {
            SetFrameRate(25.00);
        }
    }
    else
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC +
            QString("SetOption(%1,%2): Option not recognized")
                .arg(name, value));
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
    const StandardSetting *setting = profile->byName(name);
    if (setting)
        SetOption(name, setting->getValue().toInt());
    else
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("SetIntOption(...%1): Option not in profile.").arg(name));
}

void RecorderBase::SetStrOption(RecordingProfile *profile, const QString &name)
{
    const StandardSetting *setting = profile->byName(name);
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
    QMutexLocker locker(&m_pauseLock);
    m_requestRecording = false;
    m_unpauseWait.wakeAll();
    while (m_recording)
    {
        m_recordingWait.wait(&m_pauseLock, 100);
        if (m_requestRecording)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                "Programmer Error: Recorder started while we were in "
                "StopRecording");
            m_requestRecording = false;
        }
    }
}

/// \brief Tells whether the StartRecorder() loop is running.
bool RecorderBase::IsRecording(void)
{
    QMutexLocker locker(&m_pauseLock);
    return m_recording;
}

/// \brief Tells us if StopRecording() has been called.
bool RecorderBase::IsRecordingRequested(void)
{
    QMutexLocker locker(&m_pauseLock);
    return m_requestRecording;
}

/** \brief Pause tells recorder to pause, it should not block.
 *
 *   Once paused the recorder calls m_tvrec->RecorderPaused().
 *
 *  \param clear if true any generated timecodes should be reset.
 *  \sa Unpause(), WaitForPause()
 */
void RecorderBase::Pause([[maybe_unused]] bool clear)
{
    QMutexLocker locker(&m_pauseLock);
    m_requestPause = true;
}

/** \brief Unpause tells recorder to unpause.
 *  This is an asynchronous call it should not wait block waiting
 *  for the command to be processed.
 */
void RecorderBase::Unpause(void)
{
    QMutexLocker locker(&m_pauseLock);
    m_requestPause = false;
    m_unpauseWait.wakeAll();
}

/// \brief Returns true iff recorder is paused.
bool RecorderBase::IsPaused(bool holding_lock) const
{
    if (!holding_lock)
        m_pauseLock.lock();
    bool ret = m_paused;
    if (!holding_lock)
        m_pauseLock.unlock();
    return ret;
}

/**

 *  \brief WaitForPause blocks until recorder is actually paused,
 *         or timeout milliseconds elapse.
 *  \param timeout number of milliseconds to wait defaults to 1000.
 *  \return true iff pause happened within timeout period.
 */
bool RecorderBase::WaitForPause(std::chrono::milliseconds timeout)
{
    MythTimer t;
    t.start();

    QMutexLocker locker(&m_pauseLock);
    while (!IsPaused(true) && m_requestPause)
    {
        std::chrono::milliseconds wait = timeout - t.elapsed();
        if (wait <= 0ms)
            return false;
        m_pauseWait.wait(&m_pauseLock, wait.count());
    }
    return true;
}

/**
 *  \brief If m_requestPause is true, sets pause and blocks up to
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
bool RecorderBase::PauseAndWait(std::chrono::milliseconds timeout)
{
    QMutexLocker locker(&m_pauseLock);
    if (m_requestPause)
    {
        if (!IsPaused(true))
        {
            m_paused = true;
            m_pauseWait.wakeAll();
            if (m_tvrec)
                m_tvrec->RecorderPaused();
        }

        m_unpauseWait.wait(&m_pauseLock, timeout.count());
    }

    if (!m_requestPause && IsPaused(true))
    {
        m_paused = false;
        m_unpauseWait.wakeAll();
    }

    return IsPaused(true);
}

bool RecorderBase::CheckForRingBufferSwitch(void)
{
    bool did_switch = false;

    m_nextRingBufferLock.lock();

    RecordingQuality *recq = nullptr;

    if (m_nextRingBuffer)
    {
        FinishRecording();

        recq = GetRecordingQuality(nullptr);

        ResetForNewFile();

        m_videoAspect = m_videoWidth = m_videoHeight = 0;
        m_frameRate = MythAVRational(0);

        SetRingBuffer(m_nextRingBuffer);
        SetRecording(m_nextRecording);

        m_nextRingBuffer = nullptr;
        m_nextRecording = nullptr;

        StartNewFile();
        did_switch = true;
    }
    m_nextRingBufferLock.unlock();

    if (recq && m_tvrec)
    {
        // This call will free recq.
        m_tvrec->RingBufferChanged(m_ringBuffer, m_curRecording, recq);
    }
    else
    {
        delete recq;
    }

    m_ringBufferCheckTimer.restart();
    return did_switch;
}

void RecorderBase::SetRecordingStatus(RecStatus::Type status,
                                      const QString& file, int line)
{
    if (m_curRecording && m_curRecording->GetRecordingStatus() != status)
    {
        LOG(VB_RECORD, LOG_INFO,
            QString("Modifying recording status from %1 to %2 at %3:%4")
            .arg(RecStatus::toString(m_curRecording->GetRecordingStatus(), kSingleRecord),
                 RecStatus::toString(status, kSingleRecord),
                 file,
                 QString::number(line)));

        m_curRecording->SetRecordingStatus(status);

        if (status == RecStatus::Failing)
        {
            m_curRecording->SaveVideoProperties(VID_DAMAGED, VID_DAMAGED);
            SendMythSystemRecEvent("REC_FAILING", m_curRecording);
        }

        MythEvent me(QString("UPDATE_RECORDING_STATUS %1 %2 %3 %4 %5")
                    .arg(m_curRecording->GetInputID())
                    .arg(m_curRecording->GetChanID())
                    .arg(m_curRecording->GetScheduledStartTime(MythDate::ISODate))
                    .arg(status)
                    .arg(m_curRecording->GetRecordingEndTime(MythDate::ISODate)));
        gCoreContext->dispatch(me);
    }
}

void RecorderBase::ClearStatistics(void)
{
    QMutexLocker locker(&m_statisticsLock);
    m_timeOfFirstData = QDateTime();
    m_timeOfFirstDataIsSet.fetchAndStoreRelaxed(0);
    m_timeOfLatestData = QDateTime();
    m_timeOfLatestDataCount.fetchAndStoreRelaxed(0);
    m_timeOfLatestDataPacketInterval.fetchAndStoreRelaxed(2000);
    m_recordingGaps.clear();
}

void RecorderBase::FinishRecording(void)
{
    if (m_curRecording)
    {
        if (m_primaryVideoCodec == AV_CODEC_ID_H264)
            m_curRecording->SaveVideoProperties(VID_AVC, VID_AVC);
        else if (m_primaryVideoCodec == AV_CODEC_ID_H265)
            m_curRecording->SaveVideoProperties(VID_HEVC, VID_HEVC);
        else if (m_primaryVideoCodec == AV_CODEC_ID_MPEG2VIDEO)
            m_curRecording->SaveVideoProperties(VID_MPEG2, VID_MPEG2);

        RecordingFile *recFile = m_curRecording->GetRecordingFile();
        if (recFile)
        {
            // Container
            recFile->m_containerFormat = m_containerFormat;

            // Video
            recFile->m_videoCodec = avcodec_get_name(m_primaryVideoCodec);
            switch (m_curRecording->QueryAverageAspectRatio())
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
            QSize resolution(m_curRecording->QueryAverageWidth(),
                            m_curRecording->QueryAverageHeight());
            recFile->m_videoResolution = resolution;
            recFile->m_videoFrameRate = (double)m_curRecording->QueryAverageFrameRate() / 1000.0;

            // Audio
            recFile->m_audioCodec = avcodec_get_name(m_primaryAudioCodec);

            recFile->Save();
        }
        else
        {
            LOG(VB_GENERAL, LOG_CRIT, "RecordingFile object is NULL. No video file metadata can be stored");
        }

        SavePositionMap(true, true); // Save Position Map only, not file size

        if (m_ringBuffer)
            m_curRecording->SaveFilesize(m_ringBuffer->GetRealFileSize());
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
                                        .arg(avcodec_get_name(m_primaryAudioCodec),
                                             RecordingFile::AVContainerToString(m_containerFormat)));
}

RecordingQuality *RecorderBase::GetRecordingQuality(
    const RecordingInfo *r) const
{
    QMutexLocker locker(&m_statisticsLock);
    if (r && m_curRecording &&
        (r->MakeUniqueKey() == m_curRecording->MakeUniqueKey()))
    {
        m_curRecording->SetDesiredStartTime(r->GetDesiredStartTime());
        m_curRecording->SetDesiredEndTime(r->GetDesiredEndTime());
    }
    return new RecordingQuality(
        m_curRecording, m_recordingGaps,
        m_timeOfFirstData, m_timeOfLatestData);
}

long long RecorderBase::GetKeyframePosition(long long desired) const
{
    QMutexLocker locker(&m_positionMapLock);

    if (m_positionMap.empty())
        return -1;

    // find closest exact or previous keyframe position...
    frm_pos_map_t::const_iterator it = m_positionMap.lowerBound(desired);
    if (it == m_positionMap.end())
        return *m_positionMap.begin();
    if (it.key() == desired)
        return *it;

    it--;
    if (it != m_positionMap.end())
        return *it;

    return -1;
}

bool RecorderBase::GetKeyframePositions(
    long long start, long long end, frm_pos_map_t &map) const
{
    map.clear();

    QMutexLocker locker(&m_positionMapLock);
    if (m_positionMap.empty())
        return true;

    frm_pos_map_t::const_iterator it = m_positionMap.lowerBound(start);
    end = (end < 0) ? INT64_MAX : end;
    for (; (it != m_positionMap.end()) &&
             (it.key() <= end); ++it)
        map[it.key()] = *it;

    LOG(VB_GENERAL, LOG_DEBUG, LOC +
        QString("GetKeyframePositions(%1,%2,#%3) out of %4")
            .arg(start).arg(end).arg(map.size()).arg(m_positionMap.size()));

    return true;
}

bool RecorderBase::GetKeyframeDurations(
    long long start, long long end, frm_pos_map_t &map) const
{
    map.clear();

    QMutexLocker locker(&m_positionMapLock);
    if (m_durationMap.empty())
        return true;

    frm_pos_map_t::const_iterator it = m_durationMap.lowerBound(start);
    end = (end < 0) ? INT64_MAX : end;
    for (; (it != m_durationMap.end()) &&
             (it.key() <= end); ++it)
        map[it.key()] = *it;

    LOG(VB_GENERAL, LOG_DEBUG, LOC +
        QString("GetKeyframeDurations(%1,%2,#%3) out of %4")
            .arg(start).arg(end).arg(map.size()).arg(m_durationMap.size()));

    return true;
}

/**
 *  \brief This saves the position map delta to the database if force
 *         is true or there are 30 frames in the map or there are five
 *         frames in the map with less than 30 frames in the non-delta
 *         position map.
 *  \param force If true this forces a DB sync.
 *  \param finished Is this a finished recording?
 */
void RecorderBase::SavePositionMap(bool force, bool finished)
{
    bool needToSave = force;
    m_positionMapLock.lock();

    bool has_delta = !m_positionMapDelta.empty();
    // set pm_elapsed to a fake large value if the timer hasn't yet started
    std::chrono::milliseconds pm_elapsed = (m_positionMapTimer.isRunning()) ?
        m_positionMapTimer.elapsed() : std::chrono::milliseconds::max();
    // save on every 1.5 seconds if in the first few frames of a recording
    needToSave |= (m_positionMap.size() < 30) &&
        has_delta && (pm_elapsed >= 1.5s);
    // save every 10 seconds later on
    needToSave |= has_delta && (pm_elapsed >= 10s);
    // Assume that m_durationMapDelta is the same size as
    // m_positionMapDelta and implicitly use the same logic about when
    // to same m_durationMapDelta.

    if (m_curRecording && needToSave)
    {
        m_positionMapTimer.start();
        if (has_delta)
        {
            // copy the delta map because most times we are called it will be in
            // another thread and we don't want to lock the main recorder thread
            // which is populating the delta map
            frm_pos_map_t deltaCopy(m_positionMapDelta);
            m_positionMapDelta.clear();
            frm_pos_map_t durationDeltaCopy(m_durationMapDelta);
            m_durationMapDelta.clear();
            m_positionMapLock.unlock();

            m_curRecording->SavePositionMapDelta(deltaCopy, m_positionMapType);
            m_curRecording->SavePositionMapDelta(durationDeltaCopy,
                                               MARK_DURATION_MS);

            TryWriteProgStartMark(durationDeltaCopy);
        }
        else
        {
            m_positionMapLock.unlock();
        }

        if (m_ringBuffer && !finished) // Finished Recording will update the final size for us
        {
            m_curRecording->SaveFilesize(m_ringBuffer->GetWritePosition());
        }
    }
    else
    {
        m_positionMapLock.unlock();
    }

    // Make sure a ringbuffer switch is checked at least every 3
    // seconds.  Otherwise, this check is only performed on keyframes,
    // and if there is a problem with the input we may never see one
    // again, resulting in a wedged recording.
    if (!finished && m_ringBufferCheckTimer.isRunning() &&
        m_ringBufferCheckTimer.elapsed() > 3s)
    {
        if (CheckForRingBufferSwitch())
            LOG(VB_RECORD, LOG_WARNING, LOC +
                "Ringbuffer was switched due to timeout instead of keyframe.");
    }
}

void RecorderBase::TryWriteProgStartMark(const frm_pos_map_t &durationDeltaCopy)
{
    // Note: all log strings contain "progstart mark" for searching.
    if (m_estimatedProgStartMS <= 0)
    {
        // Do nothing because no progstart mark is needed.
        LOG(VB_RECORD, LOG_DEBUG,
            QString("No progstart mark needed because delta=%1")
            .arg(m_estimatedProgStartMS));
        return;
    }
    frm_pos_map_t::const_iterator first_it = durationDeltaCopy.begin();
    if (first_it == durationDeltaCopy.end())
    {
        LOG(VB_RECORD, LOG_DEBUG, "No progstart mark because map is empty");
        return;
    }
    frm_pos_map_t::const_iterator last_it = durationDeltaCopy.end();
    --last_it;
    long long bookmarkFrame = 0;
    long long first_time { first_it.value() };
    long long last_time  { last_it.value()  };
    LOG(VB_RECORD, LOG_DEBUG,
        QString("durationDeltaCopy.begin() = (%1,%2)")
        .arg(first_it.key())
        .arg(first_it.value()));
    if (m_estimatedProgStartMS > last_time)
    {
        // Do nothing because we haven't reached recstartts yet.
        LOG(VB_RECORD, LOG_DEBUG,
            QString("No progstart mark yet because estimatedProgStartMS=%1 "
                    "and *last_it=%2")
            .arg(m_estimatedProgStartMS).arg(last_time));
    }
    else if (m_lastSavedDuration <= m_estimatedProgStartMS &&
             m_estimatedProgStartMS < first_time)
    {
        // Set progstart mark @ lastSavedKeyframe
        LOG(VB_RECORD, LOG_DEBUG,
            QString("Set progstart mark=%1 because %2<=%3<%4")
            .arg(m_lastSavedKeyframe).arg(m_lastSavedDuration)
            .arg(m_estimatedProgStartMS).arg(first_time));
        bookmarkFrame = m_lastSavedKeyframe;
    }
    else if (first_time <= m_estimatedProgStartMS &&
             m_estimatedProgStartMS < last_time)
    {
        frm_pos_map_t::const_iterator upper_it = first_it;
        for (; upper_it != durationDeltaCopy.end(); ++upper_it)
        {
            if (*upper_it > m_estimatedProgStartMS)
            {
                --upper_it;
                // Set progstart mark @ upper_it.key()
                LOG(VB_RECORD, LOG_DEBUG,
                    QString("Set progstart mark=%1 because "
                            "estimatedProgStartMS=%2 and upper_it.value()=%3")
                    .arg(upper_it.key()).arg(m_estimatedProgStartMS)
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
        m_curRecording->SaveMarkupMap(progStartMap, MARK_UTIL_PROGSTART);
    }
    m_lastSavedKeyframe = last_it.key();
    m_lastSavedDuration = last_it.value();
    LOG(VB_RECORD, LOG_DEBUG,
        QString("Setting lastSavedKeyframe=%1 lastSavedDuration=%2 "
                "for progstart mark calculations")
        .arg(m_lastSavedKeyframe).arg(m_lastSavedDuration));
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
    if (m_curRecording && m_curRecording->GetRecordingFile() &&
        m_curRecording->GetRecordingFile()->m_videoAspectRatio == 0.0)
    {
        RecordingFile *recFile = m_curRecording->GetRecordingFile();
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

    if (m_curRecording)
        m_curRecording->SaveAspect(frame, mark, customAspect);
}

void RecorderBase::ResolutionChange(uint width, uint height, long long frame)
{
    if (m_curRecording)
    {
        // Populate the recordfile table as early as possible, the best value
        // value will be determined when the recording completes.
        if (m_curRecording && m_curRecording->GetRecordingFile() &&
            m_curRecording->GetRecordingFile()->m_videoResolution.isNull())
        {
            m_curRecording->GetRecordingFile()->m_videoResolution = QSize(width, height);
            m_curRecording->GetRecordingFile()->Save();
        }
        m_curRecording->SaveResolution(frame, width, height);
    }
}

void RecorderBase::FrameRateChange(uint framerate, uint64_t frame)
{
    if (m_curRecording)
    {
        // Populate the recordfile table as early as possible, the average
        // value will be determined when the recording completes.
        if (m_curRecording->GetRecordingFile()->m_videoFrameRate == 0.0)
        {
            m_curRecording->GetRecordingFile()->m_videoFrameRate = (double)framerate / 1000.0;
            m_curRecording->GetRecordingFile()->Save();
        }
        m_curRecording->SaveFrameRate(frame, framerate);
    }
}

void RecorderBase::VideoScanChange(SCAN_t scan, uint64_t frame)
{
    if (m_curRecording)
        m_curRecording->SaveVideoScanType(frame, scan != SCAN_t::INTERLACED);
}

void RecorderBase::VideoCodecChange(AVCodecID vCodec)
{
    if (m_curRecording && m_curRecording->GetRecordingFile())
    {
        m_curRecording->GetRecordingFile()->m_videoCodec = avcodec_get_name(vCodec);
        m_curRecording->GetRecordingFile()->Save();
    }
}

void RecorderBase::AudioCodecChange(AVCodecID aCodec)
{
    if (m_curRecording && m_curRecording->GetRecordingFile())
    {
        m_curRecording->GetRecordingFile()->m_audioCodec = avcodec_get_name(aCodec);
        m_curRecording->GetRecordingFile()->Save();
    }
}

void RecorderBase::SetDuration(std::chrono::milliseconds duration)
{
    if (m_curRecording)
        m_curRecording->SaveTotalDuration(duration);
}

void RecorderBase::SetTotalFrames(uint64_t total_frames)
{
    if (m_curRecording)
        m_curRecording->SaveTotalFrames(total_frames);
}


RecorderBase *RecorderBase::CreateRecorder(
    TVRec                  *tvrec,
    ChannelBase            *channel,
    RecordingProfile       &profile,
    const GeneralDBOptions &genOpt)
{
    if (!channel)
        return nullptr;

    RecorderBase *recorder = nullptr;
    if (genOpt.m_inputType == "IMPORT")
    {
        recorder = new ImportRecorder(tvrec);
    }
    else if (genOpt.m_inputType == "EXTERNAL")
    {
        if (dynamic_cast<ExternalChannel*>(channel))
            recorder = new ExternalRecorder(tvrec, dynamic_cast<ExternalChannel*>(channel));
    }
#ifdef USING_V4L2
    else if ((genOpt.m_inputType == "MPEG") ||
	     (genOpt.m_inputType == "HDPVR") ||
	     (genOpt.m_inputType == "DEMO"))
    {
        recorder = new MpegRecorder(tvrec);
    }
    else if (genOpt.m_inputType == "V4L2ENC")
    {
        if (dynamic_cast<V4LChannel*>(channel))
            recorder = new V4L2encRecorder(tvrec, dynamic_cast<V4LChannel*>(channel));
    }
#else
    else if (genOpt.m_inputType == "DEMO")
    {
        recorder = new ImportRecorder(tvrec);
    }
#endif // USING_V4L2
#ifdef USING_FIREWIRE
    else if (genOpt.m_inputType == "FIREWIRE")
    {
        if (dynamic_cast<FirewireChannel*>(channel))
            recorder = new FirewireRecorder(tvrec, dynamic_cast<FirewireChannel*>(channel));
    }
#endif // USING_FIREWIRE
#ifdef USING_HDHOMERUN
    else if (genOpt.m_inputType == "HDHOMERUN")
    {
        if (dynamic_cast<HDHRChannel*>(channel))
        {
            recorder = new HDHRRecorder(tvrec, dynamic_cast<HDHRChannel*>(channel));
            recorder->SetBoolOption("wait_for_seqstart", genOpt.m_waitForSeqstart);
        }
    }
#endif // USING_HDHOMERUN
#ifdef USING_CETON
    else if (genOpt.m_inputType == "CETON")
    {
        if (dynamic_cast<CetonChannel*>(channel))
        {
            recorder = new CetonRecorder(tvrec, dynamic_cast<CetonChannel*>(channel));
            recorder->SetBoolOption("wait_for_seqstart", genOpt.m_waitForSeqstart);
        }
    }
#endif // USING_CETON
#ifdef USING_DVB
    else if (genOpt.m_inputType == "DVB")
    {
        if (dynamic_cast<DVBChannel*>(channel))
        {
            recorder = new DVBRecorder(tvrec, dynamic_cast<DVBChannel*>(channel));
            recorder->SetBoolOption("wait_for_seqstart", genOpt.m_waitForSeqstart);
        }
    }
#endif // USING_DVB
#ifdef USING_IPTV
    else if (genOpt.m_inputType == "FREEBOX")
    {
        if (dynamic_cast<IPTVChannel*>(channel))
        {
            recorder = new IPTVRecorder(tvrec, dynamic_cast<IPTVChannel*>(channel));
            recorder->SetOption("mrl", genOpt.m_videoDev);
        }
    }
#endif // USING_IPTV
#ifdef USING_VBOX
    else if (genOpt.m_inputType == "VBOX")
    {
        if (dynamic_cast<IPTVChannel*>(channel))
            recorder = new IPTVRecorder(tvrec, dynamic_cast<IPTVChannel*>(channel));
    }
#endif // USING_VBOX
#ifdef USING_SATIP
    else if (genOpt.m_inputType == "SATIP")
    {
        if (dynamic_cast<SatIPChannel*>(channel))
            recorder = new SatIPRecorder(tvrec, dynamic_cast<SatIPChannel*>(channel));
    }
#endif // USING_SATIP
#ifdef USING_ASI
    else if (genOpt.m_inputType == "ASI")
    {
        if (dynamic_cast<ASIChannel*>(channel))
        {
            recorder = new ASIRecorder(tvrec, dynamic_cast<ASIChannel*>(channel));
            recorder->SetBoolOption("wait_for_seqstart", genOpt.m_waitForSeqstart);
        }
    }
#endif // USING_ASI
#if CONFIG_LIBMP3LAME && defined(USING_V4L2)
    else if (CardUtil::IsV4L(genOpt.m_inputType))
    {
        // V4L/MJPEG/GO7007 from here on
        recorder = new NuppelVideoRecorder(tvrec, channel);
        recorder->SetBoolOption("skipbtaudio", genOpt.m_skipBtAudio);
    }
#endif // USING_V4L2

    if (recorder)
    {
        recorder->SetOptionsFromProfile(&profile,
            genOpt.m_videoDev, genOpt.m_audioDev, genOpt.m_vbiDev);
        // Override the samplerate defined in the profile if this card
        // was configured with a fixed rate.
        if (genOpt.m_audioSampleRate)
            recorder->SetOption("samplerate", genOpt.m_audioSampleRate);
    }
    else
    {
        QString msg = "Need %1 recorder, but compiled without %2 support!";
        msg = msg.arg(genOpt.m_inputType, genOpt.m_inputType);
        LOG(VB_GENERAL, LOG_ERR,
            "RecorderBase::CreateRecorder() Error, " + msg);
    }

    return recorder;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
