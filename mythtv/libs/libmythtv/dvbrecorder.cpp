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
    error = false;
    isopen = false;
    cardnum = 0;
    swfilter = false;
    recordts = false;
    was_paused = true;
    channel_changed = true;
    dvbchannel = advbchannel;
    dvb_on_demand = false;

    paused = false;
    mainpaused = false;
    recording = false;

    cont_errors = 0;
    stream_overflows = 0;
    bad_packets = 0;

    signal_monitor_interval = 0;
    expire_data_days = 3;
    signal_monitor_quit = false;

    wait_for_seqstart = true;
    wait_for_seqstart_enabled = true;
    dmx_buf_size = DEF_DMX_BUF_SIZE;
    pkt_buf_size = MPEG_TS_PKT_SIZE * 50;
    pktbuffer = (unsigned char*)malloc(pkt_buf_size);
}

DVBRecorder::~DVBRecorder()
{
    if (isopen)
        Close();

    if (pktbuffer)
        free(pktbuffer);
}

void DVBRecorder::SetOption(const QString &name, int value)
{
    if (name == "cardnum")
        cardnum = value;
    else if (name == "swfilter")
        swfilter = (value == 1);
    else if (name == "recordts")
    {
        if (value == 1)
            GENERAL("Was told to record transport stream,"
                    " but this feature is broken, not enabling.");
//        recordts = (value == 1);
    }
    else if (name == "wait_for_seqstart")
        wait_for_seqstart_enabled = (value == 1);
    else if (name == "dmx_buf_size")
    {
        if (value != 0)
            dmx_buf_size = value;
    }
    else if (name == "pkt_buf_size")
    {
        if (value != 0)
        {
            pkt_buf_size = value - (value % MPEG_TS_PKT_SIZE);
            pktbuffer = (unsigned char*)realloc(pktbuffer, pkt_buf_size);
        }
    }
    else if (name == "signal_monitor_interval")
    {
        signal_monitor_interval = value;

        if (signal_monitor_interval < 0)
            signal_monitor_interval = 0; 
    }
    else if (name == "expire_data_days")
    {
        expire_data_days = value;

        if (expire_data_days < 1)
            expire_data_days = 1;
    }
    else if (name == "dvb_on_demand")
    {
        dvb_on_demand = value;
    }
    else
        RecorderBase::SetOption(name, value);
}

void DVBRecorder::SetOptionsFromProfile(RecordingProfile *profile,
                                        const QString &videodev,
                                        const QString &audiodev,
                                        const QString &vbidev, int ispip)
{
    (void)profile;
    (void)audiodev;
    (void)vbidev;
    (void)ispip;

    SetOption("cardnum", videodev.toInt());
}

void DVBRecorder::ChannelChanged(dvb_channel_t& chan)
{
    chan_opts = chan;

    if (wait_for_seqstart_enabled)
        wait_for_seqstart = true;
    else
        wait_for_seqstart = false;

    channel_changed = true;

    framesWritten = 0;
    memset(prvpkt, 0, 3);
}

bool DVBRecorder::Open()
{
    if (isopen)
        return true;

    if (dvb_on_demand && dvbchannel->Open())
    {
        // this is required to trigger a re-tune
        dvbchannel->SetChannelByString(dvbchannel->GetCurrentName());
    }

    fd_dvr = open(dvbdevice(DVB_DEV_DVR,cardnum), O_RDONLY | O_NONBLOCK);
    if (fd_dvr < 0)
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

    VERBOSE(VB_ALL, "Closing DVB recorder");

    CloseFilters();

    if (fd_dvr >= 0)
        close(fd_dvr);

    if (dvb_on_demand && dvbchannel)
        dvbchannel->CloseDVB();

    isopen = false;
}

void DVBRecorder::CloseFilters()
{
    for(unsigned int i=0; i<fd_demux.size(); i++)
        if (fd_demux[i] >= 0)
            close(fd_demux[i]);
    fd_demux.clear();

    pid_ipack_t::iterator iter = pid_ipack.begin();
    for (;iter != pid_ipack.end(); iter++)
    {
        free_ipack((*iter).second);
        free((void*)(*iter).second);
    }
    pid_ipack.clear();
    contcounter.clear();
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

        RECORD(QString("Adding pid %1, type %2").arg(this_pid).arg((int)type));

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

            if (ioctl(fd_tmp, DMX_SET_BUFFER_SIZE, dmx_buf_size) == -1)
            {
                ERRNO("DMX_SET_BUFFER_SIZE failed for pid " << this_pid);
            }

            params.pid = this_pid;

            if (swfilter)
            {
                GENERAL("Using Software Filtering.");
                params.pes_type = DMX_PES_OTHER;
                params.pid = DMX_DONT_FILTER;
                swfilter_open = true;
            }

            if (i)
                params.pes_type = DMX_PES_OTHER;

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

        contcounter[this_pid] = 16;
    }
}

void DVBRecorder::SetDemuxFilters(dvb_pids_t& pids)
{
    CloseFilters();

    if (swfilter)
        swfilter_open = false;

/*
    // FIXME: This should add the pid with PMT not PAT
    // NOTE: The section filter could report back what pid it's on.
    if (recordts)
        pids.other.push_back(0);
*/
    OpenFilters(pids.audio,       DMX_PES_AUDIO);
    OpenFilters(pids.video,       DMX_PES_VIDEO);
    OpenFilters(pids.teletext,    DMX_PES_TELETEXT);
    OpenFilters(pids.subtitle,    DMX_PES_SUBTITLE);
    OpenFilters(pids.pcr,         DMX_PES_PCR);
    OpenFilters(pids.other,       DMX_PES_OTHER);

    if (fd_demux.size() == 0 && pid_ipack.size() == 0)
    {
        ERROR("No PIDS set, please correct your channel setup.");
        return;
    }

    pid_ipack[pids.audio[0]]->pv = (struct ipack_s *)pid_ipack[pids.video[0]];
    pid_ipack[pids.video[0]]->pa = (struct ipack_s *)pid_ipack[pids.audio[0]];
}

void DVBRecorder::CorrectStreamNumber(ipack* ip, int pid)
{
    for (unsigned int i=0; i<chan_opts.pids.audio.size(); i++)
        if (chan_opts.pids.audio[i] == pid)
            ip->cid = 0xC0 + i;

    for (unsigned int i=0; i<chan_opts.pids.video.size(); i++)
        if (chan_opts.pids.video[i] == pid)
            ip->cid = 0xE0 + i;
}

void DVBRecorder::StartRecording()
{
    if (!Open())
    {
        error = true;
        return;
    }

    int ret, dataflow = -1;
    receiving = false;
    struct pollfd polls;

    pthread_t qualthread;
    bool qthreadexists = false;
    signal_monitor_quit = false;

    polls.fd = fd_dvr;
    polls.events = POLLIN;
    polls.revents = 0;

    cont_errors = 0;
    stream_overflows = 0;
    bad_packets = 0;

    // start the thread that logs signal data to the db

/*
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    if (signal_monitor_interval > 0 &&
        pthread_create(&qualthread, &attr, QualityMonitorHelper, this) == 0)
    {
        qthreadexists = true;
        RECORD("DVB Quality monitor is starting at " << 
               signal_monitor_interval << "s for card " << cardnum);
    }
*/

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
            was_paused = false;
            mainpaused = false;
            emit Unpaused();
        }

        for (;;)
        {
            ret = poll(&polls, 1, 1000);
            if (ret == -1 && (errno == EAGAIN || errno == EINTR))
                continue;
            break;
        }

        if (ret == 0 && --dataflow < 1)
            WARNING("No data from card in 1 second.");

        if (ret == 1 && polls.revents & POLLIN)
        {
            dataflow = 1;

            if (paused)
                continue;

            ReadFromDMX();
        }
        else if ((ret < 0) || (ret == 1 && polls.revents & POLLERR))
            ERRNO("Poll failed while waiting for data.");
    }

    Close();

    FinishRecording();

    // stop collecting data if the thread was started successfully

    if (qthreadexists)
    {
        signal_monitor_quit = true;
    }

    ExpireQualityData();

    recording = false;

    emit Stopped();
}

void DVBRecorder::ReadFromDMX()
{
    int readsz = 1;
    unsigned char *pktbuf;

    while (readsz > 0)
    {
        readsz = read(fd_dvr, pktbuffer, pkt_buf_size);

        if (readsz < 0)
        {
            if (errno == EOVERFLOW)
            {
                ++stream_overflows;
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
            pktbuf = pktbuffer + (curpkt * MPEG_TS_PKT_SIZE);
            curpkt++;

            int pes_offset = 0;
            int pid = ((pktbuf[1]&0x1f) << 8) | pktbuf[2];
            uint8_t cc = pktbuf[3] & 0xf;
            uint8_t content = (pktbuf[3] & 0x30) >> 4;

            if (pktbuf[1] & 0x80)
            {
                WARNING("Uncorrectable error in packet, dropped.");
                ++bad_packets;
                continue;
            }

            if (pid_ipack.find(pid) == pid_ipack.end())
                continue;
            
            if (!receiving)
            {
                receiving = true;
                emit Receiving();
            }
            
            if (content & 0x1)
            {
                if (contcounter[pid] == 16)
                    contcounter[pid] = cc;
                else
                    contcounter[pid]++;

                if (contcounter[pid] > 15)
                    contcounter[pid] = 0;

                if (contcounter[pid] != cc)
                {
                    WARNING("Transport Stream Continuity Error. PID = " << pid );
                    RECORD(QString("PID %1 contcounter %2 cc %3")
                           .arg(pid).arg(contcounter[pid]).arg(cc));
                    contcounter[pid] = cc;
                    ++cont_errors;
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

            CorrectStreamNumber(ip,pid);
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

        curRecording->SetFilesize(ringBuffer->GetRealFileSize(), db_conn);
        if (positionMapDelta.size())
        {
            curRecording->SetPositionMapDelta(positionMapDelta,
                                              MARK_GOP_BYFRAME, db_conn);
            positionMapDelta.clear();
        }

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
        // FIXME: Mark gop positions (Combine with HDTVRecorder?)
        // FIXME: Rewrite PMT/PAT
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

                if (state == SEQ_START)
                    wait_for_seqstart = false;

                switch (state)
                {
                    case GOP_START:
                    {
                        long long startpos = ringBuffer->GetFileWritePosition();

                        if (!positionMap.contains(framesWritten))
                        {
                            positionMapDelta[framesWritten] = startpos;
                            positionMap[framesWritten] = startpos;

                            if (curRecording && db_lock && db_conn &&
                                ((positionMapDelta.size() % 30) == 0))
                            {
                                pthread_mutex_lock(db_lock);
                                MythContext::KickDatabase(db_conn);
                                curRecording->SetPositionMapDelta(
                                                positionMapDelta, MARK_GOP_BYFRAME,
                                                db_conn);
                                curRecording->SetFilesize(startpos, db_conn);
                                pthread_mutex_unlock(db_lock);
                                positionMapDelta.clear();
                            }
                        }
                    }
                    break;

                    case PICTURE_START:
                        framesWritten++;
                    break;
                }
                continue;
            }
            state = ((state << 8) | v) & 0xFFFFFF;
        }

        memcpy(prvpkt, &buffer[len-3], 3);
    }

    if (!wait_for_seqstart)
        ringBuffer->Write(buffer, len);
}

void DVBRecorder::StopRecording(void)
{
    encoding = false;
}

void DVBRecorder::Reset(void)
{
    error = false;
    framesWritten = 0;

    positionMap.clear();
    positionMapDelta.clear();

    if (curRecording && db_lock && db_conn)
    {
        pthread_mutex_lock(db_lock);
        MythContext::KickDatabase(db_conn);
        curRecording->ClearPositionMap(MARK_GOP_BYFRAME, db_conn);
        pthread_mutex_unlock(db_lock);
    }
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

// we need the capture card id and I can't see an easy way to get it
// from this object

int DVBRecorder::GetIDForCardNumber(int cardnum)
{
    pthread_mutex_lock(db_lock);
    int cardid = -1;

    QSqlQuery result = db_conn->exec(QString("SELECT cardid FROM capturecard "
                                        "WHERE videodevice=\"%1\" AND "
                                        " cardtype='DVB';").arg(cardnum));

    if (result.isActive() && result.numRowsAffected() > 0)
    {
        result.next();

        cardid = result.value(0).toInt();
    }
    else
        RECORD("Could not get cardid for card number " << cardnum);

    pthread_mutex_unlock(db_lock);

    return cardid;
}

void DVBRecorder::QualityMonitorSample(int cardid,
                                       dvb_stats_t& sample)
{
    QString sql = QString("INSERT INTO dvb_signal_quality("
                          "sampletime, cardid, fe_snr, fe_ss, fe_ber, "
                          "fe_unc, myth_cont, myth_over, myth_pkts) "
                          "VALUES(NOW(), "
                          "\"%1\",\"%2\",\"%3\",\"%4\","
                          "\"%5\",\"%6\",\"%7\",\"%8\");")
        .arg(cardid)
        .arg(sample.snr & 0xffff)
        .arg(sample.ss & 0xffff).arg(sample.ber).arg(sample.ub)
        .arg(cont_errors).arg(stream_overflows).arg(bad_packets);

    QSqlQuery result = db_conn->exec(sql);

    if (!result.isActive())
        MythContext::DBError("DVB quality sample insert failed", result);
}

void *DVBRecorder::QualityMonitorThread()
{
    dvb_stats_t fe_stats;

    int cardid = GetIDForCardNumber(cardnum);

    // loop until cancelled, wake at intervals and log data

    while (!signal_monitor_quit)
    {
        sleep(signal_monitor_interval);

        if (signal_monitor_quit)
            break;

        if (cardid >= 0 &&
            db_conn != NULL && db_lock != NULL && dvbchannel != NULL &&
            dvbchannel -> FillFrontendStats(fe_stats))
        {
            pthread_mutex_lock(db_lock);

            QualityMonitorSample(cardid, fe_stats);

            pthread_mutex_unlock(db_lock);
        }
    }

    RECORD("DVB Quality monitor thread stopped for card " << cardnum);
    return NULL;
}

void DVBRecorder::ExpireQualityData()
{
    RECORD("Expiring DVB quality data older than " << expire_data_days <<
           " day(s)");

    pthread_mutex_lock(db_lock);

    QString sql = QString("DELETE FROM dvb_signal_quality "
                          "WHERE sampletime < "
                          "SUBDATE(NOW(), INTERVAL \"%1\" DAY);").
        arg(expire_data_days);

    QSqlQuery query = db_conn->exec(sql);

    if (!query.isActive())
        MythContext::DBError("Could not expire DVB signal data",
                             query);

    pthread_mutex_unlock(db_lock);
}

void *DVBRecorder::QualityMonitorHelper(void *self)
{
    return ((DVBRecorder *)self)->QualityMonitorThread();
}

