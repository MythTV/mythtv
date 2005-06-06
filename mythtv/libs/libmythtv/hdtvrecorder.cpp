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
#include <cerrno>
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
#include "mythdbcon.h"
#include "programinfo.h"
#include "channel.h"
#include "mpegtables.h"
#include "atsctables.h"
#include "atscstreamdata.h"

extern "C" {
#include "../libavcodec/avcodec.h"
#include "../libavformat/avformat.h"
#include "../libavformat/mpegts.h"
}

#define REPORT_RING_STATS 1

#define DEFAULT_SUBCHANNEL 1

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
    : DTVRecorder(), _atsc_stream_data(0), _resync_count(0)
{
    _atsc_stream_data = new ATSCStreamData(-1, DEFAULT_SUBCHANNEL);
    connect(_atsc_stream_data, SIGNAL(UpdatePATSingleProgram(
                                          ProgramAssociationTable*)),
            this, SLOT(WritePAT(ProgramAssociationTable*)));
    connect(_atsc_stream_data, SIGNAL(UpdatePMTSingleProgram(ProgramMapTable*)),
            this, SLOT(WritePMT(ProgramMapTable*)));
    connect(_atsc_stream_data, SIGNAL(UpdateMGT(const MasterGuideTable*)),
            this, SLOT(ProcessMGT(const MasterGuideTable*)));
    connect(_atsc_stream_data,
            SIGNAL(UpdateVCT(uint, const VirtualChannelTable*)),
            this, SLOT(ProcessVCT(uint, const VirtualChannelTable*)));

    _buffer_size = TSPacket::SIZE * 1500;
    if ((_buffer = new unsigned char[_buffer_size])) {
        // make valgrind happy, initialize buffer memory
        memset(_buffer, 0xFF, _buffer_size);
    }

    VERBOSE(VB_RECORD, QString("HD buffer size %1 KB").arg(_buffer_size/1024));

    ringbuf.run = false;
    ringbuf.buffer = 0;
    pthread_mutex_init(&ringbuf.lock, NULL);
    pthread_mutex_init(&ringbuf.lock_stats, NULL);
    loop = random() % (report_loops / 2);
}

HDTVRecorder::~HDTVRecorder()
{
    if (_stream_fd >= 0)
        close(_stream_fd);
    if (_atsc_stream_data)
        delete _atsc_stream_data;
    if (_buffer)
        delete[] _buffer;

    // Make SURE that the ringbuffer thread is cleaned up
    StopRecording();

    pthread_mutex_destroy(&ringbuf.lock);
    pthread_mutex_destroy(&ringbuf.lock_stats);
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
    if (_stream_fd >=0 && close(_stream_fd))
    {
        VERBOSE(VB_IMPORTANT,
                QString("HDTVRecorder::Open(): Error, failed to close "
                        "existing fd (%1)").arg(strerror(errno)));
        return false;
    }

    _stream_fd = open(FAKE_VIDEO_FILES[fake_video_index], O_RDWR);
    VERBOSE(VB_IMPORTANT, QString("Opened fake video source %1").arg(FAKE_VIDEO_FILES[fake_video_index]));
    fake_video_index = (fake_video_index+1)%FAKE_VIDEO_NUM;
#else
    if (_stream_fd <= 0)
        _stream_fd = open(videodevice.ascii(), O_RDWR);
#endif
    if (_stream_fd <= 0)
    {
        VERBOSE(VB_IMPORTANT, QString("Can't open video device: %1 chanfd = %2")
                .arg(videodevice).arg(_stream_fd));
        perror("open video:");
    }
    return (_stream_fd>0);
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

void * HDTVRecorder::boot_ringbuffer(void * arg)
{
    HDTVRecorder *dtv = (HDTVRecorder *)arg;
    dtv->fill_ringbuffer();
    return NULL;
}

void HDTVRecorder::fill_ringbuffer(void)
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
            if (++errcnt > 5)
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
            if (++errcnt > 5)
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

    close(_stream_fd);
    _stream_fd = -1;

    pthread_exit(reinterpret_cast<void *>(0));
}

/* read count bytes from ring into buffer */
int HDTVRecorder::ringbuf_read(unsigned char *buffer, size_t count)
{
    size_t          avail;
    size_t          cnt = count;
    size_t          min_read;
    unsigned char  *cPtr = buffer;

    bool            dev_error = false;
    bool            dev_eof = false;

    pthread_mutex_lock(&ringbuf.lock);
    avail = ringbuf.used;
    pthread_mutex_unlock(&ringbuf.lock);

    min_read = cnt < ringbuf.min_read ? cnt : ringbuf.min_read;

    while (min_read > avail)
    {
        usleep(50000);

        if (_request_pause || dev_error || dev_eof)
            return 0;

        pthread_mutex_lock(&ringbuf.lock);
        dev_error = ringbuf.error;
        dev_eof = ringbuf.eof;
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

void HDTVRecorder::StartRecording(void)
{
    bool            pause;
    bool            dev_error, dev_eof;
    int             len;

    const int unsyncpackets = 50; // unsynced packets to look at before giving up
    const int syncpackets   = 10; // synced packets to require before starting recording

    VERBOSE(VB_RECORD, QString("StartRecording"));

    if (!Open())
    {
        _error = true;        
        return;
    }

    _request_recording = true;
    _recording = true;

    // Setup device ringbuffer
    delete[] ringbuf.buffer;

//    ringbuf.size = 60 * 1024 * TSPacket::SIZE;
    ringbuf.size = gContext->GetNumSetting("HDRingbufferSize", 50*188);
    ringbuf.size *= 1024;

    if ((ringbuf.buffer =
         new unsigned char[ringbuf.size + TSPacket::SIZE]) == NULL)
    {
        VERBOSE(VB_IMPORTANT, "Failed to allocate HDTVRecorder ring buffer.");
        _error = true;
        return;
    }

    memset(ringbuf.buffer, 0xFF, ringbuf.size + TSPacket::SIZE);
    ringbuf.endPtr = ringbuf.buffer + ringbuf.size;
    ringbuf.readPtr = ringbuf.writePtr = ringbuf.buffer;
    ringbuf.dev_read_size = TSPacket::SIZE * 48;
    ringbuf.min_read = TSPacket::SIZE * 4;
    ringbuf.used = 0;
    ringbuf.max_used = 0;
    ringbuf.avg_used = 0;
    ringbuf.avg_cnt = 0;
    ringbuf.request_pause = false;
    ringbuf.paused = false;
    ringbuf.run = true;
    ringbuf.error = false;
    ringbuf.eof = false;

    VERBOSE(VB_RECORD, QString("HD ring buffer size %1 KB")
            .arg(ringbuf.size/1024));

    // sync device stream so it starts with a valid ts packet
    if (!syncchan(_stream_fd, TSPacket::SIZE*unsyncpackets, syncpackets))
    {
        _error = true;
        return;
    }

    // create thread to fill the ringbuffer
    pthread_create(&ringbuf.thread, NULL, boot_ringbuffer,
                   reinterpret_cast<void *>(this));

    int remainder = 0;
    // TRANSFER DATA
    while (_request_recording) 
    {
        pthread_mutex_lock(&ringbuf.lock);
        dev_error = ringbuf.error;
        dev_eof = ringbuf.eof;
        pause = ringbuf.paused;
        pthread_mutex_unlock(&ringbuf.lock);

        if (_request_pause)
        {
            pthread_mutex_lock(&ringbuf.lock);
            ringbuf.request_pause = true;
            pthread_mutex_unlock(&ringbuf.lock);

            usleep(1000);
            continue;
        }
        else if (pause)
        {
            pthread_mutex_lock(&ringbuf.lock);
            ringbuf.request_pause = false;
            pthread_mutex_unlock(&ringbuf.lock);

            usleep(1500);
            continue;
        }

        if (dev_error)
        {
            _error = true;
            break;
        }

        if (dev_eof)
            break;

        len = ringbuf_read(&(_buffer[remainder]), _buffer_size - remainder);

        if (len == 0)
            continue;

        len += remainder;
        remainder = ProcessData(_buffer, len);
        if (remainder > 0) // leftover bytes
            memmove(_buffer, &(_buffer[_buffer_size - remainder]),
                    remainder);
    }

    FinishRecording();
    _recording = false;
}

void HDTVRecorder::StopRecording(void)
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

void HDTVRecorder::Pause(bool /*clear*/)
{
    pthread_mutex_lock(&ringbuf.lock);
    ringbuf.paused = false;
    pthread_mutex_unlock(&ringbuf.lock);
    _request_pause = true;
}

bool HDTVRecorder::GetPause(void)
{
    bool paused;

    pthread_mutex_lock(&ringbuf.lock);
    paused = ringbuf.paused;
    pthread_mutex_unlock(&ringbuf.lock);

    return paused;
}

void HDTVRecorder::WaitForPause(void)
{
    if (!GetPause())
        if (!pauseWait.wait(1000))
            VERBOSE(VB_IMPORTANT,
                    QString("Waited too long for recorder to pause"));
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

void HDTVRecorder::WritePAT(ProgramAssociationTable *pat)
{
    int next = (pat->tsheader()->ContinuityCounter()+1)&0xf;
    pat->tsheader()->SetContinuityCounter(next);
    ringBuffer->Write(pat->tsheader()->data(), TSPacket::SIZE);
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

void HDTVRecorder::WritePMT(ProgramMapTable* pmt)
{
    if (pmt) {
        int next = (pmt->tsheader()->ContinuityCounter()+1)&0xf;
        pmt->tsheader()->SetContinuityCounter(next);

#if WHACK_A_BUG_VIDEO
        WABV_wait_a_while--;
        if (WABV_wait_a_while<=0) {
            WABV_started = true;
            WABV_wait_a_while = WABV_WAIT;
            WABV_base_pid = (((WABV_base_pid-0x100)+1)%32)+0x100;
            if (StreamID::MPEG2Video != StreamData()->PMT()->StreamType(0))
            {
                VERBOSE(VB_IMPORTANT, "HDTVRecorder::WritePMT(): Error,"
                        "Whack a Bug can not rewrite PMT, wrong stream type");
            }
            else
            {
                VERBOSE(VB_IMPORTANT, QString("Whack a Bug: new video pid %1").
                        arg(WABV_base_pid));
                // rewrite video pid
                const uint old_video_pid=StreamData()->PMT()->StreamPID(0);
                StreamData()->PMT()->SetStreamPID(0, WABV_base_pid);
                if (StreamData()->PMT()->PCRPID() == old_video_pid)
                    StreamData()->PMT()->SetPCRPID(WABV_base_pid);
                StreamData()->PMT()->SetCRC(StreamData()->PMT()->CalcCRC());
                VERBOSE(VB_IMPORTANT, StreamData()->PMT()->toString());
            }
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

/** \fn HDTVRecorder::ProcessMGT(const MasterGuideTable*)
 *  \brief Processes Master Guide Table, by enabling the 
 *         scanning of all PIDs listed.
 */
void HDTVRecorder::ProcessMGT(const MasterGuideTable *mgt)
{
    for (unsigned int i=0; i<mgt->TableCount(); i++)
        StreamData()->AddListeningPID(mgt->TablePID(i));
}

/** \fn HDTVRecorder::ProcessVCT(uint, const VirtualChannelTable*)
 *  \brief Processes Virtual Channel Tables by finding the program
 *         number to use.
 *  \bug Assumes there is only one VCT, may break on Cable.
 */
void HDTVRecorder::ProcessVCT(uint /*tsid*/, const VirtualChannelTable *vct)
{
    if (vct->ChannelCount() < 1)
    {
        VERBOSE(VB_IMPORTANT,
                "HDTVRecorder::ProcessVCT: table has no channels");
        return;
    }

    bool found = false;    
    for (uint i=0; i<vct->ChannelCount(); i++)
    {
        if ((StreamData()->DesiredMajorChannel() == -1 ||
             vct->MajorChannel(i)==(uint)StreamData()->DesiredMajorChannel()) &&
            vct->MinorChannel(i)==(uint)StreamData()->DesiredMinorChannel())
        {
            VERBOSE(VB_RECORD, QString("***Desired subchannel %1")
                    .arg(StreamData()->DesiredMinorChannel()));
            if (vct->ProgramNumber(i) != (uint)StreamData()->DesiredProgram())
            {
                VERBOSE(VB_RECORD, 
                        QString("Resetting desired program from %1"
                                " to %2")
                        .arg(StreamData()->DesiredProgram())
                        .arg(vct->ProgramNumber(i)));
                // Do a (partial?) reset here if old desired
                // program is not 0?
                StreamData()->SetDesiredProgram(vct->ProgramNumber(i));
                found = true;
            }
        }
    }
    if (!found)
    {
        VERBOSE(VB_IMPORTANT, 
                QString("Desired subchannel %1 not found;"
                        " using %2 instead")
                .arg(StreamData()->DesiredMinorChannel())
                .arg(vct->MinorChannel(0)));
        StreamData()->SetDesiredProgram(vct->ProgramNumber(0));
    }
}

void HDTVRecorder::HandleVideo(const TSPacket* tspacket)
{
    FindKeyframes(tspacket);
    // decoder needs video, of course (just this PID)
    // delay until first GOP to avoid decoder crash on res change
    if (_wait_for_keyframe && !_keyframe_seen)
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
    if (_wait_for_keyframe && !_keyframe_seen)
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
            if (lpid == StreamData()->VideoPIDSingleProgram())
                HandleVideo(&tspacket);
            else if (StreamData()->IsAudioPID(lpid))
                HandleAudio(&tspacket);
            else if (StreamData()->IsListeningPID(lpid))
                StreamData()->HandleTables(&tspacket);
            else if (StreamData()->IsWritingPID(lpid))
                ringBuffer->Write(tspacket.data(), TSPacket::SIZE);
            else if (StreamData()->VersionMGT()>=0)
                _ts_stats.IncrPIDCount(lpid);
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
            _ts_stats.IncrTSPacketCount();
            if (0 == _ts_stats.TSPacketCount()%1000000)
                VERBOSE(VB_RECORD, _ts_stats.toString());
        } else // Let it resync in case of dropped bytes
            buffer[pos] = SYNC_BYTE+1;
    }

    return len - pos;
}

void HDTVRecorder::Reset(void)
{
    DTVRecorder::Reset();

    _resync_count = 0;
    _ts_stats.Reset();

    if (curRecording)
    {
        curRecording->ClearPositionMap(MARK_GOP_BYFRAME);
    }

    if (_stream_fd >= 0) 
    {
        Pause(true);
        WaitForPause();
        int ret = close(_stream_fd);
        if (ret < 0) 
        {
            perror("close");
            return;
        }
#if FAKE_VIDEO
        // open file instead of device
        _stream_fd = open(FAKE_VIDEO_FILES[fake_video_index], O_RDWR);
        VERBOSE(VB_IMPORTANT, QString("Opened fake video source %1").arg(FAKE_VIDEO_FILES[fake_video_index]));
        fake_video_index = (fake_video_index+1)%FAKE_VIDEO_NUM;
#else
        _stream_fd = open(videodevice.ascii(), O_RDWR);
#endif
        if (_stream_fd < 0)
        {
            VERBOSE(VB_IMPORTANT, QString("HD1 Can't open video device: %1 chanfd = %2").
                    arg(videodevice).arg(_stream_fd));
            perror("open video");
            return;
        }
        else
        {
            pthread_mutex_lock(&ringbuf.lock);
            ringbuf.used = 0;
            ringbuf.max_used = 0;
            ringbuf.readPtr = ringbuf.writePtr = ringbuf.buffer;
            pthread_mutex_unlock(&ringbuf.lock);
        }
        Unpause();
    }

    StreamData()->Reset();
}

void HDTVRecorder::ChannelNameChanged(const QString& new_chan)
{
    if (!_atsc_stream_data && !_buffer) 
        return;

    _wait_for_keyframe = _wait_for_keyframe_option;

#if FAKE_VIDEO
    RecorderBase::ChannelNameChanged("51");
#else
    DTVRecorder::ChannelNameChanged(new_chan);
#endif

    // look up freqid
 
    MSqlQuery query(MSqlQuery::InitCon());
    QString thequery = QString("SELECT freqid "
                               "FROM channel WHERE channum = \"%1\";")
                              .arg(curChannelName);
    query.prepare(thequery);

    if (!query.exec() || !query.isActive())
    {
        MythContext::DBError("fetchtuningparamschanid", query);
    }

    if (query.size() <= 0)
    {
        return;
    }
    query.next();
    QString freqid(query.value(0).toString());
    VERBOSE(VB_RECORD, QString("Setting frequency for startRecording freqid: %1").arg(freqid));

    int desired_channel = -1;
    int desired_subchannel = DEFAULT_SUBCHANNEL;
    int pos = freqid.find('-');
    if (pos != -1) 
    {
        desired_channel = -1;
        desired_subchannel = atoi(freqid.mid(pos+1).ascii());
    }
    else
    {
        VERBOSE(VB_IMPORTANT, QString("Error: Desired subchannel not "
                                      "specified in freqid \"%1\", Using %2.")
                                    .arg(freqid).arg(desired_subchannel));
    }

    StreamData()->Reset(desired_channel, desired_subchannel);
}
