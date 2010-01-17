#ifndef RSSMANAGER_H
#define RSSMANAGER_H

#include <vector>
using namespace std;

#include <QObject>
#include <QMetaType>
#include <QMutex>
#include <QTimer>
#include <QProcess>
#include <QDateTime>
#include <QByteArray>
#include <QVariant>

#include "parse.h"

#include <mythhttppool.h>

class RSSSite;
class RSSSite : public QObject, public MythHttpListener
{
    Q_OBJECT

  public:

    enum State {
        Retrieving = 0,
        RetrieveFailed,
        WriteFailed,
        Success
    };

    class List : public vector<RSSSite*>
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

    RSSSite(const QString& title,
                  const QString& image,
                  const QString& description,
                  const QString& url,
                  const QString& author,
                  const bool& download,
                  const QDateTime& updated);

    ~RSSSite();

    typedef QList<RSSSite *> rssList;

    const QString& GetTitle() const { return m_title; }
    const QString& GetImage() const { return m_image; }
    const QString& GetDescription() const { return m_description; }
    const QString& GetURL() const { return m_url; }
    const QString& GetAuthor() const { return m_author; }
    const bool& GetDownload() const { return m_download; }
    const QDateTime& GetUpdated() const { return m_updated; }

    virtual void deleteLater();

    unsigned int timeSinceLastUpdate(void) const; // in minutes

    void insertRSSArticle(ResultVideo *item);
    void clearRSSArticles(void);

    ResultVideo::resultList GetVideoList(void) const;

    void retrieve(void);
    void stop(void);
    void process(void);

    bool     successful(void) const;
    QString  errorMsg(void) const;

    virtual void Update(QHttp::Error      error,
                        const QString    &error_str,
                        const QUrl       &url,
                        uint              http_status_id,
                        const QString    &http_status_str,
                        const QByteArray &data);

  private:

    QString    m_title;
    QString    m_image;
    QString    m_description;
    QString    m_url;
    QUrl       m_urlReq;
    QString    m_author;
    bool       m_download;
    QDateTime  m_updated;

    mutable    QMutex m_lock;
    QString    m_destDir;
    QByteArray m_data;
    State      m_state;
    QString    m_errorString;
    QString    m_updateErrorString;
    QString    m_imageURL;
    bool       m_podcast;

    ResultVideo::resultList m_articleList;

  signals:

    void finished(RSSSite *item);

};
Q_DECLARE_METATYPE(RSSSite*)

class RSSManager : public QObject
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

    QTimer                        *m_timer;
    RSSSite::rssList               m_sites;
    uint                           m_updateFreq;
};

#endif
