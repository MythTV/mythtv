/* HDTVRecorder
   Version 0.1
   July 4th, 2003
   GPL License (c)
   By Brandon Beattie

   Portions of this code taken from mpegrecorder

   Modified Oct. 2003 by Doug Larrick
   * decodes MPEG-TS locally (does not use libavformat) for flexibility
   * output format is MPEG-TS omitting unneeded PIDs, for smaller files
     and no audio stutter on stations with >1 audio stream
   * Emits proper keyframe data to recordedmarkup db, enabling seeking

   Oct. 22, 2003:
   * delay until GOP before writing output stream at start and reset
     (i.e. channel change)
   * Rewrite PIDs after channel change to be the same as before, so
     decoder can follow

   Oct. 27, 2003 by Jason Hoos:
   * if no GOP is detected within the first 30 frames of the stream, then
     assume that it's safe to treat a picture start as a GOP.

   Oct. 30, 2003 by Jason Hoos:
   * added code to rewrite PIDs in the MPEG PAT and PMT tables.  fixes 
     a problem with stations (particularly, WGN in Chicago) that list their 
     programs in reverse order in the PAT, which was confusing ffmpeg 
     (ffmpeg was looking for program 5, but only program 2 was left in 
     the stream, so it choked).

   Nov. 3, 2003 by Doug Larrick:
   * code to enable selecting a program number for stations with
     multiple programs
   * always renumber PIDs to match outgoing program number (#1:
     0x10 (base), 0x11 (video), 0x14 (audio))
   * change expected keyframe distance to 30 frames to see if this
     lets people seek past halfway in recordings

   Jan 30, 2004 by Daniel Kristjansson
   * broke out tspacket to handle TS packets
   * added CRC check on PAT & PMT packets
   Sept 27, 2004
   * added decoding of most ATSC Tables
   * added multiple audio support


   References / example code: 
     ATSC standards a.54, a.69 (www.atsc.org)
     ts2pes from mpegutils from dvb (www.linuxtv.org)
     bbinfo from Brent Beyeler, beyeler@home.com
*/

#include <iostream>
#include <pthread.h>
#include <cstdio>
#include <cstdlib>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <ctime>
#include "videodev_myth.h"

using namespace std;

#include "hdtvrecorder.h"
#include "RingBuffer.h"
#include "mythcontext.h"
#include "programinfo.h"
#include "channel.h"
#include "mpegtables.h"
#include "atscstreamdata.h"

extern "C" {
#include "../libavcodec/avcodec.h"
#include "../libavformat/avformat.h"
#include "../libavformat/mpegts.h"
}

#ifndef L2_CACHE_SIZE_KB
#define L2_CACHE_SIZE_KB 512
#endif
#define DEFAULT_PROGRAM 1

#define WHACK_A_BUG_VIDEO 0
#define WHACK_A_BUG_AUDIO 0

#if FAKE_VIDEO
    static int fake_video_index = 0;
#define FAKE_VIDEO_NUM 4
    static const char* FAKE_VIDEO_FILES[FAKE_VIDEO_NUM] =
        {
            "/video/abc.ts",
            "/video/wb.ts",
            "/video/abc2.ts",
            "/video/nbc.ts",
        };
#endif

HDTVRecorder::HDTVRecorder()
    : RecorderBase(), _atsc_stream_fd(-1), _atsc_stream_data(0), _buffer(0), _buffer_size(0),
      _error(false), _wait_for_gop(true),
      _request_recording(false), _request_pause(false), _recording(false), _paused(false),
      _tspacket_count(0), _nullpacket_count(0), _resync_count(0),
      _frames_written_count(0), _frames_seen_count(0), _frame_of_first_gop(0),
      _position_within_gop_header(0), _scanning_pes_header_for_gop(0), _gop_seen(false)
{
    _atsc_stream_data = new ATSCStreamData(DEFAULT_PROGRAM);

    //_buffer_size = max((L2_CACHE_SIZE_KB*1024)/2-(16*1024), 8*1024);
    _buffer_size = 2 * 1024 * 1024;
    if ((_buffer = new unsigned char[_buffer_size])) {
        // make valgrind happy, initialize buffer memory
        memset(_buffer, 0xFF, _buffer_size);
    }

    VERBOSE(VB_RECORD, QString("HD buffer size %1 KB").arg(_buffer_size/1024));
}

HDTVRecorder::~HDTVRecorder()
{
    if (_atsc_stream_fd >= 0)
        close(_atsc_stream_fd);
    if (_atsc_stream_data)
        delete _atsc_stream_data;
    if (_buffer)
        delete[] _buffer;
}

void HDTVRecorder::SetOption(const QString &opt, int value)
{
    (void)opt;
    (void)value;
}

void HDTVRecorder::SetOptionsFromProfile(RecordingProfile *profile,
                                         const QString &videodev,
                                         const QString &audiodev,
                                         const QString &vbidev, int ispip)
{
    (void)audiodev;
    (void)vbidev;
    (void)profile;
    (void)ispip;

    SetOption("videodevice", videodev);
    SetOption("tvformat", gContext->GetSetting("TVFormat"));
    SetOption("vbiformat", gContext->GetSetting("VbiFormat"));
}

bool HDTVRecorder::Open()
{
    if (!_atsc_stream_data || !_buffer) 
        return false;

#if FAKE_VIDEO
    // open file instead of device
    if (_atsc_stream_fd >=0)
    {
        int ret = close(_atsc_stream_fd);
        assert(ret);
    }
    _atsc_stream_fd = open(FAKE_VIDEO_FILES[fake_video_index], O_RDWR);
    VERBOSE(VB_IMPORTANT, QString("Opened fake video source %1").arg(FAKE_VIDEO_FILES[fake_video_index]));
    fake_video_index = (fake_video_index+1)%FAKE_VIDEO_NUM;
#else
    if (_atsc_stream_fd <= 0)
        _atsc_stream_fd = open(videodevice.ascii(), O_RDWR);
#endif
    if (_atsc_stream_fd <= 0)
    {
        VERBOSE(VB_IMPORTANT, QString("Can't open video device: %1 chanfd = %2")
                .arg(videodevice).arg(_atsc_stream_fd));
        perror("open video:");
    }
    return (_atsc_stream_fd>0);
}

bool readchan(int chanfd, unsigned char* buffer, int dlen) {
    int len = read(chanfd, buffer, dlen); // read next byte
    if (dlen != len)
    {
        if (len < 0)
        {
            VERBOSE(VB_IMPORTANT, QString("HD1 error reading from device"));
            perror("read");
        }
        else if (len == 0)
            VERBOSE(VB_IMPORTANT, QString("HD2 end of file found in packet"));
        else 
            VERBOSE(VB_IMPORTANT, QString("HD3 partial read. This shouldn't happen!"));
    }
    return (dlen == len);
}

bool syncchan(int chanfd, int dlen, int keepsync) {
    unsigned char b[188];
    int i, j;
    for (i=0; i<dlen; i++) {
        if (!readchan(chanfd, b, 1))
            break;
        if (SYNC_BYTE == b[0])
        {
            if (readchan(chanfd, &b[1], TSPacket::SIZE-1)) {
                i += (TSPacket::SIZE - 1);
                for (j=0; j<keepsync; j++)
                {
                    if (!readchan(chanfd, b, TSPacket::SIZE))
                        return false;
                    i += TSPacket::SIZE;
                    if (SYNC_BYTE != b[0])
                        break;
                }
                if (j==keepsync)
                {
                    VERBOSE(VB_RECORD,
                            QString("HD4 obtained device stream sync after reading %1 bytes").
                            arg(dlen));
                    return true;
                }
                continue;
            }
            break;
        }
    }
    VERBOSE(VB_IMPORTANT, QString("HD5 Error: could not obtain sync"));
    return false;
}

void HDTVRecorder::StartRecording(void)
{
    const int unsyncpackets = 50; // unsynced packets to look at before giving up
    const int syncpackets   = 10; // synced packets to require before starting recording

    VERBOSE(VB_RECORD, QString("StartRecording"));

    if (!Open())
    {
        _error = true;        
        return;
    }

    if (!SetupRecording())
    {
        VERBOSE(VB_IMPORTANT, QString("HD6 Error initializing recording"));
        _error = true;
        return;
    }

    _request_recording = true;
    _recording = true;

    // sync device stream so it starts with a valid ts packet
    if (!syncchan(_atsc_stream_fd, TSPacket::SIZE*unsyncpackets, syncpackets))
    {
        _error = true;
        return;
    }
    int remainder = 0;
    // TRANSFER DATA
    while (_request_recording) 
    {
        if (_request_pause)
        {
            _paused = true;
            pauseWait.wakeAll();

            usleep(50);
            continue;
        }

        int len = read(_atsc_stream_fd, &(_buffer[remainder]),
                       _buffer_size - remainder);

        if (len < 0)
        {
            VERBOSE(VB_IMPORTANT, QString("HD7 error reading from: %1").
                    arg(videodevice));
            perror("read");
            continue;
        }
        else if (len > 0)
        {
            len += remainder;
            remainder = ProcessData(_buffer, len);
            if (remainder > 0) // leftover bytes
                memmove(&(_buffer[0]), &(_buffer[_buffer_size - remainder]), remainder);
        }
        else
        {
            VERBOSE(VB_IMPORTANT, QString("HD8 end of file found in packet"));
            break;
        }
    }

    FinishRecording();

    _recording = false;
}

int HDTVRecorder::GetVideoFd(void)
{
    return _atsc_stream_fd;
}

// start common code to the dvbrecorder class.

bool HDTVRecorder::SetupRecording(void)
{
    return true;
}

void HDTVRecorder::FinishRecording(void)
{
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

void HDTVRecorder::SetVideoFilters(QString &filters)
{
    (void)filters;
}

void HDTVRecorder::Initialize(void)
{
    
}

void HDTVRecorder::HandleGOP() {
    // group_of_pictures
    int frameNum = _frames_written_count - 1;

    if (!_gop_seen && _frames_seen_count > 0)
    {
        if (_frame_of_first_gop > 0)
        {
            // don't set keyframedist...
            // playback assumes it's 1 in 15
            // frames, and in ATSC they come
            // somewhat unevenly.
            //keyframedist=frameNum-firstgoppos;
            _gop_seen = true;
        }
        else
            _frame_of_first_gop = (frameNum > 0) ? frameNum : 1;
    }

    long long startpos = 
        ringBuffer->GetFileWritePosition();

    if (!_position_map.contains(frameNum))
    {
        _position_map_delta[frameNum] = startpos;
        _position_map[frameNum] = startpos;

        if (curRecording && db_lock && db_conn &&
            ((_position_map.size() % 30) == 0))
        {
            pthread_mutex_lock(db_lock);
            MythContext::KickDatabase(db_conn);
            curRecording->SetPositionMap(_position_map, MARK_GOP_BYFRAME, db_conn);
            curRecording->SetFilesize(startpos, db_conn);
            pthread_mutex_unlock(db_lock);
            _position_map_delta.clear();
        }
    }
}

void HDTVRecorder::FindKeyframes(const TSPacket* tspacket)
{
    if (!tspacket->HasPayload())
        return; // no payload to scan...
  
    if (tspacket->PayloadStart())
    { // packet contains start of PES packet
        _scanning_pes_header_for_gop = true; // start scanning for PES headers
        _position_within_gop_header = 0; // start looking for first byte of pattern
    }
    else if (!_scanning_pes_header_for_gop)
        return;

    // Scan for PES header codes; specifically picture_start
    // and group_start (of_pictures).  These should be within
    // this first TS packet of the PES packet.
    //   00 00 01 00: picture_start_code
    //   00 00 01 B8: group_start_code
    //   (there are others that we don't care about)
    unsigned int i=tspacket->AFCOffset(); // borked if AFCOffset > 188
    const unsigned char *buffer=tspacket->data();
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
            if (0x00 == k1) // picture start
            {
                if (_gop_seen || _frame_of_first_gop > 0)
                    _frames_written_count++;
                _frames_seen_count++;
            }
            else if (0xB8 == k1)
                HandleGOP();
            else if (0xB3 == k1 && !_gop_seen)
                HandleGOP();
            
            // video slice ... end of "interesting" headers
            if (0x01 <= k1 && k1 <= 0xAF)
                _scanning_pes_header_for_gop = false;
            _position_within_gop_header = 0;
        }
    }
}

int HDTVRecorder::ResyncStream(unsigned char *buffer, int curr_pos, int len)
{
    // Search for two sync bytes 188 bytes apart, 
    int pos = curr_pos;
    int nextpos = pos + TSPacket::SIZE;
    if (nextpos >= len)
        return -1; // not enough bytes; caller should try again
    
    while (buffer[pos] != SYNC_BYTE || buffer[nextpos] != SYNC_BYTE) {
        pos++;
        nextpos++;
        if (nextpos == len)
            return -2; // not found
    }

    return pos;
}

void HDTVRecorder::WritePAT() {
    if (StreamData()->PAT()) {
        ProgramAssociationTable* pat = StreamData()->PAT();
        int next = (pat->tsheader()->ContinuityCounter()+1)&0xf;
        pat->tsheader()->SetContinuityCounter(next);
        ringBuffer->Write(pat->tsheader()->data(), TSPacket::SIZE);
    }
}

#if WHACK_A_BUG_VIDEO
static int WABV_base_pid     = 0x100;
#define WABV_WAIT 60
static int WABV_wait_a_while = WABV_WAIT;
bool WABV_started = false;
#endif

#if WHACK_A_BUG_AUDIO
static int WABA_base_pid     = 0x200;
#define WABA_WAIT 60
static int WABA_wait_a_while = WABA_WAIT;
bool WABA_started = false;
#endif

void HDTVRecorder::WritePMT() {
    if (StreamData()->PMT()) {
        ProgramMapTable* pmt = StreamData()->PMT();
        int next = (pmt->tsheader()->ContinuityCounter()+1)&0xf;
        pmt->tsheader()->SetContinuityCounter(next);

#if WHACK_A_BUG_VIDEO
        WABV_wait_a_while--;
        if (WABV_wait_a_while<=0) {
            WABV_started = true;
            WABV_wait_a_while = WABV_WAIT;
            WABV_base_pid = (((WABV_base_pid-0x100)+1)%32)+0x100;
            VERBOSE(VB_IMPORTANT, QString("Whack a Bug: new video pid %1").arg(WABV_base_pid));
            // rewrite video pid
            assert(StreamID::MPEG2Video == StreamData()->PMT()->StreamType(0));
            const uint old_video_pid=StreamData()->PMT()->StreamPID(0);
            StreamData()->PMT()->SetStreamPID(0, WABV_base_pid);
            if (StreamData()->PMT()->PCRPID() == old_video_pid)
                StreamData()->PMT()->SetPCRPID(WABV_base_pid);
            StreamData()->PMT()->SetCRC(StreamData()->PMT()->CalcCRC());
            VERBOSE(VB_IMPORTANT, StreamData()->PMT()->toString());
        }
#endif
#if WHACK_A_BUG_AUDIO
        WABA_wait_a_while--;
        if (WABA_wait_a_while<=0) {
            WABA_started = true;
            WABA_wait_a_while = WABA_WAIT;
            WABA_base_pid = (((WABA_base_pid-0x200)+1)%32)+0x200;
            VERBOSE(VB_IMPORTANT, QString("Whack a Bug: new audio BASE pid %1").arg(WABA_base_pid));
            // rewrite audio pids
            for (uint i=0; i<StreamData()->PMT()->StreamCount(); i++) {
                if (StreamID::MPEG2Audio == StreamData()->PMT()->StreamType(i) ||
                    StreamID::MPEG2Audio == StreamData()->PMT()->StreamType(i)) {
                    const uint old_audio_pid = StreamData()->PMT()->StreamPID(i);
                    const uint new_audio_pid = WABA_base_pid + old_audio_pid;
                    StreamData()->PMT()->SetStreamPID(i, new_audio_pid);
                    if (StreamData()->PMT()->PCRPID() == old_audio_pid)
                        StreamData()->PMT()->SetPCRPID(new_audio_pid);
                    StreamData()->PMT()->SetCRC(StreamData()->PMT()->CalcCRC());
                    VERBOSE(VB_IMPORTANT, StreamData()->PMT()->toString());
                }
            }
        }
#endif

        ringBuffer->Write(pmt->tsheader()->data(), TSPacket::SIZE);
    }
}

void HDTVRecorder::HandleVideo(const TSPacket* tspacket)
{
    FindKeyframes(tspacket);
    // decoder needs video, of course (just this PID)
    // delay until first GOP to avoid decoder crash on res change
    if (_wait_for_gop && !_gop_seen && !_frame_of_first_gop)
        return;

#if WHACK_A_BUG_VIDEO
    if (WABV_started)
        ((TSPacket*)(tspacket))->SetPID(WABV_base_pid);
#endif

    ringBuffer->Write(tspacket->data(), TSPacket::SIZE);
}

void HDTVRecorder::HandleAudio(const TSPacket* tspacket)
{
    // decoder needs audio, of course (just this PID)
    if (_wait_for_gop && !_gop_seen && !_frame_of_first_gop)
        return;

#if WHACK_A_BUG_AUDIO
    if (WABA_started)
        ((TSPacket*)(tspacket))->SetPID(WABA_base_pid+tspacket->PID());
#endif

    ringBuffer->Write(tspacket->data(), TSPacket::SIZE);
}

bool HDTVRecorder::ProcessTSPacket(const TSPacket& tspacket)
{
    bool ok = !tspacket.TransportError();
    if (ok && !tspacket.ScramplingControl())
    {
        if (tspacket.HasAdaptationField())
            StreamData()->HandleAdaptationFieldControl(&tspacket);
        if (tspacket.HasPayload())
        {
            const unsigned int lpid = tspacket.PID();
            // Pass or reject frames based on PID, and parse info from them
            if (lpid == StreamData()->VideoPID())
                HandleVideo(&tspacket);
            else if (lpid == 0x1fff) // null packet
                _nullpacket_count++;
            else if (StreamData()->IsAudioPID(lpid))
                HandleAudio(&tspacket);
            else if (StreamData()->IsListeningPID(lpid))
                StreamData()->HandleTables(&tspacket, this);
            else if (StreamData()->VersionMGT()>=0 && lpid>0x60)
                VERBOSE(VB_RECORD, QString("Unknown pid 0x%1").arg(lpid, 0, 16));
        }
    }
    return ok;
}

int HDTVRecorder::ProcessData(unsigned char *buffer, int len)
{
    int pos = 0;

    while (pos + 187 < len) // while we have a whole packet left
    {
        if (buffer[pos] != SYNC_BYTE)
        {
            _resync_count++;
            if (25 == _resync_count) 
                VERBOSE(VB_RECORD, QString("Resyncing many of times, suppressing error messages"));
            else if (25 > _resync_count)
                VERBOSE(VB_RECORD, QString("Resyncing"));
            int newpos = ResyncStream(buffer, pos, len);
            if (newpos == -1)
                return len - pos;
            if (newpos == -2)
                return TSPacket::SIZE;

            pos = newpos;
        }

        const TSPacket *pkt = reinterpret_cast<const TSPacket*>(&buffer[pos]);
        if (ProcessTSPacket(*pkt)) {
            pos += TSPacket::SIZE; // Advance to next TS packet
            _tspacket_count++;
        } else // Let it resync in case of dropped bytes
            buffer[pos] = SYNC_BYTE+1;
    }

    return len - pos;
}

void HDTVRecorder::StopRecording(void)
{
    _request_recording = false;
}

void HDTVRecorder::Reset(void)
{
    _error = false;

    _tspacket_count = 0;
    _nullpacket_count = 0;
    _resync_count = 0;
    _frames_written_count = 0;
    _frames_seen_count = 0;

    _frame_of_first_gop = 0;
    _position_within_gop_header = 0;
    _scanning_pes_header_for_gop = false;
    _gop_seen = false;

    _position_map.clear();
    _position_map_delta.clear();

    if (curRecording && db_lock && db_conn)
    {
        pthread_mutex_lock(db_lock);
        MythContext::KickDatabase(db_conn);
        curRecording->ClearPositionMap(MARK_GOP_BYFRAME, db_conn);
        pthread_mutex_unlock(db_lock);
    }

    if (_atsc_stream_fd >= 0) 
    {
        int ret = close(_atsc_stream_fd);
        if (ret < 0) 
        {
            perror("close");
            return;
        }
#if FAKE_VIDEO
        // open file instead of device
        _atsc_stream_fd = open(FAKE_VIDEO_FILES[fake_video_index], O_RDWR);
        VERBOSE(VB_IMPORTANT, QString("Opened fake video source %1").arg(FAKE_VIDEO_FILES[fake_video_index]));
        fake_video_index = (fake_video_index+1)%FAKE_VIDEO_NUM;
#else
        _atsc_stream_fd = open(videodevice.ascii(), O_RDWR);
#endif
        if (_atsc_stream_fd < 0)
        {
            VERBOSE(VB_IMPORTANT, QString("HD1 Can't open video device: %1 chanfd = %2").
                    arg(videodevice).arg(_atsc_stream_fd));
            perror("open video");
            return;
        }
    }

    StreamData()->Reset();
}

void HDTVRecorder::Pause(bool /*clear*/)
{
    _paused = false;
    _request_pause = true;
}

void HDTVRecorder::Unpause(void)
{
    _request_pause = false;
}

bool HDTVRecorder::GetPause(void)
{
    return _paused;
}

void HDTVRecorder::WaitForPause(void)
{
    if (!_paused)
        if (!pauseWait.wait(1000))
            VERBOSE(VB_IMPORTANT, QString("Waited too long for recorder to pause"));
}

bool HDTVRecorder::IsRecording(void)
{
    return _recording;
}

long long HDTVRecorder::GetFramesWritten(void)
{
    return _frames_written_count;
}

long long HDTVRecorder::GetKeyframePosition(long long desired)
{
    long long ret = -1;

    if (_position_map.find(desired) != _position_map.end())
        ret = _position_map[desired];

    return ret;
}

void HDTVRecorder::GetBlankFrameMap(QMap<long long, int> &blank_frame_map)
{
    (void)blank_frame_map;
}

void HDTVRecorder::ChannelNameChanged(const QString& new_chan)
{
    if (!_atsc_stream_data && !_buffer) 
        return;
    RecorderBase::ChannelNameChanged(new_chan);
    // look up freqid
    pthread_mutex_lock(db_lock);
    QString thequery = QString("SELECT freqid "
                               "FROM channel WHERE channum = \"%1\";")
        .arg(curChannelName);
    QSqlQuery query = db_conn->exec(thequery);
    if (!query.isActive())
        MythContext::DBError("fetchtuningparamschanid", query);
    if (query.numRowsAffected() <= 0)
    {
        pthread_mutex_unlock(db_lock);
        return;
    }
    query.next();
    QString freqid(query.value(0).toString());
    VERBOSE(VB_RECORD, QString("Setting frequency for startRecording freqid: %1").arg(freqid));
    pthread_mutex_unlock(db_lock);

    int desired_program = DEFAULT_PROGRAM;
    int pos = freqid.find('-');
    if (pos != -1) 
        desired_program = atoi(freqid.mid(pos+1).ascii());
    else
        VERBOSE(VB_IMPORTANT,
                QString("Error: Desired program not specified in freqid \"%1\", Using %2.").arg(freqid).arg(desired_program));
    StreamData()->Reset(desired_program);
}
