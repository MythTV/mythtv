// -*- Mode: c++ -*-

#ifndef _DVBSTREAMHANDLER_H_
#define _DVBSTREAMHANDLER_H_

#include <vector>
using namespace std;

#include <QString>
#include <QMutex>
#include <QMap>

#include "streamhandler.h"

class QString;
class DVBStreamHandler;
class DTVSignalMonitor;
class DVBChannel;
class DeviceReadBuffer;

//#define RETUNE_TIMEOUT 5000

class DVBPIDInfo : public PIDInfo
{
  public:
    DVBPIDInfo(uint pid) : PIDInfo(pid) {}
    DVBPIDInfo(uint pid, uint stream_type, int pes_type) :
        PIDInfo(pid, stream_type, pes_type) {}
    bool Open(const QString &dvb_dev, bool use_section_reader);
    bool Close(const QString &dvb_dev);
};

class DVBStreamHandler : public StreamHandler
{
  public:
    static DVBStreamHandler *Get(const QString &dvb_device);
    static void Return(DVBStreamHandler * & ref);

    // DVB specific

    void RetuneMonitor(void);

    bool IsRetuneAllowed(void) const { return _allow_retune; }

    void SetRetuneAllowed(bool              allow,
                          DTVSignalMonitor *sigmon,
                          DVBChannel       *dvbchan);

  private:
    DVBStreamHandler(const QString &);

    virtual void run(void); // MThread
    void RunTS(void);
    void RunSR(void);

    virtual void CycleFiltersByPriority(void);

    bool SupportsTSMonitoring(void);

    virtual PIDInfo *CreatePIDInfo(uint pid, uint stream_type, int pes_type)
        { return new DVBPIDInfo(pid, stream_type, pes_type); }

    virtual void SetRunningDesired(bool desired); // StreamHandler

  private:
    QString           _dvr_dev_path;
    volatile bool     _allow_retune;

    DTVSignalMonitor *_sigmon;
    DVBChannel       *_dvbchannel;
    DeviceReadBuffer *_drb;

    // for caching TS monitoring supported value.
    static QMutex             _rec_supports_ts_monitoring_lock;
    static QMap<QString,bool> _rec_supports_ts_monitoring;

    // for implementing Get & Return
    static QMutex                          _handlers_lock;
    static QMap<QString,DVBStreamHandler*> _handlers;
    static QMap<QString,uint>              _handlers_refcnt;
};

#endif // _DVBSTREAMHANDLER_H_
