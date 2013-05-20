#ifndef _NEWSSITE_H_
#define _NEWSSITE_H_

// C++ headers
#include <vector>
using namespace std;

// MythTV headers
#include <QObject>
#if QT_VERSION < 0x050000
#include <mythhttppool.h>
#endif

// QT headers
#include <QDomDocument>
#include <QByteArray>
#include <QDateTime>
#include <QVariant>
#include <QObject>
#include <QString>
#include <QMutex>
#include <QUrl>

// MythNews headers
#include "newsarticle.h"

class NewsSiteItem
{
  public:
    typedef vector<NewsSiteItem> List;

    QString name;
    QString category;
    QString url;
    QString ico;
    bool    inDB;
    bool    podcast;
};
Q_DECLARE_METATYPE(NewsSiteItem*)

class NewsCategory
{
  public:
    typedef vector<NewsCategory> List;

    QString             name;
    NewsSiteItem::List  siteList;

    void clear(void) { siteList.clear(); }
};
Q_DECLARE_METATYPE(NewsCategory*)

class NewsSite;
class NewsSite : public QObject
#if QT_VERSION < 0x050000
, public MythHttpListener
#endif
{
    Q_OBJECT

  public:

    enum State {
        Retrieving = 0,
        RetrieveFailed,
        WriteFailed,
        Success
    };

    class List : public vector<NewsSite*>
    {
      public:
        void clear(void)
        {
            while (size())
            {
                NewsSite *tmp = back();
                pop_back();
                tmp->deleteLater();
            }
        }
    };

    NewsSite(const QString   &name,    const QString &url,
             const QDateTime &updated, const bool     podcast);
    virtual void deleteLater();

    QString   url(void)  const;
    QString   name(void) const;
    QString   description(void) const;
    QDateTime lastUpdated(void) const;
    QString   imageURL(void) const;
    bool      podcast(void) const;
    unsigned int timeSinceLastUpdate(void) const; // in minutes

    void insertNewsArticle(const NewsArticle &item);
    void clearNewsArticles(void);

    NewsArticle::List GetArticleList(void) const;

    void retrieve(void);
    void stop(void);
    void process(void);
    void parseRSS(QDomDocument domDoc);
    void parseAtom(QDomDocument domDoc);

    bool     successful(void) const;
    QString  errorMsg(void) const;

#if QT_VERSION < 0x050000
    virtual void Update(QHttp::Error      error,
                        const QString    &error_str,
                        const QUrl       &url,
                        uint              http_status_id,
                        const QString    &http_status_str,
                        const QByteArray &data);
#endif

  private:
    ~NewsSite();

    mutable QMutex m_lock;
    QString    m_name;
    QString    m_url;
    QUrl       m_urlReq;
    QString    m_desc;
    QDateTime  m_updated;
    QString    m_destDir;
    QByteArray m_data;
    State      m_state;
    QString    m_errorString;
    QString    m_updateErrorString;
    QString    m_imageURL;
    bool       m_podcast;

    NewsArticle::List m_articleList;
    static QString ReplaceHtmlChar(const QString &s);

  signals:

    void finished(NewsSite *item);
};
Q_DECLARE_METATYPE(NewsSite*)

#endif // _NEWSSITE_H_
