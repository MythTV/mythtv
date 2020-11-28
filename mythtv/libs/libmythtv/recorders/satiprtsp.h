/** -*- Mode: c++ -*-*/

#ifndef SATIPRTSP_H
#define SATIPRTSP_H

// C++ includes
#include <cstdint>

// Qt includes
#include <QObject>
#include <QMap>
#include <QString>
#include <QMutex>
#include <QUrl>
#include <QTimerEvent>
#include <QUdpSocket>
#include <QTime>

// MythTV includes
#include "mythchrono.h"
#include "packetbuffer.h"

class SatIPRTSP;
class SatIPStreamHandler;
using Headers = QMap<QString, QString>;

// --- SatIPRTSPReadHelper ---------------------------------------------------

class SatIPRTSPReadHelper : public QObject
{
    friend class SatIPRTSP;
    Q_OBJECT

  public:
    explicit SatIPRTSPReadHelper(SatIPRTSP *p);
    ~SatIPRTSPReadHelper() override;

  public slots:
    void ReadPending(void);

  protected:
    QUdpSocket *m_socket  {nullptr};

  private:
    SatIPRTSP *m_parent   {nullptr};
};

// --- SatIPRTCPReadHelper ---------------------------------------------------

class SatIPRTCPReadHelper : public QObject
{
    friend class SatIPRTSP;
    Q_OBJECT

  public:
    explicit SatIPRTCPReadHelper(SatIPRTSP *p);
    ~SatIPRTCPReadHelper() override;

  public slots:
    void ReadPending(void);

  protected:
    QUdpSocket *m_socket  {nullptr};

  private:
    SatIPRTSP *m_parent   {nullptr};
};

// --- SatIPRTSPWriteHelper --------------------------------------------------

class SatIPRTSPWriteHelper : public QObject
{
    Q_OBJECT

  public:
    SatIPRTSPWriteHelper(SatIPRTSP* parent, SatIPStreamHandler* handler);

  protected:
    void timerEvent(QTimerEvent* /*event*/) override; // QObject

  private:
    SatIPRTSP          *m_parent                      {nullptr};
    SatIPStreamHandler *m_streamHandler               {nullptr};
    int                 m_timer                       {0};
    uint                m_lastSequenceNumber          {0};
    uint                m_lastTimeStamp               {0};
    uint                m_previousLastSequenceNumber  {0};
};

// --- SatIPRTSP -------------------------------------------------------------

class SatIPRTSP : public QObject
{
    friend class SatIPRTSPReadHelper;
    friend class SatIPRTCPReadHelper;
    friend class SatIPRTSPWriteHelper;
    friend class SatIPSignalMonitor;

    Q_OBJECT

  public:
    explicit SatIPRTSP(SatIPStreamHandler* handler);
    ~SatIPRTSP() override;

    bool Setup(const QUrl& url);
    bool Play(QStringList &pids);
    bool Teardown();

    bool HasLock();
    int  GetSignalStrength();

  protected:
    void timerEvent(QTimerEvent* /*timerEvent*/) override;   // QObject
    void SetSigmonValues(bool lock, int level);

  signals:
    void startKeepAlive(std::chrono::milliseconds timeout);
    void stopKeepAlive(void);

  protected slots:
    void startKeepAliveRequested(std::chrono::milliseconds timeout);
    void stopKeepAliveRequested(void);

  protected:
    QUrl m_requestUrl;
    PacketBuffer *m_buffer  {nullptr};

  private:
    bool sendMessage(const QUrl& url, const QString& msg, QStringList* additionalHeaders = nullptr);

  private:
    const SatIPStreamHandler *m_streamHandler {nullptr};

    uint    m_cseq        {0};
    QString m_sessionid;
    QString m_streamid;
    Headers m_headers;

    int   m_timer         {0};
    std::chrono::milliseconds m_timeout {0ms};

    QMutex m_ctrlSocketLock;
    QMutex m_sigmonLock;

    bool m_valid            {false};
    bool m_validOld         {false};
    bool m_hasLock          {false};
    int  m_signalStrength   {0};

    SatIPRTSPReadHelper  *m_readHelper        {nullptr};
    SatIPRTSPWriteHelper *m_writeHelper       {nullptr};
    SatIPRTCPReadHelper  *m_rtcpReadHelper    {nullptr};
};

#endif // SATIPRTSP_H
