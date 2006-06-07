#include <unistd.h>

#include <algorithm>
using namespace std;

#include "NuppelVideoPlayer.h"
#include "remoteencoder.h"
#include "mythcontext.h"
#include "mythdbcon.h"
#include "decoderbase.h"
#include "programinfo.h"
#include "livetvchain.h"
#include "DVDRingBuffer.h"
#include "iso639.h"

#define LOC QString("Dec: ")
#define LOC_ERR QString("Dec, Error: ")

DecoderBase::DecoderBase(NuppelVideoPlayer *parent, ProgramInfo *pginfo) 
    : m_parent(parent), m_playbackinfo(NULL),

      ringBuffer(NULL), nvr_enc(NULL),

      current_width(640), current_height(480),
      current_aspect(1.33333), fps(29.97),

      framesPlayed(0), framesRead(0), lastKey(0), keyframedist(-1),
      indexOffset(0),

      ateof(false), exitafterdecoded(false), transcoding(false),

      hasFullPositionMap(false), recordingHasPositionMap(false),
      posmapStarted(false), positionMapType(MARK_UNSET),

      exactseeks(false), livetv(false), watchingrecording(false),

      hasKeyFrameAdjustTable(false), lowbuffers(false),
      getrawframes(false), getrawvideo(false),
      errored(false), waitingForChange(false), readAdjust(0),
      justAfterChange(false),
      // language preference
      languagePreference(iso639_get_language_key_list())
{
    ResetTracks();
    tracks[kTrackTypeAudio].push_back(StreamInfo(0, 0, 0, 0));
    tracks[kTrackTypeCC608].push_back(StreamInfo(0, 0, 0, 1));
    tracks[kTrackTypeCC608].push_back(StreamInfo(0, 0, 2, 3));

    if (pginfo)
        m_playbackinfo = new ProgramInfo(*pginfo);
}

DecoderBase::~DecoderBase()
{
    if (m_playbackinfo)
        delete m_playbackinfo;
}

void DecoderBase::SetProgramInfo(ProgramInfo *pginfo)
{
    if (m_playbackinfo)
        delete m_playbackinfo;
    m_playbackinfo = NULL;

    if (pginfo)
        m_playbackinfo = new ProgramInfo(*pginfo);
}

void DecoderBase::Reset(void)
{
    SeekReset(0, 0, true, true);

    m_positionMap.clear();
    framesPlayed = 0;
    framesRead = 0;

    ateof = false;
}

void DecoderBase::SeekReset(long long, uint, bool, bool)
{
    readAdjust = 0;
}

void DecoderBase::setWatchingRecording(bool mode)
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
    QMap<long long, long long> posMap;

    if (ringBuffer->isDVD())
    {
        long long totframes;
        keyframedist = 15;
        fps = ringBuffer->DVD()->GetFrameRate();
        if (fps < 26 && fps > 24)
           keyframedist = 12;
        totframes = (long long)(ringBuffer->DVD()->GetTotalTimeOfTitle() * fps);
        posMap[totframes] = ringBuffer->DVD()->GetTotalReadPosition();
    }
    else if ((positionMapType == MARK_UNSET) ||
        (keyframedist == -1))
    {
        m_playbackinfo->GetPositionMap(posMap, MARK_GOP_BYFRAME);
        if (!posMap.empty())
        {
            positionMapType = MARK_GOP_BYFRAME;
            if (keyframedist == -1) 
                keyframedist = 1;
        }
        else
        {
            m_playbackinfo->GetPositionMap(posMap, MARK_GOP_START);
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
                m_playbackinfo->GetPositionMap(posMap, MARK_KEYFRAME);
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
        m_playbackinfo->GetPositionMap(posMap, positionMapType);
    }

    if (posMap.empty())
        return false; // no position map in recording

    m_positionMap.clear();
    m_positionMap.reserve(posMap.size());

    for (QMap<long long,long long>::const_iterator it = posMap.begin();
         it != posMap.end(); it++) 
    {
        PosMapEntry e = {it.key(), it.key() * keyframedist, it.data()};
        m_positionMap.push_back(e);
    }
    if (!m_positionMap.empty())
    {
        VERBOSE(VB_PLAYBACK, QString("Position map filled from DB to: %1")
                .arg((long int) m_positionMap[m_positionMap.size()-1].index));
        if (!ringBuffer->isDVD())
            indexOffset = m_positionMap[0].index;
    }

    return true;
}

bool DecoderBase::PosMapFromEnc(void)
{
    // Reads only new positionmap entries from encoder
    if (!(livetv || (nvr_enc && nvr_enc->IsValidRecorder())))
        return false;

    // if livetv, and we're not the last entry, don't get it from the encoder
    if (livetv)
    {
        LiveTVChain *chain = m_parent->GetTVChain();
        if (chain->HasNext())
            return false;
    }

    QMap<long long, long long> posMap;
    
    int start = 0;
    unsigned int size = m_positionMap.size();
    if (size > 0)
        start = m_positionMap[size-1].index + 1;

    int end = nvr_enc->GetFramesWritten();

    if (!end)
    {
        VERBOSE(VB_PLAYBACK, QString("PosMapFromEnc: Warning, tried to fetch "
                                     "PositionMap from Encoder but encoder "
                                     "returned framesWritten == 0"));
        return false;
    }

    if (size > 0 && keyframedist > 0) 
        end /= keyframedist;

    VERBOSE(VB_PLAYBACK, QString("Filling position map from %1 to %2")
                                 .arg(start).arg(end));

    nvr_enc->FillPositionMap(start, end, posMap);
    if (keyframedist == -1 && posMap.size() > 1)
    {
        // If the indices are sequential, index is by keyframe num
        // else it is by frame num
        QMap<long long,long long>::const_iterator i1 = posMap.begin();
        QMap<long long,long long>::const_iterator i2 = i1;
        i2++;
        if (i1.key() + 1 == i2.key()) 
        {
            //cerr << "keyframedist to 15/12 due to encoder" << endl;
            positionMapType = MARK_GOP_START;
            keyframedist = 15;
            if (fps < 26 && fps > 24)
                keyframedist = 12;
        }
        else
        {
            //cerr << "keyframedist to 1 due to encoder" << endl;
            positionMapType = MARK_GOP_BYFRAME;
            keyframedist = 1;
        }
    }

    // append this new position map to class's
    m_positionMap.reserve(m_positionMap.size() + posMap.size());
    for (QMap<long long,long long>::const_iterator it = posMap.begin();
         it != posMap.end(); it++) 
    {
        PosMapEntry e = {it.key(), it.key() * keyframedist, it.data()};
        m_positionMap.push_back(e);
    }
    if (!m_positionMap.empty())
    {
        VERBOSE(VB_PLAYBACK, QString("Position map filled from Encoder to: %1")
                .arg((long int) m_positionMap[m_positionMap.size()-1].index));
    }

    return true;
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
    VERBOSE(VB_PLAYBACK, "Resyncing position map. posmapStarted = "
            << (int) posmapStarted << " livetv(" << livetv << ") "
            << "watchingRec(" << watchingrecording << ")");

    unsigned int old_posmap_size = m_positionMap.size();
    
    if (livetv || watchingrecording)
    {
        if (!posmapStarted) 
        {
            // starting up -- try first from database
            PosMapFromDb();
            VERBOSE(VB_PLAYBACK,
                    QString("SyncPositionMap watchingrecording, from DB: "
                            "%1 entries")
                    .arg(m_positionMap.size()));
        }
        // always try to get more from encoder
        if (!PosMapFromEnc()) 
        {
            VERBOSE(VB_PLAYBACK,
                    QString("SyncPositionMap watchingrecording no entries "
                            "from encoder, try DB"));
            PosMapFromDb(); // try again from db
        }
        VERBOSE(VB_PLAYBACK,
                QString("SyncPositionMap watchingrecording total: %1 entries")
                .arg(m_positionMap.size()));
    }
    else
    {
        // watching prerecorded ... just get from db
        if (!posmapStarted)
        {
            PosMapFromDb();
            VERBOSE(VB_PLAYBACK,
                    QString("SyncPositionMap prerecorded, from DB: %1 entries")
                    .arg(m_positionMap.size()));
        }
    }

    bool ret_val = m_positionMap.size() > old_posmap_size;
    if (ret_val && keyframedist > 0)
    {
        long long totframes;
        int length;

        if (ringBuffer->isDVD())
        { 
            totframes = m_positionMap[m_positionMap.size() - 1].index;
            length = ringBuffer->DVD()->GetTotalTimeOfTitle();
        }
        else
        { 
            totframes = 
                     m_positionMap[m_positionMap.size()-1].index * keyframedist;
            length = (int)((totframes * 1.0) / fps);
        }

        GetNVP()->SetFileLength(length, totframes);
        GetNVP()->SetKeyframeDistance(keyframedist);
        posmapStarted = true;

        VERBOSE(VB_PLAYBACK,
                QString("SyncPositionMap, new totframes: %1, new length: %2, "
                        "posMap size: %3")
                        .arg((long)totframes).arg(length)
                        .arg(m_positionMap.size()));
    }
    recordingHasPositionMap |= !m_positionMap.empty();
    return ret_val;
}

// returns true iff found exactly
// searches position if search_pos, index otherwise
bool DecoderBase::FindPosition(long long desired_value, bool search_adjusted,
                               int &lower_bound, int &upper_bound)
{
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
    return false;
}

void DecoderBase::SetPositionMap(void)
{
    if (m_playbackinfo && (positionMapType != MARK_UNSET)) 
    {
        QMap<long long, long long> posMap;
        for (unsigned int i=0; i < m_positionMap.size(); i++) 
            posMap[m_positionMap[i].index] = m_positionMap[i].pos;
    
        m_playbackinfo->SetPositionMap(posMap, positionMapType);
    }
}

bool DecoderBase::DoRewind(long long desiredFrame, bool discardFrames)
{
    VERBOSE(VB_PLAYBACK, LOC +
            QString("DoRewind(%1 (%2), %3 discard frames)")
            .arg(desiredFrame).arg(framesPlayed)
            .arg((discardFrames) ? "do" : "don't"));

    if (m_positionMap.empty())
        return false;

    if (!DoRewindSeek(desiredFrame))
        return false;

    framesPlayed = lastKey;
    framesRead = lastKey;

    // Do any Extra frame-by-frame seeking for exactseeks mode
    // And flush pre-seek frame if we are allowed to and need to..
    int normalframes = (exactseeks) ? desiredFrame - framesPlayed : 0;
    normalframes = max(normalframes, 0);
    SeekReset(lastKey, normalframes, true, discardFrames);

    if (discardFrames)
    {
        // We need to tell the NVP and VideoOutput what frame we're on.
        GetNVP()->SetFramesPlayed(framesPlayed+1);
        GetNVP()->getVideoOutput()->SetFramesPlayed(framesPlayed+1);
    }

    return true;
}

long long DecoderBase::GetKey(PosMapEntry &e) const
{
    long long kf  = (ringBuffer->isDVD()) ? 1LL : keyframedist;
    return (hasKeyFrameAdjustTable) ? e.adjFrame :(e.index - indexOffset) * kf;
}

bool DecoderBase::DoRewindSeek(long long desiredFrame)
{
    if (ringBuffer->isDVD())
    {
        long long pos = DVDFindPosition(desiredFrame);
        ringBuffer->Seek(pos, SEEK_SET);
        lastKey = desiredFrame + 1;
        return true;
    }

    // Find keyframe <= desiredFrame, store in lastKey (frames)
    int pre_idx, post_idx;
    FindPosition(desiredFrame, hasKeyFrameAdjustTable, pre_idx, post_idx);

    uint pos_idx  = min(pre_idx, post_idx);
    PosMapEntry e = m_positionMap[pos_idx];
    lastKey = GetKey(e);

    // ??? Don't rewind past the beginning of the file
    while (e.pos < 0)
    {
        pos_idx++;
        if (pos_idx >= m_positionMap.size())
            return false;

        e = m_positionMap[pos_idx];
        lastKey = GetKey(e);
    }

    ringBuffer->Seek(e.pos, SEEK_SET);

    return true;
}

long long DecoderBase::GetLastFrameInPosMap(long long desiredFrame)
{
    long long last_frame = 0;
    if (!m_positionMap.empty())
        last_frame = GetKey(m_positionMap.back());

    // Resync keyframe map if we are trying to seek to a frame
    // not yet in out map and then check for last frame again.
    if ((desiredFrame >= 0) && (desiredFrame > last_frame))
    {
        VERBOSE(VB_PLAYBACK, LOC + "DoFastForward: "
                "Not enough info in positionMap," +
                QString("\n\t\t\twe need frame %1 but highest we have is %2.")
                .arg((long int)desiredFrame).arg((long int)last_frame));

        SyncPositionMap();

        if (!m_positionMap.empty())
            last_frame = GetKey(m_positionMap.back());

        if (desiredFrame > last_frame)
        {
            VERBOSE(VB_PLAYBACK, LOC + "DoFastForward: "
                    "Still not enough info in positionMap after sync, " +
                    QString("\n\t\t\twe need frame %1 but highest we have "
                            "is %2. Will seek frame-by-frame")
                    .arg((long int)desiredFrame).arg((long int)last_frame));
        }
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
    VERBOSE(VB_PLAYBACK, LOC +
            QString("DoFastForward(%1 (%2), %3 discard frames)")
            .arg(desiredFrame).arg(framesPlayed)
            .arg((discardFrames) ? "do" : "don't"));

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

    // Fetch last keyframe in position map
    long long last_frame = GetLastFrameInPosMap(desiredFrame);

    // If the desiredFrame is past the end of the position map,
    // do some frame-by-frame seeking until we get to it.
    bool needflush = false;
    if (desiredFrame > last_frame)
    {
        needflush = true;

        exitafterdecoded = true; // don't actualy get a frame
        while ((desiredFrame > last_frame) && !ateof)
        {
            GetFrame(-1); // don't need to return frame...
            last_frame = GetLastFrameInPosMap(-1);
        }
        exitafterdecoded = false; // allow frames to be returned again

        if (ateof)
        {
            // Re-enable rawframe state if it was enabled before FF
            getrawframes = oldrawstate;
            return false;
        }
    }

    if (m_positionMap.empty())
    {
        // Re-enable rawframe state if it was enabled before FF
        getrawframes = oldrawstate;
        return false;
    }

    // Handle non-frame-by-frame seeking
    DoFastForwardSeek(desiredFrame, needflush);

    // Do any Extra frame-by-frame seeking for exactseeks mode
    // And flush pre-seek frame if we are allowed to and need to..
    int normalframes = (exactseeks) ? desiredFrame - framesPlayed : 0;
    SeekReset(lastKey, normalframes, needflush, discardFrames);

    if (discardFrames)
    {
        // We need to tell the NVP and VideoOutput what frame we're on.
        GetNVP()->SetFramesPlayed(framesPlayed+1);
        GetNVP()->getVideoOutput()->SetFramesPlayed(framesPlayed+1);
    }

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
    if (ringBuffer->isDVD())
    {
        long long pos = DVDFindPosition(desiredFrame);
        ringBuffer->Seek(pos,SEEK_SET);
        needflush    = true;
        lastKey      = desiredFrame+1;
        framesPlayed = lastKey;
        framesRead   = lastKey;
        return;
    }

    int pre_idx, post_idx;
    FindPosition(desiredFrame, hasKeyFrameAdjustTable, pre_idx, post_idx);

    // if exactseeks, use keyframe <= desiredFrame
    uint pos_idx = (exactseeks) ? pre_idx : max(pre_idx, post_idx);

    PosMapEntry e = m_positionMap[pos_idx];
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
    GetNVP()->SetFramesPlayed(framesPlayed);
}

void DecoderBase::FileChanged(void)
{
    m_positionMap.clear();
    framesPlayed = 0;
    framesRead = 0;

    waitingForChange = false;
    justAfterChange = true;

    GetNVP()->FileChangedCallback();
}

void DecoderBase::SetReadAdjust(long long adjust)
{
    readAdjust = adjust;
}

void DecoderBase::SetWaitForChange(void)
{
    waitingForChange = true;
}

bool DecoderBase::GetWaitForChange(void)
{
    return waitingForChange;
}

void DecoderBase::ChangeDVDTrack(bool ffw)
{
    bool result = true;
    if (!ringBuffer->isDVD())
        return;

    uint prevcellstart = ringBuffer->DVD()->GetCellStart();

    if (ffw)
        result = ringBuffer->DVD()->nextTrack();
    else
        ringBuffer->DVD()->prevTrack();

    if (result)
    {
        if ((prevcellstart == 0 && ffw) || (prevcellstart != 0))
        {
            while (prevcellstart == ringBuffer->DVD()->GetCellStart())
                usleep(10000);
        } 

        uint elapsed = ringBuffer->DVD()->GetCellStart();

        if (elapsed == 0)
            SeekReset(framesPlayed, 0, true, true);

        // update frames played
        long long played = DVDCurrentFrameNumber();

        framesPlayed = played;
        GetNVP()->getVideoOutput()->SetFramesPlayed(played + 1);
        GetNVP()->SetFramesPlayed(played + 1);
    }
}

long long DecoderBase::DVDFindPosition(long long desiredFrame)
{
    if (!ringBuffer->isDVD())
        return 0;

    int size = m_positionMap.size() - 1;
    int multiplier = m_positionMap[size].pos / m_positionMap[size].index ;
    return (long long)(desiredFrame * multiplier);
} 

long long DecoderBase::DVDCurrentFrameNumber(void)
{
    if (!ringBuffer->isDVD())
        return 0;

    int size = m_positionMap.size() - 1;
    long long currentpos = ringBuffer->GetReadPosition();
    long long multiplier = (currentpos * m_positionMap[size].index);
    long long currentframe = multiplier / m_positionMap[size].pos;
    return currentframe;
}

QStringList DecoderBase::GetTracks(uint type) const
{
    QStringList list;

    QMutexLocker locker(&avcodeclock);

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

    QMutexLocker locker(&avcodeclock);

    QString type_msg = track_type_to_string(type);
    int lang = tracks[type][trackNo].language;
    int hnum = trackNo + 1;
    if (kTrackTypeCC608 == type)
        hnum = tracks[type][trackNo].stream_id;

    if (!lang)
        return type_msg + QString("-%1").arg(hnum);
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

    QMutexLocker locker(&avcodeclock);

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
    QMutexLocker locker(&avcodeclock);

    if (trackNo >= tracks[type].size())
    {
        StreamInfo si;
        return si;
    }

    return tracks[type][trackNo];
}

bool DecoderBase::InsertTrack(uint type, const StreamInfo &info)
{
    QMutexLocker locker(&avcodeclock);

    for (uint i = 0; i < tracks[type].size(); i++)
        if (info.stream_id == tracks[type][i].stream_id)
            return false;

    tracks[type].push_back(info);

    if (GetNVP())
	GetNVP()->TracksChanged(type);

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
        VERBOSE(VB_PLAYBACK, LOC + "Trying to reselect track");
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
        VERBOSE(VB_PLAYBACK, LOC + "Trying to select track (w/lang)");
        // Find first track stream that matches a language in
        // order of most preferred to least preferred language.
        vector<int>::iterator it = languagePreference.begin();
        for (; it != languagePreference.end() && (selTrack < 0); ++it)
        {
            for (uint i = 0; i < numStreams; i++)
            {
                if (*it == tracks[type][i].language)
                {
                    selTrack = i;
                    break;
                }
            }
        }
    }

    if (selTrack < 0 && numStreams)
    {
        VERBOSE(VB_PLAYBACK, LOC + "Selecting first track");
        selTrack = 0;
    }

    int oldTrack = currentTrack[type];
    currentTrack[type] = (selTrack < 0) ? -1 : selTrack;
    StreamInfo tmp = tracks[type][currentTrack[type]];
    selectedTrack[type] = tmp;

    if (wantedTrack[type].av_stream_index < 0)
        wantedTrack[type] = tmp;
     
    int lang = tracks[type][currentTrack[type]].language;
    VERBOSE(VB_PLAYBACK, LOC + 
            QString("Selected track #%1 in the %2 language(%3)")
            .arg(currentTrack[type]+1)
            .arg(iso639_key_toName(lang)).arg(lang));

    if (GetNVP() && (oldTrack != currentTrack[type]))
        GetNVP()->TracksChanged(type);

    return selTrack;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
