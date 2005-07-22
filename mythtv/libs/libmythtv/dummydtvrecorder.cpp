// -*- Mode: c++ -*-

#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <cerrno>
#include <ctime>

#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "mythcontext.h"
#include "dummydtvrecorder.h"
#include "tspacket.h"
#include "RingBuffer.h"

#define DUMMY_VIDEO_PID VIDEO_PID(0x20)

/** \class DummyDTVRecorder
    \brief Outputs a dummy mpeg stream to a RingBuffer

\TODO DummyDTVRecorder does not yet implement pes streams, only ts streams

\code
To create a program stream video try something like:
  convert /tmp/hello_world.png ppm:- | \
          ppmtoy4m -n15 -F30000:1001 -A10:11 -I p -r | \
          mpeg2enc -n n -f3 -b8000 -a3 --no-constraints -o /tmp/tmp.es

To convert this to a transport stream try something like:
  vlc /tmp/tmp.es --sout \
      '#std{access=file,mux=ts{pid-pmt=0x20,pid-video=0x21.tsid=1},\
            url="/tmp/tmp.ts"}'
OR, convert it to a ps or pes stream and use the following
  iso13818ts --fpsi 500 --ident 23 --ps /tmp/tmp.ps 1 > /tmp/tmp.ts
(You will need iso13818ts from http://www.scara.com/~schirmer/o/mplex13818/)
\endcode
 */

DummyDTVRecorder::DummyDTVRecorder(bool tsmode, RingBuffer *rbuffer, bool autoStart)
    : _tsmode(tsmode),
      _desired_width(1920), _desired_height(1088), _desired_frame_rate(29.97),
      _stream_data(-1)
{
    gettimeofday(&_next_frame_time, NULL);

    _buffer_size = TSPacket::SIZE * 15000;
    if ((_buffer = new unsigned char[_buffer_size]))
        bzero(_buffer, _buffer_size);

    if (rbuffer)
        SetRingBuffer(rbuffer);

    if (rbuffer && autoStart)
        StartRecordingThread();
}

DummyDTVRecorder::~DummyDTVRecorder()
{
    StopRecordingThread();
    if (_stream_data >= 0)
        close(_stream_fd);
    if (_buffer)
        delete [] _buffer;
}

/** \fn DummyDTVRecorder::SetDesiredAttributes(uint,uint,double)
 *  \brief preferred resolution and framerate for dummy stream.
 */
void DummyDTVRecorder::SetDesiredAttributes(uint width, uint height,
                                            double frame_rate)
{
    _desired_width      = width;
    _desired_height     = height;
    _desired_frame_rate = frame_rate;
}

void DummyDTVRecorder::StartRecording(void)
{
    VERBOSE(VB_RECORD, QString("StartRecording"));

    if (!Open())
    {
        _error = true;        
        return;
    }

    _request_recording = true;
    _recording = true;

    int len;
    int remainder = 0;
    // TRANSFER DATA
    while (_request_recording) 
    {
        len = read(_stream_fd, &(_buffer[remainder]), _buffer_size - remainder);

        if (len == 0)
        {
            lseek(_stream_fd, 0, SEEK_SET);
            continue;
        }

        len += remainder;
        remainder = ProcessData(_buffer, len);
        if (remainder > 0) // leftover bytes
            memmove(_buffer, &(_buffer[_buffer_size - remainder]),
                    remainder);
    }

    FinishRecording();
    _recording = false;
}

/** \fn DummyDTVRecorder::Open()
 *  \brief Opens a video file matching the desired resolution and frame rate.
 *
 *  \TODO DummyDTVRecorder::Open() should pick the closest matching file for
 *        the desired params, but currently only works if there is an exact
 *        match.
 */
bool DummyDTVRecorder::Open(void)
{
    QString filename = QString("dummy%1x%2a%3.%4")
        .arg(gContext->GetShareDir()+"videos/")
        .arg(_desired_width).arg(_desired_height)
        .arg(_desired_frame_rate, 0, 'f', 2)
        .arg(_tsmode ? "ts" : "pes");

    SetFrameRate(_desired_frame_rate);
    _stream_fd = open(filename.ascii(), O_RDWR);    

    return _stream_fd >= 0;
}

int DummyDTVRecorder::ProcessData(unsigned char *buffer, int len)
{
    int pos = 0;

    while (pos + 187 < len) // while we have a whole packet left
    {
        const TSPacket *pkt = reinterpret_cast<const TSPacket*>(&buffer[pos]);

        // Parse frame info from video packets
        int cnt = _frames_seen_count;
        if (pkt->PID() == DUMMY_VIDEO_PID)
            FindKeyframes(pkt);

        if (cnt != _frames_seen_count)
        {
            // we got a new frame
            struct timeval now;
            gettimeofday(&now, NULL);

            int usec = (_next_frame_time.tv_sec - now.tv_sec) * 1000000 +
                (_next_frame_time.tv_usec - now.tv_usec);

            // sleep if we are sending too much data...
            if (usec > 5000 && _frames_seen_count > 15)
                usleep(usec);

            // set next trigger
            gettimeofday(&_next_frame_time, NULL);
            int delay = (int) (1000000.0 / GetFrameRate());
            int x = _next_frame_time.tv_usec + delay;
            _next_frame_time.tv_usec = x % 1000000;
            _next_frame_time.tv_sec  = x / 1000000;
        }

        ringBuffer->Write(pkt->data(), TSPacket::SIZE);

        pos += TSPacket::SIZE; // Advance to next TS packet
    }

    return len - pos;
}

void *DummyDTVRecorder::RecordingThread(void *param)
{
    DummyDTVRecorder *rec = (DummyDTVRecorder*) param;
    rec->StartRecording();
    return NULL;    
}

void DummyDTVRecorder::StartRecordingThread(void)
{
    pthread_create(&_rec_thread, NULL, RecordingThread, this);
}

void DummyDTVRecorder::StopRecordingThread(void)
{
    if (_recording)
    {
        StopRecording();
        while (_recording)
            usleep(100);
    }
}
