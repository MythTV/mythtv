#ifndef NEWSENGINE_H
#define NEWSENGINE_H

// QT headers
#include <QString>
#include <Q3PtrList>
#include <QObject>
#include <QDateTime>

class Q3UrlOperator;
class Q3NetworkOperation;
class NewsSite;
class QDomDocument;

class NewsArticle
{
public:

    typedef Q3PtrList<NewsArticle> List;

    NewsArticle(NewsSite *parent, const QString& title,
                const QString& desc, const QString& artURL,
                const QString& thumbnail, const QString& mediaURL,
                const QString& enclosure);
    ~NewsArticle();

    const QString& title() const { return m_title; }
    const QString& description() const { return m_desc; }
    const QString& articleURL() const { return m_articleURL; }
    const QString& thumbnail() const { return m_thumbnail; }
    const QString& mediaURL() const { return m_mediaURL; }
    const QString& enclosure() const { return m_enclosure; }

private:

    QString   m_title;
    QString   m_desc;
    NewsSite *m_parent;
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

    typedef Q3PtrList<NewsSite> List;

    NewsSite(const QString& name, const QString& url,
             const QDateTime& updated, const bool& podcast);
    ~NewsSite();

    const QString&   url()  const;
    const QString&   name() const;
    QString          description() const;
    const QDateTime& lastUpdated() const;
    const QString&   imageURL() const;
    const bool&      podcast() const;
    unsigned int timeSinceLastUpdate() const; // in minutes

    void insertNewsArticle(NewsArticle* item);
    void clearNewsArticles();
    NewsArticle::List& articleList();

    void retrieve();
    void stop();
    void process();
    void parseRSS(QDomDocument domDoc);
    void parseAtom(QDomDocument domDoc);

    bool     successful() const;
    QString  errorMsg() const;

private:

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
    void slotGotData(const QByteArray& data,
                     Q3NetworkOperation* op);
};

class NewsSiteItem
{
public:

    typedef Q3PtrList<NewsSiteItem> List;

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

    typedef Q3PtrList<NewsCategory> List;

    QString             name;
    NewsSiteItem::List  siteList;

    NewsCategory()
    {
        siteList.setAutoDelete(true);
    }

    ~NewsCategory()
    {
        siteList.clear();
    }

    void clear()
    {
        siteList.clear();
    };
};

#endif /* NEWSENGINE_H */
