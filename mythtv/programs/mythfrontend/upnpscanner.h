#ifndef UPNPSCANNER_H
#define UPNPSCANNER_H

#include <utility>

// Qt headers
#include <QDomDocument>
#if QT_VERSION < QT_VERSION_CHECK(5,14,0)
#include <QMutex>
#else
#include <QRecursiveMutex>
#endif
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>

// MythTV headers
#include "upnpsubscription.h"
#include "mthread.h"
#include "upnpexp.h"
#include "videometadatalistmanager.h"

class MediaServer;
class UPNPSubscription;
class meta_dir_node;

class MediaServerItem
{
  public:
    MediaServerItem() = default;
    MediaServerItem(QString  id, QString  parent,
                    QString  name, QString  url)
      : m_id(std::move(id)), m_parentid(std::move(parent)),
        m_name(std::move(name)), m_url(std::move(url)) { }
    QString NextUnbrowsed(void);
    MediaServerItem* Find(QString &id);
    bool Add(const MediaServerItem &item);
    void Reset(void);

    QString m_id;
    QString m_parentid;
    QString m_name;
    QString m_url;
    bool    m_scanned {false};
    QMap<QString, MediaServerItem> m_children;
};

class UPNPScanner : public QObject
{
   Q_OBJECT

  public:
   ~UPNPScanner() override;

    static void         Enable(bool enable, UPNPSubscription *sub = nullptr);
    static UPNPScanner* Instance(UPNPSubscription *sub = nullptr);

    void StartFullScan(void);
    void GetInitialMetadata(VideoMetadataListManager::metadata_list* list,
                            meta_dir_node *node);
    void GetMetadata(VideoMetadataListManager::metadata_list* list,
                     meta_dir_node *node);
    bool GetMetadata(QVariant &data);
    QMap<QString,QString> ServerList(void);

  protected:
    void customEvent(QEvent *event) override; // QObject
    void timerEvent(QTimerEvent * event) override; // QObject

  private slots:
    void Start();
    void Stop(void);
    void Update(void);
    void CheckStatus(void);
    void replyFinished(QNetworkReply *reply);

  private:
    explicit UPNPScanner(UPNPSubscription *sub)
        :  m_subscription(sub) {}
    void ScheduleUpdate(void);
    void CheckFailure(const QUrl &url);
    void Debug(void);
    void BrowseNextContainer(void);
    void SendBrowseRequest(const QUrl &url, const QString &objectid);
    void AddServer(const QString &usn, const QString &url);
    void RemoveServer(const QString &usn);
    void ScheduleRenewal(const QString &usn, std::chrono::seconds timeout);

    // xml parsing of browse requests
    void ParseBrowse(const QUrl &url, QNetworkReply *reply);
    void FindItems(const QDomNode &n, MediaServerItem &content,
                   bool &resetparent);
    QDomDocument* FindResult(const QDomNode &n, uint &num,
                             uint &total, uint &updateid);

    // xml parsing of device description
    bool ParseDescription(const QUrl &url, QNetworkReply *reply);
    static void ParseDevice(QDomElement &element, QString &controlURL,
                            QString &eventURL, QString &friendlyName);
    static void ParseServiceList(QDomElement &element, QString &controlURL,
                                 QString &eventURL);
    static void ParseService(QDomElement &element, QString &controlURL,
                             QString &eventURL);

    // convert MediaServerItems to video metadata
    void GetServerContent(QString &usn, MediaServerItem *content,
                          VideoMetadataListManager::metadata_list* list,
                          meta_dir_node *node);

  private:
    static  UPNPScanner* gUPNPScanner;
    static  bool         gUPNPScannerEnabled;
    static  MThread*     gUPNPScannerThread;
#if QT_VERSION < QT_VERSION_CHECK(5,14,0)
    static  QMutex*      gUPNPScannerLock;
#else
    static  QRecursiveMutex* gUPNPScannerLock;
#endif

    UPNPSubscription *m_subscription {nullptr};
#if QT_VERSION < QT_VERSION_CHECK(5,14,0)
    QMutex  m_lock {QMutex::Recursive};
#else
    QRecursiveMutex  m_lock;
#endif
    QHash<QString,MediaServer*> m_servers;
    QNetworkAccessManager *m_network {nullptr};
    // TODO Move to QMultiHash when we move to Qt >=4.7
    // QHash(QUrl) unsupported on < 4.7
    QMultiMap<QUrl, QNetworkReply*> m_descriptionRequests;
    QMultiMap<QUrl, QNetworkReply*> m_browseRequests;

    QTimer *m_updateTimer {nullptr};
    QTimer *m_watchdogTimer {nullptr};

    QString m_masterHost;
    int     m_masterPort {0};

    bool    m_scanComplete {false};
    bool    m_fullscan {false};
};

#endif // UPNPSCANNER_H
