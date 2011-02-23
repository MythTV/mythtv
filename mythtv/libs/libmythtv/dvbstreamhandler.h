// -*- Mode: c++ -*-

#ifndef _DVBSTREAMHANDLER_H_
#define _DVBSTREAMHANDLER_H_

#include <vector>
using namespace std;

#include <QMap>
#include <QMutex>
#include <QThread>

#include "util.h"
#include "DeviceReadBuffer.h"
#include "mpegstreamdata.h"

class QString;
class DVBStreamHandler;
class DTVSignalMonitor;
class DVBChannel;
class DeviceReadBuffer;

typedef QMap<uint,int> FilterMap;

//#define RETUNE_TIMEOUT 5000

class PIDInfo
{
  public:
    PIDInfo() :
        _pid(0xffffffff), filter_fd(-1), streamType(0), pesType(-1) {;}
    PIDInfo(uint pid) :
        _pid(pid),        filter_fd(-1), streamType(0), pesType(-1) {;}
    PIDInfo(uint pid, uint stream_type, uint pes_type) :
        _pid(pid),                       filter_fd(-1),
        streamType(stream_type),         pesType(pes_type) {;}

    bool Open(const QString &dvb_dev, bool use_section_reader);
    bool Close(const QString &dvb_dev);
    bool IsOpen(void) const { return filter_fd >= 0; }

    uint        _pid;
    int         filter_fd;         ///< Input filter file descriptor
    uint        streamType;        ///< StreamID
    int         pesType;           ///< PESStreamID
};
typedef QMap<uint,PIDInfo*> PIDInfoMap;

class DVBReadThread : public QThread
{
    Q_OBJECT
  public:
    DVBReadThread() : m_parent(NULL) {}
    void SetParent(DVBStreamHandler *parent) { m_parent = parent; }
    void run(void);
  private:
    DVBStreamHandler *m_parent;
};

class DVBStreamHandler : public ReaderPausedCB
{
    friend class DVBReadThread;

  public:
    static DVBStreamHandler *Get(const QString &dvb_device);
    static void Return(DVBStreamHandler * & ref);

    void AddListener(MPEGStreamData *data,
                     bool allow_section_reader,
                     bool needs_buffering);
    void RemoveListener(MPEGStreamData *data);

    void RetuneMonitor(void);

    bool IsRunning(void) const { return _reader_thread.isRunning(); }
    bool IsRetuneAllowed(void) const { return _allow_retune; }

    void SetRetuneAllowed(bool              allow,
                          DTVSignalMonitor *sigmon,
                          DVBChannel       *dvbchan);

    // ReaderPausedCB
    virtual void ReaderPaused(int fd) { (void) fd; }

  private:
    DVBStreamHandler(const QString &);
    ~DVBStreamHandler();

    void Start(void);
    void Stop(void);

    void Run(void);
    void RunTS(void);
    void RunSR(void);

    void UpdateListeningForEIT(void);
    bool UpdateFiltersFromStreamData(void);
    bool AddPIDFilter(PIDInfo *info);
    bool RemovePIDFilter(uint pid);
    bool RemoveAllPIDFilters(void);
    void CycleFiltersByPriority(void);

    PIDPriority GetPIDPriority(uint pid) const;
    bool SupportsTSMonitoring(void);

  private:
    QString           _dvb_dev;
    QString           _dvr_dev_path;
    bool              _allow_section_reader;
    bool              _needs_buffering;
    bool              _allow_retune;

    mutable QMutex     _start_stop_lock;
    DVBReadThread     _reader_thread;
    bool              _using_section_reader;
    DeviceReadBuffer *_device_read_buffer;
    DTVSignalMonitor *_sigmon;
    DVBChannel       *_dvbchannel;

    mutable QMutex    _pid_lock;
    vector<uint>      _eit_pids;
    PIDInfoMap        _pid_info;
    uint              _open_pid_filters;
    MythTimer         _cycle_timer;

    mutable QMutex          _listener_lock;
    vector<MPEGStreamData*> _stream_data_list;

    // for caching TS monitoring supported value.
    static QMutex             _rec_supports_ts_monitoring_lock;
    static QMap<QString,bool> _rec_supports_ts_monitoring;

    // for implementing Get & Return
    static QMutex                          _handlers_lock;
    static QMap<QString,DVBStreamHandler*> _handlers;
    static QMap<QString,uint>              _handlers_refcnt;
};

#endif // _DVBSTREAMHANDLER_H_
