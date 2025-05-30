// MythTV
#include "libmythbase/iso639.h"
#include "libmythbase/mythlogging.h"

#include "mythdvdbuffer.h"
#include "mythdvdplayer.h"
#include "mythdvddecoder.h"

// FFmpeg
extern "C" {
#include "libavcodec/avcodec.h"
}

// Std
#include <thread>

#define LOC QString("DVDDec: ")

MythDVDDecoder::MythDVDDecoder(MythPlayer *Parent, const ProgramInfo &PGInfo, PlayerFlags Flags)
  : AvFormatDecoder(Parent, PGInfo, Flags)
{
}

MythDVDDecoder::~MythDVDDecoder()
{
    ReleaseContext(m_curContext);
    ReleaseContext(m_returnContext);

    while (!m_contextList.empty())
        m_contextList.takeFirst()->DecrRef();

    ReleaseLastVideoPkt();
}

void MythDVDDecoder::ReleaseLastVideoPkt(void)
{
    if (m_lastVideoPkt)
    {
        av_packet_free(&m_lastVideoPkt);
        m_lbaLastVideoPkt = INVALID_LBA;
    }
}

void MythDVDDecoder::ReleaseContext(MythDVDContext *&Context)
{
    if (Context)
    {
        Context->DecrRef();
        Context = nullptr;
    }
}

void MythDVDDecoder::Reset(bool ResetVideoData, bool SeekReset, bool ResetFile)
{
    AvFormatDecoder::Reset(ResetVideoData, SeekReset, ResetFile);
    SyncPositionMap();
}


void MythDVDDecoder::UpdateFramesPlayed(void)
{
    if (!m_ringBuffer->IsDVD())
        return;

    auto currentpos = static_cast<long long>(m_ringBuffer->DVD()->GetCurrentTime().count() * m_fps);
    m_framesPlayed = m_framesRead = currentpos ;
    m_parent->SetFramesPlayed(static_cast<uint64_t>(currentpos + 1));
}

bool MythDVDDecoder::GetFrame(DecodeType /*Type*/, bool &Retry)
{
    // Always try to decode audio and video for DVDs
    return AvFormatDecoder::GetFrame(kDecodeAV, Retry);
}

int MythDVDDecoder::ReadPacket(AVFormatContext *Ctx, AVPacket* Pkt, bool& StorePacket)
{
    int result = 0;

    if (m_framesReq > 0)
    {
        m_framesReq--;

        if (m_lastVideoPkt)
        {
            av_packet_ref(Pkt, m_lastVideoPkt);
            if (m_lastVideoPkt->pts != AV_NOPTS_VALUE)
                m_lastVideoPkt->pts += Pkt->duration;
            if (m_lastVideoPkt->dts != AV_NOPTS_VALUE)
                m_lastVideoPkt->dts += Pkt->duration;
        }
        else
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + QString( "Need to generate frame @ %1 - %2 but no frame available!")
                .arg(Pkt->pts).arg(m_framesReq));
        }
    }
    else
    {
        bool gotPacket = false;

        while (!gotPacket)
        {
            gotPacket = true;

            m_avCodecLock.lock();
            result = av_read_frame(Ctx, Pkt);
            m_avCodecLock.unlock();
            std::this_thread::yield();

            while (m_ringBuffer->DVD()->IsReadingBlocked())
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
                        // During a seek, the Reset call above resets the frames played
                        // to zero - so we need to re-establish our position. Playback
                        // appears unaffected by removing the Reset call - but better
                        // safe than sorry when it comes to DVD so just update
                        // the frames played.
                        UpdateFramesPlayed();
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
                            StorePacket = false;
                            // Return the first buffered packet
                            AVPacket *storedPkt = m_storedPackets.takeFirst();
                            av_packet_ref(Pkt, storedPkt);
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

                m_avCodecLock.lock();
                result = av_read_frame(Ctx, Pkt);
                m_avCodecLock.unlock();

                // Make sure we yield.  Otherwise other threads may not
                // get chance to take the lock.  Shouldn't be necessary
                // but calling up the OSD menu in a still frame without
                // this still causes a deadlock.
                std::this_thread::yield();
            }

            if (result >= 0)
            {
                Pkt->dts = m_ringBuffer->DVD()->AdjustTimestamp(Pkt->dts);
                Pkt->pts = m_ringBuffer->DVD()->AdjustTimestamp(Pkt->pts);

                if (m_returnContext)
                {
                    // We've jumped in a slideshow and have had to jump again
                    // to find the right video packet to show so only allow
                    // the packets through that let us find it.
                    gotPacket = false;

                    AVStream *curstream = m_ic->streams[Pkt->stream_index];

                    if ((curstream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) ||
                        (curstream->codecpar->codec_id == AV_CODEC_ID_DVD_NAV))
                    {
                        // Allow video or NAV packets through
                        gotPacket = true;
                    }
                }
            }
        }
    }

    return result;
}

void MythDVDDecoder::CheckContext(int64_t Pts)
{
    if (Pts != AV_NOPTS_VALUE)
    {
        // Remove any contexts we should have
        // already processed.(but have somehow jumped past)
        while (!m_contextList.empty() && (Pts >= m_contextList.first()->GetEndPTS()))
        {
            ReleaseContext(m_curContext);
            m_curContext = m_contextList.takeFirst();
            LOG(VB_GENERAL, LOG_ERR, LOC + QString("DVD context missed! lba: %1, curpts: %2, nav end pts: %3")
                .arg(m_curContext->GetLBA()).arg(Pts).arg(m_curContext->GetEndPTS()));
        }

        // See whether we can take the next context from the list
        if (!m_contextList.empty() && (Pts >= m_contextList.first()->GetStartPTS()))
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
                        LOG(VB_PLAYBACK, LOG_DEBUG, LOC + QString("Missing video.  Jumping to sector %1")
                            .arg(lastVideoSector));
                        m_ringBuffer->DVD()->SectorSeek(lastVideoSector);
                        m_returnContext = m_curContext;
                        m_curContext = nullptr;
                    }
                    else
                    {
                        LOG(VB_GENERAL, LOG_ERR, LOC + QString("Missing video frame and no previous frame available! lba: %1")
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


bool MythDVDDecoder::ProcessVideoPacket(AVCodecContext* codecContext, AVStream *Stream, AVPacket *Pkt, bool &Retry)
{
    int64_t pts = Pkt->pts;

    if (pts == AV_NOPTS_VALUE)
        pts = Pkt->dts;

    CheckContext(pts);

    bool ret = AvFormatDecoder::ProcessVideoPacket(codecContext, Stream, Pkt, Retry);
    if (Retry)
        return ret;

    if (ret && m_curContext && (pts != AV_NOPTS_VALUE) && (pts + Pkt->duration == m_curContext->GetSeqEndPTS()))
    {
        // If this video frame is the last in the sequence,
        // make a copy of it so we can 'generate' more
        // to fill in the gaps (e.g. when a single frame
        // should be displayed with audio)
        if (!m_lastVideoPkt)
        {
            // This packet will be freed in the destructor.
            m_lastVideoPkt = av_packet_alloc();
        }
        else
        {
            av_packet_unref(m_lastVideoPkt);
        }
        av_packet_ref(m_lastVideoPkt, Pkt);
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
                m_lastVideoPkt->pts += Pkt->duration;

            if (m_lastVideoPkt->dts != AV_NOPTS_VALUE)
                m_lastVideoPkt->dts += Pkt->duration;

            m_framesReq = m_curContext->GetNumFrames() - m_curContext->GetNumFramesPresent();

            LOG(VB_PLAYBACK, LOG_DEBUG, LOC + QString( "SeqEnd @ %1 - require %2 frame(s)")
                .arg(Pkt->pts).arg(m_framesReq));
        }
    }

    return ret;
}

bool MythDVDDecoder::ProcessVideoFrame(AVCodecContext* codecContext, AVStream *Stream, AVFrame *Frame)
{
    bool ret = true;

    if (m_returnContext == nullptr)
    {
        // Only process video frames if we're not searching for
        // the previous video frame after seeking in a slideshow.
        ret = AvFormatDecoder::ProcessVideoFrame(codecContext, Stream, Frame);
    }

    return ret;
}

bool MythDVDDecoder::ProcessDataPacket(AVStream *Curstream, AVPacket *Pkt,
                                           DecodeType Decodetype)
{
    bool ret = true;

    if (Curstream->codecpar->codec_id == AV_CODEC_ID_DVD_NAV)
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
        else if (m_lastVideoPkt)
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
        ret = AvFormatDecoder::ProcessDataPacket(Curstream, Pkt, Decodetype);
    }

    return ret;
}

void MythDVDDecoder::PostProcessTracks(void)
{
    if (!m_ringBuffer)
        return;
    if (!m_ringBuffer->IsDVD())
        return;

    QMutexLocker locker(&m_trackLock);

    if (m_tracks[kTrackTypeAudio].size() > 1)
    {
        stable_sort(m_tracks[kTrackTypeAudio].begin(), m_tracks[kTrackTypeAudio].end());

        int trackNo = -1;
        int dvdTrack = m_ringBuffer->DVD()->GetTrack(kTrackTypeAudio);

        for (uint i = 0; i < m_tracks[kTrackTypeAudio].size(); i++)
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
                trackNo = static_cast<int>(i);
        }

        if (trackNo < 0 && (!m_tracks[kTrackTypeAudio].empty()))
        {
            // Take the first track
            trackNo = 0;
        }

        if (trackNo >= 0)
            SetTrack(kTrackTypeAudio, trackNo);
    }

    if (!m_tracks[kTrackTypeSubtitle].empty())
    {
        std::map<int,uint> lang_sub_cnt;
        std::map<int,int>  stream2idx;

        // First, create a map containing stream id -> track index
        // of the subtitle streams that have been found so far.
        for (uint n = 0; n < m_tracks[kTrackTypeSubtitle].size(); n++)
        {
            int stream_id = m_tracks[kTrackTypeSubtitle][n].m_stream_id & 0x1f;
            stream2idx[stream_id] = static_cast<int>(n);
        }

        // Get all subtitle tracks from the DVD and filter out any that
        // are not mapped in the current program chain.
        sinfo_vec_t filteredTracks;

        if (!m_ringBuffer->DVD()->IsInMenu())
        {
            for (uint i = 0; i < 32; ++i)
            {
                int8_t streamid = m_ringBuffer->DVD()->GetSubtitleTrackNum(i);
                if (streamid >= 0)
                {
                    // This stream is mapped in the current program chain
                    int lang = static_cast<int>(m_ringBuffer->DVD()->GetSubtitleLanguage(static_cast<int>(i)));
                    uint lang_indx = lang_sub_cnt[lang]++;
                    int trackNo = -1;

                    if (stream2idx.count(streamid) != 0)
                        trackNo = stream2idx[streamid];

                    if (trackNo == -1)
                    {
                        // Create a dummy track if the physical stream has not
                        // yet been seen.
                        filteredTracks.emplace_back(-1, streamid, lang, lang_indx);
                    }
                    else
                    {
                        // Otherwise use the real data
                        filteredTracks.push_back(m_tracks[kTrackTypeSubtitle][static_cast<uint>(trackNo)]);
                        filteredTracks.back().m_stream_id &= 0x1f;
                        filteredTracks.back().m_language = lang;
                        filteredTracks.back().m_language_index = lang_indx;
                    }
                }
            }
        }

        m_tracks[kTrackTypeSubtitle] = filteredTracks;
        stable_sort(m_tracks[kTrackTypeSubtitle].begin(), m_tracks[kTrackTypeSubtitle].end());

        int track = -1;
        int selectedStream = m_ringBuffer->DVD()->GetTrack(kTrackTypeSubtitle);

        // Now iterate over the sorted list and try to find the index of the
        // currently selected track.
        for (uint idx = 0; idx < m_tracks[kTrackTypeSubtitle].size(); idx++)
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
                    .arg(mpegstream,
                         iso639_key_toName(stream. m_language)));

            if ((selectedStream != -1) && (stream.m_stream_id == selectedStream))
                track = static_cast<int>(idx);
        }

        int trackcount = static_cast<int>(m_tracks[kTrackTypeSubtitle].size());
        if (auto * dvdplayer = dynamic_cast<MythDVDPlayer*>(m_parent); dvdplayer && (track < 0 || track >= trackcount))
        {
            emit dvdplayer->DisableDVDSubtitles();
        }
        else if (track >= 0 && track < trackcount)
        {
            SetTrack(kTrackTypeSubtitle, track);
            if (auto * player = dynamic_cast<MythPlayerUI*>(m_parent); player)
                emit player->EnableSubtitles(true);
        }
    }
}

bool MythDVDDecoder::DoRewindSeek(long long DesiredFrame)
{
    if (!m_ringBuffer->IsDVD())
        return false;

    m_ringBuffer->Seek(DVDFindPosition(DesiredFrame), SEEK_SET);
    m_framesPlayed = m_framesRead = m_lastKey = DesiredFrame + 1;
    m_frameCounter += 100;
    return true;
}

void MythDVDDecoder::DoFastForwardSeek(long long DesiredFrame, bool &NeedFlush)
{
    if (!m_ringBuffer->IsDVD())
        return;

    m_ringBuffer->Seek(DVDFindPosition(DesiredFrame), SEEK_SET);
    NeedFlush    = true;
    m_framesPlayed = m_framesRead = m_lastKey = DesiredFrame + 1;
    m_frameCounter += 100;
}

void MythDVDDecoder::StreamChangeCheck(void)
{
    if (!m_ringBuffer->IsDVD())
        return;

    if (m_streamsChanged)
    {
        // This was originally in HandleDVDStreamChange
        m_trackLock.lock();
        ScanStreams(true);
        m_trackLock.unlock();
        m_streamsChanged=false;
    }

    // Update the title length
    if (m_parent->AtNormalSpeed() && m_ringBuffer->DVD()->PGCLengthChanged())
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
            m_trackLock.lock();
            m_selectedTrack[kTrackTypeVideo].m_av_stream_index = static_cast<int>(i);
            m_trackLock.unlock();
            break;
        }
    }
}

int MythDVDDecoder::GetAudioLanguage(uint /*AudioIndex*/, uint StreamIndex)
{
    if ((m_ic->streams[StreamIndex]->id >= 0) && (m_ringBuffer && m_ringBuffer->IsDVD()))
    {
        auto track = m_ringBuffer->DVD()->GetAudioTrackNum(static_cast<uint>(m_ic->streams[StreamIndex]->id));
        return static_cast<int>(m_ringBuffer->DVD()->GetAudioLanguage(track));
    }
    return iso639_str3_to_key("und");
}

long long MythDVDDecoder::DVDFindPosition(long long DesiredFrame)
{
    if (!m_ringBuffer->IsDVD())
        return 0;

    int ffrewSkip = 1;
    int current_speed = 0;
    if (m_parent)
    {
        ffrewSkip = m_parent->GetFFRewSkip();
        current_speed = static_cast<int>(m_parent->GetNextPlaySpeed());
    }

    if (ffrewSkip == 1 || ffrewSkip == 0)
    {
        std::chrono::seconds diffTime = std::chrono::seconds(static_cast<int>(ceil((DesiredFrame - m_framesPlayed) / m_fps)));
        std::chrono::seconds desiredTimePos = m_ringBuffer->DVD()->GetCurrentTime() +
                        diffTime;
        if (diffTime <= 0s)
            desiredTimePos--;
        else
            desiredTimePos++;

        if (desiredTimePos < 0s)
            desiredTimePos = 0s;
        return (desiredTimePos.count() * 90000LL);
    }
    return current_speed;
}

AudioTrackType MythDVDDecoder::GetAudioTrackType(uint Index)
{
    int type = 0;

    if (m_ringBuffer && m_ringBuffer->DVD())
    {
        int logical_idx = m_ringBuffer->DVD()->GetAudioTrackNum(static_cast<uint>(m_ic->streams[Index]->id));
        type = m_ringBuffer->DVD()->GetAudioTrackType(static_cast<uint>(logical_idx));
    }

     // These are the only types defined in unofficial documentation
    if (type > 0 && type < 5)
    {
        AudioTrackType ret = kAudioTypeNormal;
        switch (type)
        {
            case 1: return kAudioTypeNormal;
            case 2: return kAudioTypeAudioDescription;
            case 3:
            case 4: return kAudioTypeCommentary;
            default: break;
        }
        return ret;
    }

    // If the DVD metadata doesn't include the info then we might as well fall through, maybe we'll get lucky
    return AvFormatDecoder::GetAudioTrackType(Index);
}
