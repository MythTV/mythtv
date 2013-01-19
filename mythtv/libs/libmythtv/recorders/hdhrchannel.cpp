/**
 *  HDHRChannel
 *  Copyright (c) 2006-2009 by Silicondust Engineering Ltd.
 *  Distributed as part of MythTV under GPL v2 and later.
 */

// C includes
#include <unistd.h>
#include <sys/types.h>
#ifndef USING_MINGW
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#endif
#include <sys/time.h>
#include <fcntl.h>

// C++ includes
#include <algorithm>
using namespace std;

// MythTV includes
#include "mythdbcon.h"
#include "mythlogging.h"
#include "hdhrchannel.h"
#include "videosource.h"
#include "channelutil.h"
#include "hdhrstreamhandler.h"

#define LOC     QString("HDHRChan[%1](%2): ").arg(GetCardID()).arg(GetDevice())

HDHRChannel::HDHRChannel(TVRec *parent, const QString &device)
    : DTVChannel(parent),
      _device_id(device),
      _stream_handler(NULL)
{
    RegisterForMaster(_device_id);
}

HDHRChannel::~HDHRChannel(void)
{
    Close();
    DeregisterForMaster(_device_id);
}

bool HDHRChannel::IsMaster(void) const
{
    DTVChannel *master = DTVChannel::GetMasterLock(_device_id);
    bool is_master = (master == static_cast<const DTVChannel*>(this));
    DTVChannel::ReturnMasterLock(master);
    return is_master;
}

bool HDHRChannel::Open(void)
{
    LOG(VB_CHANNEL, LOG_INFO, LOC + "Opening HDHR channel");

    if (IsOpen())
        return true;

    _stream_handler = HDHRStreamHandler::Get(_device_id);

    _tuner_types = _stream_handler->GetTunerTypes();
    tunerType = (_tuner_types.empty()) ?
        DTVTunerType::kTunerTypeUnknown : (int) _tuner_types[0];

    if (!InitializeInputs())
    {
        Close();
        return false;
    }

    return _stream_handler->IsConnected();
}

void HDHRChannel::Close(void)
{
    LOG(VB_CHANNEL, LOG_INFO, LOC + "Closing HDHR channel");

    if (!IsOpen())
        return; // this caller didn't have it open in the first place..

    HDHRStreamHandler::Return(_stream_handler);
}

bool HDHRChannel::EnterPowerSavingMode(void)
{
    if (IsOpen())
        return _stream_handler->EnterPowerSavingMode();
    else
        return true;
}

bool HDHRChannel::IsOpen(void) const
{
      return _stream_handler;
}

/// This is used when the tuner type is kTunerTypeOCUR
bool HDHRChannel::Tune(const QString &freqid, int /*finetune*/)
{
    return _stream_handler->TuneVChannel(freqid);
}

static QString format_modulation(const DTVMultiplex &tuning)
{
    if (DTVModulation::kModulationQAM256 == tuning.modulation)
        return "qam256";
    else if (DTVModulation::kModulationQAM128 == tuning.modulation)
        return "qam128";
    else if (DTVModulation::kModulationQAM64 == tuning.modulation)
        return "qam64";
    else if (DTVModulation::kModulationQAM16 == tuning.modulation)
        return "qam16";
    else if (DTVModulation::kModulationDQPSK == tuning.modulation)
        return "qpsk";
    else if (DTVModulation::kModulation8VSB == tuning.modulation)
        return "8vsb";

    return "auto";
}

static QString format_dvbt(const DTVMultiplex &tuning, const QString &mod)
{
    const QChar b = tuning.bandwidth.toChar();

    if ((QChar('a') == b) || (mod == "auto"))
        return "auto"; // uses bandwidth from channel map
    else if (QChar('a') != b)
        return QString("t%1%2").arg(b).arg(mod);

    return QString("auto%1t").arg(b);
}

static QString format_dvbc(const DTVMultiplex &tuning, const QString &mod)
{
    const QChar b = tuning.bandwidth.toChar();

    if ((QChar('a') == b) || (mod == "auto"))
        return "auto"; // uses bandwidth from channel map
    else if ((QChar('a') != b) && (tuning.symbolrate > 0))
        return QString("a%1%2-%3")
            .arg(b).arg(mod).arg(tuning.symbolrate/1000);

    return QString("auto%1c").arg(b);
}

static QString get_tune_spec(
    const DTVTunerType &tunerType, const DTVMultiplex &tuning)
{
    const QString mod = format_modulation(tuning);

    if (DTVTunerType::kTunerTypeATSC == tunerType)
        // old atsc firmware does no recognize "auto"
        return (mod == "auto") ? "qam" : mod;
    else if (DTVTunerType::kTunerTypeDVBC == tunerType)
        return format_dvbc(tuning, mod);
    else if (DTVTunerType::kTunerTypeDVBT == tunerType)
        return format_dvbt(tuning, mod);

    return "auto";
}

bool HDHRChannel::Tune(const DTVMultiplex &tuning, QString /*inputname*/)
{
    QString spec = get_tune_spec(tunerType, tuning);
    QString chan = QString("%1:%2").arg(spec).arg(tuning.frequency);

    LOG(VB_CHANNEL, LOG_INFO, LOC + "Tuning to " + chan);

    if (_stream_handler->TuneChannel(chan))
    {
        SetSIStandard(tuning.sistandard);
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
    if (!ok && DTVTunerType::kTunerTypeDVBT == tunerType)
    {
        bool has_dvbc = false, has_dvbt = false;
        vector<DTVTunerType>::const_iterator it = _tuner_types.begin();
        for (; it != _tuner_types.end(); ++it)
        {
            has_dvbt |= (DTVTunerType::kTunerTypeDVBT == *it);
            has_dvbc |= (DTVTunerType::kTunerTypeDVBC == *it);
        }

        if (has_dvbt && has_dvbc)
        {
            tunerType = DTVTunerType::kTunerTypeDVBC;
            ok = DTVChannel::SetChannelByString(channum);
            if (!ok)
            {
                tunerType = DTVTunerType::kTunerTypeDVBT;
                return false;
            }
        }
    }
    // HACK HACK HACK -- END

    return ok;
}
