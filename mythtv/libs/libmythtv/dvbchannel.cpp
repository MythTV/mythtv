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

// POSIX headers
#include <fcntl.h>
#include <unistd.h>
#include <sys/poll.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>

// MythTV headers
#include "mythcontext.h"
#include "mythdbcon.h"
#include "cardutil.h"
#include "channelutil.h"
#include "dvbtypes.h"
#include "dvbchannel.h"
#include "dvbcam.h"

static void drain_dvb_events(int fd);
static bool wait_for_backend(int fd, int timeout_ms);
static struct dvb_fe_params dtvmultiplex_to_dvbparams(
    DTVTunerType, const DTVMultiplex&);
static DTVMultiplex dvbparams_to_dtvmultiplex(
    DTVTunerType, const dvb_fe_params&);

#define LOC QString("DVBChan(%1): ").arg(cardnum)
#define LOC_WARN QString("DVBChan(%1) Warning: ").arg(cardnum)
#define LOC_ERR QString("DVBChan(%1) Error: ").arg(cardnum)

/** \class DVBChannel
 *  \brief Provides interface to the tuning hardware when using DVB drivers
 *
 *  \bug Only supports single input cards.
 */
DVBChannel::DVBChannel(int aCardNum, TVRec *parent)
    : DTVChannel(parent),
      // Helper classes
      diseqc_tree(NULL),            dvbcam(NULL),
      // Device info
      frontend_name(QString::null), card_type(DTVTunerType::kTunerTypeUnknown),
      // Tuning
      tuning_delay(0),              sigmon_delay(25),
      first_tune(true),
      // Misc
      fd_frontend(-1),              cardnum(aCardNum),
      has_crc_bug(false)
{
    dvbcam = new DVBCam(cardnum);
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
    // diseqc_tree is managed elsewhere
}

void DVBChannel::Close()
{
    VERBOSE(VB_CHANNEL, LOC + "Closing DVB channel");

    if (diseqc_tree)
        diseqc_tree->Close();

    if (fd_frontend >= 0)
    {
        close(fd_frontend);
        fd_frontend = -1;

        dvbcam->Stop();
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

    dvb_frontend_info info;
    bzero(&info, sizeof(info));
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
        bool ok = true;
        dvb_fe_caps_extended extinfo;
        bzero(&extinfo, sizeof(extinfo));
        if (ioctl(fd_frontend, FE_GET_EXTENDED_INFO, &extinfo) < 0)
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR +
                    "Failed to get frontend extended information." + ENO);

            ok = false;
        }

        if (ok && (extinfo.modulations & MOD_8PSK))
        {
            if (ioctl(fd_frontend, FE_SET_STANDARD, FE_DVB_S2) < 0)
            {
                VERBOSE(VB_IMPORTANT, LOC_ERR +
                        "Failed to set frontend standard to DVB-S2." + ENO);

                ok = false;
            }
            else if (ioctl(fd_frontend, FE_GET_INFO, &info) < 0)
            {
                VERBOSE(VB_IMPORTANT, LOC_ERR +
                        "Failed to get frontend information." + ENO);

                ok = false;
            }
        }

        if (!ok)
        {
            close(fd_frontend);
            fd_frontend = -1;
            return false;
        }

        ext_modulations   = extinfo.modulations;
    }
#endif

    frontend_name       = info.name;
    card_type           = info.type;
    capabilities        = info.caps;
    frequency_minimum   = info.frequency_min;
    frequency_maximum   = info.frequency_max;
    symbol_rate_minimum = info.symbol_rate_min;
    symbol_rate_maximum = info.symbol_rate_max;

    VERBOSE(VB_RECORD, LOC + QString("Using DVB card %1, with frontend '%2'.")
            .arg(cardnum).arg(frontend_name));

    // Turn on the power to the LNB
    if (card_type == DTVTunerType::kTunerTypeQPSK ||
        card_type == DTVTunerType::kTunerTypeDVB_S2)
    {
        diseqc_tree = diseqc_dev.FindTree(GetCardID());
        if (diseqc_tree)
            diseqc_tree->Open(fd_frontend);
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

// documented in dtvchannel.h
bool DVBChannel::TuneMultiplex(uint mplexid, QString inputname)
{
    DTVMultiplex tuning;
    if (!tuning.FillFromDB(card_type, mplexid))
        return false;

    CheckOptions(tuning);

    if (!Tune(tuning, inputname))
        return false;

    return true;
}

bool DVBChannel::SetChannelByString(const QString &channum)
{
    QString tmp     = QString("SetChannelByString(%1): ").arg(channum);
    QString loc     = LOC + tmp;
    QString loc_err = LOC_ERR + tmp;

    VERBOSE(VB_CHANNEL, loc);

    if (fd_frontend < 0)
    {
        VERBOSE(VB_IMPORTANT, loc_err + "Channel object "
                "will not open, can not change channels.");

        ClearDTVInfo();
        return false;
    }

    if (curchannelname == channum && !first_tune)
    {
        VERBOSE(VB_CHANNEL, loc + "Already on channel");
        return true;
    }

    ClearDTVInfo();

    QString inputName;
    if (!CheckChannel(channum, inputName))
    {
        VERBOSE(VB_IMPORTANT, loc_err +
                "CheckChannel failed.\n\t\t\tPlease verify the channel "
                "in the 'mythtv-setup' Channel Editor.");

        return false;
    }

    // If CheckChannel filled in the inputName we need to change inputs.
    if (!inputName.isEmpty() && (nextInputID == currentInputID))
        nextInputID = GetInputByName(inputName);

    // Get the input data for the channel
    int inputid = (nextInputID >= 0) ? nextInputID : currentInputID;
    InputMap::const_iterator it = inputs.find(inputid);
    if (it == inputs.end())
        return false;

    // Get the input data for the channel
    QString tvformat, modulation, freqtable, freqid, si_std;
    int finetune;
    uint64_t frequency;
    int mpeg_prog_num;
    uint atsc_major, atsc_minor, mplexid, tsid, netid;

    if (!ChannelUtil::GetChannelData(
        (*it)->sourceid, channum,
        tvformat, modulation, freqtable, freqid,
        finetune, frequency,
        si_std, mpeg_prog_num, atsc_major, atsc_minor, tsid, netid,
        mplexid, commfree))
    {
        VERBOSE(VB_IMPORTANT, loc_err +
                "Unable to find channel in database.");

        return false;
    }

    // Initialize basic the tuning parameters
    DTVMultiplex tuning;
    if (!mplexid || !tuning.FillFromDB(card_type, mplexid))
    {
        VERBOSE(VB_IMPORTANT, loc_err +
                "Failed to initialize multiplex options");

        return false;
    }

    SetDTVInfo(atsc_major, atsc_minor, netid, tsid, mpeg_prog_num);

    // Try to fix any problems with the multiplex
    CheckOptions(tuning);

    if (!Tune(tuning, ""))
    {
        VERBOSE(VB_IMPORTANT, loc_err + "Tuning to frequency.");

        ClearDTVInfo();
        return false;
    }

    curchannelname = QDeepCopy<QString>(channum);

    VERBOSE(VB_CHANNEL, loc + "Tuned to frequency.");

    currentInputID = nextInputID;
    inputs[currentInputID]->startChanNum = QDeepCopy<QString>(curchannelname);

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

/** \fn DVBChannel::CheckOptions(DTVMultiplex&) const
 *  \brief Checks tuning for problems, and tries to fix them.
 */
void DVBChannel::CheckOptions(DTVMultiplex &tuning) const
{
    if ((tuning.inversion == DTVInversion::kInversionAuto) &&
        !(capabilities & FE_CAN_INVERSION_AUTO))
    {
        VERBOSE(VB_GENERAL, LOC_WARN +
                "Unsupported inversion option 'auto', "
                "falling back to 'off'");

        tuning.inversion = DTVInversion::kInversionAuto;
    }

    uint64_t frequency = tuning.frequency;
    if (diseqc_tree)
    {
        DiSEqCDevLNB* lnb = diseqc_tree->FindLNB(diseqc_settings);
        if (lnb)
            frequency = lnb->GetIntermediateFrequency(diseqc_settings, tuning);
    }

    if (frequency_minimum && frequency_maximum &&
        (frequency_minimum <= frequency_maximum) &&
        (frequency < frequency_minimum || frequency > frequency_maximum))
    {
        VERBOSE(VB_GENERAL, LOC_WARN +
                QString("Your frequency setting (%1) is "
                        "out of range. (min/max:%2/%3)")
                .arg(frequency)
                .arg(frequency_minimum).arg(frequency_maximum));
    }

    if (card_type.IsFECVariable() &&
        symbol_rate_minimum && symbol_rate_maximum &&
        (symbol_rate_minimum <= symbol_rate_maximum) &&
        (tuning.symbolrate < symbol_rate_minimum ||
         tuning.symbolrate > symbol_rate_maximum))
    {
        VERBOSE(VB_GENERAL, LOC_WARN +
                QString("Symbol Rate setting (%1) is "
                        "out of range (min/max:%2/%3)")
                .arg(tuning.symbolrate)
                .arg(symbol_rate_minimum).arg(symbol_rate_maximum));
    }

    if (card_type.IsFECVariable() && !CheckCodeRate(tuning.fec))
    {
        VERBOSE(VB_GENERAL, LOC_WARN + "Unsupported fec_inner parameter.");
    }

    if (card_type.IsModulationVariable() && !CheckModulation(tuning.modulation))
    {
        VERBOSE(VB_GENERAL, LOC_WARN + "Unsupported modulation parameter.");
    }

    if (DTVTunerType::kTunerTypeOFDM != card_type)
    {
        VERBOSE(VB_CHANNEL, LOC + tuning.toString());
        return;
    }

    // Check OFDM Tuning params

    if (!CheckCodeRate(tuning.hp_code_rate))
    {
        VERBOSE(VB_GENERAL, LOC_WARN + "Unsupported code_rate_hp parameter.");
    }

    if (!CheckCodeRate(tuning.lp_code_rate))
    {
        VERBOSE(VB_GENERAL, LOC_WARN + "Unsupported code_rate_lp parameter.");
    }

    if ((tuning.bandwidth == DTVBandwidth::kBandwidthAuto) &&
        !(capabilities & FE_CAN_BANDWIDTH_AUTO))
    {
        VERBOSE(VB_GENERAL, LOC_WARN + "Unsupported bandwidth parameter.");
    }

    if ((tuning.trans_mode == DTVTransmitMode::kTransmissionModeAuto) &&
        !(capabilities & FE_CAN_TRANSMISSION_MODE_AUTO))
    {
        VERBOSE(VB_GENERAL, LOC_WARN +
                "Unsupported transmission_mode parameter.");
    }

    if ((tuning.guard_interval == DTVGuardInterval::kGuardIntervalAuto) &&
        !(capabilities & FE_CAN_GUARD_INTERVAL_AUTO))
    {
        VERBOSE(VB_GENERAL, LOC_WARN +
                "Unsupported guard_interval parameter.");
    }

    if ((tuning.hierarchy == DTVHierarchy::kHierarchyAuto) &&
        !(capabilities & FE_CAN_HIERARCHY_AUTO))
    {
        VERBOSE(VB_GENERAL, LOC_WARN + "Unsupported hierarchy parameter.");
    }

    if (!CheckModulation(tuning.modulation))
    {
        VERBOSE(VB_GENERAL, LOC_WARN + "Unsupported constellation parameter.");
    }

    VERBOSE(VB_CHANNEL, LOC + tuning.toString());
}

/** \fn DVBChannel::CheckCodeRate(fe_code_rate_t) const
 *  \brief Return true iff rate is supported rate on the frontend
 */
bool DVBChannel::CheckCodeRate(DTVCodeRate rate) const
{
    const uint64_t caps = capabilities;
    return
        ((DTVCodeRate::kFECNone == rate))                            ||
        ((DTVCodeRate::kFEC_1_2 == rate) && (caps & FE_CAN_FEC_1_2)) ||
        ((DTVCodeRate::kFEC_2_3 == rate) && (caps & FE_CAN_FEC_2_3)) ||
        ((DTVCodeRate::kFEC_3_4 == rate) && (caps & FE_CAN_FEC_3_4)) ||
        ((DTVCodeRate::kFEC_4_5 == rate) && (caps & FE_CAN_FEC_4_5)) ||
        ((DTVCodeRate::kFEC_5_6 == rate) && (caps & FE_CAN_FEC_5_6)) ||
        ((DTVCodeRate::kFEC_6_7 == rate) && (caps & FE_CAN_FEC_6_7)) ||
        ((DTVCodeRate::kFEC_7_8 == rate) && (caps & FE_CAN_FEC_7_8)) ||
        ((DTVCodeRate::kFEC_8_9 == rate) && (caps & FE_CAN_FEC_8_9)) ||
        ((DTVCodeRate::kFECAuto == rate) && (caps & FE_CAN_FEC_AUTO));
}

/** \fn DVBChannel::CheckModulation(fe_modulation_t) const
 *  \brief Return true iff modulation is supported modulation on the frontend
 */
bool DVBChannel::CheckModulation(DTVModulation modulation) const
{
    const DTVModulation m = modulation;
    const uint64_t      c = capabilities;

#ifdef FE_GET_EXTENDED_INFO
    if ((c & FE_HAS_EXTENDED_INFO)            &&
        (DTVModulation::kModulation8PSK == m) &&
        (ext_modulations & DTVModulation::kModulation8PSK))
    {
        return true;
    }
#endif // FE_GET_EXTENDED_INFO

    return
        ((DTVModulation::kModulationQPSK    == m) && (c & FE_CAN_QPSK))     ||
        ((DTVModulation::kModulationQAM16   == m) && (c & FE_CAN_QAM_16))   ||
        ((DTVModulation::kModulationQAM32   == m) && (c & FE_CAN_QAM_32))   ||
        ((DTVModulation::kModulationQAM64   == m) && (c & FE_CAN_QAM_64))   ||
        ((DTVModulation::kModulationQAM128  == m) && (c & FE_CAN_QAM_128))  ||
        ((DTVModulation::kModulationQAM256  == m) && (c & FE_CAN_QAM_256))  ||
        ((DTVModulation::kModulationQAMAuto == m) && (c & FE_CAN_QAM_AUTO)) ||
        ((DTVModulation::kModulation8VSB    == m) && (c & FE_CAN_8VSB))     ||
        ((DTVModulation::kModulation16VSB   == m) && (c & FE_CAN_16VSB));
}

/** \fn DVBChannel::SetPMT(const ProgramMapTable*)
 *  \brief Tells the Conditional Access Module which streams we wish to decode.
 */
void DVBChannel::SetPMT(const ProgramMapTable *pmt)
{
    if (pmt && dvbcam->IsRunning())
        dvbcam->SetPMT(pmt);
}

/** \fn DVBChannel::SetTimeOffset(double)
 *  \brief Tells the Conditional Access Module the offset
 *         from the computers utc time to dvb time.
 */
void DVBChannel::SetTimeOffset(double offset)
{
    if (dvbcam->IsRunning())
        dvbcam->SetTimeOffset(offset);
}

/*****************************************************************************
           Tuning functions for each of the four types of cards.
 *****************************************************************************/

/**
 *  \brief Tunes the card to a frequency but does not deal with PIDs.
 *
 *   This is used by DVB Channel Scanner, the EIT Parser, and by TVRec.
 *
 *  \param tuning      Info on transport to tune to
 *  \param inputname    Optional, forces specific input (for DiSEqC)
 *  \param force_reset If true, frequency tuning is done
 *                     even if it should not be needed.
 *  \param same_input  Optional, doesn't change input (for retuning).
 *  \return true on success, false on failure
 */
bool DVBChannel::Tune(const DTVMultiplex &tuning,
                      QString inputname,
                      bool force_reset,
                      bool same_input)
{
    QMutexLocker lock(&tune_lock);

    bool reset = (force_reset || first_tune);
    struct dvb_fe_params params = dtvmultiplex_to_dvbparams(card_type, tuning);

    bool is_dvbs = (DTVTunerType::kTunerTypeQPSK   == card_type ||
                    DTVTunerType::kTunerTypeDVB_S2 == card_type);

    bool has_diseqc = (diseqc_tree != NULL);
    if (is_dvbs && !has_diseqc)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR +
                "DVB-S needs device tree for LNB handling");
        return false;
    }

    desired_tuning = tuning;

    if (fd_frontend < 0)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Tune(): Card not open!");

        return false;
    }

    // Remove any events in queue before tuning.
    drain_dvb_events(fd_frontend);

    // send DVB-S setup
    if (is_dvbs)
    {
        // make sure we tune to frequency, no matter what happens..
        reset = first_tune = true;

        // configure for new input
        if (!same_input)
        {
            int inputid = (inputname.isEmpty()) ?
                nextInputID : GetInputByName(inputname);

            if (inputid < 0)
            {
                VERBOSE(VB_IMPORTANT, LOC_ERR + "Tune(): " +
                        QString("Invalid input '%1'").arg(inputname));

                return false;
            }

            diseqc_settings.Load(inputid);
        }
            
        // execute diseqc commands
        if (!diseqc_tree->Execute(diseqc_settings, tuning))
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR + "Tune(): "
                    "Failed to setup DiSEqC devices");
            return false;
        }

        // retrieve actual intermediate frequency
        DiSEqCDevLNB *lnb = diseqc_tree->FindLNB(diseqc_settings);
        if (!lnb)
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR + "Tune(): "
                    "No LNB for this configuration");
            return false;
        }
        params.frequency = lnb->GetIntermediateFrequency(
            diseqc_settings, tuning);

        // if card can auto-FEC, use it -- sometimes NITs are inaccurate
        if (capabilities & FE_CAN_FEC_AUTO)
            params.u.qpsk.fec_inner = FEC_AUTO;
    }

    VERBOSE(VB_CHANNEL, LOC + "Old Params: " +
            prev_tuning.toString() +
            "\n\t\t\t" + LOC + "New Params: " +
            tuning.toString());

    // DVB-S is in kHz, other DVB is in Hz
    int     freq_mult = (is_dvbs) ? 1 : 1000;
    QString suffix    = (is_dvbs) ? "kHz" : "Hz";

    if (reset || !prev_tuning.IsEqual(card_type, tuning, 500 * freq_mult))
    {
        VERBOSE(VB_CHANNEL, LOC + QString("Tune(): Tuning to %1%2")
                .arg(params.frequency).arg(suffix));

#ifdef FE_GET_EXTENDED_INFO
        if (card_type == DTVTunerType::kTunerTypeDVB_S2)
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

        prev_tuning = tuning;
        first_tune = false;
    }

    SetSIStandard(tuning.sistandard);

    VERBOSE(VB_CHANNEL, LOC + "Tune(): Frequency tuning successful.");

    return true;
}

bool DVBChannel::Retune(void)
{
    return Tune(desired_tuning, "", true, true);
}

QString DVBChannel::GetFrontendName(void) const
{
    return QDeepCopy<QString>(frontend_name);
}

/** \fn DVBChannel::IsTuningParamsProbeSupported(void) const
 *  \brief Returns true iff tuning info probing is working.
 */
bool DVBChannel::IsTuningParamsProbeSupported(void) const
{
    if (fd_frontend < 0)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Card not open!");

        return false;
    }

    dvb_fe_params params;
    return ioctl(fd_frontend, FE_GET_FRONTEND, &params) >= 0;
}

/** \fn DVBChannel::ProbeTuningParams(DTVMultiplex&) const
 *  \brief Fetches DTVMultiplex params from driver
 *
 *   Note: Only updates tuning on success.
 *
 *  \return true on success, false on failure
 */
bool DVBChannel::ProbeTuningParams(DTVMultiplex &tuning) const
{
    if (fd_frontend < 0)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Card not open!");

        return false;
    }

    dvb_fe_params params;
    if (ioctl(fd_frontend, FE_GET_FRONTEND, &params) < 0)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR +
                "Getting Frontend tuning parameters failed." + ENO);

        return false;
    }

    uint    mplex      = tuning.mplex;
    QString sistandard = QDeepCopy<QString>(tuning.sistandard);

    tuning = dvbparams_to_dtvmultiplex(card_type, params);

    tuning.mplex       = mplex;
    tuning.sistandard  = sistandard;

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
    if (chanid > 0)
        DTVChannel::SaveCachedPids(chanid, pid_cache);
}

void DVBChannel::GetCachedPids(pid_cache_t &pid_cache) const
{
    int chanid = GetChanID();
    if (chanid > 0)
        DTVChannel::GetCachedPids(chanid, pid_cache);
}

const DiSEqCDevRotor *DVBChannel::GetRotor(void) const
{
    if (diseqc_tree)
        return diseqc_tree->FindRotor(diseqc_settings);

    return NULL;
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

static struct dvb_fe_params dtvmultiplex_to_dvbparams(
    DTVTunerType tuner_type, const DTVMultiplex &tuning)
{
    dvb_fe_params params;
    bzero(&params, sizeof(params));

    params.frequency = tuning.frequency;
    params.inversion = (fe_spectral_inversion_t) (int) tuning.inversion;

    if (DTVTunerType::kTunerTypeQPSK == tuner_type)
    {
        params.u.qpsk.symbol_rate = tuning.symbolrate;
        params.u.qpsk.fec_inner   = (fe_code_rate_t) (int) tuning.fec;
    }

    if (DTVTunerType::kTunerTypeDVB_S2 == tuner_type)
    {
#ifdef FE_GET_EXTENDED_INFO
        params.u.qpsk2.symbol_rate = tuning.symbolrate;
        params.u.qpsk2.fec_inner   = (fe_code_rate_t) (int) tuning.fec;
        params.u.qpsk2.modulation  = (fe_modulation_t) (int) tuning.modulation;
#else // if !FE_GET_EXTENDED_INFO
        VERBOSE(VB_IMPORTANT, "DVBChan Error, MythTV was compiled without "
                "DVB-S2 headers being present so DVB-S2 tuning will fail.");
#endif // !FE_GET_EXTENDED_INFO
    }

    if (DTVTunerType::kTunerTypeQAM  == tuner_type)
    {
        params.u.qam.symbol_rate  = tuning.symbolrate;
        params.u.qam.fec_inner    = (fe_code_rate_t) (int) tuning.fec;
        params.u.qam.modulation   = (fe_modulation_t) (int) tuning.modulation;
    }

    if (DTVTunerType::kTunerTypeOFDM == tuner_type)
    {
        params.u.ofdm.bandwidth             =
            (fe_bandwidth_t) (int) tuning.bandwidth;
        params.u.ofdm.code_rate_HP          =
            (fe_code_rate_t) (int) tuning.hp_code_rate;
        params.u.ofdm.code_rate_LP          =
            (fe_code_rate_t) (int) tuning.lp_code_rate;
        params.u.ofdm.constellation         =
            (fe_modulation_t) (int) tuning.modulation;
        params.u.ofdm.transmission_mode     =
            (fe_transmit_mode_t) (int) tuning.trans_mode;
        params.u.ofdm.guard_interval        =
            (fe_guard_interval_t) (int) tuning.guard_interval;
        params.u.ofdm.hierarchy_information =
            (fe_hierarchy_t) (int) tuning.hierarchy;
    }

    if (DTVTunerType::kTunerTypeATSC == tuner_type)
    {
#ifdef USE_ATSC
        params.u.vsb.modulation   =
            (fe_modulation_t) (int) tuning.modulation;
#endif // USE_ATSC
    }

    return params;
}

static DTVMultiplex dvbparams_to_dtvmultiplex(
    DTVTunerType tuner_type, const dvb_fe_params &params)
{
    DTVMultiplex tuning;

    tuning.frequency = params.frequency;
    tuning.inversion = params.inversion;

    if ((DTVTunerType::kTunerTypeQPSK   == tuner_type) ||
        (DTVTunerType::kTunerTypeDVB_S2 == tuner_type))
    {
        tuning.symbolrate     = params.u.qpsk.symbol_rate;
        tuning.fec            = params.u.qpsk.fec_inner;
    }

    if (DTVTunerType::kTunerTypeQAM  == tuner_type)
    {
        tuning.symbolrate     = params.u.qam.symbol_rate;
        tuning.fec            = params.u.qam.fec_inner;
        tuning.modulation     = params.u.qam.modulation;
    }

    if (DTVTunerType::kTunerTypeOFDM == tuner_type)
    {
        tuning.bandwidth      = params.u.ofdm.bandwidth;
        tuning.hp_code_rate   = params.u.ofdm.code_rate_HP;
        tuning.lp_code_rate   = params.u.ofdm.code_rate_LP;
        tuning.modulation     = params.u.ofdm.constellation;
        tuning.trans_mode     = params.u.ofdm.transmission_mode;
        tuning.guard_interval = params.u.ofdm.guard_interval;
        tuning.hierarchy      = params.u.ofdm.hierarchy_information;
    }

    if (DTVTunerType::kTunerTypeATSC == tuner_type)
    {
#ifdef USE_ATSC
        tuning.modulation     = params.u.vsb.modulation;
#endif // USE_ATSC
    }

    return tuning;
}
