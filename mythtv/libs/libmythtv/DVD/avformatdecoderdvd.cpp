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

int64_t AvFormatDecoderDVD::AdjustTimestamp(int64_t timestamp)
{
    int64_t newTimestamp = timestamp;

    if (newTimestamp != AV_NOPTS_VALUE)
    {
        int64_t timediff = ringBuffer->DVD()->GetTimeDiff();
        if (newTimestamp >= timediff)
        {
            newTimestamp -= timediff;
        }
    }

    return newTimestamp;
}

int AvFormatDecoderDVD::ReadPacket(AVFormatContext *ctx, AVPacket* pkt)
{
    int result = av_read_frame(ctx, pkt);

    while (result == AVERROR_EOF && errno == EAGAIN)
    {
        if (ringBuffer->DVD()->IsReadingBlocked())
        {
            ringBuffer->DVD()->UnblockReading();
            result = av_read_frame(ctx, pkt);
        }
        else
        {
            break;
        }
    }

    if (result >= 0)
    {
        pkt->dts = AdjustTimestamp(pkt->dts);
        pkt->pts = AdjustTimestamp(pkt->pts);
    }

    return result;
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
        map<int,uint> lang_sub_cnt;
        map<int,int>  stream2idx;

        // First, create a map containing stream id -> track index
        // of the subtitle streams that have been found so far.
        for (uint n = 0; n < GetTrackCount(kTrackTypeSubtitle); n++)
        {
            int stream_id = tracks[kTrackTypeSubtitle][n].stream_id & 0x1f;

            stream2idx[stream_id] = n;
        }

        // Get all subtitle tracks from the DVD and filter out any that
        // are not mapped in the current program chain.
        sinfo_vec_t filteredTracks;

        if (!ringBuffer->DVD()->IsInMenu())
        {
            for (uint i = 0; i < 32; ++i)
            {
                int streamid = ringBuffer->DVD()->GetSubtitleTrackNum(i);
                if (streamid >= 0)
                {
                    // This stream is mapped in the current program chain
                    int lang = ringBuffer->DVD()->GetSubtitleLanguage(i);
                    int lang_indx = lang_sub_cnt[lang]++;
                    int trackNo = -1;

                    if (stream2idx.count(streamid) != 0)
                        trackNo = stream2idx[streamid];

                    if (trackNo == -1)
                    {
                        // Create a dummy track if the physical stream has not
                        // yet been seen.
                        filteredTracks.push_back(StreamInfo(-1, lang, lang_indx,
                                                            streamid, 0, 0, false, false, false));
                    }
                    else
                    {
                        // Otherwise use the real data
                        filteredTracks.push_back(tracks[kTrackTypeSubtitle][trackNo]);
                        filteredTracks.back().stream_id &= 0x1f;
                        filteredTracks.back().language = lang;
                        filteredTracks.back().language_index = lang_indx;
                    }
                }
            }
        }
        tracks[kTrackTypeSubtitle] = filteredTracks;

        stable_sort(tracks[kTrackTypeSubtitle].begin(),
                    tracks[kTrackTypeSubtitle].end());

        int trackNo = -1;
        int selectedStream = ringBuffer->DVD()->GetTrack(kTrackTypeSubtitle);

        // Now iterate over the sorted list and try to find the index of the
        // currently selected track.
        for (uint idx = 0; idx < GetTrackCount(kTrackTypeSubtitle); idx++)
        {
            const StreamInfo& stream = tracks[kTrackTypeSubtitle][idx];
            int avidx = stream.av_stream_index;
            QString mpegstream;

            if (avidx >= 0)
                mpegstream = QString( "0x%1").arg(ic->streams[avidx]->id,0,16);
            else
                mpegstream = "n/a";

            LOG(VB_PLAYBACK, LOG_INFO, LOC +
                QString("DVD Subtitle Track Map Stream id #%1, av_stream_idx %2, MPEG #%3, lang %4")
                    .arg(stream.stream_id)
                    .arg(stream.av_stream_index)
                    .arg(mpegstream)
                    .arg(iso639_key_toName(stream.language)));

            if ((selectedStream != -1) && (stream.stream_id == selectedStream))
                trackNo = (int)idx;
        }

        uint captionmode = m_parent->GetCaptionMode();
        int trackcount = (int)GetTrackCount(kTrackTypeSubtitle);

        if (captionmode == kDisplayAVSubtitle &&
            (trackNo < 0 || trackNo >= trackcount))
        {
            m_parent->EnableSubtitles(false);
        }
        else if (trackNo >= 0 && trackNo < trackcount)
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
