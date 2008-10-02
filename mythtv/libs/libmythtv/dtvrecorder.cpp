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
// from libavcodec
extern const uint8_t *ff_find_start_code(const uint8_t * restrict p, const uint8_t *end, uint32_t * restrict state);
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
    // TS packet buffer
    _buffer(0),                     _buffer_size(0),
    // keyframe TS buffer
    _buffer_packets(false),
    // statistics
    _frames_seen_count(0),          _frames_written_count(0)
{
    SetPositionMapType(MARK_GOP_BYFRAME);
}

DTVRecorder::~DTVRecorder()
{
}

void DTVRecorder::SetOption(const QString &name, const QString &value)
{
    if (name == "recordingtype")
        _recording_type = QDeepCopy<QString>(value);
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
            curRecording->SetFilesize(ringBuffer->GetRealFileSize());
        SavePositionMap(true);
    }
    positionMapLock.lock();
    positionMap.clear();
    positionMapDelta.clear();
    positionMapLock.unlock();
}

// documented in recorderbase.h
long long DTVRecorder::GetKeyframePosition(long long desired)
{
    QMutexLocker locker(&positionMapLock);
    long long ret = -1;

    if (positionMap.find(desired) != positionMap.end())
        ret = positionMap[desired];

    return ret;
}

void DTVRecorder::ResetForNewFile(void)
{
    VERBOSE(VB_RECORD, LOC + "ResetForNewFile(void)");
    QMutexLocker locker(&positionMapLock);

    //_start_code
    _first_keyframe             =-1;
    _has_written_other_keyframe = false;
    _last_keyframe_seen         = 0;
    _last_gop_seen              = 0;
    _last_seq_seen              = 0;
    //_recording
    _error                      = false;
    //_buffer
    //_buffer_size
    _frames_seen_count          = 0;
    _frames_written_count       = 0;
    _pes_synced                 = false;
    _seen_sps                   = false;
    _h264_kf_seq.Reset();
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
            curRecording->SetFilesize(ringBuffer->GetRealFileSize());
    }

    // Then we set the next info
    nextRingBufferLock.lock();

    nextRecording = NULL;
    if (progInf)
        nextRecording = new ProgramInfo(*progInf);

    nextRingBuffer = rb;
    nextRingBufferLock.unlock();
}

/** \fn DTVRecorder::HandleKeyframe(void)
 *  \brief This save the current frame to the position maps
 *         and handles ringbuffer switching.
 */
void DTVRecorder::HandleKeyframe(void)
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
        startpos += _payload_buffer.size();
        positionMapDelta[frameNum] = startpos;
        positionMap[frameNum]      = startpos;
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
        return false;

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
        uint32_t bytes_used = _h264_kf_seq.AddBytes(tspacket->data() + i,
                                                   TSPacket::SIZE - i,
                                                   ringBuffer->GetWritePosition());
        i += (bytes_used - 1);

        // special handling when we've synced to a NAL unit
        if (_h264_kf_seq.HasStateChanged())
        {
            if (_h264_kf_seq.LastSyncedType() == H264::NALUnitType::SPS)
                _seen_sps = true;

            if (_h264_kf_seq.IsOnKeyframe())
            {
                hasKeyFrame = true;
                hasFrame = true;
            }

            if (_h264_kf_seq.IsOnFrame())
                hasFrame = true;
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
        positionMapDelta[frameNum] = _h264_kf_seq.KeyframeAUStreamOffset();
        positionMap[frameNum]      = _h264_kf_seq.KeyframeAUStreamOffset();
    }
    positionMapLock.unlock();

    // Perform ringbuffer switch if needed.
    CheckForRingBufferSwitch();
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
