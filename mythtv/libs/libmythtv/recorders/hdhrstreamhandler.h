// -*- Mode: c++ -*-

#ifndef HDHRSTREAMHANDLER_H
#define HDHRSTREAMHANDLER_H

#include <vector>

#include <QString>
#include <QMutex>
#include <QMap>
#if QT_VERSION >= QT_VERSION_CHECK(5,14,0)
#include <QRecursiveMutex>
#endif

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
                     const QString& output_file    = QString()) override // StreamHandler
    {
        StreamHandler::AddListener(data, false, false, output_file);
    }

    void GetTunerStatus(struct hdhomerun_tuner_status_t *status);
    bool IsConnected(void) const;
    std::vector<DTVTunerType> GetTunerTypes(void) const { return m_tunerTypes; }

    // Commands
    bool TuneChannel(const QString &chanid);
    bool TuneProgram(uint mpeg_prog_num);
    bool TuneVChannel(const QString &vchn);

  private:
    explicit HDHRStreamHandler(const QString &device, int inputid, int majorid);

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
    hdhomerun_device_t          *m_hdhomerunDevice  {nullptr};
    hdhomerun_device_selector_t *m_deviceSelector   {nullptr};
    int                          m_tuner            {-1};
    std::vector<DTVTunerType>    m_tunerTypes;
    HDHRTuneMode                 m_tuneMode         {hdhrTuneModeNone}; // debug self check
    int                          m_majorId;

#if QT_VERSION < QT_VERSION_CHECK(5,14,0)
    mutable QMutex               m_hdhrLock         {QMutex::Recursive};
#else
    mutable QRecursiveMutex      m_hdhrLock;
#endif

    // for implementing Get & Return
    static QMutex                            s_handlersLock;
    static QMap<int, HDHRStreamHandler*>     s_handlers;
    static QMap<int, uint>                   s_handlersRefCnt;
};

#endif // HDHRSTREAMHANDLER_H
