/**
 *  DTVRecorder -- base class for DVBRecorder and HDTVRecorder
 *  Copyright 2003-2004 by Brandon Beattie, Doug Larrick,
 *    Jason Hoos, and Daniel Thor Kristjansson
 *  Device ringbuffer added by John Poet
 *  Distributed as part of MythTV under GPL v2 and later.
 */

#include <cassert>
#include <cerrno>
#include <unistd.h>

using namespace std;

#include "RingBuffer.h"
#include "programinfo.h"
#include "tspacket.h"
#include "dtvrecorder.h"

//#define REPORT_RING_STATS 1

DTVRecorder::DTVRecorder(void) :
    _first_keyframe(0), _position_within_gop_header(0),
    _scanning_pes_header_for_gop(0), _keyframe_seen(false),
    _last_keyframe_seen(0), _last_gop_seen(0), _last_seq_seen(0),
    _stream_fd(-1), _error(false),
    _request_recording(false), _request_pause(false),
    _wait_for_keyframe_option(true),
    _recording(false), _paused(false), _wait_for_keyframe(true),
    _buffer(0), _buffer_size(0),
    _frames_seen_count(0), _frames_written_count(0)
{
    ringbuf.run = false;
    ringbuf.buffer = 0;
    pthread_mutex_destroy(&ringbuf.lock);
    pthread_mutex_destroy(&ringbuf.lock_stats);
    loop = random() % (report_loops / 2);
}

DTVRecorder::~DTVRecorder(void)
{
    pthread_mutex_destroy(&ringbuf.lock);
    pthread_mutex_destroy(&ringbuf.lock_stats);
}

void DTVRecorder::StopRecording(void)
{
    bool            run;
    int             idx;

    _request_pause = true;
    for (idx = 0; idx < 400; ++idx)
    {
        if (GetPause())
            break;
        pauseWait.wait(500);
    }

    _request_recording = false;

    pthread_mutex_lock(&ringbuf.lock);
    run = ringbuf.run;
    ringbuf.run = false;
    pthread_mutex_unlock(&ringbuf.lock);

    if (run)
        pthread_join(ringbuf.thread, NULL);

    if (idx == 400)
    {
        // Better to have a memory leak, then a segfault?
        VERBOSE(VB_IMPORTANT, "DTV ringbuffer not cleaned up!\n");
    }
    else
    {
        delete[] ringbuf.buffer;
        ringbuf.buffer = 0;
    }
}

void DTVRecorder::SetOption(const QString &name, int value)
{
    if (name == "wait_for_seqstart")
        _wait_for_keyframe_option = (value == 1);
    else if (name == "pkt_buf_size")
    {
        if (_request_recording)
        {
            VERBOSE(VB_IMPORTANT, "Error: Attempt made to resize packet buffer while recording.");
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
            VERBOSE(VB_IMPORTANT, "Error: could not allocate new packet buffer.");
    }
}

void DTVRecorder::WaitForPause(void)
{
    if (!GetPause())
        if (!pauseWait.wait(1000))
            VERBOSE(VB_IMPORTANT,
                    QString("Waited too long for recorder to pause"));
}

void DTVRecorder::FinishRecording(void)
{
    (ringBuffer->Seek(0, SEEK_CUR), ringBuffer->Sync()); // stealth call to flush

    if (curRecording && db_lock && db_conn)
    {
        pthread_mutex_lock(db_lock);
        MythContext::KickDatabase(db_conn);

        curRecording->SetFilesize(ringBuffer->GetRealFileSize(), db_conn);
        if (_position_map_delta.size())
        {
            curRecording->SetPositionMapDelta(_position_map_delta,
                                              MARK_GOP_BYFRAME, db_conn);
            _position_map_delta.clear();
        }

        pthread_mutex_unlock(db_lock);
    }
}

void DTVRecorder::Pause(bool /*clear*/)
{
    pthread_mutex_lock(&ringbuf.lock);
    ringbuf.paused = false;
    pthread_mutex_unlock(&ringbuf.lock);
    _request_pause = true;
}

bool DTVRecorder::GetPause(void)
{
    bool paused;

    pthread_mutex_lock(&ringbuf.lock);
    paused = ringbuf.paused;
    pthread_mutex_unlock(&ringbuf.lock);

    return paused;
}

long long DTVRecorder::GetKeyframePosition(long long desired)
{
    long long ret = -1;

    if (_position_map.find(desired) != _position_map.end())
        ret = _position_map[desired];

    return ret;
}

void DTVRecorder::GetBlankFrameMap(QMap<long long, int> &blank_frame_map)
{
    (void)blank_frame_map;
}

void DTVRecorder::Reset()
{
    _error = false;

    _frames_seen_count = 0;
    _frames_written_count = 0;

    _first_keyframe = 0;
    _position_within_gop_header = 0;
    _scanning_pes_header_for_gop = false;
    _keyframe_seen = false;
    _last_gop_seen = 0;
    _last_seq_seen = 0;
}

void DTVRecorder::FindKeyframes(const TSPacket* tspacket)
{
#define AVG_KEYFRAME_DIFF 16
#define MAX_KEYFRAME_DIFF 32
#define DEBUG_FIND_KEY_FRAMES 0 /* set to 1 to debug */
    bool noPayload = !tspacket->HasPayload();
    bool payloadStart = tspacket->PayloadStart();

    if (noPayload | (!payloadStart & !_scanning_pes_header_for_gop))
        return; // not scanning or no payload to scan

    if (payloadStart)
    { // packet contains start of PES packet
        _scanning_pes_header_for_gop = true; // start scanning for PES headers
        _position_within_gop_header = 0; // start looking for first byte of pattern
    }

    // Scan for PES header codes; specifically picture_start
    // and group_start (of_pictures).  These should be within
    // this first TS packet of the PES packet.
    //   00 00 01 00: picture_start_code
    //   00 00 01 B8: group_start_code
    //   00 00 01 B3: seq_start_code
    //   (there are others that we don't care about)
    unsigned int i = tspacket->AFCOffset(); // borked if AFCOffset > 188
    long long frameSeenNum = _frames_seen_count;
    const unsigned char *buffer = tspacket->data();
    for (;(i+1<TSPacket::SIZE) && _scanning_pes_header_for_gop; i++)
    {
        const unsigned char k = buffer[i];
        if (0 == _position_within_gop_header)
            _position_within_gop_header = (k == 0x00) ? 1 : 0;
        else if (1 == _position_within_gop_header)
            _position_within_gop_header = (k == 0x00) ? 2 : 0;
        else
        {
            assert(2 == _position_within_gop_header);
            if (0x01 != k)
            {
                _position_within_gop_header = (k) ? 0 : 2;
                continue;
            }
            assert(0x01 == k);
            const unsigned char k1 = buffer[i+1];
            if (0x00 == k1)
            {   //   00 00 01 00: picture_start_code
                _frames_written_count += (_first_keyframe > 0) ? 1 : 0;
                _frames_seen_count++;
                // We've seen 30 frames and no GOP or seq header? let's pretend we have them
                if ((0==(_frames_seen_count & 0xf)) &&
                    (_last_gop_seen+(MAX_KEYFRAME_DIFF+AVG_KEYFRAME_DIFF))<frameSeenNum &&
                    (_last_seq_seen+(MAX_KEYFRAME_DIFF+AVG_KEYFRAME_DIFF))<frameSeenNum) {
#if DEBUG_FIND_KEY_FRAMES
                    VERBOSE(VB_RECORD, QString("f16 sc(%1) wc(%2) lgop(%3) lseq(%4)").
                            arg(_frames_seen_count).arg(_frames_written_count).
                            arg(_last_gop_seen).arg(_last_seq_seen));
#endif
                    HandleKeyframe();
                    _last_keyframe_seen = frameSeenNum;
                }
            } else if (0xB8 == k1)
            {   //   00 00 01 B8: group_start_code
#if DEBUG_FIND_KEY_FRAMES
                VERBOSE(VB_RECORD, QString("GOP sc(%1) wc(%2) lgop(%3) lseq(%4)").
                        arg(_frames_seen_count).arg(_frames_written_count).
                        arg(_last_gop_seen).arg(_last_seq_seen));
#endif
                HandleKeyframe();
                _last_keyframe_seen = _last_gop_seen = frameSeenNum;
            } else if (0xB3 == k1 && ((_last_gop_seen+MAX_KEYFRAME_DIFF)<frameSeenNum))
            {   //   00 00 01 B3: seq_start_code
#if DEBUG_FIND_KEY_FRAMES
                VERBOSE(VB_RECORD, QString("seq sc(%1) wc(%2) lgop(%3) lseq(%4)").
                        arg(_frames_seen_count).arg(_frames_written_count).
                        arg(_last_gop_seen).arg(_last_seq_seen));
#endif
                HandleKeyframe();
                _last_keyframe_seen = _last_seq_seen = frameSeenNum;
            }
            // video slice ... end of "interesting" headers
            if (0x01 <= k1 && k1 <= 0xAF)
                _scanning_pes_header_for_gop = false;
            _position_within_gop_header = 0;
        }
    }
#undef AVG_KEYFRAME_DIFF
#undef MAX_KEYFRAME_DIFF
#undef DEBUG_FIND_KEY_FRAMES
}

void DTVRecorder::HandleKeyframe() {
    long long frameNum = _frames_written_count - 1;

    if (!_keyframe_seen && _frames_seen_count > 0)
    {
        if (_first_keyframe > 0)
            _keyframe_seen = true;
        else
            _first_keyframe = (frameNum > 0) ? frameNum : 1;
    }

    long long startpos =
        ringBuffer->GetFileWritePosition();

    if (!_position_map.contains(frameNum))
    {
        _position_map_delta[frameNum] = startpos;
        _position_map[frameNum] = startpos;

        if (curRecording && db_lock && db_conn &&
            (_position_map_delta.size() == 30))
        {
            pthread_mutex_lock(db_lock);
            MythContext::KickDatabase(db_conn);
            curRecording->SetPositionMapDelta(_position_map_delta,
                                              MARK_GOP_BYFRAME, db_conn);
            curRecording->SetFilesize(startpos, db_conn);
            pthread_mutex_unlock(db_lock);
            _position_map_delta.clear();
        }
    }
}

void * DTVRecorder::boot_ringbuffer(void * arg)
{
    DTVRecorder *dtv = (DTVRecorder *)arg;
    dtv->fill_ringbuffer();
    return NULL;
}

void DTVRecorder::fill_ringbuffer(void)
{
    int       errcnt = 0;
    int       len;
    size_t    unused, used;
    size_t    contiguous;
    size_t    read_size;
    bool      run, request_pause, paused;

    for (;;)
    {
        pthread_mutex_lock(&ringbuf.lock);
        run = ringbuf.run;
        unused = ringbuf.size - ringbuf.used;
        request_pause = ringbuf.request_pause;
        paused = ringbuf.paused;
        pthread_mutex_unlock(&ringbuf.lock);

        if (!run)
            break;

        if (request_pause)
        {
            pthread_mutex_lock(&ringbuf.lock);
            ringbuf.paused = true;
            pthread_mutex_unlock(&ringbuf.lock);

            pauseWait.wakeAll();

            usleep(1000);
            continue;
        }
        else if (paused)
        {
            pthread_mutex_lock(&ringbuf.lock);
            ringbuf.writePtr = ringbuf.readPtr = ringbuf.buffer;
            ringbuf.used = 0;
            ringbuf.paused = false;
            pthread_mutex_unlock(&ringbuf.lock);
        }

        contiguous = ringbuf.endPtr - ringbuf.writePtr;

        while (unused < TSPacket::SIZE && contiguous > TSPacket::SIZE)
        {
            usleep(500);

            pthread_mutex_lock(&ringbuf.lock);
            unused = ringbuf.size - ringbuf.used;
            request_pause = ringbuf.request_pause;
            pthread_mutex_unlock(&ringbuf.lock);

            if (request_pause)
                break;
        }
        if (request_pause)
            continue;

        read_size = unused > contiguous ? contiguous : unused;
        if (read_size > ringbuf.dev_read_size)
            read_size = ringbuf.dev_read_size;

        len = read(_stream_fd, ringbuf.writePtr, read_size);

        if (len < 0)
        {
            if (errno == EINTR)
                continue;

            VERBOSE(VB_IMPORTANT, QString("HD7 error reading from %1")
                    .arg(videodevice));
            perror("read");
            if (++errcnt > 10)
            {
                pthread_mutex_lock(&ringbuf.lock);
                ringbuf.error = true;
                pthread_mutex_unlock(&ringbuf.lock);

                break;
            }

            usleep(500);
            continue;
        }
        else if (len == 0)
        {
            if (++errcnt > 20)
            {
                VERBOSE(VB_IMPORTANT, QString("HD8 %1 end of file found.")
                        .arg(videodevice));

                pthread_mutex_lock(&ringbuf.lock);
                ringbuf.eof = true;
                pthread_mutex_unlock(&ringbuf.lock);

                break;
            }
            usleep(500);
            continue;
        }

        errcnt = 0;

        pthread_mutex_lock(&ringbuf.lock);
        ringbuf.used += len;
        used = ringbuf.used;
        ringbuf.writePtr += len;
        pthread_mutex_unlock(&ringbuf.lock);

#ifdef REPORT_RING_STATS
        pthread_mutex_lock(&ringbuf.lock_stats);

        if (ringbuf.max_used < used)
            ringbuf.max_used = used;

        ringbuf.avg_used = ((ringbuf.avg_used * ringbuf.avg_cnt) + used)
                           / ++ringbuf.avg_cnt;
        pthread_mutex_unlock(&ringbuf.lock_stats);
#endif

        if (ringbuf.writePtr == ringbuf.endPtr)
            ringbuf.writePtr = ringbuf.buffer;
    }

    pthread_exit(reinterpret_cast<void *>(0));
}

/* read count bytes from ring into buffer */
int DTVRecorder::ringbuf_read(unsigned char *buffer, size_t count)
{
    size_t          avail;
    size_t          cnt = count;
    size_t          min_read;
    unsigned char  *cPtr = buffer;

    pthread_mutex_lock(&ringbuf.lock);
    avail = ringbuf.used;
    pthread_mutex_unlock(&ringbuf.lock);

    min_read = cnt < ringbuf.min_read ? cnt : ringbuf.min_read;

    while (min_read > avail)
    {
        usleep(100000);

        if (_request_pause)
            return 0;

        pthread_mutex_lock(&ringbuf.lock);
        avail = ringbuf.used;
        pthread_mutex_unlock(&ringbuf.lock);
    }
    if (cnt > avail)
        cnt = avail;

    if (ringbuf.readPtr + cnt > ringbuf.endPtr)
    {
        size_t      len;

        // Process as two pieces
        len = ringbuf.endPtr - ringbuf.readPtr;
        memcpy(cPtr, ringbuf.readPtr, len);
        cPtr += len;
        len = cnt - len;

        // Wrap arround to begining of buffer
        ringbuf.readPtr = ringbuf.buffer;
        memcpy(cPtr, ringbuf.readPtr, len);
        ringbuf.readPtr += len;
    }
    else
    {
// !
        memcpy(cPtr, ringbuf.readPtr, cnt);
        ringbuf.readPtr += cnt;
    }

    pthread_mutex_lock(&ringbuf.lock);
    ringbuf.used -= cnt;
    pthread_mutex_unlock(&ringbuf.lock);

    if (ringbuf.readPtr == ringbuf.endPtr)
        ringbuf.readPtr = ringbuf.buffer;
    else
    {
#ifdef REPORT_RING_STATS
        size_t samples, avg, max;

        if (++loop == report_loops)
        {
            loop = 0;
            pthread_mutex_lock(&ringbuf.lock_stats);
            avg = ringbuf.avg_used;
            samples = ringbuf.avg_cnt;
            max = ringbuf.max_used;
            ringbuf.avg_used = 0;
            ringbuf.avg_cnt = 0;
            ringbuf.max_used = 0;
            pthread_mutex_unlock(&ringbuf.lock_stats);

            VERBOSE(VB_IMPORTANT, QString("%1 ringbuf avg %2% max %3%"
                                          " samples %4")
                    .arg(videodevice)
                    .arg((static_cast<double>(avg)
                          / ringbuf.size) * 100.0)
                    .arg((static_cast<double>(max)
                          / ringbuf.size) * 100.0)
                    .arg(samples));
        }
        else
#endif
            usleep(25);
    }

    return cnt;
}
