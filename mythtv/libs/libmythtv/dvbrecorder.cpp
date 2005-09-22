/*
 *  Class DVBRecorder
 *
 *  Copyright (C) Kenneth Aafloy 2003
 *
 *  Description:
 *      Has the responsibility of opening the Demux device and reading
 *      data from it. Code for controlling which of the mpeg2 streams
 *      from the DVB device gets through. In ProcessData there is
 *      a 'map builder' which saves information about the stream
 *      to the database.
 *
 *  Author(s):
 *      Isaac Richards
 *          - Wrote the original class this work derived from.
 *      Kenneth Aafloy (ke-aa at frisurf.no)
 *          - Rewritten Recording Functions.
 *          - Moved PID handling here and rewritten.
 *      Ben Bucksch
 *          - Developed the original implementation.
 *      Dave Chapman (dave at dchapman.com)
 *          - The dvbstream library, which some code,
 *            in ::StartRecording, is based upon.
 *      Martin Smith (martin at spamcop.net)
 *          - The signal quality monitor
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

// C includes
#include <pthread.h>
#include <fcntl.h>
#include <unistd.h>

// C++ includes
#include <iostream>
using namespace std;

// MythTV includes
#include "RingBuffer.h"
#include "programinfo.h"
#include "mpegtables.h"
#include "iso639.h"

// MythTV DVB includes
#include "transform.h"
#include "dvbtypes.h"
#include "dvbdev.h"
#include "dvbchannel.h"
#include "dvbrecorder.h"

// AVLib/FFMPEG includes
#include "../libavcodec/avcodec.h"
#include "../libavformat/avformat.h"
#include "../libavformat/mpegts.h"

const int DVBRecorder::PMT_PID = 0x20; ///< PID for rewritten PMT
const int DVBRecorder::TSPACKETS_BETWEEN_PSIP_SYNC = 2000;
const int DVBRecorder::POLL_INTERVAL        =  50; // msec
const int DVBRecorder::POLL_WARNING_TIMEOUT = 500; // msec

#define RECORD(args...) \
    VERBOSE(VB_RECORD, QString("DVB#%1 Rec: ") \
            .arg(_card_number_option) << args);

#define RECWARN(args...) \
    VERBOSE(VB_GENERAL, QString("DVB#%1 Rec: WARNING - ") \
            .arg(_card_number_option) << args);

#define RECERR(args...) \
    VERBOSE(VB_IMPORTANT, QString("DVB#%1 Rec: ERROR - ") \
            .arg(_card_number_option) << args);

#define RECENO(args...) \
    VERBOSE(VB_IMPORTANT, \
            QString("DVB#%1 Rec: ERROR - ").arg(_card_number_option) \
            << args << endl \
            << QString("          (%1) ").arg(errno) << strerror(errno));

DVBRecorder::DVBRecorder(DVBChannel* advbchannel)
    : DTVRecorder(),
      // Options set in SetOption()
      _card_number_option(0), _record_transport_stream_option(false),
      _hw_decoder_option(false),
      // DVB stuff
      dvbchannel(advbchannel),
      // PS recorder stuff
      _ps_rec_audio_id(0xC0), _ps_rec_video_id(0xE0),
      // Output stream info
      _pat(NULL), _pmt(NULL), _next_pmt_version(0),
      _ts_packets_until_psip_sync(0),
      // Input Misc
      _reset_pid_filters(true),
      // Locking
      _pid_read_lock(true),
      _pid_change_lock(true),
      // Statistics
      _continuity_error_count(0), _stream_overflow_count(0),
      _bad_packet_count(0)
{
    bzero(_ps_rec_buf, sizeof(unsigned char) * 3);

    bzero(&_polls, sizeof(struct pollfd));
    _polls.fd = _stream_fd;

    _buffer_size = MPEG_TS_PKT_SIZE * 50;
    _buffer = new unsigned char[_buffer_size];
    bzero(_buffer, _buffer_size);
}

DVBRecorder::~DVBRecorder()
{
    if (_stream_fd >= 0)
        Close();

    if (_buffer)
        delete[] _buffer;

    SetPAT(NULL);
    SetPMT(NULL);
}

void DVBRecorder::SetOption(const QString &name, int value)
{
    if (name == "cardnum")
        _card_number_option = value;
    else if (name == "hw_decoder")
        _hw_decoder_option = (value == 1);
    else if (name == "recordts")
        _record_transport_stream_option = (value == 1);
    else
        DTVRecorder::SetOption(name, value);
}

void DVBRecorder::SetOptionsFromProfile(RecordingProfile*, 
                                        const QString &videodev,
                                        const QString&, const QString&, int)
{
    SetOption("cardnum", videodev.toInt());
    DTVRecorder::SetOption("tvformat", gContext->GetSetting("TVFormat"));
}

void DVBRecorder::SetPMTObject(const PMTObject *_pmt)
{
    QMutexLocker read_lock(&_pid_read_lock);
    QMutexLocker change_lock(&_pid_change_lock);
    RECORD("SetPMTObject()");
    _input_pmt = *_pmt;

    AutoPID();

    /* Rev the PMT version since PIDs are changing */
    _next_pmt_version = (_next_pmt_version + 1) & 0x1f;
    _ts_packets_until_psip_sync = 0;

    dvbchannel->SetCAPMT(&_input_pmt);

    _reset_pid_filters = true;

    _frames_written_count = 0;
    bzero(_ps_rec_buf, sizeof(unsigned char) * 3);
}

bool DVBRecorder::Open(void)
{
    if (_stream_fd >= 0)
    {
        RECWARN("Card already open");
        return true;
    }

    _stream_fd = open(dvbdevice(DVB_DEV_DVR,_card_number_option),
                      O_RDONLY | O_NONBLOCK);
    if (_stream_fd < 0)
    {
        RECENO("Failed to open DVB device");
        return false;
    }

    _polls.fd = _stream_fd;
    _polls.events = POLLIN;
    _polls.revents = 0;

    connect(dvbchannel, SIGNAL(UpdatePMTObject(const PMTObject *)),
            this, SLOT(SetPMTObject(const PMTObject *)));

    RECORD(QString("Card opened successfully (using %1 mode).")
           .arg(_record_transport_stream_option ? "TS" : "PS"));

    dvbchannel->RecorderStarted();

    return true;
}

void DVBRecorder::Close(void)
{
    if (_stream_fd < 0)
        return;

    RECORD("Closing DVB recorder");

    CloseFilters();

    if (_stream_fd >= 0)
        close(_stream_fd);

    _stream_fd = -1;
    _polls.fd = -1;
}

void DVBRecorder::CloseFilters(void)
{
    QMutexLocker read_lock(&_pid_read_lock);
    QMutexLocker change_lock(&_pid_change_lock);
    for(unsigned int i=0; i<_pid_filters.size(); i++)
        if (_pid_filters[i] >= 0)
            close(_pid_filters[i]);
    _pid_filters.clear();

    pid_ipack_t::iterator iter = _ps_rec_pid_ipack.begin();
    for (;iter != _ps_rec_pid_ipack.end(); iter++)
    {
        if ((*iter).second != NULL)
        {
            free_ipack((*iter).second);
            free((void*)(*iter).second);
        }
    }
    _ps_rec_pid_ipack.clear();
}

void DVBRecorder::OpenFilters(uint16_t pid, ES_Type type,
                              dmx_pes_type_t pes_type)
{
    QMutexLocker read_lock(&_pid_read_lock);
    QMutexLocker change_lock(&_pid_change_lock);
    RECORD(QString("Adding pid %1 (0x%2)").arg(pid).arg((int)pid,0,16));

    if (pid < 0x10 || pid > 0x1fff)
        RECWARN(QString("PID value (%1) is outside DVB specification.\n"
                        "\t\t\tPerhaps this is an ATSC stream?").arg(pid));

    struct dmx_pes_filter_params params;
    memset(&params, 0, sizeof(params));
    params.input = DMX_IN_FRONTEND;
    params.output = DMX_OUT_TS_TAP;
    params.flags = DMX_IMMEDIATE_START;
    params.pid = pid;
    params.pes_type = pes_type;

    int fd_tmp = open(dvbdevice(DVB_DEV_DEMUX,_card_number_option), O_RDWR);

    if (fd_tmp < 0)
    {
        RECENO(QString("Could not open demux device."));
        return;
    }

    if (ioctl(fd_tmp, DMX_SET_PES_FILTER, &params) < 0)
    {
        close(fd_tmp);

        RECENO(QString("Failed to set demux filter."));
        return;
    }

    _pid_filters.push_back(fd_tmp);

    if (_record_transport_stream_option)
    {
        _ps_rec_pid_ipack[pid] = NULL;
    }
    else
    {
        ipack* ip = (ipack*)malloc(sizeof(ipack));
        if (ip == NULL)
        {
            RECERR(QString("Failed to allocate ipack."));
            return;
        }

        switch (type)
        {
            case ES_TYPE_VIDEO_MPEG1:
            case ES_TYPE_VIDEO_MPEG2:
                init_ipack(ip, 2048, ProcessDataPS);
                ip->replaceid = _ps_rec_video_id++;
                break;

            case ES_TYPE_AUDIO_MPEG1:
            case ES_TYPE_AUDIO_MPEG2:
                init_ipack(ip, 65535, ProcessDataPS); /* don't repack PES */
                ip->replaceid = _ps_rec_audio_id++;
                break;

            case ES_TYPE_AUDIO_AC3:
                init_ipack(ip, 65535, ProcessDataPS); /* don't repack PES */
                ip->priv_type=PRIV_TS_AC3;
                break;

            case ES_TYPE_SUBTITLE:
            case ES_TYPE_TELETEXT:
                init_ipack(ip, 65535, ProcessDataPS); /* don't repack PES */
                ip->priv_type=PRIV_DVB_SUB;
                break;

            default:
                init_ipack(ip, 2048, ProcessDataPS);
                break;
        }

        ip->data = (void*)this;
        _ps_rec_pid_ipack[pid] = ip;
    }

    _continuity_count[pid] = -1;
}

bool DVBRecorder::SetDemuxFilters(void)
{
    QMutexLocker read_lock(&_pid_read_lock);
    QMutexLocker change_lock(&_pid_change_lock);
    CloseFilters();

    _continuity_count.clear();
    _encrypted_pid.clear();
    _payload_start_seen.clear();
    data_found = false;
    _wait_for_keyframe = _wait_for_keyframe_option;
    keyframe_found = false;

    _ps_rec_audio_id = 0xC0;
    _ps_rec_video_id = 0xE0;

    // Record all streams flagged for recording
    bool need_pcr_pid = true;
    QValueList<ElementaryPIDObject>::const_iterator es;
    for (es = _input_pmt.Components.begin();
         es != _input_pmt.Components.end(); ++es)
    {
        if (!(*es).Record)
            continue;

        int pid = (*es).PID;
        dmx_pes_type_t pes_type;
            
        if (_hw_decoder_option)
        {
            switch ((*es).Type)
            {
                case ES_TYPE_AUDIO_MPEG1:
                case ES_TYPE_AUDIO_MPEG2:
                    pes_type = DMX_PES_AUDIO;
                    break;
                case ES_TYPE_VIDEO_MPEG1:
                case ES_TYPE_VIDEO_MPEG2:
                    pes_type = DMX_PES_VIDEO;
                    break;
                case ES_TYPE_TELETEXT:
                    pes_type = DMX_PES_TELETEXT;
                    break;
                case ES_TYPE_SUBTITLE:
                    pes_type = DMX_PES_SUBTITLE;
                    break;
                default:
                    pes_type = DMX_PES_OTHER;
                    break;
            }
        }
        else
            pes_type = DMX_PES_OTHER;

        OpenFilters(pid, (*es).Type, pes_type);

        if (_hw_decoder_option)
        {
            // Set PCRPID if it's not the same as video
            if ((pes_type == DMX_PES_VIDEO) &&
                (pid != _input_pmt.PCRPID) &&
                (_input_pmt.PCRPID != 0))
            {
                OpenFilters(_input_pmt.PCRPID, ES_TYPE_UNKNOWN, DMX_PES_PCR);
            }
        }
        else if (pid == _input_pmt.PCRPID)
            need_pcr_pid = false;
    }

    if (!_hw_decoder_option && need_pcr_pid && (_input_pmt.PCRPID != 0))
        OpenFilters(_input_pmt.PCRPID, ES_TYPE_UNKNOWN, DMX_PES_OTHER);


    if (_pid_filters.size() == 0 && _ps_rec_pid_ipack.size() == 0)
    {        
        RECWARN("Recording will not commence until a PID is set.");
        return false;
    }
    return true;
}

/*
 *  Process PMT and decide which components should be recorded
 */
void DVBRecorder::AutoPID(void)
{
    QMutexLocker read_lock(&_pid_read_lock);
    QMutexLocker change_lock(&_pid_change_lock);
    _videoPID.clear();

    RECORD(QString("AutoPID for ServiceID=%1, PCRPID=%2 (0x%3)")
           .arg(_input_pmt.ServiceID).arg(_input_pmt.PCRPID)
           .arg(((uint)_input_pmt.PCRPID),0,16));

    // Wanted languages:
    //QStringList Languages = iso639_get_language_list();

    // Wanted stream types:
    QValueList<ES_Type> StreamTypes;
    StreamTypes += ES_TYPE_VIDEO_MPEG1;
    StreamTypes += ES_TYPE_VIDEO_MPEG2;
    StreamTypes += ES_TYPE_AUDIO_MPEG1;
    StreamTypes += ES_TYPE_AUDIO_MPEG2;
    StreamTypes += ES_TYPE_AUDIO_AC3;
    if (_record_transport_stream_option)
    {
        // The following types are only supported with TS recording
        StreamTypes += ES_TYPE_AUDIO_DTS;
        StreamTypes += ES_TYPE_AUDIO_AAC;
        StreamTypes += ES_TYPE_TELETEXT;
        StreamTypes += ES_TYPE_SUBTITLE;
    }

    QMap<ES_Type, bool> flagged;
    QValueList<ElementaryPIDObject>::Iterator es;
    for (es = _input_pmt.Components.begin();
         es != _input_pmt.Components.end(); ++es)
    {
        if (!StreamTypes.contains((*es).Type))
        {
            // Type not wanted
            continue;
        }

        if (((*es).Type == ES_TYPE_AUDIO_MPEG1) ||
            ((*es).Type == ES_TYPE_AUDIO_MPEG2) ||
            ((*es).Type == ES_TYPE_AUDIO_AC3))
        {
            bool ignore = false;

            // Check for audio descriptors
            DescriptorList::Iterator dit;
            for (dit = (*es).Descriptors.begin();
                 dit != (*es).Descriptors.end(); ++dit)
            {
                // Check for "Hearing impaired" or 
                // "Visual impaired commentary" stream
                if (((*dit).Data[0] == 0x0A) &&
                    ((*dit).Data[5] & 0xFE == 0x02))
                {
                    ignore = true;
                    break;
                }
            }

            if (ignore)
                continue; // Ignore this stream
        }

        if (_hw_decoder_option)
        {
            // Limit hardware decoders to one A/V stream
            switch ((*es).Type)
            {
                case ES_TYPE_VIDEO_MPEG1:
                case ES_TYPE_VIDEO_MPEG2:
                    if (flagged.contains(ES_TYPE_VIDEO_MPEG1) ||
                        flagged.contains(ES_TYPE_VIDEO_MPEG2))
                    {
                        continue; // Ignore this stream
                    }
                    break;

                case ES_TYPE_AUDIO_MPEG1:
                case ES_TYPE_AUDIO_MPEG2:
                    if (flagged.contains(ES_TYPE_AUDIO_MPEG1) ||
                        flagged.contains(ES_TYPE_AUDIO_MPEG2))
                    {
                        continue; // Ignore this stream
                    }
                    break;

                default:
                    break;
            }
        }

        //if (Languages.isEmpty() // No specific language wanted
        //    || (*es).Language.isEmpty() // Component has no language
        //    || Languages.contains((*es).Language)) // This language is wanted!
        {
            (*es).Record = true;
            flagged[(*es).Type] = true;
        }
    }

    for (es = _input_pmt.Components.begin();
         es != _input_pmt.Components.end(); ++es)
    {
        if (StreamTypes.contains((*es).Type) && !flagged.contains((*es).Type))
        {
            // We want this stream type but no component was flagged
            (*es).Record = true;
        }

        if ((*es).Record)
        {
            RECORD(QString("AutoPID selecting PID %1 (0x%2), %3")
                   .arg((*es).PID).arg((*es).PID,0,16)
                   .arg((*es).Description));

            switch ((*es).Type)
            {
                case ES_TYPE_VIDEO_MPEG1:
                case ES_TYPE_VIDEO_MPEG2:
                    _videoPID[(*es).PID] = true;
                    break;

                default:
                    // Do nothing
                    break;
            }
        }
        else
        {
            RECORD(QString("AutoPID skipping PID %1 (0x%2), %3")
                   .arg((*es).PID).arg((*es).PID,0,16).arg((*es).Description));
        }
    }

    RECORD(QString("AutoPID Complete - PAT/PMT Loaded for service"));

    if (_input_pmt.FTA())
        RECORD(QString("Service is FTA"))
    else
        RECORD(QString("Service has Conditional Access"))
}

void DVBRecorder::StartRecording(void)
{
    if (!Open())
    {
        _error = true;
        return;
    }

    _continuity_error_count = 0;
    _stream_overflow_count = 0;
    _bad_packet_count = 0;

    _request_recording = true;
    _recording = true;

    SetPAT(NULL);
    SetPMT(NULL);
    _ts_packets_until_psip_sync = 0;

    MythTimer t;
    t.start();
    while (_request_recording && !_error)
    {
        if (PauseAndWait())
            continue;

        if (_stream_fd < 0)
        {
            usleep(50);
            continue;
        }

        if (_reset_pid_filters)
        {
            RECORD("Resetting Demux Filters");
            if (SetDemuxFilters())
            {
                CreatePAT();
                CreatePMT();
            }
            _reset_pid_filters = false;
        }

        int ret;
        do
            ret = poll(&_polls, 1, POLL_INTERVAL);
        while (!request_pause && (_stream_fd >= 0) &&
               ((-1 == ret) && ((EAGAIN == errno) || (EINTR == errno))));

        if (request_pause || _stream_fd < 0)
            continue;

        if (ret == 0 && t.elapsed() > POLL_WARNING_TIMEOUT)
        {
            RECWARN(QString("No data from card in %1 milliseconds.")
                    .arg(POLL_INTERVAL));
        }
        else if (ret == 1 && _polls.revents & POLLIN)
        {
            if (t.restart() > POLL_WARNING_TIMEOUT)
                RECWARN(QString("Got data from card after %1 ms.")
                        .arg(POLL_WARNING_TIMEOUT));

            ReadFromDMX();
        }
        else if ((ret < 0) || (ret == 1 && _polls.revents & POLLERR))
            RECENO("Poll failed while waiting for data.");
    }

    Close();

    FinishRecording();

    _recording = false;
}

void DVBRecorder::ReadFromDMX(void)
{
    int cardnum = _card_number_option;
    int readsz = 1;
    unsigned char *pktbuf;

    while (readsz > 0)
    {
        readsz = read(_stream_fd, _buffer, _buffer_size);
        if (readsz < 0)
        {
            if (errno == EOVERFLOW)
            {
                ++_stream_overflow_count;
                RECORD("DVB Buffer overflow error detected on read");
                break;
            }

            if (errno == EAGAIN)
                break;
            RECENO("Error reading from DVB device.");
            break;
        } else if (readsz == 0)
            break;

        if (readsz % MPEG_TS_PKT_SIZE)
        {
            RECERR("Incomplete packet received.");
            readsz = readsz - (readsz % MPEG_TS_PKT_SIZE);
        }

        int pkts = readsz / MPEG_TS_PKT_SIZE;
        int curpkt = 0;

        _pid_read_lock.lock();
        while (curpkt < pkts)
        {
            if (data_found == false)
            {
                GENERAL("Data read from DMX - "
                        "This is for debugging with transform.c");
                data_found = true;
            }

            pktbuf = _buffer + (curpkt * MPEG_TS_PKT_SIZE);
            curpkt++;

            int pes_offset = 0;
            int pid = ((pktbuf[1]&0x1f) << 8) | pktbuf[2];
            uint8_t scrambling = (pktbuf[3] >> 6) & 0x03;
            uint8_t cc = pktbuf[3] & 0xf;
            uint8_t content = (pktbuf[3] & 0x30) >> 4;

            if (pktbuf[1] & 0x80)
            {
                RECORD("Packet dropped due to uncorrectable error.");
                ++_bad_packet_count;
                continue;
            }

            if (scrambling)
            {
                if (!_encrypted_pid[pid])
                {
                    RECORD(QString("PID %1 is encrypted, ignoring").arg(pid));
                    _encrypted_pid[pid] = true;
                }
                continue; // Drop encrypted TS packet
            }

            if (_encrypted_pid[pid])
            {
                RECORD(QString("PID %1 is no longer encrypted").arg(pid));
                _encrypted_pid[pid] = false;
            }
            if (content & 0x1)
            {
                 if (_continuity_count[pid] < 0)
                     _continuity_count[pid] = cc;
                 else
                 {
                     _continuity_count[pid] = (_continuity_count[pid]+1) & 0xf;
                     if (_continuity_count[pid] != cc)
                     {
                         RECORD(QString("PID %1 _continuity_count %2 cc %3")
                                .arg(pid).arg(_continuity_count[pid]).arg(cc));
                         _continuity_count[pid] = cc;
                         ++_continuity_error_count;
                     }
                 }
            }

            if (_record_transport_stream_option)
            {   // handle TS recording
                if (_videoPID[pid])
                {
                    // Check for keyframe
                    const TSPacket *pkt =
                        reinterpret_cast<const TSPacket*>(pktbuf);
                    FindKeyframes(pkt);
                }

                // Sync recording start to first keyframe
                if (_wait_for_keyframe && !_keyframe_seen)
                    continue;
                if (!keyframe_found)
                {
                    keyframe_found = true;
                    RECORD(QString("Found first keyframe"));
                }

                // Sync streams to the first Payload Unit Start Indicator
                // _after_ first keyframe iff _wait_for_keyframe is true
                if (!_payload_start_seen[pid])
                {
                    if ((pktbuf[1] & 0x40) == 0)
                        continue; // not payload start - drop packet

                    RECORD(QString("Found Payload Start for PID %1").arg(pid));
                    _payload_start_seen[pid] = true;
                }

                LocalProcessDataTS(pktbuf, MPEG_TS_PKT_SIZE);
            }
            else
            {   // handle PS recording
                ipack *ip = _ps_rec_pid_ipack[pid];
                if (ip == NULL)
                    continue;

                ip->ps = 1;

                if ((pktbuf[1] & 0x40) && (ip->plength == MMAX_PLENGTH-6))
                {
                    ip->plength = ip->found-6;
                    ip->found = 0;
                    send_ipack(ip);
                    reset_ipack(ip);
                }

                if (content & 0x2)
                    pes_offset = pktbuf[4] + 1;

                if (pes_offset > 183)
                    continue;

                instant_repack(pktbuf + 4 + pes_offset,
                               MPEG_TS_PKT_SIZE - 4 - pes_offset, ip);
            }
        }
        _pid_read_lock.unlock();
    }
}

#define SEQ_START     0x000001B3
#define GOP_START     0x000001B8
#define PICTURE_START 0x00000100
#define SLICE_MIN     0x00000101
#define SLICE_MAX     0x000001af

void DVBRecorder::ProcessDataPS(unsigned char *buffer, int len, void *priv)
{
    ((DVBRecorder*)priv)->LocalProcessDataPS(buffer, len);
}

void DVBRecorder::LocalProcessDataPS(unsigned char *buffer, int len)
{
    if (buffer[0] != 0x00 || buffer[1] != 0x00 || buffer[2] != 0x01)
    {
        if (!_wait_for_keyframe)
            ringBuffer->Write(buffer, len);
        return;
    }

    if (buffer[3] >= VIDEO_STREAM_S && buffer[3] <= VIDEO_STREAM_E)
    {
        int pos = 8 + buffer[8];
        int datalen = len - pos;

        unsigned char *bufptr = &buffer[pos];
        uint state = 0xFFFFFFFF;
        uint state_byte = 0;
        int prvcount = -1;

        while (bufptr < &buffer[pos] + datalen)
        {
            if (++prvcount < 3)
                state_byte = _ps_rec_buf[prvcount];
            else
                state_byte = *bufptr++;

            if (state != 0x000001)
            {
                state = ((state << 8) | state_byte) & 0xFFFFFF;
                continue;
            }

            state = ((state << 8) | state_byte) & 0xFFFFFF;
            if (state >= SLICE_MIN && state <= SLICE_MAX)
                continue;

            if (state == SEQ_START)
                _wait_for_keyframe = false;

            if (GOP_START == state)
            {
                long long startpos = ringBuffer->GetFileWritePosition();
                        
                if (!_position_map.contains(_frames_written_count))
                {
                    _position_map_delta[_frames_written_count] = startpos;
                    _position_map[_frames_written_count] = startpos;

                    if (curRecording &&
                        ((_position_map_delta.size() % 30) == 0))
                    {
                        curRecording->SetPositionMapDelta(
                            _position_map_delta,
                            MARK_GOP_BYFRAME);
                        curRecording->SetFilesize(startpos);
                        _position_map_delta.clear();
                    }
                }
            }
            else if (PICTURE_START == state)
                _frames_written_count++;
        }
    }
    memcpy(_ps_rec_buf, &buffer[len - 3], 3);

    if (!_wait_for_keyframe)
        ringBuffer->Write(buffer, len);
}

void DVBRecorder::LocalProcessDataTS(unsigned char *buffer, int len)
{
    QMutexLocker read_lock(&_pid_read_lock);
    if (_ts_packets_until_psip_sync == 0)
    {
        if (_pat && _pmt)
        {
            ringBuffer->Write(_pat->tsheader()->data(), TSPacket::SIZE);
            ringBuffer->Write(_pmt->tsheader()->data(), TSPacket::SIZE);
            _ts_packets_until_psip_sync = TSPACKETS_BETWEEN_PSIP_SYNC;
        }
    }
    else
        _ts_packets_until_psip_sync--;

    ringBuffer->Write(buffer,len);
}

void DVBRecorder::Reset(void)
{
    DTVRecorder::Reset();

    if (curRecording)
    {
        curRecording->ClearPositionMap(MARK_GOP_BYFRAME);
    }
}

void DVBRecorder::SetPAT(ProgramAssociationTable *new_pat)
{
    QMutexLocker read_lock(&_pid_read_lock);
    QMutexLocker change_lock(&_pid_change_lock);

    ProgramAssociationTable *old_pat = _pat;
    _pat = new_pat;
    if (old_pat)
        delete old_pat;
}

void DVBRecorder::SetPMT(ProgramMapTable *new_pmt)
{
    QMutexLocker read_lock(&_pid_read_lock);
    QMutexLocker change_lock(&_pid_change_lock);

    ProgramMapTable *old_pmt = _pmt;
    _pmt = new_pmt;
    if (old_pmt)
        delete old_pmt;
}

void DVBRecorder::CreatePAT(void)
{
    QMutexLocker read_lock(&_pid_read_lock);
    int next_cc = 0;
    if (_pat)
        next_cc = (_pat->tsheader()->ContinuityCounter() + 1) & 0x0F;

    uint tsid = 1; // transport stream id
    vector<uint> pnum, pid;
    pnum.push_back(1); // program number / service id
    pid.push_back(PMT_PID);

    SetPAT(ProgramAssociationTable::Create(tsid, next_cc, pnum, pid));
}

void DVBRecorder::CreatePMT(void)
{
    QMutexLocker read_lock(&_pid_read_lock);
    int pmt_cc = 0;
    if (_pmt)
        pmt_cc = (_pmt->tsheader()->ContinuityCounter() + 1) & 0x0F;

    TSPacket pkt;
    uint8_t *ts_packet = pkt.data();
    memset(ts_packet, 0xFF, TSPacket::SIZE);

    ts_packet[0] = 0x47;                            // sync byte
    ts_packet[1] = 0x40 | ((PMT_PID >> 8) & 0x1F);  // payload start & PID
    ts_packet[2] = PMT_PID & 0xFF;                  // PID
    // scrambling, adaptation & continuity counter
    ts_packet[3] = 0x10 | pmt_cc; 
    ts_packet[4] = 0x00;                            // pointer field

    ++pmt_cc &= 0x0F;   // inc. continuity counter
    uint8_t *pmt_data = ts_packet + 5;
    int p = 0;

    pmt_data[p++] = PMT_TID; // table ID
    pmt_data[p++] = 0xB0;    // section syntax indicator
    p++;                // section length (set later)
    pmt_data[p++] = 0;       // program number (ServiceID)
    pmt_data[p++] = 1;       // program number (ServiceID)
    pmt_data[p++] = 0xC1 + (_next_pmt_version << 1); // Version + Current/Next
    pmt_data[p++] = 0;       // Current Section
    pmt_data[p++] = 0;       // Last Section
    pmt_data[p++] = (_input_pmt.PCRPID >> 8) & 0x1F;
    pmt_data[p++] = _input_pmt.PCRPID & 0xFF;

    // Write descriptors
    int program_info_length = 0;
    DescriptorList::Iterator dit;
    for (dit = _input_pmt.Descriptors.begin();
         dit != _input_pmt.Descriptors.end(); ++dit)
    {
        int len = (*dit).Length;
        memcpy(&pmt_data[p + 2 + program_info_length], (*dit).Data, len);
        program_info_length += len;
    }

    // Program info length
    pmt_data[p++] = (program_info_length >> 8) & 0x0F;
    pmt_data[p++] = program_info_length & 0xFF;
    p += program_info_length;

    QValueList<ElementaryPIDObject>::Iterator es;
    for (es = _input_pmt.Components.begin();
         es != _input_pmt.Components.end(); ++es)
    {
        if (!(*es).Record)
            continue;

        // Normalize stream type to make ffmpeg happy
        uint8_t stream_type;
        switch ((*es).Type)
        {
            case ES_TYPE_VIDEO_MPEG1:
                stream_type = STREAM_TYPE_VIDEO_MPEG1;
                break;
            case ES_TYPE_VIDEO_MPEG2:
                stream_type = STREAM_TYPE_VIDEO_MPEG2;
                break;
            case ES_TYPE_VIDEO_MPEG4:
                stream_type = STREAM_TYPE_VIDEO_MPEG4;
                break;
            case ES_TYPE_VIDEO_H264:
                stream_type = STREAM_TYPE_VIDEO_H264;
                break;

            case ES_TYPE_AUDIO_MPEG1:
                stream_type = STREAM_TYPE_AUDIO_MPEG1;
                break;
            case ES_TYPE_AUDIO_MPEG2:
                stream_type = STREAM_TYPE_AUDIO_MPEG2;
                break;
            case ES_TYPE_AUDIO_AC3:
                stream_type = STREAM_TYPE_AUDIO_AC3;
                break;
            case ES_TYPE_AUDIO_DTS:
                stream_type = STREAM_TYPE_AUDIO_DTS;
                break;
            case ES_TYPE_AUDIO_AAC:
                stream_type = STREAM_TYPE_AUDIO_AAC;
                break;

            default:
                stream_type = (*es).Orig_Type;
                break;
        }
        pmt_data[p++] = stream_type;

        pmt_data[p++] = ((*es).PID >> 8) & 0x1F;
        pmt_data[p++] = (*es).PID & 0xFF;

        // Write descriptors
        int es_info_length = 0;
        DescriptorList::Iterator dit;
        for (dit = (*es).Descriptors.begin();
             dit != (*es).Descriptors.end(); ++dit)
        {
            int len = (*dit).Length;
            memcpy(&pmt_data[p + 2 + es_info_length], (*dit).Data, len);
            es_info_length += len;
        }

        // ES info length
        pmt_data[p++] = (es_info_length >> 8) & 0x0F;
        pmt_data[p++] = es_info_length & 0xFF;
        p += es_info_length;
    }

    pmt_data[2] = p + 4 - 3; // section length

    unsigned long crc = mpegts_crc32(pmt_data, p);
    pmt_data[p++] = (crc >> 24) & 0xFF;
    pmt_data[p++] = (crc >> 16) & 0xFF;
    pmt_data[p++] = (crc >> 8) & 0xFF;
    pmt_data[p++] = crc & 0xFF;
    
    SetPMT(new ProgramMapTable(PSIPTable(pkt)));
}

void DVBRecorder::DebugTSHeader(unsigned char* buffer, int len)
{
    (void) len;

    uint8_t sync = buffer[0];
    uint8_t transport_error = (buffer[1] & 0x80) >> 7;
    uint8_t payload_start = (buffer[1] & 0x40) >> 6;
    uint16_t pid = (buffer[1] & 0x1F) << 8 | buffer[2];
    uint8_t transport_scrambled = (buffer[3] & 0xB0) >> 6;
    uint8_t adaptation_control = (buffer[3] & 0x30) >> 4;
    uint8_t counter = buffer[3] & 0x0F;

    int pos=4;
    if (adaptation_control == 2 || adaptation_control == 3)
    {
        unsigned char adaptation_length;
        adaptation_length = buffer[pos++];
        pos += adaptation_length;
    }

    QString debugmsg =
        QString("sync: %1 err: %2 paystart: %3 "
                "pid: %4 enc: %5 adaptation: %6 counter: %7")
        .arg(sync, 2, 16)
        .arg(transport_error)
        .arg(payload_start)
        .arg(pid)
        .arg(transport_scrambled)
        .arg(adaptation_control)
        .arg(counter);

    const TSPacket *pkt = reinterpret_cast<const TSPacket*>(&buffer[0]);
    FindKeyframes(pkt);

    int cardnum = _card_number_option;
    GENERAL(debugmsg);
}
