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
#include <algorithm>
#include <utility>
#include <sys/poll.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>

// C++ headers
#include <chrono>
#include <thread>

// MythTV headers
#include "libmythbase/mythconfig.h"
#include "libmythbase/mythdb.h"
#include "cardutil.h"
#include "channelutil.h"
#include "dvbtypes.h"
#include "dvbchannel.h"
#include "dvbcam.h"
#include "tv_rec.h"


// Local functions
static struct dvb_frontend_parameters dtvmultiplex_to_dvbparams(
    DTVTunerType tuner_type, const DTVMultiplex& tuning, uint intermediate_freq, bool can_fec_auto);
static DTVMultiplex dvbparams_to_dtvmultiplex(
    DTVTunerType tuner_type, const dvb_frontend_parameters& params);
static struct dtv_properties *dtvmultiplex_to_dtvproperties(uint inputId,
    DTVTunerType tuner_type, DTVModulationSystem current_sys, const DTVMultiplex &tuning,
    uint intermediate_freq, bool can_fec_auto, bool do_tune = true);

static constexpr std::chrono::milliseconds concurrent_tunings_delay { 1s };
std::chrono::milliseconds DVBChannel::s_lastTuning = MythDate::currentMSecsSinceEpochAsDuration();

#define LOC QString("DVBChan[%1](%2): ").arg(m_inputId).arg(DVBChannel::GetDevice())

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define DTV_STAT_FULL_DEBUG 0 // All DTV_STAT_xxx values

/** \class DVBChannel
 *  \brief Provides interface to the tuning hardware when using DVB drivers
 *
 *  \bug Only supports single input cards.
 */
DVBChannel::DVBChannel(QString aDevice, TVRec *parent)
    : DTVChannel(parent), m_device(std::move(aDevice))
{
    s_master_map_lock.lockForWrite();
    m_key = CardUtil::GetDeviceName(DVB_DEV_FRONTEND, m_device);
    if (m_pParent)
        m_key += QString(":%1")
            .arg(CardUtil::GetSourceID(m_pParent->GetInputId()));

    s_master_map[m_key].push_back(this); // == RegisterForMaster
    auto *master = dynamic_cast<DVBChannel*>(s_master_map[m_key].front());
    if (master == this)
    {
        m_dvbCam = new DVBCam(m_device);
        m_hasCrcBug = CardUtil::HasDVBCRCBug(m_device);
    }
    else if (master != nullptr)
    {
        m_dvbCam    = master->m_dvbCam;
        m_hasCrcBug = master->m_hasCrcBug;
    }
    s_master_map_lock.unlock();

    m_sigMonDelay = CardUtil::GetMinSignalMonitoringDelay(m_device);
}

DVBChannel::~DVBChannel()
{
    // Set a new master if there are other instances and we're the master
    // whether we are the master or not remove us from the map..
    s_master_map_lock.lockForWrite();
    auto *master = dynamic_cast<DVBChannel*>(s_master_map[m_key].front());
    if (master == this)
    {
        s_master_map[m_key].pop_front();
        DVBChannel *new_master = nullptr;
        if (!s_master_map[m_key].empty())
            new_master = dynamic_cast<DVBChannel*>(s_master_map[m_key].front());
        if (new_master)
        {
            QMutexLocker master_locker(&(master->m_hwLock));
            QMutexLocker new_master_locker(&(new_master->m_hwLock));
            new_master->m_isOpen = master->m_isOpen;
        }
    }
    else
    {
        s_master_map[m_key].removeAll(this);
    }
    s_master_map_lock.unlock();

    DVBChannel::Close();

    // If we're the last one out delete dvbcam
    s_master_map_lock.lockForRead();
    MasterMap::iterator mit = s_master_map.find(m_key);
    if ((*mit).empty())
        delete m_dvbCam;
    m_dvbCam = nullptr;
    s_master_map_lock.unlock();

    // diseqc_tree is managed elsewhere
}

void DVBChannel::Close(DVBChannel *who)
{
    LOG(VB_CHANNEL, LOG_INFO, LOC + "Closing DVB channel");

    QMutexLocker locker(&m_hwLock);

    IsOpenMap::iterator it = m_isOpen.find(who);
    if (it == m_isOpen.end())
        return; // This caller didn't have it open in the first place..

    m_isOpen.erase(it);

    DVBChannel *master = GetMasterLock();
    if (master != nullptr && master != this)
    {
        if (m_dvbCam->IsRunning())
            m_dvbCam->SetPMT(this, nullptr);
        master->Close(this);
        m_fdFrontend = -1;
        ReturnMasterLock(master);
        return;
    }
    ReturnMasterLock(master); // If we're the master we don't need this lock.

    if (!m_isOpen.empty())
        return; // Not all callers have closed the DVB channel yet..

    if (m_diseqcTree)
        m_diseqcTree->Close();

    if (m_fdFrontend >= 0)
    {
        close(m_fdFrontend);
        m_fdFrontend = -1;

        m_dvbCam->Stop();
    }
}

bool DVBChannel::Open(DVBChannel *who)
{
    if (!m_inputId)
    {
        if (!InitializeInput())
            return false;
    }

    LOG(VB_CHANNEL, LOG_INFO, LOC + "Opening DVB channel");

    m_legacyFe = gCoreContext->GetDVBv3();
    if (m_legacyFe)
    {
        LOG(VB_CHANNEL, LOG_INFO, LOC + "Use legacy DVBv3 API");
    }

    QMutexLocker locker(&m_hwLock);

    if (m_fdFrontend >= 0)
    {
        m_isOpen[who] = true;
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

        m_fdFrontend          = master->m_fdFrontend;
        m_frontendName        = master->m_frontendName;
        m_tunerType           = master->m_tunerType;
        m_capabilities        = master->m_capabilities;
        m_extModulations      = master->m_extModulations;
        m_frequencyMinimum    = master->m_frequencyMinimum;
        m_frequencyMaximum    = master->m_frequencyMaximum;
        m_symbolRateMinimum   = master->m_symbolRateMinimum;
        m_symbolRateMaximum   = master->m_symbolRateMaximum;
        m_version             = master->m_version;
        m_legacyFe            = master->m_legacyFe;
        m_hasV5Stats          = master->m_hasV5Stats;
        m_currentSys          = master->m_currentSys;
        m_sysList             = master->m_sysList;

        m_isOpen[who] = true;

        if (!InitializeInput())
        {
            Close();
            ReturnMasterLock(master);
            return false;
        }

        ReturnMasterLock(master);
        return true;
    }
    ReturnMasterLock(master); // If we're the master we don't need this lock.

    // Open the DVB device
    //
    QString dvbdev = CardUtil::GetDeviceName(DVB_DEV_FRONTEND, m_device);
    QByteArray devn = dvbdev.toLatin1();

    for (int tries = 1; ; ++tries)
    {
        m_fdFrontend = open(devn.constData(), O_RDWR | O_NONBLOCK);
        if (m_fdFrontend >= 0)
            break;
        if (tries == 1)
        {
            LOG(VB_GENERAL, LOG_WARNING, LOC +
                "Opening DVB frontend device failed." + ENO);
        }
        if (tries >= 5 || (errno != EBUSY && errno != EAGAIN))
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                QString("Failed to open DVB frontend device due to "
                        "fatal error or too many attempts."));
            return false;
        }
        std::this_thread::sleep_for(50ms);
    }

    // Get the basic information from the frontend
    //
    dvb_frontend_info info {};
    if (ioctl(m_fdFrontend, FE_GET_INFO, &info) < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            "Failed to get frontend information." + ENO);

        close(m_fdFrontend);
        m_fdFrontend = -1;
        return false;
    }

    m_frontendName        = info.name;
    m_capabilities        = info.caps;
    m_frequencyMinimum    = info.frequency_min;
    m_frequencyMaximum    = info.frequency_max;
    m_symbolRateMinimum   = info.symbol_rate_min;
    m_symbolRateMaximum   = info.symbol_rate_max;

    LOG(VB_CHANNEL, LOG_INFO, LOC +
        QString("Frontend '%1' capabilities:").arg(m_frontendName));
    for (auto & capstr : CardUtil::CapabilitiesToString(m_capabilities))
    {
        LOG(VB_CHANNEL, LOG_INFO, QString("    %1").arg(capstr));
    }

    // Does this card use the DVBv5 or the legacy DVBv3 API?
    {
        std::array<struct dtv_property,2> prop = {};
        struct dtv_properties cmd = {};

        prop[0].cmd = DTV_API_VERSION;
        prop[1].cmd = DTV_DELIVERY_SYSTEM;

        cmd.num = 2;
        cmd.props = prop.data();

        if (ioctl(m_fdFrontend, FE_GET_PROPERTY, &cmd) == -1)
        {
            prop[0].u.data = 0x300;
            prop[1].u.data = SYS_UNDEFINED;
        }

	    m_version = prop[0].u.data;
	    m_currentSys = prop[1].u.data;

        m_legacyFe = m_version < 0x500 ? true : m_legacyFe;
        m_hasV5Stats = m_version >= 0x50a;
    }

    // Determine tuner capabilities and configured delivery system
    //
    m_sysList.clear();
    if (m_legacyFe || m_version < 0x505)
    {
        // Legacy DVBv3 API
        //
        DTVModulationSystem delsys;
        m_legacyFe = true;
        switch (info.type) {
        case FE_QPSK:
            m_currentSys = SYS_DVBS;
            m_sysList.append(m_currentSys);

            if (m_version < 0x0500)
                break;
            if (info.caps & FE_CAN_2G_MODULATION)
            {
                delsys = SYS_DVBS2;
                m_sysList.append(delsys);
            }
            if (info.caps & FE_CAN_TURBO_FEC)
            {
                delsys = SYS_TURBO;
                m_sysList.append(delsys);
            }
            break;
        case FE_QAM:
            m_currentSys = SYS_DVBC_ANNEX_A;
            m_sysList.append(m_currentSys);
            break;
        case FE_OFDM:
            m_currentSys = SYS_DVBT;
            m_sysList.append(m_currentSys);
            if (m_version < 0x0500)
                break;
            if (info.caps & FE_CAN_2G_MODULATION)
            {
                delsys = SYS_DVBT2;
                m_sysList.append(delsys);
            }
            break;
        case FE_ATSC:
            if (info.caps & (FE_CAN_8VSB | FE_CAN_16VSB))
            {
                delsys = SYS_ATSC;
                m_sysList.append(delsys);
            }
            if (info.caps & (FE_CAN_QAM_64 | FE_CAN_QAM_256 | FE_CAN_QAM_AUTO))
            {
                delsys = SYS_DVBC_ANNEX_B;
                m_sysList.append(delsys);
            }
            m_currentSys = m_sysList.value(0);
            break;
        }
        if (m_sysList.isEmpty())
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                QString("Frontend '%1' delivery system not detected.").arg(m_frontendName));
			close(m_fdFrontend);
			return false;
        }
    }
    else
    {
        // DVBv5 API
        //
        std::array<struct dtv_property,1> prop = {};
        struct dtv_properties cmd = {};

        prop[0].cmd = DTV_ENUM_DELSYS;

        cmd.num = 1;
        cmd.props = prop.data();

        if (ioctl(m_fdFrontend, FE_GET_PROPERTY, &cmd) == -1)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                QString("Frontend '%1' FE_GET_PROPERTY failed.").arg(m_frontendName));
            close(m_fdFrontend);
            return false;
        }

        int p_num_systems = prop[0].u.buffer.len;
        for (int i = 0; i < p_num_systems; i++)
        {
            DTVModulationSystem delsys(prop[0].u.buffer.data[i]);
            m_sysList.append(delsys);
        }

        if (p_num_systems == 0) {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                QString("Frontend '%1' returned 0 supported delivery systems!").arg(m_frontendName));
            close(m_fdFrontend);
            return false;
        }
    }

    // Frontend info
    //
    if (VERBOSE_LEVEL_CHECK(VB_CHANNEL, LOG_INFO))
    {
        LOG(VB_CHANNEL, LOG_INFO, LOC +
            QString("Frontend '%1' ").arg(m_frontendName));
        LOG(VB_CHANNEL, LOG_INFO, LOC +
            QString("DVB version:0x%1 ").arg(m_version,3,16,QChar('0')) +
            QString("Delivery system:%1 ").arg(m_currentSys.toString()) +
            QString("Legacy FE:%1 ").arg(m_legacyFe) +
            QString("Has DVBv5 stats:%1").arg(m_hasV5Stats));

        LOG(VB_CHANNEL, LOG_INFO, "Supported delivery systems: ");
        for (auto & delsys : m_sysList)
        {
            if (delsys == m_currentSys)
            {
                LOG(VB_CHANNEL, LOG_INFO, QString("   [%1]")
                    .arg(delsys.toString()));
            }
            else
            {
                LOG(VB_CHANNEL, LOG_INFO, QString("    %1")
                    .arg(delsys.toString()));
            }
        }

        uint32_t frq_min = info.frequency_min;
        uint32_t frq_max = info.frequency_max;
        uint32_t frq_stp = info.frequency_stepsize;
//      uint32_t frq_tol = info.frequency_tolerance;
        if (info.type == FE_QPSK)   			// Satellite frequencies are in kHz
        {
            frq_min *= 1000;
            frq_max *= 1000;
            frq_stp *= 1000;
//          frq_tol *= 1000;
        }

        LOG(VB_CHANNEL, LOG_INFO, QString("Frequency range for the current standard:"));
        LOG(VB_CHANNEL, LOG_INFO, QString("    From:  %1 Hz").arg(frq_min,11));
        LOG(VB_CHANNEL, LOG_INFO, QString("    To:    %1 Hz").arg(frq_max,11));
        LOG(VB_CHANNEL, LOG_INFO, QString("    Step:  %1 Hz").arg(frq_stp,11));

        if (info.type == FE_QPSK || info.type == FE_QAM)
        {
            LOG(VB_CHANNEL, LOG_INFO, QString("Symbol rate ranges for the current standard:"));
            LOG(VB_CHANNEL, LOG_INFO, QString("    From:  %1 Baud").arg(info.symbol_rate_min,11));
            LOG(VB_CHANNEL, LOG_INFO, QString("    To:    %1 Baud").arg(info.symbol_rate_max,11));
        }
    }

    if (m_currentSys == SYS_UNDEFINED)
        m_currentSys = m_sysList.value(0);

    // Get delivery system from database and configure the tuner if needed.
    DTVModulationSystem delsys = CardUtil::GetDeliverySystem(m_inputId);
    if (DTVModulationSystem::kModulationSystem_UNDEFINED != delsys)
    {
        if (delsys != m_currentSys)
        {
            LOG(VB_CHANNEL, LOG_INFO, LOC +
                QString("Change delivery system from %1 to %2.")
                .arg(m_currentSys.toString(),
                     delsys.toString()));

            CardUtil::SetDeliverySystem(m_inputId, delsys, m_fdFrontend);
            m_currentSys = delsys;
        }
        else
        {
            LOG(VB_CHANNEL, LOG_INFO, LOC +
                QString("Delivery system in database and in card equal, leave at %1.")
                .arg(m_currentSys.toString()));
        }
    }
    else
    {
        LOG(VB_CHANNEL, LOG_INFO, LOC +
            QString("No delivery system in database, leave card at %1.").arg(m_currentSys.toString()));
    }
    m_tunerType = CardUtil::ConvertToTunerType(m_currentSys);

    // Turn on the power to the LNB
    if (m_tunerType.IsDiSEqCSupported())
    {

        m_diseqcTree = DiSEqCDev::FindTree(m_inputId);
        if (m_diseqcTree)
        {
            bool is_SCR = false;

            DiSEqCDevSCR *scr = m_diseqcTree->FindSCR(m_diseqcSettings);
            if (scr)
            {
                is_SCR = true;
                LOG(VB_CHANNEL, LOG_INFO, LOC +
                    QString("Requested %1 channel is on SCR system")
                    .arg(m_tunerType.toString()));
            }
            else
            {
                LOG(VB_CHANNEL, LOG_INFO, LOC +
                    QString("Requested %1 channel is on non-SCR system")
                    .arg(m_tunerType.toString()));
            }
            m_diseqcTree->Open(m_fdFrontend, is_SCR);
        }
    }

    m_firstTune = true;

    if (!InitializeInput())
    {
        Close();
        return false;
    }

    if (m_fdFrontend >= 0)
        m_isOpen[who] = true;

    return (m_fdFrontend >= 0);
}

bool DVBChannel::IsOpen(void) const
{
    // Have to acquire the hw lock to prevent m_isOpen being modified whilst we're searching it.
    QMutexLocker locker(&m_hwLock);
    IsOpenMap::const_iterator it = m_isOpen.find(this);
    return it != m_isOpen.end();
}

bool DVBChannel::Init(QString &startchannel, bool setchan)
{
    if (setchan && !IsOpen())
        Open(this);

    return ChannelBase::Init(startchannel, setchan);
}

/** \fn DVBChannel::CheckFrequency(uint64_t) const
 *  \brief Checks tuning frequency
 */
void DVBChannel::CheckFrequency(uint64_t frequency) const
{
    if (m_frequencyMinimum && m_frequencyMaximum &&
        (m_frequencyMinimum <= m_frequencyMaximum) &&
        (frequency < m_frequencyMinimum || frequency > m_frequencyMaximum))
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC +
            QString("Frequency setting (%1) is out of range (min/max:%2/%3)")
            .arg(frequency).arg(m_frequencyMinimum).arg(m_frequencyMaximum));
    }
}

void DVBChannel::CheckOptions(DTVMultiplex &tuning) const
{
    if ((tuning.m_inversion == DTVInversion::kInversionAuto) &&
        ((m_capabilities & FE_CAN_INVERSION_AUTO) == 0U))
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC +
            "'Auto' inversion parameter unsupported by this driver, "
            "falling back to 'off'.");
        tuning.m_inversion = DTVInversion::kInversionOff;
    }

    // DVB-S/S2 needs a fully initialized DiSEqC tree and is checked later in Tune
    if (!m_diseqcTree)
    {
        DVBChannel *master = GetMasterLock();
        if (master == nullptr || !master->m_diseqcTree)
            CheckFrequency(tuning.m_frequency);
        ReturnMasterLock(master);
    }

    if (m_tunerType.IsFECVariable() &&
        m_symbolRateMinimum && m_symbolRateMaximum &&
        (m_symbolRateMinimum <= m_symbolRateMaximum) &&
        (tuning.m_symbolRate < m_symbolRateMinimum ||
         tuning.m_symbolRate > m_symbolRateMaximum))
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC +
            QString("Symbol Rate setting (%1) is out of range (min/max:%2/%3)")
                .arg(tuning.m_symbolRate)
                .arg(m_symbolRateMinimum).arg(m_symbolRateMaximum));
    }

    if (m_tunerType.IsFECVariable() && !CheckCodeRate(tuning.m_fec))
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC +
            "Selected fec_inner parameter unsupported by this driver.");
    }

    if (m_tunerType.IsModulationVariable() && !CheckModulation(tuning.m_modulation))
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC +
           "Selected modulation parameter unsupported by this driver.");
    }

    if (DTVTunerType::kTunerTypeDVBT  != m_tunerType &&
        DTVTunerType::kTunerTypeDVBT2 != m_tunerType)
    {
        LOG(VB_CHANNEL, LOG_DEBUG, LOC + tuning.toString());
        return;
    }

    // ------ Below only DVB-T/T2 tuning parameters ------

    // Check OFDM Tuning params

    if (!CheckCodeRate(tuning.m_hpCodeRate))
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC +
            "Selected code_rate_hp parameter unsupported by this driver.");
    }

    if (!CheckCodeRate(tuning.m_lpCodeRate))
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC +
            "Selected code_rate_lp parameter unsupported by this driver.");
    }

    if ((tuning.m_bandwidth == DTVBandwidth::kBandwidthAuto) &&
        ((m_capabilities & FE_CAN_BANDWIDTH_AUTO) == 0U))
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC +
            "'Auto' bandwidth parameter unsupported by this driver.");
    }

    if ((tuning.m_transMode == DTVTransmitMode::kTransmissionModeAuto) &&
        ((m_capabilities & FE_CAN_TRANSMISSION_MODE_AUTO) == 0U))
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC +
            "'Auto' transmission_mode parameter unsupported by this driver.");
    }

    if ((tuning.m_guardInterval == DTVGuardInterval::kGuardIntervalAuto) &&
        ((m_capabilities & FE_CAN_GUARD_INTERVAL_AUTO) == 0U))
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC +
            "'Auto' guard_interval parameter unsupported by this driver.");
    }

    if ((tuning.m_hierarchy == DTVHierarchy::kHierarchyAuto) &&
        ((m_capabilities & FE_CAN_HIERARCHY_AUTO) == 0U))
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC +
            "'Auto' hierarchy parameter unsupported by this driver. ");
    }

    if (!CheckModulation(tuning.m_modulation))
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC +
            "Selected modulation parameter unsupported by this driver.");
    }

    LOG(VB_CHANNEL, LOG_DEBUG, LOC + tuning.toString());
}

/**
 *  \brief Return true iff rate is supported rate on the frontend
 */
bool DVBChannel::CheckCodeRate(DTVCodeRate rate) const
{
    const uint64_t caps = m_capabilities;
    return
        ((DTVCodeRate::kFECNone == rate))                            ||
        ((DTVCodeRate::kFEC_1_2 == rate) && ((caps & FE_CAN_FEC_1_2) != 0U)) ||
        ((DTVCodeRate::kFEC_2_3 == rate) && ((caps & FE_CAN_FEC_2_3) != 0U)) ||
        ((DTVCodeRate::kFEC_3_4 == rate) && ((caps & FE_CAN_FEC_3_4) != 0U)) ||
        ((DTVCodeRate::kFEC_4_5 == rate) && ((caps & FE_CAN_FEC_4_5) != 0U)) ||
        ((DTVCodeRate::kFEC_5_6 == rate) && ((caps & FE_CAN_FEC_5_6) != 0U)) ||
        ((DTVCodeRate::kFEC_6_7 == rate) && ((caps & FE_CAN_FEC_6_7) != 0U)) ||
        ((DTVCodeRate::kFEC_7_8 == rate) && ((caps & FE_CAN_FEC_7_8) != 0U)) ||
        ((DTVCodeRate::kFEC_8_9 == rate) && ((caps & FE_CAN_FEC_8_9) != 0U)) ||
        ((DTVCodeRate::kFECAuto == rate) && ((caps & FE_CAN_FEC_AUTO) != 0U));
}

/**
 *  \brief Return true iff modulation is supported modulation on the frontend
 */
bool DVBChannel::CheckModulation(DTVModulation modulation) const
{
    const DTVModulation m = modulation;
    const uint64_t      c = m_capabilities;

    return
        ((DTVModulation::kModulationQPSK    == m) && ((c & FE_CAN_QPSK) != 0U))     ||
#if HAVE_FE_CAN_2G_MODULATION
        ((DTVModulation::kModulation8PSK    == m) && ((c & FE_CAN_2G_MODULATION) != 0U)) ||
        ((DTVModulation::kModulation16APSK  == m) && ((c & FE_CAN_2G_MODULATION) != 0U)) ||
        ((DTVModulation::kModulation32APSK  == m) && ((c & FE_CAN_2G_MODULATION) != 0U)) ||
#endif //HAVE_FE_CAN_2G_MODULATION
        ((DTVModulation::kModulationQAM16   == m) && ((c & FE_CAN_QAM_16) != 0U))   ||
        ((DTVModulation::kModulationQAM32   == m) && ((c & FE_CAN_QAM_32) != 0U))   ||
        ((DTVModulation::kModulationQAM64   == m) && ((c & FE_CAN_QAM_64) != 0U))   ||
        ((DTVModulation::kModulationQAM128  == m) && ((c & FE_CAN_QAM_128) != 0U))  ||
        ((DTVModulation::kModulationQAM256  == m) && ((c & FE_CAN_QAM_256) != 0U))  ||
        ((DTVModulation::kModulationQAMAuto == m) && ((c & FE_CAN_QAM_AUTO) != 0U)) ||
        ((DTVModulation::kModulation8VSB    == m) && ((c & FE_CAN_8VSB) != 0U))     ||
        ((DTVModulation::kModulation16VSB   == m) && ((c & FE_CAN_16VSB) != 0U));
}

/** \fn DVBChannel::SetPMT(const ProgramMapTable*)
 *  \brief Tells the Conditional Access Module which streams we wish to decode.
 */
void DVBChannel::SetPMT(const ProgramMapTable *pmt)
{
    if (!m_dvbCam->IsRunning())
        m_dvbCam->Start();
    if (pmt && m_dvbCam->IsRunning())
        m_dvbCam->SetPMT(this, pmt);
}

/** \fn DVBChannel::SetTimeOffset(double)
 *  \brief Tells the Conditional Access Module the offset
 *         from the computers utc time to dvb time.
 */
void DVBChannel::SetTimeOffset(double offset)
{
    if (m_dvbCam->IsRunning())
        m_dvbCam->SetTimeOffset(offset);
}

bool DVBChannel::Tune(const DTVMultiplex &tuning)
{
    if (!m_inputId)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + QString("Tune(): Invalid input."));
        return false;
    }
    return Tune(tuning, false, false);
}


/*****************************************************************************
           Tuning functions for each of the five types of cards.
 *****************************************************************************/

/**
 *  \brief Tunes the card to a frequency but does not deal with PIDs.
 *
 *   This is used by DVB Channel Scanner, the EIT Parser, and by TVRec.
 *
 *  \param tuning      Info on transport to tune to
 *  \param force_reset If true, frequency tuning is done
 *                     even if it should not be needed.
 *  \param same_input  Optional, doesn't change input (for retuning).
 *  \return true on success, false on failure
 */
bool DVBChannel::Tune(const DTVMultiplex &tuning,
                      bool force_reset,
                      bool same_input)
{
    QMutexLocker lock(&m_tuneLock);
    QMutexLocker locker(&m_hwLock);

    DVBChannel *master = GetMasterLock();
    if (master != this)
    {
        LOG(VB_CHANNEL, LOG_INFO, LOC + "Tuning on slave channel");
        SetSIStandard(tuning.m_sistandard);
        bool ok = master->Tune(tuning, force_reset, false);
        ReturnMasterLock(master);
        return ok;
    }
    ReturnMasterLock(master); // If we're the master we don't need this lock.


    uint intermediate_freq = 0;
    bool can_fec_auto = false;
    bool reset = (force_reset || m_firstTune);

    if (m_tunerType.IsDiSEqCSupported() && !m_diseqcTree)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            "DVB-S/S2 needs device tree for LNB handling");
        return false;
    }

    m_desiredTuning = tuning;

    if (m_fdFrontend < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Tune(): Card not open!");

        return false;
    }

    // Remove all events in queue before tuning.
    DrainDVBEvents();

    LOG(VB_CHANNEL, LOG_INFO, LOC + "Tuning parameters:");
    LOG(VB_CHANNEL, LOG_INFO, "    Old: " + m_prevTuning.toString());
    LOG(VB_CHANNEL, LOG_INFO, "    New: " + tuning.toString());

    // DVB-S/S2 is in kHz, other DVB is in Hz
    bool is_dvbs = ((DTVTunerType::kTunerTypeDVBS1 == m_tunerType) ||
                    (DTVTunerType::kTunerTypeDVBS2 == m_tunerType));
    int     freq_mult = (is_dvbs) ? 1 : 1000;
    QString suffix    = (is_dvbs) ? "kHz" : "Hz";

    if (reset || !m_prevTuning.IsEqual(m_tunerType, tuning, 500 * freq_mult))
    {
        LOG(VB_CHANNEL, LOG_INFO, LOC + QString("Tune(): Tuning to %1%2")
                .arg(intermediate_freq ? intermediate_freq : tuning.m_frequency)
                .arg(suffix));

        m_tuneDelayLock.lock();

        std::chrono::milliseconds this_tuning = MythDate::currentMSecsSinceEpochAsDuration();
        std::chrono::milliseconds tuning_delay = s_lastTuning + concurrent_tunings_delay - this_tuning;
        if (tuning_delay > 0ms)
        {
            LOG(VB_GENERAL, LOG_INFO, LOC + QString("Next tuning after less than %1ms, delaying by %2ms")
                .arg(concurrent_tunings_delay.count()).arg(tuning_delay.count()));
            std::this_thread::sleep_for(tuning_delay);
        }
        s_lastTuning = MythDate::currentMSecsSinceEpochAsDuration();

        m_tuneDelayLock.unlock();

        // For DVB-S/S2 configure DiSEqC and determine intermediate frequency
        if (m_diseqcTree)
        {
            // Configure for new input
            if (!same_input)
                m_diseqcSettings.Load(m_inputId);

            // Execute DiSEqC commands
            if (!m_diseqcTree->Execute(m_diseqcSettings, tuning))
            {
                LOG(VB_GENERAL, LOG_ERR, LOC +
                    "Tune(): Failed to setup DiSEqC devices");
                return false;
            }

            // Retrieve actual intermediate frequency
            DiSEqCDevLNB *lnb = m_diseqcTree->FindLNB(m_diseqcSettings);
            if (!lnb)
            {
                LOG(VB_GENERAL, LOG_ERR, LOC +
                    "Tune(): No LNB for this configuration");
                return false;
            }

            if (lnb->GetDeviceID() != m_lastLnbDevId)
            {
                m_lastLnbDevId = lnb->GetDeviceID();
                // Make sure we tune to frequency, if the lnb has changed
                m_firstTune = true;
            }

            intermediate_freq = lnb->GetIntermediateFrequency(
                m_diseqcSettings, tuning);

            // Retrieve SCR intermediate frequency
            DiSEqCDevSCR *scr = m_diseqcTree->FindSCR(m_diseqcSettings);
            if (lnb && scr)
            {
                intermediate_freq = scr->GetIntermediateFrequency(intermediate_freq);
            }

            // If card can auto-FEC, use it -- sometimes NITs are inaccurate
            if (m_capabilities & FE_CAN_FEC_AUTO)
                can_fec_auto = true;

            // Check DVB-S intermediate frequency here since it requires a fully
            // initialized diseqc tree
            CheckFrequency(intermediate_freq);
        }

        // DVBv5 or legacy DVBv3 API
        if (!m_legacyFe)
        {
            // DVBv5 API
            struct dtv_property p_clear = {};
            struct dtv_properties cmdseq_clear = {};

            p_clear.cmd        = DTV_CLEAR;
            cmdseq_clear.num   = 1;
            cmdseq_clear.props = &p_clear;

            if ((ioctl(m_fdFrontend, FE_SET_PROPERTY, &cmdseq_clear)) < 0)
            {
                LOG(VB_GENERAL, LOG_ERR, LOC +
                    "Tune(): Clearing DTV properties cache failed." + ENO);
                return false;
            }

            struct dtv_properties *cmds = dtvmultiplex_to_dtvproperties(m_inputId,
                m_tunerType, m_currentSys, tuning, intermediate_freq, can_fec_auto);

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

            int res = ioctl(m_fdFrontend, FE_SET_PROPERTY, cmds);

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
        {
            // Legacy DVBv3 API
            struct dvb_frontend_parameters params = dtvmultiplex_to_dvbparams(
                m_tunerType, tuning, intermediate_freq, can_fec_auto);

            if (ioctl(m_fdFrontend, FE_SET_FRONTEND, &params) < 0)
            {
                LOG(VB_GENERAL, LOG_ERR, LOC +
                    "Tune(): Setting Frontend tuning parameters failed." + ENO);
                return false;
            }
        }

        // Extra delay to add for broken DVB drivers
        if (m_tuningDelay > 0ms)
            std::this_thread::sleep_for(m_tuningDelay);

        WaitForBackend(50ms);

        m_prevTuning = tuning;
        m_firstTune = false;
    }
    else
    {
        LOG(VB_CHANNEL, LOG_INFO, LOC + QString("Tune(): Tuning to %1%2 skipped, already tuned")
            .arg(intermediate_freq ? intermediate_freq : tuning.m_frequency)
            .arg(suffix));
    }


    SetSIStandard(tuning.m_sistandard);

    LOG(VB_CHANNEL, LOG_INFO, LOC + "Tune(): Frequency tuning successful");

    return true;
}

bool DVBChannel::Retune(void)
{
    return Tune(m_desiredTuning, true, true);
}

/** \fn DVBChannel::IsTuningParamsProbeSupported(void) const
 *  \brief Returns true iff tuning info probing is working.
 */
bool DVBChannel::IsTuningParamsProbeSupported(void) const
{
    QMutexLocker locker(&m_hwLock);

    if (m_fdFrontend < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Card not open!");

        return false;
    }

    DVBChannel *master = GetMasterLock();
    if (master != this)
    {
        bool ok = master->IsTuningParamsProbeSupported();
        ReturnMasterLock(master);
        return ok;
    }
    ReturnMasterLock(master); // If we're the master we don't need this lock.

    if (m_diseqcTree)
    {
        // TODO We need to implement the inverse of
        // lnb->GetIntermediateFrequency() for ProbeTuningParams()
        // to accurately reflect the frequency before LNB transform.
        return false;
    }

    dvb_frontend_parameters params {};

    int res = ioctl(m_fdFrontend, FE_GET_FRONTEND, &params);
    if (res < 0)
    {
        LOG(VB_CHANNEL, LOG_ERR, LOC + "FE_GET_FRONTEND failed." + ENO);
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
    QMutexLocker locker(&m_hwLock);

    if (m_fdFrontend < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Card not open!");

        return false;
    }

    DVBChannel *master = GetMasterLock();
    if (master != this)
    {
        bool ok = master->ProbeTuningParams(tuning);
        ReturnMasterLock(master);
        return ok;
    }
    ReturnMasterLock(master); // If we're the master we don't need this lock.

    if (m_diseqcTree)
    {
        // TODO We need to implement the inverse of
        // lnb->GetIntermediateFrequency() for ProbeTuningParams()
        // to accurately reflect the frequency before LNB transform.
        return false;
    }

    // DVBv5 API
    if (!m_legacyFe)
    {
        // TODO implement probing of tuning parameters with FE_GET_PROPERTY
        return false;
    }

    // Legacy DVBv3 API
    dvb_frontend_parameters params {};
    if (ioctl(m_fdFrontend, FE_GET_FRONTEND, &params) < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            "Getting Frontend tuning parameters failed." + ENO);

        return false;
    }

    uint    mplex      = tuning.m_mplex;
    QString sistandard = tuning.m_sistandard;

    tuning = dvbparams_to_dtvmultiplex(m_tunerType, params);

    tuning.m_mplex       = mplex;
    tuning.m_sistandard  = sistandard;

    return true;
}

/** \fn DVBChannel::GetChanID() const
 *  \brief Returns Channel ID
 *  \bug This only works if there is only one input on the card.
 */
int DVBChannel::GetChanID() const
{
    QList<int> idlist;
    int id = -1;

    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("SELECT chanid, visible "
                  "FROM channel, capturecard "
                  "WHERE channel.deleted IS NULL AND "
                  "      capturecard.sourceid = channel.sourceid AND "
                  "      channel.channum = :CHANNUM AND "
                  "      capturecard.cardid = :INPUTID");

    query.bindValue(":CHANNUM", m_curChannelName);
    query.bindValue(":INPUTID", m_inputId);

    if (!query.exec() || !query.isActive())
    {
        MythDB::DBError("fetching chanid", query);
        return -1;
    }

    while (query.next())
    {
        bool visible = query.value(1).toInt() > 0;
        if (visible)
        {
            int chanid = query.value(0).toInt();
            idlist.append(chanid);
        }
    }

    if (idlist.isEmpty())
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC +
            QString("No visible channel ID for %1")
            .arg(m_curChannelName));
    }
    else
    {
        id = idlist.value(0);
        if (idlist.count() > 1)
        {
            QStringList sl;
            for (auto chanid : idlist)
            {
                sl.append(QString::number(chanid));
            }
            LOG(VB_CHANNEL, LOG_DEBUG, LOC +
                QString("Found for '%1' multiple visible channel IDs: %2")
                .arg(m_curChannelName, sl.join(" ")));
        }
        else
        {
            LOG(VB_CHANNEL, LOG_DEBUG, LOC +
                QString("Found visible channel ID %1 for '%2'")
                .arg(id).arg(m_curChannelName));
        }
    }

    return id;
}

const DiSEqCDevRotor *DVBChannel::GetRotor(void) const
{
    if (m_diseqcTree)
        return m_diseqcTree->FindRotor(m_diseqcSettings);

    return nullptr;
}

bool DVBChannel::HasLock(bool *ok) const
{
    DVBChannel *master = GetMasterLock();
    if (master != this)
    {
        bool haslock = master->HasLock(ok);
        ReturnMasterLock(master);
        return haslock;
    }
    ReturnMasterLock(master); // If we're the master we don't need this lock.

#if ((DVB_API_VERSION > 5) || ((DVB_API_VERSION == 5) && (DVB_API_VERSION_MINOR > 10)))
    fe_status_t status = FE_NONE;
#else // debian9, centos7
    fe_status_t status = (fe_status_t)0;
#endif

    int ret = ioctl(m_fdFrontend, FE_READ_STATUS, &status);
    if (ret < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "FE_READ_STATUS failed" + ENO);
    }
    else
    {
        LOG(VB_CHANNEL, LOG_DEBUG, LOC + QString("%1 status: 0x%2 %3")
                .arg(__func__).arg(status,2,16,QChar('0')).arg(toString(status)));
    }

    if (ok)
        *ok = (0 == ret);

    return (status & FE_HAS_LOCK) != 0;
}

double DVBChannel::GetSignalStrengthDVBv5(bool *ok) const
{
    struct dtv_property prop = {};
    struct dtv_properties cmd = {};

    prop.cmd = DTV_STAT_SIGNAL_STRENGTH;
    cmd.num = 1;
    cmd.props = &prop;
    int ret = ioctl(m_fdFrontend, FE_GET_PROPERTY, &cmd);
    if (ret < 0)
    {
        LOG(VB_CHANNEL, LOG_DEBUG, LOC + "FE_GET_PROPERTY DTV_STAT_SIGNAL_STRENGTH failed" + ENO);
    }
    else
    {
#if DTV_STAT_FULL_DEBUG
        LOG(VB_CHANNEL, LOG_DEBUG, LOC + "DTV_STAT_SIGNAL_STRENGTH " +
            QString("res=%1 len=%2 scale=%3 val=%4")
            .arg(cmd.props->result)
            .arg(cmd.props->u.st.len)
            .arg(cmd.props->u.st.stat[0].scale)
            .arg(cmd.props->u.st.stat[0].svalue));
#endif
    }

    bool tmpOk = (ret == 0) && (cmd.props->u.st.len > 0);
    if (ok)
        *ok = tmpOk;

    double value = 0;
    if (tmpOk)
    {
        if (cmd.props->u.st.stat[0].scale == FE_SCALE_DECIBEL)
        {
            // Convert range of -100dBm .. 0dBm to 0% .. 100%
            // Note that -100dBm is lower than the noise floor.
            // The measured svalue is in units of 0.001 dBm.
            // If the value is outside the range -100 to 0 dBm we do not believe it.
            int64_t svalue = cmd.props->u.st.stat[0].svalue;
            if (svalue >= -100000 && svalue <= 0)
            {
                // convert value from -100dBm to 0dBm to a 0-1 range
                value = svalue + 100000;
                value = value / 100000.0;
                value = std::min(value, 1.0);
            }
        }
        else if (cmd.props->u.st.stat[0].scale == FE_SCALE_RELATIVE)
        {
            // Return as 16 bit unsigned
            value = cmd.props->u.st.stat[0].uvalue / 65535.0;
        }
    }
    return value;
}

double DVBChannel::GetSignalStrength(bool *ok) const
{
    DVBChannel *master = GetMasterLock();
    if (master != this)
    {
        double val = master->GetSignalStrength(ok);
        ReturnMasterLock(master);
        return val;
    }
    ReturnMasterLock(master); // If we're the master we don't need this lock.

    if (!m_legacyFe && m_hasV5Stats)
    {
        bool v5_ok = false;
        double value = GetSignalStrengthDVBv5(&v5_ok);
        if (v5_ok)
        {
            if (ok)
                *ok = v5_ok;
            return value;
        }
    }

    // We use uint16_t for sig because this is correct for DVB API 4.0,
    // and works better than the correct int16_t for the 3.x API
    uint16_t sig = 0;
    int ret = ioctl(m_fdFrontend, FE_READ_SIGNAL_STRENGTH, &sig);
    if (ok)
        *ok = (0 == ret);

    return sig * (1.0 / 65535.0);
}

double DVBChannel::GetSNRDVBv5(bool *ok) const
{
    struct dtv_property prop = {};
    struct dtv_properties cmd = {};

    prop.cmd = DTV_STAT_CNR;
    cmd.num = 1;
    cmd.props = &prop;
    int ret = ioctl(m_fdFrontend, FE_GET_PROPERTY, &cmd);
    if (ret < 0)
    {
        LOG(VB_CHANNEL, LOG_DEBUG, LOC + "FE_GET_PROPERTY DTV_STAT_CNR failed" + ENO);
    }
    else
    {
#if DTV_STAT_FULL_DEBUG
        LOG(VB_CHANNEL, LOG_DEBUG, LOC + "DTV_STAT_CNR " +
            QString("res=%1 len=%2 scale=%3 val=%4")
            .arg(cmd.props->result)
            .arg(cmd.props->u.st.len)
            .arg(cmd.props->u.st.stat[0].scale)
            .arg(cmd.props->u.st.stat[0].svalue));
#endif
    }

    bool tmpOk = (ret == 0) && (cmd.props->u.st.len > 0);
    if (ok)
        *ok = tmpOk;
    double value = 0;
    if (tmpOk)
    {
        if (cmd.props->u.st.stat[0].scale == FE_SCALE_DECIBEL)
        {
            // The measured svalue is in units of 0.001 dB
            // Let 50dB+ CNR be 100% quality and 0dB be 0%
            // Convert 0.001 dB from 0-50000 to a 0-1 range
            value = cmd.props->u.st.stat[0].svalue;
            value = value / 50000.0;
            if (value > 1.0)
                value = 1.0;
            else if (value < 0)
                value = 0.0;
        }
        else if (cmd.props->u.st.stat[0].scale == FE_SCALE_RELATIVE)
        {
            // Return as 16 bit unsigned
            value = cmd.props->u.st.stat[0].uvalue / 65535.0;
        }
    }
    return value;
}

double DVBChannel::GetSNR(bool *ok) const
{
    DVBChannel *master = GetMasterLock();
    if (master != this)
    {
        double val = master->GetSNR(ok);
        ReturnMasterLock(master);
        return val;
    }
    ReturnMasterLock(master); // If we're the master we don't need this lock.

    if (!m_legacyFe && m_hasV5Stats)
    {
        bool v5_ok = false;
        double value = GetSNRDVBv5(&v5_ok);
        if (v5_ok)
        {
            if (ok)
                *ok = v5_ok;
            return value;
        }
    }

    // We use uint16_t for sig because this is correct for DVB API 4.0,
    // and works better than the correct int16_t for the 3.x API
    uint16_t snr = 0;
    int ret = ioctl(m_fdFrontend, FE_READ_SNR, &snr);
    if (ok)
        *ok = (0 == ret);

    return snr * (1.0 / 65535.0);
}

double DVBChannel::GetBitErrorRateDVBv5(bool *ok) const
{
    std::array<struct dtv_property,2> prop {};
    struct dtv_properties cmd = {};

    prop[0].cmd = DTV_STAT_POST_ERROR_BIT_COUNT;
    prop[1].cmd = DTV_STAT_POST_TOTAL_BIT_COUNT;
    cmd.num   = prop.size();
    cmd.props = prop.data();
    int ret = ioctl(m_fdFrontend, FE_GET_PROPERTY, &cmd);
    if (ret < 0)
    {
        LOG(VB_CHANNEL, LOG_DEBUG, LOC + "FE_GET_PROPERTY DTV_STAT_POST_ERROR_BIT_COUNT failed" + ENO);
    }
    else
    {
#if DTV_STAT_FULL_DEBUG
        LOG(VB_CHANNEL, LOG_DEBUG, LOC + "DTV_STAT_POST_ERROR_BIT_COUNT " +
            QString("res=%1 len=%2 scale=%3 val=%4 res=%5 len=%6 scale=%7 val=%8")
            .arg(cmd.props[0].result)
            .arg(cmd.props[0].u.st.len)
            .arg(cmd.props[0].u.st.stat[0].scale)
            .arg(cmd.props[0].u.st.stat[0].uvalue)
            .arg(cmd.props[1].result)
            .arg(cmd.props[1].u.st.len)
            .arg(cmd.props[1].u.st.stat[0].scale)
            .arg(cmd.props[1].u.st.stat[0].uvalue));
#endif
    }

    bool tmpOk = (ret == 0) &&
            (cmd.props[0].u.st.len > 0) &&
            (cmd.props[1].u.st.len > 0);
    if (ok)
        *ok = tmpOk;
    double value = 0;
    if (tmpOk)
    {
        if ((cmd.props[0].u.st.stat[0].scale == FE_SCALE_COUNTER) &&
            (cmd.props[1].u.st.stat[0].scale == FE_SCALE_COUNTER) &&
            (cmd.props[1].u.st.stat[0].uvalue != 0))
        {
            value = static_cast<double>(
                    static_cast<long double>(cmd.props[0].u.st.stat[0].uvalue) /
                    cmd.props[1].u.st.stat[0].uvalue);
        }
    }
    return value;
}

double DVBChannel::GetBitErrorRate(bool *ok) const
{
    DVBChannel *master = GetMasterLock();
    if (master != this)
    {
        double val = master->GetBitErrorRate(ok);
        ReturnMasterLock(master);
        return val;
    }
    ReturnMasterLock(master); // If we're the master we don't need this lock.

    if (!m_legacyFe && m_hasV5Stats)
    {
        bool v5_ok = false;
        double value = GetBitErrorRateDVBv5(&v5_ok);
        if (v5_ok)
        {
            if (ok)
                *ok = v5_ok;
            return value;
        }
    }

    uint32_t ber = 0;
    int ret = ioctl(m_fdFrontend, FE_READ_BER, &ber);
    if (ok)
        *ok = (0 == ret);

    return (double) ber;
}

double DVBChannel::GetUncorrectedBlockCountDVBv5(bool *ok) const
{
    struct dtv_property prop = {};
    struct dtv_properties cmd = {};

    prop.cmd = DTV_STAT_ERROR_BLOCK_COUNT;
    cmd.num = 1;
    cmd.props = &prop;
    int ret = ioctl(m_fdFrontend, FE_GET_PROPERTY, &cmd);
    if (ret < 0)
    {
        LOG(VB_CHANNEL, LOG_DEBUG, LOC + "FE_GET_PROPERTY DTV_STAT_ERROR_BLOCK_COUNT failed" + ENO);
    }
    else
    {
#if DTV_STAT_FULL_DEBUG
        LOG(VB_CHANNEL, LOG_DEBUG, LOC + "DTV_STAT_ERROR_BLOCK_COUNT " +
            QString("res=%1 len=%2 scale=%3 val=%4")
            .arg(cmd.props[0].result)
            .arg(cmd.props[0].u.st.len)
            .arg(cmd.props[0].u.st.stat[0].scale)
            .arg(cmd.props[0].u.st.stat[0].svalue));
#endif
    }

    bool tmpOk = (ret == 0) && (cmd.props->u.st.len > 0);
    if (ok)
        *ok = tmpOk;
    double value = 0;
    if (tmpOk)
    {
        if (cmd.props->u.st.stat[0].scale == FE_SCALE_COUNTER)
            value = cmd.props->u.st.stat[0].uvalue;
    }
    return value;
}

double DVBChannel::GetUncorrectedBlockCount(bool *ok) const
{
    DVBChannel *master = GetMasterLock();
    if (master != this)
    {
        double val = master->GetUncorrectedBlockCount(ok);
        ReturnMasterLock(master);
        return val;
    }
    ReturnMasterLock(master); // If we're the master we don't need this lock.

    if (!m_legacyFe && m_hasV5Stats)
    {
        bool v5_ok = false;
        double value = GetUncorrectedBlockCountDVBv5(&v5_ok);
        if (v5_ok)
        {
            if (ok)
                *ok = v5_ok;
            return value;
        }
    }

    uint32_t ublocks = 0;
    int ret = ioctl(m_fdFrontend, FE_READ_UNCORRECTED_BLOCKS, &ublocks);
    if (ok)
        *ok = (0 == ret);

    return static_cast<double>(ublocks);
}

void DVBChannel::ReturnMasterLock(DVBChannel* &dvbm)
{
    DTVChannel *chan = dvbm;
    DTVChannel::ReturnMasterLock(chan);
    dvbm = nullptr;
}

DVBChannel *DVBChannel::GetMasterLock(void) const
{
    DTVChannel *master = DTVChannel::GetMasterLock(m_key);
    auto *dvbm = dynamic_cast<DVBChannel*>(master);
    if (master && !dvbm)
        DTVChannel::ReturnMasterLock(master);
    return dvbm;
}

bool DVBChannel::IsMaster(void) const
{
    DVBChannel *master = GetMasterLock();
    bool is_master = (master == this);
    ReturnMasterLock(master);
    return is_master;
}

// Reads all the events off the queue, so we can use select in WaitForBackend.
//
// Note that FE_GET_EVENT is deprecated but there is no alternative yet.
//
void DVBChannel::DrainDVBEvents(void)
{
    const int fd  = m_fdFrontend;
    struct dvb_frontend_event event {};
    int ret = 0;
    while ((ret = ioctl(fd, FE_GET_EVENT, &event)) == 0);
    if ((ret < 0) && (EAGAIN != errno) && (EWOULDBLOCK != errno) && (EOVERFLOW != errno))
    {
        LOG(VB_CHANNEL, LOG_ERR, LOC +
            QString("%1 FE_GET_EVENT failed: ").arg(__func__) + logStrerror(errno));
    }
}

/**
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
 *  \param timeout_ms timeout before FE_READ_STATUS in milliseconds
 */
bool DVBChannel::WaitForBackend(std::chrono::milliseconds timeout_ms)
{
    const int fd  = m_fdFrontend;
    auto seconds = duration_cast<std::chrono::seconds>(timeout_ms);
    auto usecs = duration_cast<std::chrono::microseconds>(timeout_ms) - seconds;
    struct timeval select_timeout = {
         static_cast<typeof(select_timeout.tv_sec)>(seconds.count()),
         static_cast<typeof(select_timeout.tv_usec)>(usecs.count())};
    fd_set fd_select_set;
    FD_ZERO(    &fd_select_set); // NOLINT(readability-isolate-declaration)
    FD_SET (fd, &fd_select_set);

    // Try to wait for some output like an event, unfortunately
    // this fails on several DVB cards, so we have a timeout.
    int ret = 0;
    do ret = select(fd+1, &fd_select_set, nullptr, nullptr, &select_timeout);
    while ((-1 == ret) && (EINTR == errno));

    if (-1 == ret)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("%1: Failed to wait on output.").arg(__func__) + ENO);

        return false;
    }

    // This is supposed to work on all cards, post 2.6.12...
#if ((DVB_API_VERSION > 5) || ((DVB_API_VERSION == 5) && (DVB_API_VERSION_MINOR > 10)))
    fe_status_t status = FE_NONE;
#else // debian9, centos7
    fe_status_t status = (fe_status_t)0;
#endif

    if (ioctl(fd, FE_READ_STATUS, &status) < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("%1: FE_READ_STATUS failed.").arg(__func__) + ENO);

        return false;
    }

    LOG(VB_CHANNEL, LOG_DEBUG, LOC +
        QString("%1: status: 0x%2 %3") .arg(__func__)
            .arg(status,2,16,QChar('0')).arg(toString(status)));

    return true;
}

// Create legacy DVBv3 frontend parameters from DTVMultiplex.
//
static struct dvb_frontend_parameters dtvmultiplex_to_dvbparams(
    DTVTunerType tuner_type, const DTVMultiplex &tuning,
    uint intermediate_freq, bool can_fec_auto)
{
    dvb_frontend_parameters params {};

    params.frequency = tuning.m_frequency;
    params.inversion = (fe_spectral_inversion_t) (int) tuning.m_inversion;

    if (DTVTunerType::kTunerTypeDVBS1 == tuner_type)
    {
        if (tuning.m_modSys == DTVModulationSystem::kModulationSystem_DVBS2)
        {
            LOG(VB_GENERAL, LOG_ERR,
                "DVBChan: Cannot tune to DVB-S2 transport with DVB-S card.");
        }

        params.frequency          = intermediate_freq;
        params.u.qpsk.symbol_rate = tuning.m_symbolRate;
        params.u.qpsk.fec_inner   = can_fec_auto ? FEC_AUTO
            : (fe_code_rate_t) (int) tuning.m_fec;
    }

    if (DTVTunerType::kTunerTypeDVBS2 == tuner_type)
    {
        LOG(VB_GENERAL, LOG_ERR,
            "DVBChan: Cannot tune DVB-S2 card with DVBv3 API.");
    }

    if (DTVTunerType::kTunerTypeDVBC == tuner_type)
    {
        params.u.qam.symbol_rate  = tuning.m_symbolRate;
        params.u.qam.fec_inner    = (fe_code_rate_t) (int) tuning.m_fec;
        params.u.qam.modulation   = (fe_modulation_t) (int) tuning.m_modulation;
    }

    if (DTVTunerType::kTunerTypeDVBT  == tuner_type ||
        DTVTunerType::kTunerTypeDVBT2 == tuner_type)
    {
        params.u.ofdm.bandwidth             = (fe_bandwidth_t) (int) tuning.m_bandwidth;
        params.u.ofdm.code_rate_HP          = (fe_code_rate_t) (int) tuning.m_hpCodeRate;
        params.u.ofdm.code_rate_LP          = (fe_code_rate_t) (int) tuning.m_lpCodeRate;
        params.u.ofdm.constellation         = (fe_modulation_t) (int) tuning.m_modulation;
        params.u.ofdm.transmission_mode     = (fe_transmit_mode_t) (int) tuning.m_transMode;
        params.u.ofdm.guard_interval        = (fe_guard_interval_t) (int) tuning.m_guardInterval;
        params.u.ofdm.hierarchy_information = (fe_hierarchy_t) (int) tuning.m_hierarchy;
    }

    if (DTVTunerType::kTunerTypeATSC == tuner_type)
    {
        params.u.vsb.modulation = (fe_modulation_t) (int) tuning.m_modulation;
    }

    return params;
}

// Create DTVMultiplex from legacy DVBv3 frontend parameters.
//
static DTVMultiplex dvbparams_to_dtvmultiplex(
    DTVTunerType tuner_type, const dvb_frontend_parameters &params)
{
    DTVMultiplex tuning;

    tuning.m_frequency = params.frequency;
    tuning.m_inversion = params.inversion;

    if ((DTVTunerType::kTunerTypeDVBS1 == tuner_type) ||
        (DTVTunerType::kTunerTypeDVBS2 == tuner_type))
    {
        tuning.m_symbolRate     = params.u.qpsk.symbol_rate;
        tuning.m_fec            = params.u.qpsk.fec_inner;
    }

    if (DTVTunerType::kTunerTypeDVBC   == tuner_type)
    {
        tuning.m_symbolRate     = params.u.qam.symbol_rate;
        tuning.m_fec            = params.u.qam.fec_inner;
        tuning.m_modulation     = params.u.qam.modulation;
    }

    if (DTVTunerType::kTunerTypeDVBT   == tuner_type ||
        DTVTunerType::kTunerTypeDVBT2  == tuner_type)
    {
        tuning.m_bandwidth      = params.u.ofdm.bandwidth;
        tuning.m_hpCodeRate     = params.u.ofdm.code_rate_HP;
        tuning.m_lpCodeRate     = params.u.ofdm.code_rate_LP;
        tuning.m_modulation     = params.u.ofdm.constellation;
        tuning.m_transMode      = params.u.ofdm.transmission_mode;
        tuning.m_guardInterval  = params.u.ofdm.guard_interval;
        tuning.m_hierarchy      = params.u.ofdm.hierarchy_information;
    }

    if (DTVTunerType::kTunerTypeATSC   == tuner_type)
    {
        tuning.m_modulation     = params.u.vsb.modulation;
    }

    return tuning;
}

// Create a DVBv5 property list from the dtv_multiplex
//
static struct dtv_properties *dtvmultiplex_to_dtvproperties(uint inputId,
    DTVTunerType tuner_type, DTVModulationSystem current_sys, const DTVMultiplex &tuning,
    uint intermediate_freq, bool can_fec_auto, bool do_tune)
{
    DTVModulationSystem delivery_system = current_sys;
    uint c = 0;

    LOG(VB_CHANNEL, LOG_DEBUG, QString("DVBChan[%1]: m_modsys:%2  current_sys:%3")
        .arg(QString::number(inputId),
             tuning.m_modSys.toString(),
             current_sys.toString()));

    auto *cmdseq = (struct dtv_properties*) calloc(1, sizeof(struct dtv_properties));
    if (!cmdseq)
        return nullptr;

    cmdseq->props = (struct dtv_property*) calloc(20, sizeof(*(cmdseq->props)));
    if (!(cmdseq->props))
    {
        free(cmdseq);
        return nullptr;
    }

    // 20201117 TODO do this only for cx24116 but not for all DVB-S2 demods
    //
    // The cx24116 DVB-S2 demod announces FE_CAN_FEC_AUTO but has apparently
    // trouble with FEC_AUTO on DVB-S2 transponders
    if (tuning.m_modSys == DTVModulationSystem::kModulationSystem_DVBS2)
        can_fec_auto = false;

    // Select delivery system in this order
    // - from dtv_multiplex
    // - modulation system in the card
    // - default based on tuner type
    //
    if (tuning.m_modSys != DTVModulationSystem::kModulationSystem_UNDEFINED)
    {
        delivery_system = tuning.m_modSys;
        LOG(VB_CHANNEL, LOG_DEBUG, QString("DVBChan[%1]:%2 set delivery_system to %3")
            .arg(inputId).arg(__LINE__).arg(delivery_system.toString()));
    }
    else if (current_sys != DTVModulationSystem::kModulationSystem_UNDEFINED)
    {
        delivery_system = current_sys;
        LOG(VB_CHANNEL, LOG_DEBUG, QString("DVBChan[%1]:%2 set delivery_system to %3")
            .arg(inputId).arg(__LINE__).arg(delivery_system.toString()));
    }
    else if (tuner_type == DTVTunerType::kTunerTypeDVBC)
    {
        delivery_system = DTVModulationSystem::kModulationSystem_DVBC_ANNEX_A;
        LOG(VB_CHANNEL, LOG_DEBUG, QString("DVBChan[%1]:%2 set delivery_system to %3")
            .arg(inputId).arg(__LINE__).arg(delivery_system.toString()));
    }
    else if (tuner_type == DTVTunerType::kTunerTypeATSC)
    {
        delivery_system  = DTVModulationSystem::kModulationSystem_ATSC;
        LOG(VB_CHANNEL, LOG_DEBUG, QString("DVBChan[%1]:%2 set delivery_system to %3")
            .arg(inputId).arg(__LINE__).arg(delivery_system.toString()));
    }
    else if (tuner_type == DTVTunerType::kTunerTypeDVBT)
    {
        delivery_system  = DTVModulationSystem::kModulationSystem_DVBT;
        LOG(VB_CHANNEL, LOG_DEBUG, QString("DVBChan[%1]:%2 set delivery_system to %3")
            .arg(inputId).arg(__LINE__).arg(delivery_system.toString()));
    }
    else if (tuner_type == DTVTunerType::kTunerTypeDVBT2)
    {
        delivery_system  = DTVModulationSystem::kModulationSystem_DVBT2;
        LOG(VB_CHANNEL, LOG_DEBUG, QString("DVBChan[%1]:%2 set delivery_system to %3")
            .arg(inputId).arg(__LINE__).arg(delivery_system.toString()));
    }
    else if (tuner_type == DTVTunerType::kTunerTypeDVBS1)
    {
        delivery_system  = DTVModulationSystem::kModulationSystem_DVBS;
        LOG(VB_CHANNEL, LOG_DEBUG, QString("DVBChan[%1]:%2 set delivery_system to %3")
            .arg(inputId).arg(__LINE__).arg(delivery_system.toString()));
    }
    else if (tuner_type == DTVTunerType::kTunerTypeDVBS2)
    {
        delivery_system = DTVModulationSystem::kModulationSystem_DVBS2;
        LOG(VB_CHANNEL, LOG_DEBUG, QString("DVBChan[%1]:%2 set delivery_system to %3")
            .arg(inputId).arg(__LINE__).arg(delivery_system.toString()));
    }

    LOG(VB_CHANNEL, LOG_INFO, QString("DVBChan[%1]: Delivery system: %2")
        .arg(inputId).arg(delivery_system.toString()));

    if (delivery_system != DTVModulationSystem::kModulationSystem_UNDEFINED)
    {
        cmdseq->props[c].cmd      = DTV_DELIVERY_SYSTEM;
        cmdseq->props[c++].u.data = delivery_system;
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR, QString("DVBChan[%1]: Delivery system: %2")
            .arg(inputId).arg(delivery_system.toString()));
    }

    // Frequency, modulation and inversion
    cmdseq->props[c].cmd      = DTV_FREQUENCY;
    cmdseq->props[c++].u.data = intermediate_freq ? intermediate_freq : tuning.m_frequency;
    cmdseq->props[c].cmd      = DTV_MODULATION;
    cmdseq->props[c++].u.data = tuning.m_modulation;
    cmdseq->props[c].cmd      = DTV_INVERSION;
    cmdseq->props[c++].u.data = tuning.m_inversion;

    // Symbol rate
    if (tuner_type == DTVTunerType::kTunerTypeDVBS1 ||
        tuner_type == DTVTunerType::kTunerTypeDVBS2 ||
        tuner_type == DTVTunerType::kTunerTypeDVBC)
    {
        cmdseq->props[c].cmd      = DTV_SYMBOL_RATE;
        cmdseq->props[c++].u.data = tuning.m_symbolRate;
    }

    // Forward error correction
    if (tuner_type.IsFECVariable())
    {
        cmdseq->props[c].cmd      = DTV_INNER_FEC;
        cmdseq->props[c++].u.data = can_fec_auto ? FEC_AUTO
            : (fe_code_rate_t) (int) tuning.m_fec;
    }

    // DVB-T/T2 properties
    if (tuner_type == DTVTunerType::kTunerTypeDVBT ||
        tuner_type == DTVTunerType::kTunerTypeDVBT2)
    {
        cmdseq->props[c].cmd      = DTV_BANDWIDTH_HZ;
        cmdseq->props[c++].u.data = (8-tuning.m_bandwidth) * 1000000;
        cmdseq->props[c].cmd      = DTV_CODE_RATE_HP;
        cmdseq->props[c++].u.data = tuning.m_hpCodeRate;
        cmdseq->props[c].cmd      = DTV_CODE_RATE_LP;
        cmdseq->props[c++].u.data = tuning.m_lpCodeRate;
        cmdseq->props[c].cmd      = DTV_TRANSMISSION_MODE;
        cmdseq->props[c++].u.data = tuning.m_transMode;
        cmdseq->props[c].cmd      = DTV_GUARD_INTERVAL;
        cmdseq->props[c++].u.data = tuning.m_guardInterval;
        cmdseq->props[c].cmd      = DTV_HIERARCHY;
        cmdseq->props[c++].u.data = tuning.m_hierarchy;
    }

    // DVB-S properties can be set on DVB-S2 tuner
    if (delivery_system == DTVModulationSystem::kModulationSystem_DVBS)
    {
        cmdseq->props[c].cmd      = DTV_ROLLOFF;
        cmdseq->props[c++].u.data = DTVRollOff::kRollOff_35;
    }

    // DVB-S2 properties
    if (delivery_system == DTVModulationSystem::kModulationSystem_DVBS2)
    {
        cmdseq->props[c].cmd      = DTV_PILOT;
        cmdseq->props[c++].u.data = PILOT_AUTO;
        cmdseq->props[c].cmd      = DTV_ROLLOFF;
        cmdseq->props[c++].u.data = tuning.m_rolloff;
    }

    if (do_tune)
        cmdseq->props[c++].cmd    = DTV_TUNE;

    cmdseq->num = c;

    return cmdseq;
}
