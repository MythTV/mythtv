#include "dvdringbuffer.h"
#include "mythdvdplayer.h"
#include "avformatdecoderdvd.h"

extern "C" {
#include "libavcodec/avcodec.h"
}

#define LOC QString("AFD_DVD: ")
#define INVALID_LBA 0xbfffffff

AvFormatDecoderDVD::AvFormatDecoderDVD(
    MythPlayer *parent, const ProgramInfo &pginfo, PlayerFlags flags)
  : AvFormatDecoder(parent, pginfo, flags)
  , m_curContext(NULL)
  , m_lastVideoPkt(NULL)
  , m_lbaLastVideoPkt(INVALID_LBA)
  , m_framesReq(0)
  , m_returnContext(NULL)
{
}

AvFormatDecoderDVD::~AvFormatDecoderDVD()
{
    ReleaseContext(m_curContext);
    ReleaseContext(m_returnContext);

    while (m_contextList.size() > 0)
        m_contextList.takeFirst()->DecrRef();

    ReleaseLastVideoPkt();
}

void AvFormatDecoderDVD::ReleaseLastVideoPkt()
{
    if (m_lastVideoPkt)
    {
        av_free_packet(m_lastVideoPkt);
        delete m_lastVideoPkt;
        m_lastVideoPkt = NULL;
        m_lbaLastVideoPkt = INVALID_LBA;
    }
}

void AvFormatDecoderDVD::ReleaseContext(MythDVDContext *&context)
{
    if (context)
    {
        context->DecrRef();
        context = NULL;
    }
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

int AvFormatDecoderDVD::ReadPacket(AVFormatContext *ctx, AVPacket* pkt)
{
    int result = 0;

    if (m_framesReq > 0)
    {
        m_framesReq--;

        if (m_lastVideoPkt)
        {
            av_copy_packet(pkt, m_lastVideoPkt);

            if (m_lastVideoPkt->pts != AV_NOPTS_VALUE)
                m_lastVideoPkt->pts += pkt->duration;

            if (m_lastVideoPkt->dts != AV_NOPTS_VALUE)
                m_lastVideoPkt->dts += pkt->duration;
        }
        else
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + QString( "Need to generate frame @ %1 - %2 but no frame available!")
                .arg(pkt->pts)
                .arg(m_framesReq));
        }
    }
    else
    {
        bool gotPacket;

        do
        {
            gotPacket = true;

            result = av_read_frame(ctx, pkt);

            while (result == AVERROR_EOF && errno == EAGAIN)
            {
                if (ringBuffer->DVD()->IsReadingBlocked())
                {
                    if (ringBuffer->DVD()->GetLastEvent() == DVDNAV_HOP_CHANNEL)
                    {
                        // Non-seamless jump - clear all buffers
                        m_framesReq = 0;
                        ReleaseContext(m_curContext);

                        while (m_contextList.size() > 0)
                            m_contextList.takeFirst()->DecrRef();

                        Reset(true, false, false);
                        m_audio->Reset();
                        m_parent->DiscardVideoFrames(false);
                    }
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
                pkt->dts = ringBuffer->DVD()->AdjustTimestamp(pkt->dts);
                pkt->pts = ringBuffer->DVD()->AdjustTimestamp(pkt->pts);

                if (m_returnContext)
                {
                    // We've jumped in a slideshow and have had to jump again
                    // to find the right video packet to show so only allow
                    // the packets through that let us find it.
                    gotPacket = false;

                    AVStream *curstream = ic->streams[pkt->stream_index];

                    if ((curstream->codec->codec_type == AVMEDIA_TYPE_VIDEO) ||
                        (curstream->codec->codec_id == AV_CODEC_ID_DVD_NAV))
                    {
                        // Allow video or NAV packets through
                        gotPacket = true;
                    }
                }
            }
        }while(!gotPacket);
    }

    return result;
}

void AvFormatDecoderDVD::CheckContext(int64_t pts)
{
    if (pts != AV_NOPTS_VALUE)
    {
        // Remove any contexts we should have
        // already processed.(but have somehow jumped past)
        while (m_contextList.size() > 0 &&
               pts >= m_contextList.first()->GetEndPTS())
        {
            ReleaseContext(m_curContext);
            m_curContext = m_contextList.takeFirst();

            LOG(VB_GENERAL, LOG_ERR, LOC +
                QString("DVD context missed! lba: %1, curpts: %2, nav end pts: %3")
                .arg(m_curContext->GetLBA())
                .arg(pts)
                .arg(m_curContext->GetEndPTS()));
        }

        // See whether we can take the next context from the list
        if (m_contextList.size() > 0 &&
            pts >= m_contextList.first()->GetStartPTS())
        {
            ReleaseContext(m_curContext);
            m_curContext = m_contextList.takeFirst();

            if (m_curContext->GetLBAPrevVideoFrame() != m_lbaLastVideoPkt)
                ReleaseLastVideoPkt();

            if (m_curContext->GetNumFramesPresent() == 0)
            {
                if (m_lastVideoPkt)
                {
                    // No video frames present, so we need to generate
                    // them based on the last 'sequence end' video packet.
                    m_framesReq = m_curContext->GetNumFrames();
                }
                else
                {
                    // There are no video frames in this VOBU and
                    // we don't have one stored.  We've probably
                    // jumped into the middle of a cell.
                    // Jump back to the first VOBU that contains
                    // video so we can get the video frame we need
                    // before jumping back again.
                    m_framesReq = 0;
                    uint32_t lastVideoSector = m_curContext->GetLBAPrevVideoFrame();

                    if (lastVideoSector != INVALID_LBA)
                    {
                        LOG(VB_PLAYBACK, LOG_DEBUG, LOC + QString( "Missing video.  Jumping to sector %1")
                            .arg(lastVideoSector));

                        ringBuffer->DVD()->SectorSeek(lastVideoSector);

                        m_returnContext = m_curContext;
                        m_curContext = NULL;
                    }
                    else
                    {
                        LOG(VB_GENERAL, LOG_ERR, LOC +
                            QString("Missing video frame and no previous frame available! lba: %1")
                            .arg(m_curContext->GetLBA()));
                    }
                }
            }
            else
            {
                // Normal VOBU with at least one video frame so we don't need to generate frames.
                m_framesReq = 0;
                ReleaseLastVideoPkt();
            }
        }
    }
}


bool AvFormatDecoderDVD::ProcessVideoPacket(AVStream *stream, AVPacket *pkt)
{
    int64_t pts = pkt->pts;

    if (pts == AV_NOPTS_VALUE)
        pts = pkt->dts;

    CheckContext(pts);

    bool ret = AvFormatDecoder::ProcessVideoPacket(stream, pkt);
        
    if( ret &&
        m_curContext &&
        pts != AV_NOPTS_VALUE &&
        pts + pkt->duration == m_curContext->GetSeqEndPTS())
    {
        // If this video frame is the last in the sequence,
        // make a copy of it so we can 'generate' more
        // to fill in the gaps (e.g. when a single frame
        // should be displayed with audio)
        if (!m_lastVideoPkt)
        {
            m_lastVideoPkt = new AVPacket;
            memset(m_lastVideoPkt, 0, sizeof(AVPacket));
        }
        else
        {
            av_free_packet(m_lastVideoPkt);
        }

        av_init_packet(m_lastVideoPkt);
        av_copy_packet(m_lastVideoPkt, pkt);
        m_lbaLastVideoPkt = m_curContext->GetLBA();

        if (m_returnContext)
        {
            // After seeking in a slideshow, we needed to find
            // the previous video frame to display.
            // We've found it now, so we need to jump back to
            // where we originally wanted to be.
            LOG(VB_PLAYBACK, LOG_DEBUG, LOC + QString( "Found video packet, jumping back to sector %1")
                .arg(m_returnContext->GetLBA()));

            ringBuffer->DVD()->SectorSeek(m_returnContext->GetLBA());
            ReleaseContext(m_returnContext);
        }
        else
        {
            if (m_lastVideoPkt->pts != AV_NOPTS_VALUE)
                m_lastVideoPkt->pts += pkt->duration;

            if (m_lastVideoPkt->dts != AV_NOPTS_VALUE)
                m_lastVideoPkt->dts += pkt->duration;

            m_framesReq = m_curContext->GetNumFrames() - m_curContext->GetNumFramesPresent();

            LOG(VB_PLAYBACK, LOG_DEBUG, LOC + QString( "SeqEnd @ %1 - require %2 frame(s)")
                .arg(pkt->pts)
                .arg(m_framesReq));
        }
    }

    return ret;
}

bool AvFormatDecoderDVD::ProcessVideoFrame(AVStream *stream, AVFrame *mpa_pic)
{
    bool ret = true;

    if (m_returnContext == NULL)
    {
        // Only process video frames if we're not searching for
        // the previous video frame after seeking in a slideshow.
        ret = AvFormatDecoder::ProcessVideoFrame(stream, mpa_pic);
    }

    return ret;
}

bool AvFormatDecoderDVD::ProcessDataPacket(AVStream *curstream, AVPacket *pkt,
                                           DecodeType decodetype)
{
    bool ret = true;

    if (curstream->codec->codec_id == AV_CODEC_ID_DVD_NAV)
    {
        MythDVDContext* context = ringBuffer->DVD()->GetDVDContext();

        if (context)
            m_contextList.append(context);

        if ((m_curContext == NULL) && (m_contextList.size() > 0))
        {
            // If we don't have a current context, use
            // the first in the list
            CheckContext(m_contextList.first()->GetStartPTS());

            if (m_lastVideoPkt && m_curContext)
            {
                // If there was no current context but there was
                // a video packet, we've almost certainly been
                // seeking so set the timestamps of the video
                // packet to the new context to ensure we don't
                // get sync errors.
                m_lastVideoPkt->pts = m_curContext->GetStartPTS();
                m_lastVideoPkt->dts = m_lastVideoPkt->pts;
            }
        }
        else
        if (m_lastVideoPkt)
        {
            // If we've been generating frames, see whether this
            // new context should be used already (handles
            // situations where a VOBU consists of only a NAV
            // packet and nothing else)
            CheckContext(m_lastVideoPkt->pts);
        }
    }
    else
    {
        ret = AvFormatDecoder::ProcessDataPacket(curstream, pkt, decodetype);
    }

    return ret;
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

        int trackNo = -1;
        int dvdTrack = ringBuffer->DVD()->GetTrack(kTrackTypeAudio);

        for (uint i = 0; i < GetTrackCount(kTrackTypeAudio); i++)
        {
            LOG(VB_PLAYBACK, LOG_INFO, LOC +
                QString("DVD Audio Track Map Stream id #%1, av_stream_idx %2, MPEG stream 0x%3, lang %4")
                    .arg(tracks[kTrackTypeAudio][i].stream_id)
                    .arg(tracks[kTrackTypeAudio][i].av_stream_index)
                    .arg(ic->streams[tracks[kTrackTypeAudio][i].av_stream_index]->id,0,16)
                    .arg(iso639_key_toName(tracks[kTrackTypeAudio][i].language)));

            // Find the audio track in our list that maps to the
            // selected track in the ringbuffer (the ringbuffer's
            // list should be in the same order but can have gaps,
            // so we look for the track with the same index)
            if (tracks[kTrackTypeAudio][i].stream_id == dvdTrack)
                trackNo = i;
        }

        if (trackNo < 0 && GetTrackCount(kTrackTypeAudio) > 0)
        {
            // Take the first track
            trackNo = 0;
        }

        if (trackNo >= 0)
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
    for (uint i = 0; i < ic->nb_streams; i++)
    {
        AVStream *st = ic->streams[i];
        if (st && st->codec->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            selectedTrack[kTrackTypeVideo].av_stream_index = i;
            break;
        }
    }
}

int AvFormatDecoderDVD::GetAudioLanguage(uint audio_index, uint stream_index)
{
    (void)audio_index;
    if ((ic->streams[stream_index]->id >= 0) &&
        ringBuffer && ringBuffer->IsDVD())
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
    {
        int logical_idx = ringBuffer->DVD()->GetAudioTrackNum(ic->streams[stream_index]->id);
        type = ringBuffer->DVD()->GetAudioTrackType(logical_idx);
    }

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
