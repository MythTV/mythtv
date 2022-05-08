// -*- Mode: c++ -*-

#ifndef DVBSTREAMHANDLER_H
#define DVBSTREAMHANDLER_H

#include <vector>

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

    bool IsRetuneAllowed(void) const { return m_allowRetune; }

    void SetRetuneAllowed(bool              allow,
                          DTVSignalMonitor *sigmon,
                          DVBChannel       *dvbchan);

  private:
    explicit DVBStreamHandler(const QString &dvb_device, int inputid);

    void run(void) override; // MThread
    void RunTS(void);
    void RunSR(void);

    void CycleFiltersByPriority(void) override; // StreamHandler

    bool SupportsTSMonitoring(void);

    PIDInfo *CreatePIDInfo(uint pid, uint stream_type, int pes_type) override // StreamHandler
        { return new DVBPIDInfo(pid, stream_type, pes_type); }

  private:
    QString           m_dvrDevPath;
    volatile bool     m_allowRetune { false   };

    DTVSignalMonitor *m_sigMon      { nullptr };
    DVBChannel       *m_dvbChannel  { nullptr };
    DeviceReadBuffer *m_drb         { nullptr };

    // for caching TS monitoring supported value.
    static QMutex             s_rec_supportsTsMonitoringLock;
    static QMap<QString,bool> s_recSupportsTsMonitoring;

    // for implementing Get & Return
    static QMutex                          s_handlersLock;
    static QMap<QString,DVBStreamHandler*> s_handlers;
    static QMap<QString,uint>              s_handlersRefCnt;
};

#endif // DVBSTREAMHANDLER_H
