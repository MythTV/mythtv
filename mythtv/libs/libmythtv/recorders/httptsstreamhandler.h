#ifndef HTTPTSSTREAMHANDLER_H
#define HTTPTSSTREAMHANDLER_H

#include <vector>

#include <QString>
#include <QMutex>
#include <QMap>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>

#include "channelutil.h"
#include "iptvstreamhandler.h"

class HTTPReader;

class HTTPTSStreamHandler : public IPTVStreamHandler
{
    friend class HTTPReader;
  public:
    static HTTPTSStreamHandler* Get(const IPTVTuningData& tuning, int inputid);
    static void Return(HTTPTSStreamHandler * & ref, int inputid);

protected:
    explicit HTTPTSStreamHandler(const IPTVTuningData &tuning, int inputid);
    ~HTTPTSStreamHandler(void) override;
    void run(void) override; // MThread

  protected:
    HTTPReader*             m_reader {nullptr};
    // for implementing Get & Return
    static QMutex                               s_httphandlers_lock;
    static QMap<QString, HTTPTSStreamHandler*>  s_httphandlers;
    static QMap<QString, uint>                  s_httphandlers_refcnt;
};


class MTV_PUBLIC HTTPReader : public QObject
{
    Q_OBJECT

  public:
    explicit HTTPReader(HTTPTSStreamHandler* parent) :
        m_parent(parent) {}
    void Cancel(void);
    bool DownloadStream(const QUrl& url);

  protected:
    void ReadBytes();
    void WriteBytes();

  private slots:
    void HttpRead();

  private:
    QString                 m_url;
    HTTPTSStreamHandler    *m_parent     {nullptr};
    QTimer                  m_timer;
    QNetworkAccessManager   m_mgr;
    QNetworkReply          *m_reply      {nullptr};
    QMutex                  m_lock;
    QMutex                  m_replylock;
    QMutex                  m_bufferlock;
    uint8_t                *m_buffer     {nullptr};
    bool                    m_ok         {true};
    qint64                  m_size       {0};
};

#endif // HTTPTSSTREAMHANDLER_H
