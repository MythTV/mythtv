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

extern "C" {
#include "../libavcodec/avcodec.h"
#include "../libavformat/avformat.h"
#include "../libavformat/mpegts.h"
}

// n.b. at 19 Mbits/sec, default buffer size is approx. 1/10 sec.
#define PACKETS (HD_BUFFER_SIZE / 188)

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
    framesSeen = 0;

    chanfd = -1; 

    keyframedist = 30;
    gopset = false;
    m_in_mpg_headers = false;
    seq_start_is_gop = false;
    seq_count = 0;
    m_header_sync = 0;

    firstgoppos = 0;

    desired_program = 1; // default to program #1
    pat_pid = 0;
    psip_pid = 0x1ffb;
    audio_pid = video_pid = -1;
    pmt_pid = -1;
    output_base_pid = 0;
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
        curRecording->SetPositionMap(positionMap, MARK_GOP_BYFRAME, db_conn);
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
            m_in_mpg_headers = true;
            m_header_sync = 0;
        }
        if (m_in_mpg_headers) {
            // Scan for PES header codes; specifically picture_start
            // and group_start (of_pictures).  These should be within 
            // this first TS packet of the PES packet.
            //   00 00 01 00: picture_start_code
            //   00 00 01 B8: group_start_code
            //   (there are others that we don't care about)
            payload_size = packet_end_pos - pkt_start;
            for (int i=pkt_start; i < packet_end_pos && m_in_mpg_headers; i++)
            {
                char k = buffer[i];
                switch (m_header_sync)
                {
                case 0:
                    if (k == 0x00)
                        m_header_sync = 1;
                    break;
                case 1:
                    if (k == 0x00)
                        m_header_sync = 2;
                    else
                        m_header_sync = 0;
                    break;
                case 2:
                    if (k != 0x01)
                    {
                        if (k != 0x00)
                            m_header_sync = 0;
                    }
                    else 
                    {
                        if (buffer[i+1] == 0x00) // picture_start
                        {
                            if (gopset || firstgoppos > 0)
                                framesWritten++;
                            framesSeen++;
                        }

                        if (buffer[i+1] == 0xB3)
                        {
                            seq_count ++;
                            if (seq_count > 1 && !gopset) 
                            {
                                seq_start_is_gop = true;
                            }
                        }
                        if (buffer[i+1] == 0xB8 || 
                            (seq_start_is_gop && buffer[i+1] == 0xB3))
                        {
                            // group_of_pictures
                            int frameNum = framesWritten - 1;
                            
                            if (!gopset && framesSeen > 0)
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
                                    firstgoppos = (frameNum > 0) ? 
                                        frameNum : 1;
                            }
                            
                            long long startpos = 
                                ringBuffer->GetFileWritePosition();

                            positionMap[frameNum] = startpos;
                            
                            if (curRecording && db_lock && db_conn &&
                                ((positionMap.size() % 30) == 0))
                            {
                                pthread_mutex_lock(db_lock);
                                MythContext::KickDatabase(db_conn);
                                curRecording->SetPositionMap(
                                    positionMap, MARK_GOP_BYFRAME,
                                    db_conn, prev_gop_save_pos, 
                                    frameNum);
                                pthread_mutex_unlock(db_lock);
                                prev_gop_save_pos = frameNum + 1;
                            }
                            
                        }
                        if (buffer[i+1] >= 0x01 && buffer[i+1] <= 0xAF)
                        {
                            // video slice ... end of "interesting"
                            // headers
                            m_in_mpg_headers = false;
                        }
                        m_header_sync = 0;
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

bool HDTVRecorder::RewritePAT(unsigned char *buffer, int pid, int pkt_len)
{
    int pos = 0;

    // Rewrite the PAT table, eliminating any other streams that the old
    // one may have described so that ffmpeg can only find ours (useful since
    // some stations put programs in their PAT tables in descending order 
    // instead of ascending, which would confuse ffmpeg).  While we're at it
    // change the program number to 1 in case it wasn't that already.
    
    // PAT format: 2-byte TSID, 2-byte PID, repeated for each PID
    // TODO: typically PIDs seem to be padded with 1-bits.  Someone 
    // with better knowledge of PAT table formats should confirm this.
    // If it is wrong, fix the '0xe0' values in RewritePMT too.
  
    // new_len is the length of the new PAT we're creating, including the
    // 3 bytes of the header that occur before the length word.  We'll 
    // account for those 3 bytes later.  If the contents of the new PAT
    // ever change, be sure to update this!
    int new_len = 16;

    if (new_len > pkt_len)
        return false;  // new packet would exceed boundary, skip it

    pos = 8;  // section header is 8 bytes, skip it

    buffer[pos] = 0x00;
    buffer[pos+1] = 0x01;
    buffer[pos+2] = 0xe0 | ((pid & 0x1f00) >> 8);
    buffer[pos+3] = pid & 0xff;
    pos += 4;

    // rewrite section length
    new_len -= 3; // account for 3 bytes before length word in header
    buffer[1] = (buffer[1] & 0xf0) | ((new_len & 0x0f00) >> 8);
    buffer[2] = new_len & 0xff;

    // fix the checksum
    unsigned int crc = mpegts_crc32(buffer, pos);
    buffer[pos++] = (crc & 0xff000000) >> 24;
    buffer[pos++] = (crc & 0x00ff0000) >> 16;
    buffer[pos++] = (crc & 0x0000ff00) >> 8;
    buffer[pos++] = (crc & 0x000000ff);

    // pad the rest of the packet with 0xff
    memset(buffer + pos, 0xff, pkt_len - pos);

    return true;
}

bool HDTVRecorder::RewritePMT(unsigned char *buffer, int new_pid, int pkt_len)
{
    // TODO: if it's possible for a PMT to span packets, then this function
    // doesn't know how to deal with anything after the first packet.  On the
    // other hand, neither does ffmpeg as near as I can tell.
    
    int pos = 8;
    int pid;
    int sec_len = ((buffer[1] & 0x0f) << 8) + buffer[2];
    sec_len += 3;  // adjust for 3 header bytes

    // make sure section doesn't span packet bounds
    if (sec_len > pkt_len)
        sec_len = pkt_len;

    // rewrite program number to 1
    buffer[3] = 0x00;
    buffer[4] = 0x01;
    pos += 2;

    // skip program info
    if (buffer[pos] == 0xff)
        return false;  // premature end of packet
    pos += 2 + (((buffer[pos] << 8) | buffer[pos+1]) & 0x0fff);

    video_pid = audio_pid = -1;
    // rewrite other pids
    while (pos < sec_len - 4) {
        if (buffer[pos] == 0xff)
            break;  // hit end of packet, ok here
        int stream_type = (int)buffer[pos];
        pos++;

        pid = ((buffer[pos] << 8) | buffer[pos+1]) & 0x1fff;
        if (stream_type == 0x02) {
            video_pid = pid;
            pid = VIDEO_PID(new_pid);
        } 
        else if ((stream_type == 0x81 | stream_type == 0x04) && (audio_pid < 0)) {
            audio_pid = pid;
            pid = AUDIO_PID(new_pid);
        }

        buffer[pos] = ((pid & 0x1f00) >> 8) | 0xe0;
        buffer[pos+1] = pid & 0x00ff;
        pos += 2;

        if (buffer[pos] == 0xff)
            return false;  // premature end of packet
        // bounds checked at top of while loop
        pos += 2 + (((buffer[pos] << 8) | buffer[pos+1]) & 0x0fff);
    }

    // rewrite pcr_pid
    pid = ((buffer[8] << 8) | buffer[9]) & 0x1fff;
    if (pid == video_pid) {
        pid = VIDEO_PID(new_pid);
    }
    else if (pid == audio_pid) {
        pid = AUDIO_PID(new_pid);
    }
    else {
        return false;  // don't know how to rewrite
    }
    buffer[8] = ((pid & 0x1f00) >> 8) | 0xe0;
    buffer[9] = pid & 0x00ff;

    // fix the checksum
    unsigned int crc = mpegts_crc32(buffer, sec_len - 4);
    buffer[sec_len - 4] = (crc & 0xff000000) >> 24;
    buffer[sec_len - 3] = (crc & 0x00ff0000) >> 16;
    buffer[sec_len - 2] = (crc & 0x0000ff00) >> 8;
    buffer[sec_len - 1] = (crc & 0x000000ff);

    return true;
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
            adaptation_length = buffer[pos++];
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
            // the user desires, we should determine which PIDs'
            // payloads to write to the ring buffer.

            // Example PAT scan code is in the pcHDTV's dtvscan.c,
            // demux_ts_parse_pat

            // NOTE: Broadcasters are encouraged to keep the
            // subprogram:PID mapping constant.  If we store this data
            // in the channel database, we can branch-predict which
            // PIDs we are looking for, and can thus "tune" the
            // subprogram more quickly.

            // We rewrite the PAT to describe just the one
            // stream we are recording.  Always use the same set of
            // PIDs so the decoder has an easier time following
            // channel changes.
            //
            // Basically, the output PMT is pid 0x10, video is 0x11 and
            // audio is 0x14.
            //
            // sec_start should pretty much always be pos + 1, but
            // just in case...
            int sec_start = pos + buffer[pos] + 1;
            int sec_len = (buffer[sec_start + 1] & 0x0f) << 8 | buffer[sec_start + 2];
            int i;
            pmt_pid = -1;

            for (i = sec_start + 8; i < sec_start + 3 + sec_len - 4; i += 4) {
                int prog_num = (buffer[i] << 8) | buffer[i + 1];
                int prog_pid = (buffer[i + 2] & 0x1f) << 8 | buffer[i + 3];
                if (prog_num == desired_program) {
                    pmt_pid = prog_pid;
                    break;
                }
            }
            if (pmt_pid == -1) {
                pmt_pid = (buffer[sec_start + 8 + 2] & 0x1f) << 8 |
                                buffer[sec_start + 8 + 3];
            }
            output_base_pid = 16;

            // write to ringbuffer if pids gets rewritten successfully.
            if (RewritePAT(&buffer[sec_start], output_base_pid,
                           packet_end_pos - sec_start))
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
        else if (pid == video_pid)
        {
            FindKeyframes(buffer, packet_start_pos, pos, 
                    adaptation_field_control, payload_unit_start_indicator);
            // decoder needs video, of course (just this PID)
            if (gopset || firstgoppos) 
            {
                 // delay until first GOP to avoid decoder crash on res change
                RewritePID(&(buffer[packet_start_pos]), 
                           VIDEO_PID(output_base_pid));
                ringBuffer->Write(&buffer[packet_start_pos], 188);
            }
        }
        else if (pid == audio_pid)
        {
            // decoder needs audio, of course (just this PID)
            if (gopset || firstgoppos) 
            {
                RewritePID(&(buffer[packet_start_pos]), 
                           AUDIO_PID(output_base_pid));
                ringBuffer->Write(&buffer[packet_start_pos], 188);
            }
        }
        else if (pid == pmt_pid)
        {
            // decoder needs base PID
            int sec_start = pos + buffer[pos] + 1;
            RewritePID(&(buffer[packet_start_pos]), output_base_pid);

            // if it's a PMT table, rewrite the PIDs contained in it too
            if (buffer[sec_start] == 0x02) {
                if (RewritePMT(&(buffer[sec_start]), output_base_pid,
                               packet_end_pos - sec_start))
                {
                    ringBuffer->Write(&buffer[packet_start_pos], 188);
                }
            }
            else {
                // some other kind of packet?  possible?  do we care?
                ringBuffer->Write(&buffer[packet_start_pos], 188);
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
    framesSeen = 0;
    gopset = false;
    m_in_mpg_headers = false;
    seq_start_is_gop = false;
    seq_count = 0;
    m_header_sync = 0;
    firstgoppos = 0;
    ts_packets = 0;
    pmt_pid = -1;
    video_pid = audio_pid = -1;
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

void HDTVRecorder::ChannelNameChanged(const QString& new_chan)
{
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
    pthread_mutex_unlock(db_lock);
    int pos = freqid.find('-');
    if (pos != -1) 
        desired_program = atoi(freqid.mid(pos+1).ascii());
    else
        desired_program = 1; // default to program #1
}
