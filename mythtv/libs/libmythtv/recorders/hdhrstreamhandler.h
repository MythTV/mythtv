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

// Note : This class never uses a DRB && always uses a TS reader.

// locking order
// _pid_lock -> _listener_lock -> _start_stop_lock
//                             -> _hdhr_lock

class HDHRStreamHandler : public StreamHandler
{
  public:
    static HDHRStreamHandler *Get(const QString &devicename);
    static void Return(HDHRStreamHandler * & ref);

    virtual void AddListener(MPEGStreamData *data,
                             bool allow_section_reader = false,
                             bool needs_drb            = false,
                             QString output_file       = QString())
    {
        StreamHandler::AddListener(data, false, false, output_file);
    } // StreamHandler

    void GetTunerStatus(struct hdhomerun_tuner_status_t *status);
    bool IsConnected(void) const;
    vector<DTVTunerType> GetTunerTypes(void) const { return _tuner_types; }

    // Commands
    bool TuneChannel(const QString &chanid);
    bool TuneProgram(uint mpeg_prog_num);
    bool TuneVChannel(const QString &vchn);
    bool EnterPowerSavingMode(void);

  private:
    HDHRStreamHandler(const QString &);

    bool Connect(void);

    QString TunerGet(const QString &name,
                     bool report_error_return = true,
                     bool print_error = true) const;
    QString TunerSet(const QString &name, const QString &value,
                     bool report_error_return = true,
                     bool print_error = true);

    bool Open(void);
    void Close(void);

    virtual void run(void); // MThread

    virtual bool UpdateFilters(void);

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
