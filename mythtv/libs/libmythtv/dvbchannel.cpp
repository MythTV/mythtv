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
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>

#include "RingBuffer.h"
#include "recorderbase.h"
#include "mythcontext.h"
#include "mythdbcon.h"
#include "tv_rec.h"
#include "videosource.h"

#include "dvbtypes.h"
#include "dvbdev.h"
#include "dvbchannel.h"
#include "dvbrecorder.h"
#include "dvbdiseqc.h"
#include "dvbcam.h"

#if (DVB_API_VERSION_MINOR <= 3 && DVB_API_VERSION_MINOR == 0)
#    define FE_ATSC       (FE_OFDM+1)
#    define FE_CAN_8VSB   0x200000
#    define FE_CAN_16VSB  0x400000
#    define VSB_8         (QAM_AUTO+1)
#    define VSB_16        (QAM_AUTO+2)
#endif

static uint tuned_frequency(const DVBTuning&, fe_type_t, fe_sec_tone_mode_t *);
static void drain_dvb_events(int fd);
static bool wait_for_backend(int fd, int timeout_ms);
static bool handle_diseq(const DVBTuning&, DVBDiSEqC*, bool reset);

/** \class DVBChannel
 *  \brief Provides interface to the tuning hardware when using DVB drivers
 *
 *  \bug Only supports single input cards.
 */
DVBChannel::DVBChannel(int aCardNum, TVRec *parent)
    : QObject(NULL, "DVBChannel"), ChannelBase(parent),
      diseqc(NULL),    dvbcam(NULL),
      fd_frontend(-1), cardnum(aCardNum), has_crc_bug(false),
      currentTID(-1),  first_tune(true)
{
    dvbcam = new DVBCam(cardnum);
    bzero(&info, sizeof(info));
    has_crc_bug = CardUtil::HasDVBCRCBug(aCardNum);

    // print device name so that we can detect driver specific bugs...
    QString device_name("");
    QString dummy("");
    CardUtil::GetDVBType(aCardNum, device_name, dummy);
    CHANNEL("Device Name: '"<<device_name<<"'");
}

DVBChannel::~DVBChannel()
{
    Close();
    delete dvbcam;
}

void DVBChannel::Close()
{
    CHANNEL("Closing DVB channel");

    if (fd_frontend >= 0)
    {
        close(fd_frontend);
        fd_frontend = -1;

        dvbcam->Stop();

        if (diseqc)
        {
            delete diseqc;
            diseqc = NULL;
        }
    }
}

bool DVBChannel::Open()
{
    CHANNEL("Opening DVB channel");
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

    first_tune = true;

    return (fd_frontend >= 0 );
}

/** \fn DVBChannel::TuneMultiplex(uint mplexid)
 *  \brief To be used by setup sdt/nit scanner and eit parser.
 *
 *   mplexid is how the db indexes each transport
 */
bool DVBChannel::TuneMultiplex(uint mplexid)
{
    currentTID = -1;

    if (!GetTransportOptions(mplexid))
        return false;

    CheckOptions();
    CHANNEL(chan_opts.tuning.toString(info.type));

    if (!Tune(chan_opts))
        return false;

    CHANNEL(QString("Setting mplexid = %1").arg(mplexid));
    currentTID = mplexid;

    return true;
}

bool DVBChannel::SetChannelByString(const QString &chan)
{
    if (fd_frontend < 0)
        return false;

    if (curchannelname == chan && !first_tune)
        return true;

#if (DVB_API_VERSION_MINOR == 1)
    if (FE_ATSC == info.type)
        SetCachedATSCInfo("");
#endif

    CHANNEL(QString("Trying to tune to channel %1.").arg(chan));

    if (GetChannelOptions(chan) == false)
    {
        ERROR(QString("Failed to get channel options for channel %1.")
              .arg(chan));
        return false;
    }

    CheckOptions();
    CHANNEL(chan_opts.tuning.toString(info.type));

    if (!Tune(chan_opts))
    {
        ERROR(QString("Tuning to frequency for channel %1.").arg(chan));
        return false;
    }
    curchannelname = chan;

    CHANNEL(QString("Tuned to frequency for channel %1.").arg(chan));

    inputChannel[currentcapchannel] = curchannelname;

    return true;
}

// TODO: Look at better communication with recorder so that only PMAP needs tobe passed
void DVBChannel::RecorderStarted()
{
    if (chan_opts.IsPMTSet())
        emit ChannelChanged(chan_opts);
}

bool DVBChannel::SwitchToInput(const QString &input, const QString &chan)
{
    currentcapchannel = 0;
    if (channelnames.empty())
       channelnames[currentcapchannel] = input;

    return SetChannelByString(chan);
}

/** \fn DVBChannel::GetChannelOptions(const QString&)
 *  \brief This function called when tuning to a specific stream.
 */
bool DVBChannel::GetChannelOptions(const QString& channum)
{
    MSqlQuery query(MSqlQuery::InitCon());

    int cardid = GetCardID();

    QString thequery =
        QString("SELECT chanid, serviceid, mplexid, atscsrcid "
                "FROM channel, cardinput, capturecard "
                "WHERE channel.channum='%1' AND "
                "      cardinput.sourceid = channel.sourceid AND "
                "      capturecard.cardid = cardinput.cardid AND "
                "      capturecard.cardtype = 'DVB' AND "
                "      cardinput.cardid = '%2'")
        .arg(channum).arg(cardid);

    query.prepare(thequery);

    if (!query.exec() || !query.isActive())
    {
        MythContext::DBError("GetChannelOptions - ChanID", query);
        return false;
    }
    if (query.size() <= 0)
    {
        ERROR("Unable to find channel in database.");
        return false;
    }

    query.next();
    // TODO: Fix structs to be more useful to new DB structure
    chan_opts.serviceID = query.value(1).toInt();
    if (query.value(3).toInt() > 256)
    {
        SetCachedATSCInfo(QString("%1_%2")
                          .arg(query.value(3).toInt() >> 8)
                          .arg(query.value(3).toInt() & 0xff));
    }
    currentProgramNum = chan_opts.serviceID;

    int mplexid = query.value(2).toInt();

    if (!GetTransportOptions(mplexid))
        return false;

    currentTID = mplexid;

    return true;
}

/** \fn DVBChannel::GetTransportOptions(int mplexid)
 *  \brief Initializes chan_opts.tuning from database using mplexid.
 *
 *  \todo pParent doesn't exist when you create a DVBChannel from videosource
 *        but this is important (i think) for remote backends
 *  \return true on success and false on failure
 */
bool DVBChannel::GetTransportOptions(int mplexid)
{
    MSqlQuery query(MSqlQuery::InitCon());

    int capturecardnumber = GetCardID();

    // Query for our DVBTuning params
    QString thequery(
        "SELECT frequency,         inversion,      symbolrate, "
        "       fec,               polarity,       dvb_diseqc_type, "
        "       diseqc_port,       diseqc_pos,     lnb_lof_switch, "
        "       lnb_lof_hi,        lnb_lof_lo,     sistandard, "
        "       hp_code_rate,      lp_code_rate,   constellation, "
        "       transmission_mode, guard_interval, hierarchy, "
        "       modulation,        bandwidth "
        "FROM dtv_multiplex, cardinput, capturecard "
        "WHERE dtv_multiplex.sourceid = cardinput.sourceid AND ");

    thequery += QString("mplexid = '%1' AND capturecard.cardid = '%2'")
        .arg(mplexid).arg(capturecardnumber);

    query.prepare(thequery);
    if (!query.exec() || !query.isActive())
    {
        MythContext::DBError("GetChannelOptions - Options", query);
        return false;
    }
    if (query.size() <= 0)
    {
       ERROR(QString("Could not find dvb tuning parameters for transport %1")
                      .arg(mplexid));
       return false;
    }
    query.next();

    // Parse the query into our DVBTuning class
    return chan_opts.Parse(info.type,
        query.value(0).toString(),  query.value(1).toString(),
        query.value(2).toString(),  query.value(3).toString(),
        query.value(4).toString(),  query.value(5).toString(),
        query.value(6).toString(),  query.value(7).toString(),
        query.value(8).toString(),  query.value(9).toString(),
        query.value(10).toString(), query.value(11).toString(),
        query.value(12).toString(), query.value(13).toString(),
        query.value(14).toString(), query.value(15).toString(),
        query.value(16).toString(), query.value(17).toString(),
        query.value(18).toString(), query.value(19).toString());
}

/** \fn DVBChannel::CheckOptions()
 *  \brief Checks chan_opts.tuning for problems, and tries to fix them.
 */
void DVBChannel::CheckOptions()
{
    DVBTuning& t = chan_opts.tuning;

    if ((t.params.inversion == INVERSION_AUTO) &&
        !(info.caps & FE_CAN_INVERSION_AUTO))
    {
        WARNING("Unsupported inversion option 'auto',"
                " falling back to 'off'");
        t.params.inversion = INVERSION_OFF;
    }

    uint frequency = tuned_frequency(t, info.type, NULL);

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
            symbol_rate = t.QPSKSymbolRate();

            if (!CheckCodeRate(t.params.u.qpsk.fec_inner))
                WARNING("Unsupported fec_inner parameter.");
            break;
        case FE_QAM:
            symbol_rate = t.QAMSymbolRate();

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
       case FE_ATSC:
            // ATSC doesn't need any validation
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

/** \fn DVBChannel::CheckCodeRate(fe_code_rate_t) const
 *  \brief Return true iff rate is supported rate on the frontend
 */
bool DVBChannel::CheckCodeRate(fe_code_rate_t rate) const
{
    switch (rate)
    {
        case FEC_1_2:  if (info.caps & FE_CAN_FEC_1_2)  return true;
        case FEC_2_3:  if (info.caps & FE_CAN_FEC_2_3)  return true;
        case FEC_3_4:  if (info.caps & FE_CAN_FEC_3_4)  return true;
        case FEC_4_5:  if (info.caps & FE_CAN_FEC_4_5)  return true;
        case FEC_5_6:  if (info.caps & FE_CAN_FEC_5_6)  return true;
        case FEC_6_7:  if (info.caps & FE_CAN_FEC_6_7)  return true;
        case FEC_7_8:  if (info.caps & FE_CAN_FEC_7_8)  return true;
        case FEC_8_9:  if (info.caps & FE_CAN_FEC_8_9)  return true;
        case FEC_AUTO: if (info.caps & FE_CAN_FEC_AUTO) return true;
        case FEC_NONE: return true;
    }
    return false;
}

/** \fn DVBChannel::CheckModulation(fe_modulation_t) const
 *  \brief Return true iff modulation is supported modulation on the frontend
 */
bool DVBChannel::CheckModulation(fe_modulation_t modulation) const
{
    switch (modulation)
    {
        case QPSK:     if (info.caps & FE_CAN_QPSK)     return true;
        case QAM_16:   if (info.caps & FE_CAN_QAM_16)   return true;
        case QAM_32:   if (info.caps & FE_CAN_QAM_32)   return true;
        case QAM_64:   if (info.caps & FE_CAN_QAM_64)   return true;
        case QAM_128:  if (info.caps & FE_CAN_QAM_128)  return true;
        case QAM_256:  if (info.caps & FE_CAN_QAM_256)  return true;
        case QAM_AUTO: if (info.caps & FE_CAN_QAM_AUTO) return true;
        case VSB_8:    if (info.caps & FE_CAN_8VSB)     return true;
        case VSB_16:   if (info.caps & FE_CAN_16VSB)    return true;
    }
    return false;
}

/*****************************************************************************
  PMT Handler Code 
*****************************************************************************/

/** \fn DVBChannel::SetPMT(const PMTObject*)
 *  \brief Sets our PMT to a copy of the PMTObject, and emits
 *         a ChannelChanged(dvb_channel_t&) signal.
 */
void DVBChannel::SetPMT(const PMTObject *pmt)
{
    if (pmt)
    {
        CHANNEL(QString("SetPMT  ServiceID=%1, PCRPID=%2 (0x%3)")
                .arg(pmt->ServiceID).arg(pmt->PCRPID)
                .arg(((uint)pmt->PCRPID),0,16));
    }

    chan_opts.SetPMT(pmt);
    // Send the PMT to recorder (needs to be cleaned up some)
    if (pmt)
        emit ChannelChanged(chan_opts);
}

/** \fn DVBChannel::SetCAPMT(const PMTObject*)
 *  \brief Tells the Conditional Access Module which streams we wish to decode.
 *
 *   If dvbcam exists and DVBCam::IsRunning() returns true we tell it
 *   about our PMT, otherwise we do nothing.
 */
void DVBChannel::SetCAPMT(const PMTObject *pmt)
{
    // This is called from DVBRecorder
    // when AutoPID is complete

    // Set the PMT for the CAM
    if (dvbcam->IsRunning())
        dvbcam->SetPMT(pmt);
}

/*****************************************************************************
           Tuning functions for each of the four types of cards.
 *****************************************************************************/

/** \fn DVBChannel::Tune(const dvb_channel_t&, bool)
 *  \brief Tunes the card to a frequency but does not deal with PIDs.
 *
 *   This is used by DVB Channel Scanner, the EIT Parser, and by TVRec.
 *
 *  \param channel     Info on transport to tune to
 *  \param force_reset If true frequency tuning is done even if not strictly needed
 *  \return true on success, false on failure
 */
bool DVBChannel::Tune(const dvb_channel_t& channel, bool force_reset)
{
    bool reset = (force_reset || first_tune);
    bool has_diseq = (FE_QPSK == info.type) && diseqc;
    struct dvb_frontend_parameters params = channel.tuning.params;

    if (fd_frontend < 0)
    {
        ERROR("DVBChannel::Tune: Card not open!");
        return false;
    }

    // We are now waiting for a new PMT to forward to Access Control (dvbcam).
    SetPMT(NULL);

    // Remove any events in queue before tuning.
    drain_dvb_events(fd_frontend);

    // Send DisEq commands to external hardware if we need to.
    if (has_diseq && !handle_diseq(channel.tuning, diseqc, reset))
    {
        ERROR("DVBChannel::Tune: Failed to transmit DisEq commands");
        return false;
    }

    CHANNEL("Old Params: "<<toString(prev_tuning.params, info.type));
    CHANNEL("New Params: "<<toString(channel.tuning.params, info.type));

    if (reset || !prev_tuning.equal(info.type, channel.tuning, 500000))
    {
        // Adjust for Satelite recievers which offset the frequency.
        params.frequency = tuned_frequency(channel.tuning, info.type, NULL);

        if (ioctl(fd_frontend, FE_SET_FRONTEND, &params) < 0)
        {
            ERRNO("DVBChannel::Tune: "
                  "Setting Frontend tuning parameters failed.");
            return false;
        }
        wait_for_backend(fd_frontend, 5000);

        prev_tuning.params = params;
        first_tune = false;
    }

    CHANNEL("DVBChannel::Tune: Frequency tuning successful.");

    return true;
}

/** \fn DVBChannel::GetTuningParams(DVBTuning& tuning) const
 *  \brief Fetches DVBTuning params from driver
 *  \return true on success, false on failure
 */
bool DVBChannel::GetTuningParams(DVBTuning& tuning) const
{
    if (fd_frontend < 0)
    {
        ERROR("Card not open!");
        return false;
    }

    if (ioctl(fd_frontend, FE_GET_FRONTEND, &tuning.params) < 0)
    {
        ERRNO("Getting Frontend tuning parameters failed.");
        return false;
    }
    return true;
}

/** \fn DVBChannel::GetCardID() const
 *  \brief Returns card id.
 */
int DVBChannel::GetCardID() const
{
    if (pParent)
        return pParent->GetCaptureCardNum();
    else
        return CardUtil::GetCardID(GetDevice());
}

/** \fn DVBChannel::GetChanID() const
 *  \brief Returns Channel ID
 *  \bug This only works if there is only one input on the card.
 */
int DVBChannel::GetChanID() const
{
    int cardid = GetCardID();

    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("SELECT chanid "
                  "FROM channel, cardinput "
                  "WHERE cardinput.sourceid = channel.sourceid AND "
                  "      channel.channum = :CHANNUM AND "
                  "      cardinput.cardid = :CARDID");

    query.bindValue(":CHANNUM", curchannelname);
    query.bindValue(":CARDID",  cardid);

    if (!query.exec() || !query.isActive())
    {
        MythContext::DBError("fetching chanid", query);
        return -1;
    }

    if (query.size() <= 0)
        return -1;

    query.next();
    return query.value(0).toInt();
}

void DVBChannel::SaveCachedPids(const pid_cache_t &pid_cache) const
{
    int chanid = GetChanID();
    if (chanid >= 0)
        ChannelBase::SaveCachedPids(chanid, pid_cache);
}

void DVBChannel::GetCachedPids(pid_cache_t &pid_cache) const
{
    int chanid = GetChanID();
    if (chanid >= 0)
        ChannelBase::GetCachedPids(chanid, pid_cache);
}

/** \fn drain_dvb_events(int)
 *  \brief Reads all the events off the queue, so we can use select
 *         in wait_for_backend(int,int).
 */
static void drain_dvb_events(int fd)
{
    struct dvb_frontend_event event;
    while (ioctl(fd, FE_GET_EVENT, &event) == 0);
}

static uint tuned_frequency(const DVBTuning &tuning, fe_type_t type,
                            fe_sec_tone_mode_t *p_tone)
{
    if (FE_QPSK != type)
        return tuning.Frequency();

    uint freq   = tuning.Frequency();
    bool tone   = freq >= tuning.lnb_lof_switch;
    uint lnb_hi = (uint) abs((int)freq - (int)tuning.lnb_lof_hi);
    uint lnb_lo = (uint) abs((int)freq - (int)tuning.lnb_lof_lo);

    if (p_tone)
        *p_tone = (tone) ? SEC_TONE_ON : SEC_TONE_OFF;

    return (tone) ? lnb_hi : lnb_lo;
}

/** \fn wait_for_backend(int,int)
 *  \brief Waits for backend to get tune message.
 *
 *   With linux 2.6.12 or later this should block
 *   until the backend is tuned to the proper frequency.
 *
 *   With earlier kernels sleeping for timeout_ms may save us.
 *
 *   Waiting for DVB events has been tried, but this fails
 *   with several DVB cards. They either don't send the
 *   required events or delete them from the event queue
 *   before we can read the event.
 *
 *   Using a FE_GET_FRONTEND has also been tried, but returns
 *   the old data until a 2 sec timeout elapses on at least 
 *   one DVB card.
 *
 *   We do not block until we have a lock, the signal
 *   monitor does that.
 *
 *  \param fd         frontend file descriptor
 *  \param timeout_ms timeout before FE_READ_STATUS in milliseconds
 */
static bool wait_for_backend(int fd, int timeout_ms)
{
    struct timeval select_timeout = { 0, (timeout_ms) % 1000 };
    fd_set fd_select_set;
    FD_ZERO(    &fd_select_set);
    FD_SET (fd, &fd_select_set);

    // Try to wait for some output like an event, unfortunately
    // this fails on several DVB cards, so we have a timeout.
    select(fd+1, &fd_select_set, NULL, NULL, &select_timeout);

    // This is supposed to work on all cards, post 2.6.12...
    fe_status_t status;
    if (ioctl(fd, FE_READ_STATUS, &status) < 0)
    {
        VERBOSE(VB_IMPORTANT, QString("dvbchannel.cpp:wait_for_backend: "
                                      "Failed to get status, error: %1")
                .arg(strerror(errno)));
        return false;
    }

    VERBOSE(VB_CHANNEL, QString("dvbchannel.cpp:wait_for_backend: Status: %1")
            .arg(toString(status)));
    return true;
}

/** \fn handle_diseq(const DVBTuning&, DVBDiSEqC*, bool)
 *  \brief Sends DisEq commands to external hardware.
 *
 */
static bool handle_diseq(const DVBTuning &ct, DVBDiSEqC *diseqc, bool reset)
{
    if (!diseqc)
        return false;

    bool tuned  = false;
    DVBTuning t = ct;
    t.params.frequency = tuned_frequency(ct, FE_QPSK, &t.tone);

    for (uint i = 0; i < 64 && !tuned; i++)
        if (!diseqc->Set(t, reset, tuned))
            return false;
        
    return true;
}
