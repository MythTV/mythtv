#ifndef NEWSSITE_H
#define NEWSSITE_H

// C++ headers
#include <vector>

// QT headers
#include <QByteArray>
#include <QDateTime>
#include <QDomDocument>
#include <QObject>
#include <QString>
#include <QUrl>
#include <QVariant>
#include <QRecursiveMutex>

// MythTV headers
#include <libmythbase/stringutil.h>

// MythNews headers
#include "newsarticle.h"

class NewsSiteItem
{
  public:
    using List = std::vector<NewsSiteItem>;

    QString m_name;
    QString m_category;
    QString m_url;
    QString m_ico;
    bool    m_inDB    { false };
    bool    m_podcast { false };
};
Q_DECLARE_METATYPE(NewsSiteItem*)

class NewsCategory
{
  public:
    using List = std::vector<NewsCategory>;

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

    enum State : std::uint8_t {
        Retrieving = 0,
        RetrieveFailed,
        WriteFailed,
        Success
    };

    class List : public std::vector<NewsSite*>
    {
      public:
        void clear(void)
        {
            while (!empty())
            {
                NewsSite *tmp = back();
                pop_back();
                tmp->deleteLater();
            }
        }
    };

    NewsSite(QString   name,    const QString &url,
             QDateTime updated, bool     podcast);
    virtual void deleteLater();

    void customEvent(QEvent *event) override; // QObject

    QString   url(void)  const;
    QString   name(void) const;
    QString   sortName(void) const;
    QString   description(void) const;
    QDateTime lastUpdated(void) const;
    QString   imageURL(void) const;
    bool      podcast(void) const;
    std::chrono::minutes timeSinceLastUpdate(void) const;

    void insertNewsArticle(const NewsArticle &item);
    void clearNewsArticles(void);

    NewsArticle::List GetArticleList(void) const;

    void retrieve(void);
    void stop(void);
    void process(void);
    void parseRSS(const QDomDocument& domDoc);
    void parseAtom(const QDomDocument& domDoc);
    static bool sortByName(NewsSite *a, NewsSite *b)
        { return StringUtil::naturalCompare(a->m_sortName, b->m_sortName) < 0; }

    bool     successful(void) const;
    QString  errorMsg(void) const;

  private:
    ~NewsSite() override;

    mutable QRecursiveMutex m_lock;
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

#endif // NEWSSITE_H
