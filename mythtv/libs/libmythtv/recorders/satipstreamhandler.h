// -*- Mode: c++ -*-

#ifndef _SATIPSTREAMHANDLER_H_
#define _SATIPSTREAMHANDLER_H_

// Qt headers
#include <QString>
#include <QStringList>
#include <QMutex>
#include <QMap>

// MythTV headers
#include "mpegstreamdata.h"
#include "streamhandler.h"
#include "dtvmultiplex.h"
#include "dtvconfparserhelpers.h"
#include "satiprtsp.h"

class SatIPStreamHandler;
class DTVSignalMonitor;
class SatIPChannel;
class DeviceReadBuffer;

class SatIPStreamHandler : public StreamHandler
{
    friend class SatIPRTSPWriteHelper;
    friend class SatIPRTSPReadHelper;
    friend class SatIPSignalMonitor;

  public:
    static SatIPStreamHandler *Get(const QString &devname, int inputid);
    static void Return(SatIPStreamHandler * & ref, int inputid);

    void AddListener(MPEGStreamData *data,
                     bool /*allow_section_reader*/ = false,
                     bool /*needs_drb*/            = false,
                     QString output_file = QString()) override // StreamHandler
    {
        StreamHandler::AddListener(data, false, false, output_file);
    } // StreamHandler

    bool UpdateFilters() override;  // StreamHandler
    void Tune(const DTVMultiplex &tuning);

  private:
    explicit SatIPStreamHandler(const QString & device, int inputid);

    bool Open(void);
    void Close(void);

    void run(void) override; // MThread

    // For implementing Get & Return
    static QMap<QString, SatIPStreamHandler*> s_handlers;
    static QMap<QString, uint>                s_handlersRefCnt;
    static QMutex                             s_handlersLock;

  protected:
    SatIPRTSP   *m_rtsp           {nullptr};

  public:
    int          m_inputId;

  private:
    DTVTunerType m_tunerType;
    QString      m_device;
    QUrl         m_baseurl;
    QUrl         m_tuningurl;
    QUrl         m_oldtuningurl;
    bool         m_setupinvoked   {false};
    QMutex       m_tunelock       {QMutex::Recursive};
    QStringList  m_oldpids;
};

#endif // _SATIPSTREAMHANDLER_H_
