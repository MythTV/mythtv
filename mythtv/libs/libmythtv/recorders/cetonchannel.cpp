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

#define LOC QString("CetonChan[%1](%2): ").arg(m_inputid).arg(CetonChannel::GetDevice())

CetonChannel::~CetonChannel(void)
{
    CetonChannel::Close();
}

bool CetonChannel::Open(void)
{
    LOG(VB_CHANNEL, LOG_INFO, LOC + "Opening Ceton channel");

    if (IsOpen())
        return true;

    m_stream_handler = CetonStreamHandler::Get(m_device_id, GetInputID());

    m_tunerType = DTVTunerType::kTunerTypeATSC;
    m_tuner_types.push_back(m_tunerType);

    if (!InitializeInput())
    {
        Close();
        return false;
    }

    return m_stream_handler->IsConnected();
}

void CetonChannel::Close(void)
{
    LOG(VB_CHANNEL, LOG_INFO, LOC + "Closing Ceton channel");

    if (!CetonChannel::IsOpen())
        return; // this caller didn't have it open in the first place..

    CetonStreamHandler::Return(m_stream_handler, GetInputID());
}

bool CetonChannel::EnterPowerSavingMode(void)
{
    if (IsOpen())
        return m_stream_handler->EnterPowerSavingMode();
    return true;
}

bool CetonChannel::IsOpen(void) const
{
      return m_stream_handler;
}

/// This is used when the tuner type is kTunerTypeOCUR
bool CetonChannel::Tune(const QString &freqid, int /*finetune*/)
{
    return m_stream_handler->TuneVChannel(freqid);
}

static QString format_modulation(const DTVMultiplex &tuning)
{
    if (DTVModulation::kModulationQAM256 == tuning.m_modulation)
        return "qam_256";
    if (DTVModulation::kModulationQAM64 == tuning.m_modulation)
        return "qam_64";
    //note...ceton also supports NTSC-M, but not sure what to use that for
    if (DTVModulation::kModulation8VSB == tuning.m_modulation)
        return "8vsb";

    return "unknown";
}

bool CetonChannel::Tune(const DTVMultiplex &tuning)
{
    QString modulation = format_modulation(tuning);

    LOG(VB_CHANNEL, LOG_INFO, LOC + QString("Tuning to %1 %2")
        .arg(tuning.m_frequency).arg(modulation));

    if (m_stream_handler->TuneFrequency(tuning.m_frequency, modulation))
    {
        SetSIStandard(tuning.m_sistandard);
        return true;
    }

    return false;
}

bool CetonChannel::SetChannelByString(const QString &channum)
{
    bool ok = DTVChannel::SetChannelByString(channum);

    if (ok)
    {
        if (m_stream_handler->IsCableCardInstalled())
            m_currentProgramNum = m_stream_handler->GetProgramNumber();
        else
            m_stream_handler->TuneProgram(m_currentProgramNum);
    }
    return ok;
}
