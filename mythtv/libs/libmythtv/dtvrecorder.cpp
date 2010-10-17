/**
 *  DTVRecorder -- base class for Digital Televison recorders
 *  Copyright 2003-2004 by Brandon Beattie, Doug Larrick,
 *    Jason Hoos, and Daniel Thor Kristjansson
 *  Distributed as part of MythTV under GPL v2 and later.
 */

#include "RingBuffer.h"
#include "programinfo.h"
#include "mpegtables.h"
#include "mpegstreamdata.h"
#include "dtvrecorder.h"
#include "tv_rec.h"

extern "C" {
extern const uint8_t *ff_find_start_code(const uint8_t *p, const uint8_t *end, uint32_t *state);
}

#define LOC QString("DTVRec(%1): ").arg(tvrec->GetCaptureCardNum())
#define LOC_ERR QString("DTVRec(%1) Error: ").arg(tvrec->GetCaptureCardNum())

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
    // H.264 support
    _pes_synced(false),
    _seen_sps(false),
    // settings
    _request_recording(false),
    _wait_for_keyframe_option(true),
    _has_written_other_keyframe(false),
    // state
    _recording(false),
    _error(false),
    _stream_data(NULL),
    // TS packet buffer
    _buffer(0),                     _buffer_size(0),
    // keyframe TS buffer
    _buffer_packets(false),
    // statistics
    _frames_seen_count(0),          _frames_written_count(0)
{
    SetPositionMapType(MARK_GOP_BYFRAME);
    _payload_buffer.reserve(TSPacket::SIZE * (50 + 1));
}

DTVRecorder::~DTVRecorder()
{
    SetStreamData(NULL);
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
 *  \brief handles the "wait_for_seqstart" and "pkt_buf_size" options.
 */
void DTVRecorder::SetOption(const QString &name, int value)
{
    if (name == "wait_for_seqstart")
        _wait_for_keyframe_option = (value == 1);
    else if (name == "pkt_buf_size")
    {
        if (_request_recording)
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR +
                    "Attempt made to resize packet buffer while recording.");
            return;
        }
        int newsize = max(value - (value % TSPacket::SIZE), TSPacket::SIZE*50);
        unsigned char* newbuf = new unsigned char[newsize];
        if (newbuf) {
            memcpy(newbuf, _buffer, min(_buffer_size, newsize));
            memset(newbuf+_buffer_size, 0xFF, max(newsize-_buffer_size, 0));
            _buffer = newbuf;
            _buffer_size = newsize;
        }
        else
            VERBOSE(VB_IMPORTANT, LOC_ERR +
                    "Could not allocate new packet buffer.");
    }
}

/** \fn DTVRecorder::FinishRecording(void)
 *  \brief Flushes the ringbuffer, and if this is not a live LiveTV
 *         recording saves the position map and filesize.
 */
void DTVRecorder::FinishRecording(void)
{
    if (ringBuffer)
    {
        if (_payload_buffer.size())
        {
            ringBuffer->Write(&_payload_buffer[0], _payload_buffer.size());
            _payload_buffer.clear();
        }
        ringBuffer->WriterFlush();
    }

    if (curRecording)
    {
        if (ringBuffer)
            curRecording->SaveFilesize(ringBuffer->GetRealFileSize());
        SavePositionMap(true);
    }
//     positionMapLock.lock();
//     positionMap.clear();
//     positionMapDelta.clear();
//     positionMapLock.unlock();
}

void DTVRecorder::ResetForNewFile(void)
{
    VERBOSE(VB_RECORD, LOC + "ResetForNewFile(void)");
    QMutexLocker locker(&positionMapLock);

    _start_code                 = 0xffffffff;
    _first_keyframe             =-1;
    _has_written_other_keyframe = false;
    _last_keyframe_seen         = 0;
    _last_gop_seen              = 0;
    _last_seq_seen              = 0;
    _audio_bytes_remaining      = 0;
    _video_bytes_remaining      = 0;
    _other_bytes_remaining      = 0;
    //_recording
    _error                      = false;
    //_buffer
    //_buffer_size
    _frames_seen_count          = 0;
    _frames_written_count       = 0;
    _pes_synced                 = false;
    _seen_sps                   = false;
    m_h264_parser.Reset();
    positionMap.clear();
    positionMapDelta.clear();
    _payload_buffer.clear();
}

// documented in recorderbase.h
void DTVRecorder::Reset(void)
{
    VERBOSE(VB_RECORD, LOC + "Reset(void)");
    ResetForNewFile();

    _start_code = 0xffffffff;

    if (curRecording)
        curRecording->ClearPositionMap(MARK_GOP_BYFRAME);
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
        SetStreamData();
}


void DTVRecorder::BufferedWrite(const TSPacket &tspacket)
{
    // delay until first GOP to avoid decoder crash on res change
    if (_wait_for_keyframe_option && _first_keyframe<0)
        return;

    // Do we have to buffer the packet for exact keyframe detection?
    if (_buffer_packets)
    {
        int idx = _payload_buffer.size();
        _payload_buffer.resize(idx + TSPacket::SIZE);
        memcpy(&_payload_buffer[idx], tspacket.data(), TSPacket::SIZE);
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

    if (ringBuffer)
        ringBuffer->Write(tspacket.data(), TSPacket::SIZE);
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
    bool haveBufferedData = !_payload_buffer.empty();
    if (!tspacket->HasPayload()) // no payload to scan
        return !haveBufferedData;

    if (!ringBuffer)
        return !haveBufferedData;

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
    //   (there are others that we don't care about)
    const uint8_t *bufptr = tspacket->data() + tspacket->AFCOffset();
    const uint8_t *bufend = tspacket->data() + TSPacket::SIZE;

    while (bufptr < bufend)
    {
        bufptr = ff_find_start_code(bufptr, bufend, &_start_code);
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

    if (hasKeyFrame)
    {
        _last_keyframe_seen = _frames_seen_count;
        HandleKeyframe();
    }

    if (hasFrame)
    {
        _frames_seen_count++;
        if (!_wait_for_keyframe_option || _first_keyframe>=0)
            _frames_written_count++;
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
        VERBOSE(VB_RECORD, QString("FindMPEG2Keyframes: frame rate = %1")
                .arg(frameRate));
        FrameRateChange(frameRate, _frames_written_count);
    }

    return hasKeyFrame || (_payload_buffer.size() >= (188*50));
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
            HandleKeyframe();
            hasKeyFrame = true;
        }

        if (!_wait_for_keyframe_option || _first_keyframe>=0)
            _frames_written_count++;
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

    VERBOSE(VB_RECORD, LOC + "DSMCC - FindOtherKeyframes() - "
            "generating initial key-frame");

    _frames_seen_count++;
    _frames_written_count++;
    _last_keyframe_seen = _frames_seen_count;

    HandleKeyframe();

    _has_written_other_keyframe = true;

    return true;
}

// documented in recorderbase.h
void DTVRecorder::SetNextRecording(const ProgramInfo *progInf, RingBuffer *rb)
{
    VERBOSE(VB_RECORD, LOC + "SetNextRecord("<<progInf<<", "<<rb<<")");
    // First we do some of the time consuming stuff we can do now
    SavePositionMap(true);
    if (ringBuffer)
    {
        ringBuffer->WriterFlush();
        if (curRecording)
            curRecording->SaveFilesize(ringBuffer->GetRealFileSize());
    }

    // Then we set the next info
    nextRingBufferLock.lock();

    nextRecording = NULL;
    if (progInf)
        nextRecording = new ProgramInfo(*progInf);

    nextRingBuffer = rb;
    nextRingBufferLock.unlock();
}

/** \fn DTVRecorder::HandleKeyframe(uint64_t)
 *  \brief This save the current frame to the position maps
 *         and handles ringbuffer switching.
 */
void DTVRecorder::HandleKeyframe(uint64_t extra)
{
    if (!ringBuffer)
        return;

    unsigned long long frameNum = _frames_written_count;

    _first_keyframe = (_first_keyframe < 0) ? frameNum : _first_keyframe;

    // Add key frame to position map
    positionMapLock.lock();
    if (!positionMap.contains(frameNum))
    {
        long long startpos = ringBuffer->GetWritePosition();
        // FIXME: handle keyframes with start code spanning over two ts packets
        startpos += _payload_buffer.size() + extra;

        // Don't put negative offsets into the database, they get munged into
        // MAX_INT64 - offset, which is an exceedingly large number, and
        // certainly not valid.
        if (startpos >= 0)
        {
            positionMapDelta[frameNum] = startpos;
            positionMap[frameNum]      = startpos;
        }
    }
    positionMapLock.unlock();

    // Perform ringbuffer switch if needed.
    CheckForRingBufferSwitch();
}

/** \fn DTVRecorder::FindH264Keyframes(const TSPacket*)
 *  \brief This searches the TS packet to identify keyframes.
 *  \param TSPacket Pointer the the TS packet data.
 *  \return Returns true if a keyframe has been found.
 */
bool DTVRecorder::FindH264Keyframes(const TSPacket *tspacket)
{
    if (!ringBuffer)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "FindH264Keyframes: No ringbuffer");
        return false;
    }

    bool haveBufferedData = !_payload_buffer.empty();
    if (!tspacket->HasPayload()) // no payload to scan
        return !haveBufferedData;

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
    for (; i < TSPacket::SIZE; i++)
    {
        // special handling required when a new PES packet begins
        if (payloadStart && !_pes_synced)
        {
            // bounds check
            if (i + 2 >= TSPacket::SIZE)
            {
                VERBOSE(VB_IMPORTANT, LOC_ERR +
                    "PES packet start code may overflow to next TS packet, aborting keyframe search");
                break;
            }

            // must find the PES start code
            if (tspacket->data()[i++] != 0x00 ||
                tspacket->data()[i++] != 0x00 ||
                tspacket->data()[i++] != 0x01)
            {
                VERBOSE(VB_IMPORTANT, LOC_ERR +
                    "PES start code not found in TS packet with PUSI set");
                break;
            }

            // bounds check
            if (i + 5 >= TSPacket::SIZE)
            {
                VERBOSE(VB_IMPORTANT, LOC_ERR +
                    "PES packet headers overflow to next TS packet, aborting keyframe search");
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
            if ((i + 6 + pes_header_length) >= TSPacket::SIZE)
            {
                VERBOSE(VB_IMPORTANT, LOC_ERR +
                    "PES packet headers overflow to next TS packet, aborting keyframe search");
                break;
            }

            // we now know where the PES payload is
            // normally, we should have used 6, but use 5 because the for loop will bump i
            i += 5 + pes_header_length;
            _pes_synced = true;

            //VERBOSE(VB_RECORD, LOC + "PES synced");
            continue;
        }

        // ain't going nowhere if we're not PES synced
        if (!_pes_synced)
            break;

        // scan for a NAL unit start code

        uint32_t bytes_used = m_h264_parser.addBytes(
            tspacket->data() + i, TSPacket::SIZE - i,
            ringBuffer->GetWritePosition() + _payload_buffer.size()
            );
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
    } // for (; i < TSPacket::SIZE; i++)

    if (hasKeyFrame)
    {
        _last_keyframe_seen = _frames_seen_count;
        HandleH264Keyframe();
    }

    if (hasFrame)
    {
        _frames_seen_count++;
        if (!_wait_for_keyframe_option || _first_keyframe >= 0)
            _frames_written_count++;
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

        VERBOSE( VB_RECORD, QString("FindH264Keyframes: timescale: %1, tick: %2, framerate: %3") 
                      .arg( m_h264_parser.GetTimeScale() ) 
                      .arg( m_h264_parser.GetUnitsInTick() )
                      .arg( frameRate ) );
        m_frameRate = frameRate;
        FrameRateChange(frameRate, _frames_written_count);
    }

    return hasKeyFrame || (_payload_buffer.size() >= (188*50));
}

/** \fn DTVRecorder::HandleH264Keyframe(void)
 *  \brief This save the current frame to the position maps
 *         and handles ringbuffer switching.
 */
void DTVRecorder::HandleH264Keyframe(void)
{
    unsigned long long frameNum = _frames_written_count;

    _first_keyframe = (_first_keyframe < 0) ? frameNum : _first_keyframe;

    // Add key frame to position map
    positionMapLock.lock();
    if (!positionMap.contains(frameNum))
    {
        positionMapDelta[frameNum] = m_h264_parser.keyframeAUstreamOffset();
        positionMap[frameNum]      = m_h264_parser.keyframeAUstreamOffset();
    }
    positionMapLock.unlock();

    // Perform ringbuffer switch if needed.
    CheckForRingBufferSwitch();
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
        bufptr = ff_find_start_code(bufptr + skip, bufend, &_start_code);
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

        if (hasKeyFrame)
        {
            _last_keyframe_seen = _frames_seen_count;
            HandleKeyframe(bufptr - bufstart);
        }

        if (hasFrame)
        {
            _frames_seen_count++;
            if (!_wait_for_keyframe_option || _first_keyframe>=0)
                _frames_written_count++;
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
            VERBOSE(VB_RECORD, QString("FindPSKeyFrames: frame rate = %1")
                    .arg(frameRate));
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
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
