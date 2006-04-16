// -*- Mode: c++ -*-

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
#include "programinfo.h" // for MARK_GOP_BYFRAME

#define DUMMY_VIDEO_PID VIDEO_PID(0x20)

/** \class DummyDTVRecorder
    \brief Outputs a dummy mpeg stream to a RingBuffer

\TODO DummyDTVRecorder does not yet implement PS streams, only TS streams

\code
To create a program stream video try something like:
  convert /tmp/1920x1088.png ppm:- | \
          ppmtoy4m -n15 -F30000:1001 -A1:1 -I p -r | \
          mpeg2enc -c -n n -f3 -b8000 -a3 --no-constraints -o /tmp/tmp.es
  mplex -f3 -b8000 --no-constaints -L48000:1:8 -o /tmp/tmp.ps /tmp/tmp.es

OR, for an SDTV stream
  convert /tmp/640x480.png ppm:- | \
          ppmtoy4m -n15 -F30000:1001 -A10:11 -I p -r | \
          mpeg2enc -c -n n -f3 -b8000 -a3 --no-constraints -o /tmp/tmp.es
  mplex -f3 -b8000 --no-constaints -L48000:1:8 -o /tmp/tmp.ps /tmp/tmp.es

To convert this to a transport stream try something like:
  vlc /tmp/tmp.es --sout \
      '#std{access=file,mux=ts{pid-pmt=0x20,pid-video=0x21.tsid=1},\
            url="/tmp/tmp.ts"}'
OR, convert it to a ps or pes stream and use the following
  iso13818ts --fpsi 500 --ident 23 --ps /tmp/tmp.ps 1 > /tmp/tmp.ts
(You will need iso13818ts from http://www.scara.com/~schirmer/o/mplex13818/)
\endcode
 */

DummyDTVRecorder::DummyDTVRecorder(TVRec *rec,
                                   bool tsmode, RingBuffer *rbuffer,
                                   uint desired_width, uint desired_height,
                                   double desired_frame_rate,
                                   uint non_buf_frames,
                                   uint bitrate,
                                   bool autoStart)
    : DTVRecorder(rec, "DummyDTVRecorder"), _tsmode(tsmode),
      _desired_width(desired_width), _desired_height(desired_height),
      _desired_frame_rate(desired_frame_rate), _desired_bitrate(bitrate),
      _non_buf_frames(max(non_buf_frames,(uint)1)),
      _packets_in_frame(0)
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
    Close();
    if (_buffer)
    {
        delete [] _buffer;
        _buffer = NULL;
    }
}

void DummyDTVRecorder::deleteLater(void)
{
    StopRecordingThread();
    Close();
    if (_buffer)
    {
        delete [] _buffer;
        _buffer = NULL;
    }
    DTVRecorder::deleteLater();
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
    VERBOSE(VB_RECORD, QString("DummyDTVRecorder::StartRecording -- begin"));

    if (!Open())
    {
        VERBOSE(VB_IMPORTANT,
                QString("DummyDTVRecorder::StartRecording -- open failed"));
        _error = true;
        return;
    }

    _request_recording = true;
    _recording = true;

    int len;
    int remainder = 0;
    // TRANSFER DATA
    while (_request_recording || _frames_seen_count <= 5)
    {
        len = read(_stream_fd, &(_buffer[remainder]), _buffer_size - remainder);

        if (len == 0)
        {
            VERBOSE(VB_RECORD, QString("DummyRec: Restart! Frames seen %1")
                    .arg(_frames_seen_count));
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
    VERBOSE(VB_RECORD, QString("DummyDTVRecorder::StartRecording -- end"));
}

/** \fn DummyDTVRecorder::Open(void)
 *  \brief Opens a video file matching the desired resolution and frame rate.
 *
 *  \TODO DummyDTVRecorder::Open() should pick the closest matching file for
 *        the desired params, but currently only works if there is an exact
 *        match.
 */
bool DummyDTVRecorder::Open(void)
{
    SetFrameRate(_desired_frame_rate);

    QString p = gContext->GetThemesParentDir();
    QString path[] =
        { p+gContext->GetSetting("Theme", "G.A.N.T.")+"/", p+"default/", };
    QString filename = QString("dummy%1x%2p%3.%4")
        .arg(_desired_width).arg(_desired_height)
        .arg(_desired_frame_rate, 0, 'f', 2)
        .arg(_tsmode ? "ts" : "pes");

    _stream_fd = open(path[0]+filename.ascii(), O_RDONLY);
    if (_stream_fd < 0)
        _stream_fd = open(path[1]+filename.ascii(), O_RDONLY);

    return _stream_fd >= 0;
}

void DummyDTVRecorder::Close(void)
{
    if (_stream_fd >= 0)
    {
        close(_stream_fd);
        _stream_fd = -1;
    }
}

static void set_trigger(struct timeval &trig, int udelay)
{
    gettimeofday(&trig, NULL);
    int x = trig.tv_usec + udelay;
    trig.tv_usec = x % 1000000;
    trig.tv_sec += x / 1000000;
}

static void delay_to_trigger(const struct timeval &trig, bool &cont)
{
    struct timeval now;
    gettimeofday(&now, NULL);
    int usec = ((trig.tv_sec - now.tv_sec) * 1000000 +
                (trig.tv_usec - now.tv_usec));
    usec = max(usec, 0);
    int usec_f = usec / 5000;
    int usec_r = usec % 5000;
    //cerr<<"usec: "<<usec<<" ("<<usec_f<<":"<<usec_r<<")"<<endl;
    usleep(usec_r);
    for (int i=0; i<usec_f && cont; i++)
        usleep(5000);
}

static void frame_delay(bool to_trig, long long frames_seen,
                        struct timeval &nframe, struct timeval &n5frame,
                        double frame_delay, bool &cont)
{
    if (to_trig)
        delay_to_trigger((frames_seen % 5) ? nframe : n5frame, cont);
    else
        usleep(100);

    if (!(frames_seen % 5))
        set_trigger(n5frame, (int) (5.0 * frame_delay));
    set_trigger(nframe, (int) frame_delay);
}

int DummyDTVRecorder::ProcessData(unsigned char *buffer, int len)
{
    int pos = 0;

    while (pos + 187 < len && _request_recording) // while we have a whole packet left
    {
        const TSPacket *pkt = reinterpret_cast<const TSPacket*>(&buffer[pos]);

        // Parse frame info from video packets
        unsigned long long cnt = _frames_seen_count;
        if (pkt->PID() == DUMMY_VIDEO_PID)
            FindKeyframes(pkt);

        if (cnt != _frames_seen_count)
        {
            // ensure desired bitrate with null packets
            long long bpf = (long long)(_desired_bitrate / _desired_frame_rate);
            uint desired_packets = bpf / (TSPacket::SIZE * 8);
            for (; _packets_in_frame < desired_packets; _packets_in_frame++)
                ringBuffer->Write(TSPacket::NULL_PACKET->data(), TSPacket::SIZE);
            _packets_in_frame = 0;

            // sync so that these packets are seen...
            if (_frames_seen_count < _non_buf_frames ||
                ((_frames_seen_count % 10) == 0))
            {
                //ringBuffer->WriterFlush();
                ringBuffer->Sync();
            }

            // make sure we don't send too many frames
            frame_delay(_frames_seen_count > _non_buf_frames,
                        _frames_seen_count,
                        _next_frame_time, _next_5th_frame_time,
                        1000000.0 / GetFrameRate(), _request_recording);
        }

        ringBuffer->Write(pkt->data(), TSPacket::SIZE);
        _packets_in_frame++;

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

void DummyDTVRecorder::StartRecordingThread(unsigned long long req_frames)
{
    if (!ringBuffer)
        return;
    pthread_create(&_rec_thread, NULL, RecordingThread, this);
    while (_frames_seen_count < req_frames)
        usleep(1000);
    ringBuffer->WriterFlush();
}

void DummyDTVRecorder::FinishRecording(void)
{
    VERBOSE(VB_RECORD, "DummyDTVRecorder::FinishRecording()");
    ringBuffer->Sync();
    if (curRecording && _position_map_delta.size())
    {
        curRecording->SetPositionMapDelta(_position_map_delta,
                                          MARK_GOP_BYFRAME);
        _position_map_delta.clear();
    }
}

void DummyDTVRecorder::StopRecordingThread(void)
{
    VERBOSE(VB_RECORD, "DummyDTVRecorder::StopRecordingThread(void) -- beg");
    if (_recording)
    {
        StopRecording();
        pthread_join(_rec_thread, NULL);
        while (_recording)
            usleep(100);
    }
    VERBOSE(VB_RECORD, "DummyDTVRecorder::StopRecordingThread(void) -- end");
}
