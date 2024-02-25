// -*- Mode: c++ -*-

#ifndef SATIPSTREAMHANDLER_H
#define SATIPSTREAMHANDLER_H

// Qt headers
#include <QString>
#include <QStringList>
#include <QRecursiveMutex>
#include <QMap>

// MythTV headers
#include "dtvconfparserhelpers.h"
#include "dtvmultiplex.h"
#include "mpeg/mpegstreamdata.h"
#include "satiprtsp.h"
#include "streamhandler.h"

class SatIPDataReadHelper;
class SatIPControlReadHelper;

class SatIPStreamHandler : public StreamHandler
{
    friend class SatIPDataReadHelper;

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
    ~SatIPStreamHandler() override;

    bool Open(void);
    void Close(void);

    void run(void) override; // MThread

    // For implementing Get & Return
    static QMap<QString, SatIPStreamHandler*> s_handlers;
    static QMap<QString, uint>                s_handlersRefCnt;
    static QMutex                             s_handlersLock;

  public:
    int          m_inputId        {0};
    int          m_satipsrc       {1};

  private:
    DTVTunerType m_tunerType;
    QString      m_device;
    uint         m_frontend       {UINT_MAX};
    QUrl         m_baseurl;
    QUrl         m_tuningurl;
    QUrl         m_oldtuningurl;
    bool         m_setupinvoked   {false};
    QRecursiveMutex m_tunelock;
    QStringList  m_oldpids;

  public:
    QUdpSocket  *m_dsocket        {nullptr};    // RTP data socket
    QUdpSocket  *m_csocket        {nullptr};    // RTCP control socket
    ushort       m_dport          {0};          // RTP data port          Note: this is m_dsocket->localPort()
    ushort       m_cport          {0};          // RTCP control port      Note: this is m_csocket->localPort()

  public:
    SatIPRTSP               *m_rtsp               {nullptr};
    SatIPDataReadHelper     *m_dataReadHelper     {nullptr};
    SatIPControlReadHelper  *m_controlReadHelper  {nullptr};

  public:
    bool HasLock();
    int  GetSignalStrength();
    void SetSigmonValues(bool lock, int level);

  private:
    QMutex m_sigmonLock;
    bool m_hasLock          {false};
    int  m_signalStrength   {0};

  public:
    static uint  GetUDPReceiveBufferSize(QUdpSocket *socket);
    static uint  SetUDPReceiveBufferSize(QUdpSocket *socket, uint rcvbuffersize);
};

// --- SatIPDataReadHelper ---------------------------------------------------

class SatIPDataReadHelper : public QObject
{
    Q_OBJECT

  public:
    explicit SatIPDataReadHelper(SatIPStreamHandler *handler);
    ~SatIPDataReadHelper() override;

  public slots:
    void ReadPending(void);

  private:
    SatIPStreamHandler *m_streamHandler   {nullptr};
    QUdpSocket         *m_socket          {nullptr};
    int                 m_timer           {0};
    uint                m_sequenceNumber  {0};
    uint                m_count           {0};
    bool                m_valid           {false};
};

// --- SatIPControlReadHelper ------------------------------------------------

class SatIPControlReadHelper : public QObject
{
    Q_OBJECT

  public:
    explicit SatIPControlReadHelper(SatIPStreamHandler *handler);
    ~SatIPControlReadHelper() override;

  public slots:
    void ReadPending(void);

  private:
    SatIPStreamHandler *m_streamHandler   {nullptr};
    QUdpSocket         *m_socket          {nullptr};
};

#endif // SATIPSTREAMHANDLER_H
