#ifndef _NEWSSITE_H_
#define _NEWSSITE_H_

// C++ headers
#include <vector>
using namespace std;

// MythTV headers
#include <QObject>
#include <mythmiscutil.h>

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

    QString m_name;
    QString m_category;
    QString m_url;
    QString m_ico;
    bool    m_inDB;
    bool    m_podcast;
};
Q_DECLARE_METATYPE(NewsSiteItem*)

class NewsCategory
{
  public:
    typedef vector<NewsCategory> List;

    QString             m_name;
    NewsSiteItem::List  m_siteList;

    void clear(void) { m_siteList.clear(); }
};
Q_DECLARE_METATYPE(NewsCategory*)

class NewsSite;
class NewsSite : public QObject
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

    NewsSite(QString   name,    const QString &url,
             QDateTime updated, const bool     podcast);
    virtual void deleteLater();

    void customEvent(QEvent *event) override; // QObject

    QString   url(void)  const;
    QString   name(void) const;
    QString   sortName(void) const;
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
    void parseRSS(const QDomDocument& domDoc);
    void parseAtom(const QDomDocument& domDoc);
    static inline bool sortByName(NewsSite *a, NewsSite *b)
        { return naturalCompare(a->m_sortName, b->m_sortName) < 0; }

    bool     successful(void) const;
    QString  errorMsg(void) const;

  private:
    ~NewsSite();

    mutable QMutex m_lock {QMutex::Recursive};
    QString    m_name;
    QString    m_sortName;
    QString    m_url;
    QUrl       m_urlReq;
    QString    m_desc;
    QDateTime  m_updated;
    QString    m_destDir;
    QByteArray m_data;
    State      m_state    {NewsSite::Success};
    QString    m_errorString;
    QString    m_updateErrorString;
    QString    m_imageURL;
    bool       m_podcast;

    NewsArticle::List m_articleList;
    static QString ReplaceHtmlChar(const QString &orig);

  signals:

    void finished(NewsSite *item);
};
Q_DECLARE_METATYPE(NewsSite*)

#endif // _NEWSSITE_H_
