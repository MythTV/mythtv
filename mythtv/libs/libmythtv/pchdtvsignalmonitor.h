#ifndef PCHDTVSIGNALMONITOR_H
#define PCHDTVSIGNALMONITOR_H

#include "dtvsignalmonitor.h"

class Channel;

class pcHDTVSignalMonitor: public DTVSignalMonitor
{
public:
    pcHDTVSignalMonitor(int _capturecardnum, uint _inputnum, Channel *_channel);

private:
    pcHDTVSignalMonitor();
    virtual void UpdateValues();

    static int GetSignal(int fd, uint input, bool usingv4l2);

    uint     input;
    bool     usingv4l2;
    Channel *channel;
};

#endif // PCHDTVSIGNALMONITOR_H
