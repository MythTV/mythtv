// -*- Mode: c++ -*-

#ifndef _ASISTREAMHANDLER_H_
#define _ASISTREAMHANDLER_H_

#include <vector>
using namespace std;

#include <QString>
#include <QMutex>
#include <QMap>

#include "streamhandler.h"
#include "mythdate.h"

class ASIStreamHandler;
class DTVSignalMonitor;
class ASIChannel;
class DeviceReadBuffer;

enum ASIClockSource
{
    kASIInternalClock         = 0,
    kASIExternalClock         = 1,
    kASIRecoveredReceiveClock = 2,
    kASIExternalClock2        = 1,
};

enum ASIRXMode
{
    kASIRXRawMode                  = 0,
    kASIRXSyncOn188                = 1,
    kASIRXSyncOn204                = 2,
    kASIRXSyncOnActualSize         = 3,
    kASIRXSyncOnActualConvertTo188 = 4,
    kASIRXSyncOn204ConvertTo188    = 5,
};


//#define RETUNE_TIMEOUT 5000

// Note : This class always uses a DRB && a TS reader.

class ASIStreamHandler : public StreamHandler
{
  public:
    static ASIStreamHandler *Get(const QString &devname, int inputid);
    static void Return(ASIStreamHandler * & ref, int inputid);

    void AddListener(MPEGStreamData *data,
                     bool /*allow_section_reader*/ = false,
                     bool /*needs_drb*/            = false,
                     QString output_file       = QString()) override // StreamHandler
    {
        StreamHandler::AddListener(data, false, true, output_file);
    }

    void SetClockSource(ASIClockSource cs);
    void SetRXMode(ASIRXMode m);

  private:
    explicit ASIStreamHandler(const QString &, int inputid);

    bool Open(void);
    void Close(void);

    void run(void) override; // MThread

    void PriorityEvent(int fd) override; // DeviceReaderCB

    void SetRunningDesired(bool desired) override; // StreamHandler

  private:
    int               m_device_num   {-1};
    int               m_buf_size     {-1};
    int               m_num_buffers  {-1};
    int               m_fd           {-1};
    uint              m_packet_size  {TSPacket::kSize};
    ASIClockSource    m_clock_source {kASIInternalClock};
    ASIRXMode         m_rx_mode      {kASIRXSyncOnActualConvertTo188};
    DeviceReadBuffer *m_drb          {nullptr};

    // for implementing Get & Return
    static QMutex                           s_handlers_lock;
    static QMap<QString, ASIStreamHandler*> s_handlers;
    static QMap<QString, uint>              s_handlers_refcnt;
};

#endif // _ASISTREAMHANDLER_H_
