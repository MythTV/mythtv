// -*- Mode: c++ -*-

#ifndef ASISTREAMHANDLER_H
#define ASISTREAMHANDLER_H

#include <vector>

#include <QString>
#include <QMutex>
#include <QMap>

#include "libmythbase/mythdate.h"
#include "streamhandler.h"

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
                     const QString& output_file    = QString()) override // StreamHandler
    {
        StreamHandler::AddListener(data, false, true, output_file);
    }

    void SetClockSource(ASIClockSource cs);
    void SetRXMode(ASIRXMode m);

  private:
    explicit ASIStreamHandler(const QString &device, int inputid);

    bool Open(void);
    void Close(void);

    void run(void) override; // MThread

    void PriorityEvent(int fd) override; // DeviceReaderCB

    void SetRunningDesired(bool desired) override; // StreamHandler

  private:
    int               m_deviceNum    {-1};
    int               m_bufSize      {-1};
    int               m_numBuffers   {-1};
    int               m_fd           {-1};
    uint              m_packetSize   {TSPacket::kSize};
    ASIClockSource    m_clockSource  {kASIInternalClock};
    ASIRXMode         m_rxMode       {kASIRXSyncOnActualConvertTo188};
    DeviceReadBuffer *m_drb          {nullptr};

    // for implementing Get & Return
    static QMutex                           s_handlersLock;
    static QMap<QString, ASIStreamHandler*> s_handlers;
    static QMap<QString, uint>              s_handlersRefCnt;
};

#endif // ASISTREAMHANDLER_H
