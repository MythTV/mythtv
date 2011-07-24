#ifndef UPNPSCANNER_H
#define UPNPSCANNER_H

#include <QThread>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QMutex>
#include <QTimer>
#include <QDomDocument>

#include "upnpexp.h"
#include "upnpsubscription.h"

#include "videometadatalistmanager.h"

class MediaServer;
class UPNPSubscription;
class meta_dir_node;

class MediaServerItem
{
  public:
    MediaServerItem() { }
    MediaServerItem(QString id, QString parent, QString name, QString url)
      : m_id(id), m_parentid(parent), m_name(name), m_url(url),
        m_scanned(false) { }
    QString NextUnbrowsed(void);
    bool Add(MediaServerItem &item);
    void Reset(void);

    QString m_id;
    QString m_parentid;
    QString m_name;
    QString m_url;
    bool    m_scanned;
    QMap<QString, MediaServerItem> m_children;
};

class UPNPScanner : public QObject
{
   Q_OBJECT

  public:
   ~UPNPScanner();

    static void         Enable(bool enable, UPNPSubscription *sub = NULL);
    static UPNPScanner* Instance(UPNPSubscription *sub = NULL);

    void StartFullScan(void);
    void GetMetadata(VideoMetadataListManager::metadata_list* list,
                     meta_dir_node *node);
    QMap<QString,QString> ServerList(void);

  protected:
    virtual void customEvent(QEvent *event);
    virtual void timerEvent(QTimerEvent * event);

  private slots:
    void Start();
    void Stop(void);
    void Update(void);
    void CheckStatus(void);
    void replyFinished(QNetworkReply *reply);

  private:
    UPNPScanner(UPNPSubscription *sub);
    void ScheduleUpdate(void);
    void CheckFailure(const QUrl &url);
    void Debug(void);
    void BrowseNextContainer(void);
    void SendBrowseRequest(const QUrl &url, const QString &objectid);
    void AddServer(const QString &usn, const QString &url);
    void RemoveServer(const QString &usn);
    void ScheduleRenewal(const QString &usn, int timeout);

    // xml parsing of browse requests
    void ParseBrowse(const QUrl &url, QNetworkReply *reply);
    void FindItems(const QDomNode &n, MediaServerItem &content);
    QDomDocument* FindResult(const QDomNode &n, uint &num,
                             uint &total, uint &updateid);

    // xml parsing of device description
    bool ParseDescription(const QUrl &url, QNetworkReply *reply);
    void ParseDevice(QDomElement &element, QString &controlURL,
                     QString &eventURL, QString &friendlyName);
    void ParseServiceList(QDomElement &element, QString &controlURL,
                          QString &eventURL);
    void ParseService(QDomElement &element, QString &controlURL,
                      QString &eventURL);

    // convert MediaServerItems to video metadata
    void GetServerContent(MediaServerItem *content,
                          VideoMetadataListManager::metadata_list* list,
                          meta_dir_node *node);

  private:
    static  UPNPScanner* gUPNPScanner;
    static  bool         gUPNPScannerEnabled;
    static  QThread*     gUPNPScannerThread;
    static  QMutex*      gUPNPScannerLock;

    UPNPSubscription *m_subscription;
    QMutex  m_lock;
    QHash<QString,MediaServer*> m_servers;
    QNetworkAccessManager *m_network;
    // TODO Move to QMultiHash when we move to Qt >=4.7
    // QHash(QUrl) unsupported on < 4.7
    QMultiMap<QUrl, QNetworkReply*> m_descriptionRequests;
    QMultiMap<QUrl, QNetworkReply*> m_browseRequests;

    QTimer *m_updateTimer;
    QTimer *m_watchdogTimer;

    QString m_masterHost;
    int     m_masterPort;

    bool    m_scanComplete;
};

#endif // UPNPSCANNER_H
