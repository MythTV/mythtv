#ifndef UPNPSUBSCRIPTION_H
#define UPNPSUBSCRIPTION_H

#include <chrono>

#include <QHash>
#include <QRecursiveMutex>
#include <QString>
#include <QStringList>
#include <QUrl>

#include "libmythbase/mythevent.h"
#include "libmythbase/mythobservable.h"

#include "libmythupnp/httpserver.h"

class Subscription;

class UPNP_PUBLIC UPNPSubscription : public HttpServerExtension, public MythObservable
{
  public:
    UPNPSubscription(const QString &share_path, int port);
    ~UPNPSubscription() override;

    QStringList GetBasePaths() override // HttpServerExtension
        { return QStringList( "/Subscriptions" ); }
    bool ProcessRequest(HTTPRequest *pRequest) override; // HttpServerExtension

    std::chrono::seconds Subscribe(const QString &usn, const QUrl &url, const QString &path);
    void Unsubscribe(const QString &usn);
    std::chrono::seconds Renew(const QString &usn);
    void Remove(const QString &usn);

  private:
    Q_DISABLE_COPY(UPNPSubscription)
    static bool SendUnsubscribeRequest(const QString &usn, const QUrl &url,
                                       const QString &path, const QString &uuid);
    static std::chrono::seconds SendSubscribeRequest(const QString &callback,
                                     const QString &usn, const QUrl &url,
                                     const QString &path, const QString &uuidin,
                                     QString &uuidout);
  private:
    QHash<QString, Subscription*> m_subscriptions;
    QRecursiveMutex  m_subscriptionLock;
    QString m_callback         {"NOTSET"};
};

#endif // UPNPSUBSCRIPTION_H
