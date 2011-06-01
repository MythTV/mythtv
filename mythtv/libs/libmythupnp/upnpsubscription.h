#ifndef UPNPSUBSCRIPTION_H
#define UPNPSUBSCRIPTION_H

#include "mythevent.h"
#include "upnp.h"

class Subscription;

class MBASE_PUBLIC MythInfoMapEvent : public MythEvent
{
  public:
    MythInfoMapEvent(const QString &lmessage,
                     const QHash<QString,QString> &linfoMap)
      : MythEvent(lmessage), infoMap(linfoMap) { }

    virtual MythInfoMapEvent *clone() const
        { return new MythInfoMapEvent(Message(), infoMap); }
    const QHash<QString,QString>* InfoMap(void) { return &infoMap; }

  private:
    QHash<QString,QString> infoMap;
};

class UPNP_PUBLIC UPNPSubscription : public HttpServerExtension, public MythObservable
{
  public:
    UPNPSubscription(const QString &share_path, int port);
    virtual ~UPNPSubscription();

    virtual QStringList GetBasePaths() { return QStringList( "/Subscriptions" ); }
    virtual bool ProcessRequest(HttpWorkerThread *pThread,
                                HTTPRequest *pRequest);

    int  Subscribe(const QString &usn, const QUrl &url, const QString &path);
    void Unsubscribe(const QString &usn);
    int  Renew(const QString &usn);
    void Remove(const QString &usn);

  private:
    static bool SendUnsubscribeRequest(const QString &usn, const QUrl &url,
                                       const QString &path, const QString &uuid);
    static int  SendSubscribeRequest(const QString &callback,
                                     const QString &usn, const QUrl &url,
                                     const QString &path, const QString &uuidin,
                                     QString &uuidout);
  private:
    QHash<QString, Subscription*> m_subscriptions;
    QMutex  m_subscriptionLock;
    QString m_callback;
};

#endif // UPNPSUBSCRIPTION_H
