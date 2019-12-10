
#include "dvdringbuffer.h"
#include "mythdvdplayer.h"
#include "avformatdecoderdvd.h"

#include "iso639.h"

extern "C" {
#include "libavcodec/avcodec.h"
}

#include <unistd.h> // for usleep()

#define LOC QString("AFD_DVD: ")

AvFormatDecoderDVD::~AvFormatDecoderDVD()
{
    ReleaseContext(m_curContext);
    ReleaseContext(m_returnContext);

    while (!m_contextList.empty())
        m_contextList.takeFirst()->DecrRef();

    ReleaseLastVideoPkt();
}

void AvFormatDecoderDVD::ReleaseLastVideoPkt()
{
    if (m_lastVideoPkt)
    {
        av_packet_unref(m_lastVideoPkt);
        delete m_lastVideoPkt;
        m_lastVideoPkt = nullptr;
        m_lbaLastVideoPkt = INVALID_LBA;
    }
}

void AvFormatDecoderDVD::ReleaseContext(MythDVDContext *&context)
{
    if (context)
    {
        context->DecrRef();
        context = nullptr;
    }
}

void AvFormatDecoderDVD::Reset(bool reset_video_data, bool seek_reset, bool reset_file)
{
    AvFormatDecoder::Reset(reset_video_data, seek_reset, reset_file);
    SyncPositionMap();
}


void AvFormatDecoderDVD::UpdateFramesPlayed(void)
{
    if (!m_ringBuffer->IsDVD())
        return;

    auto currentpos = (long long)(m_ringBuffer->DVD()->GetCurrentTime() * m_fps);
    m_framesPlayed = m_framesRead = currentpos ;
    m_parent->SetFramesPlayed(currentpos + 1);
}

bool AvFormatDecoderDVD::GetFrame(DecodeType /*Type*/, bool &Retry)
{
    // Always try to decode audio and video for DVDs
    return AvFormatDecoder::GetFrame(kDecodeAV, Retry);
}

int AvFormatDecoderDVD::ReadPacket(AVFormatContext *ctx, AVPacket* pkt, bool& storePacket)
{
    int result = 0;

    if (m_framesReq > 0)
    {
        m_framesReq--;

        if (m_lastVideoPkt)
        {
            av_packet_ref(pkt, m_lastVideoPkt);

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

            do
            {
                if (m_ringBuffer->DVD()->IsReadingBlocked())
                {
                    int32_t lastEvent = m_ringBuffer->DVD()->GetLastEvent();
                    switch(lastEvent)
                    {
                        case DVDNAV_HOP_CHANNEL:
                            // Non-seamless jump - clear all buffers
                            m_framesReq = 0;
                            ReleaseContext(m_curContext);

                            while (!m_contextList.empty())
                                m_contextList.takeFirst()->DecrRef();

                            Reset(true, false, false);
                            m_audio->Reset();
                            m_parent->DiscardVideoFrames(false, false);
                            break;

                        case DVDNAV_WAIT:
                        case DVDNAV_STILL_FRAME:
                            if (m_storedPackets.count() > 0)
                            {
                                // Ringbuffer is waiting for the player
                                // to empty its buffers but we have one or
                                // more frames in our buffer that have not
                                // yet been sent to the player.
                                // Make sure no more frames will be buffered
                                // for the time being and start emptying our
                                // buffer.

                                // Force AvFormatDecoder to stop buffering frames
                                storePacket = false;

                                // Return the first buffered packet
                                AVPacket *storedPkt = m_storedPackets.takeFirst();
                                av_packet_ref(pkt, storedPkt);
                                av_packet_unref(storedPkt);
                                delete storedPkt;

                                return 0;
                            }
                            break;

                        case DVDNAV_NAV_PACKET:
                            // Don't need to do anything here.  There was a timecode discontinuity
                            // and the ringbuffer returned to make sure that any packets still in
                            // ffmpeg's buffers were flushed.
                            break;

                        default:
                            LOG(VB_GENERAL, LOG_ERR, LOC + QString("Unexpected DVD event - %1")
                                .arg(lastEvent));
                            break;
                    }

                    m_ringBuffer->DVD()->UnblockReading();
                }

                avcodeclock->lock();
                result = av_read_frame(ctx, pkt);
                avcodeclock->unlock();

                // Make sure we yield.  Otherwise other threads may not
                // get chance to take the lock.  Shouldn't be necessary
                // but calling up the OSD menu in a still frame without
                // this still causes a deadlock.
                usleep(0);
            }while (m_ringBuffer->DVD()->IsReadingBlocked());

            if (result >= 0)
            {
                pkt->dts = m_ringBuffer->DVD()->AdjustTimestamp(pkt->dts);
                pkt->pts = m_ringBuffer->DVD()->AdjustTimestamp(pkt->pts);

                if (m_returnContext)
                {
                    // We've jumped in a slideshow and have had to jump again
                    // to find the right video packet to show so only allow
                    // the packets through that let us find it.
                    gotPacket = false;

                    AVStream *curstream = m_ic->streams[pkt->stream_index];

                    if ((curstream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) ||
                        (curstream->codecpar->codec_id == AV_CODEC_ID_DVD_NAV))
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
        while (!m_contextList.empty() &&
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
        if (!m_contextList.empty() &&
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

                        m_ringBuffer->DVD()->SectorSeek(lastVideoSector);

                        m_returnContext = m_curContext;
                        m_curContext = nullptr;
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


bool AvFormatDecoderDVD::ProcessVideoPacket(AVStream *stream, AVPacket *pkt, bool &Retry)
{
    int64_t pts = pkt->pts;

    if (pts == AV_NOPTS_VALUE)
        pts = pkt->dts;

    CheckContext(pts);

    bool ret = AvFormatDecoder::ProcessVideoPacket(stream, pkt, Retry);
    if (Retry)
        return ret;

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
            av_packet_unref(m_lastVideoPkt);
        }

        av_init_packet(m_lastVideoPkt);
        av_packet_ref(m_lastVideoPkt, pkt);
        m_lbaLastVideoPkt = m_curContext->GetLBA();

        if (m_returnContext)
        {
            // After seeking in a slideshow, we needed to find
            // the previous video frame to display.
            // We've found it now, so we need to jump back to
            // where we originally wanted to be.
            LOG(VB_PLAYBACK, LOG_DEBUG, LOC + QString( "Found video packet, jumping back to sector %1")
                .arg(m_returnContext->GetLBA()));

            m_ringBuffer->DVD()->SectorSeek(m_returnContext->GetLBA());
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

    if (m_returnContext == nullptr)
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

    if (curstream->codecpar->codec_id == AV_CODEC_ID_DVD_NAV)
    {
        MythDVDContext* context = m_ringBuffer->DVD()->GetDVDContext();

        if (context)
            m_contextList.append(context);

        if ((m_curContext == nullptr) && (!m_contextList.empty()))
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
    if (!m_ringBuffer)
        return;
    if (!m_ringBuffer->IsDVD())
        return;

    if (m_tracks[kTrackTypeAudio].size() > 1)
    {
        stable_sort(m_tracks[kTrackTypeAudio].begin(),
                    m_tracks[kTrackTypeAudio].end());

        int trackNo = -1;
        int dvdTrack = m_ringBuffer->DVD()->GetTrack(kTrackTypeAudio);

        for (uint i = 0; i < GetTrackCount(kTrackTypeAudio); i++)
        {
            LOG(VB_PLAYBACK, LOG_INFO, LOC +
                QString("DVD Audio Track Map Stream id #%1, av_stream_idx %2, MPEG stream 0x%3, lang %4")
                    .arg(m_tracks[kTrackTypeAudio][i].m_stream_id)
                    .arg(m_tracks[kTrackTypeAudio][i].m_av_stream_index)
                    .arg(m_ic->streams[m_tracks[kTrackTypeAudio][i].m_av_stream_index]->id,0,16)
                    .arg(iso639_key_toName(m_tracks[kTrackTypeAudio][i].m_language)));

            // Find the audio track in our list that maps to the
            // selected track in the ringbuffer (the ringbuffer's
            // list should be in the same order but can have gaps,
            // so we look for the track with the same index)
            if (m_tracks[kTrackTypeAudio][i].m_stream_id == dvdTrack)
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

    if (!m_tracks[kTrackTypeSubtitle].empty())
    {
        map<int,uint> lang_sub_cnt;
        map<int,int>  stream2idx;

        // First, create a map containing stream id -> track index
        // of the subtitle streams that have been found so far.
        for (uint n = 0; n < GetTrackCount(kTrackTypeSubtitle); n++)
        {
            int stream_id = m_tracks[kTrackTypeSubtitle][n].m_stream_id & 0x1f;

            stream2idx[stream_id] = n;
        }

        // Get all subtitle tracks from the DVD and filter out any that
        // are not mapped in the current program chain.
        sinfo_vec_t filteredTracks;

        if (!m_ringBuffer->DVD()->IsInMenu())
        {
            for (uint i = 0; i < 32; ++i)
            {
                int streamid = m_ringBuffer->DVD()->GetSubtitleTrackNum(i);
                if (streamid >= 0)
                {
                    // This stream is mapped in the current program chain
                    int lang = m_ringBuffer->DVD()->GetSubtitleLanguage(i);
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
                        filteredTracks.push_back(m_tracks[kTrackTypeSubtitle][trackNo]);
                        filteredTracks.back().m_stream_id &= 0x1f;
                        filteredTracks.back().m_language = lang;
                        filteredTracks.back().m_language_index = lang_indx;
                    }
                }
            }
        }
        m_tracks[kTrackTypeSubtitle] = filteredTracks;

        stable_sort(m_tracks[kTrackTypeSubtitle].begin(),
                    m_tracks[kTrackTypeSubtitle].end());

        int trackNo = -1;
        int selectedStream = m_ringBuffer->DVD()->GetTrack(kTrackTypeSubtitle);

        // Now iterate over the sorted list and try to find the index of the
        // currently selected track.
        for (uint idx = 0; idx < GetTrackCount(kTrackTypeSubtitle); idx++)
        {
            const StreamInfo& stream = m_tracks[kTrackTypeSubtitle][idx];
            int avidx = stream.m_av_stream_index;
            QString mpegstream;

            if (avidx >= 0)
                mpegstream = QString( "0x%1").arg(m_ic->streams[avidx]->id,0,16);
            else
                mpegstream = "n/a";

            LOG(VB_PLAYBACK, LOG_INFO, LOC +
                QString("DVD Subtitle Track Map Stream id #%1, av_stream_idx %2, MPEG #%3, lang %4")
                    .arg(stream.m_stream_id)
                    .arg(stream.m_av_stream_index)
                    .arg(mpegstream)
                    .arg(iso639_key_toName(stream. m_language)));

            if ((selectedStream != -1) && (stream.m_stream_id == selectedStream))
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
    if (!m_ringBuffer->IsDVD())
        return false;

    m_ringBuffer->Seek(DVDFindPosition(desiredFrame), SEEK_SET);
    m_framesPlayed = m_framesRead = m_lastKey = desiredFrame + 1;
    m_frameCounter += 100;
    return true;
}

void AvFormatDecoderDVD::DoFastForwardSeek(long long desiredFrame, bool &needflush)
{
    if (!m_ringBuffer->IsDVD())
        return;

    m_ringBuffer->Seek(DVDFindPosition(desiredFrame),SEEK_SET);
    needflush    = true;
    m_framesPlayed = m_framesRead = m_lastKey = desiredFrame + 1;
    m_frameCounter += 100;
}

void AvFormatDecoderDVD::StreamChangeCheck(void)
{
    if (!m_ringBuffer->IsDVD())
        return;

    if (m_streamsChanged)
    {
        // This was originally in HandleDVDStreamChange
        QMutexLocker locker(avcodeclock);
        ScanStreams(true);
        avcodeclock->unlock();
        m_streamsChanged=false;
    }

    // Update the title length
    if (m_parent->AtNormalSpeed() &&
        m_ringBuffer->DVD()->PGCLengthChanged())
    {
        ResetPosMap();
        SyncPositionMap();
        UpdateFramesPlayed();
    }

    // rescan the non-video streams as necessary
    if (m_ringBuffer->DVD()->AudioStreamsChanged())
        ScanStreams(true);

    // Always use the first video stream
    // (must come after ScanStreams above)
    for (uint i = 0; i < m_ic->nb_streams; i++)
    {
        AVStream *st = m_ic->streams[i];
        if (st && st->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            m_selectedTrack[kTrackTypeVideo].m_av_stream_index = i;
            break;
        }
    }
}

int AvFormatDecoderDVD::GetAudioLanguage(uint audio_index, uint stream_index)
{
    (void)audio_index;
    if ((m_ic->streams[stream_index]->id >= 0) &&
        m_ringBuffer && m_ringBuffer->IsDVD())
    {
        return m_ringBuffer->DVD()->GetAudioLanguage(
            m_ringBuffer->DVD()->GetAudioTrackNum(m_ic->streams[stream_index]->id));
    }
    return iso639_str3_to_key("und");
}

long long AvFormatDecoderDVD::DVDFindPosition(long long desiredFrame)
{
    if (!m_ringBuffer->IsDVD())
        return 0;

    int ffrewSkip = 1;
    int current_speed = 0;
    if (m_parent)
    {
        ffrewSkip = m_parent->GetFFRewSkip();
        current_speed = (int)m_parent->GetNextPlaySpeed();
    }

    if (ffrewSkip == 1 || ffrewSkip == 0)
    {
        int diffTime = (int)ceil((desiredFrame - m_framesPlayed) / m_fps);
        long long desiredTimePos = m_ringBuffer->DVD()->GetCurrentTime() +
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

    if (m_ringBuffer && m_ringBuffer->DVD())
    {
        int logical_idx = m_ringBuffer->DVD()->GetAudioTrackNum(m_ic->streams[stream_index]->id);
        type = m_ringBuffer->DVD()->GetAudioTrackType(logical_idx);
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

    // If the DVD metadata doesn't include the info then we might as well fall through, maybe we'll get lucky
    return AvFormatDecoder::GetAudioTrackType(stream_index);
}
