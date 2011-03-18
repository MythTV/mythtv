// -*- Mode: c++ -*-

#ifndef _HTTP_HANDLER_H_
#define _HTTP_HANDLER_H_

// C++ headers
#include <deque>
using namespace std;

// Qt headers
#include <QMutex>
#include <QUrl>

typedef deque<QUrl> UrlQueue;

class QHttp;
class QHttpResponseHeader;
class MythHttpPool;

class MythHttpHandler : public QObject
{
    Q_OBJECT
  public:
    MythHttpHandler(MythHttpPool *pool);
    virtual void deleteLater(void) { TeardownAll(); QObject::deleteLater(); }

    bool HasPendingRequests(void) const;
    void AddUrlRequest(const QUrl &url);
    void RemoveUrlRequest(const QUrl &url);

  protected slots:
    void Done(bool error);
    void ResponseHeaderReceived(const QHttpResponseHeader &resp);
    void RequestFinished(int id, bool error);
    void RequestStarted(int id);
    void StateChanged(int state);

  private:
    ~MythHttpHandler() { TeardownAll(); }
    void TeardownAll();
    void Get(const QUrl &url);

  private:
    mutable QMutex m_lock;
    UrlQueue       m_urls;
    QUrl           m_cur_url;
    uint           m_cur_status_id;
    QString        m_cur_status_str;
    int            m_cur_get_id;
    uint           m_cur_redirect_cnt;
    MythHttpPool  *m_pool;
    QHttp         *m_qhttp;

    static const uint kMaxRedirectCount;
};

#endif // _HTTP_HANDLER_H_
