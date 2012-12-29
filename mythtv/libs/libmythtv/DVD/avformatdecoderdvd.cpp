#include "dvdringbuffer.h"
#include "mythdvdplayer.h"
#include "avformatdecoderdvd.h"

#define LOC QString("AFD_DVD: ")

AvFormatDecoderDVD::AvFormatDecoderDVD(
    MythPlayer *parent, const ProgramInfo &pginfo, PlayerFlags flags)
  : AvFormatDecoder(parent, pginfo, flags)
{
}

void AvFormatDecoderDVD::Reset(bool reset_video_data, bool seek_reset, bool reset_file)
{
    AvFormatDecoder::Reset(reset_video_data, seek_reset, reset_file);
    SyncPositionMap();
}

void AvFormatDecoderDVD::UpdateFramesPlayed(void)
{
    if (!ringBuffer->IsDVD())
        return;

    long long currentpos = (long long)(ringBuffer->DVD()->GetCurrentTime() * fps);
    framesPlayed = framesRead = currentpos ;
    m_parent->SetFramesPlayed(currentpos + 1);
}

bool AvFormatDecoderDVD::GetFrame(DecodeType decodetype)
{
    // Always try to decode audio and video for DVDs
    return AvFormatDecoder::GetFrame( kDecodeAV );
}

void AvFormatDecoderDVD::PostProcessTracks(void)
{
    if (!ringBuffer)
        return;
    if (!ringBuffer->IsDVD())
        return;

    if (tracks[kTrackTypeAudio].size() > 1)
    {
        stable_sort(tracks[kTrackTypeAudio].begin(),
                    tracks[kTrackTypeAudio].end());
        sinfo_vec_t::iterator it = tracks[kTrackTypeAudio].begin();
        for (; it != tracks[kTrackTypeAudio].end(); ++it)
        {
            LOG(VB_PLAYBACK, LOG_INFO, LOC +
                QString("DVD Audio Track Map Stream id #%1, MPEG stream %2")
                    .arg(it->stream_id)
                    .arg(ic->streams[it->av_stream_index]->id));
        }
        int trackNo = ringBuffer->DVD()->GetTrack(kTrackTypeAudio);
        if (trackNo >= (int)GetTrackCount(kTrackTypeAudio))
            trackNo = GetTrackCount(kTrackTypeAudio) - 1;
        SetTrack(kTrackTypeAudio, trackNo);
    }

    if (tracks[kTrackTypeSubtitle].size() > 0)
    {
        stable_sort(tracks[kTrackTypeSubtitle].begin(),
                    tracks[kTrackTypeSubtitle].end());
        sinfo_vec_t::iterator it = tracks[kTrackTypeSubtitle].begin();
        for(; it != tracks[kTrackTypeSubtitle].end(); ++it)
        {
            LOG(VB_PLAYBACK, LOG_INFO, LOC +
                QString("DVD Subtitle Track Map Stream id #%1 ")
                    .arg(it->stream_id));
        }
        stable_sort(tracks[kTrackTypeSubtitle].begin(),
                    tracks[kTrackTypeSubtitle].end());
        int trackNo = ringBuffer->DVD()->GetTrack(kTrackTypeSubtitle);
        uint captionmode = m_parent->GetCaptionMode();
        int trackcount = (int)GetTrackCount(kTrackTypeSubtitle);
        if (captionmode == kDisplayAVSubtitle &&
            (trackNo < 0 || trackNo >= trackcount))
        {
            m_parent->EnableSubtitles(false);
        }
        else if (trackNo >= 0 && trackNo < trackcount &&
                 !ringBuffer->IsInDiscMenuOrStillFrame())
        {
            SetTrack(kTrackTypeSubtitle, trackNo);
            m_parent->EnableSubtitles(true);
        }
    }
}

bool AvFormatDecoderDVD::DoRewindSeek(long long desiredFrame)
{
    if (!ringBuffer->IsDVD())
        return false;

    ringBuffer->Seek(DVDFindPosition(desiredFrame), SEEK_SET);
    framesPlayed = framesRead = lastKey = desiredFrame + 1;
    return true;
}

void AvFormatDecoderDVD::DoFastForwardSeek(long long desiredFrame, bool &needflush)
{
    if (!ringBuffer->IsDVD())
        return;

    ringBuffer->Seek(DVDFindPosition(desiredFrame),SEEK_SET);
    needflush    = true;
    framesPlayed = framesRead = lastKey = desiredFrame + 1;
}

void AvFormatDecoderDVD::StreamChangeCheck(void)
{
    if (!ringBuffer->IsDVD())
        return;

    // Update the title length
    if (m_parent->AtNormalSpeed() &&
        ringBuffer->DVD()->PGCLengthChanged())
    {
        ResetPosMap();
        SyncPositionMap();
        UpdateFramesPlayed();
    }

    // rescan the non-video streams as necessary
    if (ringBuffer->DVD()->AudioStreamsChanged())
        ScanStreams(true);

    // Always use the first video stream
    // (must come after ScanStreams above)
    selectedTrack[kTrackTypeVideo].av_stream_index = 0;
}

int AvFormatDecoderDVD::GetAudioLanguage(uint audio_index, uint stream_index)
{
    (void)audio_index;
    if (ringBuffer && ringBuffer->IsDVD())
    {
        return ringBuffer->DVD()->GetAudioLanguage(
            ringBuffer->DVD()->GetAudioTrackNum(ic->streams[stream_index]->id));
    }
    return iso639_str3_to_key("und");
}

long long AvFormatDecoderDVD::DVDFindPosition(long long desiredFrame)
{
    if (!ringBuffer->IsDVD())
        return 0;

    int diffTime = 0;
    long long desiredTimePos;
    int ffrewSkip = 1;
    int current_speed = 0;
    if (m_parent)
    {
        ffrewSkip = m_parent->GetFFRewSkip();
        current_speed = (int)m_parent->GetNextPlaySpeed();
    }

    if (ffrewSkip == 1 || ffrewSkip == 0)
    {
        diffTime = (int)ceil((desiredFrame - framesPlayed) / fps);
        desiredTimePos = ringBuffer->DVD()->GetCurrentTime() +
                        diffTime;
        if (diffTime <= 0)
            desiredTimePos--;
        else
            desiredTimePos++;

        if (desiredTimePos < 0)
            desiredTimePos = 0;
        return (desiredTimePos * 90000LL);
    }
    return current_speed;
}

AudioTrackType AvFormatDecoderDVD::GetAudioTrackType(uint stream_index)
{
    int type = 0;
    
    if (ringBuffer && ringBuffer->DVD())
        type = ringBuffer->DVD()->GetAudioTrackType(stream_index);
    
    if (type > 0 && type < 5) // These are the only types defined in unofficial documentation
    {
        AudioTrackType ret = kAudioTypeNormal;
        switch (type)
        {
            case 1:
                ret = kAudioTypeNormal;
                break;
            case 2:
                ret = kAudioTypeAudioDescription;
                break;
            case 3: case 4:
                ret = kAudioTypeCommentary;
                break;
        }
        return ret;
    }
    else // If the DVD metadata doesn't include the info then we might as well fall through, maybe we'll get lucky
        return AvFormatDecoder::GetAudioTrackType(stream_index);
}
