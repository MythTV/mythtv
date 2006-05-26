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
#include "cardutil.h"

#include "dvbtypes.h"
#include "dvbchannel.h"
#include "dvbrecorder.h"
#include "dvbdiseqc.h"
#include "dvbcam.h"

static uint tuned_frequency(const DVBTuning&, fe_type_t, fe_sec_tone_mode_t *);
static void drain_dvb_events(int fd);
static bool wait_for_backend(int fd, int timeout_ms);
static bool handle_diseq(const DVBTuning&, DVBDiSEqC*, bool reset);

#define LOC QString("DVBChan(%1): ").arg(cardnum)
#define LOC_WARN QString("DVBChan(%1) Warning: ").arg(cardnum)
#define LOC_ERR QString("DVBChan(%1) Error: ").arg(cardnum)

/** \class DVBChannel
 *  \brief Provides interface to the tuning hardware when using DVB drivers
 *
 *  \bug Only supports single input cards.
 */
DVBChannel::DVBChannel(int aCardNum, TVRec *parent)
    : ChannelBase(parent),
      // Helper classes
      diseqc(NULL),                 dvbcam(NULL),
      // Tuning
      retune_adj(-10),
      tuning_delay(0),              sigmon_delay(25),
      first_tune(true),
      // Misc
      fd_frontend(-1),              cardnum(aCardNum),
      has_crc_bug(false)
{
    dvbcam = new DVBCam(cardnum);
    bzero(&info, sizeof(info));
    has_crc_bug = CardUtil::HasDVBCRCBug(aCardNum);
    sigmon_delay = CardUtil::GetMinSignalMonitoringDelay(aCardNum);
}

DVBChannel::~DVBChannel()
{
    Close();
    if (dvbcam)
    {
        delete dvbcam;
        dvbcam = NULL;
    }
}

void DVBChannel::Close()
{
    VERBOSE(VB_CHANNEL, LOC + "Closing DVB channel");

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
    VERBOSE(VB_CHANNEL, LOC + "Opening DVB channel");
    if (fd_frontend >= 0)
        return true;

    QString devname = CardUtil::GetDeviceName(DVB_DEV_FRONTEND, cardnum);
    fd_frontend = open(devname.ascii(), O_RDWR | O_NONBLOCK);

    if (fd_frontend < 0)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR +
                "Opening DVB frontend device failed." + ENO);

        return false;
    }

    if (ioctl(fd_frontend, FE_GET_INFO, &info) < 0)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR +
                "Failed to get frontend information." + ENO);

        close(fd_frontend);
        fd_frontend = -1;
        return false;
    }

#ifdef FE_GET_EXTENDED_INFO
    if (info.caps & FE_HAS_EXTENDED_INFO)
    {
        if (ioctl(fd_frontend, FE_GET_EXTENDED_INFO, &extinfo) < 0)
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR +
                    "Failed to get frontend extended information." + ENO);
            close(fd_frontend);
            fd_frontend = -1;
            return false;
        }
        if (extinfo.modulations & MOD_8PSK)
        {
            if (ioctl(fd_frontend, FE_SET_STANDARD, FE_DVB_S2) < 0)
            {
                VERBOSE(VB_IMPORTANT, LOC_ERR +
                        "Failed to get extended frontend information." + ENO);
                close(fd_frontend);
                fd_frontend = -1;
                return false;
            }
            if (ioctl(fd_frontend, FE_GET_INFO, &info) < 0)
            {
                VERBOSE(VB_IMPORTANT, LOC_ERR +
                        "Failed to get frontend information." + ENO);
                close(fd_frontend);
                fd_frontend = -1;
                return false;
            }
        }
    }
#endif

    VERBOSE(VB_RECORD, LOC + QString("Using DVB card %1, with frontend '%2'.")
            .arg(cardnum).arg(info.name));

    // Turn on the power to the LNB
#ifdef FE_GET_EXTENDED_INFO
    if (info.type == FE_QPSK || info.type == FE_DVB_S2)
#else
    if (info.type == FE_QPSK)
#endif
    {
        if (ioctl(fd_frontend, FE_SET_TONE, SEC_TONE_OFF) < 0)
        {
            VERBOSE(VB_GENERAL, LOC_WARN +
                    "Initial Tone setting failed." + ENO);
        }

        if (ioctl(fd_frontend, FE_SET_VOLTAGE, SEC_VOLTAGE_13) < 0)
        {
            VERBOSE(VB_GENERAL, LOC_WARN +
                    "Initial Voltage setting failed." + ENO);
        }

        if (diseqc == NULL)
        {
            diseqc = new DVBDiSEqC(cardnum, fd_frontend);
            diseqc->DiseqcReset();
        }
    }
    
    dvbcam->Start();

    first_tune = true;

    if (!InitializeInputs())
    {
        Close();
        return false;
    }

    nextInputID = currentInputID;

    return (fd_frontend >= 0);
}

/** \fn DVBChannel::TuneMultiplex(uint mplexid)
 *  \brief To be used by setup sdt/nit scanner and eit parser.
 *
 *   mplexid is how the db indexes each transport
 */
bool DVBChannel::TuneMultiplex(uint mplexid)
{
    if (!GetTransportOptions(mplexid))
        return false;

    CheckOptions();
    VERBOSE(VB_CHANNEL, LOC + cur_tuning.toString(info.type));

    if (!Tune(cur_tuning))
        return false;

    return true;
}

bool DVBChannel::SetChannelByString(const QString &chan)
{
    QString func = QString("SetChannelByString(%1)").arg(chan);
    VERBOSE(VB_CHANNEL, LOC + func);
    if (fd_frontend < 0)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + func + QString(": not open"));

        return false;
    }

    if (curchannelname == chan && !first_tune)
    {
        VERBOSE(VB_CHANNEL, LOC + func + ": already on channel");
        return true;
    }

    QString inputName;
    if (!CheckChannel(chan, inputName))
    {
        VERBOSE(VB_IMPORTANT, LOC + "CheckChannel failed. " +
                QString("Please verify channel '%1'").arg(chan) +
                " in the \"mythtv-setup\" Channel Editor.");
        return false;
    }
    // If CheckChannel filled in the inputName we need to
    // change inputs.
    if (!inputName.isEmpty())
        nextInputID = GetInputByName(inputName);
    
    if (GetChannelOptions(chan) == false)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Failed to get channel options for " +
                QString("channel '%1'.").arg(chan));
        
        return false;
    }

    CheckOptions();
    VERBOSE(VB_CHANNEL, LOC + cur_tuning.toString(info.type));

    if (!Tune(cur_tuning))
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Tuning to frequency for " +
                QString("channel '%1'.").arg(chan));

        return false;
    }

    curchannelname = chan;

    VERBOSE(VB_CHANNEL, LOC + "Tuned to frequency for " +
            QString("channel '%1'.").arg(chan));

    currentInputID = nextInputID;
    inputs[currentInputID]->startChanNum = curchannelname;

    return true;
}

bool DVBChannel::SwitchToInput(const QString &input, const QString &chan)
{
    int inputNum = GetInputByName(input);
    if (inputNum < 0)
        return false;

    if (input == inputs[inputNum]->name)
    {
        nextInputID = GetInputByName(input);
        if (nextInputID == -1)
        {
            VERBOSE(VB_IMPORTANT, QString("Failed to locate input %1").
                    arg(input));
            nextInputID = currentInputID;
        }
    }
    return SetChannelByString(chan);
}

bool DVBChannel::SwitchToInput(int newInputNum, bool setstarting)
{
    (void)setstarting;

    InputMap::const_iterator it = inputs.find(newInputNum);
    if (it == inputs.end() || (*it)->startChanNum.isEmpty())
        return false;

    nextInputID = newInputNum;
    return SetChannelByString((*it)->startChanNum);
}

/** \fn DVBChannel::GetChannelOptions(const QString &)
 *  \brief This function called when tuning to a specific stream.
 */
bool DVBChannel::GetChannelOptions(const QString &channum)
{
    // Reset Channel data
    currentATSCMajorChannel = currentATSCMinorChannel = -1;
    currentProgramNum = -1;
    currentOriginalNetworkID = currentTransportID = -1;

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "SELECT chanid, serviceid, channel.mplexid, atscsrcid, "
        "       cardinputid, networkid, transportid "
        "FROM channel, cardinput, capturecard, dtv_multiplex "
        "WHERE channel.mplexid    = dtv_multiplex.mplexid AND "
        "      cardinput.sourceid = channel.sourceid      AND "
        "      capturecard.cardid = cardinput.cardid      AND "
        "      cardinput.cardid   = :CARDID               AND "
        "      channel.channum    = :CHANNUM");
    query.bindValue(":CARDID",  GetCardID());
    query.bindValue(":CHANNUM", channum);

    if (!query.exec() || !query.isActive())
    {
        MythContext::DBError("GetChannelOptions - ChanID", query);
        return false;
    }

    uint mplexid = 0;
    while (query.next() && !mplexid)
    {
        if (query.value(4).toInt() != nextInputID)
            continue;

        currentProgramNum = query.value(1).toUInt();
        mplexid           = query.value(2).toUInt();

        if (query.value(3).toUInt() & 0xff)
        {
            currentATSCMajorChannel  = query.value(3).toUInt() >> 8;
            currentATSCMinorChannel  = query.value(3).toUInt() & 0xff;
            currentTransportID       = query.value(6).toUInt();
            currentProgramNum        = -1;
        }
        else if (query.value(5).toUInt() > 0)
        {
            currentOriginalNetworkID = query.value(5).toUInt();
            currentTransportID       = query.value(6).toUInt();
        }
    }

    if (mplexid)
        return GetTransportOptions(mplexid);

    VERBOSE(VB_IMPORTANT, LOC_ERR + "Unable to find channel in database.");
    return false;
}

/** \fn DVBChannel::GetTransportOptions(int mplexid)
 *  \brief Initializes tuning from database using mplexid.
 *
 *  \todo pParent doesn't exist when you create a DVBChannel from videosource
 *        but this is important (i think) for remote backends
 *  \return true on success and false on failure
 */
bool DVBChannel::GetTransportOptions(int mplexid)
{
    MSqlQuery query(MSqlQuery::InitCon());

    // Query for our DVBTuning params
    query.prepare(
        "SELECT frequency,         inversion,      symbolrate, "
        "       fec,               polarity,       dvb_diseqc_type, "
        "       diseqc_port,       diseqc_pos,     lnb_lof_switch, "
        "       lnb_lof_hi,        lnb_lof_lo,     sistandard, "
        "       hp_code_rate,      lp_code_rate,   constellation, "
        "       transmission_mode, guard_interval, hierarchy, "
        "       modulation,        bandwidth,      cardinputid "
        "FROM dtv_multiplex, cardinput, capturecard "
        "WHERE dtv_multiplex.sourceid = cardinput.sourceid AND "
        "      dtv_multiplex.mplexid  = :MPLEXID           AND "
        "      cardinput.cardid       = capturecard.cardid AND "
        "      cardinput.cardid       = :CARDID");
    query.bindValue(":MPLEXID", mplexid);
    query.bindValue(":CARDID",  GetCardID());

    if (!query.exec() || !query.isActive())
    {
        MythContext::DBError("GetChannelOptions - Options", query);
        return false;
    }

    if (!query.next())
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR +
                "Could not find dvb tuning parameters for " +
                QString("mplex %1").arg(mplexid));

        return false;
    }

    // Parse the query into our DVBTuning class
    return ParseTuningParams(info.type,
        query.value(0).toString(),  query.value(1).toString(),
        query.value(2).toString(),  query.value(3).toString(),
        query.value(4).toString(),  query.value(5).toString(),
        query.value(6).toString(),  query.value(7).toString(),
        query.value(8).toString(),  query.value(9).toString(),
        query.value(10).toString(), query.value(11).toString(),
        query.value(12).toString(), query.value(13).toString(),
        query.value(14).toString(), query.value(15).toString(),
        query.value(16).toString(), query.value(17).toString(),
        query.value(18).toString(), query.value(19).toString(),
        query.value(20).toString());
}

bool DVBChannel::ParseTuningParams(
    fe_type_t type,
    QString frequency,    QString inversion,      QString symbolrate,
    QString fec,          QString polarity,       QString dvb_diseqc_type,
    QString diseqc_port,  QString diseqc_pos,     QString lnb_lof_switch,
    QString lnb_lof_hi,   QString lnb_lof_lo,     QString _sistandard,
    QString hp_code_rate, QString lp_code_rate,   QString constellation,
    QString trans_mode,   QString guard_interval, QString hierarchy,
    QString modulation,   QString bandwidth,      QString _input_id)
{
    (void) _sistandard;
    (void) _input_id;

    if (FE_QPSK == type)
        return cur_tuning.parseQPSK(
            frequency,       inversion,     symbolrate,   fec,   polarity,
            dvb_diseqc_type, diseqc_port,   diseqc_pos,
            lnb_lof_switch,  lnb_lof_hi,    lnb_lof_lo);
    else if (FE_QAM == type)
        return cur_tuning.parseQAM(
            frequency,       inversion,     symbolrate,   fec,   modulation);
    else if (FE_OFDM == type)
        return cur_tuning.parseOFDM(
            frequency,       inversion,     bandwidth,    hp_code_rate,
            lp_code_rate,    constellation, trans_mode,   guard_interval,
            hierarchy);
    else if (FE_ATSC == type)
        return cur_tuning.parseATSC(frequency, modulation);

    return false;
}

/** \fn DVBChannel::CheckOptions()
 *  \brief Checks tuning for problems, and tries to fix them.
 */
void DVBChannel::CheckOptions()
{
    DVBTuning& t = cur_tuning;

    if ((t.params.inversion == INVERSION_AUTO) &&
        !(info.caps & FE_CAN_INVERSION_AUTO))
    {
        VERBOSE(VB_GENERAL, LOC_WARN +
                "Unsupported inversion option 'auto', "
                "falling back to 'off'");

        t.params.inversion = INVERSION_OFF;
    }

    uint frequency = tuned_frequency(t, info.type, NULL);

    if ((info.frequency_min > 0 && info.frequency_max > 0) &&
       (frequency < info.frequency_min || frequency > info.frequency_max))
    {
        VERBOSE(VB_GENERAL, LOC_WARN +
                QString("Your frequency setting (%1) is"
                        " out of range. (min/max:%2/%3)")
                .arg(frequency)
                .arg(info.frequency_min).arg(info.frequency_max));
    }

    unsigned int symbol_rate = 0;
    switch(info.type)
    {
        case FE_QPSK:
            symbol_rate = t.QPSKSymbolRate();

            if (!CheckCodeRate(t.params.u.qpsk.fec_inner))
            {
                VERBOSE(VB_GENERAL, LOC_WARN +
                        "Unsupported fec_inner parameter.");
            }
            break;

#ifdef FE_GET_EXTENDED_INFO
        case FE_DVB_S2:
            symbol_rate = t.DVBS2SymbolRate();

            if (!CheckModulation(t.params.u.qpsk2.modulation))
                VERBOSE(VB_GENERAL, LOC_WARN +
                        "Unsupported modulation parameter.");

            if (!CheckCodeRate(t.params.u.qpsk2.fec_inner))
                VERBOSE(VB_GENERAL, LOC_WARN +
                        "Unsupported fec_inner parameter.");
            break;
#endif

        case FE_QAM:
            symbol_rate = t.QAMSymbolRate();

            if (!CheckCodeRate(t.params.u.qam.fec_inner))
            {
                VERBOSE(VB_GENERAL, LOC_WARN +
                        "Unsupported fec_inner parameter.");
            }

            if (!CheckModulation(t.params.u.qam.modulation))
            {
                VERBOSE(VB_GENERAL, LOC_WARN +
                        "Unsupported modulation parameter.");
            }
            break;

        case FE_OFDM:
            if (!CheckCodeRate(t.params.u.ofdm.code_rate_HP))
            {
                VERBOSE(VB_GENERAL, LOC_WARN +
                        "Unsupported code_rate_hp option.");
            }

            if (!CheckCodeRate(t.params.u.ofdm.code_rate_LP))
            {
                VERBOSE(VB_GENERAL, LOC_WARN +
                        "Unsupported code_rate_lp parameter.");
            }

            if ((t.params.u.ofdm.bandwidth == BANDWIDTH_AUTO) &&
                !(info.caps & FE_CAN_BANDWIDTH_AUTO))
            {
                VERBOSE(VB_GENERAL, LOC_WARN +
                        "Unsupported bandwidth parameter.");
            }

            if ((t.params.u.ofdm.transmission_mode ==
                 TRANSMISSION_MODE_AUTO) &&
                !(info.caps & FE_CAN_TRANSMISSION_MODE_AUTO))
            {
                VERBOSE(VB_GENERAL, LOC_WARN +
                        "Unsupported transmission_mode parameter.");
            }

            if ((t.params.u.ofdm.guard_interval == GUARD_INTERVAL_AUTO) &&
                !(info.caps & FE_CAN_GUARD_INTERVAL_AUTO))
            {
                VERBOSE(VB_GENERAL, LOC_WARN +
                        "Unsupported guard_interval parameter.");
            }

            if ((t.params.u.ofdm.hierarchy_information == HIERARCHY_AUTO) &&
                !(info.caps & FE_CAN_HIERARCHY_AUTO))
            {
                VERBOSE(VB_GENERAL, LOC_WARN +
                        "Unsupported hierarchy parameter.");
            }

            if (!CheckModulation(t.params.u.ofdm.constellation))
            {
                VERBOSE(VB_GENERAL, LOC_WARN +
                        "Unsupported constellation parameter.");
            }
            break;

       case FE_ATSC:
            // ATSC doesn't need any validation
            break;
       default:
            break;
    }

    if (info.type != FE_OFDM &&
       (symbol_rate < info.symbol_rate_min ||
        symbol_rate > info.symbol_rate_max))
    {
        VERBOSE(VB_GENERAL, LOC_WARN +
                QString("Symbol Rate setting (%1) is "
                        "out of range (min/max:%2/%3)")
                .arg(symbol_rate)
                .arg(info.symbol_rate_min).arg(info.symbol_rate_max));
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
        default: return false;
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
#ifdef FE_GET_EXTENDED_INFO
        case MOD_8PSK: if (info.caps & FE_HAS_EXTENDED_INFO &&
                           extinfo.modulations & MOD_8PSK) return true;
#endif
        default: return false;
    }
    return false;
}

/** \fn DVBChannel::SetPMT(const ProgramMapTable*)
 *  \brief Tells the Conditional Access Module which streams we wish to decode.
 */
void DVBChannel::SetPMT(const ProgramMapTable *pmt)
{
    if (pmt && dvbcam->IsRunning())
        dvbcam->SetPMT(pmt);
}

/*****************************************************************************
           Tuning functions for each of the four types of cards.
 *****************************************************************************/

/** \fn DVBChannel::Tune(const DVBTuning&, bool)
 *  \brief Tunes the card to a frequency but does not deal with PIDs.
 *
 *   This is used by DVB Channel Scanner, the EIT Parser, and by TVRec.
 *
 *  \param channel     Info on transport to tune to
 *  \param force_reset If true, frequency tuning is done
 *                     even if it should not be needed.
 *  \return true on success, false on failure
 */
bool DVBChannel::Tune(const DVBTuning &tuning, bool force_reset)
{
    bool reset = (force_reset || first_tune);
    bool has_diseq = (FE_QPSK == info.type) && diseqc;
    struct dvb_fe_params params = tuning.params;

#ifdef FE_GET_EXTENDED_INFO
    has_diseq |= (FE_DVB_S2 == info.type) && diseqc;
#endif // FE_GET_EXTENDED_INFO

    retune_tuning = tuning;

    if (fd_frontend < 0)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Tune(): Card not open!");

        return false;
    }

    // Remove any events in queue before tuning.
    drain_dvb_events(fd_frontend);

    // Send DisEq commands to external hardware if we need to.
    if (has_diseq && !handle_diseq(tuning, diseqc, reset))
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Tune(): "
                "Failed to transmit DisEq commands");

        return false;
    }

    VERBOSE(VB_CHANNEL, LOC + "Old Params: " +
            prev_tuning.toString(info.type) +
            "\n\t\t\t" + LOC + "New Params: " +
            tuning.toString(info.type));

    if (reset || !prev_tuning.equal(info.type, tuning, 500000))
    {
        // Adjust for Satelite recievers which offset the frequency.
        params.frequency = tuned_frequency(tuning, info.type, NULL);

        params.frequency = params.frequency + (retune_adj = -retune_adj);

#ifdef FE_GET_EXTENDED_INFO
        if (info.type == FE_DVB_S2)
        {
            if (ioctl(fd_frontend, FE_SET_FRONTEND2, &params) < 0)
            {
                VERBOSE(VB_IMPORTANT, LOC_ERR + "Tune(): " +
                        "Setting Frontend(2) tuning parameters failed." + ENO);
                return false;
            }
        }
        else
#endif // FE_GET_EXTENDED_INFO
        {
            if (ioctl(fd_frontend, FE_SET_FRONTEND, &params) < 0)
            {
                VERBOSE(VB_IMPORTANT, LOC_ERR + "Tune(): " +
                        "Setting Frontend tuning parameters failed." + ENO);
                return false;
            }
        }

        // Extra delay to add for broken DVB drivers
        if (tuning_delay)
            usleep(tuning_delay * 1000);

        wait_for_backend(fd_frontend, 5 /* msec */);

        prev_tuning.params = tuning.params;
        first_tune = false;
    }

    VERBOSE(VB_CHANNEL, LOC + "Tune(): Frequency tuning successful.");

    return true;
}

/** \fn DVBChannel::Retune(void)
 *  \brief Calls DVBChannel::Tune() with the last known parameters
 *
 *   This is used to retune DVB-C hardware. DVB-C hardware
 *   sometimes does not successfully tune the first time
 *   despite reports of success from the drivers. This is
 *   probably a hardware problem and not a driver problem
 *   per say, so it is unlikely to be fixed at a lower level.
 *
 *  \return true iff drivers report that tuning was successful
 */
bool DVBChannel::Retune(void)
{
    return Tune(retune_tuning, true);
}

/** \fn DVBChannel::GetTuningParams(DVBTuning& tuning) const
 *  \brief Fetches DVBTuning params from driver
 *  \return true on success, false on failure
 */
bool DVBChannel::GetTuningParams(DVBTuning& tuning) const
{
    if (fd_frontend < 0)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Card not open!");

        return false;
    }

    if (ioctl(fd_frontend, FE_GET_FRONTEND, &tuning.params) < 0)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR +
                "Getting Frontend tuning parameters failed." + ENO);

        return false;
    }
    return true;
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
#ifdef FE_GET_EXTENDED_INFO
    if (FE_QPSK != type && FE_DVB_S2 != type)
        return tuning.Frequency();
#else
    if (FE_QPSK != type)
        return tuning.Frequency();
#endif

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
    struct timeval select_timeout = { 0, (timeout_ms % 1000) * 1000 /*usec*/};
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

    // Wait 10 ms, recommended by Marcus Metzler, see #1552
    usleep(10 * 1000);

    return true;
}
