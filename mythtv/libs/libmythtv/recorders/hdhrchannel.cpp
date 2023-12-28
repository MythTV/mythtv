/**
 *  HDHRChannel
 *  Copyright (c) 2006-2009 by Silicondust Engineering Ltd.
 *  Distributed as part of MythTV under GPL v2 and later.
 */

// C includes
#include <unistd.h>
#include <sys/types.h>
#ifndef _WIN32
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#endif
#include <sys/time.h>
#include <fcntl.h>

// C++ includes
#include <algorithm>
#include <utility>

// MythTV includes
#include "libmythbase/mythdbcon.h"
#include "libmythbase/mythlogging.h"
#include "hdhrchannel.h"
#include "videosource.h"
#include "channelutil.h"
#include "hdhrstreamhandler.h"

#define LOC     QString("HDHRChan[%1](%2): ").arg(m_inputId).arg(HDHRChannel::GetDevice())

HDHRChannel::HDHRChannel(TVRec *parent, QString device)
    : DTVChannel(parent),
      m_deviceId(std::move(device))
{
    RegisterForMaster(m_deviceId);
}

HDHRChannel::~HDHRChannel(void)
{
    HDHRChannel::Close();
    DeregisterForMaster(m_deviceId);
}

bool HDHRChannel::IsMaster(void) const
{
    DTVChannel *master = DTVChannel::GetMasterLock(m_deviceId);
    bool is_master = (master == this);
    DTVChannel::ReturnMasterLock(master);
    return is_master;
}

bool HDHRChannel::Open(void)
{
    LOG(VB_CHANNEL, LOG_INFO, LOC + "Opening HDHR channel");

    if (IsOpen())
        return true;

    m_streamHandler = HDHRStreamHandler::Get(GetDevice(), GetInputID(),
                                             GetMajorID());

    m_tunerTypes = m_streamHandler->GetTunerTypes();
    m_tunerType = (m_tunerTypes.empty()) ?
        DTVTunerType::kTunerTypeUnknown : (int) m_tunerTypes[0];

    if (!InitializeInput())
    {
        Close();
        return false;
    }

    return m_streamHandler->IsConnected();
}

void HDHRChannel::Close(void)
{
    LOG(VB_CHANNEL, LOG_INFO, LOC + "Closing HDHR channel");

    if (!HDHRChannel::IsOpen())
        return; // this caller didn't have it open in the first place..

    HDHRStreamHandler::Return(m_streamHandler, GetInputID());
}

bool HDHRChannel::EnterPowerSavingMode(void)
{
    Close();
    return true;
}

bool HDHRChannel::IsOpen(void) const
{
      return m_streamHandler;
}

/// This is used when the tuner type is kTunerTypeOCUR
bool HDHRChannel::Tune(const QString &freqid, int /*finetune*/)
{
    return m_streamHandler->TuneVChannel(freqid);
}

static QString format_modulation(const DTVMultiplex &tuning)
{
    if (DTVModulation::kModulationQAM256 == tuning.m_modulation)
        return "qam256";
    if (DTVModulation::kModulationQAM128 == tuning.m_modulation)
        return "qam128";
    if (DTVModulation::kModulationQAM64 == tuning.m_modulation)
        return "qam64";
    if (DTVModulation::kModulationQAM16 == tuning.m_modulation)
        return "qam16";
    if (DTVModulation::kModulationDQPSK == tuning.m_modulation)
        return "qpsk";
    if (DTVModulation::kModulation8VSB == tuning.m_modulation)
        return "8vsb";

    return "auto";
}

static QString format_dvbt(const DTVMultiplex &tuning)
{
    const QChar b = tuning.m_bandwidth.toChar();
    if ((QChar('8') == b) || (QChar('7') == b) || (QChar('6') == b))
    {
        return QString("auto%1t").arg(b);
    }
    return "auto";
}

static QString format_dvbc(const DTVMultiplex &tuning, const QString &mod)
{
    const QChar b = tuning.m_bandwidth.toChar();

    // need bandwidth to set modulation and symbol rate
    if ((QChar('a') == b) && (mod != "auto") && (tuning.m_symbolRate > 0))
        return QString("a8%1-%2")
            .arg(mod).arg(tuning.m_symbolRate/1000);
    if ((QChar('a') == b) || (mod == "auto"))
        return "auto"; // uses bandwidth from channel map
    if ((QChar('a') != b) && (tuning.m_symbolRate > 0))
        return QString("a%1%2-%3")
            .arg(b).arg(mod).arg(tuning.m_symbolRate/1000);
    return QString("auto%1c").arg(b);
}

static QString get_tune_spec(
    const DTVTunerType tunerType, const DTVMultiplex &tuning)
{
    const QString mod = format_modulation(tuning);

    if (DTVTunerType::kTunerTypeATSC == tunerType)
        // old atsc firmware does no recognize "auto"
        return (mod == "auto") ? "qam" : mod;
    if (DTVTunerType::kTunerTypeDVBC == tunerType)
        return format_dvbc(tuning, mod);
    if ((DTVTunerType::kTunerTypeDVBT == tunerType) ||
        (DTVTunerType::kTunerTypeDVBT2 == tunerType))
    {
        return format_dvbt(tuning);
    }

    return "auto";
}

bool HDHRChannel::Tune(const DTVMultiplex &tuning)
{
    QString spec = get_tune_spec(m_tunerType, tuning);
    QString chan = QString("%1:%2").arg(spec).arg(tuning.m_frequency);

    LOG(VB_CHANNEL, LOG_INFO, LOC + "Tuning to " + chan);

    if (m_streamHandler->TuneChannel(chan))
    {
        SetSIStandard(tuning.m_sistandard);
        return true;
    }

    return false;
}

bool HDHRChannel::SetChannelByString(const QString &channum)
{
    bool ok = DTVChannel::SetChannelByString(channum);

    // HACK HACK HACK -- BEGIN
    // if the DTVTunerType were specified in tuning we wouldn't
    // need to try alternative tuning...
    if (!ok && DTVTunerType::kTunerTypeDVBT == m_tunerType)
    {
        bool has_dvbc = false;
        bool has_dvbt = false;
        for (const auto& type : m_tunerTypes)
        {
            has_dvbt |= (DTVTunerType::kTunerTypeDVBT == type);
            has_dvbc |= (DTVTunerType::kTunerTypeDVBC == type);
        }

        if (has_dvbt && has_dvbc)
        {
            m_tunerType = DTVTunerType::kTunerTypeDVBC;
            ok = DTVChannel::SetChannelByString(channum);
            if (!ok)
            {
                m_tunerType = DTVTunerType::kTunerTypeDVBT;
                return false;
            }
        }
    }
    // HACK HACK HACK -- END

    return ok;
}
