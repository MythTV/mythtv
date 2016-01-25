#ifndef _HTTPTSSTREAMHANDLER_H_
#define _HTTPTSSTREAMHANDLER_H_

#include <vector>
using namespace std;

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
    static HTTPTSStreamHandler* Get(const IPTVTuningData& tuning);
    static void Return(HTTPTSStreamHandler * & ref);

protected:
    HTTPTSStreamHandler(const IPTVTuningData &tuning);
    virtual ~HTTPTSStreamHandler(void);
    virtual void run(void);

  protected:
    HTTPReader*             m_reader;
    // for implementing Get & Return
    static QMutex                               s_httphandlers_lock;
    static QMap<QString, HTTPTSStreamHandler*>  s_httphandlers;
    static QMap<QString, uint>                  s_httphandlers_refcnt;
};


class MTV_PUBLIC HTTPReader : public QObject
{
    Q_OBJECT

  public:
    HTTPReader(HTTPTSStreamHandler* parent) :
        m_parent(parent), m_reply(NULL), m_buffer(NULL), m_ok(true), m_size(0) {}
    void Cancel(void);
    bool DownloadStream(const QUrl url);

  protected:
    void ReadBytes();
    void WriteBytes();

  private slots:
    void HttpRead();

  private:
    QString                 m_url;
    HTTPTSStreamHandler    *m_parent;
    QTimer                  m_timer;
    QNetworkAccessManager   m_mgr;
    QNetworkReply          *m_reply;
    QMutex                  m_lock;
    QMutex                  m_replylock;
    QMutex                  m_bufferlock;
    uint8_t                *m_buffer;
    bool                    m_ok;
    int                     m_size;
};

#endif // _HTTPTSSTREAMHANDLER_H_
