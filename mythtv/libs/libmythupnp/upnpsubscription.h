#ifndef UPNPSUBSCRIPTION_H
#define UPNPSUBSCRIPTION_H

#include "mythevent.h"
#include "upnp.h"

class Subscription;

class UPNP_PUBLIC UPNPSubscription : public HttpServerExtension, public MythObservable
{
  public:
    UPNPSubscription(const QString &share_path, int port);
    ~UPNPSubscription() override;

    QStringList GetBasePaths() override // HttpServerExtension
        { return QStringList( "/Subscriptions" ); }
    bool ProcessRequest(HTTPRequest *pRequest) override; // HttpServerExtension

    int  Subscribe(const QString &usn, const QUrl &url, const QString &path);
    void Unsubscribe(const QString &usn);
    int  Renew(const QString &usn);
    void Remove(const QString &usn);

  private:
    Q_DISABLE_COPY(UPNPSubscription)
    static bool SendUnsubscribeRequest(const QString &usn, const QUrl &url,
                                       const QString &path, const QString &uuid);
    static int  SendSubscribeRequest(const QString &callback,
                                     const QString &usn, const QUrl &url,
                                     const QString &path, const QString &uuidin,
                                     QString &uuidout);
  private:
    QHash<QString, Subscription*> m_subscriptions;
    QMutex  m_subscriptionLock {QMutex::Recursive};
    QString m_callback         {"NOTSET"};
};

#endif // UPNPSUBSCRIPTION_H
