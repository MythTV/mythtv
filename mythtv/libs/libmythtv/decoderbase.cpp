#include "NuppelVideoPlayer.h"
#include "remoteencoder.h"
#include "mythcontext.h"
#include "mythdbcon.h"
#include "decoderbase.h"
#include "programinfo.h"

DecoderBase::DecoderBase(NuppelVideoPlayer *parent, MythSqlDatabase *db,
                         ProgramInfo *pginfo) 
{ 
    m_parent = parent; 
    m_db = db;
    m_playbackinfo = pginfo;

    exactseeks = false;
    livetv = false;
    watchingrecording = false;
    nvr_enc = NULL;
    lowbuffers = false; 
    ateof = false;
    exitafterdecoded = false;

    framesRead = 0;
    framesPlayed = 0;

    hasFullPositionMap = false;
    recordingHasPositionMap = false;
    posmapStarted = false;

    hasKeyFrameAdjustMap = false;
    keyFrameAdjustMap = NULL;

    keyframedist = -1;
}

void DecoderBase::Reset(void)
{
    SeekReset(0, 0);

    m_positionMap.clear();
    framesPlayed = 0;
    framesRead = 0;
}

void DecoderBase::setWatchingRecording(bool mode)
{
    // When we switch from WatchingRecording to WatchingPreRecorded,
    // re-get the positionmap
    posmapStarted = false;
    watchingrecording = mode;
}

bool DecoderBase::PosMapFromDb(void)
{
    if (!m_db || !m_playbackinfo)
        return false;

    // Overwrites current positionmap with entire contents of database
    QMap<long long, long long> posMap;

    m_db->lock();

    if ((positionMapType == MARK_UNSET) ||
        (keyframedist == -1))
    {
        m_playbackinfo->GetPositionMap(posMap, MARK_GOP_BYFRAME, m_db->db());
        if (!posMap.empty())
        {
            positionMapType = MARK_GOP_BYFRAME;
            if (keyframedist == -1) 
                keyframedist = 1;
        }
        else
        {
            m_playbackinfo->GetPositionMap(posMap, MARK_GOP_START, m_db->db());
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
                m_playbackinfo->GetPositionMap(posMap, MARK_KEYFRAME,
                                               m_db->db());
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
        m_playbackinfo->GetPositionMap(posMap, positionMapType, m_db->db());
    }

    m_db->unlock();

    if (posMap.empty())
        return false; // no position map in recording

    m_positionMap.clear();
    m_positionMap.reserve(posMap.size());

    for (QMap<long long,long long>::const_iterator it = posMap.begin();
         it != posMap.end(); it++) 
    {
        PosMapEntry e = {it.key(), it.data()};
        m_positionMap.push_back(e);
    }

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
    if (size > 0 && keyframedist > 0) 
        end /= keyframedist;

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
            keyframedist = 15;
            if (fps < 26 && fps > 24)
                keyframedist = 12;
        }
        else
        {
            //cerr << "keyframedist to 1 due to encoder" << endl;
            keyframedist = 1;
        }
    }

    // append this new position map to class's
    m_positionMap.reserve(m_positionMap.size() + posMap.size());
    for (QMap<long long,long long>::const_iterator it = posMap.begin();
         it != posMap.end(); it++) 
    {
        PosMapEntry e = {it.key(), it.data()};
        m_positionMap.push_back(e);
    }
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

    //cerr << "Resyncing position map" << endl;
    unsigned int old_posmap_size = m_positionMap.size();
    
    if (livetv)
    {
        PosMapFromEnc();
        //cerr << "Live TV: from encoder: " << m_positionMap.size() 
        //<< " entries" << endl;
    }
    else if (watchingrecording)
    {
        //cerr << "Watching & recording..." << endl;
        if (!posmapStarted) 
        {
            // starting up -- try first from database
            PosMapFromDb();
            //cerr << "from db: " << m_positionMap.size() << " entries" << endl;
        }
        // always try to get more from encoder
        if (!PosMapFromEnc()) 
        {
            //cerr << "...none from encoder" << endl;
            PosMapFromDb(); // try again from db
        }
        //cerr << "..." << m_positionMap.size() << " total" << endl;
    }
    else
    {
        // watching prerecorded ... just get from db
        if (!posmapStarted)
        {
            PosMapFromDb();
            //cerr << "Prerecorded... from db: " << m_positionMap.size() 
            //<< " (posmapStarted: " << posmapStarted << ")" << endl;
        }
    }

    bool ret_val = m_positionMap.size() > old_posmap_size;
    if (ret_val && keyframedist > 0)
    {
        long long totframes = 
            m_positionMap[m_positionMap.size()-1].index * keyframedist;
        int length = (int)((totframes * 1.0) / fps);
        m_parent->SetFileLength(length, totframes);
        m_parent->SetVideoParams(-1, -1, -1, keyframedist, current_aspect);
        posmapStarted = true;
    }
    return ret_val;
}

// returns true iff found exactly
// searches position if search_pos, index otherwise
bool DecoderBase::FindPosition(long long desired_value, bool search_pos,
                               int &lower_bound, int &upper_bound)
{
    // Binary search
    int upper = m_positionMap.size(), lower = -1;
    if (!search_pos && keyframedist > 0)
        desired_value /= keyframedist;

    while (upper - 1 > lower) 
    {
        int i = (upper + lower) / 2;
        long long value;
        if (search_pos)
            value = m_positionMap[i].pos;
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

    if (search_pos) 
    {
        while (lower >= 0 && m_positionMap[lower].pos > desired_value)
            lower--;
        while (upper < (int)m_positionMap.size() &&
               m_positionMap[upper].pos > desired_value)
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
    QMap<long long, long long> posMap;
    for (unsigned int i=0; i < m_positionMap.size(); i++) 
        posMap[m_positionMap[i].index] = m_positionMap[i].pos;
    
    if (m_playbackinfo && m_db) 
    {
        m_db->lock();
        m_playbackinfo->SetPositionMap(posMap, positionMapType, m_db->db());
        m_db->unlock();
    }
}

int DecoderBase::GetKeyIndex(int keyFrame)
{
    if (hasKeyFrameAdjustMap)
    {
        int accum = 0;
        QMap <long long, int>::Iterator ki;
        for (ki = (*keyFrameAdjustMap).begin();
             ki != (*keyFrameAdjustMap).end(); ki++)
        {
            if (keyFrame < (ki.key() * keyframedist - ki.data() - accum))
                break;
            accum += ki.data();
        }
        return ((keyFrame + accum) / keyframedist);
    }
    else
       return (keyFrame / keyframedist);
}

long long DecoderBase::GetKeyFrameAdjustmentAtPos(long long desiredFrame)
{
    long long adjustment = 0;

    long long desiredKey = desiredFrame / keyframedist;
    QMap <long long, int>::Iterator ki;
    for (ki = (*keyFrameAdjustMap).begin();
         ki != (*keyFrameAdjustMap).end() && ki.key() <= desiredKey; ki++)
    {
        adjustment += ki.data();
    }

    return (adjustment);
}

bool DecoderBase::DoRewind(long long desiredFrame)
{
    if (m_positionMap.empty())
        return false;

    if (hasKeyFrameAdjustMap)
        desiredFrame += GetKeyFrameAdjustmentAtPos(desiredFrame);

    // Find keyframe <= desiredFrame, store in lastKey (frames)
    int pos_idx1, pos_idx2;

    FindPosition(desiredFrame, false, pos_idx1, pos_idx2);

    int pos_idx = pos_idx1 < pos_idx2 ? pos_idx1 : pos_idx2;

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

    SeekReset(lastKey, normalframes);

    UpdateFramesPlayed();

    return true;
}

bool DecoderBase::DoFastForward(long long desiredFrame)
{
    if (hasKeyFrameAdjustMap)
        desiredFrame += GetKeyFrameAdjustmentAtPos(desiredFrame);

    bool oldrawstate = getrawframes;
    getrawframes = false;

    long long last_frame = 0;
    if (!m_positionMap.empty())
        last_frame = m_positionMap[m_positionMap.size()-1].index * keyframedist;

    if (desiredFrame > last_frame) 
        SyncPositionMap();

    bool needflush = false;

    if (!m_positionMap.empty())
        last_frame = m_positionMap[m_positionMap.size()-1].index * keyframedist;

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
    FindPosition(desiredFrame, false, pos_idx1, pos_idx2);
    if (pos_idx1 > pos_idx2) 
    {
        int tmp = pos_idx1;
        pos_idx1 = pos_idx2;
        pos_idx2 = tmp;
    }

    int pos_idx = exactseeks ? pos_idx1 : pos_idx2;
    lastKey = m_positionMap[pos_idx].index * keyframedist;
    long long keyPos = m_positionMap[pos_idx].pos;
    long long number = desiredFrame - framesPlayed;
    long long desiredKey = lastKey;


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
        SeekReset(lastKey, normalframes);

    getrawframes = oldrawstate;

    UpdateFramesPlayed();

    return true;
}

void DecoderBase::UpdateFramesPlayed(void)
{
    m_parent->SetFramesPlayed(framesPlayed);
}

