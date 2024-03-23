#ifndef RSSMANAGER_H
#define RSSMANAGER_H

#include <vector>

#include <QObject>
#include <QMetaType>
#include <QRecursiveMutex>
#include <QTimer>
#include <QDateTime>
#include <QByteArray>
#include <QVariant>
#include <QNetworkReply>

#include "mythbaseexp.h"
#include "rssparse.h"

using namespace std::chrono_literals;

class RSSSite;
class MBASE_PUBLIC RSSSite : public QObject
{
    Q_OBJECT

  public:

    class List : public std::vector<RSSSite*>
    {
      public:
        void clear(void)
        {
            while (!empty())
            {
                RSSSite *tmp = back();
                pop_back();
                tmp->deleteLater();
            }
        }
    };

    RSSSite(QString title,
            QString sortTitle,
            QString image,
            ArticleType type,
            QString description,
            QString url,
            QString author,
            bool download,
            QDateTime updated);

    ~RSSSite() override = default;

    using rssList = QList<RSSSite *>;

    const QString& GetTitle() const { return m_title; }
    const QString& GetSortTitle() const { return m_sortTitle; }
    const QString& GetImage() const { return m_image; }
    const ArticleType& GetType() const { return m_type; }
    const QString& GetDescription() const { return m_description; }
    const QString& GetURL() const { return m_url; }
    const QString& GetAuthor() const { return m_author; }
    const bool& GetDownload() const { return m_download; }
    const QDateTime& GetUpdated() const { return m_updated; }

    std::chrono::minutes timeSinceLastUpdate(void) const;

    void insertRSSArticle(ResultItem *item);
    void clearRSSArticles(void);

    ResultItem::resultList GetVideoList(void) const;

    void retrieve(void);
    void stop(void);
    void process(void);

  private:

    static QUrl redirectUrl(const QUrl& possibleRedirectUrl,
                            const QUrl& oldRedirectUrl) ;

    QString     m_title;
    QString     m_sortTitle;
    QString     m_image;
    ArticleType m_type;
    QString     m_description;
    QString     m_url;
    QUrl        m_urlReq;
    QString     m_author;
    bool        m_download;
    QDateTime   m_updated;

    mutable    QRecursiveMutex m_lock;
    QByteArray m_data;
    QString    m_imageURL;
    bool       m_podcast              {false};

    ResultItem::resultList m_articleList;

    QNetworkReply          *m_reply   {nullptr};
    QNetworkAccessManager  *m_manager {nullptr};

  private slots:
    void slotCheckRedirect(QNetworkReply* reply);

  signals:

    void finished(RSSSite *item);

};

class MBASE_PUBLIC RSSManager : public QObject
{
    Q_OBJECT

  public:
    RSSManager();
    ~RSSManager() override;
    void startTimer();
    void stopTimer();

  signals:
    void finished();

  public slots:
    void doUpdate();

  private slots:
    void slotRefreshRSS(void);
    void slotRSSRetrieved(RSSSite *site);

  private:
    void processAndInsertRSS(RSSSite *site);

    QTimer                        *m_timer      {nullptr};
    RSSSite::rssList               m_sites;
    std::chrono::hours             m_updateFreq {6h};
    RSSSite::rssList               m_inprogress;
};

Q_DECLARE_METATYPE(RSSSite*)

#endif // RSSMANAGER_H
