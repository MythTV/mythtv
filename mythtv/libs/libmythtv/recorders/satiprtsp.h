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

// --- SatIPRTSP -------------------------------------------------------------

class SatIPRTSP : public QObject
{
    Q_OBJECT

  public:
    explicit SatIPRTSP(int m_inputId);
    ~SatIPRTSP() override;

    bool Setup(const QUrl& url, ushort clientPort1, ushort clientPort2);
    bool Play(const QString &pids_str);
    bool Teardown();

  protected:
    void timerEvent(QTimerEvent* timerEvent) override;   // QObject

  signals:
    void StartKeepAlive(void);
    void StopKeepAlive(void);

  protected slots:
    void StartKeepAliveRequested(void);
    void StopKeepAliveRequested(void);

  private:
    bool sendMessage(const QString& msg, QStringList* additionalHeaders = nullptr);

  private:
    int                       m_inputId          {0};
    QUrl                      m_requestUrl;
    uint                      m_cseq             {0};
    QString                   m_sessionid;
    QString                   m_streamid;
    QMap<QString, QString>    m_responseHeaders;

    int                       m_timer            {0};
    std::chrono::seconds      m_timeout          {60s};

    static QMutex             s_rtspMutex;
};

#endif // SATIPRTSP_H
