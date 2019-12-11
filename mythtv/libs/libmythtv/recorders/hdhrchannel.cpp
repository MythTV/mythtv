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

using namespace std;

// MythTV includes
#include "mythdbcon.h"
#include "mythlogging.h"
#include "hdhrchannel.h"
#include "videosource.h"
#include "channelutil.h"
#include "hdhrstreamhandler.h"

#define LOC     QString("HDHRChan[%1](%2): ").arg(m_inputid).arg(HDHRChannel::GetDevice())

HDHRChannel::HDHRChannel(TVRec *parent, QString device)
    : DTVChannel(parent),
      m_device_id(std::move(device))
{
    RegisterForMaster(m_device_id);
}

HDHRChannel::~HDHRChannel(void)
{
    HDHRChannel::Close();
    DeregisterForMaster(m_device_id);
}

bool HDHRChannel::IsMaster(void) const
{
    DTVChannel *master = DTVChannel::GetMasterLock(m_device_id);
    bool is_master = (master == static_cast<const DTVChannel*>(this));
    DTVChannel::ReturnMasterLock(master);
    return is_master;
}

bool HDHRChannel::Open(void)
{
    LOG(VB_CHANNEL, LOG_INFO, LOC + "Opening HDHR channel");

    if (IsOpen())
        return true;

    m_stream_handler = HDHRStreamHandler::Get(GetDevice(), GetInputID(),
                                             GetMajorID());

    m_tuner_types = m_stream_handler->GetTunerTypes();
    m_tunerType = (m_tuner_types.empty()) ?
        DTVTunerType::kTunerTypeUnknown : (int) m_tuner_types[0];

    if (!InitializeInput())
    {
        Close();
        return false;
    }

    return m_stream_handler->IsConnected();
}

void HDHRChannel::Close(void)
{
    LOG(VB_CHANNEL, LOG_INFO, LOC + "Closing HDHR channel");

    if (!HDHRChannel::IsOpen())
        return; // this caller didn't have it open in the first place..

    HDHRStreamHandler::Return(m_stream_handler, GetInputID());
}

bool HDHRChannel::EnterPowerSavingMode(void)
{
    Close();
    return true;
}

bool HDHRChannel::IsOpen(void) const
{
      return m_stream_handler;
}

/// This is used when the tuner type is kTunerTypeOCUR
bool HDHRChannel::Tune(const QString &freqid, int /*finetune*/)
{
    return m_stream_handler->TuneVChannel(freqid);
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

static QString format_dvbt(const DTVMultiplex &tuning, const QString &mod)
{
    const QChar b = tuning.m_bandwidth.toChar();

    if ((QChar('a') == b) || (mod == "auto"))
        return "auto"; // uses bandwidth from channel map
    if (QChar('a') != b)
        return QString("t%1%2").arg(b).arg(mod);
    return QString("auto%1t").arg(b);
}

static QString format_dvbc(const DTVMultiplex &tuning, const QString &mod)
{
    const QChar b = tuning.m_bandwidth.toChar();

    // need bandwidth to set modulation and symbol rate
    if ((QChar('a') == b) && (mod != "auto") && (tuning.m_symbolrate > 0))
        return QString("a8%1-%2")
            .arg(mod).arg(tuning.m_symbolrate/1000);
    if ((QChar('a') == b) || (mod == "auto"))
        return "auto"; // uses bandwidth from channel map
    if ((QChar('a') != b) && (tuning.m_symbolrate > 0))
        return QString("a%1%2-%3")
            .arg(b).arg(mod).arg(tuning.m_symbolrate/1000);
    return QString("auto%1c").arg(b);
}

static QString get_tune_spec(
    const DTVTunerType &tunerType, const DTVMultiplex &tuning)
{
    const QString mod = format_modulation(tuning);

    if (DTVTunerType::kTunerTypeATSC == tunerType)
        // old atsc firmware does no recognize "auto"
        return (mod == "auto") ? "qam" : mod;
    if (DTVTunerType::kTunerTypeDVBC == tunerType)
        return format_dvbc(tuning, mod);
    if (DTVTunerType::kTunerTypeDVBT == tunerType)
        return format_dvbt(tuning, mod);

    return "auto";
}

bool HDHRChannel::Tune(const DTVMultiplex &tuning)
{
    QString spec = get_tune_spec(m_tunerType, tuning);
    QString chan = QString("%1:%2").arg(spec).arg(tuning.m_frequency);

    LOG(VB_CHANNEL, LOG_INFO, LOC + "Tuning to " + chan);

    if (m_stream_handler->TuneChannel(chan))
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
        vector<DTVTunerType>::const_iterator it = m_tuner_types.begin();
        for (; it != m_tuner_types.end(); ++it)
        {
            has_dvbt |= (DTVTunerType::kTunerTypeDVBT == *it);
            has_dvbc |= (DTVTunerType::kTunerTypeDVBC == *it);
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
