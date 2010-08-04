// -*- Mode: c++ -*-

#include <cerrno>
#include <unistd.h>
#include <sys/ioctl.h>

#include "mythcontext.h"
#include "channelchangemonitor.h"
#include "channelbase.h"

#define LOC QString("ChannelChangeM: ").arg(channel->GetDevice())
#define LOC_ERR QString("ChannelChangeM, Error: ").arg(channel->GetDevice())

ChannelChangeMonitor::ChannelChangeMonitor(
    int db_cardnum, ChannelBase *_channel, uint64_t _flags) :
    SignalMonitor(db_cardnum, _channel, _flags)
{
}

void ChannelChangeMonitor::UpdateValues(void)
{
    if (!running || exit)
        return;

    if (!IsChannelTuned())
        return;

    {
        QMutexLocker locker(&statusLock);
        signalLock.SetValue(true);
        signalStrength.SetValue(100);
    }

    EmitStatus();
    SendMessageAllGood();
}
