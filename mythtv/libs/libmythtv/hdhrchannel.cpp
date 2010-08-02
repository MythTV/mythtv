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
#include "mythverbose.h"
#include "hdhrchannel.h"
#include "videosource.h"
#include "channelutil.h"
#include "hdhrstreamhandler.h"

#define DEBUG_PID_FILTERS

#define LOC     QString("HDHRChan(%1): ").arg(GetDevice())
#define LOC_ERR QString("HDHRChan(%1), Error: ").arg(GetDevice())

HDHRChannel::HDHRChannel(TVRec *parent, const QString &device)
    : DTVChannel(parent),
      _device_id(device),           _stream_handler(NULL),
      _lock(QMutex::Recursive),
      tune_lock(QMutex::Recursive), hw_lock(QMutex::Recursive)
{
}

HDHRChannel::~HDHRChannel(void)
{
    Close();
}

bool HDHRChannel::Open(void)
{
    VERBOSE(VB_CHANNEL, LOC + "Opening HDHR channel");

    QMutexLocker locker(&hw_lock);

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
    VERBOSE(VB_CHANNEL, LOC + "Closing HDHR channel");

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

bool HDHRChannel::Init(
    QString &inputname, QString &startchannel, bool setchan)
{
    if (setchan && !IsOpen())
        Open();

    return ChannelBase::Init(inputname, startchannel, setchan);
}

bool HDHRChannel::SetChannelByString(const QString &channum)
{
    QString loc = LOC + QString("SetChannelByString(%1)").arg(channum);
    QString loc_err = loc + ", Error: ";
    VERBOSE(VB_CHANNEL, loc);

    if (!Open())
    {
        VERBOSE(VB_IMPORTANT, loc_err + "Channel object "
                "will not open, can not change channels.");

        return false;
    }

    QString inputName;
    if (!CheckChannel(channum, inputName))
    {
        VERBOSE(VB_IMPORTANT, loc_err +
                "CheckChannel failed.\n\t\t\tPlease verify the channel "
                "in the 'mythtv-setup' Channel Editor.");

        return false;
    }

    // If CheckChannel filled in the inputName then we need to
    // change inputs and return, since the act of changing
    // inputs will change the channel as well.
    if (!inputName.isEmpty())
        return SelectInput(inputName, channum, false);

    ClearDTVInfo();

    InputMap::const_iterator it = m_inputs.find(m_currentInputID);
    if (it == m_inputs.end())
        return false;

    uint mplexid_restriction;
    if (!IsInputAvailable(m_currentInputID, mplexid_restriction))
        return false;

    if (Aborted())
        return false;

    // Fetch tuning data from the database.
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
        mplexid, m_commfree))
    {
        return false;
    }

    if (mplexid_restriction && (mplexid != mplexid_restriction))
        return false;

    // If the frequency is zeroed out, don't use it directly.
    bool ok = (frequency > 0);
    if (!ok)
    {
        frequency = (freqid.toInt(&ok) + finetune) * 1000;
        mplexid = 0;
    }
    bool isFrequency = ok && (frequency > 10000000);

    // Tune to proper frequency
    if ((*it)->externalChanger.isEmpty())
    {
        if (isFrequency)
        {
            if (!Tune(frequency, inputName, modulation, si_std))
                return false;
        }
        else
        {
            if (!_stream_handler->TuneVChannel(channum))
            {
                VERBOSE(VB_IMPORTANT, LOC_ERR +
                        "dtv_multiplex data is required for tuning");
                return false;
            }

            SetSIStandard(si_std);
        }
    }
    else if (!ChangeExternalChannel(freqid))
        return false;

    // Set the current channum to the new channel's channum
    QString tmp = channum; tmp.detach();
    m_curchannelname = tmp;

    // Set the major and minor channel for any additional multiplex tuning
    SetDTVInfo(atsc_major, atsc_minor, netid, tsid, mpeg_prog_num);

    // Set this as the future start channel for this source
    QString tmpX = m_curchannelname; tmpX.detach();
    m_inputs[m_currentInputID]->startChanNum = tmpX;

    return true;
}

// documented in dtvchannel.h
bool HDHRChannel::TuneMultiplex(uint mplexid, QString inputname)
{
    VERBOSE(VB_CHANNEL, LOC + QString("TuneMultiplex(%1)").arg(mplexid));

    QString  modulation;
    QString  si_std;
    uint64_t frequency;
    uint     transportid;
    uint     dvb_networkid;

    if (!ChannelUtil::GetTuningParams(
            mplexid, modulation, frequency,
            transportid, dvb_networkid, si_std))
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "TuneMultiplex(): " +
                QString("Could not find tuning parameters for multiplex %1.")
                .arg(mplexid));

        return false;
    }

    if (!Tune(frequency, inputname, modulation, si_std))
        return false;

    return true;
}

bool HDHRChannel::Tune(const DTVMultiplex &tuning, QString inputname)
{
    return Tune(tuning.frequency, inputname,
                tuning.modulation.toString(), tuning.sistandard);
}

bool HDHRChannel::Tune(uint frequency, QString /*input*/,
                       QString modulation, QString si_std)
{
    QString chan = modulation + ':' + QString::number(frequency);

    VERBOSE(VB_CHANNEL, LOC + "Tuning to " + chan);

    if (_stream_handler->TuneChannel(chan))
    {
        SetSIStandard(si_std);
        return true;
    }


    // dtv_multiplex.modulation is from the DB. Could contain almost anything.
    // As a fallback, use the HDHR device's automatic scanning:
    chan = "auto:" + QString::number(frequency);

    VERBOSE(VB_CHANNEL, LOC + "Failed. Now trying " + chan);

    if (_stream_handler->TuneChannel(chan))
    {
        SetSIStandard(si_std);
        return true;
    }


    return false;
}
