/*
 *  Class DVBChannel
 *
 * Copyright (C) Kenneth Aafloy 2003
 *
 *  Description:
 *      Has the responsibility of opening the Frontend device and
 *      setting the options to tune a channel. It also keeps other
 *      channel options used by the dvb hierarcy.
 *
 *  Author(s):
 *      Kenneth Aafloy (ke-aa at frisurf.no)
 *          - Rewritten for faster tuning.
 *      Ben Bucksch
 *          - Wrote the original implementation
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

#include <qsqldatabase.h>

#include <iostream>
#include <string>
#include <vector>
using namespace std;

#include <pthread.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/poll.h>

#include "RingBuffer.h"
#include "recorderbase.h"
#include "mythcontext.h"
#include "tv_rec.h"

#include "dvbtypes.h"
#include "dvbdev.h"
#include "dvbchannel.h"
#include "dvbrecorder.h"
#include "dvbdiseqc.h"
#include "dvbcam.h"
#include "dvbsections.h"


DVBChannel::DVBChannel(int aCardNum, TVRec *parent)
           : ChannelBase(parent), cardnum(aCardNum)
{
    fd_frontend = -1;
    force_channel_change = false;
    isOpen = false;
    diseqc = NULL;
    monitorRunning = false;
    monitorClients = 0;

    dvbcam = NULL;
    dvbsct = NULL;

    pthread_mutex_init(&chan_opts.lock, NULL);
}

DVBChannel::~DVBChannel()
{

    CloseDVB();
}

void DVBChannel::CloseDVB()
{
    VERBOSE(VB_ALL, "Closing DVB channel");

    if (fd_frontend >= 0)
    {
        close(fd_frontend);
        fd_frontend = -1;
        if (dvbcam)
            delete dvbcam;
        dvbcam = NULL;
        if (dvbsct)
            delete dvbsct;
        dvbsct = NULL;
        if (diseqc)
            delete diseqc;
        diseqc = NULL;
        isOpen = false;
    }
}

bool DVBChannel::Open()
{
    if (fd_frontend >= 0)
        return true;

    fd_frontend = open(dvbdevice(DVB_DEV_FRONTEND, cardnum),
                       O_RDWR | O_NONBLOCK);
    if (fd_frontend < 0)
    {
        ERRNO("Opening DVB frontend device failed.");
        return false;
    }

    if (ioctl(fd_frontend, FE_GET_INFO, &info) < 0)
    {
        ERRNO("Failed to get frontend information.")
        return false;
    }

    GENERAL(QString("Using DVB card %1, with frontend %2.")
            .arg(cardnum).arg(info.name));

    dvbsct = new DVBSections(cardnum);
    connect(this, SIGNAL(ChannelChanged(dvb_channel_t&)),
            dvbsct, SLOT(ChannelChanged(dvb_channel_t&)));

    if (info.type == FE_QPSK)
    {
        if (ioctl(fd_frontend, FE_SET_TONE, SEC_TONE_OFF) < 0)
            WARNING("Initial Tone setting failed.");

        if (ioctl(fd_frontend, FE_SET_VOLTAGE, SEC_VOLTAGE_13) < 0)
            WARNING("Initial Voltage setting failed.");

        if (diseqc == NULL)
        {
            diseqc = new DVBDiSEqC(cardnum, fd_frontend);
            diseqc->DiseqcReset();
        }
    }
    
    dvbcam = new DVBCam(cardnum);
    if (dvbcam->Open())
    {
        connect(this, SIGNAL(ChannelChanged(dvb_channel_t&)),
                dvbcam, SLOT(ChannelChanged(dvb_channel_t&)));
        connect(dvbsct, SIGNAL(ChannelChanged(dvb_channel_t&, uint8_t*, int)),
                dvbcam, SLOT(ChannelChanged(dvb_channel_t&, uint8_t*, int)));
    }

    force_channel_change = true;
    first_tune = true;

    return isOpen = true;
}

bool DVBChannel::SetChannelByString(const QString &chan)
{
    if (curchannelname == chan && !force_channel_change)
        return true;

    if (!isOpen)
        return false;

    force_channel_change = false;

    CHANNEL(QString("Trying to tune to channel %1.").arg(chan));

    if (GetChannelOptions(chan) == false)
    {
        ERROR(QString("Failed to get channel options for channel %1.").arg(chan));
        return false;
    }

    CheckOptions();
    PrintChannelOptions();

    if (!Tune(chan_opts))
    {
        ERROR(QString("Tuning for channel #%1 failed.").arg(chan));
        return false;
    }

    GENERAL(QString("Successfully tuned to channel %1.").arg(chan));

    ChannelChanged(chan_opts);

    curchannelname = chan;
    inputChannel[currentcapchannel] = curchannelname;

    return true;
}

void DVBChannel::RecorderStarted()
{
    ChannelChanged(chan_opts);
}

void DVBChannel::SwitchToInput(const QString &input, const QString &chan)
{
    currentcapchannel = 0;
    if (channelnames.empty())
       channelnames[currentcapchannel] = input;

    SetChannelByString(chan);
}

bool DVBChannel::GetChannelOptions(QString channum)
{
    QString         thequery;
    QSqlQuery       query;
    QSqlDatabase    *db_conn;
    pthread_mutex_t *db_lock;
    QString         inputName;

    if (!pParent->CheckChannel((ChannelBase*)this, channum,
                               db_conn, db_lock, inputName))
    {
        ERROR("Failed to verify channel integrity.");
        return false;
    }

    dvbcam->SetDatabase(db_conn, db_lock);

    pthread_mutex_lock(db_lock);
    MythContext::KickDatabase(db_conn);

    thequery = QString("SELECT channel.chanid "
                       "FROM channel,cardinput,capturecard WHERE "
                       "channel.channum='%1' AND "
                       "cardinput.sourceid = channel.sourceid AND "
                       "cardinput.cardid = '%2' AND "
                       "capturecard.cardid = cardinput.cardid AND "
                       "capturecard.cardtype = 'DVB'")
                       .arg(channum).arg(pParent->GetCaptureCardNum());
    query = db_conn->exec(thequery);

    if (!query.isActive())
        MythContext::DBError("GetChannelOptions - ChanID", query);

    if (query.numRowsAffected() <= 0)
    {
        ERROR("Unable to find channel in database.");
        pthread_mutex_unlock(db_lock);
        return false;
    }

    query.next();
    int chanid = query.value(0).toInt();    

    thequery = "SELECT serviceid, networkid, providerid, transportid,"
               " frequency, inversion, ";
    switch(info.type)
    {
        case FE_QPSK:
            thequery += QString("symbolrate, fec, polarity, diseqc_type,"
                                " diseqc_port, lnb_lof_switch, lnb_lof_hi,"
                                " lnb_lof_lo FROM dvb_channel, dvb_sat "
                                " WHERE dvb_channel.satid=dvb_sat.satid"
                                " AND chanid='%1'").arg(chanid);
            break;
        case FE_QAM:
            thequery += QString("symbolrate, fec, modulation"
                                " FROM dvb_channel WHERE chanid='%1'")
                                .arg(chanid);
            break;
        case FE_OFDM:
            thequery += QString("bandwidth, fec, lp_code_rate, modulation,"
                                " transmission_mode, guard_interval, hierarchy"
                                " FROM dvb_channel WHERE chanid='%1'")
                                .arg(chanid);
            break;
    }

    query = db_conn->exec(thequery);

    if (!query.isActive())
        MythContext::DBError("GetChannelOptions - Options", query);

    if (query.numRowsAffected() <= 0)
    {
        thequery = QString("SELECT satid FROM dvb_channel WHERE chanid='%1'")
                    .arg(chanid);
        query = db_conn->exec(thequery);

        if (!query.isActive())
            MythContext::DBError("GetChannelOptions - Channel Check", query);

        if (query.numRowsAffected() <= 0)
        {
            ERROR(QString("Could not find dvb tuning parameters for channel %1"
                          " with id %2.").arg(channum).arg(chanid));
        }
        else if (info.type == FE_QPSK)
        {
            int satid = query.value(0).toInt();
            thequery = QString("SELECT * FROM dvb_sat WHERE satid='%1'")
                        .arg(satid);
            query = db_conn->exec(thequery);

            if (!query.isActive())
                MythContext::DBError("GetChannelOptions - Sat Check", query);

            if (query.numRowsAffected() <= 0)
                ERROR(QString("Channel %1 is configured with satid=%2, but this"
                              " satellite does not exist, check your setup.")
                              .arg(channum).arg(satid));
        }

        pthread_mutex_unlock(db_lock);
        return false;
    }

    pthread_mutex_unlock(db_lock);

    query.next();
    if (!ParseQuery(query))
        return false;    

    if (!GetChannelPids(db_conn, db_lock, chanid))
        return false;

    return true;
}

bool DVBChannel::GetChannelPids(QSqlDatabase*& db_conn, 
                                pthread_mutex_t*& db_lock,
                                int chanid)
{
    dvb_pids_t& pids = chan_opts.pids;

    pids.audio.clear();
    pids.video.clear();
    pids.teletext.clear();
    pids.subtitle.clear();
    pids.pcr.clear();
    pids.other.clear();

    // TODO: Also fetch & store language field.
    pthread_mutex_lock(db_lock);
    MythContext::KickDatabase(db_conn);
    QString thequery = QString("SELECT pid, type FROM dvb_pids WHERE chanid=%1")
                               .arg(chanid);
    QSqlQuery query = db_conn->exec(thequery);

    if (!query.isActive())
        MythContext::DBError("GetChannelPids", query);

    if (query.numRowsAffected() <= 0)
    {
        ERROR(QString("Unable to find any pids for channel id %1.").arg(chanid));
        pthread_mutex_unlock(db_lock);
        return false;
    }

    int numpids = query.numRowsAffected();
    while (numpids--)
    {
        query.next();
        int this_pid = query.value(0).toInt();

        switch(query.value(1).toString()[0])
        {
            case 'a': pids.audio.push_back(this_pid);     break;
            case 'v': pids.video.push_back(this_pid);     break;
            case 't': pids.teletext.push_back(this_pid);  break;
            case 's': pids.subtitle.push_back(this_pid);  break;
            case 'p': pids.pcr.push_back(this_pid);       break;
            case 'o': pids.other.push_back(this_pid);     break;
            default: WARNING("Invalid type for pid!");
        }
    }

    pthread_mutex_unlock(db_lock);
    return true;
}

void DVBChannel::PrintChannelOptions()
{
    dvb_tuning_t& t = chan_opts.tuning;

    QString msg;
    switch(info.type)
    {
        case FE_QPSK:
            msg = QString("Frequency: %1 Symbol Rate: %2 Pol: %3")
                    .arg(t.params.frequency)
                    .arg(t.params.u.qpsk.symbol_rate)
                    .arg(t.voltage==SEC_VOLTAGE_13?'V':'H');

            msg += " Inv:";
            switch(t.params.inversion) {
                case INVERSION_AUTO: msg += "Auto"; break;
                case INVERSION_OFF: msg += "Off"; break;
                case INVERSION_ON: msg += "On"; break;
            }

            //NOTE: Diseqc and LNB is handled by DVBDiSEqC.
            break;

        case FE_QAM:
            msg = QString("Frequency: %1 Symbol Rate: %2.")
                    .arg(t.params.frequency)
                    .arg(t.params.u.qam.symbol_rate);

            msg += " Inv:";
            switch(t.params.inversion) {
                case INVERSION_AUTO: msg += "Auto"; break;
                case INVERSION_OFF: msg += "Off"; break;
                case INVERSION_ON: msg += "On"; break;
            }

            msg += " Fec:";
            switch(t.params.u.qam.fec_inner) {
                case FEC_NONE: msg += "None"; break;
                case FEC_AUTO: msg += "Auto"; break;
                case FEC_8_9: msg += "8/9"; break;
                case FEC_7_8: msg += "7/8"; break;
                case FEC_6_7: msg += "6/7"; break;
                case FEC_5_6: msg += "5/6"; break;
                case FEC_4_5: msg += "4/5"; break;
                case FEC_3_4: msg += "3/4"; break;
                case FEC_2_3: msg += "2/3"; break;
                case FEC_1_2: msg += "1/2"; break;
            }

            msg += " Mod:";
            switch(t.params.u.qam.modulation) {
                case QPSK: msg += "QPSK"; break;
                case QAM_AUTO: msg += "Auto"; break;
                case QAM_256: msg += "256"; break;
                case QAM_128: msg += "128"; break;
                case QAM_64: msg += "64"; break;
                case QAM_32: msg += "32"; break;
                case QAM_16: msg += "16"; break;
            }
            break;

        case FE_OFDM:
            msg = QString("Frequency: %1.").arg(t.params.frequency);

            msg += " BW:";
            switch(t.params.u.ofdm.bandwidth) {
                case BANDWIDTH_AUTO: msg += "Auto"; break;
                case BANDWIDTH_8_MHZ: msg += "8Mhz"; break;
                case BANDWIDTH_7_MHZ: msg += "7Mhz"; break;
                case BANDWIDTH_6_MHZ: msg += "6Mhz"; break;
            }

            msg += " HP:";
            switch(t.params.u.ofdm.code_rate_HP) {
                case FEC_NONE: msg += "None"; break;
                case FEC_AUTO: msg += "Auto"; break;
                case FEC_8_9: msg += "8/9"; break;
                case FEC_7_8: msg += "7/8"; break;
                case FEC_6_7: msg += "6/7"; break;
                case FEC_5_6: msg += "5/6"; break;
                case FEC_4_5: msg += "4/5"; break;
                case FEC_3_4: msg += "3/4"; break;
                case FEC_2_3: msg += "2/3"; break;
                case FEC_1_2: msg += "1/2"; break;
            }

            msg += " LP:";
            switch(t.params.u.ofdm.code_rate_LP) {
                case FEC_NONE: msg += "None"; break;
                case FEC_AUTO: msg += "Auto"; break;
                case FEC_8_9: msg += "8/9"; break;
                case FEC_7_8: msg += "7/8"; break;
                case FEC_6_7: msg += "6/7"; break;
                case FEC_5_6: msg += "5/6"; break;
                case FEC_4_5: msg += "4/5"; break;
                case FEC_3_4: msg += "3/4"; break;
                case FEC_2_3: msg += "2/3"; break;
                case FEC_1_2: msg += "1/2"; break;
            }

            msg += " C:";
            switch(t.params.u.ofdm.constellation) {
                case QPSK: msg += "QPSK"; break;
                case QAM_AUTO: msg += "Auto"; break;
                case QAM_256: msg += "256"; break;
                case QAM_128: msg += "128"; break;
                case QAM_64: msg += "64"; break;
                case QAM_32: msg += "32"; break;
                case QAM_16: msg += "16"; break;
            }

            msg += " TM:";
            switch(t.params.u.ofdm.transmission_mode) {
                case TRANSMISSION_MODE_AUTO: msg += "Auto"; break;
                case TRANSMISSION_MODE_2K: msg += "2K"; break;
                case TRANSMISSION_MODE_8K: msg += "8K"; break;
            }

            msg += " H:";
            switch(t.params.u.ofdm.hierarchy_information) {
                case HIERARCHY_AUTO: msg += "Auto"; break;
                case HIERARCHY_NONE: msg += "None"; break;
                case HIERARCHY_1: msg += "1"; break;
                case HIERARCHY_2: msg += "2"; break;
                case HIERARCHY_4: msg += "4"; break;
            }

            msg += " GI:";
            switch(t.params.u.ofdm.guard_interval) {
                case GUARD_INTERVAL_AUTO: msg += "Auto"; break;
                case GUARD_INTERVAL_1_4: msg += "1/4"; break;
                case GUARD_INTERVAL_1_8: msg += "1/8"; break;
                case GUARD_INTERVAL_1_16: msg += "1/16"; break;
                case GUARD_INTERVAL_1_32: msg += "1/32"; break;
            }
            break;
    }

    CHANNEL(msg);
}

void DVBChannel::CheckOptions()
{
    dvb_tuning_t& t = chan_opts.tuning;

    if ((t.params.inversion == INVERSION_AUTO)
        && !(info.caps & FE_CAN_INVERSION_AUTO))
    {
        WARNING("Unsupported inversion option 'auto',"
                " falling back to 'off'");
        t.params.inversion = INVERSION_OFF;
    }

    unsigned int frequency;
    if (info.type == FE_QPSK)
        if (t.params.frequency > t.lnb_lof_switch)
            frequency = abs((int)t.params.frequency - (int)t.lnb_lof_hi);
        else
            frequency = abs((int)t.params.frequency - (int)t.lnb_lof_lo);
    else
        frequency = t.params.frequency;

    if ((info.frequency_min > 0 && info.frequency_max > 0) &&
       (frequency < info.frequency_min || frequency > info.frequency_max))
        WARNING(QString("Your frequency setting (%1) is"
                        " out of range. (min/max:%2/%3)")
                        .arg(frequency)
                        .arg(info.frequency_min)
                        .arg(info.frequency_max));

    unsigned int symbol_rate = 0;
    switch(info.type)
    {
        case FE_QPSK:
            symbol_rate = t.params.u.qpsk.symbol_rate;

            if (!CheckCodeRate(t.params.u.qpsk.fec_inner))
                WARNING("Unsupported fec_inner parameter.");
            break;
        case FE_QAM:
            symbol_rate = t.params.u.qam.symbol_rate;

            if (!CheckCodeRate(t.params.u.qam.fec_inner))
                WARNING("Unsupported fec_inner parameter.");

            if (!CheckModulation(t.params.u.qam.modulation))
                WARNING("Unsupported modulation parameter.");

            break;
        case FE_OFDM:
            if (!CheckCodeRate(t.params.u.ofdm.code_rate_HP))
                WARNING("Unsupported code_rate_hp option.");

            if (!CheckCodeRate(t.params.u.ofdm.code_rate_LP))
                WARNING("Unsupported code_rate_lp parameter.");

            if ((t.params.u.ofdm.bandwidth == BANDWIDTH_AUTO)
                && !(info.caps & FE_CAN_BANDWIDTH_AUTO))
                WARNING("Unsupported bandwidth parameter.");

            if ((t.params.u.ofdm.transmission_mode == TRANSMISSION_MODE_AUTO)
                && !(info.caps & FE_CAN_TRANSMISSION_MODE_AUTO))
                WARNING("Unsupported transmission_mode parameter.");

            if ((t.params.u.ofdm.guard_interval == GUARD_INTERVAL_AUTO)
                && !(info.caps & FE_CAN_GUARD_INTERVAL_AUTO))
                WARNING("Unsupported quard_interval parameter.");

            if ((t.params.u.ofdm.hierarchy_information == HIERARCHY_AUTO)
                && !(info.caps & FE_CAN_HIERARCHY_AUTO))
                WARNING("Unsupported hierarchy parameter.");

            if (!CheckModulation(t.params.u.ofdm.constellation))
                WARNING("Unsupported constellation parameter.");
            break;
    }

    if (info.type != FE_OFDM &&
       (symbol_rate < info.symbol_rate_min ||
        symbol_rate > info.symbol_rate_max))
        WARNING(QString("Symbol Rate setting (%1) is "
                        "out of range (min/max:%2/%3)")
                        .arg(symbol_rate)
                        .arg(info.symbol_rate_min)
                        .arg(info.symbol_rate_max));
}

bool DVBChannel::CheckCodeRate(fe_code_rate_t& rate)
{
    switch(rate)
    {
        case FEC_1_2:   if (info.caps & FE_CAN_FEC_1_2)     return true; break;
        case FEC_2_3:   if (info.caps & FE_CAN_FEC_2_3)     return true; break;
        case FEC_3_4:   if (info.caps & FE_CAN_FEC_3_4)     return true; break;
        case FEC_4_5:   if (info.caps & FE_CAN_FEC_4_5)     return true; break;
        case FEC_5_6:   if (info.caps & FE_CAN_FEC_5_6)     return true; break;
        case FEC_6_7:   if (info.caps & FE_CAN_FEC_6_7)     return true; break;
        case FEC_7_8:   if (info.caps & FE_CAN_FEC_7_8)     return true; break;
        case FEC_8_9:   if (info.caps & FE_CAN_FEC_8_9)     return true; break;
        case FEC_AUTO:  if (info.caps & FE_CAN_FEC_AUTO)    return true; break;
        case FEC_NONE:  return true;
    }
    return false;
}

bool DVBChannel::CheckModulation(fe_modulation_t& modulation)
{
    switch(modulation)
    {
        case QPSK:      if (info.caps & FE_CAN_QPSK)        return true; break;
        case QAM_16:    if (info.caps & FE_CAN_QAM_16)      return true; break;
        case QAM_32:    if (info.caps & FE_CAN_QAM_32)      return true; break;
        case QAM_64:    if (info.caps & FE_CAN_QAM_64)      return true; break;
        case QAM_128:   if (info.caps & FE_CAN_QAM_128)     return true; break;
        case QAM_256:   if (info.caps & FE_CAN_QAM_256)     return true; break;
        case QAM_AUTO:  if (info.caps & FE_CAN_QAM_AUTO)    return true; break;
    }
    return false;
}

bool DVBChannel::FillFrontendStats(dvb_stats_t& stats)
{
    if (fd_frontend < 0)
        return false;

    ioctl(fd_frontend, FE_READ_SNR, &stats.snr);
    ioctl(fd_frontend, FE_READ_SIGNAL_STRENGTH, &stats.ss);
    ioctl(fd_frontend, FE_READ_BER, &stats.ber);
    ioctl(fd_frontend, FE_READ_UNCORRECTED_BLOCKS, &stats.ub);
    ioctl(fd_frontend, FE_READ_STATUS, &stats.status);

    return true;
}

void* DVBChannel::StatusMonitorHelper(void* self)
{
    ((DVBChannel*)self)->StatusMonitorLoop();
    return NULL;
}

void DVBChannel::StatusMonitorLoop()
{
    dvb_stats_t stats;

    monitorRunning = true;
    GENERAL("StatusMonitor Starting");

    while (monitorClients > 0)
    {
        if (!isOpen)
        {
            usleep(250*1000);
            continue;
        }
        
        if (FillFrontendStats(stats))
        {
            emit Status(stats);
            emit StatusSignalToNoise(stats.snr);
            emit StatusSignalStrength(stats.ss);
            emit StatusBitErrorRate(stats.ber);
            emit StatusUncorrectedBlocks(stats.ub);

            QString str = "";
            if (stats.status & FE_TIMEDOUT)
            {
                str = "Timed out waiting for signal.";
            }
            else
            {
                if (stats.status & FE_HAS_SIGNAL)  str += "SIGNAL ";
                if (stats.status & FE_HAS_CARRIER) str += "CARRIER ";
                if (stats.status & FE_HAS_VITERBI) str += "VITERBI ";
                if (stats.status & FE_HAS_SYNC) str += "SYNC ";
                if (stats.status & FE_HAS_LOCK) str += "LOCK ";
            }
            emit Status(str);
        }

        usleep(250*1000);
    }

    monitorRunning = false;
    GENERAL("StatusMonitor Stopping");
}

void DVBChannel::connectNotify(const char* signal)
{
    QString sig = signal;
    if (sig == SIGNAL(Status(dvb_stats_t &)) ||
        sig == SIGNAL(Status(QString)))
    {
        monitorClients++;
        if (!monitorRunning)
        {
            pthread_attr_t attr;
            pthread_attr_init(&attr);
            pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

            pthread_create(&statusMonitorThread, &attr,
                           StatusMonitorHelper, this);
        }
    }
}

void DVBChannel::disconnectNotify(const char* signal)
{
    QString sig = signal;
    if (sig == SIGNAL(Status(dvb_stats_t &)) ||
        sig == SIGNAL(Status(QString)))
    {
        monitorClients--;
        if (monitorClients < 1)
            while(monitorRunning)
                usleep(1000);
    }
}

/*****************************************************************************
        Query parser functions for each of the three types of cards.
 *****************************************************************************/

bool DVBChannel::ParseQuery(QSqlQuery& query)
{
    pthread_mutex_lock(&chan_opts.lock);

    chan_opts.serviceID     = query.value(0).toInt();
    chan_opts.networkID     = query.value(1).toInt();
    chan_opts.providerID    = query.value(2).toInt();
    chan_opts.transportID   = query.value(3).toInt();

    switch(info.type)
    {
        case FE_QPSK:
            if (!ParseQPSK(query.value(4).toString(), query.value(5).toString(),
                           query.value(6).toString(), query.value(7).toString(),
                           query.value(8).toString(), query.value(9).toString(),
                           query.value(10).toString(), query.value(11).toString(),
                           query.value(12).toString(), query.value(13).toString(),
                           chan_opts.tuning))
            {
                pthread_mutex_unlock(&chan_opts.lock);
                return false;
            }
            break;
        case FE_QAM:
            if (!ParseQAM(query.value(4).toString(), query.value(5).toString(),
                         query.value(6).toString(), query.value(7).toString(),
                         query.value(8).toString(), chan_opts.tuning))
            {
                pthread_mutex_unlock(&chan_opts.lock);
                return false;
            }
            break;
        case FE_OFDM:
            if (!ParseOFDM(query.value(4).toString(), query.value(5).toString(),
                           query.value(6).toString(), query.value(7).toString(),
                           query.value(8).toString(), query.value(9).toString(),
                           query.value(10).toString(), query.value(11).toString(),
                           query.value(12).toString(), chan_opts.tuning))
            {
                pthread_mutex_unlock(&chan_opts.lock);
                return false;
            }
            break;
    }

    pthread_mutex_unlock(&chan_opts.lock);
    return true;
}

bool DVBChannel::ParseQPSK(const QString& frequency, const QString& inversion,
                           const QString& symbol_rate, const QString& fec_inner,
                           const QString& pol, 
                           const QString& diseqc_type, const QString& diseqc_port,
                           const QString& lnb_lof_switch, const QString& lnb_lof_hi,
                           const QString& lnb_lof_lo, dvb_tuning_t& t)
{
    dvb_qpsk_parameters& p = t.params.u.qpsk;

    t.params.frequency = frequency.toInt();

    switch(inversion[0]) {
        case '1': t.params.inversion = INVERSION_ON;    break;
        case '0': t.params.inversion = INVERSION_OFF;   break;
        case 'a': t.params.inversion = INVERSION_AUTO;  break;
        default : ERROR("Invalid inversion, aborting.");
                  return false;
    }

    p.symbol_rate = symbol_rate.toInt();
    if (p.symbol_rate == 0) {
        ERROR("Invalid symbol rate parameter '" << symbol_rate
              << "', aborting.");
        return false;
    }

    if (fec_inner == "none")        p.fec_inner = FEC_NONE;
    else if (fec_inner == "auto")   p.fec_inner = FEC_AUTO;
    else if (fec_inner == "8/9")    p.fec_inner = FEC_8_9;
    else if (fec_inner == "7/8")    p.fec_inner = FEC_7_8;
    else if (fec_inner == "6/7")    p.fec_inner = FEC_6_7;
    else if (fec_inner == "5/6")    p.fec_inner = FEC_5_6;
    else if (fec_inner == "4/5")    p.fec_inner = FEC_4_5;
    else if (fec_inner == "3/4")    p.fec_inner = FEC_3_4;
    else if (fec_inner == "2/3")    p.fec_inner = FEC_2_3;
    else if (fec_inner == "1/2")    p.fec_inner = FEC_1_2;
    else {
        WARNING("Invalid fec_inner parameter '" << fec_inner
                << "', falling back to 'auto'.");
        p.fec_inner = FEC_AUTO;
    }

    switch(pol[0]) {
        case 'v': case 'r': t.voltage = SEC_VOLTAGE_13; break;
        case 'h': case 'l': t.voltage = SEC_VOLTAGE_18; break;
        default : ERROR("Invalid polarization, aborting.");
                  return false;
    }

    t.diseqc_type = diseqc_type.toInt();
    t.diseqc_port = diseqc_port.toInt();
    t.lnb_lof_switch = lnb_lof_switch.toInt();
    t.lnb_lof_hi = lnb_lof_hi.toInt();
    t.lnb_lof_lo = lnb_lof_lo.toInt();
    return true;
}

bool DVBChannel::ParseQAM(const QString& frequency, const QString& inversion,
                          const QString& symbol_rate, const QString& fec_inner,
                          const QString& modulation, dvb_tuning_t& t)
{
    dvb_qam_parameters& p = t.params.u.qam;

    t.params.frequency = frequency.toInt();

    switch(inversion[0]) {
        case '1': t.params.inversion = INVERSION_ON;    break;
        case '0': t.params.inversion = INVERSION_OFF;   break;
        case 'a': t.params.inversion = INVERSION_AUTO;  break;
        default : ERROR("Invalid inversion, aborting.");
                  return false;
    }

    p.symbol_rate = symbol_rate.toInt();
    if (p.symbol_rate == 0) {
        ERROR("Invalid symbol rate parameter '" << symbol_rate
              << "', aborting.");
        return false;
    }

    if (fec_inner == "none")        p.fec_inner = FEC_NONE;
    else if (fec_inner == "auto")   p.fec_inner = FEC_AUTO;
    else if (fec_inner == "8/9")    p.fec_inner = FEC_8_9;
    else if (fec_inner == "7/8")    p.fec_inner = FEC_7_8;
    else if (fec_inner == "6/7")    p.fec_inner = FEC_6_7;
    else if (fec_inner == "5/6")    p.fec_inner = FEC_5_6;
    else if (fec_inner == "4/5")    p.fec_inner = FEC_4_5;
    else if (fec_inner == "3/4")    p.fec_inner = FEC_3_4;
    else if (fec_inner == "2/3")    p.fec_inner = FEC_2_3;
    else if (fec_inner == "1/2")    p.fec_inner = FEC_1_2;
    else {
        WARNING("Invalid fec_inner parameter '" << fec_inner
                << "', falling back to 'auto'.");
        p.fec_inner = FEC_AUTO;
    }

    if (modulation == "qpsk")           p.modulation = QPSK;
    else if (modulation == "auto")      p.modulation = QAM_AUTO;
    else if (modulation == "qam_256")   p.modulation = QAM_256;
    else if (modulation == "qam_128")   p.modulation = QAM_128;
    else if (modulation == "qam_64")    p.modulation = QAM_64;
    else if (modulation == "qam_32")    p.modulation = QAM_32;
    else if (modulation == "qam_16")    p.modulation = QAM_16;
    else {
        WARNING("Invalid modulationulation parameter '" << modulation
                << "', falling bac to 'auto'.");
        p.modulation = QAM_AUTO;
    }

    return true;
}

bool DVBChannel::ParseOFDM(const QString& frequency, const QString& inversion,
                           const QString& bandwidth, const QString& coderate_hp,
                           const QString& coderate_lp, const QString& constellation,
                           const QString& trans_mode, const QString& guard_interval,
                           const QString& hierarchy, dvb_tuning_t& t)
{
    dvb_ofdm_parameters& p = t.params.u.ofdm;

    t.params.frequency = frequency.toInt();

    switch(inversion[0]) {
        case '1': t.params.inversion = INVERSION_ON;    break;
        case '0': t.params.inversion = INVERSION_OFF;   break;
        case 'a': t.params.inversion = INVERSION_AUTO;  break;
        default : ERROR("Invalid inversion, aborting.");
                  return false;
    }

    switch(bandwidth[0]) {
        case 'a': p.bandwidth = BANDWIDTH_AUTO; break;
        case '8': p.bandwidth = BANDWIDTH_8_MHZ; break;
        case '7': p.bandwidth = BANDWIDTH_7_MHZ; break;
        case '6': p.bandwidth = BANDWIDTH_6_MHZ; break;
        default: WARNING("Invalid bandwidth parameter '" << bandwidth
                         << "', falling back to 'auto'.");
                 p.bandwidth = BANDWIDTH_AUTO;
    }

    if (coderate_hp == "none")      p.code_rate_HP = FEC_NONE;
    else if (coderate_hp == "auto") p.code_rate_HP = FEC_AUTO;
    else if (coderate_hp == "8/9")  p.code_rate_HP = FEC_8_9;
    else if (coderate_hp == "7/8")  p.code_rate_HP = FEC_7_8;
    else if (coderate_hp == "6/7")  p.code_rate_HP = FEC_6_7;
    else if (coderate_hp == "5/6")  p.code_rate_HP = FEC_5_6;
    else if (coderate_hp == "4/5")  p.code_rate_HP = FEC_4_5;
    else if (coderate_hp == "3/4")  p.code_rate_HP = FEC_3_4;
    else if (coderate_hp == "2/3")  p.code_rate_HP = FEC_2_3;
    else if (coderate_hp == "1/2")  p.code_rate_HP = FEC_1_2;
    else {
        WARNING("Invalid hp code rate parameter '" << coderate_hp
                << "', falling back to 'auto'.");
        p.code_rate_HP = FEC_AUTO;
    }

    if (coderate_lp == "none")      p.code_rate_LP = FEC_NONE;
    else if (coderate_lp == "auto") p.code_rate_LP = FEC_AUTO;
    else if (coderate_lp == "8/9")  p.code_rate_LP = FEC_8_9;
    else if (coderate_lp == "7/8")  p.code_rate_LP = FEC_7_8;
    else if (coderate_lp == "6/7")  p.code_rate_LP = FEC_6_7;
    else if (coderate_lp == "5/6")  p.code_rate_LP = FEC_5_6;
    else if (coderate_lp == "4/5")  p.code_rate_LP = FEC_4_5;
    else if (coderate_lp == "3/4")  p.code_rate_LP = FEC_3_4;
    else if (coderate_lp == "2/3")  p.code_rate_LP = FEC_2_3;
    else if (coderate_lp == "1/2")  p.code_rate_LP = FEC_1_2;
    else {
        WARNING("Invalid lp code rate parameter '" << coderate_lp
                << "', falling back to 'auto'.");
        p.code_rate_LP = FEC_AUTO;
    }

    if (constellation == "auto")            p.constellation = QAM_AUTO;
    else if (constellation == "qpsk")       p.constellation = QPSK;
    else if (constellation == "qam_16")     p.constellation = QAM_16;
    else if (constellation == "qam_32")     p.constellation = QAM_32;
    else if (constellation == "qam_64")     p.constellation = QAM_64;
    else if (constellation == "qam_128")    p.constellation = QAM_128;
    else if (constellation == "qam_256")    p.constellation = QAM_256;
    else {
        WARNING("Invalid constellation parameter '" << constellation
                << "', falling bac to 'auto'.");
        p.constellation = QAM_AUTO;
    }

    switch (trans_mode[0])
    {
        case 'a': p.transmission_mode = TRANSMISSION_MODE_AUTO; break;
        case '2': p.transmission_mode = TRANSMISSION_MODE_2K; break;
        case '8': p.transmission_mode = TRANSMISSION_MODE_8K; break;
        default: WARNING("invalid transmission mode parameter '" << trans_mode
                         << "', falling back to 'auto'.");
                  p.transmission_mode = TRANSMISSION_MODE_AUTO;
    }

    switch (hierarchy[0]) {
        case 'a': p.hierarchy_information = HIERARCHY_AUTO; break;
        case 'n': p.hierarchy_information = HIERARCHY_NONE; break;
        case '1': p.hierarchy_information = HIERARCHY_1; break;
        case '2': p.hierarchy_information = HIERARCHY_2; break;
        case '4': p.hierarchy_information = HIERARCHY_4; break;
        default: WARNING("invalid hierarchy parameter '" << hierarchy
                         << "', falling back to 'auto'.");
                 p.hierarchy_information = HIERARCHY_AUTO;
    }

    if (guard_interval == "auto")       p.guard_interval = GUARD_INTERVAL_AUTO;
    else if (guard_interval == "1/4")     p.guard_interval = GUARD_INTERVAL_1_4;
    else if (guard_interval == "1/8")     p.guard_interval = GUARD_INTERVAL_1_8;
    else if (guard_interval == "1/16")    p.guard_interval = GUARD_INTERVAL_1_16;
    else if (guard_interval == "1/32")    p.guard_interval = GUARD_INTERVAL_1_32;
    else {
        WARNING("invalid guard interval parameter '" << guard_interval
                << "', falling back to 'auto'.");
        p.guard_interval = GUARD_INTERVAL_AUTO;
    }

    return true;
}

/*****************************************************************************
           Tuning functions for each of the three types of cards.
 *****************************************************************************/

bool DVBChannel::Tune(dvb_channel_t& channel, bool all)
{
    dvb_tuning_t& tuning = channel.tuning;

    if (fd_frontend < 0)
    {
        ERROR("Card not open!");
        return false;
    }

    struct dvb_frontend_event event;

    bool reset      = false;
    bool havetuned  = false;
    bool tune       = true;

    int max_poll_timeout_count = 30;
    int max_frontend_timeout_count = 30;
    int poll_return;

    if (all == true)
        first_tune = true;
    if (first_tune)
    {
        reset      = true;
        first_tune = false;
    }

    struct pollfd polls;
    polls.fd = fd_frontend;

    while (true)
    {
        if (tune)
        {
            switch(info.type)
            {
                case FE_QPSK:
                    if (!TuneQPSK(tuning, reset, havetuned))
                        return false;
                    break;
                case FE_QAM:
                    if (!TuneQAM(tuning, reset, havetuned))
                        return false;
                    break;
                case FE_OFDM:
                    if (!TuneOFDM(tuning, reset, havetuned))
                        return false;
                    break;
            }

            if (havetuned == false)
                return true;

            tune = false;
            reset = false;

            CHANNEL("Waiting for frontend event after tune.");
        }

        polls.events = POLLIN;
        polls.revents = 0;

        for (;;)
        {
            poll_return = poll(&polls, 1, 150);
            if (poll_return == -1 && (errno == EAGAIN || errno == EINTR))
                continue;
            break;
        }

        if (poll_return > 0)
        {
            if (ioctl(fd_frontend, FE_GET_EVENT, &event) < 0)
            {
                if (errno == EOVERFLOW || errno == EAGAIN)
                    continue;

                ERRNO("Failed getting frontend event.");
                return false;
            }

            QString status = "Status: ";
            if (event.status & FE_HAS_CARRIER)  status += "CARRIER | ";
            if (event.status & FE_HAS_VITERBI)  status += "VITERBI | ";
            if (event.status & FE_HAS_SIGNAL)   status += "SIGNAL | ";
            if (event.status & FE_HAS_SYNC)     status += "SYNC | ";
            if (event.status & FE_HAS_LOCK)
            {
                status += "LOCK.";
                CHANNEL(status);
                return true;
            }
            else
            {
                status += "NO LOCK!";
                WARNING(status);
                continue;
            }

            if (event.status & FE_TIMEDOUT)
            {
                if (max_frontend_timeout_count==0)
                {
                    ERROR("Frontend timed out too many times, bailing.");
                    return false;
                }

                ERROR("Timed out, waiting for frontend event, retrying.");
                max_frontend_timeout_count--;
                continue;
            }

            if (event.status & FE_REINIT)
            {
                ERROR("Frontend was reinitialized, retuning.");
                reset = tune = true;
            }
        }
        else
        {
            if (poll_return == 0)
            {
                if (max_poll_timeout_count)
                {
                    max_poll_timeout_count--;
                    continue;
                }
                else
                {
                    ERROR("Poll timed out too many times, bailing.");
                    return false;
                }
            }

            if (poll_return == -1)
            {
                ERRNO("Poll failed waiting for frontend event.");
                continue;
            }
        }
    }

    return true;
}

bool DVBChannel::TuneQPSK(dvb_tuning_t& tuning, bool reset, bool& havetuned)
{
    int frequency = tuning.params.frequency;
    if (tuning.params.frequency >= tuning.lnb_lof_switch)
    {
        tuning.params.frequency = abs((int)tuning.params.frequency - 
                                      (int)tuning.lnb_lof_hi);
        tuning.tone = SEC_TONE_ON;
    }
    else
    {
        tuning.params.frequency = abs((int)tuning.params.frequency - 
                                      (int)tuning.lnb_lof_lo);
        tuning.tone = SEC_TONE_OFF;
    }

    if (diseqc)
        if (!diseqc->Set(tuning, reset, havetuned))
            return false;

    if (reset ||
        prev_tuning.params.frequency != tuning.params.frequency ||
        prev_tuning.params.inversion != tuning.params.inversion ||
        prev_tuning.params.u.qpsk.symbol_rate != tuning.params.u.qpsk.symbol_rate ||
        prev_tuning.params.u.qpsk.fec_inner   != tuning.params.u.qpsk.fec_inner)
    {
        if (ioctl(fd_frontend, FE_SET_FRONTEND, &tuning.params) < 0)
        {
            ERRNO("Setting Frontend failed.");
            return false;
        }

        prev_tuning.params.frequency = tuning.params.frequency;
        prev_tuning.params.inversion = tuning.params.inversion;
        prev_tuning.params.u.qpsk.symbol_rate = tuning.params.u.qpsk.symbol_rate;
        prev_tuning.params.u.qpsk.fec_inner   = tuning.params.u.qpsk.fec_inner;
        havetuned = true;
    }

    tuning.params.frequency = frequency;

    return true;
}

bool DVBChannel::TuneQAM(dvb_tuning_t& tuning, bool reset, bool& havetuned)
{
    if (reset ||
        prev_tuning.params.frequency != tuning.params.frequency ||
        prev_tuning.params.inversion != tuning.params.inversion ||
        prev_tuning.params.u.qam.symbol_rate != tuning.params.u.qam.symbol_rate ||
        prev_tuning.params.u.qam.fec_inner   != tuning.params.u.qam.fec_inner   ||
        prev_tuning.params.u.qam.modulation  != tuning.params.u.qam.modulation)
    {
        if (ioctl(fd_frontend, FE_SET_FRONTEND, &tuning.params) < 0)
        {
            ERRNO("Setting Frontend failed.");
            return false;
        }

        prev_tuning.params.frequency = tuning.params.frequency;
        prev_tuning.params.inversion = tuning.params.inversion;
        prev_tuning.params.u.qam.symbol_rate = tuning.params.u.qam.symbol_rate;
        prev_tuning.params.u.qam.fec_inner   = tuning.params.u.qam.fec_inner;
        prev_tuning.params.u.qam.modulation  = tuning.params.u.qam.modulation;
        havetuned = true;
    }

    return true;
}

bool DVBChannel::TuneOFDM(dvb_tuning_t& tuning, bool reset, bool& havetuned)
{
    if (reset ||
        prev_tuning.params.frequency != tuning.params.frequency ||
        prev_tuning.params.inversion != tuning.params.inversion ||
        prev_tuning.params.u.ofdm.bandwidth != tuning.params.u.ofdm.bandwidth ||
        prev_tuning.params.u.ofdm.code_rate_HP != tuning.params.u.ofdm.code_rate_HP ||
        prev_tuning.params.u.ofdm.code_rate_LP != tuning.params.u.ofdm.code_rate_LP ||
        prev_tuning.params.u.ofdm.constellation != tuning.params.u.ofdm.constellation ||
        prev_tuning.params.u.ofdm.transmission_mode != tuning.params.u.ofdm.transmission_mode ||
        prev_tuning.params.u.ofdm.guard_interval != tuning.params.u.ofdm.guard_interval ||
        prev_tuning.params.u.ofdm.hierarchy_information != tuning.params.u.ofdm.hierarchy_information)
    {
        if (ioctl(fd_frontend, FE_SET_FRONTEND, &tuning.params) < 0)
        {
            ERRNO("Setting Frontend failed.");
            return false;
        }

        prev_tuning.params.frequency = tuning.params.frequency;
        prev_tuning.params.inversion = tuning.params.inversion;
        prev_tuning.params.u.ofdm.bandwidth = tuning.params.u.ofdm.bandwidth;
        prev_tuning.params.u.ofdm.code_rate_HP = tuning.params.u.ofdm.code_rate_HP;
        prev_tuning.params.u.ofdm.code_rate_LP = tuning.params.u.ofdm.code_rate_LP;
        prev_tuning.params.u.ofdm.constellation = tuning.params.u.ofdm.constellation;
        prev_tuning.params.u.ofdm.transmission_mode = tuning.params.u.ofdm.transmission_mode;
        prev_tuning.params.u.ofdm.guard_interval = tuning.params.u.ofdm.guard_interval;
        prev_tuning.params.u.ofdm.hierarchy_information = tuning.params.u.ofdm.hierarchy_information;
        havetuned = true;
    }

    return true;
}

