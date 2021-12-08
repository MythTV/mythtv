// -*- Mode: c++ -*-

#ifndef SATIPSTREAMHANDLER_H
#define SATIPSTREAMHANDLER_H

// Qt headers
#include <QString>
#include <QStringList>
#if QT_VERSION < QT_VERSION_CHECK(5,14,0)
#include <QMutex>
#else
#include <QRecursiveMutex>
#endif
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
    friend class SatIPSignalMonitor;
    friend class SatIPWriteHelper;
    friend class SatIPReadHelper;

  public:
    static SatIPStreamHandler *Get(const QString &devname, int inputid);
    static void Return(SatIPStreamHandler * & ref, int inputid);

    void AddListener(MPEGStreamData *data,
                     bool /*allow_section_reader*/ = false,
                     bool /*needs_drb*/            = false,
                     const QString& output_file    = QString()) override // StreamHandler
    {
        StreamHandler::AddListener(data, false, false, output_file);
    } // StreamHandler

    bool UpdateFilters() override;  // StreamHandler
    bool Tune(const DTVMultiplex &tuning);

  private:
    explicit SatIPStreamHandler(const QString & device, int inputid);

    bool Open(void);
    void Close(void);

    void run(void) override; // MThread

    // For implementing Get & Return
    static QMap<QString, SatIPStreamHandler*> s_handlers;
    static QMap<QString, uint>                s_handlersRefCnt;
    static QMutex                             s_handlersLock;

  public:
    int          m_inputId        {0};

  private:
    DTVTunerType m_tunerType;
    QString      m_device;
    uint         m_frontend       {UINT_MAX};
    QUrl         m_baseurl;
    QUrl         m_tuningurl;
    QUrl         m_oldtuningurl;
    bool         m_setupinvoked   {false};
#if QT_VERSION < QT_VERSION_CHECK(5,14,0)
    QMutex       m_tunelock       {QMutex::Recursive};
#else
    QRecursiveMutex m_tunelock;
#endif
    QStringList  m_oldpids;

  protected:
    SatIPRTSP   *m_rtsp           {nullptr};
};

#endif // SATIPSTREAMHANDLER_H
