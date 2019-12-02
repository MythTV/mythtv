#ifndef MYTHRSSMANAGER_H
#define MYTHRSSMANAGER_H

#include <vector>

#include <QObject>
#include <QMetaType>
#include <QMutex>
#include <QTimer>
#include <QDateTime>
#include <QByteArray>
#include <QVariant>
#include <QNetworkReply>

#include "rssparse.h"


class RSSSite;
class MPUBLIC RSSSite : public QObject
{
    Q_OBJECT

  public:

    class List : public std::vector<RSSSite*>
    {
      public:
        void clear(void)
        {
            while (size())
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
            const ArticleType& type,
            QString description,
            QString url,
            QString author,
            const bool& download,
            QDateTime updated);

    ~RSSSite() = default;

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

    unsigned int timeSinceLastUpdate(void) const; // in minutes

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

    mutable    QMutex m_lock          {QMutex::Recursive};
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

class MPUBLIC RSSManager : public QObject
{
    Q_OBJECT

  public:
    RSSManager();
    ~RSSManager();
    void startTimer();
    void stopTimer();

  signals:
    void finished();

  public slots:
    void doUpdate();

  private slots:
    void slotRefreshRSS(void);
    void slotRSSRetrieved(RSSSite*);

  private:
    void processAndInsertRSS(RSSSite *site);

    QTimer                        *m_timer      {nullptr};
    RSSSite::rssList               m_sites;
    uint                           m_updateFreq {6 * 3600 * 1000};
    RSSSite::rssList               m_inprogress;
};

Q_DECLARE_METATYPE(RSSSite*)

#endif
