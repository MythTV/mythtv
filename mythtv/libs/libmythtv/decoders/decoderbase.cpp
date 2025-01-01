
#include <algorithm>

#include "libmythbase/iso639.h"
#include "libmythbase/mythconfig.h"
#include "libmythbase/mythlogging.h"
#include "libmythbase/programinfo.h"

#include "Bluray/mythbdbuffer.h"
#include "DVD/mythdvdbuffer.h"
#include "decoderbase.h"
#include "mythcodeccontext.h"
#include "mythplayer.h"

#define LOC QString("Dec: ")

DecoderBase::DecoderBase(MythPlayer *parent, const ProgramInfo &pginfo)
    : m_parent(parent), m_playbackInfo(new ProgramInfo(pginfo)),
      m_audio(m_parent->GetAudio()),

      // language preference
      m_languagePreference(iso639_get_language_key_list())
{
    ResetTracks();
    m_tracks[kTrackTypeAudio].emplace_back(0, 0, 0, 0, kAudioTypeNormal);
    m_tracks[kTrackTypeCC608].emplace_back(0, 1);
    m_tracks[kTrackTypeCC608].emplace_back(0, 3);
}

DecoderBase::~DecoderBase()
{
    delete m_playbackInfo;
}

void DecoderBase::SetRenderFormats(const VideoFrameTypes* RenderFormats)
{
    if (RenderFormats != nullptr)
        m_renderFormats = RenderFormats;
}

void DecoderBase::SetProgramInfo(const ProgramInfo &pginfo)
{
    delete m_playbackInfo;
    m_playbackInfo = new ProgramInfo(pginfo);
}

void DecoderBase::Reset(bool reset_video_data, bool seek_reset, bool reset_file)
{
    LOG(VB_PLAYBACK, LOG_INFO, LOC +
        QString("Reset: Video %1, Seek %2, File %3")
            .arg(reset_video_data).arg(seek_reset).arg(reset_file));

    if (seek_reset)
        SeekReset(0, 0, true, true);

    if (reset_video_data)
    {
        ResetPosMap();
        m_framesPlayed = 0;
        m_frameCounter += 100;
        m_fpsSkip = 0;
        m_framesRead = 0;
        m_totalDuration = MythAVRational(0);
        m_dontSyncPositionMap = false;
    }

    if (reset_file)
    {
        m_waitingForChange = false;
        SetEofState(kEofStateNone);
    }
}

void DecoderBase::SeekReset(long long /*newkey*/, uint /*skipFrames*/,
                            bool /*doFlush*/, bool /*discardFrames*/)
{
    m_readAdjust = 0;
    m_frameCounter += 100; // NB don't just set to 0
}

void DecoderBase::SetWatchingRecording(bool mode)
{
    bool wereWatchingRecording = m_watchingRecording;

    // When we switch from WatchingRecording to WatchingPreRecorded,
    // re-get the positionmap
    m_posmapStarted = false;
    m_watchingRecording = mode;

    if (wereWatchingRecording && !m_watchingRecording)
        SyncPositionMap();
}

bool DecoderBase::PosMapFromDb(void)
{
    if (!m_playbackInfo)
        return false;

    // Overwrites current positionmap with entire contents of database
    frm_pos_map_t posMap;
    frm_pos_map_t durMap;

    if (m_ringBuffer && m_ringBuffer->IsDVD())
    {
        m_keyframeDist = 15;
        m_fps = m_ringBuffer->DVD()->GetFrameRate();
        if (m_fps < 26 && m_fps > 24)
           m_keyframeDist = 12;
        auto totframes =
            (long long)(m_ringBuffer->DVD()->GetTotalTimeOfTitle().count() * m_fps);
        posMap[totframes] = m_ringBuffer->DVD()->GetTotalReadPosition();
    }
    else if (m_ringBuffer && m_ringBuffer->IsBD())
    {
        m_keyframeDist = 15;
        m_fps = m_ringBuffer->BD()->GetFrameRate();
        if (m_fps < 26 && m_fps > 24)
           m_keyframeDist = 12;
        auto totframes =
            (long long)(m_ringBuffer->BD()->GetTotalTimeOfTitle().count() * m_fps);
        posMap[totframes] = m_ringBuffer->BD()->GetTotalReadPosition();
#if 0
        LOG(VB_PLAYBACK, LOG_DEBUG, LOC +
            QString("%1 TotalTimeOfTitle() in ticks, %2 TotalReadPosition() "
                    "in bytes, %3 is fps")
                .arg(m_ringBuffer->BD()->GetTotalTimeOfTitle())
                .arg(m_ringBuffer->BD()->GetTotalReadPosition()).arg(m_fps));
#endif
    }
    else if ((m_positionMapType == MARK_UNSET) ||
        (m_keyframeDist == -1))
    {
        m_playbackInfo->QueryPositionMap(posMap, MARK_GOP_BYFRAME);
        if (!posMap.empty())
        {
            m_positionMapType = MARK_GOP_BYFRAME;
            if (m_keyframeDist == -1)
                m_keyframeDist = 1;
        }
        else
        {
            m_playbackInfo->QueryPositionMap(posMap, MARK_GOP_START);
            if (!posMap.empty())
            {
                m_positionMapType = MARK_GOP_START;
                if (m_keyframeDist == -1)
                {
                    m_keyframeDist = 15;
                    if (m_fps < 26 && m_fps > 24)
                        m_keyframeDist = 12;
                }
            }
            else
            {
                m_playbackInfo->QueryPositionMap(posMap, MARK_KEYFRAME);
                if (!posMap.empty())
                {
                    // keyframedist should be set in the fileheader so no
                    // need to try to determine it in this case
                    m_positionMapType = MARK_KEYFRAME;
                }
            }
        }
    }
    else
    {
        m_playbackInfo->QueryPositionMap(posMap, m_positionMapType);
    }

    if (posMap.empty())
        return false; // no position map in recording

    m_playbackInfo->QueryPositionMap(durMap, MARK_DURATION_MS);

    QMutexLocker locker(&m_positionMapLock);
    m_positionMap.clear();
    m_positionMap.reserve(posMap.size());
    m_frameToDurMap.clear();
    m_durToFrameMap.clear();

    for (auto it = posMap.cbegin(); it != posMap.cend(); ++it)
    {
        PosMapEntry e = {it.key(), it.key() * m_keyframeDist, *it};
        m_positionMap.push_back(e);
    }

    if (!m_positionMap.empty() && !(m_ringBuffer && m_ringBuffer->IsDisc()))
        m_indexOffset = m_positionMap[0].index;

    if (!m_positionMap.empty())
    {
        LOG(VB_PLAYBACK, LOG_INFO, LOC +
            QString("Position map filled from DB to: %1")
                .arg(m_positionMap.back().index));
    }

    uint64_t last = 0;
    for (auto it = durMap.cbegin(); it != durMap.cend(); ++it)
    {
        m_frameToDurMap[it.key()] = it.value();
        m_durToFrameMap[it.value()] = it.key();
        last = it.key();
    }

    if (!m_durToFrameMap.empty())
    {
        LOG(VB_PLAYBACK, LOG_INFO, LOC +
            QString("Duration map filled from DB to: %1").arg(last));
    }

    return true;
}

/** \fn DecoderBase::PosMapFromEnc(void)
 *  \brief Queries encoder for position map data
 *         that has not been committed to the DB yet.
 *
 *  PosMapFromDb(void) must be called first in order
 *  to set positionMapType and keyframedist correctly.
 *
 */
bool DecoderBase::PosMapFromEnc(void)
{
    if (!m_parent || m_keyframeDist < 1)
        return false;

    unsigned long long start = 0;
    {
        QMutexLocker locker(&m_positionMapLock);
        if (!m_positionMap.empty())
            start = m_positionMap.back().index + 1;
    }

    frm_pos_map_t posMap;
    frm_pos_map_t durMap;
    if (!m_parent->PosMapFromEnc(start, posMap, durMap))
        return false;

    QMutexLocker locker(&m_positionMapLock);

    // append this new position map to class's
    m_positionMap.reserve(m_positionMap.size() + posMap.size());

    long long last_index = 0;
    if (!m_positionMap.empty())
        last_index = m_positionMap.back().index;
    for (auto it = posMap.cbegin(); it != posMap.cend(); ++it)
    {
        if (it.key() <= last_index)
            continue;

        PosMapEntry e = {it.key(), it.key() * m_keyframeDist, *it};
        m_positionMap.push_back(e);
    }

    if (!m_positionMap.empty() && !(m_ringBuffer && m_ringBuffer->IsDisc()))
        m_indexOffset = m_positionMap[0].index;

    if (!m_positionMap.empty())
    {
        LOG(VB_PLAYBACK, LOG_INFO, LOC +
            QString("Position map filled from Encoder to: %1")
                .arg(m_positionMap.back().index));
    }

    bool isEmpty = m_frameToDurMap.empty();
    if (!isEmpty)
    {
        frm_pos_map_t::const_iterator it = m_frameToDurMap.cend();
        --it;
        last_index = it.key();
    }
    for (frm_pos_map_t::const_iterator it = durMap.cbegin();
         it != durMap.cend(); ++it)
    {
        if (!isEmpty && it.key() <= last_index)
            continue; // we released the m_positionMapLock for a few ms...
        m_frameToDurMap[it.key()] = it.value();
        m_durToFrameMap[it.value()] = it.key();
    }

    if (!m_frameToDurMap.empty())
    {
        frm_pos_map_t::const_iterator it = m_frameToDurMap.cend();
        --it;
        LOG(VB_PLAYBACK, LOG_INFO, LOC +
            QString("Duration map filled from Encoder to: %1").arg(it.key()));
    }

    return true;
}

unsigned long DecoderBase::GetPositionMapSize(void) const
{
    QMutexLocker locker(&m_positionMapLock);
    return m_positionMap.size();
}

/** \fn DecoderBase::SyncPositionMap()
 *  \brief Updates the position map used for skipping frames.
 *
 *   There are different sources for position maps,
 *   depending on where we are getting the stream from.
 *
 *   positionmap sources:
 *   live tv:
 *   1. remote encoder
 *   2. stream parsing
 *   decide keyframedist based on samples from remote encoder
 *
 *   watching recording:
 *   1. initial fill from db
 *   2. incremental from remote encoder, until it finishes recording
 *   3. then db again (which should be the final time)
 *   4. stream parsing
 *   decide keyframedist based on which table in db
 *
 *   watching prerecorded:
 *   1. initial fill from db is all that's needed
 */
bool DecoderBase::SyncPositionMap(void)
{
    LOG(VB_PLAYBACK, LOG_INFO, LOC +
        QString("Resyncing position map. posmapStarted = %1"
                " livetv(%2) watchingRec(%3)")
            .arg((int) m_posmapStarted).arg(m_livetv).arg(m_watchingRecording));

    if (m_dontSyncPositionMap)
        return false;

    unsigned long old_posmap_size = GetPositionMapSize();
    unsigned long new_posmap_size = old_posmap_size;

    if (m_livetv || m_watchingRecording)
    {
        if (!m_posmapStarted)
        {
            // starting up -- try first from database
            PosMapFromDb();
            new_posmap_size = GetPositionMapSize();
            LOG(VB_PLAYBACK, LOG_INFO, LOC +
                QString("SyncPositionMap watchingrecording, from DB: "
                        "%1 entries") .arg(new_posmap_size));
        }
        // always try to get more from encoder
        if (!PosMapFromEnc())
        {
            LOG(VB_PLAYBACK, LOG_INFO, LOC +
                QString("SyncPositionMap watchingrecording no entries "
                        "from encoder, try DB"));
            PosMapFromDb(); // try again from db
        }

        new_posmap_size = GetPositionMapSize();
        LOG(VB_PLAYBACK, LOG_INFO, LOC +
            QString("SyncPositionMap watchingrecording total: %1 entries")
                .arg(new_posmap_size));
    }
    else
    {
        // watching prerecorded ... just get from db
        if (!m_posmapStarted)
        {
            PosMapFromDb();

            new_posmap_size = GetPositionMapSize();
            LOG(VB_PLAYBACK, LOG_INFO, LOC +
                QString("SyncPositionMap prerecorded, from DB: %1 entries")
                    .arg(new_posmap_size));
        }
    }

    bool ret_val = new_posmap_size > old_posmap_size;

    if (ret_val && m_keyframeDist > 0)
    {
        long long totframes = 0;
        std::chrono::seconds length = 0s;

        if (m_ringBuffer && m_ringBuffer->IsDVD())
        {
            length = m_ringBuffer->DVD()->GetTotalTimeOfTitle();
            QMutexLocker locker(&m_positionMapLock);
            totframes = m_positionMap.back().index;
        }
        else if (m_ringBuffer && m_ringBuffer->IsBD())
        {
            length = m_ringBuffer->BD()->GetTotalTimeOfTitle();
            QMutexLocker locker(&m_positionMapLock);
            totframes = m_positionMap.back().index;
        }
        else
        {
            QMutexLocker locker(&m_positionMapLock);
            totframes = m_positionMap.back().index * m_keyframeDist;
            if (m_fps != 0.0)
                length = secondsFromFloat((totframes * 1.0) / m_fps);
        }

        m_parent->SetFileLength(length, totframes);
        m_parent->SetKeyframeDistance(m_keyframeDist);
        m_posmapStarted = true;

        LOG(VB_PLAYBACK, LOG_INFO, LOC +
            QString("SyncPositionMap, new totframes: %1, new length: %2, "
                    "posMap size: %3")
                .arg(totframes).arg(length.count()).arg(new_posmap_size));
    }
    m_recordingHasPositionMap |= (0 != new_posmap_size);
    {
        QMutexLocker locker(&m_positionMapLock);
        m_lastPositionMapUpdate = QDateTime::currentDateTime();
    }
    return ret_val;
}

// returns true iff found exactly
// searches position if search_pos, index otherwise
bool DecoderBase::FindPosition(long long desired_value, bool search_adjusted,
                               int &lower_bound, int &upper_bound)
{
    QMutexLocker locker(&m_positionMapLock);
    // Binary search
    auto size = (long long) m_positionMap.size();
    long long lower = -1;
    long long upper = size;

    if (!search_adjusted && m_keyframeDist > 0)
        desired_value /= m_keyframeDist;

    while (upper - 1 > lower)
    {
        long long i = (upper + lower) / 2;
        long long value = 0;
        if (search_adjusted)
            value = m_positionMap[i].adjFrame;
        else
            value = m_positionMap[i].index - m_indexOffset;
        if (value == desired_value)
        {
            // found it
            upper_bound = i;
            lower_bound = i;

            LOG(VB_PLAYBACK, LOG_INFO, LOC +
                QString("FindPosition(%1, search%2 adjusted)")
                    .arg(desired_value).arg((search_adjusted) ? "" : " not") +
                QString(" --> [%1:%2(%3)]")
                    .arg(i).arg(GetKey(m_positionMap[i]))
                    .arg(m_positionMap[i].pos));

            return true;
        }
        if (value > desired_value)
            upper = i;
        else
            lower = i;
    }
    // Did not find it exactly -- return bounds

    if (search_adjusted)
    {
        while (lower >= 0 && m_positionMap[lower].adjFrame > desired_value)
            lower--;
        while (upper < size && m_positionMap[upper].adjFrame < desired_value)
            upper++;
    }
    else
    {
        while (lower >= 0 &&
               (m_positionMap[lower].index - m_indexOffset) > desired_value)
            lower--;
        while (upper < size &&
               (m_positionMap[upper].index - m_indexOffset) < desired_value)
            upper++;
    }
    // keep in bounds
    lower = std::max(lower, 0LL);
    upper = std::min(upper, size - 1LL);

    upper_bound = upper;
    lower_bound = lower;
    bool empty = m_positionMap.empty();

    LOG(VB_PLAYBACK, LOG_INFO, LOC +
        QString("FindPosition(%1, search%3 adjusted)")
            .arg(desired_value).arg((search_adjusted) ? "" : " not") +
        QString(" --> \n\t\t\t[%1:%2(%3),%4:%5(%6)]")
            .arg(lower_bound)
            .arg(empty ? -1 : GetKey(m_positionMap[lower_bound]))
            .arg(empty ? -1 : m_positionMap[lower_bound].pos)
            .arg(upper_bound)
            .arg(empty ? -1 : GetKey(m_positionMap[upper_bound]))
            .arg(empty ? -1 : m_positionMap[upper_bound].pos));

    return false;
}

uint64_t DecoderBase::SavePositionMapDelta(long long first, long long last)
{
    MythTimer ttm;
    MythTimer ctm;
    MythTimer stm;
    ttm.start();

    QMutexLocker locker(&m_positionMapLock);
    MarkTypes type = m_positionMapType;
    uint64_t saved = 0;

    if (!m_playbackInfo || (m_positionMapType == MARK_UNSET))
        return saved;

    ctm.start();
    frm_pos_map_t posMap;
    for (auto & entry : m_positionMap)
    {
        if (entry.index < first)
            continue;
        if (entry.index > last)
            break;

        posMap[entry.index] = entry.pos;
        saved++;
    }

    frm_pos_map_t durMap;
    for (auto it = m_frameToDurMap.cbegin(); it != m_frameToDurMap.cend(); ++it)
    {
        if (it.key() < first)
            continue;
        if (it.key() > last)
            break;
        durMap[it.key()] = it.value();
    }

    locker.unlock();

    stm.start();
    m_playbackInfo->SavePositionMapDelta(posMap, type);
    m_playbackInfo->SavePositionMapDelta(durMap, MARK_DURATION_MS);

#if 0
    LOG(VB_GENERAL, LOG_DEBUG, LOC +
        QString("Saving position map [%1,%2] w/%3 keyframes, "
                "took (%4,%5,%6) ms")
            .arg(first).arg(last).arg(saved)
            .arg(ttm.elapsed())
            .arg(ctm.elapsed()-stm.elapsed()).arg(stm.elapsed()));
#endif

    return saved;
}

bool DecoderBase::DoRewind(long long desiredFrame, bool discardFrames)
{
    LOG(VB_PLAYBACK, LOG_INFO, LOC +
        QString("DoRewind(%1 (%2), %3 discard frames)")
            .arg(desiredFrame).arg(m_framesPlayed)
            .arg((discardFrames) ? "do" : "don't"));

    if (!DoRewindSeek(desiredFrame))
        return false;

    m_framesPlayed = m_lastKey;
    m_fpsSkip = 0;
    m_framesRead = m_lastKey;

    // Do any Extra frame-by-frame seeking for exactseeks mode
    // And flush pre-seek frame if we are allowed to and need to..
    int normalframes = (uint64_t)(desiredFrame - (m_framesPlayed - 1)) > m_seekSnap
        ? desiredFrame - m_framesPlayed : 0;
    normalframes = std::max(normalframes, 0);
    SeekReset(m_lastKey, normalframes, true, discardFrames);

    if (discardFrames || (m_ringBuffer && m_ringBuffer->IsDisc()))
        m_parent->SetFramesPlayed(m_framesPlayed+1);

    return true;
}

long long DecoderBase::GetKey(const PosMapEntry &e) const
{
    long long kf = (m_ringBuffer && m_ringBuffer->IsDisc()) ?
        1LL : m_keyframeDist;
    return (m_hasKeyFrameAdjustTable) ? e.adjFrame :(e.index - m_indexOffset) * kf;
}

bool DecoderBase::DoRewindSeek(long long desiredFrame)
{
    ConditionallyUpdatePosMap(desiredFrame);

    if (!GetPositionMapSize())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "PosMap is empty, can't seek");
        return false;
    }

    if (!m_ringBuffer)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "No ringBuffer yet, can't seek");
        return false;
    }

    // Find keyframe <= desiredFrame, store in lastKey (frames)
    int pre_idx = 0;
    int post_idx = 0;
    FindPosition(desiredFrame, m_hasKeyFrameAdjustTable, pre_idx, post_idx);

    PosMapEntry e {};
    {
        QMutexLocker locker(&m_positionMapLock);
        PosMapEntry e_pre  = m_positionMap[pre_idx];
        PosMapEntry e_post = m_positionMap[post_idx];
        int pos_idx = pre_idx;
        e = e_pre;
        if (((uint64_t) (GetKey(e_post) - desiredFrame)) <= m_seekSnap &&
            m_framesPlayed - 1 > GetKey(e_post) &&
            GetKey(e_post) - desiredFrame <= desiredFrame - GetKey(e_pre))
        {
            // Snap to the right if e_post is within snap distance and
            // is at least as close a snap as e_pre.  Take into
            // account that if framesPlayed has already reached
            // e_post, we should only snap to the left.
            pos_idx = post_idx;
            e = e_post;
        }
        m_lastKey = GetKey(e);

        // ??? Don't rewind past the beginning of the file
        while (e.pos < 0)
        {
            pos_idx++;
            if (pos_idx >= (int)m_positionMap.size())
                return false;

            e = m_positionMap[pos_idx];
            m_lastKey = GetKey(e);
        }
    }

    m_ringBuffer->Seek(e.pos, SEEK_SET);

    return true;
}

void DecoderBase::ResetPosMap(void)
{
    QMutexLocker locker(&m_positionMapLock);
    m_posmapStarted = false;
    m_positionMap.clear();
    m_frameToDurMap.clear();
    m_durToFrameMap.clear();
}

long long DecoderBase::GetLastFrameInPosMap(void) const
{
    long long last_frame = 0;

    QMutexLocker locker(&m_positionMapLock);
    if (!m_positionMap.empty())
        last_frame = GetKey(m_positionMap.back());

    return last_frame;
}

long long DecoderBase::ConditionallyUpdatePosMap(long long desiredFrame)
{
    long long last_frame = GetLastFrameInPosMap();

    if (desiredFrame < 0)
        return last_frame;

    // Resync keyframe map if we are trying to seek to a frame
    // not yet equalled or exceeded in the seek map.
    if (desiredFrame < last_frame)
        return last_frame;

    LOG(VB_PLAYBACK, LOG_INFO, LOC +
        "ConditionallyUpdatePosMap: Not enough info in positionMap," +
        QString("\n\t\t\twe need frame %1 but highest we have is %2.")
            .arg(desiredFrame).arg(last_frame));

    SyncPositionMap();

    last_frame = GetLastFrameInPosMap();

    if (desiredFrame > last_frame)
    {
        LOG(VB_PLAYBACK, LOG_INFO, LOC +
            "ConditionallyUpdatePosMap: Still not "
            "enough info in positionMap after sync, " +
            QString("\n\t\t\twe need frame %1 but highest we have "
                    "is %2. Will attempt to seek frame-by-frame")
                .arg(desiredFrame).arg(last_frame));
    }

    return last_frame;
}

/** \fn DecoderBase::DoFastForward(long long, bool)
 *  \brief Skips ahead or rewinds to desiredFrame.
 *
 *  If discardFrames is true and cached frames are released and playback
 *  continues at the desiredFrame, if it is not any interviening frames
 *  between the last frame already in the buffer and the desiredFrame are
 *  released, but none of the frames decoded at the time this is called
 *  are released.
 */
bool DecoderBase::DoFastForward(long long desiredFrame, bool discardFrames)
{
    LOG(VB_PLAYBACK, LOG_INFO, LOC +
        QString("DoFastForward(%1 (%2), %3 discard frames)")
            .arg(desiredFrame).arg(m_framesPlayed)
            .arg((discardFrames) ? "do" : "don't"));

    if (!m_ringBuffer)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "No ringBuffer yet, can't fast forward");
        return false;
    }

    if (m_ringBuffer->IsDVD() &&
        !m_ringBuffer->IsInDiscMenuOrStillFrame() &&
        m_ringBuffer->DVD()->TitleTimeLeft() < 5s)
    {
        return false;
    }
    // Rewind if we have already played the desiredFrame. The +1 is for
    // MPEG4 NUV files, which need to decode an extra frame sometimes.
    // This shouldn't effect how this works in general because this is
    // only triggered on the first keyframe/frame skip when paused. At
    // that point the decoding is more than one frame ahead of display.
    if (desiredFrame+1 < m_framesPlayed)
        return DoRewind(desiredFrame, discardFrames);
    desiredFrame = std::max(desiredFrame, m_framesPlayed);

    ConditionallyUpdatePosMap(desiredFrame);

    // Fetch last keyframe in position map
    long long last_frame = GetLastFrameInPosMap();

    // If the desiredFrame is past the end of the position map,
    // do some frame-by-frame seeking until we get to it.
    bool needflush = false;
    if (desiredFrame > last_frame)
    {
        LOG(VB_GENERAL, LOG_NOTICE, LOC +
            QString("DoFastForward(): desiredFrame(%1) > last_frame(%2)")
                .arg(desiredFrame).arg(last_frame));

        if (desiredFrame - last_frame > 32)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "DoFastForward(): "
                    "Desired frame is way past the end of the keyframe map!"
                    "\n\t\t\tSeeking to last keyframe instead.");
            desiredFrame = last_frame;
        }

        needflush = true;

        // Handle non-frame-by-frame seeking
        DoFastForwardSeek(last_frame, needflush);

        m_exitAfterDecoded = true; // don't actualy get a frame
        while ((desiredFrame > last_frame) && !m_atEof)
        {
            bool retry = false;
            GetFrame(kDecodeNothing, retry); // don't need to return frame...
            SyncPositionMap();
            last_frame = GetLastFrameInPosMap();
        }
        m_exitAfterDecoded = false; // allow frames to be returned again

        if (m_atEof)
        {
            return false;
        }
    }

    {
        QMutexLocker locker(&m_positionMapLock);
        if (m_positionMap.empty())
        {
            return false;
        }
    }

    // Handle non-frame-by-frame seeking
    DoFastForwardSeek(desiredFrame, needflush);

    // Do any Extra frame-by-frame seeking for exactseeks mode
    // And flush pre-seek frame if we are allowed to and need to..
    int normalframes = (uint64_t)(desiredFrame - (m_framesPlayed - 1)) > m_seekSnap
        ? desiredFrame - m_framesPlayed : 0;
    normalframes = std::max(normalframes, 0);
    SeekReset(m_lastKey, normalframes, needflush, discardFrames);

    if (discardFrames || m_transcoding)
        m_parent->SetFramesPlayed(m_framesPlayed+1);

    return true;
}

/** \fn DecoderBase::DoFastForwardSeek(long long,bool&)
 *  \brief Seeks to the keyframe just before the desiredFrame if exact
 *         seeks  is enabled, or the frame just after it if exact seeks
 *         is not enabled.
 *
 *   The seek is not made if framesPlayed is greater than the keyframe
 *   this would jump too. This means that frame-by-frame seeking after
 *   a keyframe must be done elsewhere.
 *
 *   If the seek is made the needflush parameter is set.
 *
 *  \param desiredFrame frame we are attempting to seek to.
 *  \param needflush    set to true if a seek is made.
 */
void DecoderBase::DoFastForwardSeek(long long desiredFrame, bool &needflush)
{
    if (!m_ringBuffer)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            "No ringBuffer yet, can't fast forward seek");
        return;
    }

    int pre_idx = 0;
    int post_idx = 0;
    FindPosition(desiredFrame, m_hasKeyFrameAdjustTable, pre_idx, post_idx);

    // if exactseeks, use keyframe <= desiredFrame

    PosMapEntry e {};
    PosMapEntry e_pre {};
    PosMapEntry e_post {};
    {
        QMutexLocker locker(&m_positionMapLock);
        e_pre  = m_positionMap[pre_idx];
        e_post = m_positionMap[post_idx];
    }
    e = e_pre;
    if (((uint64_t) (GetKey(e_post) - desiredFrame)) <= m_seekSnap &&
        (m_framesPlayed - 1 >= GetKey(e_pre) ||
         GetKey(e_post) - desiredFrame < desiredFrame - GetKey(e_pre)))
    {
        // Snap to the right if e_post is within snap distance and is
        // a closer snap than e_pre.  Take into account that if
        // framesPlayed has already reached e_pre, we should only snap
        // to the right.
        e = e_post;
    }
    m_lastKey = GetKey(e);

    if (m_framesPlayed < m_lastKey)
    {
        m_ringBuffer->Seek(e.pos, SEEK_SET);
        needflush    = true;
        m_framesPlayed = m_lastKey;
        m_fpsSkip = 0;
        m_framesRead = m_lastKey;
    }
}

void DecoderBase::UpdateFramesPlayed(void)
{
    m_parent->SetFramesPlayed(m_framesPlayed);
}

void DecoderBase::FileChanged(void)
{
    ResetPosMap();
    m_framesPlayed = 0;
    m_framesRead = 0;
    m_totalDuration = MythAVRational(0);

    m_waitingForChange = false;
    m_justAfterChange = true;

    m_parent->FileChangedCallback();
}

void DecoderBase::SetReadAdjust(long long adjust)
{
    m_readAdjust = adjust;
}

void DecoderBase::SetWaitForChange(void)
{
    m_waitingForChange = true;
}

bool DecoderBase::GetWaitForChange(void) const
{
    return m_waitingForChange;
}

uint DecoderBase::GetTrackCount(uint Type)
{
    QMutexLocker locker(&m_trackLock);
    return static_cast<uint>(m_tracks[Type].size());
}

void DecoderBase::SetDecodeAllSubtitles(bool DecodeAll)
{
    m_trackLock.lock();
    m_decodeAllSubtitles = DecodeAll;
    m_trackLock.unlock();
}

QStringList DecoderBase::GetTracks(uint Type)
{
    QMutexLocker locker(&m_trackLock);
    QStringList list;
    for (size_t i = 0; i < m_tracks[Type].size(); i++)
        list += GetTrackDesc(Type, static_cast<uint>(i));
    return list;
}

int DecoderBase::GetTrackLanguageIndex(uint Type, uint TrackNo)
{
    QMutexLocker locker(&m_trackLock);
    if (TrackNo >= m_tracks[Type].size())
        return 0;
    return static_cast<int>(m_tracks[Type][TrackNo].m_language_index);
}

QString DecoderBase::GetTrackDesc(uint Type, uint TrackNo)
{
    QMutexLocker locker(&m_trackLock);
    if (TrackNo >= m_tracks[Type].size())
        return "";

    QString type_msg = toString(static_cast<TrackType>(Type));
    int lang = m_tracks[Type][TrackNo].m_language;
    int hnum = static_cast<int>(TrackNo + 1);
    if (kTrackTypeCC608 == Type)
        hnum = m_tracks[Type][TrackNo].m_stream_id;

    if (!lang)
        return type_msg + QString(" %1").arg(hnum);
    QString lang_msg = iso639_key_toName(lang);
    return type_msg + QString(" %1: %2").arg(hnum).arg(lang_msg);
}

int DecoderBase::GetTrack(uint Type)
{
    QMutexLocker locker(&m_trackLock);
    return m_currentTrack[Type];
}

int DecoderBase::SetTrack(uint Type, int TrackNo)
{
    QMutexLocker locker(&m_trackLock);
    if (TrackNo >= static_cast<int>(m_tracks[Type].size()))
        return -1;

    m_currentTrack[Type] = std::max(-1, TrackNo);
    if (m_currentTrack[Type] < 0)
    {
        m_selectedTrack[Type].m_av_stream_index = -1;
    }
    else
    {
        m_wantedTrack[Type]   = m_tracks[Type][static_cast<size_t>(m_currentTrack[Type])];
        m_selectedTrack[Type] = m_tracks[Type][static_cast<size_t>(m_currentTrack[Type])];
        if (Type == kTrackTypeSubtitle)
        {
            // Rechoose the associated forced track, preferring the same language
            int forcedTrackIndex = BestTrack(Type, true, m_selectedTrack[Type].m_language);
            if (m_tracks[Type][forcedTrackIndex].m_forced)
                m_selectedForcedTrack[Type] = m_tracks[Type][forcedTrackIndex];
        }
    }

    return m_currentTrack[Type];
}

StreamInfo DecoderBase::GetTrackInfo(uint Type, uint TrackNo)
{
    QMutexLocker locker(&m_trackLock);
    if (TrackNo >= m_tracks[Type].size())
    {
        StreamInfo si;
        return si;
    }
    return m_tracks[Type][TrackNo];
}

int DecoderBase::ChangeTrack(uint Type, int Dir)
{
    QMutexLocker locker(&m_trackLock);

    int next_track = -1;
    int size = static_cast<int>(m_tracks[Type].size());
    if (size)
    {
        if (Dir > 0)
            next_track = (std::max(-1, m_currentTrack[Type]) + 1) % size;
        else
            next_track = (std::max(+0, m_currentTrack[Type]) + size - 1) % size;
    }
    return SetTrack(Type, next_track);
}

int DecoderBase::NextTrack(uint Type)
{
    QMutexLocker locker(&m_trackLock);

    int next_track = -1;
    int size = static_cast<int>(m_tracks[Type].size());
    if (size)
        next_track = (std::max(0, m_currentTrack[Type]) + 1) % size;
    return next_track;
}

/** \fn DecoderBase::BestTrack(uint, bool)
 *  \brief Determine the best track according to weights
 *
 * Select the best track.  Primary attribute is to favor or disfavor
 * a forced track. Secondary attribute is language preference,
 * in order of most preferred to least preferred language.
 * Third attribute is track order, preferring the earliesttrack.
 *
 * Whether to favor or disfavor forced is controlled by the second
 * parameter.
 *
 * A preferredlanguage can be specified as third parameter, which
 * will override the user's preferrence list.
 *
 * This function must not be called without taking m_trackLock
 *
 *  \return the highest weighted track, or -1 if none.
*/
int DecoderBase::BestTrack(uint Type, bool forcedPreferred, int preferredLanguage)
{
    LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("Trying to select track (w/lang & %1forced)")
        .arg(forcedPreferred ? "" : "!"));
    const int kForcedWeight   = forcedPreferred ? (1 << 20) : -(1 << 20);
    const int kLanguageWeight = (1 << 10);
    const int kPositionWeight = (1 << 0);
    int bestScore = -1;
    int selTrack = -1;
    uint numStreams = static_cast<uint>(m_tracks[Type].size());

    for (uint i = 0; i < numStreams; i++)
    {
        bool forced = (Type == kTrackTypeSubtitle &&
                        m_tracks[Type][i].m_forced);
        int position = static_cast<int>(numStreams) - static_cast<int>(i);
        int language = 0;
        if (preferredLanguage != 0 && m_tracks[Type][i].m_language == preferredLanguage)
        {
            language = static_cast<int>(m_languagePreference.size()) + 1;
        }
        for (uint j = 0; (language == 0) && (j < m_languagePreference.size()); ++j)
        {
            if (m_tracks[Type][i].m_language == m_languagePreference[j])
                language = static_cast<int>(m_languagePreference.size()) - static_cast<int>(j);
        }
        int score = (1 << 20) +
                    (kForcedWeight * static_cast<int>(forced)) +
                    (kLanguageWeight * language) +
                    (kPositionWeight * position);
        if (score > bestScore)
        {
            bestScore = score;
            selTrack = static_cast<int>(i);
        }
    }

    return selTrack;
}

/** \fn DecoderBase::AutoSelectTrack(uint)
 *  \brief Select best track.
 *
 *   In case there's only one track available, always choose it.
 *
 *   If there is a user selected track we try to find it.
 *
 *   If we can't find the user selected track we try to
 *   picked according to the ISO639Language[0..] settings.
 *
 *   In case there are no ISOLanguage[0..] settings, or no preferred language
 *   is found, the first found track stream is chosen
 *
 *  \return track if a track was selected, -1 otherwise
 */
int DecoderBase::AutoSelectTrack(uint Type)
{
    QMutexLocker locker(&m_trackLock);

    uint numStreams = static_cast<uint>(m_tracks[Type].size());

    if ((m_currentTrack[Type] >= 0) && (m_currentTrack[Type] < static_cast<int>(numStreams)))
        return m_currentTrack[Type]; // track already selected

    if (!numStreams)
    {
        m_currentTrack[Type] = -1;
        m_selectedTrack[Type].m_av_stream_index = -1;
        return -1;
    }

    int selTrack = (1 == numStreams) ? 0 : -1;

    if ((selTrack < 0) && m_wantedTrack[Type].m_language>=-1)
    {
        LOG(VB_PLAYBACK, LOG_INFO, LOC + "Trying to reselect track");
        // Try to reselect user selected track stream.
        // This should find the stream after a commercial
        // break and in some cases after a channel change.
        int  wlang = m_wantedTrack[Type].m_language;
        uint windx = m_wantedTrack[Type].m_language_index;
        for (uint i = 0; i < numStreams; i++)
        {
            if (wlang == m_tracks[Type][i].m_language)
            {
                selTrack = static_cast<int>(i);
                if (windx == m_tracks[Type][i].m_language_index)
                    break;
            }
        }
    }

    if (selTrack < 0)
    {
        // Find best track favoring forced.
        selTrack = BestTrack(Type, true);

        if (Type == kTrackTypeSubtitle)
        {
            if (m_tracks[Type][selTrack].m_forced)
            {
                // A forced AV Subtitle tracks is handled without the user
                // explicitly enabling subtitles. Try to find a good non-forced
                // track that can be swapped to in the case the user does
                // explicitly enable subtitles.
                int nonForcedTrack = BestTrack(Type, false);

                if (!m_tracks[Type][nonForcedTrack].m_forced)
                {
                    m_selectedForcedTrack[Type] = m_tracks[Type][selTrack];
                    selTrack = nonForcedTrack;
                }
            }
        }
    }

    int oldTrack = m_currentTrack[Type];
    m_currentTrack[Type] = selTrack;
    StreamInfo tmp = m_tracks[Type][static_cast<size_t>(m_currentTrack[Type])];
    m_selectedTrack[Type] = tmp;

    if (m_wantedTrack[Type].m_av_stream_index < 0)
        m_wantedTrack[Type] = tmp;

    int lang = m_tracks[Type][static_cast<size_t>(m_currentTrack[Type])].m_language;
    LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("Selected track #%1 (type %2) in the %3 language(%4)")
            .arg(m_currentTrack[Type]+1).arg(Type).arg(iso639_key_toName(lang)).arg(lang));

    if (m_parent && (oldTrack != m_currentTrack[Type]))
        m_parent->tracksChanged(Type);

    return selTrack;
}

void DecoderBase::AutoSelectTracks(void)
{
    for (uint i = 0; i < kTrackTypeCount; i++)
        AutoSelectTrack(i);
}

void DecoderBase::ResetTracks(void)
{
    QMutexLocker locker(&m_trackLock);
    std::fill(m_currentTrack.begin(), m_currentTrack.end(), -1);
}

QString toString(TrackType type)
{
    QString str = QObject::tr("Track");

    if (kTrackTypeAudio == type)
        str = QObject::tr("Audio track");
    else if (kTrackTypeVideo == type)
        str = QObject::tr("Video track");
    else if (kTrackTypeSubtitle == type)
        str = QObject::tr("Subtitle track");
    else if (kTrackTypeCC608 == type)
        str = QObject::tr("CC", "EIA-608 closed captions");
    else if (kTrackTypeCC708 == type)
        str = QObject::tr("ATSC CC", "EIA-708 closed captions");
    else if (kTrackTypeTeletextCaptions == type)
        str = QObject::tr("TT CC", "Teletext closed captions");
    else if (kTrackTypeTeletextMenu == type)
        str = QObject::tr("TT Menu", "Teletext Menu");
    else if (kTrackTypeRawText == type)
        str = QObject::tr("Text", "Text stream");
    else if (kTrackTypeTextSubtitle == type)
        str = QObject::tr("TXT File", "Text File");
    return str;
}

int to_track_type(const QString &str)
{
    int ret = -1;

    if (str.startsWith("AUDIO"))
        ret = kTrackTypeAudio;
    else if (str.startsWith("VIDEO"))
        ret = kTrackTypeVideo;
    else if (str.startsWith("SUBTITLE"))
        ret = kTrackTypeSubtitle;
    else if (str.startsWith("CC608"))
        ret = kTrackTypeCC608;
    else if (str.startsWith("CC708"))
        ret = kTrackTypeCC708;
    else if (str.startsWith("TTC"))
        ret = kTrackTypeTeletextCaptions;
    else if (str.startsWith("TTM"))
        ret = kTrackTypeTeletextMenu;
    else if (str.startsWith("TFL"))
        ret = kTrackTypeTextSubtitle;
    else if (str.startsWith("RAWTEXT"))
        ret = kTrackTypeRawText;
    return ret;
}

QString toString(AudioTrackType type)
{
    QString str;

    switch (type)
    {
        case kAudioTypeAudioDescription :
            str = QObject::tr("Audio Description",
                              "On-screen events described for the visually impaired");
            break;
        case kAudioTypeCleanEffects :
            str = QObject::tr("Clean Effects",
                              "No dialog, background audio only");
            break;
        case kAudioTypeHearingImpaired :
            str = QObject::tr("Hearing Impaired",
                              "Clear dialog for the hearing impaired");
            break;
        case kAudioTypeSpokenSubs :
            str = QObject::tr("Spoken Subtitles",
                              "Subtitles are read out for the visually impaired");
            break;
        case kAudioTypeCommentary :
            str = QObject::tr("Commentary", "Director/Cast commentary track");
            break;
        case kAudioTypeNormal :
        default:
            str = QObject::tr("Normal", "Ordinary audio track");
            break;
    }

    return str;
}

void DecoderBase::SaveTotalDuration(void)
{
    if (!m_playbackInfo || !m_totalDuration.isNonzero() || !m_totalDuration.isValid())
        return;

    m_playbackInfo->SaveTotalDuration(std::chrono::milliseconds{m_totalDuration.toFixed(1000)});
}

void DecoderBase::SaveTotalFrames(void)
{
    if (!m_playbackInfo || !m_framesRead)
        return;

    m_playbackInfo->SaveTotalFrames(m_framesRead);
}

// Linearly interpolate the value for a given key in the map.  If the
// key is outside the range of keys in the map, linearly extrapolate
// using the fallback ratio.
uint64_t DecoderBase::TranslatePosition(const frm_pos_map_t &map,
                                        long long key,
                                        float fallback_ratio)
{
    uint64_t key1 = 0;
    uint64_t key2 = 0;
    uint64_t val1 = 0;
    uint64_t val2 = 0;

    frm_pos_map_t::const_iterator lower = map.lowerBound(key);
    // QMap::lowerBound() finds a key >= the given key.  We want one
    // <= the given key, so back up one element upon > condition.
    if (lower != map.begin() && (lower == map.end() || lower.key() > key))
        --lower;
    if (lower == map.end() || lower.key() > key)
    {
        key1 = 0;
        val1 = 0;
        LOG(VB_PLAYBACK, LOG_DEBUG, LOC +
            QString("TranslatePosition(key=%1): extrapolating to (0,0)")
            .arg(key));
    }
    else
    {
        key1 = lower.key();
        val1 = lower.value();
    }
    // Find the next key >= the given key.  QMap::lowerBound() is
    // precisely correct in this case.
    frm_pos_map_t::const_iterator upper = map.lowerBound(key);
    if (upper == map.end())
    {
        // Extrapolate from (key1,val1) based on fallback_ratio
        key2 = key;
        val2 = llroundf(val1 + (fallback_ratio * (key2 - key1)));
        LOG(VB_PLAYBACK, LOG_DEBUG, LOC +
            QString("TranslatePosition(key=%1, ratio=%2): "
                    "extrapolating to (%3,%4)")
            .arg(key).arg(fallback_ratio).arg(key2).arg(val2));
        return val2;
    }
    key2 = upper.key();
    val2 = upper.value();
    if (key1 == key2) // this happens for an exact keyframe match
        return val2; // can also set key2 = key1 + 1 avoid dividing by zero

    return llround(val1 + ((double) (key - key1) * (val2 - val1) / (key2 - key1)));
}

// Convert from an absolute frame number (not cutlist adjusted) to its
// cutlist-adjusted position in milliseconds.
std::chrono::milliseconds DecoderBase::TranslatePositionFrameToMs(long long position,
                                                 float fallback_framerate,
                                                 const frm_dir_map_t &cutlist)
{
    QMutexLocker locker(&m_positionMapLock);
    // Accurate calculation of duration requires an up-to-date
    // duration map.  However, the last frame (total duration) will
    // almost always appear to be past the end of the duration map, so
    // we limit duration map syncing to once every 3 seconds (a
    // somewhat arbitrary value).
    if (!m_frameToDurMap.empty())
    {
        frm_pos_map_t::const_iterator it = m_frameToDurMap.cend();
        --it;
        if (position > it.key())
        {
            if (!m_lastPositionMapUpdate.isValid() ||
                (QDateTime::currentDateTime() >
                 m_lastPositionMapUpdate.addSecs(3)))
                SyncPositionMap();
        }
    }
    return std::chrono::milliseconds(TranslatePositionAbsToRel(cutlist, position, m_frameToDurMap,
                                     1000 / fallback_framerate));
}

// Convert from a cutlist-adjusted position in milliseconds to its
// absolute frame number (not cutlist-adjusted).
uint64_t DecoderBase::TranslatePositionMsToFrame(std::chrono::milliseconds dur_ms,
                                                 float fallback_framerate,
                                                 const frm_dir_map_t &cutlist)
{
    QMutexLocker locker(&m_positionMapLock);
    // Convert relative position in milliseconds (cutlist-adjusted) to
    // its absolute position in milliseconds (not cutlist-adjusted).
    uint64_t ms = TranslatePositionRelToAbs(cutlist, dur_ms.count(), m_frameToDurMap,
                                            1000 / fallback_framerate);
    // Convert absolute position in milliseconds to its absolute frame
    // number.
    return TranslatePosition(m_durToFrameMap, ms, fallback_framerate / 1000);
}

// Convert from an "absolute" (not cutlist-adjusted) value to its
// "relative" (cutlist-adjusted) mapped value.  Usually the position
// argument is a frame number, the map argument maps frames to
// milliseconds, the fallback_ratio is 1000/framerate_fps, and the
// return value is in milliseconds.
//
// If the map and fallback_ratio arguments are omitted, it simply
// converts from an absolute frame number to a relative
// (cutlist-adjusted) frame number.
uint64_t
DecoderBase::TranslatePositionAbsToRel(const frm_dir_map_t &deleteMap,
                                       uint64_t absPosition, // frames
                                       const frm_pos_map_t &map, // frame->ms
                                       float fallback_ratio)
{
    uint64_t subtraction = 0;
    uint64_t startOfCutRegion = 0;
    bool withinCut = false;
    bool first = true;
    for (frm_dir_map_t::const_iterator i = deleteMap.begin();
         i != deleteMap.end(); ++i)
    {
        if (first)
            withinCut = (i.value() == MARK_CUT_END);
        first = false;
        if (i.key() > absPosition)
            break;
        uint64_t mappedKey = TranslatePosition(map, i.key(), fallback_ratio);
        if (i.value() == MARK_CUT_START && !withinCut)
        {
            withinCut = true;
            startOfCutRegion = mappedKey;
        }
        else if (i.value() == MARK_CUT_END && withinCut)
        {
            withinCut = false;
            subtraction += (mappedKey - startOfCutRegion);
        }
    }
    uint64_t mappedPos = TranslatePosition(map, absPosition, fallback_ratio);
    if (withinCut)
        subtraction += (mappedPos - startOfCutRegion);
    return mappedPos - subtraction;
}

// Convert from a "relative" (cutlist-adjusted) value to its
// "absolute" (not cutlist-adjusted) mapped value.  Usually the
// position argument is in milliseconds, the map argument maps frames
// to milliseconds, the fallback_ratio is 1000/framerate_fps, and the
// return value is also in milliseconds.  Upon return, if necessary,
// the result may need a separate, non-cutlist adjusted conversion
// from milliseconds to frame number, using the inverse
// millisecond-to-frame map and the inverse fallback_ratio; see for
// example TranslatePositionMsToFrame().
//
// If the map and fallback_ratio arguments are omitted, it simply
// converts from a relatve (cutlist-adjusted) frame number to an
// absolute frame number.
uint64_t
DecoderBase::TranslatePositionRelToAbs(const frm_dir_map_t &deleteMap,
                                       uint64_t relPosition, // ms
                                       const frm_pos_map_t &map, // frame->ms
                                       float fallback_ratio)
{
    uint64_t addition = 0;
    uint64_t startOfCutRegion = 0;
    bool withinCut = false;
    bool first = true;
    for (frm_dir_map_t::const_iterator i = deleteMap.begin();
         i != deleteMap.end(); ++i)
    {
        if (first)
            withinCut = (i.value() == MARK_CUT_END);
        first = false;
        uint64_t mappedKey = TranslatePosition(map, i.key(), fallback_ratio);
        if (i.value() == MARK_CUT_START && !withinCut)
        {
            withinCut = true;
            startOfCutRegion = mappedKey;
            if (relPosition + addition <= startOfCutRegion)
                break;
        }
        else if (i.value() == MARK_CUT_END && withinCut)
        {
            withinCut = false;
            addition += (mappedKey - startOfCutRegion);
        }
    }
    return relPosition + addition;
}

/*! \brief Find a suitable frame format that is mutually acceptable to the decoder and render device.
 * \note Most hardware decoders will only provide one software frame format.
 * \note Currently only the OpenGL renderer supports anything other than FMT_YV12/AV_PIX_FMT_YUV420P
*/
AVPixelFormat DecoderBase::GetBestVideoFormat(AVPixelFormat* Formats, const VideoFrameTypes* RenderFormats)
{
    for (AVPixelFormat *format = Formats; *format != AV_PIX_FMT_NONE; format++)
    {
        for (auto fmt : *RenderFormats)
            if (MythAVUtil::FrameTypeToPixelFormat(fmt) == *format)
                return *format;
    }
    return AV_PIX_FMT_NONE;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
