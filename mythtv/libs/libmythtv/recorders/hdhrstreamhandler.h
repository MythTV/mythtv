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
#include HDHOMERUN_HEADERFILE
#else
struct hdhomerun_device_t { int dummy; };
struct hdhomerun_device_selector_t { int dummy; };
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
    static HDHRStreamHandler *Get(const QString &devname, int inputid,
                                  int majorid);
    static void Return(HDHRStreamHandler * & ref, int inputid);

    void AddListener(MPEGStreamData *data,
                     bool /*allow_section_reader*/ = false,
                     bool /*needs_drb*/            = false,
                     QString output_file       = QString()) override // StreamHandler
    {
        StreamHandler::AddListener(data, false, false, output_file);
    }

    void GetTunerStatus(struct hdhomerun_tuner_status_t *status);
    bool IsConnected(void) const;
    vector<DTVTunerType> GetTunerTypes(void) const { return m_tuner_types; }

    // Commands
    bool TuneChannel(const QString &chanid);
    bool TuneProgram(uint mpeg_prog_num);
    bool TuneVChannel(const QString &vchn);

  private:
    explicit HDHRStreamHandler(const QString &, int inputid, int majorid);

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
    hdhomerun_device_t          *m_hdhomerun_device {nullptr};
    hdhomerun_device_selector_t *m_device_selector  {nullptr};
    int                          m_tuner            {-1};
    vector<DTVTunerType>         m_tuner_types;
    HDHRTuneMode                 m_tune_mode        {hdhrTuneModeNone}; // debug self check
    int                          m_majorid;

    mutable QMutex               m_hdhr_lock        {QMutex::Recursive};

    // for implementing Get & Return
    static QMutex                            s_handlers_lock;
    static QMap<int, HDHRStreamHandler*>     s_handlers;
    static QMap<int, uint>                   s_handlers_refcnt;
};

#endif // _HDHRSTREAMHANDLER_H_
