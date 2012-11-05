#include <unistd.h>
#include <math.h>

#include <algorithm>
using namespace std;

#include "mythconfig.h"

#include "mythplayer.h"
#include "remoteencoder.h"
#include "mythdbcon.h"
#include "mythlogging.h"
#include "decoderbase.h"
#include "programinfo.h"
#include "livetvchain.h"
#include "iso639.h"
#include "DVD/dvdringbuffer.h"
#include "Bluray/bdringbuffer.h"

#define LOC QString("Dec: ")

DecoderBase::DecoderBase(MythPlayer *parent, const ProgramInfo &pginfo)
    : m_parent(parent), m_playbackinfo(new ProgramInfo(pginfo)),
      m_audio(m_parent->GetAudio()), ringBuffer(NULL),

      current_width(640), current_height(480),
      current_aspect(1.33333), fps(29.97),
      bitrate(4000),

      framesPlayed(0), framesRead(0), totalDuration(0),
      lastKey(0), keyframedist(-1), indexOffset(0),

      ateof(false), exitafterdecoded(false), transcoding(false),

      hasFullPositionMap(false), recordingHasPositionMap(false),
      posmapStarted(false), positionMapType(MARK_UNSET),

      m_positionMapLock(QMutex::Recursive),
      dontSyncPositionMap(false),

      seeksnap(UINT64_MAX), livetv(false), watchingrecording(false),

      hasKeyFrameAdjustTable(false), lowbuffers(false),
      getrawframes(false), getrawvideo(false),
      errored(false), waitingForChange(false), readAdjust(0),
      justAfterChange(false),
      video_inverted(false),
      decodeAllSubtitles(false),
      // language preference
      languagePreference(iso639_get_language_key_list())
{
    ResetTracks();
    tracks[kTrackTypeAudio].push_back(StreamInfo(0, 0, 0, 0, 0));
    tracks[kTrackTypeCC608].push_back(StreamInfo(0, 0, 0, 1, 0));
    tracks[kTrackTypeCC608].push_back(StreamInfo(0, 0, 2, 3, 0));
}

DecoderBase::~DecoderBase()
{
    if (m_playbackinfo)
        delete m_playbackinfo;
}

void DecoderBase::SetProgramInfo(const ProgramInfo &pginfo)
{
    if (m_playbackinfo)
        delete m_playbackinfo;
    m_playbackinfo = new ProgramInfo(pginfo);
}

void DecoderBase::Reset(bool reset_video_data, bool seek_reset, bool reset_file)
{
    LOG(VB_PLAYBACK, LOG_INFO, LOC +
        QString("Reset: Video %1, Seek %2, File %3")
            .arg(reset_video_data).arg(seek_reset).arg(reset_file));

    if (seek_reset)
    {
        SeekReset(0, 0, true, true);
    }

    if (reset_video_data)
    {
        ResetPosMap();
        framesPlayed = 0;
        framesRead = 0;
        totalDuration = 0;
        dontSyncPositionMap = false;
    }

    if (reset_file)
    {
        waitingForChange = false;
        SetEof(false);
    }
}

void DecoderBase::SeekReset(long long, uint, bool, bool)
{
    readAdjust = 0;
}

void DecoderBase::SetWatchingRecording(bool mode)
{
    bool wereWatchingRecording = watchingrecording;

    // When we switch from WatchingRecording to WatchingPreRecorded,
    // re-get the positionmap
    posmapStarted = false;
    watchingrecording = mode;

    if (wereWatchingRecording && !watchingrecording)
        SyncPositionMap();
}

bool DecoderBase::PosMapFromDb(void)
{
    if (!m_playbackinfo)
        return false;

    // Overwrites current positionmap with entire contents of database
    frm_pos_map_t posMap;

    if (ringBuffer && ringBuffer->IsDVD())
    {
        long long totframes;
        keyframedist = 15;
        fps = ringBuffer->DVD()->GetFrameRate();
        if (fps < 26 && fps > 24)
           keyframedist = 12;
        totframes = (long long)(ringBuffer->DVD()->GetTotalTimeOfTitle() * fps);
        posMap[totframes] = ringBuffer->DVD()->GetTotalReadPosition();
    }
    else if (ringBuffer && ringBuffer->IsBD())
    {
        long long totframes;
        keyframedist = 15;
        fps = ringBuffer->BD()->GetFrameRate();
        if (fps < 26 && fps > 24)
           keyframedist = 12;
        totframes = (long long)(ringBuffer->BD()->GetTotalTimeOfTitle() * fps);
        posMap[totframes] = ringBuffer->BD()->GetTotalReadPosition();
#if 0
        LOG(VB_PLAYBACK, LOG_DEBUG, LOC +
            QString("%1 TotalTimeOfTitle() in ticks, %2 TotalReadPosition() "
                    "in bytes, %3 is fps")
                .arg(ringBuffer->BD()->GetTotalTimeOfTitle())
                .arg(ringBuffer->BD()->GetTotalReadPosition()).arg(fps));
#endif
    }
    else if ((positionMapType == MARK_UNSET) ||
        (keyframedist == -1))
    {
        m_playbackinfo->QueryPositionMap(posMap, MARK_GOP_BYFRAME);
        if (!posMap.empty())
        {
            positionMapType = MARK_GOP_BYFRAME;
            if (keyframedist == -1)
                keyframedist = 1;
        }
        else
        {
            m_playbackinfo->QueryPositionMap(posMap, MARK_GOP_START);
            if (!posMap.empty())
            {
                positionMapType = MARK_GOP_START;
                if (keyframedist == -1)
                {
                    keyframedist = 15;
                    if (fps < 26 && fps > 24)
                        keyframedist = 12;
                }
            }
            else
            {
                m_playbackinfo->QueryPositionMap(posMap, MARK_KEYFRAME);
                if (!posMap.empty())
                {
                    // keyframedist should be set in the fileheader so no
                    // need to try to determine it in this case
                    positionMapType = MARK_KEYFRAME;
                }
            }
        }
    }
    else
    {
        m_playbackinfo->QueryPositionMap(posMap, positionMapType);
    }

    if (posMap.empty())
        return false; // no position map in recording

    QMutexLocker locker(&m_positionMapLock);
    m_positionMap.clear();
    m_positionMap.reserve(posMap.size());

    for (frm_pos_map_t::const_iterator it = posMap.begin();
         it != posMap.end(); ++it)
    {
        PosMapEntry e = {it.key(), it.key() * keyframedist, *it};
        m_positionMap.push_back(e);
    }

    if (!m_positionMap.empty() && !(ringBuffer && ringBuffer->IsDisc()))
        indexOffset = m_positionMap[0].index;

    if (!m_positionMap.empty())
    {
        LOG(VB_PLAYBACK, LOG_INFO, LOC +
            QString("Position map filled from DB to: %1")
                .arg(m_positionMap.back().index));
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
    if (!m_parent || keyframedist < 1)
        return false;

    unsigned long long start = 0;
    {
        QMutexLocker locker(&m_positionMapLock);
        if (!m_positionMap.empty())
            start = m_positionMap.back().index + 1;
    }

    QMap<long long, long long> posMap;
    if (!m_parent->PosMapFromEnc(start, posMap))
        return false;

    QMutexLocker locker(&m_positionMapLock);

    // append this new position map to class's
    m_positionMap.reserve(m_positionMap.size() + posMap.size());
    long long last_index = m_positionMap.back().index;
    for (QMap<long long,long long>::const_iterator it = posMap.begin();
         it != posMap.end(); ++it)
    {
        if (it.key() <= last_index)
            continue; // we released the m_positionMapLock for a few ms...

        PosMapEntry e = {it.key(), it.key() * keyframedist, *it};
        m_positionMap.push_back(e);
    }

    if (!m_positionMap.empty() && !(ringBuffer && ringBuffer->IsDisc()))
        indexOffset = m_positionMap[0].index;

    if (!m_positionMap.empty())
    {
        LOG(VB_PLAYBACK, LOG_INFO, LOC +
            QString("Position map filled from Encoder to: %1")
                .arg(m_positionMap.back().index));
    }

    return true;
}

unsigned long DecoderBase::GetPositionMapSize(void) const
{
    QMutexLocker locker(&m_positionMapLock);
    return (unsigned long) m_positionMap.size();
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
            .arg((int) posmapStarted).arg(livetv).arg(watchingrecording));

    if (dontSyncPositionMap)
        return false;

    unsigned long old_posmap_size = GetPositionMapSize();
    unsigned long new_posmap_size = old_posmap_size;

    if (livetv || watchingrecording)
    {
        if (!posmapStarted)
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
        if (!posmapStarted)
        {
            PosMapFromDb();

            new_posmap_size = GetPositionMapSize();
            LOG(VB_PLAYBACK, LOG_INFO, LOC +
                QString("SyncPositionMap prerecorded, from DB: %1 entries")
                    .arg(new_posmap_size));
        }
    }

    bool ret_val = new_posmap_size > old_posmap_size;

    if (ret_val && keyframedist > 0)
    {
        long long totframes = 0;
        int length = 0;

        if (ringBuffer && ringBuffer->IsDVD())
        {
            length = ringBuffer->DVD()->GetTotalTimeOfTitle();
            QMutexLocker locker(&m_positionMapLock);
            totframes = m_positionMap.back().index;
        }
        else if (ringBuffer && ringBuffer->IsBD())
        {
            length = ringBuffer->BD()->GetTotalTimeOfTitle();
            QMutexLocker locker(&m_positionMapLock);
            totframes = m_positionMap.back().index;
        }
        else
        {
            QMutexLocker locker(&m_positionMapLock);
            totframes = m_positionMap.back().index * keyframedist;
            if (fps)
                length = (int)((totframes * 1.0) / fps);
        }

        m_parent->SetFileLength(length, totframes);
        m_parent->SetKeyframeDistance(keyframedist);
        posmapStarted = true;

        LOG(VB_PLAYBACK, LOG_INFO, LOC +
            QString("SyncPositionMap, new totframes: %1, new length: %2, "
                    "posMap size: %3")
                .arg(totframes).arg(length).arg(new_posmap_size));
    }
    recordingHasPositionMap |= (0 != new_posmap_size);
    return ret_val;
}

// returns true iff found exactly
// searches position if search_pos, index otherwise
bool DecoderBase::FindPosition(long long desired_value, bool search_adjusted,
                               int &lower_bound, int &upper_bound)
{
    QMutexLocker locker(&m_positionMapLock);
    // Binary search
    long long size  = (long long) m_positionMap.size();
    long long lower = -1;
    long long upper = size;

    if (!search_adjusted && keyframedist > 0)
        desired_value /= keyframedist;

    while (upper - 1 > lower)
    {
        long long i = (upper + lower) / 2;
        long long value;
        if (search_adjusted)
            value = m_positionMap[i].adjFrame;
        else
            value = m_positionMap[i].index - indexOffset;
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
        else if (value > desired_value)
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
               (m_positionMap[lower].index - indexOffset) > desired_value)
            lower--;
        while (upper < size &&
               (m_positionMap[upper].index - indexOffset) < desired_value)
            upper++;
    }
    // keep in bounds
    lower = max(lower, 0LL);
    upper = min(upper, size - 1LL);

    upper_bound = upper;
    lower_bound = lower;

    LOG(VB_PLAYBACK, LOG_INFO, LOC +
        QString("FindPosition(%1, search%3 adjusted)")
            .arg(desired_value).arg((search_adjusted) ? "" : " not") +
        QString(" --> \n\t\t\t[%1:%2(%3),%4:%5(%6)]")
            .arg(lower_bound).arg(GetKey(m_positionMap[lower_bound]))
            .arg(m_positionMap[lower_bound].pos)
            .arg(upper_bound).arg(GetKey(m_positionMap[upper_bound]))
            .arg(m_positionMap[upper_bound].pos));

    return false;
}

uint64_t DecoderBase::SavePositionMapDelta(uint64_t first, uint64_t last)
{
    MythTimer ttm, ctm, stm;
    ttm.start();

    QMutexLocker locker(&m_positionMapLock);
    MarkTypes type = positionMapType;
    uint64_t saved = 0;

    if (!m_playbackinfo || (positionMapType == MARK_UNSET))
        return saved;

    ctm.start();
    frm_pos_map_t posMap;
    for (uint i = 0; i < m_positionMap.size(); i++)
    {
        if ((uint64_t)m_positionMap[i].index < first)
            continue;
        if ((uint64_t)m_positionMap[i].index > last)
            break;

        posMap[m_positionMap[i].index] = m_positionMap[i].pos;
        saved++;
    }

    locker.unlock();

    stm.start();
    m_playbackinfo->SavePositionMapDelta(posMap, type);

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
            .arg(desiredFrame).arg(framesPlayed)
            .arg((discardFrames) ? "do" : "don't"));

    if (!DoRewindSeek(desiredFrame))
        return false;

    framesPlayed = lastKey;
    framesRead = lastKey;

    // Do any Extra frame-by-frame seeking for exactseeks mode
    // And flush pre-seek frame if we are allowed to and need to..
    int normalframes = (uint64_t)(desiredFrame - (framesPlayed - 1)) > seeksnap
        ? desiredFrame - framesPlayed : 0;
    normalframes = max(normalframes, 0);
    SeekReset(lastKey, normalframes, true, discardFrames);

    if (discardFrames || (ringBuffer && ringBuffer->IsDisc()))
        m_parent->SetFramesPlayed(framesPlayed+1);

    return true;
}

long long DecoderBase::GetKey(const PosMapEntry &e) const
{
    long long kf = (ringBuffer && ringBuffer->IsDisc()) ?
        1LL : keyframedist;
    return (hasKeyFrameAdjustTable) ? e.adjFrame :(e.index - indexOffset) * kf;
}

bool DecoderBase::DoRewindSeek(long long desiredFrame)
{
    ConditionallyUpdatePosMap(desiredFrame);

    if (!GetPositionMapSize())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "PosMap is empty, can't seek");
        return false;
    }

    if (!ringBuffer)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "No ringBuffer yet, can't seek");
        return false;
    }

    // Find keyframe <= desiredFrame, store in lastKey (frames)
    int pre_idx, post_idx;
    FindPosition(desiredFrame, hasKeyFrameAdjustTable, pre_idx, post_idx);

    PosMapEntry e;
    {
        QMutexLocker locker(&m_positionMapLock);
        PosMapEntry e_pre  = m_positionMap[pre_idx];
        PosMapEntry e_post = m_positionMap[post_idx];
        int pos_idx = pre_idx;
        e = e_pre;
        if (((uint64_t) (GetKey(e_post) - desiredFrame)) <= seeksnap &&
            framesPlayed - 1 > GetKey(e_post) &&
            GetKey(e_post) - desiredFrame <= desiredFrame - GetKey(e_pre))
        {
            // Snap to the right if e_post is within snap distance and
            // is at least as close a snap as e_pre.  Take into
            // account that if framesPlayed has already reached
            // e_post, we should only snap to the left.
            pos_idx = post_idx;
            e = e_post;
        }
        lastKey = GetKey(e);

        // ??? Don't rewind past the beginning of the file
        while (e.pos < 0)
        {
            pos_idx++;
            if (pos_idx >= (int)m_positionMap.size())
                return false;

            e = m_positionMap[pos_idx];
            lastKey = GetKey(e);
        }
    }

    ringBuffer->Seek(e.pos, SEEK_SET);

    return true;
}

void DecoderBase::ResetPosMap(void)
{
    QMutexLocker locker(&m_positionMapLock);
    posmapStarted = false;
    m_positionMap.clear();
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
            .arg(desiredFrame).arg(framesPlayed)
            .arg((discardFrames) ? "do" : "don't"));

    if (!ringBuffer)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "No ringBuffer yet, can't fast forward");
        return false;
    }

    if (ringBuffer->IsDVD() &&
        !ringBuffer->IsInDiscMenuOrStillFrame() &&
        ringBuffer->DVD()->TitleTimeLeft() < 5)
    {
        return false;
    }
    // Rewind if we have already played the desiredFrame. The +1 is for
    // MPEG4 NUV files, which need to decode an extra frame sometimes.
    // This shouldn't effect how this works in general because this is
    // only triggered on the first keyframe/frame skip when paused. At
    // that point the decoding is more than one frame ahead of display.
    if (desiredFrame+1 < framesPlayed)
        return DoRewind(desiredFrame, discardFrames);
    desiredFrame = max(desiredFrame, framesPlayed);

    // Save rawframe state, for later restoration...
    bool oldrawstate = getrawframes;
    getrawframes = false;

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

        exitafterdecoded = true; // don't actualy get a frame
        while ((desiredFrame > last_frame) && !ateof)
        {
            GetFrame(kDecodeNothing); // don't need to return frame...
            SyncPositionMap();
            last_frame = GetLastFrameInPosMap();
        }
        exitafterdecoded = false; // allow frames to be returned again

        if (ateof)
        {
            // Re-enable rawframe state if it was enabled before FF
            getrawframes = oldrawstate;
            return false;
        }
    }

    {
        QMutexLocker locker(&m_positionMapLock);
        if (m_positionMap.empty())
        {
            // Re-enable rawframe state if it was enabled before FF
            getrawframes = oldrawstate;
            return false;
        }
    }

    // Handle non-frame-by-frame seeking
    DoFastForwardSeek(desiredFrame, needflush);

    // Do any Extra frame-by-frame seeking for exactseeks mode
    // And flush pre-seek frame if we are allowed to and need to..
    int normalframes = (uint64_t)(desiredFrame - (framesPlayed - 1)) > seeksnap
        ? desiredFrame - framesPlayed : 0;
    normalframes = max(normalframes, 0);
    SeekReset(lastKey, normalframes, needflush, discardFrames);

    if (discardFrames)
        m_parent->SetFramesPlayed(framesPlayed+1);

    // Re-enable rawframe state if it was enabled before FF
    getrawframes = oldrawstate;

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
    if (!ringBuffer)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            "No ringBuffer yet, can't fast forward seek");
        return;
    }

    int pre_idx, post_idx;
    FindPosition(desiredFrame, hasKeyFrameAdjustTable, pre_idx, post_idx);

    // if exactseeks, use keyframe <= desiredFrame

    PosMapEntry e, e_pre, e_post;
    {
        QMutexLocker locker(&m_positionMapLock);
        e_pre  = m_positionMap[pre_idx];
        e_post = m_positionMap[post_idx];
    }
    e = e_pre;
    if (((uint64_t) (GetKey(e_post) - desiredFrame)) <= seeksnap &&
        (framesPlayed - 1 >= GetKey(e_pre) ||
         GetKey(e_post) - desiredFrame < desiredFrame - GetKey(e_pre)))
    {
        // Snap to the right if e_post is within snap distance and is
        // a closer snap than e_pre.  Take into account that if
        // framesPlayed has already reached e_pre, we should only snap
        // to the right.
        e = e_post;
    }
    lastKey = GetKey(e);

    if (framesPlayed < lastKey)
    {
        ringBuffer->Seek(e.pos, SEEK_SET);
        needflush    = true;
        framesPlayed = lastKey;
        framesRead = lastKey;
    }
}

void DecoderBase::UpdateFramesPlayed(void)
{
    m_parent->SetFramesPlayed(framesPlayed);
}

void DecoderBase::FileChanged(void)
{
    ResetPosMap();
    framesPlayed = 0;
    framesRead = 0;
    totalDuration = 0;

    waitingForChange = false;
    justAfterChange = true;

    m_parent->FileChangedCallback();
}

void DecoderBase::SetReadAdjust(long long adjust)
{
    readAdjust = adjust;
}

void DecoderBase::SetWaitForChange(void)
{
    waitingForChange = true;
}

bool DecoderBase::GetWaitForChange(void) const
{
    return waitingForChange;
}

QStringList DecoderBase::GetTracks(uint type) const
{
    QStringList list;

    QMutexLocker locker(avcodeclock);

    for (uint i = 0; i < tracks[type].size(); i++)
        list += GetTrackDesc(type, i);

    return list;
}

int DecoderBase::GetTrackLanguageIndex(uint type, uint trackNo) const
{
    if (trackNo >= tracks[type].size())
        return 0;

    return tracks[type][trackNo].language_index;
}

QString DecoderBase::GetTrackDesc(uint type, uint trackNo) const
{
    if (trackNo >= tracks[type].size())
        return "";

    QMutexLocker locker(avcodeclock);

    QString type_msg = toString((TrackType)type);
    int lang = tracks[type][trackNo].language;
    int hnum = trackNo + 1;
    if (kTrackTypeCC608 == type)
        hnum = tracks[type][trackNo].stream_id;

    if (!lang)
        return type_msg + QString(" %1").arg(hnum);
    else
    {
        QString lang_msg = iso639_key_toName(lang);
        return type_msg + QString(" %1: %2").arg(hnum).arg(lang_msg);
    }
}

int DecoderBase::SetTrack(uint type, int trackNo)
{
    if (trackNo >= (int)tracks[type].size())
        return false;

    QMutexLocker locker(avcodeclock);

    currentTrack[type] = max(-1, trackNo);

    if (currentTrack[type] < 0)
        selectedTrack[type].av_stream_index = -1;
    else
    {
        wantedTrack[type]   = tracks[type][currentTrack[type]];
        selectedTrack[type] = tracks[type][currentTrack[type]];
    }

    return currentTrack[type];
}

StreamInfo DecoderBase::GetTrackInfo(uint type, uint trackNo) const
{
    QMutexLocker locker(avcodeclock);

    if (trackNo >= tracks[type].size())
    {
        StreamInfo si;
        return si;
    }

    return tracks[type][trackNo];
}

bool DecoderBase::InsertTrack(uint type, const StreamInfo &info)
{
    QMutexLocker locker(avcodeclock);

    for (uint i = 0; i < tracks[type].size(); i++)
        if (info.stream_id == tracks[type][i].stream_id)
            return false;

    tracks[type].push_back(info);

    if (m_parent)
        m_parent->TracksChanged(type);

    return true;
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
int DecoderBase::AutoSelectTrack(uint type)
{
    uint numStreams = tracks[type].size();

    if ((currentTrack[type] >= 0) &&
        (currentTrack[type] < (int)numStreams))
    {
        return true; // track already selected
    }

    if (!numStreams)
    {
        currentTrack[type] = -1;
        selectedTrack[type].av_stream_index = -1;
        return false; // no tracks available
    }

    int selTrack = (1 == numStreams) ? 0 : -1;

    if ((selTrack < 0) &&
        wantedTrack[type].language>=-1 && numStreams)
    {
        LOG(VB_PLAYBACK, LOG_INFO, LOC + "Trying to reselect track");
        // Try to reselect user selected track stream.
        // This should find the stream after a commercial
        // break and in some cases after a channel change.
        int  wlang = wantedTrack[type].language;
        uint windx = wantedTrack[type].language_index;
        for (uint i = 0; i < numStreams; i++)
        {
            if (wlang == tracks[type][i].language)
                selTrack = i;
            if (windx == tracks[type][i].language_index)
                break;
        }
    }

    if (selTrack < 0 && numStreams)
    {
        // Select the best track.  Primary attribute is to favor a
        // forced track.  Secondary attribute is language preference,
        // in order of most preferred to least preferred language.
        // Third attribute is track order, preferring the earliest
        // track.
        LOG(VB_PLAYBACK, LOG_INFO,
            LOC + "Trying to select track (w/lang & forced)");
        const int kForcedWeight   = (1 << 20);
        const int kLanguageWeight = (1 << 10);
        const int kPositionWeight = (1 << 0);
        int bestScore = -1;
        selTrack = 0;
        for (uint i = 0; i < numStreams; i++)
        {
            int forced = (type == kTrackTypeSubtitle &&
                          tracks[type][i].forced &&
                          m_parent->ForcedSubtitlesFavored());
            int position = numStreams - i;
            int language = 0;
            for (uint j = 0;
                 (language == 0) && (j < languagePreference.size()); ++j)
            {
                if (tracks[type][i].language == languagePreference[j])
                    language = languagePreference.size() - j;
            }
            int score = kForcedWeight * forced
                + kLanguageWeight * language
                + kPositionWeight * position;
            if (score > bestScore)
            {
                bestScore = score;
                selTrack = i;
            }
        }
    }

    int oldTrack = currentTrack[type];
    currentTrack[type] = (selTrack < 0) ? -1 : selTrack;
    StreamInfo tmp = tracks[type][currentTrack[type]];
    selectedTrack[type] = tmp;

    if (wantedTrack[type].av_stream_index < 0)
        wantedTrack[type] = tmp;

    int lang = tracks[type][currentTrack[type]].language;
    LOG(VB_PLAYBACK, LOG_INFO, LOC +
        QString("Selected track #%1 in the %2 language(%3)")
            .arg(currentTrack[type]+1)
            .arg(iso639_key_toName(lang)).arg(lang));

    if (m_parent && (oldTrack != currentTrack[type]))
        m_parent->TracksChanged(type);

    return selTrack;
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

    if (str.left(5) == "AUDIO")
        ret = kTrackTypeAudio;
    else if (str.left(5) == "VIDEO")
        ret = kTrackTypeVideo;
    else if (str.left(8) == "SUBTITLE")
        ret = kTrackTypeSubtitle;
    else if (str.left(5) == "CC608")
        ret = kTrackTypeCC608;
    else if (str.left(5) == "CC708")
        ret = kTrackTypeCC708;
    else if (str.left(3) == "TTC")
        ret = kTrackTypeTeletextCaptions;
    else if (str.left(3) == "TTM")
        ret = kTrackTypeTeletextMenu;
    else if (str.left(3) == "TFL")
        ret = kTrackTypeTextSubtitle;
    else if (str.left(7) == "RAWTEXT")
        ret = kTrackTypeRawText;
    return ret;
}

void DecoderBase::SaveTotalDuration(void)
{
    if (!m_playbackinfo || !totalDuration)
        return;

    m_playbackinfo->SaveTotalDuration(totalDuration);
}

void DecoderBase::SaveTotalFrames(void)
{
    if (!m_playbackinfo || !framesRead)
        return;

    m_playbackinfo->SaveTotalFrames(framesRead);
}


/* vim: set expandtab tabstop=4 shiftwidth=4: */
