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
#include <sys/poll.h>

#include "RingBuffer.h"
#include "programinfo.h"

#include "transform.h"
#include "dvbtypes.h"
#include "dvbdev.h"
#include "dvbchannel.h"
#include "dvbrecorder.h"

DVBRecorder::DVBRecorder(DVBChannel* advbchannel): RecorderBase()
{
    isopen = false;
    cardnum = 0;
    swfilter = false;
    recordts = false;
    was_paused = true;
    channel_changed = true;
    dvbchannel = advbchannel;

    paused = false;
    mainpaused = false;
    recording = false;

}

DVBRecorder::~DVBRecorder()
{
    if (isopen)
        Close();
}

void DVBRecorder::SetOption(const QString &name, int value)
{
    if (name == "cardnum")
        cardnum = value;
    else if (name == "swfilter")
        swfilter = (value == 1);
    else if (name == "recordts")
        recordts = (value == 1);
    else
        RecorderBase::SetOption(name, value);
}

void DVBRecorder::ChannelChanged(dvb_channel_t& chan)
{
    chan_opts = chan;
    wait_for_keyframe = true;
    channel_changed = true;

    framesWritten = 0;
    keyframedist = 0;
    keyCount = 0;
    gopset = false;
    prev_gop_save_pos = -1;
    memset(prvpkt, 0, 3);
}

bool DVBRecorder::Open()
{
    if (isopen)
        return true;

    fd_dvr = open(dvbdevice(DVB_DEV_DVR,cardnum), O_RDONLY | O_NONBLOCK);
    if(fd_dvr < 0)
    {
        ERRNO("Recorder: Failed to open dvb device");
        return false;
    }

    GENERAL("Recorder: Card opened successfully.");

    connect(dvbchannel, SIGNAL(ChannelChanged(dvb_channel_t&)),
            this, SLOT(ChannelChanged(dvb_channel_t&)));

    dvbchannel->RecorderStarted();

    isopen = true;
    return true;
}

void DVBRecorder::Close()
{
    if (!isopen)
        return;

    CloseFilters();

    if (fd_dvr > 0)
        close(fd_dvr);

    isopen = false;
}

void DVBRecorder::CloseFilters()
{
    for(unsigned int i=0; i<fd_demux.size(); i++)
        if (fd_demux[i] > 0)
            close(fd_demux[i]);
    fd_demux.clear();

    pid_ipack_t::iterator iter = pid_ipack.begin();
    for (;iter != pid_ipack.end(); iter++)
    {
        free_ipack((*iter).second);
        free((void*)(*iter).second);
    }
    pid_ipack.clear();
}

void DVBRecorder::OpenFilters(dvb_pid_t& pids, dmx_pes_type_t type)
{
    struct dmx_pes_filter_params params;
    params.input = DMX_IN_FRONTEND;
    params.output = DMX_OUT_TS_TAP;
    params.flags = DMX_IMMEDIATE_START;
    params.pes_type = type;
    params.pid = 0;

    for (unsigned int i = 0; i < pids.size(); i++)
    {
        int this_pid = pids[i];

        if (this_pid < 0x10 || this_pid > 0x1fff)
            WARNING(QString("PID value (%1) is outside dvb specification.")
                            .arg(this_pid));

        if ((swfilter && !swfilter_open) || !swfilter)
        {
            int fd_tmp = open(dvbdevice(DVB_DEV_DEMUX,cardnum), O_RDWR);

            if (fd_tmp < 0)
            {
                ERRNO(QString("Could not open filter device for pid %1.")
                      .arg(this_pid));
                continue;
            }

            params.pid = this_pid;

            if (swfilter)
            {
                GENERAL("Using Software Filtering.");
                params.pes_type = DMX_PES_OTHER;
                params.pid = DMX_DONT_FILTER;
                swfilter_open = true;
            }

            if (ioctl(fd_tmp, DMX_SET_PES_FILTER, &params) < 0)
            {
                close(fd_tmp);

                if (swfilter)
                {
                    ERRNO("Failed to open pid for Software Filtering.");
                    break;
                }
                else
                {
                    ERRNO(QString("Failed to set filter for pid %1.")
                          .arg(this_pid));
                    continue;
                }
            }

            if (swfilter)
                params.pid = this_pid;

            fd_demux.push_back(fd_tmp);
        }

        if (!recordts)
        {
            ipack* ip = (ipack*)malloc(sizeof(ipack));
            if (ip == NULL)
            {
                ERROR(QString("Failed to allocate ipack for pid %1.").arg(this_pid));
                continue;
            }

            init_ipack(ip, 2048, ProcessData, 1);
            ip->data = (void*)this;

            pid_ipack[this_pid] = ip;
        } else
            pid_ipack[this_pid] = NULL;
    }
}

void DVBRecorder::SetDemuxFilters(dvb_pids_t& pids)
{
    CloseFilters();

    if (swfilter)
        swfilter_open = false;

    if (recordts)
        pids.other.push_back(0);

    OpenFilters(pids.audio,       DMX_PES_AUDIO);
    OpenFilters(pids.video,       DMX_PES_VIDEO);
    OpenFilters(pids.teletext,    DMX_PES_TELETEXT);
    OpenFilters(pids.subtitle,    DMX_PES_SUBTITLE);
    OpenFilters(pids.pcr,         DMX_PES_PCR);
    OpenFilters(pids.other,       DMX_PES_OTHER);
}

void DVBRecorder::StartRecording()
{
    if (!Open())
        return;

    int readsz = 0;
    int ret, dataflow = -1;
    bool receiving = false;
    unsigned char pktbuf[MPEG_TS_SIZE];
    struct pollfd polls;

    polls.fd = fd_dvr;
    polls.events = POLLIN;
    polls.revents = 0;

    encoding = true;
    recording = true;

    emit Started();
    while (encoding)
    {
        if (channel_changed)
        {
            pthread_mutex_lock(&chan_opts.lock);
            SetDemuxFilters(chan_opts.pids);
            pthread_mutex_unlock(&chan_opts.lock);
            channel_changed = false;
        }

        if (paused)
        {
            for (int i=0; i<fd_demux.size(); i++)
                if (ioctl(fd_demux[i], DMX_STOP) < 0)
                    ERRNO(QString("Pausing DVB filter #%1 failed.").arg(i));
            receiving = false;
            mainpaused = true;
            emit Paused();
            pauseWait.wakeAll();
            was_paused = true;
            usleep(50);
            continue;
        }
        else if (was_paused)
        {
            for (int i=0; i<fd_demux.size(); i++)
                if (ioctl(fd_demux[i], DMX_START) < 0)
                    ERRNO(QString("Unpausing DVB filter #%1 failed.").arg(i));

            was_paused = false;
            mainpaused = false;
            emit Unpaused();
        }

        ret = poll(&polls, 1, 1000);

        if (ret == 0 && --dataflow < 1)
            WARNING("No data from card in 1 second.");

        if (ret == 1 && polls.revents & POLLIN)
        {
            dataflow = 1;

            if (paused)
                continue;

            readsz = read(fd_dvr, pktbuf, MPEG_TS_SIZE);
            if (readsz > 0)
            {
                int pes_offset = 0;
                int pid = ((pktbuf[1]&0x1f) << 8) | pktbuf[2];
                uint8_t cc = pktbuf[3] & 0xf;
                uint8_t content = (pktbuf[3] & 0x30) >> 4;

                if (pid_ipack.find(pid) == pid_ipack.end())
                    continue;
            
                if (!receiving)
                {
                    receiving = true;
                    emit Receiving();
                }
            
                if (content & 0x1)
                {
                    contcounter[pid]++;
                    if (contcounter[pid] > 15)
                        contcounter[pid] = 0;
            
                    if (contcounter[pid] != cc)
                    {
                        WARNING("Transport Stream Continuity Error.");
                        contcounter[pid] = cc;
                    }
                }
            
                if (recordts)
                {
                    LocalProcessData(&pktbuf[0], readsz);
                    continue;
                }
            
                ipack *ip = pid_ipack[pid];
                if (ip == NULL)
                    continue;
            
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
                               MPEG_TS_SIZE - 4 - pes_offset, ip);
            }
            else if (readsz < 0)
            {
                if (errno == EOVERFLOW || errno == EAGAIN)
                    continue;

                ERRNO("Error reading from DVB device.");
                continue;
            }
        } else if (ret < 0)
                ERRNO("Poll failed waiting for data.");
    }

    Close();

    FinishRecording();

    recording = false;

    emit Stopped();
}

int DVBRecorder::GetVideoFd(void)
{
    return -1;
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

void DVBRecorder::SetVideoFilters(QString &filters)
{
    (void)filters;
}

void DVBRecorder::Initialize(void)
{
}

#define SEQ_START     0x000001B3
#define GOP_START     0x000001B8
#define PICTURE_START 0x00000100
#define SLICE_MIN     0x00000101
#define SLICE_MAX     0x000001af

void DVBRecorder::ProcessData(unsigned char *buffer, int len, void *priv)
{
    ((DVBRecorder*)priv)->LocalProcessData(buffer, len);
}

void DVBRecorder::LocalProcessData(unsigned char *buffer, int len)
{
    if (recordts)
    {
        ringBuffer->Write(buffer, len);
        return;
    }

    if (buffer[0] == 0x00 && buffer[1] == 0x00 &&
        buffer[2] == 0x01 && (buffer[3]>>4) == 0xE)
    {
        int pos = 8 + buffer[8];
        int datalen = len - pos;

        unsigned char *bufptr = &buffer[pos];
        unsigned int state = 0xFFFFFFFF, v = 0;
        int prvcount = -1;

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

                switch (state)
                {
                    case GOP_START:
                    {
                        if (wait_for_keyframe)
                            wait_for_keyframe = false;

                        long long startpos = ringBuffer->GetFileWritePosition();

                        if (!gopset && framesWritten > 0)
                        {
                            keyframedist = framesWritten;
                            GENERAL("Setting KeyFrameDist to " << keyframedist);
                            gopset = true;
                        }                          

                        if (!gopset)
                            keyCount = 1;
                        else
                            keyCount++;

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
                    break;

                    case PICTURE_START:
                        if (!wait_for_keyframe)
                            framesWritten++;
                    break;

                }
                continue;
            }
            state = ((state << 8) | v) & 0xFFFFFF;
        }

        memcpy(prvpkt, &buffer[len-3], 3);
    }

    if (!wait_for_keyframe)
        ringBuffer->Write(buffer, len);
}

void DVBRecorder::StopRecording(void)
{
    encoding = false;
}

void DVBRecorder::Reset(void)
{
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

void DVBRecorder::WaitForPause(void)
{
    if (!mainpaused)
        if (!pauseWait.wait(1000))
            ERROR("Waited too long for recorder to pause.");
}

bool DVBRecorder::IsRecording(void)
{
    return recording;
}

long long DVBRecorder::GetFramesWritten(void)
{
    return framesWritten;
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
