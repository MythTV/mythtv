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
 *      Taylor Jacob (rtjacob at earthlink.net)
 *          - Changed tuning and DB structure
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
#include "mythdbcon.h"
#include "tv_rec.h"

#include "dvbtypes.h"
#include "dvbdev.h"
#include "dvbchannel.h"
#include "dvbrecorder.h"
#include "dvbdiseqc.h"
#include "dvbcam.h"
#include "dvbsiparser.h"

//Timeout between checking for a tuning lock micro seconds
#define TUNER_INTERVAL 300000

DVBChannel::DVBChannel(int aCardNum, TVRec *parent)
           : ChannelBase(parent), cardnum(aCardNum)
{
    fd_frontend = -1;
    force_channel_change = false;
    diseqc = NULL;
    currentTID = -1;

    stopTuning = false;

    siparser = NULL;
    dvbcam = new DVBCam(cardnum);

    pthread_mutex_init(&chan_opts.lock, NULL);
}

DVBChannel::~DVBChannel()
{
    CloseDVB();

    delete dvbcam;

    pthread_mutex_destroy(&chan_opts.lock);
}

void *DVBChannel::SpawnSectionReader(void *param)
{
    DVBSIParser *siparser = (DVBSIParser *)param;
    siparser->StartSectionReader();
    return NULL;
}

void DVBChannel::CloseDVB()
{
    VERBOSE(VB_ALL, "Closing DVB channel");
    if (fd_frontend >= 0)
    {
        close(fd_frontend);
        fd_frontend = -1;

        dvbcam->Stop();

        if (siparser)
        {
            siparser->StopSectionReader();
            pthread_join(siparser_thread, NULL);
            delete siparser;
            siparser = NULL;
        }

        if (diseqc)
        {
            delete diseqc;
            diseqc = NULL;
        }
    }
}

void DVBChannel::StopTuning()
{
    stopTuning = true;
}

bool DVBChannel::Open()
{
    if (siparser == NULL )
    {
        // TODO: Rename sections to PMap and have it ONLY pull the ProgramMap
        siparser = new DVBSIParser(cardnum);
        pthread_create(&siparser_thread,NULL,SpawnSectionReader, siparser);

        connect(siparser, SIGNAL(UpdatePMT(const PMTObject*)),
                this, SLOT(SetPMT(const PMTObject*)));
    }

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
        close(fd_frontend);
        fd_frontend = -1;
        return false;
    }

    GENERAL(QString("Using DVB card %1, with frontend %2.")
            .arg(cardnum).arg(info.name));

    // Turn on the power to the LNB
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
    
    dvbcam->Start();

    force_channel_change = true;
    first_tune = true;

    return (fd_frontend >= 0 );
}

/** \fn DVBChannel::SetTransportByInt(int mplexid)
 *  \brief To be used by setup sdt/nit scanner and eit parser.
 *
 *   mplexid is how the db indexes each transport
 */
bool DVBChannel::SetTransportByInt(int mplexid)
{
    currentTID = -1;

    if (!GetTransportOptions(mplexid))
        return false;

    CheckOptions();
    PrintChannelOptions();

    if (!TuneTransport(chan_opts))
        return false;


printf("Setting currenTID = %d\n",mplexid);
    currentTID = mplexid;

    return true;
}

bool DVBChannel::SetChannelByString(const QString &chan)
{
    if (curchannelname == chan && !force_channel_change)
        return true;

    if (fd_frontend < 0)
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
    curchannelname = chan;

    GENERAL(QString("Successfully tuned to channel %1.").arg(chan));

    if (!chan_opts.pmt.OnAir())
    {
        ERROR(QString("Channel #%1 is off air.").arg(chan));
        return false;
    }

    inputChannel[currentcapchannel] = curchannelname;

    return true;
}

// TODO: Look at better communication with recorder so that only PMAP needs tobe passed
void DVBChannel::RecorderStarted()
{
    emit ChannelChanged(chan_opts);
}

void DVBChannel::SwitchToInput(const QString &input, const QString &chan)
{
    currentcapchannel = 0;
    if (channelnames.empty())
       channelnames[currentcapchannel] = input;

    SetChannelByString(chan);
}

/** \fn DVBChannel::GetChannelOptions(const QString&)
 *  \brief This function called when tuning to a specific program not 
 *         just the transport in general.
 */
bool DVBChannel::GetChannelOptions(const QString& channum)
{
    QString         thequery;
    QString         inputName;

// TODO: pParent doesn't exist when you create a DVBChannel from videosource
//       but this is important (i think) for remote backends

    MSqlQuery query(MSqlQuery::InitCon());

    if (pParent == NULL)
        thequery = QString("SELECT chanid, serviceid, mplexid "
                       "FROM channel,cardinput,capturecard WHERE "
                       "channel.channum='%1' AND "
                       "cardinput.sourceid = channel.sourceid AND "
                       "cardinput.videodevice = '%2' AND "
                       "capturecard.cardid = cardinput.cardid AND "
                       "capturecard.cardtype = 'DVB'")
                       .arg(channum).arg(cardnum);
    else
        thequery = QString("SELECT chanid, serviceid, mplexid "
                       "FROM channel,cardinput,capturecard WHERE "
                       "channel.channum='%1' AND "
                       "cardinput.sourceid = channel.sourceid AND "
                       "cardinput.cardid = '%2' AND "
                       "capturecard.cardid = cardinput.cardid AND "
                       "capturecard.cardtype = 'DVB'")
                       .arg(channum).arg(pParent->GetCaptureCardNum());

    query.prepare(thequery);

    if (!query.exec() || !query.isActive())
        MythContext::DBError("GetChannelOptions - ChanID", query);

    if (query.size() <= 0)
    {
        ERROR("Unable to find channel in database.");
        return false;
    }

    query.next();
    // TODO: Fix structs to be more useful to new DB structure
    chan_opts.serviceID     = query.value(1).toInt();

    int mplexid = query.value(2).toInt();

    if (!GetTransportOptions(mplexid))
        return false;

    currentTID = mplexid;

    return true;
}

bool DVBChannel::GetTransportOptions(int mplexid)
{
    MSqlQuery query(MSqlQuery::InitCon());
    QString thequery;

     // TODO: See previous comment about pParent
    int capturecardnumber;
    if (pParent == NULL) {
       thequery = QString("SELECT cardid FROM capturecard "
                          " WHERE videodevice = %1;").arg(cardnum);
       query.prepare(thequery);
       if (!query.exec() || !query.isActive() || query.size() <= 0)
       {
           ERROR(QString("Could not find capture card for transport %1")
                      .arg(mplexid));
           return false;
       }
       query.next();
       capturecardnumber = query.value(0).toInt();
    }
    else
       capturecardnumber = pParent->GetCaptureCardNum();

    switch(info.type)
    {
        case FE_QPSK:
            thequery = QString("SELECT frequency, inversion, symbolrate, "
                               "fec, polarity, dvb_diseqc_type, diseqc_port, "
                               "diseqc_pos, lnb_lof_switch, lnb_lof_hi, "
                               "lnb_lof_lo,sistandard FROM dtv_multiplex,cardinput,"
                               "capturecard WHERE mplexid=%1 AND "
                               "dtv_multiplex.sourceid = cardinput.sourceid "
                               "AND capturecard.cardid=%2")
                               .arg(mplexid)
                               .arg(capturecardnumber);
            break;
        case FE_QAM:
            thequery = QString("SELECT frequency, inversion, symbolrate, "
                               "fec, modulation, sistandard "
                               "FROM dtv_multiplex,cardinput,"
                               "capturecard WHERE mplexid=%1 AND "
                               "dtv_multiplex.sourceid = cardinput.sourceid "
                               "AND capturecard.cardid=%2")
                               .arg(mplexid)
                               .arg(capturecardnumber);
            break;
        case FE_OFDM:
            thequery = QString("SELECT frequency, inversion, "
                               "bandwidth, hp_code_rate, lp_code_rate, "
                               "constellation, transmission_mode, "
                               "guard_interval, hierarchy, sistandard "
                               "FROM dtv_multiplex,cardinput,"
                               "capturecard WHERE mplexid=%1 AND "
                               "dtv_multiplex.sourceid = cardinput.sourceid "
                               "AND capturecard.cardid=%2")
                               .arg(mplexid)
                               .arg(capturecardnumber);
            break;
#if (DVB_API_VERSION_MINOR == 1)
        case FE_ATSC:
            thequery = QString("SELECT frequency, modulation, sistandard "
                               "FROM dtv_multiplex,cardinput,"
                               "capturecard WHERE mplexid=%1 AND "
                               "dtv_multiplex.sourceid = cardinput.sourceid "
                               "AND capturecard.cardid=%2")
                               .arg(mplexid)
                               .arg(capturecardnumber);
            break;
#endif
    }

    query.prepare(thequery);

    if (!query.exec() || !query.isActive())
        MythContext::DBError("GetChannelOptions - Options", query);

    if (query.size() <= 0)
    {
       ERROR(QString("Could not find dvb tuning parameters for transport %1")
                      .arg(mplexid));
       return false;
    }

    query.next();

    if (!ParseTransportQuery(query))
    {
        return false;
    }
    return true;
}

void DVBChannel::PrintChannelOptions()
{
    DVBTuning& t = chan_opts.tuning;

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

            // TODO: Make DiSEqC class print options here as well
            // NOTE: Diseqc and LNB is handled by DVBDiSEqC.
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

#if (DVB_API_VERSION_MINOR == 1)
        case FE_ATSC:

            msg += " Mod:";
            switch(t.params.u.qam.modulation) {
                case QPSK: msg += "QPSK"; break;
                case QAM_AUTO: msg += "Auto"; break;
                case QAM_256: msg += "256"; break;
                case QAM_128: msg += "128"; break;
                case QAM_64: msg += "64"; break;
                case QAM_32: msg += "32"; break;
                case QAM_16: msg += "16"; break;
                case VSB_8: msg += "8VSB"; break;
                case VSB_16: msg += "16VSB"; break;
            }
#endif
            break;

        case FE_OFDM:
            msg = QString("Frequency: %1.").arg(t.params.frequency);

            msg += " BW:";
            switch(t.params.u.ofdm.bandwidth) {
                case BANDWIDTH_AUTO: msg += "Auto"; break;
                case BANDWIDTH_8_MHZ: msg += "8MHz"; break;
                case BANDWIDTH_7_MHZ: msg += "7MHz"; break;
                case BANDWIDTH_6_MHZ: msg += "6MHz"; break;
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
#if (DVB_API_VERSION_MINOR == 1)
                case VSB_8: msg += "8vsb"; break;
                case VSB_16: msg += "16vsb"; break;
#endif
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
    DVBTuning& t = chan_opts.tuning;

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
#if (DVB_API_VERSION_MINOR == 1)
       case FE_ATSC:

            // ATSC doesn't need any validation
#endif
            break;
    }

    if (info.type != FE_OFDM &&
       (symbol_rate < info.symbol_rate_min ||
        symbol_rate > info.symbol_rate_max))
    {
        WARNING(QString("Symbol Rate setting (%1) is "
                        "out of range (min/max:%2/%3)")
                        .arg(symbol_rate)
                        .arg(info.symbol_rate_min)
                        .arg(info.symbol_rate_max));
    }
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
#if (DVB_API_VERSION_MINOR == 1)
        case VSB_8:     if (info.caps & FE_CAN_8VSB)        return true; break;
        case VSB_16:    if (info.caps & FE_CAN_16VSB)       return true; break;
#endif

    }
    return false;
}

/*****************************************************************************
        Query parser functions for each of the three types of cards.
 *****************************************************************************/

bool DVBChannel::ParseTransportQuery(MSqlQuery& query)
{

    pthread_mutex_lock(&chan_opts.lock);

    switch(info.type)
    {
        case FE_QPSK:
            if (!chan_opts.tuning.parseQPSK(query.value(0).toString(), 
                          query.value(1).toString(),
                          query.value(2).toString(), query.value(3).toString(),
                          query.value(4).toString(), query.value(5).toString(),
                          query.value(6).toString(), query.value(7).toString(),
                          query.value(8).toString(), query.value(9).toString(),
                          query.value(10).toString()))
            {
                pthread_mutex_unlock(&chan_opts.lock);
                return false;
            }
            chan_opts.sistandard = query.value(11).toString();
            break;
        case FE_QAM:
            if (!chan_opts.tuning.parseQAM(query.value(0).toString(), 
                        query.value(1).toString(), query.value(2).toString(),
                        query.value(3).toString(), query.value(4).toString()))
            {
                pthread_mutex_unlock(&chan_opts.lock);
                return false;
            }
            chan_opts.sistandard = query.value(5).toString();
            break;
        case FE_OFDM:
            if (!chan_opts.tuning.parseOFDM(query.value(0).toString(), 
                        query.value(1).toString(), query.value(2).toString(),
                        query.value(3).toString(), query.value(4).toString(),
                        query.value(5).toString(), query.value(6).toString(),
                        query.value(7).toString(), query.value(8).toString()))
            {
                pthread_mutex_unlock(&chan_opts.lock);
                return false;
            }
            chan_opts.sistandard = query.value(9).toString();
            break;
#if (DVB_API_VERSION_MINOR == 1)
        case FE_ATSC:
            if (!chan_opts.tuning.parseATSC(query.value(0).toString(), 
                                            query.value(1).toString()))
            {
                pthread_mutex_unlock(&chan_opts.lock);
                return false;
            }
            chan_opts.sistandard = query.value(2).toString();
            break;
#endif
    }

    pthread_mutex_unlock(&chan_opts.lock);

    return true;
}

/*****************************************************************************
  PMT Handler Code 
*****************************************************************************/

void DVBChannel::SetPMT(const PMTObject *pmt)
{
    pthread_mutex_lock(&chan_opts.lock);

    chan_opts.pmt = *pmt;
    chan_opts.PMTSet = true;

    pthread_mutex_unlock(&chan_opts.lock);

    // Send the PMT to recorder (needs to be cleaned up some)
    emit ChannelChanged(chan_opts);
}

void DVBChannel::SetCAPMT(const PMTObject *pmt)
{
    // This is called from DVBRecorder
    // when AutoPID is complete

    // Set the PMT for the CAM
    if (dvbcam->IsRunning())
        dvbcam->SetPMT(pmt);
}

/*****************************************************************************
           Tuning functions for each of the three types of cards.
 *****************************************************************************/
bool DVBChannel::Tune(dvb_channel_t& channel, bool all)
{
    // This function allows you to tune to more than just a transport, and also
    // verifies the service exists, and get the PIDs if auto-pid is turned on
    SetPMTSet(false);

    if (!TuneTransport(channel,all))
        return false;

    GENERAL("Multiplex Locked");

    siparser->AddPMT(channel.serviceID);


// TODO: Pick a more reasonable time here
    for (int x = 0; x < 3000 ; x++)
    {
        pthread_mutex_lock(&chan_opts.lock);
        if (channel.PMTSet == true)
        {
            pthread_mutex_unlock(&chan_opts.lock);
            return true;
        }
        pthread_mutex_unlock(&chan_opts.lock);
        usleep(100);
    }

    GENERAL("Timeout Getting PMT");
    return false;
}

/** \fn DVBChannel::TuneTransport(dvb_channel_t&, bool, int)
 *  \brief Tunes the card to a transport but does not deal with PIDs.
 *
 *   This is used by DVB Channel Scanner, the EIT Parser, and by TVRec.
 *
 *  \param channel Info on transport to tune to
 *  \param all     If true frequency tuning is done even if not strictly needed.
 */
bool DVBChannel::TuneTransport(dvb_channel_t& channel, bool all, int timeout)
{

    if (stopTuning)
        return false;

    DVBTuning& tuning = channel.tuning;

    if (fd_frontend < 0)
    {
        ERROR("Card not open!");
        return false;
    }

    bool reset      = false;
    bool havetuned  = false;
    bool tune       = true;

    int max_tune_timeout_count = (timeout*1000)/TUNER_INTERVAL;

    if (all == true)
        first_tune = true;
    if (first_tune)
    {
        reset      = true;
        first_tune = false;
    }

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
#if (DVB_API_VERSION_MINOR == 1)
                case FE_ATSC:
                    if (!TuneATSC(tuning, reset, havetuned))
                        return false;
                    break;
#endif
            }

            /* Stop the SIParser.  It will be resumed as soon as a lock is
               obtained.  For DVB-C/T/S with no Dish Movement it could be
               immediatly started */
            siparser->Reset();

            if (havetuned == false)
            {
                /* Start the SIParser back now that a lock has been obtained */
                if (channel.sistandard == "atsc")
                    siparser->FillPMap(SI_STANDARD_ATSC);
                else
                    siparser->FillPMap(SI_STANDARD_DVB);
                return true;
            }

            tune = false;
            reset = false;

            CHANNEL("Waiting for frontend event after tune.");
        }

        fe_status_t stat;
        int timeout = 0;
        QString status = "Status: ";

        do {
            if (ioctl(fd_frontend, FE_READ_STATUS, &stat) < 0)
                perror("FE_READ_STATUS failed");
            if (fd_frontend < 0)
                return false;
            else if (!(timeout % (1000000/TUNER_INTERVAL)))
            {
                uint16_t ss;
                uint16_t snr;
                uint32_t ber;
                uint32_t ub;
                ioctl(fd_frontend, FE_READ_SIGNAL_STRENGTH, &ss);
                ioctl(fd_frontend, FE_READ_SNR, &snr);
                ioctl(fd_frontend, FE_READ_BER, &ber);
                ioctl(fd_frontend, FE_READ_UNCORRECTED_BLOCKS, &ub);
                QString msg = QString("DVB signal %1 | snr %2 | ber %3 | unc %4").arg(ss,2,16).arg(snr,2,16).arg(ber,4,16).arg(ub,4,16);
                GENERAL(msg);
            }

            if (stat & FE_HAS_LOCK)
            {
                status += "LOCK.";
                GENERAL(status);
                /* Start the SIParser back now that a lock has been obtained */
                if (channel.sistandard == "atsc")
                    siparser->FillPMap(SI_STANDARD_ATSC);
                else
                    siparser->FillPMap(SI_STANDARD_DVB);
                return true;
            }

            usleep(TUNER_INTERVAL);
        } while ((++timeout <= max_tune_timeout_count) && (!stopTuning));

        status += "NO LOCK!";
        WARNING(status);
        return false;
    }
}

bool DVBChannel::GetTuningParams(DVBTuning& tuning) const
{
    if (fd_frontend < 0)
    {
        ERROR("Card not open!");
        return false;
    }
    if (ioctl(fd_frontend, FE_GET_FRONTEND, &tuning.params) < 0)
    {
        ERRNO("Getting Frontend failed.");
        return false;
    }
    return true;
}

bool DVBChannel::TuneQPSK(DVBTuning& tuning, bool reset, bool& havetuned)
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

bool DVBChannel::TuneATSC(DVBTuning& tuning, bool reset, bool& havetuned)
{
#if (DVB_API_VERSION_MINOR == 1)
    if (reset ||
        prev_tuning.params.frequency != tuning.params.frequency ||
        prev_tuning.params.u.vsb.modulation != tuning.params.u.vsb.modulation)
    {
        if (ioctl(fd_frontend, FE_SET_FRONTEND, &tuning.params) < 0)
        {
            ERRNO("Setting Frontend failed.");
            return false;
        }
        prev_tuning.params.frequency = tuning.params.frequency;
        prev_tuning.params.u.vsb.modulation  = tuning.params.u.vsb.modulation;
        havetuned = true;
    }
#else
    (void)tuning;
    (void)reset;
    (void)havetuned;
#endif
    return true;
}

bool DVBChannel::TuneQAM(DVBTuning& tuning, bool reset, bool& havetuned)
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

bool DVBChannel::TuneOFDM(DVBTuning& tuning, bool reset, bool& havetuned)
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

