#include <QCoreApplication>
#include <QTextCodec>

#include "mythcorecontext.h"
#include "mythlogging.h"
#include "ssdp.h"
#include "upnpscanner.h"

#define LOC QString("UPnPScan: ")
#define ERR QString("UPnPScan error: ")

#define MAX_ATTEMPTS 5
#define MAX_REQUESTS 1

QString MediaServerItem::NextUnbrowsed(void)
{
    // items don't need scanning
    if (!m_url.isEmpty())
        return QString();

    // scan this container
    if (!m_scanned)
        return m_id;

    // scan children
    QMutableMapIterator<QString,MediaServerItem> it(m_children);
    while (it.hasNext())
    {
        it.next();
        QString result = it.value().NextUnbrowsed();
        if (!result.isEmpty())
            return result;
    }

    return QString();
}

MediaServerItem* MediaServerItem::Find(QString &id)
{
    if (m_id == id)
        return this;

    QMutableMapIterator<QString,MediaServerItem> it(m_children);
    while (it.hasNext())
    {
        it.next();
        MediaServerItem* result = it.value().Find(id);
        if (result)
            return result;
    }

    return NULL;
}

bool MediaServerItem::Add(MediaServerItem &item)
{
    if (m_id == item.m_parentid)
    {
        m_children.insert(item.m_id, item);
        return true;
    }
    return false;
}

void MediaServerItem::Reset(void)
{
    m_children.clear();
    m_scanned = false;
}

/**
 * \class MediaServer
 *  A simple wrapper containing details about a UPnP Media Server
 */
class MediaServer : public MediaServerItem
{
  public:
    MediaServer()
     : MediaServerItem(QString("0"), QString(), QString(), QString()),
       m_URL(), m_connectionAttempts(0), m_controlURL(QUrl()),
       m_eventSubURL(QUrl()), m_eventSubPath(QString()),
       m_friendlyName(QString("Unknown")), m_subscribed(false),
       m_renewalTimerId(0), m_systemUpdateID(-1)
    {
    }
    MediaServer(QUrl URL)
     : MediaServerItem(QString("0"), QString(), QString(), QString()),
       m_URL(URL), m_connectionAttempts(0), m_controlURL(QUrl()),
       m_eventSubURL(QUrl()), m_eventSubPath(QString()),
       m_friendlyName(QString("Unknown")), m_subscribed(false),
       m_renewalTimerId(0), m_systemUpdateID(-1)
    {
    }

    bool ResetContent(int new_id)
    {
        bool result = true;
        if (m_systemUpdateID != -1)
        {
            result = false;
            Reset();
        }
        m_systemUpdateID = new_id;
        return result;
    }

    QUrl    m_URL;
    int     m_connectionAttempts;
    QUrl    m_controlURL;
    QUrl    m_eventSubURL;
    QString m_eventSubPath;
    QString m_friendlyName;
    bool    m_subscribed;
    int     m_renewalTimerId;
    int     m_systemUpdateID;
};

UPNPScanner* UPNPScanner::gUPNPScanner        = NULL;
bool         UPNPScanner::gUPNPScannerEnabled = false;
MThread*     UPNPScanner::gUPNPScannerThread  = NULL;
QMutex*      UPNPScanner::gUPNPScannerLock    = new QMutex(QMutex::Recursive);

/**
 * \class UPNPScanner
 *  UPnPScanner detects UPnP Media Servers available on the local network (via
 *  the UPnP SSDP cache), requests the device description from those devices
 *  and, if the device description is successfully parsed, will request a
 *  a subscription to the device's event control url in order to receive
 *  notifications when the available media has changed. The subscription is
 *  renewed at an appropriate time before it expires. The available media for
 *  each device can then be queried by sending browse requests as needed.
 */
UPNPScanner::UPNPScanner(UPNPSubscription *sub)
  : QObject(), m_subscription(sub), m_lock(QMutex::Recursive),
    m_network(NULL), m_updateTimer(NULL), m_watchdogTimer(NULL),
    m_masterHost(QString()), m_masterPort(0), m_scanComplete(false),
    m_fullscan(false)
{
}

UPNPScanner::~UPNPScanner()
{
    Stop();
}

/**
 * \fn UPNPScanner::Enable(bool, UPNPSubscription*)
 *  Creates or destroys the global UPNPScanner instance.
 */
void UPNPScanner::Enable(bool enable, UPNPSubscription *sub)
{
    QMutexLocker locker(gUPNPScannerLock);
    gUPNPScannerEnabled = enable;
    Instance(sub);
}

/**
 * \fn UPNPScanner::Instance(UPNPSubscription*)
 *  Returns the global UPNPScanner instance if it has been enabled or NULL
 *  if UPNPScanner is currently disabled.
 */
UPNPScanner* UPNPScanner::Instance(UPNPSubscription *sub)
{
    QMutexLocker locker(gUPNPScannerLock);
    if (!gUPNPScannerEnabled)
    {
        if (gUPNPScannerThread)
        {
            gUPNPScannerThread->quit();
            gUPNPScannerThread->wait();
        }
        delete gUPNPScannerThread;
        gUPNPScannerThread = NULL;
        delete gUPNPScanner;
        gUPNPScanner = NULL;
        return NULL;
    }

    if (!gUPNPScannerThread)
        gUPNPScannerThread = new MThread("UPnPScanner");
    if (!gUPNPScanner)
        gUPNPScanner = new UPNPScanner(sub);

    if (!gUPNPScannerThread->isRunning())
    {
        gUPNPScanner->moveToThread(gUPNPScannerThread->qthread());
        QObject::connect(
            gUPNPScannerThread->qthread(), SIGNAL(started()),
            gUPNPScanner,                  SLOT(Start()));
        gUPNPScannerThread->start(QThread::LowestPriority);
    }

    return gUPNPScanner;
}
/**
 * \fn UPNPScanner::StartFullScan
 *  Instruct the UPNPScanner thread to start a full scan of metadata from
 *  known media servers.
 */
void UPNPScanner::StartFullScan(void)
{
    m_fullscan = true;
    MythEvent *me = new MythEvent(QString("UPNP_STARTSCAN"));
    qApp->postEvent(this, me);
}

/**
 * \fn UPNPScanner::GetInitialMetadata
 *  Fill the given metadata_list and meta_dir_node with the root media
 *  server metadata (i.e. the MediaServers) and any additional metadata that
 *  that has already been scanned and cached.
 */
void UPNPScanner::GetInitialMetadata(VideoMetadataListManager::metadata_list* list,
                                     meta_dir_node *node)
{
    // nothing to see..
    QMap<QString,QString> servers = ServerList();
    if (servers.isEmpty())
        return;

    // Add MediaServers
    LOG(VB_GENERAL, LOG_INFO, QString("Adding MediaServer metadata."));

    smart_dir_node mediaservers = node->addSubDir(tr("Media Servers"));
    mediaservers->setPathRoot();

    m_lock.lock();
    QMutableHashIterator<QString,MediaServer*> it(m_servers);
    while (it.hasNext())
    {
        it.next();
        if (!it.value()->m_subscribed)
            continue;

        QString usn = it.key();
        GetServerContent(usn, it.value(), list, mediaservers.get());
    }
    m_lock.unlock();
}

/**
 * \fn UPNPScanner::GetMetadata
 *  Fill the given metadata_list and meta_dir_node with the metadata
 *  of content retrieved from known media servers. A full scan is triggered.
 */
void UPNPScanner::GetMetadata(VideoMetadataListManager::metadata_list* list,
                              meta_dir_node *node)
{
    // nothing to see..
    QMap<QString,QString> servers = ServerList();
    if (servers.isEmpty())
        return;

    // Start scanning if it isn't already running
    StartFullScan();

    // wait for the scanner to complete - with a 30 second timeout
    LOG(VB_GENERAL, LOG_INFO, LOC + "Waiting for scan to complete.");

    int count = 0;
    while (!m_scanComplete && (count++ < 300))
        usleep(100000);

    // some scans may just take too long (PlayOn)
    if (!m_scanComplete)
        LOG(VB_GENERAL, LOG_ERR, LOC + "MediaServer scan is incomplete.");
    else
        LOG(VB_GENERAL, LOG_INFO, LOC + "MediaServer scanning finished.");


    smart_dir_node mediaservers = node->addSubDir(tr("Media Servers"));
    mediaservers->setPathRoot();

    m_lock.lock();
    QMutableHashIterator<QString,MediaServer*> it(m_servers);
    while (it.hasNext())
    {
        it.next();
        if (!it.value()->m_subscribed)
            continue;

        QString usn = it.key();
        GetServerContent(usn, it.value(), list, mediaservers.get());
    }
    m_lock.unlock();
}

bool UPNPScanner::GetMetadata(QVariant &data)
{
    // we need a USN and objectID
    if (!data.canConvert(QVariant::StringList))
        return false;

    QStringList list = data.toStringList();
    if (list.size() != 2)
        return false;

    QString usn = list[0];
    QString object = list[1];

    m_lock.lock();
    bool valid = m_servers.contains(usn);
    if (valid)
    {
        MediaServerItem* item = m_servers[usn]->Find(object);
        valid = item ? !item->m_scanned : false;
    }
    m_lock.unlock();
    if (!valid)
        return false;

    MythEvent *me = new MythEvent("UPNP_BROWSEOBJECT", list);
    qApp->postEvent(this, me);

    int count = 0;
    bool found = false;
    LOG(VB_GENERAL, LOG_INFO, "START");
    while (!found && (count++ < 100)) // 10 seconds
    {
        usleep(100000);
        m_lock.lock();
        if (m_servers.contains(usn))
        {
            MediaServerItem *item = m_servers[usn]->Find(object);
            if (item)
            {
                found = item->m_scanned;
            }
            else
            {
                LOG(VB_GENERAL, LOG_INFO, QString("Item went away..."));
                found = true;
            }
        }
        else
        {
            LOG(VB_GENERAL, LOG_INFO,
                QString("Server went away while browsing."));
            found = true;
        }
        m_lock.unlock();
    }
    LOG(VB_GENERAL, LOG_INFO, "END");
    return true;
}

/**
 * \fn UPNPScanner::GetServerContent
 *  Recursively search a MediaServerItem for video metadata and add it to
 *  the metadata_list and meta_dir_node.
 */
void UPNPScanner::GetServerContent(QString &usn,
                                   MediaServerItem *content,
                                   VideoMetadataListManager::metadata_list* list,
                                   meta_dir_node *node)
{
    if (!content->m_scanned)
    {
        smart_dir_node subnode = node->addSubDir(content->m_name);

        QStringList data;
        data << usn;
        data << content->m_id;
        subnode->SetData(data);

        VideoMetadataListManager::VideoMetadataPtr item(new VideoMetadata(QString()));
        item->SetTitle(QString("Dummy"));
        list->push_back(item);
        subnode->addEntry(smart_meta_node(new meta_data_node(item.get())));
        return;
    }

    node->SetData(QVariant());

    if (content->m_url.isEmpty())
    {
        smart_dir_node container = node->addSubDir(content->m_name);
        QMutableMapIterator<QString,MediaServerItem> it(content->m_children);
        while (it.hasNext())
        {
            it.next();
            GetServerContent(usn, &it.value(), list, container.get());
        }
        return;
    }

    VideoMetadataListManager::VideoMetadataPtr item(new VideoMetadata(content->m_url));
    item->SetTitle(content->m_name);
    list->push_back(item);
    node->addEntry(smart_meta_node(new meta_data_node(item.get())));
}

/**
 * \fn UPNPScanner::ServerList(void)
 *  Returns a list of valid Media Servers discovered on the network. The
 *  returned map is a QString pair of USNs and friendly names.
 */
QMap<QString,QString> UPNPScanner::ServerList(void)
{
    QMap<QString,QString> servers;
    m_lock.lock();
    QHashIterator<QString,MediaServer*> it(m_servers);
    while (it.hasNext())
    {
        it.next();
        servers.insert(it.key(), it.value()->m_friendlyName);
    }
    m_lock.unlock();
    return servers;
}

/**
 * \fn UPNPScanner::Start(void)
 *  Initialises the scanner, hooks it up to the subscription service and the
 *  SSDP cache and starts scanning.
 */
void UPNPScanner::Start()
{
    m_lock.lock();

    // create our network handler
    m_network = new QNetworkAccessManager();
    connect(m_network, SIGNAL(finished(QNetworkReply*)),
            this, SLOT(replyFinished(QNetworkReply*)));

    // listen for SSDP updates
    SSDP::Instance()->AddListener(this);

    // listen for subscriptions and events
    if (m_subscription)
        m_subscription->addListener(this);

    // create our update timer (driven by AddServer and ParseDescription)
    m_updateTimer = new QTimer(this);
    m_updateTimer->setSingleShot(true);
    connect(m_updateTimer, SIGNAL(timeout()), this, SLOT(Update()));

    // create our watchdog timer (checks for stale servers)
    m_watchdogTimer = new QTimer(this);
    connect(m_watchdogTimer, SIGNAL(timeout()), this, SLOT(CheckStatus()));
    m_watchdogTimer->start(1000 * 10); // every 10s

    // avoid connecting to the master backend
    m_masterHost = gCoreContext->GetSetting("MasterServerIP");
    m_masterPort = gCoreContext->GetSettingOnHost("BackendStatusPort",
                                             m_masterHost, "6544").toInt();

    m_lock.unlock();
    LOG(VB_GENERAL, LOG_INFO, LOC + "Started");
}

/**
 * \fn UPNPScanner::Stop(void)
 *  Stops scanning.
 */
void UPNPScanner::Stop(void)
{
    m_lock.lock();

    // stop listening
    SSDP::Instance()->RemoveListener(this);
    if (m_subscription)
        m_subscription->removeListener(this);

    // disable updates
    if (m_updateTimer)
        m_updateTimer->stop();
    if (m_watchdogTimer)
        m_watchdogTimer->stop();

    // cleanup our servers and subscriptions
    QHashIterator<QString,MediaServer*> it(m_servers);
    while (it.hasNext())
    {
        it.next();
        if (m_subscription && it.value()->m_subscribed)
            m_subscription->Unsubscribe(it.key());
        if (it.value()->m_renewalTimerId)
            killTimer(it.value()->m_renewalTimerId);
        delete it.value();
    }
    m_servers.clear();

    // cleanup the network
    foreach (QNetworkReply *reply, m_descriptionRequests)
    {
        reply->abort();
        delete reply;
    }
    m_descriptionRequests.clear();
    foreach (QNetworkReply *reply, m_browseRequests)
    {
        reply->abort();
        delete reply;
    }
    m_browseRequests.clear();
    delete m_network;
    m_network = NULL;

    // delete the timers
    delete m_updateTimer;
    delete m_watchdogTimer;
    m_updateTimer   = NULL;
    m_watchdogTimer = NULL;

    m_lock.unlock();
    LOG(VB_GENERAL, LOG_INFO, LOC + "Finished");
}

/**
 * \fn UPNPScanner::Update(void)
 *  Iterates through the list of known servers and initialises a connection by
 *  requesting the device description.
 */
void UPNPScanner::Update(void)
{
    // decide which servers still need to be checked
    m_lock.lock();
    if (m_servers.isEmpty())
    {
        m_lock.unlock();
        return;
    }

    // if our network queue is full, then we may need to come back later
    bool reschedule = false;

    QHashIterator<QString,MediaServer*> it(m_servers);
    while (it.hasNext())
    {
        it.next();
        if ((it.value()->m_connectionAttempts < MAX_ATTEMPTS) &&
            (it.value()->m_controlURL.isEmpty()))
        {
            bool sent = false;
            QUrl url = it.value()->m_URL;
            if (!m_descriptionRequests.contains(url) &&
                (m_descriptionRequests.size() < MAX_REQUESTS) &&
                url.isValid())
            {
                QNetworkReply *reply = m_network->get(QNetworkRequest(url));
                if (reply)
                {
                    sent = true;
                    m_descriptionRequests.insert(url, reply);
                    it.value()->m_connectionAttempts++;
                }
            }
            if (!sent)
                reschedule = true;
        }
    }

    if (reschedule)
        ScheduleUpdate();
    m_lock.unlock();
}

/**
 * \fn UPNPScanner::CheckStatus(void)
 *  Removes media servers that can no longer be found in the SSDP cache.
 */
void UPNPScanner::CheckStatus(void)
{
    // FIXME
    // Remove stale servers - the SSDP cache code does not send out removal
    // notifications for expired (rather than explicitly closed) connections
    m_lock.lock();
    QMutableHashIterator<QString,MediaServer*> it(m_servers);
    while (it.hasNext())
    {
        it.next();
        if (!SSDP::Find("urn:schemas-upnp-org:device:MediaServer:1", it.key()))
        {
            LOG(VB_UPNP, LOG_INFO, LOC +
                QString("%1 no longer in SSDP cache. Removing")
                    .arg(it.value()->m_URL.toString()));
            MediaServer* last = it.value();
            it.remove();
            delete last;
        }
    }
    m_lock.unlock();
}

/**
 * \fn UPNPScanner::replyFinished(void)
 *  Validates network responses against known requests and parses expected
 *  responses for the required data.
 */
void UPNPScanner::replyFinished(QNetworkReply *reply)
{
    if (!reply)
        return;

    QUrl url   = reply->url();
    bool valid = reply->error() == QNetworkReply::NoError;

    if (!valid)
    {
        LOG(VB_UPNP, LOG_ERR, LOC +
            QString("Network request for '%1' returned error '%2'")
                .arg(url.toString()).arg(reply->errorString()));
    }

    bool description = false;
    bool browse      = false;

    m_lock.lock();
    if (m_descriptionRequests.contains(url, reply))
    {
        m_descriptionRequests.remove(url, reply);
        description = true;
    }
    else if (m_browseRequests.contains(url, reply))
    {
        m_browseRequests.remove(url, reply);
        browse = true;
    }
    m_lock.unlock();

    if (browse && valid)
    {
        ParseBrowse(url, reply);
        if (m_fullscan)
            BrowseNextContainer();
    }
    else if (description)
    {
        if (!valid || (valid && !ParseDescription(url, reply)))
        {
            // if there will be no more attempts, update the logs
            CheckFailure(url);
            // try again
            ScheduleUpdate();
        }
    }
    else
        LOG(VB_UPNP, LOG_ERR, LOC + "Received unknown reply");

    reply->deleteLater();
}

/**
 * \fn UPNPScanner::CustomEvent(QEvent*)
 *  Processes subscription and SSDP cache update events.
 */
void UPNPScanner::customEvent(QEvent *event)
{
    if ((MythEvent::Type)(event->type()) != MythEvent::MythEventMessage)
        return;

    // UPnP events
    MythEvent *me  = (MythEvent *)event;
    QString    ev  = me->Message();

    if (ev == "UPNP_STARTSCAN")
    {
        BrowseNextContainer();
        return;
    }
    else if (ev == "UPNP_BROWSEOBJECT")
    {
        if (me->ExtraDataCount() == 2)
        {
            QUrl url;
            QString usn = me->ExtraData(0);
            QString objectid = me->ExtraData(1);
            m_lock.lock();
            if (m_servers.contains(usn))
            {
                url = m_servers[usn]->m_controlURL;
                LOG(VB_GENERAL, LOG_INFO, QString("UPNP_BROWSEOBJECT: %1->%2")
                    .arg(m_servers[usn]->m_friendlyName).arg(objectid));
            }
            m_lock.unlock();
            if (!url.isEmpty())
                SendBrowseRequest(url, objectid);
        }
        return;
    }
    else if (ev == "UPNP_EVENT")
    {
        MythInfoMapEvent *info = (MythInfoMapEvent*)event;
        if (!info)
            return;
        if (!info->GetInfoMap())
            return;

        QString usn = info->GetInfoMap()->value("usn");
        QString id  = info->GetInfoMap()->value("SystemUpdateID");
        if (usn.isEmpty() || id.isEmpty())
            return;

        m_lock.lock();
        if (m_servers.contains(usn))
        {
            int newid = id.toInt();
            if (m_servers[usn]->m_systemUpdateID != newid)
            {
                m_scanComplete &= m_servers[usn]->ResetContent(newid);
                LOG(VB_GENERAL, LOG_INFO, LOC +
                    QString("New SystemUpdateID '%1' for %2").arg(id).arg(usn));
                Debug();
            }
        }
        m_lock.unlock();
        return;
    }

    // process SSDP cache updates
    QString    uri = me->ExtraDataCount() > 0 ? me->ExtraData(0) : QString();
    QString    usn = me->ExtraDataCount() > 1 ? me->ExtraData(1) : QString();

    if (uri == "urn:schemas-upnp-org:device:MediaServer:1")
    {
        QString url = (ev == "SSDP_ADD") ? me->ExtraData(2) : QString();
        AddServer(usn, url);
    }
}

/**
 * \fn UPNPScanner::timerEvent(QTimerEvent*)
 *  Handles subscription renewal timer events.
 */
void UPNPScanner::timerEvent(QTimerEvent * event)
{
    int id = event->timerId();
    if (id)
        killTimer(id);

    int timeout = 0;
    QString usn;

    m_lock.lock();
    QHashIterator<QString,MediaServer*> it(m_servers);
    while (it.hasNext())
    {
        it.next();
        if (it.value()->m_renewalTimerId == id)
        {
            it.value()->m_renewalTimerId = 0;
            usn = it.key();
            if (m_subscription)
                timeout = m_subscription->Renew(usn);
        }
    }
    m_lock.unlock();

    if (timeout > 0)
    {
        ScheduleRenewal(usn, timeout);
        LOG(VB_GENERAL, LOG_INFO, LOC +
            QString("Re-subscribed for %1 seconds to %2")
                .arg(timeout).arg(usn));
    }
}

/**
 * \fn UPNPScanner::ScheduleUpdate(void)
 */
void UPNPScanner::ScheduleUpdate(void)
{
    m_lock.lock();
    if (m_updateTimer && !m_updateTimer->isActive())
        m_updateTimer->start(200);
    m_lock.unlock();
}

/**
 * \fn UPNPScanner::CheckFailure(const QUrl&)
 *  Updates the logs for failed server connections.
 */
void UPNPScanner::CheckFailure(const QUrl &url)
{
    m_lock.lock();
    QHashIterator<QString,MediaServer*> it(m_servers);
    while (it.hasNext())
    {
        it.next();
        if (it.value()->m_URL == url &&
            it.value()->m_connectionAttempts == MAX_ATTEMPTS)
        {
            Debug();
            break;
        }
    }
    m_lock.unlock();
}

/**
 * \fn UPNPScanner::Debug(void)
 */
void UPNPScanner::Debug(void)
{
    m_lock.lock();
    LOG(VB_UPNP, LOG_INFO, LOC + QString("%1 media servers discovered:")
                                   .arg(m_servers.size()));
    QHashIterator<QString,MediaServer*> it(m_servers);
    while (it.hasNext())
    {
        it.next();
        QString status = "Probing";
        if (it.value()->m_controlURL.toString().isEmpty())
        {
            if (it.value()->m_connectionAttempts >= MAX_ATTEMPTS)
                status = "Failed";
        }
        else
            status = "Yes";
        LOG(VB_UPNP, LOG_INFO, LOC +
            QString("'%1' Connected: %2 Subscribed: %3 SystemUpdateID: "
                    "%4 timerId: %5")
                .arg(it.value()->m_friendlyName).arg(status)
                .arg(it.value()->m_subscribed ? "Yes" : "No")
                .arg(it.value()->m_systemUpdateID)
                .arg(it.value()->m_renewalTimerId));
    }
    m_lock.unlock();
}

/**
 * \fn UPNPScanner::BrowseNextContainer
 *  For each known media server, find the next container which needs to be
 *  browsed and trigger sending of the browse request (with a maximum of one
 *  active browse request for each server). Once all containers have been
 *  browsed, the scan is considered complete. N.B. failed browse requests
 *  are ignored.
 */
void UPNPScanner::BrowseNextContainer(void)
{
    QMutexLocker locker(&m_lock);

    QHashIterator<QString,MediaServer*> it(m_servers);
    bool complete = true;
    while (it.hasNext())
    {
        it.next();
        if (it.value()->m_subscribed)
        {
            // limit browse requests to one active per server
            if (m_browseRequests.contains(it.value()->m_controlURL))
            {
                complete = false;
                continue;
            }

            QString next = it.value()->NextUnbrowsed();
            if (!next.isEmpty())
            {
                complete = false;
                SendBrowseRequest(it.value()->m_controlURL, next);
                continue;
            }

            LOG(VB_UPNP, LOG_INFO, LOC + QString("Scan completed for %1")
                .arg(it.value()->m_friendlyName));
        }
    }

    if (complete)
    {
        LOG(VB_GENERAL, LOG_INFO, LOC +
            QString("Media Server scan is complete."));
        m_scanComplete = true;
        m_fullscan = false;
    }
}

/**
 * \fn UPNPScanner::SendBrowseRequest(const QUrl&, const QString&)
 *  Formulates and sends a ContentDirectory Service Browse Request to the given
 *  control URL, requesting data for the object identified by objectid.
 */
void UPNPScanner::SendBrowseRequest(const QUrl &url, const QString &objectid)
{
    QNetworkRequest req = QNetworkRequest(url);
    req.setRawHeader("CONTENT-TYPE", "text/xml; charset=\"utf-8\"");
    req.setRawHeader("SOAPACTION",
                  "\"urn:schemas-upnp-org:service:ContentDirectory:1#Browse\"");
#if 0
    req.setRawHeader("MAN", "\"http://schemasxmlsoap.org/soap/envelope/\"");
    req.setRawHeader("01-SOAPACTION",
                  "\"urn:schemas-upnp-org:service:ContentDirectory:1#Browse\"");
#endif

    QByteArray body;
    QTextStream data(&body);
    data.setCodec(QTextCodec::codecForName("UTF-8"));
    data << "<?xml version=\"1.0\"?>\r\n";
    data << "<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">\r\n";
    data << "  <s:Body>\r\n";
    data << "    <u:Browse xmlns:u=\"urn:schemas-upnp-org:service:ContentDirectory:1\">\r\n";
    data << "      <ObjectID>" << objectid.toUtf8() << "</ObjectID>\r\n";
    data << "      <BrowseFlag>BrowseDirectChildren</BrowseFlag>\r\n";
    data << "      <Filter>*</Filter>\r\n";
    data << "      <StartingIndex>0</StartingIndex>\r\n";
    data << "      <RequestedCount>0</RequestedCount>\r\n";
    data << "      <SortCriteria></SortCriteria>\r\n";
    data << "    </u:Browse>\r\n";
    data << "  </s:Body>\r\n";
    data << "</s:Envelope>\r\n";
    data.flush();

    m_lock.lock();
    QNetworkReply *reply = m_network->post(req, body);
    if (reply)
        m_browseRequests.insert(url, reply);
    m_lock.unlock();
}

/**
 * \fn UPNPScanner::AddServer(const QString&, const QString&)
 *  Adds the server identified by usn and reachable via url to the list of
 *  known media servers and schedules an update to initiate a connection.
 */
void UPNPScanner::AddServer(const QString &usn, const QString &url)
{
    if (url.isEmpty())
    {
        RemoveServer(usn);
        return;
    }

    // sometimes initialisation is too early and m_masterHost is empty
    if (m_masterHost.isEmpty())
    {
        m_masterHost = gCoreContext->GetSetting("MasterServerIP");
        m_masterPort = gCoreContext->GetSettingOnHost("BackendStatusPort",
                                                  m_masterHost, "6544").toInt();
    }

    QUrl qurl(url);
    if (qurl.host() == m_masterHost && qurl.port() == m_masterPort)
    {
        LOG(VB_UPNP, LOG_INFO, LOC + "Ignoring master backend.");
        return;
    }

    m_lock.lock();
    if (!m_servers.contains(usn))
    {
        m_servers.insert(usn, new MediaServer(url));
        LOG(VB_GENERAL, LOG_INFO, LOC + QString("Adding: %1").arg(usn));
        ScheduleUpdate();
    }
    m_lock.unlock();
}

/**
 * \fn UPNPScanner::RemoveServer(const QString&)
 */
void UPNPScanner::RemoveServer(const QString &usn)
{
    m_lock.lock();
    if (m_servers.contains(usn))
    {
        LOG(VB_GENERAL, LOG_INFO, LOC + QString("Removing: %1").arg(usn));
        MediaServer* old = m_servers[usn];
        if (old->m_renewalTimerId)
            killTimer(old->m_renewalTimerId);
        m_servers.remove(usn);
        delete old;
        if (m_subscription)
            m_subscription->Remove(usn);
    }
    m_lock.unlock();

    Debug();
}

/**
 * \fn UPNPScanner::ScheduleRenewal(const QString&, int)
 *  Creates a QTimer to trigger a subscription renewal for a given media server.
 */
void UPNPScanner::ScheduleRenewal(const QString &usn, int timeout)
{
    // sanitise the timeout
    int renew = timeout - 10;
    if (renew < 10)
        renew = 10;
    if (renew > 43200)
        renew = 43200;

    m_lock.lock();
    if (m_servers.contains(usn))
        m_servers[usn]->m_renewalTimerId = startTimer(renew * 1000);
    m_lock.unlock();
}

/**
 * \fn UPNPScanner::ParseBrowse(const QUrl&, QNetworkReply*)
 *  Parse the XML returned from Content Directory Service browse request.
 */
void UPNPScanner::ParseBrowse(const QUrl &url, QNetworkReply *reply)
{
    QByteArray data = reply->readAll();
    if (data.isEmpty())
        return;

    // Open the response for parsing
    QDomDocument *parent = new QDomDocument();
    QString errorMessage;
    int errorLine   = 0;
    int errorColumn = 0;
    if (!parent->setContent(data, false, &errorMessage, &errorLine,
                            &errorColumn))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("DIDL Parse error, Line: %1 Col: %2 Error: '%3'")
                .arg(errorLine).arg(errorColumn).arg(errorMessage));
        delete parent;
        return;
    }

    LOG(VB_UPNP, LOG_INFO, "\n\n" + parent->toString(4) + "\n\n");

    // pull out the actual result
    QDomDocument *result = NULL;
    uint num      = 0;
    uint total    = 0;
    uint updateid = 0;
    QDomElement docElem = parent->documentElement();
    QDomNode n = docElem.firstChild();
    if (!n.isNull())
        result = FindResult(n, num, total, updateid);
    delete parent;

    if (!result || num < 1 || total < 1)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("Failed to find result for %1") .arg(url.toString()));
        return;
    }

    // determine the 'server' which requested the browse
    m_lock.lock();

    MediaServer* server = NULL;
    QHashIterator<QString,MediaServer*> it(m_servers);
    while (it.hasNext())
    {
        it.next();
        if (url == it.value()->m_controlURL)
        {
            server = it.value();
            break;
        }
    }

    // discard unmatched responses
    if (!server)
    {
        m_lock.unlock();
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("Received unknown response for %1").arg(url.toString()));
        return;
    }

    // check the update ID
    if (server->m_systemUpdateID != (int)updateid)
    {
        // if this is not the root container, this browse will now fail
        // as the appropriate parentID will not be found
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("%1 updateID changed during browse (old %2 new %3)")
                .arg(server->m_friendlyName).arg(server->m_systemUpdateID)
                .arg(updateid));
        m_scanComplete &= server->ResetContent(updateid);
        Debug();
    }

    // find containers (directories) and actual items and add them and reset
    // the parent when we have found the first item
    bool reset = true;
    docElem = result->documentElement();
    n = docElem.firstChild();
    while (!n.isNull())
    {
        FindItems(n, *server, reset);
        n = n.nextSibling();
    }
    delete result;

    m_lock.unlock();
}

void UPNPScanner::FindItems(const QDomNode &n, MediaServerItem &content,
                            bool &resetparent)
{
    QDomElement node = n.toElement();
    if (node.isNull())
        return;

    if (node.tagName() == "container")
    {
        QString title = "ERROR";
        QDomNode next = node.firstChild();
        while (!next.isNull())
        {
            QDomElement container = next.toElement();
            if (!container.isNull() && container.tagName() == "title")
                title = container.text();
            next = next.nextSibling();
        }

        QString thisid   = node.attribute("id", "ERROR");
        QString parentid = node.attribute("parentID", "ERROR");
        MediaServerItem container =
            MediaServerItem(thisid, parentid, title, QString());
        MediaServerItem *parent = content.Find(parentid);
        if (parent)
        {
            if (resetparent)
            {
                parent->Reset();
                resetparent = false;
            }
            parent->m_scanned = true;
            parent->Add(container);
        }
        return;
    }

    if (node.tagName() == "item")
    {
        QString title = "ERROR";
        QString url = "ERROR";
        QDomNode next = node.firstChild();
        while (!next.isNull())
        {
            QDomElement item = next.toElement();
            if (!item.isNull())
            {
                if(item.tagName() == "res")
                    url = item.text();
                if(item.tagName() == "title")
                    title = item.text();
            }
            next = next.nextSibling();
        }

        QString thisid   = node.attribute("id", "ERROR");
        QString parentid = node.attribute("parentID", "ERROR");
        MediaServerItem item =
                MediaServerItem(thisid, parentid, title, url);
        item.m_scanned = true;
        MediaServerItem *parent = content.Find(parentid);
        if (parent)
        {
            if (resetparent)
            {
                parent->Reset();
                resetparent = false;
            }
            parent->m_scanned = true;
            parent->Add(item);
        }
        return;
    }

    QDomNode next = node.firstChild();
    while (!next.isNull())
    {
        FindItems(next, content, resetparent);
        next = next.nextSibling();
    }
}

QDomDocument* UPNPScanner::FindResult(const QDomNode &n, uint &num,
                                      uint &total, uint &updateid)
{
    QDomDocument *result = NULL;
    QDomElement node = n.toElement();
    if (node.isNull())
        return NULL;

    if (node.tagName() == "NumberReturned")
        num = node.text().toUInt();
    if (node.tagName() == "TotalMatches")
        total = node.text().toUInt();
    if (node.tagName() == "UpdateID")
        updateid = node.text().toUInt();
    if (node.tagName() == "Result" && !result)
    {
        QString errorMessage;
        int errorLine   = 0;
        int errorColumn = 0;
        result = new QDomDocument();
        if (!result->setContent(node.text(), true, &errorMessage, &errorLine, &errorColumn))
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                QString("DIDL Parse error, Line: %1 Col: %2 Error: '%3'")
                    .arg(errorLine).arg(errorColumn).arg(errorMessage));
            delete result;
            result = NULL;
        }
    }

    QDomNode next = node.firstChild();
    while (!next.isNull())
    {
        QDomDocument *res = FindResult(next, num, total, updateid);
        if (res)
            result = res;
        next = next.nextSibling();
    }
    return result;
}

/**
 * \fn UPNPScanner::ParseDescription(const QUrl&, QNetworkReply*)
 *  Parse the device description XML return my a media server.
 */
bool UPNPScanner::ParseDescription(const QUrl &url, QNetworkReply *reply)
{
    if (url.isEmpty() || !reply)
        return false;

    QByteArray data = reply->readAll();
    if (data.isEmpty())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("%1 returned an empty device description.")
                .arg(url.toString()));
        return false;
    }

    // parse the device description
    QString controlURL = QString();
    QString eventURL   = QString();
    QString friendlyName = QString("Unknown");
    QString URLBase = QString();

    QDomDocument doc;
    QString errorMessage;
    int errorLine   = 0;
    int errorColumn = 0;
    if (!doc.setContent(data, false, &errorMessage, &errorLine, &errorColumn))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("Failed to parse device description from %1")
                .arg(url.toString()));
        LOG(VB_GENERAL, LOG_ERR, LOC + QString("Line: %1 Col: %2 Error: '%3'")
            .arg(errorLine).arg(errorColumn).arg(errorMessage));
        return false;
    }

    QDomElement docElem = doc.documentElement();
    QDomNode n = docElem.firstChild();
    while (!n.isNull())
    {
        QDomElement e1 = n.toElement();
        if (!e1.isNull())
        {
            if(e1.tagName() == "device")
                ParseDevice(e1, controlURL, eventURL, friendlyName);
            if (e1.tagName() == "URLBase")
                URLBase = e1.text();
        }
        n = n.nextSibling();
    }

    if (controlURL.isEmpty())
    {
        LOG(VB_UPNP, LOG_ERR, LOC +
            QString("Failed to parse device description for %1")
                .arg(url.toString()));
        return false;
    }

    // if no URLBase was provided, use the known url
    if (URLBase.isEmpty())
        URLBase = url.toString(QUrl::RemovePath | QUrl::RemoveFragment |
                               QUrl::RemoveQuery);

    // strip leading slashes off the controlURL
    while (!controlURL.isEmpty() && controlURL.startsWith("/"))
        controlURL = controlURL.mid(1);

    // strip leading slashes off the eventURL
    //while (!eventURL.isEmpty() && eventURL.startsWith("/"))
    //    eventURL = eventURL.mid(1);

    // strip trailing slashes off URLBase
    while (!URLBase.isEmpty() && URLBase.endsWith("/"))
        URLBase = URLBase.mid(0, URLBase.size() - 1);

    controlURL = URLBase + "/" + controlURL;
    QString fulleventURL = URLBase + "/" + eventURL;

    LOG(VB_UPNP, LOG_INFO, LOC + QString("Control URL for %1 at %2")
            .arg(friendlyName).arg(controlURL));
    LOG(VB_UPNP, LOG_INFO, LOC + QString("Event URL for %1 at %2")
            .arg(friendlyName).arg(fulleventURL));

    // update the server details. If the server has gone away since the request
    // was posted, this will silently fail and we won't try again
    QString usn;
    QUrl qeventurl = QUrl(fulleventURL);
    int timeout = 0;

    m_lock.lock();
    QHashIterator<QString,MediaServer*> it(m_servers);
    while (it.hasNext())
    {
        it.next();
        if (it.value()->m_URL == url)
        {
            usn = it.key();
            QUrl qcontrolurl(controlURL);
            it.value()->m_controlURL   = qcontrolurl;
            it.value()->m_eventSubURL  = qeventurl;
            it.value()->m_eventSubPath = eventURL;
            it.value()->m_friendlyName = friendlyName;
            it.value()->m_name         = friendlyName;
            break;
        }
    }

    if (m_subscription && !usn.isEmpty())
    {
        timeout = m_subscription->Subscribe(usn, qeventurl, eventURL);
        m_servers[usn]->m_subscribed = (timeout > 0);
    }
    m_lock.unlock();

    if (timeout > 0)
    {
        LOG(VB_GENERAL, LOG_INFO, LOC +
            QString("Subscribed for %1 seconds to %2") .arg(timeout).arg(usn));
        ScheduleRenewal(usn, timeout);
        // we only scan servers we are subscribed to - and the scan is now
        // incomplete
        m_scanComplete = false;
    }

    Debug();
    return true;
}


void UPNPScanner::ParseDevice(QDomElement &element, QString &controlURL,
                              QString &eventURL, QString &friendlyName)
{
    QDomNode dev = element.firstChild();
    while (!dev.isNull())
    {
        QDomElement e = dev.toElement();
        if (!e.isNull())
        {
            if (e.tagName() == "friendlyName")
                friendlyName = e.text();
            if (e.tagName() == "serviceList")
                ParseServiceList(e, controlURL, eventURL);
        }
        dev = dev.nextSibling();
    }
}

void UPNPScanner::ParseServiceList(QDomElement &element, QString &controlURL,
                                   QString &eventURL)
{
    QDomNode list = element.firstChild();
    while (!list.isNull())
    {
        QDomElement e = list.toElement();
        if (!e.isNull())
            if (e.tagName() == "service")
                ParseService(e, controlURL, eventURL);
        list = list.nextSibling();
    }
}

void UPNPScanner::ParseService(QDomElement &element, QString &controlURL,
                               QString &eventURL)
{
    bool     iscds       = false;
    QString  control_url = QString();
    QString  event_url   = QString();
    QDomNode service     = element.firstChild();

    while (!service.isNull())
    {
        QDomElement e = service.toElement();
        if (!e.isNull())
        {
            if (e.tagName() == "serviceType")
                iscds = (e.text() == "urn:schemas-upnp-org:service:ContentDirectory:1");
            if (e.tagName() == "controlURL")
                control_url = e.text();
            if (e.tagName() == "eventSubURL")
                event_url = e.text();
        }
        service = service.nextSibling();
    }

    if (iscds)
    {
        controlURL = control_url;
        eventURL   = event_url;
    }
}
