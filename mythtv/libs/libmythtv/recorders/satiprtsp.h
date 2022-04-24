/** -*- Mode: c++ -*-*/

#ifndef SATIPRTSP_H
#define SATIPRTSP_H

// C++ includes
#include <cstdint>

// Qt includes
#include <QObject>
#include <QMap>
#include <QMutex>
#include <QString>
#include <QTcpSocket>
#include <QTime>
#include <QTimerEvent>
#include <QUdpSocket>
#include <QUrl>

// MythTV includes
#include "libmythbase/mythchrono.h"

#include "recorders/rtp/packetbuffer.h"

class SatIPRTSP;
class SatIPStreamHandler;

// --- SatIPReadHelper -------------------------------------------------------

class SatIPReadHelper : public QObject
{
    friend class SatIPRTSP;
    Q_OBJECT

  public:
    explicit SatIPReadHelper(SatIPRTSP *p);
    ~SatIPReadHelper() override;

  public slots:
    void ReadPending(void);

  protected:
    QUdpSocket *m_socket  {nullptr};

  private:
    SatIPRTSP *m_parent   {nullptr};
};

// --- SatIPWriteHelper ------------------------------------------------------

class SatIPWriteHelper : public QObject
{
    Q_OBJECT

  public:
    explicit SatIPWriteHelper(SatIPRTSP* parent, SatIPStreamHandler* handler);
    ~SatIPWriteHelper() override;

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

// --- SatIPControlReadHelper ------------------------------------------------

class SatIPControlReadHelper : public QObject
{
    friend class SatIPRTSP;
    Q_OBJECT

  public:
    explicit SatIPControlReadHelper(SatIPRTSP *p);
    ~SatIPControlReadHelper() override;

  public slots:
    void ReadPending(void);

  protected:
    QUdpSocket *m_socket  {nullptr};

  private:
    SatIPRTSP *m_parent   {nullptr};
};

// --- SatIPRTSP -------------------------------------------------------------

class SatIPRTSP : public QObject
{
    friend class SatIPSignalMonitor;
    friend class SatIPReadHelper;
    friend class SatIPWriteHelper;
    friend class SatIPControlReadHelper;

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
    QMap<QString, QString> m_headers;

    int   m_timer         {0};
    std::chrono::milliseconds m_timeout {0ms};

    QMutex m_ctrlSocketLock;
    QMutex m_sigmonLock;

    bool m_valid            {false};
    bool m_validOld         {false};
    bool m_hasLock          {false};
    int  m_signalStrength   {0};

    SatIPReadHelper         *m_readHelper        {nullptr};
    SatIPWriteHelper        *m_writeHelper       {nullptr};
    SatIPControlReadHelper  *m_controlReadHelper {nullptr};
};

#endif // SATIPRTSP_H
