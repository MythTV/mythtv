/**
 *  DTVRecorder -- base class for Digital Televison recorders
 *  Copyright 2003-2004 by Brandon Beattie, Doug Larrick,
 *    Jason Hoos, and Daniel Thor Kristjansson
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "atscstreamdata.h"
#include "mpegstreamdata.h"
#include "dvbstreamdata.h"
#include "dtvrecorder.h"
#include "programinfo.h"
#include "mythlogging.h"
#include "mpegtables.h"
#include "ringbuffer.h"
#include "tv_rec.h"

extern "C" {
#include "libavcodec/mpegvideo.h"
}

#define LOC ((tvrec) ? \
    QString("DTVRec[%1]: ").arg(tvrec->GetCaptureCardNum()) : \
    QString("DTVRec(0x%1): ").arg(intptr_t(this),0,16))

const uint DTVRecorder::kMaxKeyFrameDistance = 80;

/** \class DTVRecorder
 *  \brief This is a specialization of RecorderBase used to
 *         handle MPEG-2, MPEG-4, MPEG-4 AVC, DVB and ATSC streams.
 *
 *  \sa DBox2Recorder, DVBRecorder, FirewireRecorder,
        HDHRRecoreder, IPTVRecorder
 */

DTVRecorder::DTVRecorder(TVRec *rec) :
    RecorderBase(rec),
    // file handle for stream
    _stream_fd(-1),
    _recording_type("all"),
    // used for scanning pes headers for keyframes
    _start_code(0xffffffff),        _first_keyframe(-1),
    _last_gop_seen(0),              _last_seq_seen(0),
    _last_keyframe_seen(0),
    _audio_bytes_remaining(0),      _video_bytes_remaining(0),
    _other_bytes_remaining(0),
    // MPEG2 parser information
    _progressive_sequence(0),
    _repeat_pict(0),
    // H.264 support
    _pes_synced(false),
    _seen_sps(false),
    // settings
    _wait_for_keyframe_option(true),
    _has_written_other_keyframe(false),
    // state
    _error(),
    _stream_data(NULL),
    // TS packet buffer
    // keyframe TS buffer
    _buffer_packets(false),
    // general recorder stuff
    _pid_lock(QMutex::Recursive),
    _input_pat(NULL),
    _input_pmt(NULL),
    _has_no_av(false),
    // statistics
    _use_pts(false),
    _packet_count(0),
    _continuity_error_count(0),
    _frames_seen_count(0),          _frames_written_count(0),
    _frame_interval(0),             _frame_duration(0),
    _total_duration(0)
{
    SetPositionMapType(MARK_GOP_BYFRAME);
    _payload_buffer.reserve(TSPacket::kSize * (50 + 1));

    ResetForNewFile();

    memset(_stream_id,  0, sizeof(_stream_id));
    memset(_pid_status, 0, sizeof(_pid_status));
    memset(_continuity_counter, 0xff, sizeof(_continuity_counter));
}

DTVRecorder::~DTVRecorder()
{
    StopRecording();

    SetStreamData(NULL);

    if (_input_pat)
    {
        delete _input_pat;
        _input_pat = NULL;
    }

    if (_input_pmt)
    {
        delete _input_pmt;
        _input_pmt = NULL;
    }
}

void DTVRecorder::SetOption(const QString &name, const QString &value)
{
    if (name == "recordingtype")
    {
        _recording_type = value;
        _recording_type.detach();
    }
    else
        RecorderBase::SetOption(name, value);
}

/** \fn DTVRecorder::SetOption(const QString&,int)
 *  \brief handles the "wait_for_seqstart" option.
 */
void DTVRecorder::SetOption(const QString &name, int value)
{
    if (name == "wait_for_seqstart")
        _wait_for_keyframe_option = (value == 1);
    else
        RecorderBase::SetOption(name, value);
}

void DTVRecorder::SetOptionsFromProfile(RecordingProfile *profile,
                                        const QString &videodev,
                                        const QString&, const QString&)
{
    SetOption("videodevice", videodev);
    DTVRecorder::SetOption("tvformat", gCoreContext->GetSetting("TVFormat"));
    SetStrOption(profile, "recordingtype");
}

/** \fn DTVRecorder::FinishRecording(void)
 *  \brief Flushes the ringbuffer, and if this is not a live LiveTV
 *         recording saves the position map and filesize.
 */
void DTVRecorder::FinishRecording(void)
{
    if (ringBuffer)
        ringBuffer->WriterFlush();

    if (curRecording)
    {
        if (ringBuffer)
            curRecording->SaveFilesize(ringBuffer->GetRealFileSize());
        SavePositionMap(true);
        curRecording->SaveTotalDuration((int64_t)_total_duration);
        curRecording->SaveTotalFrames(_frames_written_count);
    }
}

void DTVRecorder::ResetForNewFile(void)
{
    LOG(VB_RECORD, LOG_INFO, LOC + "ResetForNewFile(void)");
    QMutexLocker locker(&positionMapLock);

    // _seen_psp and m_h264_parser should
    // not be reset here. This will only be called just as
    // we're seeing the first packet of a new keyframe for
    // writing to the new file and anything that makes the
    // recorder think we're waiting on another keyframe will
    // send significant amounts of good data to /dev/null.
    // -- Daniel Kristjansson 2011-02-26

    _start_code                 = 0xffffffff;
    _first_keyframe             = -1;
    _has_written_other_keyframe = false;
    _last_keyframe_seen         = 0;
    _last_gop_seen              = 0;
    _last_seq_seen              = 0;
    _audio_bytes_remaining      = 0;
    _video_bytes_remaining      = 0;
    _other_bytes_remaining      = 0;
    //_recording
    _error                      = QString();

    _progressive_sequence       = 0;
    _repeat_pict                = 0;

    //_pes_synced
    //_seen_sps
    positionMap.clear();
    positionMapDelta.clear();
    durationMap.clear();
    durationMapDelta.clear();

    locker.unlock();
    ClearStatistics();
}

void DTVRecorder::ClearStatistics(void)
{
    RecorderBase::ClearStatistics();

    memset(_ts_count, 0, sizeof(_ts_count));
    for (int i = 0; i < 256; ++i)
        _ts_last[i] = -1LL;
    for (int i = 0; i < 256; ++i)
        _ts_first[i] = -1LL;
    //_ts_first_dt -- doesn't need to be cleared only used if _ts_first>=0
    _packet_count.fetchAndStoreRelaxed(0);
    _continuity_error_count.fetchAndStoreRelaxed(0);
    _frames_seen_count          = 0;
    _frames_written_count       = 0;
    _total_duration             = 0;
}

// documented in recorderbase.h
void DTVRecorder::Reset(void)
{
    LOG(VB_RECORD, LOG_INFO, LOC + "Reset(void)");
    ResetForNewFile();

    _start_code = 0xffffffff;

    if (curRecording)
    {
        curRecording->ClearPositionMap(MARK_GOP_BYFRAME);
        curRecording->ClearPositionMap(MARK_DURATION_MS);
    }
}

void DTVRecorder::SetStreamData(MPEGStreamData *data)
{
    if (data == _stream_data)
        return;

    MPEGStreamData *old_data = _stream_data;
    _stream_data = data;
    if (old_data)
        delete old_data;

    if (_stream_data)
        InitStreamData();
}

void DTVRecorder::InitStreamData(void)
{
    _stream_data->AddMPEGSPListener(this);
    _stream_data->AddMPEGListener(this);

    DVBStreamData *dvb = dynamic_cast<DVBStreamData*>(_stream_data);
    if (dvb)
        dvb->AddDVBMainListener(this);

    ATSCStreamData *atsc = dynamic_cast<ATSCStreamData*>(_stream_data);

    if (atsc && atsc->DesiredMinorChannel())
        atsc->SetDesiredChannel(atsc->DesiredMajorChannel(),
                                atsc->DesiredMinorChannel());
    else if (_stream_data->DesiredProgram() >= 0)
        _stream_data->SetDesiredProgram(_stream_data->DesiredProgram());
}

void DTVRecorder::BufferedWrite(const TSPacket &tspacket, bool insert)
{
    if (!insert) // PAT/PMT may need inserted in front of any buffered data
    {
        // delay until first GOP to avoid decoder crash on res change
        if (!_buffer_packets && _wait_for_keyframe_option &&
            _first_keyframe < 0)
            return;

        if (curRecording && timeOfFirstDataIsSet.testAndSetRelaxed(0,1))
        {
            QMutexLocker locker(&statisticsLock);
            timeOfFirstData = MythDate::current();
            timeOfLatestData = MythDate::current();
            timeOfLatestDataTimer.start();
        }

        int val = timeOfLatestDataCount.fetchAndAddRelaxed(1);
        int thresh = timeOfLatestDataPacketInterval.fetchAndAddRelaxed(0);
        if (val > thresh)
        {
            QMutexLocker locker(&statisticsLock);
            uint elapsed = timeOfLatestDataTimer.restart();
            int interval = thresh;
            if (elapsed > kTimeOfLatestDataIntervalTarget + 250)
                interval = timeOfLatestDataPacketInterval
                           .fetchAndStoreRelaxed(thresh * 4 / 5);
            else if (elapsed + 250 < kTimeOfLatestDataIntervalTarget)
                interval = timeOfLatestDataPacketInterval
                           .fetchAndStoreRelaxed(thresh * 9 / 8);

            timeOfLatestDataCount.fetchAndStoreRelaxed(1);
            timeOfLatestData = MythDate::current();

            LOG(VB_RECORD, LOG_DEBUG, LOC +
                QString("Updating timeOfLatestData elapsed(%1) interval(%2)")
                .arg(elapsed).arg(interval));
        }

        // Do we have to buffer the packet for exact keyframe detection?
        if (_buffer_packets)
        {
            int idx = _payload_buffer.size();
            _payload_buffer.resize(idx + TSPacket::kSize);
            memcpy(&_payload_buffer[idx], tspacket.data(), TSPacket::kSize);
            return;
        }

        // We are free to write the packet, but if we have buffered packet[s]
        // we have to write them first...
        if (!_payload_buffer.empty())
        {
            if (ringBuffer)
                ringBuffer->Write(&_payload_buffer[0], _payload_buffer.size());
            _payload_buffer.clear();
        }
    }

    if (ringBuffer)
        ringBuffer->Write(tspacket.data(), TSPacket::kSize);
}

enum { kExtractPTS, kExtractDTS };
static int64_t extract_timestamp(
    const uint8_t *bufptr, int bytes_left, int pts_or_dts)
{
    if (bytes_left < 4)
        return -1LL;

    bool has_pts = bufptr[3] & 0x80;
    int offset = 5;
    if (((kExtractPTS == pts_or_dts) && !has_pts) || (offset + 5 > bytes_left))
        return -1LL;

    bool has_dts = bufptr[3] & 0x40;
    if (kExtractDTS == pts_or_dts)
    {
        if (!has_dts)
            return -1LL;
        offset += has_pts ? 5 : 0;
        if (offset + 5 > bytes_left)
            return -1LL;
    }

    return ((uint64_t(bufptr[offset+0] & 0x0e) << 29) |
            (uint64_t(bufptr[offset+1]       ) << 22) |
            (uint64_t(bufptr[offset+2] & 0xfe) << 14) |
            (uint64_t(bufptr[offset+3]       ) <<  7) |
            (uint64_t(bufptr[offset+4] & 0xfe) >>  1));
}

static QDateTime ts_to_qdatetime(
    uint64_t pts, uint64_t pts_first, QDateTime &pts_first_dt)
{
    if (pts < pts_first)
        pts += 0x1FFFFFFFFLL;
    QDateTime dt = pts_first_dt;
    return dt.addMSecs((pts - pts_first)/90);
}

static const uint frameRateMap[16] = {
    0, 23796, 24000, 25000, 29970, 30000, 50000, 59940, 60000,
    0, 0, 0, 0, 0, 0, 0
};

/** \fn DTVRecorder::FindMPEG2Keyframes(const TSPacket* tspacket)
 *  \brief Locates the keyframes and saves them to the position map.
 *
 *   This searches for three magic integers in the stream.
 *   The picture start code 0x00000100, the GOP code 0x000001B8,
 *   and the sequence start code 0x000001B3. The GOP code is
 *   prefered, but is only required of MPEG1 streams, the
 *   sequence start code is a decent fallback for MPEG2
 *   streams, and if all else fails we just look for the picture
 *   start codes and call every 16th frame a keyframe.
 *
 *   NOTE: This does not only find keyframes but also tracks the
 *         total frames as well. At least a couple times seeking
 *         has been broken by short-circuiting the search once
 *         a keyframe stream id has been found. This may work on
 *         some channels, but will break on others so algorithmic
 *         optimizations should be done with great care.
 *
 *  \code
 *   PES header format
 *   byte 0  byte 1  byte 2  byte 3      [byte 4     byte 5]
 *   0x00    0x00    0x01    PESStreamID  PES packet length
 *  \endcode
 *
 *  \return Returns true if packet[s] should be output.
 */
bool DTVRecorder::FindMPEG2Keyframes(const TSPacket* tspacket)
{
    if (!tspacket->HasPayload()) // no payload to scan
        return _first_keyframe >= 0;

    if (!ringBuffer)
        return _first_keyframe >= 0;

    // if packet contains start of PES packet, start
    // looking for first byte of MPEG start code (3 bytes 0 0 1)
    // otherwise, pick up search where we left off.
    const bool payloadStart = tspacket->PayloadStart();
    _start_code = (payloadStart) ? 0xffffffff : _start_code;

    // Just make these local for efficiency reasons (gcc not so smart..)
    const uint maxKFD = kMaxKeyFrameDistance;
    bool hasFrame     = false;
    bool hasKeyFrame  = false;

    uint aspectRatio = 0;
    uint height = 0;
    uint width = 0;
    uint frameRate = 0;

    // Scan for PES header codes; specifically picture_start
    // sequence_start (SEQ) and group_start (GOP).
    //   00 00 01 00: picture_start_code
    //   00 00 01 B8: group_start_code
    //   00 00 01 B3: seq_start_code
    //   00 00 01 B5: ext_start_code
    //   (there are others that we don't care about)
    const uint8_t *bufptr = tspacket->data() + tspacket->AFCOffset();
    const uint8_t *bufend = tspacket->data() + TSPacket::kSize;
    int ext_type, bytes_left;
    _repeat_pict = 0;

    while (bufptr < bufend)
    {
        bufptr = avpriv_mpv_find_start_code(bufptr, bufend, &_start_code);
        bytes_left = bufend - bufptr;
        if ((_start_code & 0xffffff00) == 0x00000100)
        {
            // At this point we have seen the start code 0 0 1
            // the next byte will be the PES packet stream id.
            const int stream_id = _start_code & 0x000000ff;
            if (PESStreamID::PictureStartCode == stream_id)
                hasFrame = true;
            else if (PESStreamID::GOPStartCode == stream_id)
            {
                _last_gop_seen  = _frames_seen_count;
                hasKeyFrame    |= true;
            }
            else if (PESStreamID::SequenceStartCode == stream_id)
            {
                _last_seq_seen  = _frames_seen_count;
                hasKeyFrame    |= (_last_gop_seen + maxKFD)<_frames_seen_count;

                // Look for aspectRatio changes and store them in the database
                aspectRatio = (bufptr[3] >> 4);

                // Get resolution
                height = ((bufptr[1] & 0xf) << 8) | bufptr[2];
                width = (bufptr[0] <<4) | (bufptr[1]>>4);

                frameRate = frameRateMap[(bufptr[3] & 0x0000000f)];
            }
            else if (PESStreamID::MPEG2ExtensionStartCode == stream_id)
            {
                if (bytes_left >= 1)
                {
                    ext_type = (bufptr[0] >> 4);
                    switch(ext_type)
                    {
                    case 0x1: /* sequence extension */
                        if (bytes_left >= 6)
                        {
                            _progressive_sequence = bufptr[1] & (1 << 3);
                        }
                        break;
                    case 0x8: /* picture coding extension */
                        if (bytes_left >= 5)
                        {
                            //int picture_structure = bufptr[2]&3;
                            int top_field_first = bufptr[3] & (1 << 7);
                            int repeat_first_field = bufptr[3] & (1 << 1);
                            int progressive_frame = bufptr[4] & (1 << 7);

                            /* check if we must repeat the frame */
                            _repeat_pict = 1;
                            if (repeat_first_field)
                            {
                                if (_progressive_sequence)
                                {
                                    if (top_field_first)
                                        _repeat_pict = 5;
                                    else
                                        _repeat_pict = 3;
                                }
                                else if (progressive_frame)
                                {
                                    _repeat_pict = 2;
                                }
                            }
                            // The _repeat_pict code above matches
                            // mpegvideo_extract_headers(), but the
                            // code in mpeg_field_start() computes a
                            // value one less, which seems correct.
                            --_repeat_pict;
                        }
                        break;
                    }
                }
            }
            if ((stream_id >= PESStreamID::MPEGVideoStreamBegin) &&
                (stream_id <= PESStreamID::MPEGVideoStreamEnd))
            {
                int64_t pts = extract_timestamp(
                    bufptr, bytes_left, kExtractPTS);
                int64_t dts = extract_timestamp(
                    bufptr, bytes_left, kExtractPTS);
                HandleTimestamps(stream_id, pts, dts);
            }
        }
    }

    if (hasFrame && !hasKeyFrame)
    {
        // If we have seen kMaxKeyFrameDistance frames since the
        // last GOP or SEQ stream_id, then pretend this picture
        // is a keyframe. We may get artifacts but at least
        // we will be able to skip frames.
        hasKeyFrame = !(_frames_seen_count & 0xf);
        hasKeyFrame &= (_last_gop_seen + maxKFD) < _frames_seen_count;
        hasKeyFrame &= (_last_seq_seen + maxKFD) < _frames_seen_count;
    }

    // _buffer_packets will only be true if a payload start has been seen
    if (hasKeyFrame && _buffer_packets)
    {
        LOG(VB_RECORD, LOG_DEBUG, LOC + QString
            ("Keyframe @ %1 + %2 = %3")
            .arg(ringBuffer->GetWritePosition())
            .arg(_payload_buffer.size())
            .arg(ringBuffer->GetWritePosition() + _payload_buffer.size()));

        _last_keyframe_seen = _frames_seen_count;
        HandleKeyframe(0);
    }

    if (hasFrame)
    {
        LOG(VB_RECORD, LOG_DEBUG, LOC + QString
            ("Frame @ %1 + %2 = %3")
            .arg(ringBuffer->GetWritePosition())
            .arg(_payload_buffer.size())
            .arg(ringBuffer->GetWritePosition() + _payload_buffer.size()));

        _buffer_packets = false;  // We now know if it is a keyframe, or not
        _frames_seen_count++;
        if (!_wait_for_keyframe_option || _first_keyframe >= 0)
            UpdateFramesWritten();
        else
        {
            /* Found a frame that is not a keyframe, and we want to
             * start on a keyframe */
            _payload_buffer.clear();
        }
    }

    if ((aspectRatio > 0) && (aspectRatio != m_videoAspect))
    {
        m_videoAspect = aspectRatio;
        AspectChange((AspectRatio)aspectRatio, _frames_written_count);
    }

    if (height && width && (height != m_videoHeight || m_videoWidth != width))
    {
        m_videoHeight = height;
        m_videoWidth = width;
        ResolutionChange(width, height, _frames_written_count);
    }

    if (frameRate && frameRate != m_frameRate)
    {
        m_frameRate = frameRate;
        LOG(VB_RECORD, LOG_INFO, LOC +
            QString("FindMPEG2Keyframes: frame rate = %1") .arg(frameRate));
        FrameRateChange(frameRate, _frames_written_count);
    }

    return _first_keyframe >= 0;
}

void DTVRecorder::HandleTimestamps(int stream_id, int64_t pts, int64_t dts)
{
    if (pts < 0)
    {
        _ts_last[stream_id] = -1;
        return;
    }

    if ((dts < 0) && !_use_pts)
    {
        _ts_last[stream_id] = -1;
        _use_pts = true;
        LOG(VB_RECORD, LOG_DEBUG,
            "Switching from dts tracking to pts tracking." +
            QString("TS count is %1").arg(_ts_count[stream_id]));
    }

    int64_t ts = dts;
    int64_t gap_threshold = 90000; // 1 second
    if (_use_pts)
    {
        ts = dts;
        gap_threshold = 2*90000; // two seconds, compensate for GOP ordering
    }

    if (_ts_last[stream_id] >= 0)
    {
        int64_t diff = ts - _ts_last[stream_id];
        if ((diff < 0) && (diff < (10 * -90000)))
            diff += 0x1ffffffffLL;
        if (diff < 0)
            diff = -diff;
        if (diff > gap_threshold)
        {
            QMutexLocker locker(&statisticsLock);
            recordingGaps.push_back(
                RecordingGap(
                    ts_to_qdatetime(
                        _ts_last[stream_id], _ts_first[stream_id],
                        _ts_first_dt[stream_id]),
                    ts_to_qdatetime(
                        ts, _ts_first[stream_id], _ts_first_dt[stream_id])));
            LOG(VB_RECORD, LOG_DEBUG, LOC + QString("Inserted gap %1 dur %2")
                .arg(recordingGaps.back().toString()).arg(diff/90000.0));
        }
    }

    _ts_last[stream_id] = ts;

    if (_ts_count[stream_id] < 30)
    {
        if (!_ts_count[stream_id])
        {
            _ts_first[stream_id] = ts;
            _ts_first_dt[stream_id] = MythDate::current();
        }
        else if (ts < _ts_first[stream_id])
        {
            _ts_first[stream_id] = ts;
            _ts_first_dt[stream_id] = MythDate::current();
        }
    }

    _ts_count[stream_id]++;
}

void DTVRecorder::UpdateFramesWritten(void)
{
    _frames_written_count++;
    if (m_frameRate > 0)
    {
        // m_frameRate is frames per 1000 seconds, e.g. 29970 for
        // 29.97 fps.  Calculate usec values.
        _frame_interval = 1000000000.0 / m_frameRate;
        _frame_duration = _frame_interval +
            (_repeat_pict * _frame_interval * 0.5f);
        _total_duration += _frame_duration;
    }
}

bool DTVRecorder::FindAudioKeyframes(const TSPacket*)
{
    bool hasKeyFrame = false;
    if (!ringBuffer || (GetStreamData()->VideoPIDSingleProgram() <= 0x1fff))
        return hasKeyFrame;

    static const uint64_t msec_per_day = 24 * 60 * 60 * 1000ULL;
    const double frame_interval = (1000.0 / video_frame_rate);
    uint64_t elapsed = (uint64_t) max(_audio_timer.elapsed(), 0);
    uint64_t expected_frame =
        (uint64_t) ((double)elapsed / frame_interval);

    while (_frames_seen_count > expected_frame + 10000)
        expected_frame += (uint64_t) ((double)msec_per_day / frame_interval);

    if (!_frames_seen_count || (_frames_seen_count < expected_frame))
    {
        if (!_frames_seen_count)
            _audio_timer.start();

        _frames_seen_count++;

        if (1 == (_frames_seen_count & 0x7))
        {
            _last_keyframe_seen = _frames_seen_count;
            HandleKeyframe(_payload_buffer.size());
            hasKeyFrame = true;
        }

        if (!_wait_for_keyframe_option || _first_keyframe>=0)
            UpdateFramesWritten();
    }

    return hasKeyFrame;
}

/// Non-Audio/Video data. For streams which contain no audio/video,
/// write just 1 key-frame at the start.
bool DTVRecorder::FindOtherKeyframes(const TSPacket *tspacket)
{
    if (!ringBuffer || (GetStreamData()->VideoPIDSingleProgram() <= 0x1fff))
        return true;

    if (_has_written_other_keyframe)
        return true;

    LOG(VB_RECORD, LOG_INFO, LOC + "DSMCC - FindOtherKeyframes() - "
            "generating initial key-frame");

    _frames_seen_count++;
    UpdateFramesWritten();
    _last_keyframe_seen = _frames_seen_count;

    HandleKeyframe(_payload_buffer.size());

    _has_written_other_keyframe = true;

    return true;
}

/** \fn DTVRecorder::HandleKeyframe(uint64_t)
 *  \brief This save the current frame to the position maps
 *         and handles ringbuffer switching.
 */
void DTVRecorder::HandleKeyframe(int64_t extra)
{
    if (!ringBuffer)
        return;

    // Perform ringbuffer switch if needed.
    CheckForRingBufferSwitch();

    uint64_t frameNum = _frames_written_count;
    _first_keyframe = (_first_keyframe < 0) ? frameNum : _first_keyframe;

    // Add key frame to position map
    positionMapLock.lock();
    if (!positionMap.contains(frameNum))
    {
        int64_t startpos = ringBuffer->GetWritePosition() + extra;

        // Don't put negative offsets into the database, they get munged into
        // MAX_INT64 - offset, which is an exceedingly large number, and
        // certainly not valid.
        if (startpos >= 0)
        {
            positionMapDelta[frameNum] = startpos;
            positionMap[frameNum]      = startpos;
            durationMap[frameNum]      = _total_duration / 1000;
            durationMapDelta[frameNum] = _total_duration / 1000;
        }
    }
    positionMapLock.unlock();
}

/** \fn DTVRecorder::FindH264Keyframes(const TSPacket*)
 *  \brief This searches the TS packet to identify keyframes.
 *  \param TSPacket Pointer the the TS packet data.
 *  \return Returns true if a keyframe has been found.
 */
bool DTVRecorder::FindH264Keyframes(const TSPacket *tspacket)
{
    if (!tspacket->HasPayload()) // no payload to scan
        return _first_keyframe >= 0;

    if (!ringBuffer)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "FindH264Keyframes: No ringbuffer");
        return _first_keyframe >= 0;
    }

    const bool payloadStart = tspacket->PayloadStart();
    if (payloadStart)
    {
        // reset PES sync state
        _pes_synced = false;
        _start_code = 0xffffffff;
    }

    uint aspectRatio = 0;
    uint height = 0;
    uint width = 0;
    uint frameRate = 0;

    bool hasFrame = false;
    bool hasKeyFrame = false;

    // scan for PES packets and H.264 NAL units
    uint i = tspacket->AFCOffset();
    for (; i < TSPacket::kSize; ++i)
    {
        // special handling required when a new PES packet begins
        if (payloadStart && !_pes_synced)
        {
            // bounds check
            if (i + 2 >= TSPacket::kSize)
            {
                LOG(VB_GENERAL, LOG_ERR, LOC +
                    "PES packet start code may overflow to next TS packet, "
                    "aborting keyframe search");
                break;
            }

            // must find the PES start code
            if (tspacket->data()[i++] != 0x00 ||
                tspacket->data()[i++] != 0x00 ||
                tspacket->data()[i++] != 0x01)
            {
                LOG(VB_GENERAL, LOG_ERR, LOC +
                    "PES start code not found in TS packet with PUSI set");
                break;
            }

            // bounds check
            if (i + 5 >= TSPacket::kSize)
            {
                LOG(VB_GENERAL, LOG_ERR, LOC +
                    "PES packet headers overflow to next TS packet, "
                    "aborting keyframe search");
                break;
            }

            // now we need to compute where the PES payload begins
            // skip past the stream_id (+1)
            // the next two bytes are the PES packet length (+2)
            // after that, one byte of PES packet control bits (+1)
            // after that, one byte of PES header flags bits (+1)
            // and finally, one byte for the PES header length
            const unsigned char pes_header_length = tspacket->data()[i + 5];

            // bounds check
            if ((i + 6 + pes_header_length) >= TSPacket::kSize)
            {
                LOG(VB_GENERAL, LOG_ERR, LOC +
                    "PES packet headers overflow to next TS packet, "
                    "aborting keyframe search");
                break;
            }

            // we now know where the PES payload is
            // normally, we should have used 6, but use 5 because the for
            // loop will bump i
            i += 5 + pes_header_length;
            _pes_synced = true;

#if 0
            LOG(VB_RECORD, LOG_DEBUG, LOC + "PES synced");
#endif
            continue;
        }

        // ain't going nowhere if we're not PES synced
        if (!_pes_synced)
            break;

        // scan for a NAL unit start code

        uint32_t bytes_used = m_h264_parser.addBytes
                              (tspacket->data() + i, TSPacket::kSize - i,
                               ringBuffer->GetWritePosition());
        i += (bytes_used - 1);

        if (m_h264_parser.stateChanged())
        {
            if (m_h264_parser.onFrameStart() &&
                m_h264_parser.FieldType() != H264Parser::FIELD_BOTTOM)
            {
                hasKeyFrame = m_h264_parser.onKeyFrameStart();
                hasFrame = true;
                _seen_sps |= hasKeyFrame;

                width = m_h264_parser.pictureWidth();
                height = m_h264_parser.pictureHeight();
                aspectRatio = m_h264_parser.aspectRatio();
                frameRate = m_h264_parser.frameRate();
            }
        }
    } // for (; i < TSPacket::kSize; ++i)

    // _buffer_packets will only be true if a payload start has been seen
    if (hasKeyFrame && _buffer_packets)
    {
        LOG(VB_RECORD, LOG_DEBUG, LOC + QString
            ("Keyframe @ %1 + %2 = %3 AU %4")
            .arg(ringBuffer->GetWritePosition())
            .arg(_payload_buffer.size())
            .arg(ringBuffer->GetWritePosition() + _payload_buffer.size())
            .arg(m_h264_parser.keyframeAUstreamOffset()));

        _last_keyframe_seen = _frames_seen_count;
        HandleH264Keyframe();
    }

    if (hasFrame)
    {
        LOG(VB_RECORD, LOG_DEBUG, LOC + QString
            ("Frame @ %1 + %2 = %3 AU %4")
            .arg(ringBuffer->GetWritePosition())
            .arg(_payload_buffer.size())
            .arg(ringBuffer->GetWritePosition() + _payload_buffer.size())
            .arg(m_h264_parser.keyframeAUstreamOffset()));

        _buffer_packets = false;  // We now know if this is a keyframe
        _frames_seen_count++;
        if (!_wait_for_keyframe_option || _first_keyframe >= 0)
            UpdateFramesWritten();
        else
        {
            /* Found a frame that is not a keyframe, and we want to
             * start on a keyframe */
            _payload_buffer.clear();
        }
    }

    if ((aspectRatio > 0) && (aspectRatio != m_videoAspect))
    {
        m_videoAspect = aspectRatio;
        AspectChange((AspectRatio)aspectRatio, _frames_written_count);
    }

    if (height && width && (height != m_videoHeight || m_videoWidth != width))
    {
        m_videoHeight = height;
        m_videoWidth = width;
        ResolutionChange(width, height, _frames_written_count);
    }

    if (frameRate != 0 && frameRate != m_frameRate)
    {
        LOG(VB_RECORD, LOG_INFO, LOC +
            QString("FindH264Keyframes: timescale: %1, tick: %2, framerate: %3")
                      .arg( m_h264_parser.GetTimeScale() )
                      .arg( m_h264_parser.GetUnitsInTick() )
                      .arg( frameRate ) );
        m_frameRate = frameRate;
        FrameRateChange(frameRate, _frames_written_count);
    }

    return _seen_sps;
}

/** \fn DTVRecorder::HandleH264Keyframe(void)
 *  \brief This save the current frame to the position maps
 *         and handles ringbuffer switching.
 */
void DTVRecorder::HandleH264Keyframe(void)
{
    // Perform ringbuffer switch if needed.
    CheckForRingBufferSwitch();

    uint64_t startpos;
    uint64_t frameNum = _frames_written_count;

    if (_first_keyframe < 0)
    {
        _first_keyframe = frameNum;
        startpos = 0;
    }
    else
        startpos = m_h264_parser.keyframeAUstreamOffset();

    // Add key frame to position map
    positionMapLock.lock();
    if (!positionMap.contains(frameNum))
    {
        positionMapDelta[frameNum] = startpos;
        positionMap[frameNum]      = startpos;
        durationMap[frameNum]      = _total_duration / 1000;
        durationMapDelta[frameNum] = _total_duration / 1000;
    }
    positionMapLock.unlock();
}

void DTVRecorder::FindPSKeyFrames(const uint8_t *buffer, uint len)
{
    const uint maxKFD = kMaxKeyFrameDistance;

    const uint8_t *bufstart = buffer;
    const uint8_t *bufptr   = buffer;
    const uint8_t *bufend   = buffer + len;

    uint aspectRatio = 0;
    uint height = 0;
    uint width = 0;
    uint frameRate = 0;

    uint skip = std::max(_audio_bytes_remaining, _other_bytes_remaining);
    while (bufptr + skip < bufend)
    {
        bool hasFrame     = false;
        bool hasKeyFrame  = false;

        const uint8_t *tmp = bufptr;
        bufptr =
            avpriv_mpv_find_start_code(bufptr + skip, bufend, &_start_code);
        _audio_bytes_remaining = 0;
        _other_bytes_remaining = 0;
        _video_bytes_remaining -= std::min(
            (uint)(bufptr - tmp), _video_bytes_remaining);

        if ((_start_code & 0xffffff00) != 0x00000100)
            continue;

        // NOTE: Length may be zero for packets that only contain bytes from
        // video elementary streams in TS packets. 13818-1:2000 2.4.3.7
        int pes_packet_length = -1;
        if ((bufend - bufptr) >= 2)
            pes_packet_length = ((bufptr[0]<<8) | bufptr[1]) + 2 + 6;

        const int stream_id = _start_code & 0x000000ff;
        if (_video_bytes_remaining)
        {
            if (PESStreamID::PictureStartCode == stream_id)
            { // pes_packet_length is meaningless
                pes_packet_length = -1;
                uint frmtypei = 1;
                if (bufend-bufptr >= 4)
                {
                    frmtypei = (bufptr[1]>>3) & 0x7;
                    if ((1 <= frmtypei) && (frmtypei <= 5))
                        hasFrame = true;
                }
                else
                {
                    hasFrame = true;
                }
            }
            else if (PESStreamID::GOPStartCode == stream_id)
            { // pes_packet_length is meaningless
                pes_packet_length = -1;
                _last_gop_seen  = _frames_seen_count;
                hasKeyFrame    |= true;
            }
            else if (PESStreamID::SequenceStartCode == stream_id)
            { // pes_packet_length is meaningless
                pes_packet_length = -1;
                _last_seq_seen  = _frames_seen_count;
                hasKeyFrame    |= (_last_gop_seen + maxKFD)<_frames_seen_count;

                // Look for aspectRatio changes and store them in the database
                aspectRatio = (bufptr[3] >> 4);

                // Get resolution
                height = ((bufptr[1] & 0xf) << 8) | bufptr[2];
                width = (bufptr[0] <<4) | (bufptr[1]>>4);

                frameRate = frameRateMap[(bufptr[3] & 0x0000000f)];
            }
        }
        else if (!_video_bytes_remaining && !_audio_bytes_remaining)
        {
            if ((stream_id >= PESStreamID::MPEGVideoStreamBegin) &&
                (stream_id <= PESStreamID::MPEGVideoStreamEnd))
            { // ok-dvdinfo
                _video_bytes_remaining = std::max(0, (int)pes_packet_length);
            }
            else if ((stream_id >= PESStreamID::MPEGAudioStreamBegin) &&
                     (stream_id <= PESStreamID::MPEGAudioStreamEnd))
            { // ok-dvdinfo
                _audio_bytes_remaining = std::max(0, (int)pes_packet_length);
            }
        }

        if (PESStreamID::PaddingStream == stream_id)
        { // ok-dvdinfo
            _other_bytes_remaining = std::max(0, (int)pes_packet_length);
        }

        _start_code = 0xffffffff; // reset start code

        if (hasFrame && !hasKeyFrame)
        {
            // If we have seen kMaxKeyFrameDistance frames since the
            // last GOP or SEQ stream_id, then pretend this picture
            // is a keyframe. We may get artifacts but at least
            // we will be able to skip frames.
            hasKeyFrame = !(_frames_seen_count & 0xf);
            hasKeyFrame &= (_last_gop_seen + maxKFD) < _frames_seen_count;
            hasKeyFrame &= (_last_seq_seen + maxKFD) < _frames_seen_count;
        }

        if (hasFrame)
        {
            _frames_seen_count++;
            if (!_wait_for_keyframe_option || _first_keyframe >= 0)
                UpdateFramesWritten();
        }

        if (hasKeyFrame)
        {
            _last_keyframe_seen = _frames_seen_count;
            HandleKeyframe(_payload_buffer.size() - (bufptr - bufstart));
        }

        if ((aspectRatio > 0) && (aspectRatio != m_videoAspect))
        {
            m_videoAspect = aspectRatio;
            AspectChange((AspectRatio)aspectRatio, _frames_written_count);
        }

        if (height && width &&
            (height != m_videoHeight || m_videoWidth != width))
        {
            m_videoHeight = height;
            m_videoWidth = width;
            ResolutionChange(width, height, _frames_written_count);
        }

        if (frameRate && frameRate != m_frameRate)
        {
            m_frameRate = frameRate;
            LOG(VB_RECORD, LOG_INFO, LOC +
                QString("FindPSKeyFrames: frame rate = %1").arg(frameRate));
            FrameRateChange(frameRate, _frames_written_count);
        }

        if (hasKeyFrame || hasFrame)
        {
            // We are free to write the packet, but if we have
            // buffered packet[s] we have to write them first...
            if (!_payload_buffer.empty())
            {
                if (ringBuffer)
                {
                    ringBuffer->Write(
                        &_payload_buffer[0], _payload_buffer.size());
                }
                _payload_buffer.clear();
            }

            if (ringBuffer)
                ringBuffer->Write(bufstart, (bufptr - bufstart));

            bufstart = bufptr;
        }

        skip = std::max(_audio_bytes_remaining, _other_bytes_remaining);
    }

    int bytes_skipped = bufend - bufptr;
    if (bytes_skipped > 0)
    {
        _audio_bytes_remaining -= std::min(
            (uint)bytes_skipped, _audio_bytes_remaining);
        _video_bytes_remaining -= std::min(
            (uint)bytes_skipped, _video_bytes_remaining);
        _other_bytes_remaining -= std::min(
            (uint)bytes_skipped, _other_bytes_remaining);
    }

    uint64_t idx = _payload_buffer.size();
    uint64_t rem = (bufend - bufstart);
    _payload_buffer.resize(idx + rem);
    memcpy(&_payload_buffer[idx], bufstart, rem);
#if 0
    LOG(VB_GENERAL, LOG_DEBUG, LOC +
        QString("idx: %1, rem: %2").arg(idx).arg(rem));
#endif
}

void DTVRecorder::HandlePAT(const ProgramAssociationTable *_pat)
{
    if (!_pat)
    {
        LOG(VB_RECORD, LOG_ERR, LOC + "SetPAT(NULL)");
        return;
    }

    QMutexLocker change_lock(&_pid_lock);

    int progNum = _stream_data->DesiredProgram();
    uint pmtpid = _pat->FindPID(progNum);

    if (!pmtpid)
    {
        LOG(VB_RECORD, LOG_ERR, LOC +
            QString("SetPAT(): Ignoring PAT not containing our desired "
                    "program (%1)...").arg(progNum));
        return;
    }

    LOG(VB_RECORD, LOG_INFO, LOC + QString("SetPAT(%1 on 0x%2)")
            .arg(progNum).arg(pmtpid,0,16));

    ProgramAssociationTable *oldpat = _input_pat;
    _input_pat = new ProgramAssociationTable(*_pat);
    delete oldpat;

    // Listen for the other PMTs for faster channel switching
    for (uint i = 0; _input_pat && (i < _input_pat->ProgramCount()); ++i)
    {
        uint pmt_pid = _input_pat->ProgramPID(i);
        if (!_stream_data->IsListeningPID(pmt_pid))
            _stream_data->AddListeningPID(pmt_pid, kPIDPriorityLow);
    }
}

void DTVRecorder::HandlePMT(uint progNum, const ProgramMapTable *_pmt)
{
    QMutexLocker change_lock(&_pid_lock);

    if ((int)progNum == _stream_data->DesiredProgram())
    {
        LOG(VB_RECORD, LOG_INFO, LOC + QString("SetPMT(%1)").arg(progNum));
        ProgramMapTable *oldpmt = _input_pmt;
        _input_pmt = new ProgramMapTable(*_pmt);

        QString sistandard = GetSIStandard();

        bool has_no_av = true;
        for (uint i = 0; i < _input_pmt->StreamCount() && has_no_av; ++i)
        {
            has_no_av &= !_input_pmt->IsVideo(i, sistandard);
            has_no_av &= !_input_pmt->IsAudio(i, sistandard);
        }
        _has_no_av = has_no_av;

        SetCAMPMT(_input_pmt);
        delete oldpmt;
    }
}

void DTVRecorder::HandleSingleProgramPAT(ProgramAssociationTable *pat,
                                         bool insert)
{
    if (!pat)
    {
        LOG(VB_RECORD, LOG_ERR, LOC + "HandleSingleProgramPAT(NULL)");
        return;
    }

    if (!ringBuffer)
        return;

    uint next_cc = (pat->tsheader()->ContinuityCounter()+1)&0xf;
    pat->tsheader()->SetContinuityCounter(next_cc);
    pat->GetAsTSPackets(_scratch, next_cc);

    for (uint i = 0; i < _scratch.size(); ++i)
        DTVRecorder::BufferedWrite(_scratch[i], insert);
}

void DTVRecorder::HandleSingleProgramPMT(ProgramMapTable *pmt, bool insert)
{
    if (!pmt)
    {
        LOG(VB_RECORD, LOG_ERR, LOC + "HandleSingleProgramPMT(NULL)");
        return;
    }

    // collect stream types for H.264 (MPEG-4 AVC) keyframe detection
    for (uint i = 0; i < pmt->StreamCount(); ++i)
        _stream_id[pmt->StreamPID(i)] = pmt->StreamType(i);

    if (!ringBuffer)
        return;

    uint next_cc = (pmt->tsheader()->ContinuityCounter()+1)&0xf;
    pmt->tsheader()->SetContinuityCounter(next_cc);
    pmt->GetAsTSPackets(_scratch, next_cc);

    for (uint i = 0; i < _scratch.size(); ++i)
        DTVRecorder::BufferedWrite(_scratch[i], insert);
}

bool DTVRecorder::ProcessTSPacket(const TSPacket &tspacket)
{
    const uint pid = tspacket.PID();

    if (pid != 0x1fff)
        _packet_count.fetchAndAddAcquire(1);

    // Check continuity counter
    uint old_cnt = _continuity_counter[pid];
    if ((pid != 0x1fff) && !CheckCC(pid, tspacket.ContinuityCounter()))
    {
        int v = _continuity_error_count.fetchAndAddRelaxed(1) + 1;
        double erate = v * 100.0 / _packet_count.fetchAndAddRelaxed(0);
        LOG(VB_RECORD, LOG_WARNING, LOC +
            QString("PID 0x%1 discontinuity detected ((%2+1)\%16!=%3) %4\%")
                .arg(pid,0,16).arg(old_cnt,2)
                .arg(tspacket.ContinuityCounter(),2)
                .arg(erate));
    }

    // Only create fake keyframe[s] if there are no audio/video streams
    if (_input_pmt && _has_no_av)
    {
        FindOtherKeyframes(&tspacket);
        _buffer_packets = false;
    }
    else
    {
        // There are audio/video streams. Only write the packet
        // if audio/video key-frames have been found
        if (_wait_for_keyframe_option && _first_keyframe < 0)
            return true;

        _buffer_packets = true;
    }

    BufferedWrite(tspacket);

    return true;
}

bool DTVRecorder::ProcessVideoTSPacket(const TSPacket &tspacket)
{
    if (!ringBuffer)
        return true;

    uint streamType = _stream_id[tspacket.PID()];

    if (tspacket.HasPayload() && tspacket.PayloadStart())
    {
        // buffer packets until we know if this is a keyframe
        _buffer_packets = true;
    }

    // Check for keyframes and count frames
    if (streamType == StreamID::H264Video)
        FindH264Keyframes(&tspacket);
    else if (streamType != 0)
        FindMPEG2Keyframes(&tspacket);
    else
        LOG(VB_RECORD, LOG_ERR, LOC +
            "ProcessVideoTSPacket: unknown stream type!");

    return ProcessAVTSPacket(tspacket);
}

bool DTVRecorder::ProcessAudioTSPacket(const TSPacket &tspacket)
{
    if (!ringBuffer)
        return true;

    if (tspacket.HasPayload() && tspacket.PayloadStart())
    {
        // buffer packets until we know if this is a keyframe
        _buffer_packets = true;
    }

    FindAudioKeyframes(&tspacket);
    return ProcessAVTSPacket(tspacket);
}

/// Common code for processing either audio or video packets
bool DTVRecorder::ProcessAVTSPacket(const TSPacket &tspacket)
{
    // Sync recording start to first keyframe
    if (_wait_for_keyframe_option && _first_keyframe < 0)
    {
        if (_buffer_packets)
            BufferedWrite(tspacket);
        return true;
    }

    const uint pid = tspacket.PID();

    if (pid != 0x1fff)
        _packet_count.fetchAndAddAcquire(1);

    // Check continuity counter
    uint old_cnt = _continuity_counter[pid];
    if ((pid != 0x1fff) && !CheckCC(pid, tspacket.ContinuityCounter()))
    {
        int v = _continuity_error_count.fetchAndAddRelaxed(1) + 1;
        double erate = v * 100.0 / _packet_count.fetchAndAddRelaxed(0);
        LOG(VB_RECORD, LOG_WARNING, LOC +
            QString("A/V PID 0x%1 discontinuity detected ((%2+1)\%16!=%3) %4\%")
                .arg(pid,0,16).arg(old_cnt).arg(tspacket.ContinuityCounter())
                .arg(erate,5,'f',2));
    }

    if (!(_pid_status[pid] & kPayloadStartSeen))
    {
        _pid_status[pid] |= kPayloadStartSeen;
        LOG(VB_RECORD, LOG_INFO, LOC +
            QString("PID 0x%1 Found Payload Start").arg(pid,0,16));
    }

    BufferedWrite(tspacket);

    return true;
}

RecordingQuality *DTVRecorder::GetRecordingQuality(const RecordingInfo *r) const
{
    RecordingQuality *recq = RecorderBase::GetRecordingQuality(r);
    recq->AddTSStatistics(
        _continuity_error_count.fetchAndAddRelaxed(0),
        _packet_count.fetchAndAddRelaxed(0));
    return recq;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
