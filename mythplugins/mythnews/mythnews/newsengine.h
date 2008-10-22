#ifndef NEWSENGINE_H
#define NEWSENGINE_H

#include <vector>
using namespace std;

// QT headers
#include <QString>
#include <QObject>
#include <QDateTime>
#include <QDomDocument>

class Q3UrlOperator;
class Q3NetworkOperation;
class NewsSite;

class NewsArticle
{
  public:
    typedef vector<NewsArticle> List;

    NewsArticle(const QString &title,
                const QString &desc, const QString &artURL,
                const QString &thumbnail, const QString &mediaURL,
                const QString &enclosure);
    NewsArticle(const QString &title);

    QString title(void)       const { return m_title;      }
    QString description(void) const { return m_desc;       }
    QString articleURL(void)  const { return m_articleURL; }
    QString thumbnail(void)   const { return m_thumbnail;  }
    QString mediaURL(void)    const { return m_mediaURL;   }
    QString enclosure(void)   const { return m_enclosure;  }

  private:
    QString   m_title;
    QString   m_desc;
    QString   m_articleURL;
    QString   m_thumbnail;
    QString   m_mediaURL;
    QString   m_enclosure;
    QString   m_enclosureType;
};

// -------------------------------------------------------

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

    NewsSite(const QString &name, const QString &url,
             const QDateTime &updated, const bool &podcast);

    QString   url(void)  const;
    QString   name(void) const;
    QString   description(void) const;
    QDateTime lastUpdated(void) const;
    QString   imageURL(void) const;
    bool      podcast(void) const;
    unsigned int timeSinceLastUpdate(void) const; // in minutes

    void insertNewsArticle(const NewsArticle &item);
    void clearNewsArticles(void);
    NewsArticle::List &articleList(void);

    void retrieve(void);
    void stop(void);
    void process(void);
    void parseRSS(QDomDocument domDoc);
    void parseAtom(QDomDocument domDoc);

    bool     successful(void) const;
    QString  errorMsg(void) const;

private:
    ~NewsSite();

    QString    m_name;
    QString    m_url;
    QString    m_desc;
    QDateTime  m_updated;
    QString    m_destDir;
    QByteArray m_data;
    State      m_state;
    QString    m_errorString;
    QString    m_imageURL;
    bool       m_podcast;

    NewsArticle::List m_articleList;
    Q3UrlOperator     *m_urlOp;

    void ReplaceHtmlChar( QString &s);

signals:

    void finished(NewsSite* item);

private slots:

    void slotFinished(Q3NetworkOperation*);
    void slotGotData(const QByteArray &data,
                     Q3NetworkOperation* op);
};

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


class NewsCategory
{
  public:
    typedef vector<NewsCategory> List;

    QString             name;
    NewsSiteItem::List  siteList;

    void clear(void) { siteList.clear(); }
};

#endif /* NEWSENGINE_H */
