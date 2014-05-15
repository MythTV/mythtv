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
 *   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
 */

// POSIX headers
#include <fcntl.h>
#include <unistd.h>
#include <sys/poll.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>

// MythTV headers
#include "mythconfig.h"
#include "mythdb.h"
#include "cardutil.h"
#include "channelutil.h"
#include "dvbtypes.h"
#include "dvbchannel.h"
#include "dvbcam.h"

static void drain_dvb_events(int fd);
static bool wait_for_backend(int fd, int timeout_ms);
static struct dvb_frontend_parameters dtvmultiplex_to_dvbparams(
    DTVTunerType, const DTVMultiplex&, int intermediate_freq, bool can_fec_auto);
static DTVMultiplex dvbparams_to_dtvmultiplex(
    DTVTunerType, const dvb_frontend_parameters&);

int64_t concurrent_tunings_delay = 1000;
QDateTime DVBChannel::last_tuning = QDateTime::currentDateTime();

#define LOC QString("DVBChan[%1](%2): ").arg(GetCardID()).arg(GetDevice())

/** \class DVBChannel
 *  \brief Provides interface to the tuning hardware when using DVB drivers
 *
 *  \bug Only supports single input cards.
 */
DVBChannel::DVBChannel(const QString &aDevice, TVRec *parent)
    : DTVChannel(parent),
    // Helper classes
    diseqc_tree(NULL),              dvbcam(NULL),
    // Device info
    capabilities(0),                ext_modulations(0),
    frequency_minimum(0),           frequency_maximum(0),
    symbol_rate_minimum(0),         symbol_rate_maximum(0),
    // Tuning
    tune_lock(),                    hw_lock(QMutex::Recursive),
    last_lnb_dev_id(~0x0),
    tuning_delay(0),                sigmon_delay(25),
    first_tune(true),
    // Misc
    fd_frontend(-1),                device(aDevice),
    has_crc_bug(false)
{
    master_map_lock.lockForWrite();
    QString devname = CardUtil::GetDeviceName(DVB_DEV_FRONTEND, device);
    master_map[devname].push_back(this); // == RegisterForMaster
    DVBChannel *master = static_cast<DVBChannel*>(master_map[devname].front());
    if (master == this)
    {
        dvbcam = new DVBCam(device);
        has_crc_bug = CardUtil::HasDVBCRCBug(device);
    }
    else
    {
        dvbcam       = master->dvbcam;
        has_crc_bug  = master->has_crc_bug;
    }
    master_map_lock.unlock();

    sigmon_delay = CardUtil::GetMinSignalMonitoringDelay(device);
}

DVBChannel::~DVBChannel()
{
    // set a new master if there are other instances and we're the master
    // whether we are the master or not remove us from the map..
    master_map_lock.lockForWrite();
    QString devname = CardUtil::GetDeviceName(DVB_DEV_FRONTEND, device);
    DVBChannel *master = static_cast<DVBChannel*>(master_map[devname].front());
    if (master == this)
    {
        master_map[devname].pop_front();
        DVBChannel *new_master = NULL;
        if (!master_map[devname].empty())
            new_master = dynamic_cast<DVBChannel*>(master_map[devname].front());
        if (new_master)
            new_master->is_open = master->is_open;
    }
    else
    {
        master_map[devname].removeAll(this);
    }
    master_map_lock.unlock();

    Close();

    // if we're the last one out delete dvbcam
    master_map_lock.lockForRead();
    MasterMap::iterator mit = master_map.find(devname);
    if ((*mit).empty())
        delete dvbcam;
    dvbcam = NULL;
    master_map_lock.unlock();

    // diseqc_tree is managed elsewhere
}

void DVBChannel::Close(DVBChannel *who)
{
    LOG(VB_CHANNEL, LOG_INFO, LOC + "Closing DVB channel");

    IsOpenMap::iterator it = is_open.find(who);
    if (it == is_open.end())
        return; // this caller didn't have it open in the first place..

    is_open.erase(it);

    QMutexLocker locker(&hw_lock);

    DVBChannel *master = GetMasterLock();
    if (master != NULL && master != this)
    {
        if (dvbcam->IsRunning())
            dvbcam->SetPMT(this, NULL);
        master->Close(this);
        fd_frontend = -1;
        ReturnMasterLock(master);
        return;
    }
    ReturnMasterLock(master); // if we're the master we don't need this lock..

    if (!is_open.empty())
        return; // not all callers have closed the DVB channel yet..

    if (diseqc_tree)
        diseqc_tree->Close();

    if (fd_frontend >= 0)
    {
        close(fd_frontend);
        fd_frontend = -1;

        dvbcam->Stop();
    }
}

bool DVBChannel::Open(DVBChannel *who)
{
    LOG(VB_CHANNEL, LOG_INFO, LOC + "Opening DVB channel");

    QMutexLocker locker(&hw_lock);

    if (fd_frontend >= 0)
    {
        is_open[who] = true;
        return true;
    }

    DVBChannel *master = GetMasterLock();
    if (master != this)
    {
        if (!master->Open(who))
        {
            ReturnMasterLock(master);
            return false;
        }

        fd_frontend         = master->fd_frontend;
        frontend_name       = master->frontend_name;
        tunerType           = master->tunerType;
        capabilities        = master->capabilities;
        ext_modulations     = master->ext_modulations;
        frequency_minimum   = master->frequency_minimum;
        frequency_maximum   = master->frequency_maximum;
        symbol_rate_minimum = master->symbol_rate_minimum;
        symbol_rate_maximum = master->symbol_rate_maximum;

        is_open[who] = true;

        if (!InitializeInputs())
        {
            Close();
            ReturnMasterLock(master);
            return false;
        }

        ReturnMasterLock(master);
        return true;
    }
    ReturnMasterLock(master); // if we're the master we don't need this lock..

    QString devname = CardUtil::GetDeviceName(DVB_DEV_FRONTEND, device);
    QByteArray devn = devname.toLatin1();

    for (int tries = 1; ; ++tries)
    {
        fd_frontend = open(devn.constData(), O_RDWR | O_NONBLOCK);
        if (fd_frontend >= 0)
            break;
        LOG(VB_GENERAL, LOG_WARNING, LOC +
            "Opening DVB frontend device failed." + ENO);
        if (tries >= 20 || (errno != EBUSY && errno != EAGAIN))
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                QString("Failed to open DVB frontend device due to "
                        "fatal error or too many attempts."));
            return false;
        }
        usleep(50000);
    }

    dvb_frontend_info info;
    memset(&info, 0, sizeof(info));
    if (ioctl(fd_frontend, FE_GET_INFO, &info) < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            "Failed to get frontend information." + ENO);

        close(fd_frontend);
        fd_frontend = -1;
        return false;
    }

    frontend_name       = info.name;
    tunerType           = info.type;
#if HAVE_FE_CAN_2G_MODULATION
    if (tunerType == DTVTunerType::kTunerTypeDVBS1 &&
        (info.caps & FE_CAN_2G_MODULATION))
        tunerType = DTVTunerType::kTunerTypeDVBS2;
#endif // HAVE_FE_CAN_2G_MODULATION
    capabilities        = info.caps;
    frequency_minimum   = info.frequency_min;
    frequency_maximum   = info.frequency_max;
    symbol_rate_minimum = info.symbol_rate_min;
    symbol_rate_maximum = info.symbol_rate_max;

    LOG(VB_RECORD, LOG_INFO, LOC +
        QString("Using DVB card %1, with frontend '%2'.")
            .arg(device).arg(frontend_name));

    // Turn on the power to the LNB
    if (tunerType.IsDiSEqCSupported())
    {

        diseqc_tree = diseqc_dev.FindTree(GetCardID());
        if (diseqc_tree)
        {
            bool is_SCR = false;

            DiSEqCDevSCR *scr = diseqc_tree->FindSCR(diseqc_settings);
            if (scr)
            {
                is_SCR = true;
                LOG(VB_CHANNEL, LOG_INFO, LOC + "Requested DVB channel is on SCR system");
            }
            else
                LOG(VB_CHANNEL, LOG_INFO, LOC + "Requested DVB channel is on non-SCR system");

            diseqc_tree->Open(fd_frontend, is_SCR);
        }
    }

    first_tune = true;

    if (!InitializeInputs())
    {
        Close();
        return false;
    }

    if (fd_frontend >= 0)
        is_open[who] = true;

    return (fd_frontend >= 0);
}

bool DVBChannel::IsOpen(void) const
{
    IsOpenMap::const_iterator it = is_open.find(this);
    return it != is_open.end();
}

bool DVBChannel::Init(QString &inputname, QString &startchannel, bool setchan)
{
    if (setchan && !IsOpen())
        Open(this);

    return ChannelBase::Init(inputname, startchannel, setchan);
}

bool DVBChannel::SwitchToInput(const QString &inputname, const QString &chan)
{
    int input = GetInputByName(inputname);

    bool ok = false;
    if (input >= 0)
    {
        m_currentInputID = input;
        ok = SetChannelByString(chan);
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("DVBChannel: Could not find input: %1 on card when "
                    "setting channel %2\n").arg(inputname).arg(chan));
    }
    return ok;
}

bool DVBChannel::SwitchToInput(int newInputNum, bool setstarting)
{
    if (!ChannelBase::SwitchToInput(newInputNum, false))
        return false;

    m_currentInputID = newInputNum;
    InputMap::const_iterator it = m_inputs.find(m_currentInputID);
    return SetChannelByString((*it)->startChanNum);
}


/** \fn DVBChannel::CheckFrequency(uint64_t) const
 *  \brief Checks tuning frequency
 */
void DVBChannel::CheckFrequency(uint64_t frequency) const
{
    if (frequency_minimum && frequency_maximum &&
        (frequency_minimum <= frequency_maximum) &&
        (frequency < frequency_minimum || frequency > frequency_maximum))
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC +
            QString("Your frequency setting (%1) is out of range. "
                    "(min/max:%2/%3)")
                .arg(frequency).arg(frequency_minimum).arg(frequency_maximum));
    }
}

void DVBChannel::CheckOptions(DTVMultiplex &tuning) const
{
    if ((tuning.inversion == DTVInversion::kInversionAuto) &&
        !(capabilities & FE_CAN_INVERSION_AUTO))
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC +
            "'Auto' inversion parameter unsupported by this driver, "
            "falling back to 'off'.");
        tuning.inversion = DTVInversion::kInversionOff;
    }

    // DVB-S needs a fully initialized diseqc tree and is checked later in Tune
    if (!diseqc_tree)
    {
        const DVBChannel *master = GetMasterLock();
        if (master == NULL || !master->diseqc_tree)
            CheckFrequency(tuning.frequency);
        ReturnMasterLock(master);
    }

    if (tunerType.IsFECVariable() &&
        symbol_rate_minimum && symbol_rate_maximum &&
        (symbol_rate_minimum <= symbol_rate_maximum) &&
        (tuning.symbolrate < symbol_rate_minimum ||
         tuning.symbolrate > symbol_rate_maximum))
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC +
            QString("Symbol Rate setting (%1) is out of range (min/max:%2/%3)")
                .arg(tuning.symbolrate)
                .arg(symbol_rate_minimum).arg(symbol_rate_maximum));
    }

    if (tunerType.IsFECVariable() && !CheckCodeRate(tuning.fec))
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC +
            "Selected fec_inner parameter unsupported by this driver.");
    }

    if (tunerType.IsModulationVariable() && !CheckModulation(tuning.modulation))
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC +
           "Selected modulation parameter unsupported by this driver.");
    }

    if (DTVTunerType::kTunerTypeDVBT != tunerType)
    {
        LOG(VB_CHANNEL, LOG_INFO, LOC + tuning.toString());
        return;
    }

    // Check OFDM Tuning params

    if (!CheckCodeRate(tuning.hp_code_rate))
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC +
            "Selected code_rate_hp parameter unsupported by this driver.");
    }

    if (!CheckCodeRate(tuning.lp_code_rate))
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC +
            "Selected code_rate_lp parameter unsupported by this driver.");
    }

    if ((tuning.bandwidth == DTVBandwidth::kBandwidthAuto) &&
        !(capabilities & FE_CAN_BANDWIDTH_AUTO))
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC +
            "'Auto' bandwidth parameter unsupported by this driver.");
    }

    if ((tuning.trans_mode == DTVTransmitMode::kTransmissionModeAuto) &&
        !(capabilities & FE_CAN_TRANSMISSION_MODE_AUTO))
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC +
            "'Auto' transmission_mode parameter unsupported by this driver.");
    }

    if ((tuning.guard_interval == DTVGuardInterval::kGuardIntervalAuto) &&
        !(capabilities & FE_CAN_GUARD_INTERVAL_AUTO))
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC +
            "'Auto' guard_interval parameter unsupported by this driver.");
    }

    if ((tuning.hierarchy == DTVHierarchy::kHierarchyAuto) &&
        !(capabilities & FE_CAN_HIERARCHY_AUTO))
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC +
            "'Auto' hierarchy parameter unsupported by this driver. ");
    }

    if (!CheckModulation(tuning.modulation))
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC +
            "Selected modulation parameter unsupported by this driver.");
    }

    LOG(VB_CHANNEL, LOG_INFO, LOC + tuning.toString());
}

/**
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

/**
 *  \brief Return true iff modulation is supported modulation on the frontend
 */
bool DVBChannel::CheckModulation(DTVModulation modulation) const
{
    const DTVModulation m = modulation;
    const uint64_t      c = capabilities;

    return
        ((DTVModulation::kModulationQPSK    == m) && (c & FE_CAN_QPSK))     ||
#if HAVE_FE_CAN_2G_MODULATION
        ((DTVModulation::kModulation8PSK    == m) && (c & FE_CAN_2G_MODULATION)) ||
#endif //HAVE_FE_CAN_2G_MODULATION
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
    if (!dvbcam->IsRunning())
        dvbcam->Start();
    if (pmt && dvbcam->IsRunning())
        dvbcam->SetPMT(this, pmt);
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


bool DVBChannel::Tune(const DTVMultiplex &tuning, QString inputname)
{
    int inputid = inputname.isEmpty() ?
        m_currentInputID : GetInputByName(inputname);
    if (inputid < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + QString("Tune(): Invalid input '%1'.")
                .arg(inputname));
        return false;
    }
    return Tune(tuning, inputid, false, false);
}

#if DVB_API_VERSION >= 5
static struct dtv_properties *dtvmultiplex_to_dtvproperties(
    DTVTunerType tuner_type, const DTVMultiplex &tuning, int intermediate_freq,
    bool can_fec_auto, bool do_tune = true)
{
    uint c = 0;
    struct dtv_properties *cmdseq;

    if (tuner_type != DTVTunerType::kTunerTypeDVBT  &&
        tuner_type != DTVTunerType::kTunerTypeDVBC  &&
        tuner_type != DTVTunerType::kTunerTypeDVBS1 &&
        tuner_type != DTVTunerType::kTunerTypeDVBS2)
    {
        LOG(VB_GENERAL, LOG_ERR, "DVBChan: Unsupported tuner type " +
            tuner_type.toString());
        return NULL;
    }

    cmdseq = (struct dtv_properties*) calloc(1, sizeof(*cmdseq));
    if (!cmdseq)
        return NULL;

    cmdseq->props = (struct dtv_property*) calloc(11, sizeof(*(cmdseq->props)));
    if (!(cmdseq->props))
    {
        free(cmdseq);
        return NULL;
    }

    // The cx24116 DVB-S2 demod anounce FE_CAN_FEC_AUTO but has apparently
    // trouble with FEC_AUTO on DVB-S2 transponders
    if (tuning.mod_sys == DTVModulationSystem::kModulationSystem_DVBS2)
        can_fec_auto = false;

    if (tuner_type == DTVTunerType::kTunerTypeDVBS2)
    {
        cmdseq->props[c].cmd      = DTV_DELIVERY_SYSTEM;
        cmdseq->props[c++].u.data = tuning.mod_sys;
    }

    cmdseq->props[c].cmd      = DTV_FREQUENCY;
    cmdseq->props[c++].u.data = intermediate_freq ? intermediate_freq : tuning.frequency;
    cmdseq->props[c].cmd      = DTV_MODULATION;
    cmdseq->props[c++].u.data = tuning.modulation;
    cmdseq->props[c].cmd      = DTV_INVERSION;
    cmdseq->props[c++].u.data = tuning.inversion;

    if (tuner_type == DTVTunerType::kTunerTypeDVBS1 ||
        tuner_type == DTVTunerType::kTunerTypeDVBS2 ||
        tuner_type == DTVTunerType::kTunerTypeDVBC)
    {
        cmdseq->props[c].cmd      = DTV_SYMBOL_RATE;
        cmdseq->props[c++].u.data = tuning.symbolrate;
    }

    if (tuner_type.IsFECVariable())
    {
        cmdseq->props[c].cmd      = DTV_INNER_FEC;
        cmdseq->props[c++].u.data = can_fec_auto ? FEC_AUTO : tuning.fec;
    }

    if (tuner_type == DTVTunerType::kTunerTypeDVBT)
    {
        cmdseq->props[c].cmd      = DTV_BANDWIDTH_HZ;
        cmdseq->props[c++].u.data = (8-tuning.bandwidth) * 1000000;
        cmdseq->props[c].cmd      = DTV_CODE_RATE_HP;
        cmdseq->props[c++].u.data = tuning.hp_code_rate;
        cmdseq->props[c].cmd      = DTV_CODE_RATE_LP;
        cmdseq->props[c++].u.data = tuning.lp_code_rate;
        cmdseq->props[c].cmd      = DTV_TRANSMISSION_MODE;
        cmdseq->props[c++].u.data = tuning.trans_mode;
        cmdseq->props[c].cmd      = DTV_GUARD_INTERVAL;
        cmdseq->props[c++].u.data = tuning.guard_interval;
        cmdseq->props[c].cmd      = DTV_HIERARCHY;
        cmdseq->props[c++].u.data = tuning.hierarchy;
    }

    if (tuning.mod_sys == DTVModulationSystem::kModulationSystem_DVBS2)
    {
        cmdseq->props[c].cmd      = DTV_PILOT;
        cmdseq->props[c++].u.data = PILOT_AUTO;
        cmdseq->props[c].cmd      = DTV_ROLLOFF;
        cmdseq->props[c++].u.data = tuning.rolloff;
    }
    else if (tuning.mod_sys == DTVModulationSystem::kModulationSystem_DVBS)
    {
        cmdseq->props[c].cmd      = DTV_ROLLOFF;
        cmdseq->props[c++].u.data = DTVRollOff::kRollOff_35;
    }

    if (do_tune)
        cmdseq->props[c++].cmd    = DTV_TUNE;

    cmdseq->num = c;

    return cmdseq;
}
#endif


/*****************************************************************************
           Tuning functions for each of the five types of cards.
 *****************************************************************************/

/**
 *  \brief Tunes the card to a frequency but does not deal with PIDs.
 *
 *   This is used by DVB Channel Scanner, the EIT Parser, and by TVRec.
 *
 *  \param tuning      Info on transport to tune to
 *  \param inputid     Optional, forces specific input (for DiSEqC)
 *  \param force_reset If true, frequency tuning is done
 *                     even if it should not be needed.
 *  \param same_input  Optional, doesn't change input (for retuning).
 *  \return true on success, false on failure
 */
bool DVBChannel::Tune(const DTVMultiplex &tuning,
                      uint inputid,
                      bool force_reset,
                      bool same_input)
{
    QMutexLocker lock(&tune_lock);
    QMutexLocker locker(&hw_lock);

    DVBChannel *master = GetMasterLock();
    if (master != this)
    {
        LOG(VB_CHANNEL, LOG_INFO, LOC + "tuning on slave channel");
        SetSIStandard(tuning.sistandard);
        bool ok = master->Tune(tuning, inputid, force_reset, false);
        ReturnMasterLock(master);
        return ok;
    }
    ReturnMasterLock(master); // if we're the master we don't need this lock..


    int intermediate_freq = 0;
    bool can_fec_auto = false;
    bool reset = (force_reset || first_tune);

    if (tunerType.IsDiSEqCSupported() && !diseqc_tree)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            "DVB-S needs device tree for LNB handling");
        return false;
    }

    desired_tuning = tuning;

    if (fd_frontend < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Tune(): Card not open!");

        return false;
    }

    // Remove any events in queue before tuning.
    drain_dvb_events(fd_frontend);

    LOG(VB_CHANNEL, LOG_INFO, LOC + "\nOld Params: " + prev_tuning.toString() +
            "\nNew Params: " + tuning.toString());

    // DVB-S is in kHz, other DVB is in Hz
    bool is_dvbs = ((DTVTunerType::kTunerTypeDVBS1 == tunerType) ||
                    (DTVTunerType::kTunerTypeDVBS2 == tunerType));
    int     freq_mult = (is_dvbs) ? 1 : 1000;
    QString suffix    = (is_dvbs) ? "kHz" : "Hz";

    if (reset || !prev_tuning.IsEqual(tunerType, tuning, 500 * freq_mult))
    {
        LOG(VB_CHANNEL, LOG_INFO, LOC + QString("Tune(): Tuning to %1%2")
                .arg(intermediate_freq ? intermediate_freq : tuning.frequency)
                .arg(suffix));

        tune_delay_lock.lock();

        if (QDateTime::currentDateTime() < last_tuning)
        {
            LOG(VB_GENERAL, LOG_INFO, LOC + QString("Next tuning after less than %1ms. Delaying by %1ms")
                .arg(concurrent_tunings_delay));
            usleep(concurrent_tunings_delay * 1000);
        }

        last_tuning = QDateTime::currentDateTime();
        last_tuning = last_tuning.addMSecs(concurrent_tunings_delay);

        tune_delay_lock.unlock();

        // send DVB-S setup
        if (diseqc_tree)
        {
            // configure for new input
            if (!same_input)
                diseqc_settings.Load(inputid);

            // execute diseqc commands
            if (!diseqc_tree->Execute(diseqc_settings, tuning))
            {
                LOG(VB_GENERAL, LOG_ERR, LOC +
                    "Tune(): Failed to setup DiSEqC devices");
                return false;
            }

            // retrieve actual intermediate frequency
            DiSEqCDevLNB *lnb = diseqc_tree->FindLNB(diseqc_settings);
            if (!lnb)
            {
                LOG(VB_GENERAL, LOG_ERR, LOC +
                    "Tune(): No LNB for this configuration");
                return false;
            }

            if (lnb->GetDeviceID() != last_lnb_dev_id)
            {
                last_lnb_dev_id = lnb->GetDeviceID();
                // make sure we tune to frequency, if the lnb has changed
                first_tune = true;
            }

            intermediate_freq = lnb->GetIntermediateFrequency(
                diseqc_settings, tuning);

            // retrieve scr intermediate frequency
            DiSEqCDevSCR *scr = diseqc_tree->FindSCR(diseqc_settings);
            if (lnb && scr)
            {
                intermediate_freq = scr->GetIntermediateFrequency(intermediate_freq);
            }

            // if card can auto-FEC, use it -- sometimes NITs are inaccurate
            if (capabilities & FE_CAN_FEC_AUTO)
                can_fec_auto = true;

            // Check DVB-S intermediate frequency here since it requires a fully
            // initialized diseqc tree
            CheckFrequency(intermediate_freq);
        }

#if DVB_API_VERSION >=5
        if (DTVTunerType::kTunerTypeDVBS2 == tunerType)
        {
            struct dtv_property p_clear;
            struct dtv_properties cmdseq_clear;

            p_clear.cmd        = DTV_CLEAR;
            cmdseq_clear.num   = 1;
            cmdseq_clear.props = &p_clear;

            if ((ioctl(fd_frontend, FE_SET_PROPERTY, &cmdseq_clear)) < 0)
            {
                LOG(VB_GENERAL, LOG_ERR, LOC +
                    "Tune(): Clearing DTV properties cache failed." + ENO);
                return false;
            }

            struct dtv_properties *cmds = dtvmultiplex_to_dtvproperties(
                tunerType, tuning, intermediate_freq, can_fec_auto);

            if (!cmds) {
                LOG(VB_GENERAL, LOG_ERR, LOC +
                    "Failed to convert DTVMultiplex to DTV_PROPERTY sequence");
                return false;
            }

            if (VERBOSE_LEVEL_CHECK(VB_CHANNEL, LOG_DEBUG))
            {
                for (uint i = 0; i < cmds->num; i++)
                {
                    LOG(VB_CHANNEL, LOG_DEBUG, LOC +
                        QString("prop %1: cmd = %2, data %3")
                            .arg(i).arg(cmds->props[i].cmd)
                            .arg(cmds->props[i].u.data));
                }
            }

            int res = ioctl(fd_frontend, FE_SET_PROPERTY, cmds);

            free(cmds->props);
            free(cmds);

            if (res < 0)
            {
                LOG(VB_GENERAL, LOG_ERR, LOC +
                    "Tune(): Setting Frontend tuning parameters failed." + ENO);
                return false;
            }
        }
        else
#endif
        {
            struct dvb_frontend_parameters params = dtvmultiplex_to_dvbparams(
                tunerType, tuning, intermediate_freq, can_fec_auto);

            if (ioctl(fd_frontend, FE_SET_FRONTEND, &params) < 0)
            {
                LOG(VB_GENERAL, LOG_ERR, LOC +
                    "Tune(): Setting Frontend tuning parameters failed." + ENO);
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

    LOG(VB_CHANNEL, LOG_INFO, LOC + "Tune(): Frequency tuning successful.");

    return true;
}

bool DVBChannel::Retune(void)
{
    return Tune(desired_tuning, m_currentInputID, true, true);
}

QString DVBChannel::GetFrontendName(void) const
{
    QString tmp = frontend_name;
    tmp.detach();
    return tmp;
}

/** \fn DVBChannel::IsTuningParamsProbeSupported(void) const
 *  \brief Returns true iff tuning info probing is working.
 */
bool DVBChannel::IsTuningParamsProbeSupported(void) const
{
    QMutexLocker locker(&hw_lock);

    if (fd_frontend < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Card not open!");

        return false;
    }

    const DVBChannel *master = GetMasterLock();
    if (master != this)
    {
        bool ok = master->IsTuningParamsProbeSupported();
        ReturnMasterLock(master);
        return ok;
    }
    ReturnMasterLock(master); // if we're the master we don't need this lock..

    if (diseqc_tree)
    {
        // TODO We need to implement the inverse of
        // lnb->GetIntermediateFrequency() for ProbeTuningParams()
        // to accurately reflect the frequency before LNB transform.
        return false;
    }

    dvb_frontend_parameters params;

    int res = ioctl(fd_frontend, FE_GET_FRONTEND, &params);
    if (res < 0)
    {
        LOG(VB_CHANNEL, LOG_ERR, LOC + "Getting device frontend failed." + ENO);
    }

    return (res >= 0);
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
    QMutexLocker locker(&hw_lock);

    if (fd_frontend < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Card not open!");

        return false;
    }

    const DVBChannel *master = GetMasterLock();
    if (master != this)
    {
        bool ok = master->ProbeTuningParams(tuning);
        ReturnMasterLock(master);
        return ok;
    }
    ReturnMasterLock(master); // if we're the master we don't need this lock..

    if (diseqc_tree)
    {
        // TODO We need to implement the inverse of
        // lnb->GetIntermediateFrequency() for ProbeTuningParams()
        // to accurately reflect the frequency before LNB transform.
        return false;
    }

    if (tunerType == DTVTunerType::kTunerTypeDVBS2)
    {
        // TODO implement probing of tuning parameters with FE_GET_PROPERTY
        return false;
    }

    dvb_frontend_parameters params;
    if (ioctl(fd_frontend, FE_GET_FRONTEND, &params) < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            "Getting Frontend tuning parameters failed." + ENO);

        return false;
    }

    uint    mplex      = tuning.mplex;
    QString sistandard = tuning.sistandard; sistandard.detach();

    tuning = dvbparams_to_dtvmultiplex(tunerType, params);

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
    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("SELECT chanid "
                  "FROM channel, cardinput "
                  "WHERE cardinput.sourceid = channel.sourceid AND "
                  "      channel.channum = :CHANNUM AND "
                  "      cardinput.cardid = :CARDID");

    query.bindValue(":CHANNUM", m_curchannelname);
    query.bindValue(":CARDID", GetCardID());

    if (!query.exec() || !query.isActive())
    {
        MythDB::DBError("fetching chanid", query);
        return -1;
    }

    if (!query.next())
        return -1;

    return query.value(0).toInt();
}

const DiSEqCDevRotor *DVBChannel::GetRotor(void) const
{
    if (diseqc_tree)
        return diseqc_tree->FindRotor(diseqc_settings);

    return NULL;
}

// documented in dvbchannel.h
bool DVBChannel::HasLock(bool *ok) const
{
    const DVBChannel *master = GetMasterLock();
    if (master != this)
    {
        bool haslock = master->HasLock(ok);
        ReturnMasterLock(master);
        return haslock;
    }
    ReturnMasterLock(master); // if we're the master we don't need this lock..

    fe_status_t status;
    memset(&status, 0, sizeof(status));

    int ret = ioctl(fd_frontend, FE_READ_STATUS, &status);
    if (ret < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            "Getting Frontend status failed." + ENO);
    }

    if (ok)
        *ok = (0 == ret);

    return status & FE_HAS_LOCK;
}

// documented in dvbchannel.h
double DVBChannel::GetSignalStrength(bool *ok) const
{
    const DVBChannel *master = GetMasterLock();
    if (master != this)
    {
        double val = master->GetSignalStrength(ok);
        ReturnMasterLock(master);
        return val;
    }
    ReturnMasterLock(master); // if we're the master we don't need this lock..

    // We use uint16_t for sig because this is correct for DVB API 4.0,
    // and works better than the correct int16_t for the 3.x API
    uint16_t sig = 0;

    int ret = ioctl(fd_frontend, FE_READ_SIGNAL_STRENGTH, &sig);
    if (ret < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            "Getting Frontend signal strength failed." + ENO);
    }

    if (ok)
        *ok = (0 == ret);

    return sig * (1.0 / 65535.0);
}

// documented in dvbchannel.h
double DVBChannel::GetSNR(bool *ok) const
{
    const DVBChannel *master = GetMasterLock();
    if (master != this)
    {
        double val = master->GetSNR(ok);
        ReturnMasterLock(master);
        return val;
    }
    ReturnMasterLock(master); // if we're the master we don't need this lock..

    // We use uint16_t for sig because this is correct for DVB API 4.0,
    // and works better than the correct int16_t for the 3.x API

    uint16_t snr = 0;
    int ret = ioctl(fd_frontend, FE_READ_SNR, &snr);
    if (ret < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            "Getting Frontend signal/noise ratio failed." + ENO);
    }

    if (ok)
        *ok = (0 == ret);

    return snr * (1.0 / 65535.0);
}

// documented in dvbchannel.h
double DVBChannel::GetBitErrorRate(bool *ok) const
{
    const DVBChannel *master = GetMasterLock();
    if (master != this)
    {
        double val = master->GetBitErrorRate(ok);
        ReturnMasterLock(master);
        return val;
    }
    ReturnMasterLock(master); // if we're the master we don't need this lock..

    uint32_t ber = 0;
    int ret = ioctl(fd_frontend, FE_READ_BER, &ber);
    if (ret < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            "Getting Frontend signal error rate failed." + ENO);
    }

    if (ok)
        *ok = (0 == ret);

    return (double) ber;
}

// documented in dvbchannel.h
double DVBChannel::GetUncorrectedBlockCount(bool *ok) const
{
    const DVBChannel *master = GetMasterLock();
    if (master != this)
    {
        double val = master->GetUncorrectedBlockCount(ok);
        ReturnMasterLock(master);
        return val;
    }
    ReturnMasterLock(master); // if we're the master we don't need this lock..

    uint32_t ublocks = 0;
    int ret = ioctl(fd_frontend, FE_READ_UNCORRECTED_BLOCKS, &ublocks);
    if (ret < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            "Getting Frontend uncorrected block count failed." + ENO);
    }

    if (ok)
        *ok = (0 == ret);

    return (double) ublocks;
}

DVBChannel *DVBChannel::GetMasterLock(void)
{
    QString devname = CardUtil::GetDeviceName(DVB_DEV_FRONTEND, device);
    DTVChannel *master = DTVChannel::GetMasterLock(devname);
    DVBChannel *dvbm = dynamic_cast<DVBChannel*>(master);
    if (master && !dvbm)
        DTVChannel::ReturnMasterLock(master);
    return dvbm;
}

void DVBChannel::ReturnMasterLock(DVBChannelP &dvbm)
{
    DTVChannel *chan = static_cast<DTVChannel*>(dvbm);
    DTVChannel::ReturnMasterLock(chan);
    dvbm = NULL;
}

const DVBChannel *DVBChannel::GetMasterLock(void) const
{
    QString devname = CardUtil::GetDeviceName(DVB_DEV_FRONTEND, device);
    DTVChannel *master = DTVChannel::GetMasterLock(devname);
    DVBChannel *dvbm = dynamic_cast<DVBChannel*>(master);
    if (master && !dvbm)
        DTVChannel::ReturnMasterLock(master);
    return dvbm;
}

void DVBChannel::ReturnMasterLock(DVBChannelCP &dvbm)
{
    DTVChannel *chan =
        static_cast<DTVChannel*>(const_cast<DVBChannel*>(dvbm));
    DTVChannel::ReturnMasterLock(chan);
    dvbm = NULL;
}

bool DVBChannel::IsMaster(void) const
{
    const DVBChannel *master = GetMasterLock();
    bool is_master = (master == this);
    ReturnMasterLock(master);
    return is_master;
}

/** \fn drain_dvb_events(int)
 *  \brief Reads all the events off the queue, so we can use select
 *         in wait_for_backend(int,int).
 */
static void drain_dvb_events(int fd)
{
    struct dvb_frontend_event event;
    int ret = 0;
    while ((ret = ioctl(fd, FE_GET_EVENT, &event)) == 0);
    if (ret < 0)
    {
        LOG(VB_CHANNEL, LOG_DEBUG, "Draining DVB Event failed. " + ENO);
    }
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
    int ret = 0;
    do ret = select(fd+1, &fd_select_set, NULL, NULL, &select_timeout);
    while ((-1 == ret) && (EINTR == errno));

    if (-1 == ret)
    {
        LOG(VB_GENERAL, LOG_ERR,
            "DVBChan: wait_for_backend: Failed to wait on output" + ENO);

        return false;
    }

    // This is supposed to work on all cards, post 2.6.12...
    fe_status_t status;
    if (ioctl(fd, FE_READ_STATUS, &status) < 0)
    {
        LOG(VB_GENERAL, LOG_ERR,
            "DVBChan: wait_for_backend: Failed to get status" + ENO);

        return false;
    }

    LOG(VB_CHANNEL, LOG_INFO, QString("DVBChan: wait_for_backend: Status: %1")
            .arg(toString(status)));

    return true;
}

static struct dvb_frontend_parameters dtvmultiplex_to_dvbparams(
    DTVTunerType tuner_type, const DTVMultiplex &tuning,
    int intermediate_freq, bool can_fec_auto)
{
    dvb_frontend_parameters params;
    memset(&params, 0, sizeof(params));

    params.frequency = tuning.frequency;
    params.inversion = (fe_spectral_inversion_t) (int) tuning.inversion;

    if (DTVTunerType::kTunerTypeDVBS1 == tuner_type)
    {
        if (tuning.mod_sys == DTVModulationSystem::kModulationSystem_DVBS2)
            LOG(VB_GENERAL, LOG_ERR,
                "DVBChan: Error, Tuning of a DVB-S2 transport "
                "with a DVB-S card will fail.");

        params.frequency = intermediate_freq;
        params.u.qpsk.symbol_rate = tuning.symbolrate;
        params.u.qpsk.fec_inner   = can_fec_auto ? FEC_AUTO
            : (fe_code_rate_t) (int) tuning.fec;
    }

    if (DTVTunerType::kTunerTypeDVBS2 == tuner_type)
    {
        LOG(VB_GENERAL, LOG_ERR,
            "DVBChan: Error, MythTV was compiled without "
            "DVB-S2 headers being present so DVB-S2 tuning will fail.");
    }

    if (DTVTunerType::kTunerTypeDVBC == tuner_type)
    {
        params.u.qam.symbol_rate  = tuning.symbolrate;
        params.u.qam.fec_inner    = (fe_code_rate_t) (int) tuning.fec;
        params.u.qam.modulation   = (fe_modulation_t) (int) tuning.modulation;
    }

    if (DTVTunerType::kTunerTypeDVBT == tuner_type)
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
        params.u.vsb.modulation   =
            (fe_modulation_t) (int) tuning.modulation;
    }

    return params;
}

static DTVMultiplex dvbparams_to_dtvmultiplex(
    DTVTunerType tuner_type, const dvb_frontend_parameters &params)
{
    DTVMultiplex tuning;

    tuning.frequency = params.frequency;
    tuning.inversion = params.inversion;

    if ((DTVTunerType::kTunerTypeDVBS1 == tuner_type) ||
        (DTVTunerType::kTunerTypeDVBS2 == tuner_type))
    {
        tuning.symbolrate     = params.u.qpsk.symbol_rate;
        tuning.fec            = params.u.qpsk.fec_inner;
    }

    if (DTVTunerType::kTunerTypeDVBC   == tuner_type)
    {
        tuning.symbolrate     = params.u.qam.symbol_rate;
        tuning.fec            = params.u.qam.fec_inner;
        tuning.modulation     = params.u.qam.modulation;
    }

    if (DTVTunerType::kTunerTypeDVBT   == tuner_type)
    {
        tuning.bandwidth      = params.u.ofdm.bandwidth;
        tuning.hp_code_rate   = params.u.ofdm.code_rate_HP;
        tuning.lp_code_rate   = params.u.ofdm.code_rate_LP;
        tuning.modulation     = params.u.ofdm.constellation;
        tuning.trans_mode     = params.u.ofdm.transmission_mode;
        tuning.guard_interval = params.u.ofdm.guard_interval;
        tuning.hierarchy      = params.u.ofdm.hierarchy_information;
    }

    if (DTVTunerType::kTunerTypeATSC    == tuner_type)
    {
        tuning.modulation     = params.u.vsb.modulation;
    }

    return tuning;
}
