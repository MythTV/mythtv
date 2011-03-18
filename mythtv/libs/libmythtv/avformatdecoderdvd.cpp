#include "dvdringbuffer.h"
#include "mythdvdplayer.h"
#include "avformatdecoderdvd.h"

#define LOC QString("AFD_DVD: ")
#define LOC_ERR QString("AFD_DVD Error: ")
#define LOC_WARN QString("AFD_DVD Warning: ")

AvFormatDecoderDVD::AvFormatDecoderDVD(
    MythPlayer *parent, const ProgramInfo &pginfo,
    bool use_null_video_out, bool allow_private_decode,
    bool no_hardware_decode, AVSpecialDecode av_special_decode)
  : AvFormatDecoder(parent, pginfo, use_null_video_out, allow_private_decode,
                    no_hardware_decode, av_special_decode)
{
}

void AvFormatDecoderDVD::Reset(bool reset_video_data, bool seek_reset, bool reset_file)
{
    AvFormatDecoder::Reset(reset_video_data, seek_reset, reset_file);
    SyncPositionMap();
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
            VERBOSE(VB_PLAYBACK, LOC +
                    QString("DVD Audio Track Map "
                            "Stream id #%1, MPEG stream %2")
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
            VERBOSE(VB_PLAYBACK, LOC +
                    QString("DVD Subtitle Track Map "
                            "Stream id #%1 ")
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
