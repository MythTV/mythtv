// -*- Mode: c++ -*-

#ifndef _HDHRSTREAMHANDLER_H_
#define _HDHRSTREAMHANDLER_H_

#include <vector>
using namespace std;

#include <QMap>
#include <QMutex>
#include <QThread>

#include "util.h"
#include "DeviceReadBuffer.h"
#include "mpegstreamdata.h"
#include "dtvconfparserhelpers.h"

class QString;
class HDHRStreamHandler;
class DTVSignalMonitor;
class HDHRChannel;
class DeviceReadBuffer;

// HDHomeRun headers
#ifdef USING_HDHOMERUN
#include "hdhomerun.h"
#else
struct hdhomerun_device_t { int dummy; };
#endif

enum HDHRTuneMode {
    hdhrTuneModeNone = 0,
    hdhrTuneModeFrequency,
    hdhrTuneModeFrequencyPid,
    hdhrTuneModeFrequencyProgram,
    hdhrTuneModeVChannel
};

typedef QMap<uint,int> FilterMap;

//#define RETUNE_TIMEOUT 5000

class HDHRReadThread : public QThread
{
    Q_OBJECT
  public:
    HDHRReadThread() : m_parent(NULL) {}
    void SetParent(HDHRStreamHandler *parent) { m_parent = parent; }
    void run(void);
  private:
    HDHRStreamHandler *m_parent;
};

class HDHRStreamHandler : public ReaderPausedCB
{
    friend class HDHRReadThread;

  public:
    static HDHRStreamHandler *Get(const QString &devicename);
    static void Return(HDHRStreamHandler * & ref);

    void AddListener(MPEGStreamData *data);
    void RemoveListener(MPEGStreamData *data);

    bool IsRunning(void) const { return _reader_thread.isRunning(); }
    void GetTunerStatus(struct hdhomerun_tuner_status_t *status);
    bool IsConnected(void) const;
    vector<DTVTunerType> GetTunerTypes(void) const { return _tuner_types; }

    // Commands
    bool TuneChannel(const QString &chanid);
    bool TuneProgram(uint mpeg_prog_num);
    bool TuneVChannel(const QString &vchn);
    bool EnterPowerSavingMode(void);


    // ReaderPausedCB
    virtual void ReaderPaused(int fd) { (void) fd; }

  private:
    HDHRStreamHandler(const QString &);
    ~HDHRStreamHandler();

    bool Connect(void);

    QString TunerGet(const QString &name,
                     bool report_error_return = true,
                     bool print_error = true) const;
    QString TunerSet(const QString &name, const QString &value,
                     bool report_error_return = true,
                     bool print_error = true);

    bool Open(void);
    void Close(void);

    void Start(void);
    void Stop(void);

    void Run(void);
    void RunTS(void);

    void UpdateListeningForEIT(void);
    bool UpdateFiltersFromStreamData(void);
    bool AddPIDFilter(uint pid, bool do_update = true);
    bool RemovePIDFilter(uint pid, bool do_update = true);
    bool RemoveAllPIDFilters(void);
    bool UpdateFilters(void);

    PIDPriority GetPIDPriority(uint pid) const;

  private:
    hdhomerun_device_t *_hdhomerun_device;
    uint                 _tuner;
    QString              _devicename;
    vector<DTVTunerType> _tuner_types;
    HDHRTuneMode         _tune_mode; // debug self check

    mutable QMutex    _start_stop_lock;
    HDHRReadThread    _reader_thread;
    bool              _run;

    mutable QMutex    _pid_lock;
    vector<uint>      _eit_pids;
    vector<uint>      _pid_info; // kept sorted
    uint              _open_pid_filters;
    MythTimer         _cycle_timer;

    mutable QMutex          _listener_lock;
    vector<MPEGStreamData*> _stream_data_list;

    mutable QMutex          _hdhr_lock;

    // for caching TS monitoring supported value.
    static QMutex          _rec_supports_ts_monitoring_lock;
    static QMap<uint,bool> _rec_supports_ts_monitoring;

    // for implementing Get & Return
    static QMutex                            _handlers_lock;
    static QMap<QString, HDHRStreamHandler*> _handlers;
    static QMap<QString, uint>               _handlers_refcnt;
};

#endif // _HDHRSTREAMHANDLER_H_
