/* HDTVRecorder
   Version 0.1
   July 4th, 2003
   GPL License (c)
   By Brandon Beattie

   Portions of this code taken from mpegrecorder
   The current version does not support seeking.  If someone
   can add that it would be very kind.  Other countless 
   improvements still need to be made by others if possible.
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
extern AVInputFormat mpegps_demux;  // change me possible
}

//#define BUFFER_SIZE 4096
//#define BUFFER_SIZE 256001
#define BUFFER_SIZE 255868
#define PACKETS (BUFFER_SIZE / 188)

HDTVRecorder::HDTVRecorder()
            : RecorderBase()
{
    paused = false;
    mainpaused = false;
    recording = false;

    framesWritten = 0;

    chanfd = -1; 
    readfd = -1;

    keyframedist = 15;
    gopset = false;

    firstgoppos = 0;
}

HDTVRecorder::~HDTVRecorder()
{
    if (chanfd > 0)
        close(chanfd);
    if (readfd > 0)
        close(readfd);
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
    int i,len;
    unsigned char data_byte[4];
    unsigned int pids[10], counts[10];
    int insync = 0;
    int ret;

    for(i=0;i<10;i++) 
    {
        pids[i] = 0;
        counts[i] = 0;
    }

    chanfd = open(videodevice.ascii(), O_RDWR);
    if (chanfd <= 0)
    {
        cerr << "HD1 Can't open video device: " << videodevice 
             << " chanfd = "<< chanfd << endl;
        perror("open video:");
        return;
    }

    readfd=chanfd;
    if (readfd < 0)
    {
        cerr << "HD2 Can't open video device: " << videodevice 
             << " readfd = "<< readfd << endl;
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
        len = read(readfd, &data_byte[0], 1); // read next byte
        if (len == 0) 
            return;  // end of file

        if (data_byte[0] == 0x47)
        {
            read(readfd, buffer, 187); //read next 187 bytes-remainder of packet
            // process next packet
            while (encoding) 
            {
                len = read(readfd, &data_byte[0], 4); // read next 4 bytes

                if (len != 4) 
                   return;

                len = read(readfd, buffer+4, 184);
                
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

                if (buf[0] != 0x47) 
                {
                    cout<<"Bad SYNC byte!!!"<<endl;
                    errors++;
                    insync = 0;
                    break;
                } 
                else 
                {
                    insync++;
                    if (insync == 10) 
                    {
                        // TRANSFER DATA
                        while (encoding) 
                        {
                            if (paused)
                            {
                                if (readfd > 0)
                                {
                                    //close(readfd);
                                    readfd = -1;
                                }
                                mainpaused = true;
                                usleep(50);
                                continue;
                            }

                            if (readfd < 0)
                            {
                                //readfd = open(videodevice.ascii(), O_RDWR);
                                readfd = chanfd;
                            }

                            ret = read(readfd, buffer, 188*PACKETS);
                            //fwrite(fout,188,PACKETS,fout);

                            if (ret < 0)
                            {
                                cerr << "HD error reading from: "
                                     << videodevice << endl;
                                perror("read");
                                continue;
                            }
                            else if (ret > 0)
                                ProcessData(buffer, ret);

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
    AVInputFormat *fmt = &mpegps_demux;
    fmt->flags |= AVFMT_NOFILE;

    ic = (AVFormatContext *)av_mallocz(sizeof(AVFormatContext));
    if (!ic)
    {
        cerr << "HD Couldn't allocate context\n";
        return false;
    }

    QString filename = "blah.mpg";
    char *cfilename = (char *)filename.ascii();
    AVFormatParameters params;

    ic->pb.buffer_size = BUFFER_SIZE;
    ic->pb.buffer = NULL;
    ic->pb.buf_ptr = NULL;
    ic->pb.write_flag = 0;
    ic->pb.buf_end = NULL;
    ic->pb.opaque = this;
    ic->pb.read_packet = mpg_read_packet;
    ic->pb.write_packet = mpg_write_packet;
    ic->pb.seek = mpg_seek_packet;
    ic->pb.pos = 0;
    ic->pb.must_flush = 0;
    ic->pb.eof_reached = 0;
    ic->pb.is_streamed = 0;
    ic->pb.max_packet_size = 0;

    int err = av_open_input_file(&ic, cfilename, fmt, 0, &params);
    if (err < 0)
    {
        cerr << "HD Couldn't initialize decocder\n";
        return false;
    }    

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

#define GOP_START     0x000001B8
#define PICTURE_START 0x00000100
#define SLICE_MIN     0x00000101
#define SLICE_MAX     0x000001af

bool HDTVRecorder::PacketHasHeader(unsigned char *buf, int len,
                                   unsigned int startcode)
{
    unsigned char *bufptr;
    unsigned int state = 0xFFFFFFFF, v;

    bufptr = buf;

    while (bufptr < buf + len)
    {
        v = *bufptr++;
        if (state == 0x000001)
        {
            state = ((state << 8) | v) & 0xFFFFFF;
            if (state >= SLICE_MIN && state <= SLICE_MAX)
                return false;
            if (state == startcode)
                return true;
        }
        state = ((state << 8) | v) & 0xFFFFFF;
    }

    return false;
}

void HDTVRecorder::ProcessData(unsigned char *buffer, int len)
{
    AVPacket pkt;

    ic->pb.buffer = buffer;
    ic->pb.buf_ptr = ic->pb.buffer;
    ic->pb.buf_end = ic->pb.buffer + len;
    ic->pb.eof_reached = 0;
    ic->pb.pos = len;

    while (ic->pb.eof_reached == 0)
    {
        if (av_read_packet(ic, &pkt) < 0)
            break;
        
        if (pkt.stream_index > ic->nb_streams)
        {
            cerr << "HD bad stream\n";
            av_free_packet(&pkt);
            continue;
        }

        AVStream *curstream = ic->streams[pkt.stream_index];
        if (pkt.size > 0 && curstream->codec.codec_type == CODEC_TYPE_VIDEO)
        {
            if (PacketHasHeader(pkt.data, pkt.size, PICTURE_START))
            {
                framesWritten++;
            }

            if (PacketHasHeader(pkt.data, pkt.size, GOP_START))
            {
                int frameNum = framesWritten - 1;

                if (!gopset && frameNum > 0)
                {
                    if (firstgoppos > 0)
                    {
                        keyframedist = frameNum - firstgoppos;
                        gopset = true;
                    }
                    else
                        firstgoppos = frameNum;
                }

                long long startpos = ringBuffer->GetFileWritePosition();
                startpos += pkt.startpos;

                long long keyCount = frameNum / keyframedist;

                positionMap[keyCount] = startpos;

                if (curRecording && db_lock && db_conn &&
                    ((positionMap.size() % 30) == 0))
                {
                    pthread_mutex_lock(db_lock);
                    MythContext::KickDatabase(db_conn);
                    curRecording->SetPositionMap(positionMap, MARK_GOP_START,
                            db_conn, prev_gop_save_pos, keyCount);
                    pthread_mutex_unlock(db_lock);
                    prev_gop_save_pos = keyCount + 1;
                }
            }
        }

        av_free_packet(&pkt);
    }

    ringBuffer->Write(buffer, len);
}

void HDTVRecorder::StopRecording(void)
{
    encoding = false;
}

void HDTVRecorder::Reset(void)
{
    AVPacketList *pktl = NULL;
    while ((pktl = ic->packet_buffer))
    {
        ic->packet_buffer = pktl->next;
        av_free(pktl);
    }

    ic->pb.pos = 0;
    ic->pb.buf_ptr = ic->pb.buffer;
    ic->pb.buf_end = ic->pb.buffer;

    framesWritten = 0;

    positionMap.clear();
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
