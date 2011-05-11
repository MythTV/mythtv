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
      _device_id(device),           _master(NULL),
      _stream_handler(NULL)
{
    _master = dynamic_cast<HDHRChannel*>(GetMaster(device));
    _master = (_master == this) ? NULL : _master;
}

HDHRChannel::~HDHRChannel(void)
{
    Close();
}

bool HDHRChannel::Open(void)
{
    VERBOSE(VB_CHANNEL, LOC + "Opening HDHR channel");

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

bool HDHRChannel::Tune(const QString &freqid, int /*finetune*/)
{
    return _stream_handler->TuneVChannel(freqid);
}

bool HDHRChannel::Tune(const DTVMultiplex &tuning, QString /*inputname*/)
{
    QString chan = tuning.modulation.toString() + ':' +
        QString::number(tuning.frequency);

    VERBOSE(VB_CHANNEL, LOC + "Tuning to " + chan);

    if (_stream_handler->TuneChannel(chan))
    {
        SetSIStandard(tuning.sistandard);
        return true;
    }

    return false;
}
