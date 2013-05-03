/** -*- Mode: c++ -*-
 *  CetonChannel
 *  Copyright (c) 2011 Ronald Frazier
 *  Copyright (c) 2006-2009 by Silicondust Engineering Ltd.
 *  Distributed as part of MythTV under GPL v2 and later.
 */

// MythTV includes
#include "cetonstreamhandler.h"
#include "cetonchannel.h"
#include "videosource.h"
#include "mythlogging.h"
#include "channelutil.h"
#include "mythdbcon.h"

#define LOC QString("CetonChan[%1](%2): ").arg(GetCardID()).arg(GetDevice())

CetonChannel::CetonChannel(TVRec *parent, const QString &device) :
    DTVChannel(parent), _device_id(device), _stream_handler(NULL)
{
}

CetonChannel::~CetonChannel(void)
{
    Close();
}

bool CetonChannel::Open(void)
{
    LOG(VB_CHANNEL, LOG_INFO, LOC + "Opening Ceton channel");

    if (IsOpen())
        return true;

    _stream_handler = CetonStreamHandler::Get(_device_id);

    tunerType = DTVTunerType::kTunerTypeATSC;
    _tuner_types.push_back(tunerType);

    if (!InitializeInputs())
    {
        Close();
        return false;
    }

    return _stream_handler->IsConnected();
}

void CetonChannel::Close(void)
{
    LOG(VB_CHANNEL, LOG_INFO, LOC + "Closing Ceton channel");

    if (!IsOpen())
        return; // this caller didn't have it open in the first place..

    CetonStreamHandler::Return(_stream_handler);
}

bool CetonChannel::EnterPowerSavingMode(void)
{
    if (IsOpen())
        return _stream_handler->EnterPowerSavingMode();
    else
        return true;
}

bool CetonChannel::IsOpen(void) const
{
      return _stream_handler;
}

/// This is used when the tuner type is kTunerTypeOCUR
bool CetonChannel::Tune(const QString &freqid, int /*finetune*/)
{
    return _stream_handler->TuneVChannel(freqid);
}

static QString format_modulation(const DTVMultiplex &tuning)
{
    if (DTVModulation::kModulationQAM256 == tuning.modulation)
        return "qam_256";
    else if (DTVModulation::kModulationQAM64 == tuning.modulation)
        return "qam_64";
    //note...ceton also supports NTSC-M, but not sure what to use that for
    else if (DTVModulation::kModulation8VSB == tuning.modulation)
        return "8vsb";

    return "unknown";
}

bool CetonChannel::Tune(const DTVMultiplex &tuning, QString /*inputname*/)
{
    QString modulation = format_modulation(tuning);

    LOG(VB_CHANNEL, LOG_INFO, LOC + QString("Tuning to %1 %2")
        .arg(tuning.frequency).arg(modulation));

    if (_stream_handler->TuneFrequency(tuning.frequency, modulation))
    {
        SetSIStandard(tuning.sistandard);
        return true;
    }

    return false;
}

bool CetonChannel::SetChannelByString(const QString &channum)
{
    bool ok = DTVChannel::SetChannelByString(channum);

    if (ok)
    {
        if (_stream_handler->IsCableCardInstalled())
            currentProgramNum = _stream_handler->GetProgramNumber();
        else
            _stream_handler->TuneProgram(currentProgramNum);
    }
    return ok;
}
