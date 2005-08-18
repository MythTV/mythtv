/**
 *  DTVRecorder -- base class for DVBRecorder and HDTVRecorder
 *  Copyright 2003-2004 by Brandon Beattie, Doug Larrick, 
 *    Jason Hoos, and Daniel Thor Kristjansson
 *  Distributed as part of MythTV under GPL v2 and later.
 */

using namespace std;

#include "RingBuffer.h"
#include "programinfo.h"
#include "tspacket.h"
#include "dtvrecorder.h"

/** \class DTVRecorder
 *  \brief This is a specialization of RecorderBase used to
 *         handle DVB and ATSC streams.
 *
 *  This class is an abstract class. If you are using a
 *  pcHDTV card with the bttv drivers, ATSC streams are
 *  handled by the HDTVRecorder. If you are using DVB
 *  drivers DVBRecorder is used. If you are using firewire
 *  cable box input the FirewireRecorder is used.
 *
 *  \sa DVBRecorder, HDTVRecorder, FirewrireRecorder, DBox2Recorder
 */

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
            VERBOSE(VB_IMPORTANT, "Error: Attempt made to "
                    "resize packet buffer while recording.");
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
            VERBOSE(VB_IMPORTANT, "Error: could not allocate "
                    "new packet buffer.");
    }
}

// documented in recorderbase.h
bool DTVRecorder::WaitForPause(int timeout)
{
    if (!_paused && !pauseWait.wait(timeout))
    {
        VERBOSE(VB_IMPORTANT, "Waited too long for recorder to pause");
        return false;
    }
    return true;
}

/** \fn DTVRecorder::FinishRecording()
 *  \brief Flushes the ringbuffer, and if this is not a live LiveTV
 *         recording saves the position map and filesize.
 */
void DTVRecorder::FinishRecording(void)
{
    // stealth call to flush
    (ringBuffer->Seek(0, SEEK_CUR), ringBuffer->Sync());

    if (curRecording)
    {
        curRecording->SetFilesize(ringBuffer->GetRealFileSize());
        if (_position_map_delta.size())
        {
            curRecording->SetPositionMapDelta(_position_map_delta,
                                              MARK_GOP_BYFRAME);
            _position_map_delta.clear();
        }
    }
}

// documented in recorderbase.h
long long DTVRecorder::GetKeyframePosition(long long desired)
{
    long long ret = -1;

    if (_position_map.find(desired) != _position_map.end())
        ret = _position_map[desired];

    return ret;
}

// documented in recorderbase.h
void DTVRecorder::Reset()
{
    _error = false;

    _frames_seen_count = 0;
    _frames_written_count = 0;

    _first_keyframe = 0;
    _position_within_gop_header = 0;
    _keyframe_seen = false;
    _last_gop_seen = 0;
    _last_seq_seen = 0;
}

/** \fn DTVRecorder::FindKeyframes(const TSPacket* tspacket)
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
 */
void DTVRecorder::FindKeyframes(const TSPacket* tspacket)
{
#define AVG_KEYFRAME_DIFF 16
#define MAX_KEYFRAME_DIFF 32
#define DEBUG_FIND_KEY_FRAMES 0 /* set to 1 to debug */
    bool noPayload = !tspacket->HasPayload();
    bool payloadStart = tspacket->PayloadStart();

    if (noPayload)
        return; // no payload to scan

    if (payloadStart)
    {
        // packet contains start of PES packet
        // start looking for first byte of pattern
        _position_within_gop_header = 0;
    }

    // Scan for PES header codes; specifically picture_start
    // and group_start (of_pictures).  These should be within
    // this first TS packet of the PES packet.
    //   00 00 01 00: picture_start_code
    //   00 00 01 B8: group_start_code
    //   00 00 01 B3: seq_start_code
    //   (there are others that we don't care about)
    long long frameSeenNum = _frames_seen_count;
    const unsigned char *buffer = tspacket->data();
    for (unsigned int i = tspacket->AFCOffset(); i+1<TSPacket::SIZE; i++)
    {
        const unsigned char k = buffer[i];
        if (0 == _position_within_gop_header)
            _position_within_gop_header = (k == 0x00) ? 1 : 0;
        else if (1 == _position_within_gop_header)
            _position_within_gop_header = (k == 0x00) ? 2 : 0;
        else 
        {
            if (0x01 != k)
            {
                _position_within_gop_header = (k == 0x00) ? 2 : 0;
                continue;
            }
            const unsigned char k1 = buffer[i+1];
            if (0x00 == k1)
            {   //   00 00 01 00: picture_start_code
                _frames_written_count += (_first_keyframe > 0) ? 1 : 0;
                _frames_seen_count++;

                bool f16 = (0 == (_frames_seen_count & 0xf));
                if (f16 &&
                    (_last_gop_seen + MAX_KEYFRAME_DIFF < frameSeenNum) &&
                    (_last_seq_seen + MAX_KEYFRAME_DIFF < frameSeenNum))
                {
                    // We've seen MAX_KEYFRAME_DIFF frames and no GOP
                    // or seq header? let's pretend we have them
#if DEBUG_FIND_KEY_FRAMES
                    VERBOSE(VB_RECORD,
                            QString("f16 sc(%1) wc(%2) lgop(%3) lseq(%4)")
                            .arg(_frames_seen_count).arg(_frames_written_count)
                            .arg(_last_gop_seen).arg(_last_seq_seen));
#endif
                    HandleKeyframe();
                    _last_keyframe_seen = frameSeenNum;
                }
            }
            else if (0xB8 == k1)
            {   //   00 00 01 B8: group_start_code
#if DEBUG_FIND_KEY_FRAMES
                VERBOSE(VB_RECORD,
                        QString("GOP sc(%1) wc(%2) lgop(%3) lseq(%4)")
                        .arg(_frames_seen_count).arg(_frames_written_count)
                        .arg(_last_gop_seen).arg(_last_seq_seen));
#endif
                HandleKeyframe();
                _last_keyframe_seen = _last_gop_seen = frameSeenNum;
            }
            else if (0xB3 == k1)
            {   //   00 00 01 B3: seq_start_code
                if (_last_gop_seen + MAX_KEYFRAME_DIFF < frameSeenNum)
                {
#if DEBUG_FIND_KEY_FRAMES
                    VERBOSE(VB_RECORD,
                            QString("seq sc(%1) wc(%2) lgop(%3) lseq(%4)")
                            .arg(_frames_seen_count).arg(_frames_written_count)
                            .arg(_last_gop_seen).arg(_last_seq_seen));
#endif
                    HandleKeyframe();
                    _last_keyframe_seen = frameSeenNum;
                }
                _last_seq_seen = frameSeenNum;
            }
            _position_within_gop_header = 0;
        }
    }
#undef AVG_KEYFRAME_DIFF
#undef MAX_KEYFRAME_DIFF
#undef DEBUG_FIND_KEY_FRAMES
}

/** \fn DTVRecorder::HandleKeyframe()
 *  \brief This save the current frame to the position maps.
 *
 *   This looks up the ringBuffer->GetFileWritePosition()
 *   and saves it to both the _position_map and _position_map_delta
 *   then it saves the _position_map_delta to the database
 *   every few 30 key frames, or every 5th key frame when
 *   the first 30 key frames are saved.
 */
void DTVRecorder::HandleKeyframe(void)
{
    long long frameNum = _frames_written_count - 1;

    if (!_keyframe_seen && _frames_seen_count > 0)
    {
        if (_first_keyframe > 0)
            _keyframe_seen = true;
        else
            _first_keyframe = (frameNum > 0) ? frameNum : 1;
    }

    long long startpos = ringBuffer->GetFileWritePosition();

    if (!_position_map.contains(frameNum))
    {
        VERBOSE(VB_RECORD, "DTV: pm#"
                <<_position_map.size()<<"["<<frameNum<<"]: "
                <<startpos<<" cr("<<curRecording<<")");
        _position_map_delta[frameNum] = startpos;
        _position_map[frameNum] = startpos;

        if (curRecording &&
            (((_position_map.size() < 30) && (_position_map.size()%5 == 1)) ||
             (_position_map_delta.size() >= 30)))
        {
            curRecording->SetPositionMapDelta(_position_map_delta, 
                                              MARK_GOP_BYFRAME);
            curRecording->SetFilesize(startpos);
            _position_map_delta.clear();
        }
    }
}
