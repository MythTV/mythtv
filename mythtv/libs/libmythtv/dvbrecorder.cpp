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

#include <iostream>
using namespace std;

#include <pthread.h>
#include <fcntl.h>
#include <unistd.h>

#include "RingBuffer.h"
#include "programinfo.h"

#include "transform.h"
#include "dvbtypes.h"
#include "dvbdev.h"
#include "dvbchannel.h"
#include "dvbrecorder.h"

extern "C" {
#include "../libavcodec/avcodec.h"
#include "../libavformat/avformat.h"
#include "../libavformat/mpegts.h"
}

const int DVBRecorder::PMT_PID = 0x10;

DVBRecorder::DVBRecorder(DVBChannel* advbchannel): DTVRecorder()
{
    _card_number_option = 0;
    _reset_pid_filters = true;
    dvbchannel = advbchannel;

    _dvb_on_demand_option = false;
    _software_filter_option = false;
    _record_transport_stream_option = false;

    _software_filter_open = false;

    _continuity_error_count = 0;
    _stream_overflow_count = 0;
    _bad_packet_count = 0;

    _buffer_size = MPEG_TS_PKT_SIZE * 50;
    if ((_buffer = new unsigned char[_buffer_size])) {
        // make valgrind happy, initialize buffer memory
        memset(_buffer, 0xFF, _buffer_size);
    }

    pat_pkt = new uint8_t[188];
    pmt_pkt = new uint8_t[188];
}

DVBRecorder::~DVBRecorder()
{
    if (_stream_fd >= 0)
        Close();

    if (_buffer)
        delete[] _buffer;

    delete [] pat_pkt;
    delete [] pmt_pkt;
}

void DVBRecorder::SetOption(const QString &name, int value)
{
    if (name == "cardnum")
        _card_number_option = value;
    else if (name == "swfilter")
        _software_filter_option = (value == 1);
    else if (name == "recordts")
        _record_transport_stream_option = (value == 1);
#if 0
    else if (name == "signal_monitor_interval")
    {
        signal_monitor_interval = value;

        if (signal_monitor_interval < 0)
            signal_monitor_interval = 0; 
    }
#endif
#if 0
    else if (name == "expire_data_days")
    {
        expire_data_days = value;

        if (expire_data_days < 1)
            expire_data_days = 1;
    }
#endif
    else if (name == "dvb_on_demand")
    {
        _dvb_on_demand_option = value;
    }
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

void DVBRecorder::ChannelChanged(dvb_channel_t& chan)
{
    m_pmt = chan.pmt;

    AutoPID();
    dvbchannel->SetCAPMT(m_pmt);

    _reset_pid_filters = true;

    _frames_written_count = 0;
    memset(prvpkt, 0, 3);
}

bool DVBRecorder::Open()
{
    if (_stream_fd >= 0)
        return true;

    if (_dvb_on_demand_option && dvbchannel->Open())
    {
        // this is required to trigger a re-tune
        dvbchannel->SetChannelByString(dvbchannel->GetCurrentName());
    }

    _stream_fd = open(dvbdevice(DVB_DEV_DVR,_card_number_option), O_RDONLY | O_NONBLOCK);
    if (_stream_fd < 0)
    {
        VERBOSE(VB_IMPORTANT, QString("DVB#%1 ERROR - ").arg(_card_number_option)
                << "Recorder: Failed to open dvb device"
                << endl << QString("          (%1) ").arg(errno) << strerror(errno));
        return false;
    }

    connect(dvbchannel, SIGNAL(ChannelChanged(dvb_channel_t&)),
            this, SLOT(ChannelChanged(dvb_channel_t&)));

    VERBOSE(VB_GENERAL, QString("DVB#%1 Recorder: Card opened successfully (using %2 mode).")
                        .arg(_card_number_option)
                        .arg(_record_transport_stream_option ? "TS" : "PS"));

    dvbchannel->RecorderStarted();

    return true;
}

void DVBRecorder::Close()
{
    if (_stream_fd < 0)
        return;

    VERBOSE(VB_ALL, "Closing DVB recorder");

    CloseFilters();

    if (_stream_fd >= 0)
        close(_stream_fd);

    if (_dvb_on_demand_option && dvbchannel)
        dvbchannel->CloseDVB();

    _stream_fd = -1;
}

void DVBRecorder::CloseFilters()
{
    for(unsigned int i=0; i<_pid_filters.size(); i++)
        if (_pid_filters[i] >= 0)
            close(_pid_filters[i]);
    _pid_filters.clear();

    pid_ipack_t::iterator iter = pid_ipack.begin();
    for (;iter != pid_ipack.end(); iter++)
    {
        if ((*iter).second != NULL)
        {
            free_ipack((*iter).second);
            free((void*)(*iter).second);
        }
    }
    pid_ipack.clear();

    _software_filter_open = false;
}

void DVBRecorder::OpenFilters(uint16_t pid, ES_Type type)
{
    VERBOSE(VB_RECORD, QString("DVB#%1 ").arg(_card_number_option) << QString("Adding pid %1").arg(pid));

    int cardnum = _card_number_option;
    if (pid < 0x10 || pid > 0x1fff)
        WARNING(QString("PID value (%1) is outside DVB specification.")
                        .arg(pid));

    if (!_software_filter_option || !_software_filter_open)
    {
        struct dmx_pes_filter_params params;
        memset(&params, 0, sizeof(params));
        params.input = DMX_IN_FRONTEND;
        params.output = DMX_OUT_TS_TAP;
        params.flags = DMX_IMMEDIATE_START;
        params.pes_type = DMX_PES_OTHER;

        if ( _software_filter_option )
        {
            params.pes_type = DMX_PES_OTHER;
            params.pid = DMX_DONT_FILTER;
        }
        else
        {
            params.pid = pid;
            switch ( type ) 
            {
                case ES_TYPE_VIDEO_MPEG1:
                case ES_TYPE_VIDEO_MPEG2:
                case ES_TYPE_VIDEO_MPEG4:
                case ES_TYPE_VIDEO_H264:
                    params.pes_type = DMX_PES_VIDEO;
                    break;
                case ES_TYPE_AUDIO_MPEG1:
                case ES_TYPE_AUDIO_MPEG2:
                case ES_TYPE_AUDIO_AAC:
                case ES_TYPE_AUDIO_AC3:
                case ES_TYPE_AUDIO_DTS:
                    params.pes_type = DMX_PES_AUDIO;
                    break;
                case ES_TYPE_TELETEXT:
                    params.pes_type = DMX_PES_TELETEXT;
                    break;
                case ES_TYPE_SUBTITLE:
                    params.pes_type = DMX_PES_SUBTITLE;
                    break;
                case ES_TYPE_DATA:
                    params.pes_type = DMX_PES_PCR;
                    break;
                default:
                    params.pes_type = DMX_PES_OTHER;
                    break;
            }
        }

        int fd_tmp = open(dvbdevice(DVB_DEV_DEMUX,_card_number_option), O_RDWR);

        if (fd_tmp < 0)
        {
            ERRNO(QString("Could not open demux device."));
            return;
        }

        if (_software_filter_option)
        {
            GENERAL("Using Software Filtering.");
            _software_filter_open = true;
        }

        if (ioctl(fd_tmp, DMX_SET_PES_FILTER, &params) < 0)
        {
            close(fd_tmp);

            ERRNO(QString("Failed to set demux filter."));
            return;
        }

        _pid_filters.push_back(fd_tmp);
    }

    if (_record_transport_stream_option)
    {
        pid_ipack[pid] = NULL;
    }
    else
    {
        ipack* ip = (ipack*)malloc(sizeof(ipack));
        if (ip == NULL)
        {
            ERROR(QString("Failed to allocate ipack."));
            return;
        }

        switch (type)
        {
            case ES_TYPE_VIDEO_MPEG1:
            case ES_TYPE_VIDEO_MPEG2:
                init_ipack(ip, 2048, ProcessDataPS);
                ip->replaceid = videoid++;
                break;

            case ES_TYPE_AUDIO_MPEG1:
            case ES_TYPE_AUDIO_MPEG2:
                init_ipack(ip, 65535, ProcessDataPS); /* don't repack PES */
                ip->replaceid = audioid++;
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
        pid_ipack[pid] = ip;
    }

    _continuity_count[pid] = 16;
}

void DVBRecorder::SetDemuxFilters()
{

    CloseFilters();

    _continuity_count.clear();
    scrambled.clear();
    pusi_seen.clear();
    data_found = false;
    _wait_for_keyframe = _wait_for_keyframe_option;
    keyframe_found = false;

    audioid = 0xC0;
    videoid = 0xE0;


    // Record all streams flagged for recording
    bool need_pcr_pid = true;
    QValueList<ElementaryPIDObject>::Iterator es;
    for (es = m_pmt.Components.begin(); es != m_pmt.Components.end(); ++es)
    {
        if ((*es).Record)
        {
            OpenFilters((*es).PID, (*es).Type);

            if ((*es).PID == m_pmt.PCRPID)
                need_pcr_pid = false;
        }
    }

    if (_record_transport_stream_option && need_pcr_pid)
        OpenFilters(m_pmt.PCRPID, ES_TYPE_UNKNOWN);


    if (_pid_filters.size() == 0 && pid_ipack.size() == 0)
    {
        VERBOSE(VB_IMPORTANT, QString("DVB#%1 ERROR - ").arg(_card_number_option)
                << "No PIDS set, please correct your channel setup.");
        return;
    }
}

/*
 *  Process PMT and decide which components should be recorded
 */
void DVBRecorder::AutoPID()
{
    int cardnum = _card_number_option;

    isVideo.clear();

    // Wanted languages:
    QStringList Languages = QStringList::split(",", gContext->GetSetting("PreferredLanguages", ""));

    // Wanted stream types:
    QValueList<ES_Type> StreamTypes;
    StreamTypes += ES_TYPE_VIDEO_MPEG1;
    StreamTypes += ES_TYPE_VIDEO_MPEG2;
    StreamTypes += ES_TYPE_AUDIO_MPEG1;
    StreamTypes += ES_TYPE_AUDIO_MPEG2;
    StreamTypes += ES_TYPE_AUDIO_AC3;
    if (_record_transport_stream_option)
    {
        // PS recorder can't handle these
        StreamTypes += ES_TYPE_AUDIO_AAC;
        StreamTypes += ES_TYPE_TELETEXT;
        StreamTypes += ES_TYPE_SUBTITLE;
    }

    QMap<ES_Type, bool> flagged;
    QValueList<ElementaryPIDObject>::Iterator es;
    for (es = m_pmt.Components.begin(); es != m_pmt.Components.end(); ++es)
    {
        if (!StreamTypes.contains((*es).Type))
        {
            // Type not wanted
            continue;
        }

        if (((*es).Type == ES_TYPE_AUDIO_MPEG1)
                || ((*es).Type == ES_TYPE_AUDIO_MPEG2)
                || ((*es).Type == ES_TYPE_AUDIO_AC3))
        {
            bool ignore = false;

            // Check for audio descriptors
            DescriptorList::Iterator dit;
            for (dit = (*es).Descriptors.begin(); dit != (*es).Descriptors.end(); ++dit)
            {
                // Check for "Hearing impaired" or "Visual impaired commentary" stream
                if (((*dit).Data[0] == 0x0A) && ((*dit).Data[5] & 0xFE == 0x02))
                {
                    ignore = true;
                    break;
                }
            }

            if (ignore)
                continue; // Ignore this stream
        }

        if (!_record_transport_stream_option)
        {
            // The MPEG PS decoder in Myth currently only handles one stream
            // of each type, so make sure we don't already have one
            switch ((*es).Type)
            {
                case ES_TYPE_VIDEO_MPEG1:
                case ES_TYPE_VIDEO_MPEG2:
                    if (flagged.contains(ES_TYPE_VIDEO_MPEG1) || flagged.contains(ES_TYPE_VIDEO_MPEG2))
                        continue; // Ignore this stream
                    break;

                case ES_TYPE_AUDIO_MPEG1:
                case ES_TYPE_AUDIO_MPEG2:
                    if (flagged.contains(ES_TYPE_AUDIO_MPEG1) || flagged.contains(ES_TYPE_AUDIO_MPEG2))
                        continue; // Ignore this stream
                    break;

                case ES_TYPE_AUDIO_AC3:
                    if (flagged.contains(ES_TYPE_AUDIO_AC3))
                        continue; // Ignore this stream
                    break;

                default:
                    break;
            }
        }


        if (Languages.isEmpty() // No specific language wanted
            || (*es).Language.isEmpty() // Component has no language
            || Languages.contains((*es).Language)) // This language is wanted!
        {
            (*es).Record = true;
            flagged[(*es).Type] = true;
        }
    }

    for (es = m_pmt.Components.begin(); es != m_pmt.Components.end(); ++es)
    {
        if (StreamTypes.contains((*es).Type) && !flagged.contains((*es).Type))
        {
            // We want this stream type but no component was flagged
            (*es).Record = true;
        }

        if ((*es).Record)
        {
            VERBOSE(VB_RECORD, QString("DVB#%1 ").arg(_card_number_option)
                << QString("AutoPID selecting PID %1, %2").arg((*es).PID).arg((*es).Description));

            switch ((*es).Type)
            {
                case ES_TYPE_VIDEO_MPEG1:
                case ES_TYPE_VIDEO_MPEG2:
                    isVideo[(*es).PID] = true;
                    break;

                default:
                    // Do nothing
                    break;
            }
        }
        else
        {
            VERBOSE(VB_RECORD, QString("DVB#%1 ").arg(_card_number_option)
                << QString("AutoPID skipping PID %1, %2").arg((*es).PID).arg((*es).Description));
        }
    }

    RECORD(QString("AutoPID Complete - PAT/PMT Loaded for service"));

    if (m_pmt.FTA())
        RECORD(QString("Service is FTA"))
    else
        RECORD(QString("Service has Conditional Access"))
}

void DVBRecorder::StartRecording()
{
    int cardnum = _card_number_option;
    if (!Open())
    {
        _error = true;
        return;
    }

    struct pollfd polls;

    polls.fd = _stream_fd;
    polls.events = POLLIN;
    polls.revents = 0;

    _continuity_error_count = 0;
    _stream_overflow_count = 0;
    _bad_packet_count = 0;

    _request_recording = true;
    _recording = true;

    pat_cc = 0x00;
    pmt_cc = 0x00;
    pkts_until_pat_pmt = 0;

    while (_request_recording)
    {
        if (_reset_pid_filters)
        {
            SetDemuxFilters();
            CreatePAT(pat_pkt);
            CreatePMT(pmt_pkt);
            _reset_pid_filters = false;
        }

        if (_request_pause)
        {
            _paused = true;
            pauseWait.wakeAll();
            usleep(50);
            continue;
        }
        else if (_paused)
        {
            _paused = false;
        }

        int ret;
        do 
        {
            ret = poll(&polls, 1, 1000);
        } while ((-1==ret) && ((EAGAIN==errno) || (EINTR==errno)));

        if (ret == 0)
        {
            WARNING("No data from card in 1 second.");
        }
        else if (ret == 1 && polls.revents & POLLIN)
        {
            if (!_request_pause)
                ReadFromDMX();
        }
        else if ((ret < 0) || (ret == 1 && polls.revents & POLLERR))
            ERRNO("Poll failed while waiting for data.");
    }

    Close();

    FinishRecording();

    _recording = false;
}

void DVBRecorder::ReadFromDMX()
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
            ERRNO("Error reading from DVB device.");
            break;
        } else if (readsz == 0)
            break;

        if (readsz % MPEG_TS_PKT_SIZE)
        {
            ERROR("Incomplete packet received.");
            readsz = readsz - (readsz % MPEG_TS_PKT_SIZE);
        }

        int pkts = readsz / MPEG_TS_PKT_SIZE;
        int curpkt = 0;

        while (curpkt < pkts)
        {
            if (data_found == false)
            {
                GENERAL("Data read from DMX - This is for debugging with transform.c")
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
                WARNING("Uncorrectable error in packet, dropped.");
                ++_bad_packet_count;
                continue;
            }

            if (scrambling)
            {
                if (!scrambled[pid])
                {
                    RECORD(QString("PID %1 is scrambled").arg(pid));
                    scrambled[pid] = true;
                }
                continue; // Drop scrambled TS packet
            }

            if (scrambled[pid])
            {
                RECORD(QString("PID %1 is unscrambled").arg(pid));
                scrambled[pid] = false;
            }
            
            if (content & 0x1)
            {
                if (_continuity_count[pid] == 16)
                    _continuity_count[pid] = cc;
                else
                    _continuity_count[pid]++;

                if (_continuity_count[pid] > 15)
                    _continuity_count[pid] = 0;

                if (_continuity_count[pid] != cc)
                {
                    WARNING("Transport Stream Continuity Error. PID = " << pid );
                    RECORD(QString("PID %1 _continuity_count %2 cc %3")
                           .arg(pid).arg(_continuity_count[pid]).arg(cc));
                    _continuity_count[pid] = cc;
                    ++_continuity_error_count;
                }
            }

            if (_record_transport_stream_option)
            {
                if (isVideo[pid])
                {
                    // Check for keyframe
                    const TSPacket *pkt = reinterpret_cast<const TSPacket*>(pktbuf);
                    FindKeyframes(pkt);
                }

                // Sync recording start to first keyframe
                if (_wait_for_keyframe && !_keyframe_seen)
                    continue;
                if (!keyframe_found)
                {
                    keyframe_found = true;
                    RECORD(QString("Found keyframe"));
                }

                // Sync streams to the first PUSI (after video keyframe)
                if (!pusi_seen[pid])
                {
                    if ((pktbuf[1] & 0x40) == 0)
                        continue; // No PUSI - drop packet

                    RECORD(QString("Found PUSI for PID %1").arg(pid));
                    pusi_seen[pid] = true;
                }

                LocalProcessDataTS(pktbuf, MPEG_TS_PKT_SIZE);
                continue;
            }

            ipack *ip = pid_ipack[pid];
            if (ip == NULL)
                continue;

            ip->ps = 1;

            if ( (pktbuf[1] & 0x40) && (ip->plength == MMAX_PLENGTH-6) )
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
    if (buffer[0] == 0x00 && buffer[1] == 0x00 && buffer[2] == 0x01)
    {
        switch  (buffer[3]) 
        {
            case PRIVATE_STREAM1:
                break;
            case AUDIO_STREAM_S ... AUDIO_STREAM_E:
                buffer[3] = 0xc0; // fix audio ID to 0xC0
                break;

            case VIDEO_STREAM_S ... VIDEO_STREAM_E:
            {
                int pos = 8 + buffer[8];
                int datalen = len - pos;

                unsigned char *bufptr = &buffer[pos];
                unsigned int state = 0xFFFFFFFF, v = 0;
                int prvcount = -1;

                buffer[3] = 0xe0; // fix video ID to 0xE0
                while (bufptr < &buffer[pos] + datalen)
                {
                    if (++prvcount < 3)
                        v = prvpkt[prvcount];
                    else
                        v = *bufptr++;

                    if (state == 0x000001)
                    {
                        state = ((state << 8) | v) & 0xFFFFFF;
                        if (state >= SLICE_MIN && state <= SLICE_MAX)
                            continue;

                        if (state == SEQ_START)
                            _wait_for_keyframe = false;

                        switch (state)
                        {
                            case GOP_START:
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
                                break;
                            }

                            case PICTURE_START:
                                _frames_written_count++;
                                break;
                        }
                        continue;
                    }
                    state = ((state << 8) | v) & 0xFFFFFF;
                }
                break;
            }
        }

        memcpy(prvpkt, &buffer[len - 3], 3);
    }

    if (!_wait_for_keyframe)
        ringBuffer->Write(buffer, len);
}

void DVBRecorder::LocalProcessDataTS(unsigned char *buffer, int len)
{
    if (pkts_until_pat_pmt == 0)
    {
        ringBuffer->Write(pat_pkt, 188);
        ringBuffer->Write(pmt_pkt, 188);
        pkts_until_pat_pmt = 2000;
    }
    else
        pkts_until_pat_pmt--;

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

void DVBRecorder::CreatePAT(uint8_t *ts_packet)
{
    memset(ts_packet, 0xFF, 188);

    ts_packet[0] = 0x47;                            // sync byte
    ts_packet[1] = 0x40 | ((PAT_PID >> 8) & 0x1F);  // payload start & PID
    ts_packet[2] = PAT_PID & 0xFF;                  // PID
    ts_packet[3] = 0x10 | pat_cc;                   // scrambling, adaptation & continuity counter
    ts_packet[4] = 0x00;                            // pointer field

    ++pat_cc &= 0x0F;   // inc. continuity counter
    uint8_t *pat = ts_packet + 5;
    int p = 0;

    pat[p++] = PAT_TID; // table ID
    pat[p++] = 0xB0;    // section syntax indicator
    p++;                // section length (set later)
    pat[p++] = 0;       // TSID
    pat[p++] = 1;       // TSID
    pat[p++] = 0xC3;    // Version + Current/Next
    pat[p++] = 0;       // Current Section
    pat[p++] = 0;       // Last Section
    pat[p++] = 0;
    pat[p++] = 1;        // Always write ServiceID as 1
    pat[p++] = (PMT_PID >> 8) & 0x1F;
    pat[p++] = PMT_PID & 0xFF;

    pat[2] = p + 4 - 3; // section length

    unsigned int crc = mpegts_crc32(pat, p);
    pat[p++] = (crc >> 24) & 0xFF;
    pat[p++] = (crc >> 16) & 0xFF;
    pat[p++] = (crc >> 8) & 0xFF;
    pat[p++] = crc & 0xFF;
}

void DVBRecorder::CreatePMT(uint8_t *ts_packet) {
    memset(ts_packet, 0xFF, 188);

    ts_packet[0] = 0x47;                            // sync byte
    ts_packet[1] = 0x40 | ((PMT_PID >> 8) & 0x1F);  // payload start & PID
    ts_packet[2] = PMT_PID & 0xFF;                  // PID
    ts_packet[3] = 0x10 | pmt_cc;                   // scrambling, adaptation & continuity counter
    ts_packet[4] = 0x00;                            // pointer field

    ++pmt_cc &= 0x0F;   // inc. continuity counter
    uint8_t *pmt = ts_packet + 5;
    int p = 0;

    pmt[p++] = PMT_TID; // table ID
    pmt[p++] = 0xB0;    // section syntax indicator
    p++;                // section length (set later)
    pmt[p++] = 0;       // program number (ServiceID)
    pmt[p++] = 1;       // program number (ServiceID)
    pmt[p++] = 0xC3;    // Version + Current/Next
    pmt[p++] = 0;       // Current Section
    pmt[p++] = 0;       // Last Section
    pmt[p++] = (m_pmt.PCRPID >> 8) & 0x1F;
    pmt[p++] = m_pmt.PCRPID & 0xFF;

    // Write descriptors
    int program_info_length = 0;
    DescriptorList::Iterator dit;
    for (dit = m_pmt.Descriptors.begin(); dit != m_pmt.Descriptors.end(); ++dit)
    {
        int len = (*dit).Length;
        memcpy(&pmt[p + 2 + program_info_length], (*dit).Data, len);
        program_info_length += len;
    }

    // Program info length
    pmt[p++] = (program_info_length >> 8) & 0x0F;
    pmt[p++] = program_info_length & 0xFF;
    p += program_info_length;

    QValueList<ElementaryPIDObject>::Iterator es;
    for (es = m_pmt.Components.begin(); es != m_pmt.Components.end(); ++es)
    {
        if ((*es).Record)
        {
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
            pmt[p++] = stream_type;

            pmt[p++] = ((*es).PID >> 8) & 0x1F;
            pmt[p++] = (*es).PID & 0xFF;


            // Write descriptors
            int es_info_length = 0;
            DescriptorList::Iterator dit;
            for (dit = (*es).Descriptors.begin(); dit != (*es).Descriptors.end(); ++dit)
            {
                int len = (*dit).Length;
                memcpy(&pmt[p + 2 + es_info_length], (*dit).Data, len);
                es_info_length += len;
            }

            // ES info length
            pmt[p++] = (es_info_length >> 8) & 0x0F;
            pmt[p++] = es_info_length & 0xFF;
            p += es_info_length;
        }
    }

    pmt[2] = p + 4 - 3; // section length

    unsigned long crc = mpegts_crc32(pmt, p);
    pmt[p++] = (crc >> 24) & 0xFF;
    pmt[p++] = (crc >> 16) & 0xFF;
    pmt[p++] = (crc >> 8) & 0xFF;
    pmt[p++] = crc & 0xFF;
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

    QString debugmsg = QString("sync: %1 err: %2 paystart: %3 pid: %4 enc: %5 adaptation: %6 counter: %7")
                       .arg(sync,2,16)
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

//TODO
}

