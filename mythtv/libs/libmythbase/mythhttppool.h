#ifndef _HTTP_POOL_H_
#define _HTTP_POOL_H_

// C++ headers
#include <set>
#include <deque>
using namespace std;

// Qt headers
#include <QString>
#include <QMutex>
#include <QHttp>
#include <QUrl>

// MythTV headers
#include "mythexp.h"

class MPUBLIC MythHttpListener
{
  public:
    virtual void Update(QHttp::Error      error,
                        const QString    &error_str,
                        const QUrl       &url,
                        uint              http_status_id,
                        const QString    &http_status_str,
                        const QByteArray &data) = 0;
    virtual ~MythHttpListener() { }
};

class HttpRequest
{
  public:
    HttpRequest(const QUrl &url, MythHttpListener *listener) :
        m_url(url), m_listener(listener) {}

    QUrl              m_url;
    MythHttpListener *m_listener;
};

class MythHttpHandler;
typedef QMap<QString,MythHttpHandler*> HostToHandler;
typedef QMultiMap<QUrl,MythHttpListener*> UrlToListener;

class MPUBLIC MythHttpPool
{
  public:
    MythHttpPool(uint max_connections = 20);
    ~MythHttpPool();

    void AddUrlRequest(const QUrl &url, MythHttpListener *listener);
    void RemoveUrlRequest(const QUrl &url, MythHttpListener *listener);
    void RemoveListener(MythHttpListener *listener);

    static MythHttpPool *GetSingleton();

    void Update(QHttp::Error      error,
                const QString    &error_str,
                const QUrl       &url,
                uint              http_status_id,
                const QString    &http_status_str,
                const QByteArray &data);

    void Done(const QString &host, MythHttpHandler *handler);

  private:
    mutable QMutex         m_lock;
    uint                   m_maxConnections;
    deque<HttpRequest>     m_httpQueue;
    set<MythHttpListener*> m_listeners;
    UrlToListener          m_urlToListener;
    HostToHandler          m_hostToHandler;

    static MythHttpPool *singleton;
};

#endif // _HTTP_POOL_H_
