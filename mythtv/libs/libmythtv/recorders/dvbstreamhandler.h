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
    explicit DVBPIDInfo(uint pid) : PIDInfo(pid) {}
    DVBPIDInfo(uint pid, uint stream_type, int pes_type) :
        PIDInfo(pid, stream_type, pes_type) {}
    bool Open(const QString &dvb_dev, bool use_section_reader) override; // PIDInfo
    bool Close(const QString &dvb_dev) override; // PIDInfo
};

class DVBStreamHandler : public StreamHandler
{
  public:
    static DVBStreamHandler *Get(const QString &devname, int inputid);
    static void Return(DVBStreamHandler * & ref, int inputid);

    // DVB specific

    void RetuneMonitor(void);

    bool IsRetuneAllowed(void) const { return _allow_retune; }

    void SetRetuneAllowed(bool              allow,
                          DTVSignalMonitor *sigmon,
                          DVBChannel       *dvbchan);

  private:
    explicit DVBStreamHandler(const QString &, int inputid);

    void run(void) override; // MThread
    void RunTS(void);
    void RunSR(void);

    void CycleFiltersByPriority(void) override; // StreamHandler

    bool SupportsTSMonitoring(void);

    PIDInfo *CreatePIDInfo(uint pid, uint stream_type, int pes_type) override // StreamHandler
        { return new DVBPIDInfo(pid, stream_type, pes_type); }

  private:
    QString           _dvr_dev_path;
    volatile bool     _allow_retune;

    DTVSignalMonitor *_sigmon;
    DVBChannel       *_dvbchannel;
    DeviceReadBuffer *_drb;

    // for caching TS monitoring supported value.
    static QMutex             s_rec_supports_ts_monitoring_lock;
    static QMap<QString,bool> s_rec_supports_ts_monitoring;

    // for implementing Get & Return
    static QMutex                          s_handlers_lock;
    static QMap<QString,DVBStreamHandler*> s_handlers;
    static QMap<QString,uint>              s_handlers_refcnt;
};

#endif // _DVBSTREAMHANDLER_H_
