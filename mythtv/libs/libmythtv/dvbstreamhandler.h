// -*- Mode: c++ -*-

#ifndef _DVBSTREAMHANDLER_H_
#define _DVBSTREAMHANDLER_H_

#include <vector>
using namespace std;

#include <qmap.h>
#include <qmutex.h>

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

    bool Open(uint dvb_dev_num, bool use_section_reader);
    bool Close(uint dvb_dev_num);
    bool IsOpen(void) const { return filter_fd >= 0; }

    uint        _pid;
    int         filter_fd;         ///< Input filter file descriptor
    uint        streamType;        ///< StreamID
    int         pesType;           ///< PESStreamID
};
typedef QMap<uint,PIDInfo*> PIDInfoMap;

class DVBStreamHandler : public ReaderPausedCB
{
    friend void *run_dvb_stream_handler_thunk(void *param);

  public:
    static DVBStreamHandler *Get(uint dvb_device_number);
    static void Return(DVBStreamHandler * & ref);

    void AddListener(MPEGStreamData *data,
                     bool allow_section_reader,
                     bool needs_buffering);
    void RemoveListener(MPEGStreamData *data);

    void RetuneMonitor(void);

    bool IsRunning(void) const { return _running; }
    bool IsRetuneAllowed(void) const { return _allow_retune; }

    void SetRetuneAllowed(bool              allow,
                          DTVSignalMonitor *sigmon,
                          DVBChannel       *dvbchan);

    // ReaderPausedCB
    virtual void ReaderPaused(int fd) { (void) fd; }

  private:
    DVBStreamHandler(uint);
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

    void SetRunning(bool);

    PIDPriority GetPIDPriority(uint pid) const;
    bool SupportsTSMonitoring(void);

  private:
    uint              _dvb_dev_num;
    QString           _dvr_dev_path;
    bool              _allow_section_reader;
    bool              _needs_buffering;
    bool              _allow_retune;

    mutable QMutex     _start_stop_lock;
    bool              _running;
    QWaitCondition    _running_state_changed;
    pthread_t         _reader_thread;
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
    static QMutex          _rec_supports_ts_monitoring_lock;
    static QMap<uint,bool> _rec_supports_ts_monitoring;

    // for implementing Get & Return
    static QMutex                       _handlers_lock;
    static QMap<uint,DVBStreamHandler*> _handlers;
    static QMap<uint,uint>              _handlers_refcnt;
};

#endif // _DVBSTREAMHANDLER_H_
