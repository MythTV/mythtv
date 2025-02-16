/*
An HttpServer Extension that manages subscriptions to UPnP services.

An object wishing to subscribe to a service needs to register as a listener for
events and subscribe using a valid usn and subscription path. The subscriber
is responsible for requesting a renewal before the subscription expires,
removing any stale subscriptions, unsubsubscribing on exit and must re-implement
QObject::customEvent to receive event notifications for subscribed services.
*/

#include "upnpsubscription.h"

#if QT_VERSION >= QT_VERSION_CHECK(6,0,0)
#include <QStringConverter>
#else
#include <QTextCodec>
#endif
#include <utility>

#include <QHostAddress>
#include <QString>
#include <QUrl>
#include <QDomDocument>

#include "libmythbase/mythlogging.h"
#include "libmythbase/mythtypes.h"

#include "bufferedsocketdevice.h"
#include "upnp.h"

// default requested time for subscription (actual is dictated by server)
static constexpr uint16_t SUBSCRIPTION_TIME { 1800 };
// maximum time to wait for responses to subscription requests (UPnP spec. 30s)
static constexpr std::chrono::milliseconds MAX_WAIT { 30s };

#define LOC QString("UPnPSub: ")

class Subscription
{
  public:
    Subscription(QUrl url, QString path)
      : m_url(std::move(url)), m_path(std::move(path)) { }
    QUrl    m_url;
    QString m_path;
    QString m_uuid;
};

UPNPSubscription::UPNPSubscription(const QString &share_path, int port)
  : HttpServerExtension("UPnPSubscriptionManager", share_path)
{
    m_nSupportedMethods = (uint)RequestTypeNotify; // Only NOTIFY supported

    QHostAddress addr;
    if (!UPnp::g_IPAddrList.isEmpty())
        addr = UPnp::g_IPAddrList.at(0);

    QString host;
    if (addr.protocol() == QAbstractSocket::IPv6Protocol)
        host = "[" + addr.toString() + "]";
    else
        host = addr.toString();

    m_callback = QString("http://%1:%2/Subscriptions/event?usn=")
        .arg(host, QString::number(port));
}

UPNPSubscription::~UPNPSubscription()
{
    m_subscriptionLock.lock();
    QList<QString> usns = m_subscriptions.keys();
    while (!usns.isEmpty())
        Unsubscribe(usns.takeLast());
    m_subscriptions.clear();
    m_subscriptionLock.unlock();

    LOG(VB_UPNP, LOG_DEBUG, LOC + "Finished");
}

std::chrono::seconds UPNPSubscription::Subscribe(const QString &usn, const QUrl &url,
                                const QString &path)
{
    LOG(VB_UPNP, LOG_DEBUG, LOC + QString("Subscribe %1 %2 %3")
        .arg(usn, url.toString(), path));

    // N.B. this is called from the client object's thread. Hence we have to
    // lock until the subscription request has returned, otherwise we may
    // receive the first event notification (in the HttpServer thread)
    // before the subscription is processed and the event will fail

    QMutexLocker lock(&m_subscriptionLock);
    if (m_subscriptions.contains(usn))
    {
        if (m_subscriptions[usn]->m_url != url ||
            m_subscriptions[usn]->m_path != path)
        {
            LOG(VB_GENERAL, LOG_WARNING, LOC +
                "Re-subscribing with different url and path.");
            m_subscriptions[usn]->m_url  = url;
            m_subscriptions[usn]->m_path = path;
            m_subscriptions[usn]->m_uuid = QString();
        }
    }
    else
    {
        m_subscriptions.insert(usn, new Subscription(url, path));
    }

    return SendSubscribeRequest(m_callback, usn, url, path, QString(),
                                m_subscriptions[usn]->m_uuid);
}

void UPNPSubscription::Unsubscribe(const QString &usn)
{
    QUrl url;
    QString path;
    QString uuid = QString();
    m_subscriptionLock.lock();
    if (m_subscriptions.contains(usn))
    {
        url  = m_subscriptions[usn]->m_url;
        path = m_subscriptions[usn]->m_path;
        uuid = m_subscriptions[usn]->m_uuid;
        delete m_subscriptions.value(usn);
        m_subscriptions.remove(usn);
    }
    m_subscriptionLock.unlock();

    if (!uuid.isEmpty())
        SendUnsubscribeRequest(usn, url, path, uuid);
}

std::chrono::seconds UPNPSubscription::Renew(const QString &usn)
{
    LOG(VB_UPNP, LOG_DEBUG, LOC + QString("Renew: %1").arg(usn));

    QUrl    url;
    QString path;
    QString sid;

    // see locking comment in Subscribe
    QMutexLocker lock(&m_subscriptionLock);
    if (m_subscriptions.contains(usn))
    {
        url  = m_subscriptions[usn]->m_url;
        path = m_subscriptions[usn]->m_path;
        sid  = m_subscriptions[usn]->m_uuid;
    }
    else
    {
        LOG(VB_UPNP, LOG_ERR, LOC + QString("Unrecognised renewal usn: %1")
            .arg(usn));
        return 0s;
    }

    if (!sid.isEmpty())
    {
        return SendSubscribeRequest(m_callback, usn, url, path, sid,
                                    m_subscriptions[usn]->m_uuid);
    }

    LOG(VB_UPNP, LOG_ERR, LOC + QString("No uuid - not renewing usn: %1")
             .arg(usn));
    return 0s;
}

void UPNPSubscription::Remove(const QString &usn)
{
    // this could be handled by hooking directly into the SSDPCache updates
    // but the subscribing object will also be doing so. Having the that
    // object initiate the removal avoids temoporary race conditions during
    // periods of UPnP/SSDP activity
    m_subscriptionLock.lock();
    if (m_subscriptions.contains(usn))
    {
        LOG(VB_UPNP, LOG_INFO, LOC + QString("Removing %1").arg(usn));
        delete m_subscriptions.value(usn);
        m_subscriptions.remove(usn);
    }
    m_subscriptionLock.unlock();
}

bool UPNPSubscription::ProcessRequest(HTTPRequest *pRequest)
{
    if (!pRequest)
        return false;

    if (pRequest->m_sBaseUrl != "/Subscriptions")
        return false;
    if (pRequest->m_sMethod != "event")
        return false;

    LOG(VB_UPNP, LOG_DEBUG, LOC + QString("%1\n%2")
        .arg(pRequest->m_sRawRequest, pRequest->m_sPayload));

    if (pRequest->m_sPayload.isEmpty())
        return true;

    pRequest->m_eResponseType = ResponseTypeHTML;

    QString nt  = pRequest->GetLastHeader("nt");
    QString nts = pRequest->GetLastHeader("nts");
    bool    no  = (pRequest->m_eType == RequestTypeNotify);

    if (nt.isEmpty() || nts.isEmpty() || !no)
    {
        pRequest->m_nResponseStatus = 400;
        return true;
    }

    pRequest->m_nResponseStatus = 412;
    if (nt != "upnp:event" || nts != "upnp:propchange")
        return true;

    QString usn = pRequest->m_mapParams["usn"];
    QString sid = pRequest->GetLastHeader("sid");
    if (usn.isEmpty() || sid.isEmpty())
        return true;

    // N.B. Validating the usn and uuid here might mean blocking for some time
    // while waiting for a subscription to complete. While this operates in a
    // worker thread, worker threads are a limited resource which we could
    // rapidly overload if a number of events arrive. Instead let the
    // subscribing objects validate the usn - the uuid should be superfluous.

    QString seq = pRequest->GetLastHeader("seq");

    // mediatomb sends some extra character(s) at the end of the payload
    // which throw Qt, so try and trim them off
    int loc = pRequest->m_sPayload.lastIndexOf("propertyset>");
    QString payload = (loc > -1) ? pRequest->m_sPayload.left(loc + 12) :
                                   pRequest->m_sPayload;

    LOG(VB_UPNP, LOG_DEBUG, LOC + QString("Payload:\n%1").arg(payload));

    pRequest->m_nResponseStatus = 400;
    QDomDocument body;
#if QT_VERSION < QT_VERSION_CHECK(6,5,0)
    QString error;
    int errorCol = 0;
    int errorLine = 0;
    if (!body.setContent(payload, true, &error, &errorLine, &errorCol))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + 
            QString("Failed to parse event: Line: %1 Col: %2 Error: '%3'")
                .arg(errorLine).arg(errorCol).arg(error));
        return true;
    }
#else
    auto parseResult =
        body.setContent(payload,
                        QDomDocument::ParseOption::UseNamespaceProcessing);
    if (!parseResult)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("Failed to parse event: Line: %1 Col: %2 Error: '%3'")
                .arg(parseResult.errorLine).arg(parseResult.errorColumn)
                .arg(parseResult.errorMessage));
        return true;
    }
#endif

    LOG(VB_UPNP, LOG_DEBUG, LOC + "/n/n" + body.toString(4) + "/n/n");

    QDomNodeList properties = body.elementsByTagName("property");
    InfoMap results;

    // this deals with both one argument per property (compliant) and mutliple
    // arguments per property as sent by mediatomb
    for (int i = 0; i < properties.size(); i++)
    {
        QDomNodeList arguments = properties.at(i).childNodes();
        for (int j = 0; j < arguments.size(); j++)
        {
            QDomElement e = arguments.at(j).toElement();
            if (!e.isNull() && !e.text().isEmpty() && !e.tagName().isEmpty())
                results.insert(e.tagName(), e.text());
        }
    }

    // using MythObservable allows multiple objects to subscribe to the same
    // service but is less efficient from an eventing perspective, especially
    // if multiple objects are subscribing
    if (!results.isEmpty())
    {
        pRequest->m_nResponseStatus = 200;
        results.insert("usn", usn);
        results.insert("seq", seq);
        MythInfoMapEvent me("UPNP_EVENT", results);
        dispatch(me);
    }

    return true;
}

bool UPNPSubscription::SendUnsubscribeRequest(const QString &usn,
                                              const QUrl &url,
                                              const QString &path,
                                              const QString &uuid)
{
    bool success = false;
    QString host = url.host();
    int     port = url.port();

    QByteArray sub;
    QTextStream data(&sub);
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
    data.setCodec(QTextCodec::codecForName("UTF-8"));
#else
    data.setEncoding(QStringConverter::Utf8);
#endif
    // N.B. Play On needs an extra space between UNSUBSCRIBE and path...
    data << QString("UNSUBSCRIBE  %1 HTTP/1.1\r\n").arg(path);
    data << QString("HOST: %1:%2\r\n").arg(host, QString::number(port));
    data << QString("SID: uuid:%1\r\n").arg(uuid);
    data << "\r\n";
    data.flush();

    LOG(VB_UPNP, LOG_DEBUG, LOC + "\n\n" + sub);

    auto *sockdev = new MSocketDevice(MSocketDevice::Stream);
    auto *sock = new BufferedSocketDevice(sockdev);
    sockdev->setBlocking(true);

    if (sock->Connect(QHostAddress(host), port))
    {
        if (sock->WriteBlockDirect(sub.constData(), sub.size()) != -1)
        {
            QString line = sock->ReadLine(MAX_WAIT);
            success = !line.isEmpty();
        }
        else
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                QString("Socket write error for %1:%2") .arg(host).arg(port));
        }
        sock->Close();
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("Failed to open socket for %1:%2") .arg(host).arg(port));
    }

    delete sock;
    delete sockdev;
    if (success)
        LOG(VB_GENERAL, LOG_INFO, LOC + QString("Unsubscribed to %1").arg(usn));
    else
        LOG(VB_UPNP, LOG_WARNING, LOC + QString("Failed to unsubscribe to %1")
                                      .arg(usn));
    return success;
}

std::chrono::seconds UPNPSubscription::SendSubscribeRequest(const QString &callback,
                                           const QString &usn,
                                           const QUrl    &url,
                                           const QString &path,
                                           const QString &uuidin,
                                           QString       &uuidout)
{
    QString host = url.host();
    int     port = url.port();

    QByteArray sub;
    QTextStream data(&sub);
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
    data.setCodec(QTextCodec::codecForName("UTF-8"));
#else
    data.setEncoding(QStringConverter::Utf8);
#endif
    // N.B. Play On needs an extra space between SUBSCRIBE and path...
    data << QString("SUBSCRIBE  %1 HTTP/1.1\r\n").arg(path);
    data << QString("HOST: %1:%2\r\n").arg(host, QString::number(port));


    if (uuidin.isEmpty()) // new subscription
    {
        data << QString("CALLBACK: <%1%2>\r\n")
            .arg(callback, usn);
        data << "NT: upnp:event\r\n";
    }
    else
    {
        // renewal
        data << QString("SID: uuid:%1\r\n").arg(uuidin);
    }

    data << QString("TIMEOUT: Second-%1\r\n").arg(SUBSCRIPTION_TIME);
    data << "\r\n";
    data.flush();

    LOG(VB_UPNP, LOG_DEBUG, LOC + "\n\n" + sub);

    auto *sockdev = new MSocketDevice(MSocketDevice::Stream);
    auto *sock = new BufferedSocketDevice(sockdev);
    sockdev->setBlocking(true);

    QString uuid;
    QString timeout;
    std::chrono::seconds result = 0s;

    if (sock->Connect(QHostAddress(host), port))
    {
        if (sock->WriteBlockDirect(sub.constData(), sub.size()) != -1)
        {
            bool ok = false;
            QString line = sock->ReadLine(MAX_WAIT);
            while (!line.isEmpty())
            {
                LOG(VB_UPNP, LOG_DEBUG, LOC + line);
                if (line.contains("HTTP/1.1 200 OK", Qt::CaseInsensitive))
                    ok = true;
                if (line.startsWith("SID:", Qt::CaseInsensitive))
                    uuid = line.mid(4).trimmed().mid(5).trimmed();
                if (line.startsWith("TIMEOUT:", Qt::CaseInsensitive))
                    timeout = line.mid(8).trimmed().mid(7).trimmed();
                if (ok && !uuid.isEmpty() && !timeout.isEmpty())
                    break;
                line = sock->ReadLine(MAX_WAIT);
            }

            if (ok && !uuid.isEmpty() && !timeout.isEmpty())
            {
                uuidout = uuid;
                result  = std::chrono::seconds(timeout.toUInt());
            }
            else
            {
                LOG(VB_GENERAL, LOG_ERR, LOC +
                    QString("Failed to subscribe to %1").arg(usn));
            }
        }
        else
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                QString("Socket write error for %1:%2") .arg(host).arg(port));
        }
        sock->Close();
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("Failed to open socket for %1:%2") .arg(host).arg(port));
    }

    delete sock;
    delete sockdev;
    return result;
}
