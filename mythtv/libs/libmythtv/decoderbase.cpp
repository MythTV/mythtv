#include "NuppelVideoPlayer.h"
#include "remoteencoder.h"
#include "mythcontext.h"
#include "mythdbcon.h"
#include "decoderbase.h"
#include "programinfo.h"

DecoderBase::DecoderBase(NuppelVideoPlayer *parent, ProgramInfo *pginfo) 
    : m_parent(parent), m_playbackinfo(pginfo),

      ringBuffer(NULL), nvr_enc(NULL),

      current_width(640), current_height(480),
      current_aspect(1.33333), fps(29.97),

      framesPlayed(0), framesRead(0), lastKey(0), keyframedist(-1),

      ateof(false), exitafterdecoded(false), transcoding(false),

      hasFullPositionMap(false), recordingHasPositionMap(false),
      posmapStarted(false), positionMapType(MARK_UNSET),

      exactseeks(false), livetv(false), watchingrecording(false),

      hasKeyFrameAdjustTable(false), lowbuffers(false),
      getrawframes(false), getrawvideo(false),
      currentAudioTrack(-1),
      errored(false)
{
}

void DecoderBase::Reset(void)
{
    SeekReset(0, 0, true);

    m_positionMap.clear();
    framesPlayed = 0;
    framesRead = 0;
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


    if ((positionMapType == MARK_UNSET) ||
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
    VERBOSE(VB_PLAYBACK, QString("Position map filled from DB to: %1")
            .arg((long int) m_positionMap[m_positionMap.size()-1].index));


    return true;
}

bool DecoderBase::PosMapFromEnc(void)
{
    // Reads only new positionmap entries from encoder
    if (!(livetv || (nvr_enc && nvr_enc->IsValidRecorder())))
        return false;

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
    VERBOSE(VB_PLAYBACK, QString("Position map filled from Encoder to: %1")
            .arg((long int) m_positionMap[m_positionMap.size()-1].index));

    return true;
}

bool DecoderBase::SyncPositionMap(void)
{
    // positionmap sources:
    // live tv:
    // 1. remote encoder
    // 2. stream parsing
    // decide keyframedist based on samples from remote encoder
    //
    // watching recording:
    // 1. initial fill from db
    // 2. incremental from remote encoder, until it finishes recording
    // 3. then db again (which should be the final time)
    // 4. stream parsing
    // decide keyframedist based on which table in db
    //
    // watching prerecorded:
    // 1. initial fill from db is all that's needed

    //cerr << "Resyncing position map. posmapStarted = "
    //     << (int) posmapStarted << endl;

    unsigned int old_posmap_size = m_positionMap.size();
    
    if (livetv)
    {
        PosMapFromEnc();
        VERBOSE(VB_PLAYBACK,
                QString("SyncPositionMap liveTV, from Encoder: %1 entries")
                .arg(m_positionMap.size()));
    }
    else if (watchingrecording)
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
        long long totframes = 
            m_positionMap[m_positionMap.size()-1].index * keyframedist;
        int length = (int)((totframes * 1.0) / fps);
        GetNVP()->SetFileLength(length, totframes);
        GetNVP()->SetKeyframeDistance(keyframedist);
        posmapStarted = true;

        VERBOSE(VB_PLAYBACK,
                QString("SyncPositionMap, new totframes: %1, new length: %2, "
                        "posMap size: %3")
                        .arg((long)totframes).arg(length)
                        .arg(m_positionMap.size()));
    }
    return ret_val;
}

// returns true iff found exactly
// searches position if search_pos, index otherwise
bool DecoderBase::FindPosition(long long desired_value, bool search_adjusted,
                               int &lower_bound, int &upper_bound)
{
    // Binary search
    int upper = m_positionMap.size(), lower = -1;
    if (!search_adjusted && keyframedist > 0)
        desired_value /= keyframedist;

    while (upper - 1 > lower) 
    {
        int i = (upper + lower) / 2;
        long long value;
        if (search_adjusted)
            value = m_positionMap[i].adjFrame;
        else
            value = m_positionMap[i].index;
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
        while (upper < (int)m_positionMap.size() &&
               m_positionMap[upper].adjFrame > desired_value)
            upper++;
    }
    else
    {
        while (lower >= 0 &&m_positionMap[lower].index > desired_value)
            lower--;
        while (upper < (int)m_positionMap.size() && 
               m_positionMap[upper].index < desired_value)
            upper++;
    }
    // keep in bounds
    if (lower < 0)
        lower = 0;
    if (upper >= (int)m_positionMap.size())
        upper = (int)m_positionMap.size() - 1;

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

bool DecoderBase::DoRewind(long long desiredFrame, bool doflush)
{
    if (m_positionMap.empty())
        return false;

    // Find keyframe <= desiredFrame, store in lastKey (frames)
    int pos_idx1, pos_idx2;

    FindPosition(desiredFrame, hasKeyFrameAdjustTable, pos_idx1, pos_idx2);

    int pos_idx = pos_idx1 < pos_idx2 ? pos_idx1 : pos_idx2;

    if (hasKeyFrameAdjustTable)
        lastKey = m_positionMap[pos_idx].adjFrame;
    else
        lastKey = m_positionMap[pos_idx].index * keyframedist;

    long long keyPos = m_positionMap[pos_idx].pos;
    long long curPosition = ringBuffer->GetTotalReadPosition();
    long long diff = keyPos - curPosition;

    // Don't rewind further than we have space to store video
    while (ringBuffer->GetFreeSpaceWithReadChange(diff) <= 0)
    {
        pos_idx++;
        if (pos_idx >= (int)m_positionMap.size())
        {
            diff = 0;
            return false;
        }
        if (hasKeyFrameAdjustTable)
            lastKey = m_positionMap[pos_idx].adjFrame;
        else
            lastKey = m_positionMap[pos_idx].index * keyframedist;
        keyPos = m_positionMap[pos_idx].pos;

        diff = keyPos - curPosition;
    }

    ringBuffer->Seek(diff, SEEK_CUR);
    
    framesPlayed = lastKey;
    framesRead = lastKey;

    int normalframes = desiredFrame - framesPlayed;

    if (!exactseeks)
        normalframes = 0;

    SeekReset(lastKey, normalframes, doflush);

    if (doflush)
    {
        GetNVP()->SetFramesPlayed(framesPlayed+1);
        GetNVP()->getVideoOutput()->SetFramesPlayed(framesPlayed+1);
    }

    return true;
}

bool DecoderBase::DoFastForward(long long desiredFrame, bool doflush)
{
    if (desiredFrame < framesPlayed)
        return DoRewind(desiredFrame, doflush);

    bool oldrawstate = getrawframes;
    getrawframes = false;

    long long last_frame = 0;
    if (!m_positionMap.empty())
        last_frame = m_positionMap[m_positionMap.size()-1].index * keyframedist;

    if (desiredFrame > last_frame)
    {
        VERBOSE(VB_PLAYBACK, QString("DoFastForward: Not enough info in "
                                     "positionMap, we need frame %1 but "
                                     "highest we have is %2")
                                     .arg((long int)desiredFrame)
                                     .arg((long int)last_frame));
        SyncPositionMap();
    }

    bool needflush = false;

    if (!m_positionMap.empty())
        last_frame = m_positionMap[m_positionMap.size()-1].index * keyframedist;

    if (desiredFrame > last_frame)
    {
        VERBOSE(VB_PLAYBACK, QString("DoFastForward: Still Not enough info in "
                                     "positionMap, we need frame %1 but "
                                     "highest we have is %2.  Will seek "
                                     "frame-by-frame")
                                     .arg((long int)desiredFrame)
                                     .arg((long int)last_frame));
    }

    while (desiredFrame > last_frame)
    {
        needflush = true;
        
        exitafterdecoded = true;
        GetFrame(-1);
        exitafterdecoded = false;

        if (!m_positionMap.empty())
            last_frame = m_positionMap[m_positionMap.size()-1].index * 
                                                                  keyframedist; 
        
        if (ateof)
            return false;
    }

    if (m_positionMap.empty())
        return false;

    // Find keyframe >= desiredFrame, store in lastKey
    // if exactseeks, use keyframe <= desiredFrame
    int pos_idx1, pos_idx2;
    FindPosition(desiredFrame, hasKeyFrameAdjustTable, pos_idx1, pos_idx2);

    int pos_idx = pos_idx1 < pos_idx2 ? pos_idx1 : pos_idx2;

    if (hasKeyFrameAdjustTable)
        lastKey = m_positionMap[pos_idx].adjFrame;
    else
        lastKey = m_positionMap[pos_idx].index * keyframedist;

    long long keyPos = m_positionMap[pos_idx].pos;

    if (framesPlayed < lastKey)
    {
        long long diff = keyPos - ringBuffer->GetTotalReadPosition();

        ringBuffer->Seek(diff, SEEK_CUR);
        needflush = true;
    
        framesPlayed = lastKey;
        framesRead = lastKey;
    }

    int normalframes = desiredFrame - framesPlayed;

    if (!exactseeks)
        normalframes = 0;

    if (needflush || normalframes)
        SeekReset(lastKey, normalframes, needflush && doflush);

    if (doflush)
    {
        GetNVP()->SetFramesPlayed(framesPlayed+1);
        GetNVP()->getVideoOutput()->SetFramesPlayed(framesPlayed+1);
    }

    getrawframes = oldrawstate;

    return true;
}

void DecoderBase::UpdateFramesPlayed(void)
{
    GetNVP()->SetFramesPlayed(framesPlayed);
}

