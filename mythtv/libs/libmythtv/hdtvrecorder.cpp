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

   References / example code: 
     ATSC standards a.54, a.69 (www.atsc.org)
     ts2pes from mpegutils from dvb (www.linuxtv.org)
     bbinfo from Brent Beyeler, beyeler@home.com
*/

#include <iostream>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <time.h>
#include "videodev_myth.h"

using namespace std;

#include "hdtvrecorder.h"
#include "RingBuffer.h"
#include "mythcontext.h"
#include "programinfo.h"

extern "C" {
#include "../libavcodec/avcodec.h"
#include "../libavformat/avformat.h"
}

//#define BUFFER_SIZE 4096
//#define BUFFER_SIZE 256001
#define BUFFER_SIZE 255868
#define PACKETS (BUFFER_SIZE / 188)

#define SYNC_BYTE 0x47

// n.b. these PID relationships are only a recommendation from ATSC,
// but seem to be universal
#define VIDEO_PID(bp) ((bp)+1)
#define AUDIO_PID(bp) ((bp)+4)

HDTVRecorder::HDTVRecorder()
            : RecorderBase()
{
    paused = false;
    mainpaused = false;
    recording = false;

    framesWritten = 0;

    chanfd = -1; 

    keyframedist = 15;
    gopset = false;

    firstgoppos = 0;

    pat_pid = 0;
    psip_pid = 0x1ffb;
    base_pid = 0; // will be filled in when we find it
    first_base_pid = 0;
    ts_packets = 0; // cumulative packet count

    // temporary to find lowest PID in first 500 packets
    lowest_video_pid = 0x1fff;
    video_pid_packets = 0;
}

HDTVRecorder::~HDTVRecorder()
{
    if (chanfd > 0)
        close(chanfd);
}

void HDTVRecorder::SetOption(const QString &opt, int value)
{
    (void)opt;
    (void)value;
}

void HDTVRecorder::StartRecording(void)
{
    uint8_t * buf;
    uint8_t * end;
    unsigned int errors=0;
    int len;
    unsigned char data_byte[4];
    int insync = 0;
    int ret;

    chanfd = open(videodevice.ascii(), O_RDWR);
    if (chanfd <= 0)
    {
        cerr << "HD1 Can't open video device: " << videodevice 
             << " chanfd = "<< chanfd << endl;
        perror("open video:");
        return;
    }

    static uint8_t buffer[BUFFER_SIZE];

    if (!SetupRecording())
    {
        cerr << "HD Error initializing recording\n";
        return;
    }

    encoding = true;
    recording = true;

    while (encoding) 
    {
        insync = 0;
        len = read(chanfd, &data_byte[0], 1); // read next byte
        if (len == 0) 
            return;  // end of file

        if (data_byte[0] == SYNC_BYTE)
        {
            read(chanfd, buffer, 187); //read next 187 bytes-remainder of packet
            // process next packet
            while (encoding) 
            {
                len = read(chanfd, &data_byte[0], 4); // read next 4 bytes

                if (len != 4) 
                   return;

                len = read(chanfd, buffer+4, 184);
                
                if (len != 184) 
                {
                    cerr <<"HD End of file found in packet "<<len<<endl;
                    return;
                }

                buf = buffer;
                end = buf + 188;

                buf[0] = data_byte[0];
                buf[1] = data_byte[1];
                buf[2] = data_byte[2];
                buf[3] = data_byte[3];

                if (buf[0] != SYNC_BYTE) 
                {
                    VERBOSE(VB_RECORD, "Bad SYNC byte!!!");
                    errors++;
                    insync = 0;
                    break;
                } 
                else 
                {
                    insync++;
                    if (insync == 10) 
                    {
                        int remainder = 0;
                        // TRANSFER DATA
                        while (encoding) 
                        {
                            if (paused)
                            {
                                mainpaused = true;
                                pauseWait.wakeAll();

                                usleep(50);
                                continue;
                            }

                            ret = read(chanfd, &(buffer[remainder]), 
                                       188*PACKETS - remainder);

                            if (ret < 0)
                            {
                                cerr << "HD error reading from: "
                                     << videodevice << endl;
                                perror("read");
                                continue;
                            }
                            else if (ret > 0)
                            {
                                ret += remainder;
                                remainder = ProcessData(buffer, ret);
                                if (remainder > 0) // leftover bytes
                                    memmove(&(buffer[0]), 
                                            &(buffer[188*PACKETS-remainder]),
                                            remainder);
                            }

                        }
                    }
                }
            } // while(2)
        }
    } // while(1)

    FinishRecording();

    recording = false;
}

int HDTVRecorder::GetVideoFd(void)
{
    return chanfd;
}

// start common code to the dvbrecorder class.

static void mpg_write_packet(void *opaque, uint8_t *buf, int buf_size)
{
    (void)opaque;
    (void)buf;
    (void)buf_size;
}

static int mpg_read_packet(void *opaque, uint8_t *buf, int buf_size)
{
    (void)opaque;
    (void)buf;
    (void)buf_size;
    return 0;
}

static int mpg_seek_packet(void *opaque, int64_t offset, int whence)
{
    (void)opaque;
    (void)offset;
    (void)whence;
    return 0;
}

bool HDTVRecorder::SetupRecording(void)
{
    prev_gop_save_pos = -1;

    return true;
}

void HDTVRecorder::FinishRecording(void)
{
    if (curRecording && db_lock && db_conn)
    {
        pthread_mutex_lock(db_lock);
        MythContext::KickDatabase(db_conn);
        curRecording->SetPositionMap(positionMap, MARK_GOP_START, db_conn);
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

static unsigned int get1bit(unsigned char byte, int bit)
{
    bit = 7-bit;
    return (byte & (1 << bit)) >> bit;
}

void HDTVRecorder::FindKeyframes(const unsigned char *buffer, 
                                 int packet_start_pos,
                                 int current_pos,
                                 char adaptation_field_control,
                                 bool payload_unit_start_indicator)
{
    int pos = current_pos;
    int packet_end_pos = packet_start_pos + 188;

    int pkt_start = pos;
    int pkt_pointer = 0;
    if (adaptation_field_control == 3 && payload_unit_start_indicator) 
    {
        pkt_pointer = buffer[pos++];
        if (pkt_pointer < 184)
            pkt_start += pkt_pointer;
    }
    if (adaptation_field_control == 1 || adaptation_field_control == 3)
    {
        // we have payload
        int payload_size = 0;
        if (payload_unit_start_indicator)
        {
            // packet contains start of PES packet

            // Scan for PES header codes; specifically picture_start
            // and group_start (of_pictures).  These should be within 
            // this first TS packet of the PES packet.
            //   00 00 01 00: picture_start_code
            //   00 00 01 B8: group_start_code
            //   (there are others that we don't care about)
            int sync = 0;
            payload_size = packet_end_pos - pkt_start;
            for (int i=pos; i < payload_size+pos; i++)
            {
                char k = buffer[i];
                switch (sync)
                {
                case 0:
                    if (k == 0x00)
                        sync = 1;
                    break;
                case 1:
                    if (k == 0x00)
                        sync = 2;
                    else
                        sync = 0;
                    break;
                case 2:
                    if (k != 0x01)
                    {
                        if (k != 0x00)
                            sync = 0;
                    }
                    else 
                    {
                        if (buffer[i+1] == 0x00) // picture_start
                        {
                            framesWritten++;
                        }
                        else if (buffer[i+1] == 0xB8) // group_of_pictures
                        {
                            int frameNum = framesWritten - 1;
                            
                            if (!gopset && frameNum > 0)
                            {
                                if (firstgoppos > 0)
                                {
                                    // don't set keyframedist...
                                    // playback assumes it's 1 in 15
                                    // frames, and in ATSC they come
                                    // somewhat unevenly.
                                    //keyframedist=frameNum-firstgoppos;
                                    gopset = true;
                                }
                                else
                                    firstgoppos = frameNum;
                            }
                            
                            long long startpos = 
                                ringBuffer->GetFileWritePosition();

                            long long keyCount = frameNum/keyframedist;
                            
                            positionMap[keyCount] = startpos;
                            
                            if (curRecording && db_lock && db_conn &&
                                ((positionMap.size() % 30) == 0))
                            {
                                pthread_mutex_lock(db_lock);
                                MythContext::KickDatabase(db_conn);
                                curRecording->SetPositionMap(
                                    positionMap, MARK_GOP_START,
                                    db_conn, prev_gop_save_pos, 
                                    keyCount);
                                pthread_mutex_unlock(db_lock);
                                prev_gop_save_pos = keyCount + 1;
                            }
                            
                        }
                        sync = 0;
                    }
                    break;
                }
            }
        }
    }
}

int HDTVRecorder::ResyncStream(unsigned char *buffer, int curr_pos, int len)
{
    // Search for two sync bytes 188 bytes apart, 
    int pos = curr_pos;
    int nextpos = pos + 188;
    //int count = 0;
    if (nextpos >= len)
        return -1; // not enough bytes; caller should try again
    
    while (buffer[pos] != SYNC_BYTE || buffer[nextpos] != SYNC_BYTE) {
        pos++;
        nextpos++;
        //count++;
        if (nextpos == len)
            return -2; // not found
    }
    //if (count)
    //cerr << "Skipped " << count << " bytes to resync" << endl;
    return pos;
}

void HDTVRecorder::RewritePID(unsigned char *buffer, int pid)
{
    // We need to rewrite the outgoing PIDs to match the first one
    // the decoder saw ... it does not seem to track PID changes.
    char b1, b2;
    // PID is 13 bits starting in byte 1, bit 4
    b1 = ((pid >> 8) & 0x1F) | (buffer[1] & 0xE0);
    b2 = (pid & 0xFF);
    buffer[1] = b1;
    buffer[2] = b2;
}

int HDTVRecorder::ProcessData(unsigned char *buffer, int len)
{
    int pos = 0;
    int packet_end_pos;
    int packet_start_pos;
    int pid;
    bool payload_unit_start_indicator;
    char adaptation_field_control;

    while (pos + 187 < len) // while we have a whole packet left
    {
        if (buffer[pos] != SYNC_BYTE)
        {
            int newpos = ResyncStream(buffer, pos, len);
            if (newpos == -1)
                return len - pos;
            if (newpos == -2)
                return 188;
            
            pos = newpos;
        }

        ts_packets++;
        packet_start_pos = pos;
        packet_end_pos = pos + 188;
        pos++;
        
        // Decode the link header (next 3 bytes):
        //   1 bit transport_packet_error (if set discard immediately: 
        //      modem error)
        //   1 bit payload_unit_start_indicator (if set this 
        //      packet starts a PES packet)
        //   1 bit transport_priority (ignore)
        //   13 bit PID (packet ID, which transport stream)
        //   2 bit transport_scrambling_control (00,01 OK; 10,11 scrambled)
        //   2 bit adaptation_field_control 
        //     (01-no adptation field,payload only
        //      10-adaptation field only,no payload
        //      11-adaptation field followed by payload
        //      00-reserved)
        //   4 bit continuity counter (should cycle 0->15 in sequence 
        //     for each PID; if skip, we lost a packet; if dup, we can
        //     ignore packet)

        if (get1bit(buffer[pos], 0))
        {
            // transport_packet_error -- skip packet but don't skip
            // ahead, let it resync in case modem dropped a byte
            continue;
        }
        payload_unit_start_indicator = (get1bit(buffer[pos], 1));
        pid = ((buffer[pos] << 8) + buffer[pos+1]) & 0x1fff;
        //cerr << "found PID " << pid << endl;
        pos += 2;
        if (get1bit(buffer[pos], 1)) 
        {
            // packet is from a scrambled stream
            pos = packet_end_pos;
            continue;
        }
        adaptation_field_control = (buffer[pos] & 0x30) >> 4;
        pos++;
        
        if (adaptation_field_control == 2 || adaptation_field_control == 3)
        {
            // If we later care about any adaptation layer bits,
            // we should parse them here:
            /*
              8 bit adaptation header length (after which is payload data)
              1 bit discontinuity_indicator (time base may change)
              1 bit random_access_indicator (?)
              1 bit elementary_stream_priority_indicator (ignore)
              Each of the following extends the adaptation header.  In order:
              1 bit PCR flag (we have PCR data) (adds 6 bytes after adaptation 
                  header)
              1 bit OPCR flag (we have OPCR data) (adds 6 bytes)
                   ((Original) Program Clock Reference; used to time output)
              1 bit splicing_point_flag (we have splice point data) 
                   (adds 1 byte)
                   (splice data is packets until a good splice point for 
                   e.g. commercial insertion -- if these are still set,
                   might be a good way to recognize potential commercials 
                   for flagging)
              1 bit transport_private_data_flag 
                   (adds 1 byte additional bytecount)
              1 bit adaptation_field_extension_flag
              8 bits extension length
              1 bit ltw flag (adds 16 bits after extension flags)
              1 bit piecewise_rate_flag (adds 24 bits)
              1 bit seamless_splice_flag (adds 40 bits)
              5 bits unused flags
            */
            unsigned char adaptation_length;
            adaptation_length = buffer[pos];
            pos += adaptation_length;
        }

        //
        // Pass or reject frames based on PID, and parse info from them
        //

        if (pid == pat_pid) 
        {

            // PID 0: Program Association Table -- lists all other
            // PIDs that make up the program(s) in the whole stream.
            // Based on info in this table, and on which subprogram
            // the user // desires, we should determine which PIDs'
            // payloads to write // to the ring buffer.

            // Example PAT scan code is in the pcHDTV's dtvscan.c,
            // demux_ts_parse_pat

            // NOTE: Broadcasters are encouraged to keep the
            // subprogram:PID mapping constant.  If we store this data
            // in the channel database, we can branch-predict which
            // PIDs we are looking for, and can thus "tune" the
            // subprogram more quickly.

            // For now we're using the dirty hack below instead.
            
            // We should rewrite the PAT to describe just the one
            // stream we are recording.  Always use the same set of
            // PIDs // so the decoder has an easier time following
            // channel changes.

            // decoder needs PAT, write it to the stream
            ringBuffer->Write(&buffer[packet_start_pos], 188);
        }
        else if (pid == psip_pid)
        {
            // Here we should decode the PSIP and fill guide data
            // decoder does not need psip

            // Some sample code is in pcHDTV's dtvscan.c,
            // accum_sect/dequeue_buf/atsc_tables.  We should stuff
            // this data back into the channel's guide data, though if
            // it's unreliable we will need to be able to prefer the
            // downloaded XMLTV data.

        }
        else if (pid == VIDEO_PID(base_pid))
        {
            FindKeyframes(buffer, packet_start_pos, pos, 
                    adaptation_field_control, payload_unit_start_indicator);
            // decoder needs video, of course (just this PID)
            if (gopset || firstgoppos) 
            {
                 // delay until first GOP to avoid decoder crash on res change
                RewritePID(&(buffer[packet_start_pos]), 
                           VIDEO_PID(first_base_pid));
            ringBuffer->Write(&buffer[packet_start_pos], 188);
        }
        }
        else if (pid == AUDIO_PID(base_pid))
        {
            // decoder needs audio, of course (just this PID)
            if (gopset || firstgoppos) 
            {
                RewritePID(&(buffer[packet_start_pos]), 
                           AUDIO_PID(first_base_pid));
            ringBuffer->Write(&buffer[packet_start_pos], 188);
        }
        }
        else if (pid == base_pid)
        {
            // decoder needs base PID
            RewritePID(&(buffer[packet_start_pos]), first_base_pid);
            ringBuffer->Write(&buffer[packet_start_pos], 188);
        }
        else 
        {
            // Not a PID we're watching
            
            // As a hack until we start decoding PAT, find the lowest
            // video PID in the first 50 packets and record only that
            // stream
            if ((pid & 0xff0f) == 0x0001 && !base_pid) 
            {
                if (pid < lowest_video_pid)
                    lowest_video_pid = pid;
                video_pid_packets++;
                if (video_pid_packets >= 50) 
                {
                    base_pid = lowest_video_pid - 1;
                    if (first_base_pid == 0)
                        first_base_pid = base_pid;
                }
            }
        }
        // Advance to next TS packet
        pos = packet_end_pos;
    }

    return len - pos;
}

void HDTVRecorder::StopRecording(void)
{
    encoding = false;
}

void HDTVRecorder::Reset(void)
{
    framesWritten = 0;
    gopset = false;
    firstgoppos = 0;
    base_pid = 0;
    ts_packets = 0;
    lowest_video_pid = 0x1fff;
    video_pid_packets = 0;
    
    prev_gop_save_pos = -1;

    positionMap.clear();

    if (chanfd > 0) 
    {
        int ret = close(chanfd);
        if (ret < 0) 
        {
            perror("close");
            return;
        }
        chanfd = open(videodevice.ascii(), O_RDWR);
        if (chanfd <= 0)
        {
            cerr << "HD1 Can't open video device: " << videodevice 
                 << " chanfd = "<< chanfd << endl;
            perror("open video");
            return;
        }
    }
}

void HDTVRecorder::Pause(bool clear)
{
    cleartimeonpause = clear;
    mainpaused = false;
    paused = true;
}

void HDTVRecorder::Unpause(void)
{
    paused = false;
}

bool HDTVRecorder::GetPause(void)
{
    return mainpaused;
}

void HDTVRecorder::WaitForPause(void)
{
    if (!mainpaused)
        if (!pauseWait.wait(1000))
            cerr << "Waited too long for recorder to pause\n";
}

bool HDTVRecorder::IsRecording(void)
{
    return recording;
}

long long HDTVRecorder::GetFramesWritten(void)
{
    return framesWritten;
}

long long HDTVRecorder::GetKeyframePosition(long long desired)
{
    long long ret = -1;

    if (positionMap.find(desired) != positionMap.end())
        ret = positionMap[desired];

    return ret;
}

void HDTVRecorder::GetBlankFrameMap(QMap<long long, int> &blank_frame_map)
{
    (void)blank_frame_map;
}
