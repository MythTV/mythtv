#include <iostream>
#include <pthread.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/time.h>
#include <time.h>
#include <qbuffer.h>
#include <sys/poll.h>

using namespace std;

#include "dvbrecorder.h"
#include "RingBuffer.h"
#include "dvbchannel.h"

#ifdef USING_DVB
    #include "dvbdev.h"
#endif

#define MAX_PIDS 50

// Generic MPEG
#include "programinfo.h"
extern "C" {
#include "../libavcodec/avcodec.h"
#include "../libavformat/avformat.h"
extern AVInputFormat mpegps_demux;
}



DVBRecorder::DVBRecorder(const DVBChannel* dvbchannel)
           : RecorderBase()
{
    isopen = false;
    dvr_fd = -1;
    cardnum = 0;
    was_paused = true;
    pid_changed = true;
    channel = dvbchannel;

    // generic MPEG code from MpegRecorder
    paused = false;
    mainpaused = false;
    recording = false;
    framesWritten = 0;
    keyframedist = 15;
    gopset = false;
    prev_gop_save_pos = -1;
}

DVBRecorder::~DVBRecorder()
{
    Close();
}

void DVBRecorder::SetOption(const QString &name, int value)
{
    if (name == "cardnum")
        cardnum = value;
    else
        RecorderBase::SetOption(name, value);
}

/* This Recorder fetches the PIDs itself when the recording starts,
   but maybe the channel gets changed during a recording?
   We can't rely on this SetOption either, because that's called by the
   Channel when the channel changes, and when the initial channel is
   set, this Recorder doesn't exist yet. */
void DVBRecorder::SetPID(const vector<int>& some_pids)
{
  pid = some_pids;
  pid_changed = true;
}

bool DVBRecorder::Open()
{
    if (isopen)
        return true;

#ifdef USING_DVB
    dvr_fd = open(devicenodename(dvbdev_dvr,cardnum), O_RDONLY|O_NONBLOCK);
    if(dvr_fd < 0)
    {
        cerr << "Can't open DVB device "
             << devicenodename(dvbdev_dvr,cardnum) << endl;
        return false;
    }
    return true;
#else
    cerr << "DVB support not compiled in" << endl;
    return false;
#endif
}

void DVBRecorder::Close()
{
    if (!isopen)
        return;
    if (dvr_fd > 0)
        close(dvr_fd);
}

void DVBRecorder::StartRecording()
{
    if (!SetupRecording())
        return;

#ifdef USING_DVB
    channel->GetPID(pid);

    if (!Open())
        return;

    // Set up buffers
    const int IN_BUFFER_SIZE = TS_SIZE;
    const int OUT_BUFFER_SIZE = 256000;
    unsigned char buffer_raw[IN_BUFFER_SIZE + 1];
    unsigned char buffer_filtered[OUT_BUFFER_SIZE + 1];
    int bytes_read;
    int filtered_len;

    // for ts_to_ps()
    uint16_t pid_array[MAX_PIDS]; // MAX_PIDS is a hack, better use pid.size(),
    ipack ipacks[MAX_PIDS];       // but that may change at any time when the
    int npids = 0;                // channel changes in LiveTV.
    ipack *ipacks_p = ipacks;     // needed for typ-safety?

    // Set up polling of the device
    struct pollfd pfd[1];
    pfd[0].fd = dvr_fd;
    pfd[0].events = POLLIN;

    encoding = true;
    recording = true;

    while (encoding)
    {
        if (paused)
        {
            Close();
            mainpaused = true;
            was_paused = true;
            usleep(50);
            continue;
        }
        else if (was_paused)
        {
            if (Open())
                was_paused = false;
                // XXX need to set mainpaused = false? mpegrecorder doesn't.
            else
            {
                usleep(500);
                continue;
            }
        }
        else if (poll(pfd, 1, 1) && (pfd[0].revents & POLLIN))
        {
            if (paused)
                continue;

            bytes_read = read(dvr_fd, buffer_raw, IN_BUFFER_SIZE);
            if (bytes_read > 0)
            {
                if (pid_changed)
                {
                    // make |pid| suitable for ts_to_ps()
                    npids = pid.size(); //Thread-safety: Use threading locks!
                    if (npids > MAX_PIDS)
                        npids = MAX_PIDS;
                    for (int i = 0; i < npids; i++)
                    {
                        pid_array[i] = pid[i];
                        init_ipack(ipacks + i, IPACKS, ts_to_ps_write_out, 1);
                    }
                    pid_changed = false;

                    cout << "filtering pids:";
                    for (vector_int::iterator j=pid.begin();j !=pid.end(); j++)
                      cout << " " << *j;
                    cout << endl;
                }

                /* This function call fulfills 2 purposes:
                   - Converts MPEG-TS (transport stream) to MPEG-PS (what's
                     usually used in MPEG video files)
                   - Filters out all unneeded program streams.
                     This is redundant for use_ts==false,
                     but required for use_ts==true. */
                ts_to_ps(buffer_raw,
                         pid_array, npids, &ipacks_p,
                         buffer_filtered, OUT_BUFFER_SIZE, &filtered_len);

                if (filtered_len > 0)
                {
                    // write out data to file, also adding seeking
                    ProcessData(buffer_filtered, filtered_len);
                }
            }
            else if (bytes_read < 0)
            {
                cerr << "error reading from DVB device "
                     << devicenodename(dvbdev_dvr,cardnum)
                     << ", error code " << bytes_read << endl;
                continue;
            }
        }
    }

    Close();


    FinishRecording();
#else
    cerr << "DVB support not compiled in" << endl;
#endif

    recording = false;
}

int DVBRecorder::GetVideoFd(void)
{
    return -1;
}




/* ===========================================================================
   Generic MPEG code follows (identical in MpegRecorder, DVBRecorder and co).
   I would have liked to use a common base class for that, but Isaac preferred
   to have all the code for a given class in one file
   =========================================================================== */

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

bool DVBRecorder::SetupRecording()
{
    AVInputFormat *fmt = &mpegps_demux;
    fmt->flags |= AVFMT_NOFILE;

    ic = (AVFormatContext *)av_mallocz(sizeof(AVFormatContext));
    if (!ic)
    {
        cerr << "Couldn't allocate context\n";
        return false;
    }

    QString filename = "blah.mpg";
    char *cfilename = (char *)filename.ascii();
    AVFormatParameters params;

    ic->pb.buffer_size = 256001;
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
        cerr << "Couldn't initialize decoder\n";
        return false;
    } 

    return true;
}

void DVBRecorder::FinishRecording()
{
    if (curRecording && db_lock && db_conn)
    {
        pthread_mutex_lock(db_lock);
        MythContext::KickDatabase(db_conn);
        curRecording->SetPositionMap(positionMap, MARK_GOP_START, db_conn);
        pthread_mutex_unlock(db_lock);
    }
}

void DVBRecorder::ChangeDeinterlacer(int deint_mode)
{
    (void)deint_mode;
}

void DVBRecorder::SetVideoFilters(QString &filters)
{
    (void)filters;
}

void DVBRecorder::Initialize(void)
{
}

#define GOP_START     0x000001B8
#define PICTURE_START 0x00000100
#define SLICE_MIN     0x00000101
#define SLICE_MAX     0x000001af

bool DVBRecorder::PacketHasHeader(unsigned char *buf, int len,
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

void DVBRecorder::ProcessData(unsigned char *buffer, int len)
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
            cerr << "bad stream\n";
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
                    keyframedist = frameNum;
                    gopset = true;
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

void DVBRecorder::StopRecording(void)
{
    encoding = false;
}

void DVBRecorder::Reset(void)
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

void DVBRecorder::Pause(bool clear)
{
    cleartimeonpause = clear;
    mainpaused = false;
    paused = true;
}

void DVBRecorder::Unpause(void)
{
    paused = false;
}

bool DVBRecorder::GetPause(void)
{
    return mainpaused;
}

bool DVBRecorder::IsRecording(void)
{
    return recording;
}

long long DVBRecorder::GetFramesWritten(void)
{
    return framesWritten;
}

void DVBRecorder::TransitionToFile(const QString &lfilename)
{
    ringBuffer->TransitionToFile(lfilename);
}

void DVBRecorder::TransitionToRing(void)
{
    if (curRecording)
    {
        pthread_mutex_lock(db_lock);
        MythContext::KickDatabase(db_conn);
        curRecording->SetPositionMap(positionMap, MARK_GOP_START, db_conn);
        pthread_mutex_unlock(db_lock);

        delete curRecording;
        curRecording = NULL;
    }

    ringBuffer->TransitionToRing();
}

long long DVBRecorder::GetKeyframePosition(long long desired)
{
    long long ret = -1;

    if (positionMap.find(desired) != positionMap.end())
        ret = positionMap[desired];

    return ret;
}

void DVBRecorder::GetBlankFrameMap(QMap<long long, int> &blank_frame_map)
{
    (void)blank_frame_map;
}
