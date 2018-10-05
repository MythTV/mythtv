// -*- Mode: c++ -*-

#ifndef _HDHRSTREAMHANDLER_H_
#define _HDHRSTREAMHANDLER_H_

#include <vector>
using namespace std;

#include <QString>
#include <QMutex>
#include <QMap>

#include "mythdate.h"
#include "DeviceReadBuffer.h"
#include "mpegstreamdata.h"
#include "streamhandler.h"
#include "dtvconfparserhelpers.h"

class HDHRStreamHandler;
class DTVSignalMonitor;
class HDHRChannel;
class DeviceReadBuffer;

// HDHomeRun headers
#ifdef USING_HDHOMERUN
#ifdef HDHOMERUN_LIBPREFIX
#include "libhdhomerun/hdhomerun.h"
#else
#include "hdhomerun/hdhomerun.h"
#endif
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

// Note : This class never uses a DRB && always uses a TS reader.

// locking order
// _pid_lock -> _listener_lock -> _start_stop_lock
//                             -> _hdhr_lock

class HDHRStreamHandler : public StreamHandler
{
  public:
    static HDHRStreamHandler *Get(const QString &devicename,
                                  int recorder_id = -1);
    static void Return(HDHRStreamHandler * & ref, int recorder_id = -1);

    void AddListener(MPEGStreamData *data,
                     bool /*allow_section_reader*/ = false,
                     bool /*needs_drb*/            = false,
                     QString output_file       = QString()) override // StreamHandler
    {
        StreamHandler::AddListener(data, false, false, output_file);
    }

    void GetTunerStatus(struct hdhomerun_tuner_status_t *status);
    bool IsConnected(void) const;
    vector<DTVTunerType> GetTunerTypes(void) const { return _tuner_types; }

    // Commands
    bool TuneChannel(const QString &chanid);
    bool TuneProgram(uint mpeg_prog_num);
    bool TuneVChannel(const QString &vchn);
    bool EnterPowerSavingMode(void);

  private:
    explicit HDHRStreamHandler(const QString &);

    bool Connect(void);

    QString TunerGet(const QString &name,
                     bool report_error_return = true,
                     bool print_error = true) const;
    QString TunerSet(const QString &name, const QString &value,
                     bool report_error_return = true,
                     bool print_error = true);

    bool Open(void);
    void Close(void);

    void run(void) override; // MThread

    bool UpdateFilters(void) override; // StreamHandler

  private:
    hdhomerun_device_t     *_hdhomerun_device;
    int                     _tuner;
    vector<DTVTunerType>    _tuner_types;
    HDHRTuneMode            _tune_mode; // debug self check

    mutable QMutex          _hdhr_lock;

    // for implementing Get & Return
    static QMutex                            _handlers_lock;
    static QMap<QString, HDHRStreamHandler*> _handlers;
    static QMap<QString, uint>               _handlers_refcnt;
};

#endif // _HDHRSTREAMHANDLER_H_
