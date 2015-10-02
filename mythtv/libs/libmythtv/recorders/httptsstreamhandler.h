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

protected:
    HTTPTSStreamHandler(const IPTVTuningData &tuning);
    virtual ~HTTPTSStreamHandler(void);
    virtual void run(void);

  protected:
    HTTPReader*             m_reader;
    // for implementing Get & Return
    static QMutex                               s_handlers_lock;
    static QMap<QString, HTTPTSStreamHandler*>  s_handlers;
    static QMap<QString, uint>                  s_handlers_refcnt;
};


class MBASE_PUBLIC HTTPReader : public QObject
{
    Q_OBJECT

  public:
    HTTPReader(HTTPTSStreamHandler* parent) : m_parent(parent){}
    void Cancel(void);
    bool DownloadStream(const QUrl url);

  protected:
    void ReadBytes();
    void WriteBytes();

  private slots:
    void HttpRead();

  private:
    HTTPTSStreamHandler     *m_parent;
    QTimer                  m_timer;
    QNetworkAccessManager   m_mgr;
    QNetworkReply           *m_reply;
    QMutex                  m_lock;
    QMutex                  m_replylock;
    QMutex                  m_bufferlock;
    uint8_t                 *m_buffer;
    bool                    m_ok;
    int                     size;
};

#endif // _HTTPTSSTREAMHANDLER_H_
