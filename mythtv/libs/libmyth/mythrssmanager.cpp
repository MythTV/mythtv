// qt
#include <QString>
#include <QFile>
#include <QDir>

#include "mythdate.h"
#include "mythdirs.h"
#include "mythcontext.h"
#include "mythlogging.h"

#include "mythrssmanager.h"
#include "netutils.h"
#include "rssparse.h"

using namespace std;

#define LOC      QString("RSSSite: ")

// ---------------------------------------------------

RSSManager::RSSManager()
{
    m_updateFreq = (gCoreContext->GetNumSetting(
                       "rss.updateFreq", 6) * 3600 * 1000);

    m_timer = new QTimer();

    connect( m_timer, SIGNAL(timeout()),
                      this, SLOT(doUpdate()));
}

RSSManager::~RSSManager()
{
    delete m_timer;
}

void RSSManager::startTimer()
{
    m_timer->start(m_updateFreq);
}

void RSSManager::stopTimer()
{
    m_timer->stop();
}

void RSSManager::doUpdate()
{
    m_sites = findAllDBRSS();

    for (RSSSite::rssList::iterator i = m_sites.begin();
            i != m_sites.end(); ++i)
    {
        LOG(VB_GENERAL, LOG_INFO, LOC +
            QString("Updating RSS Feed %1") .arg((*i)->GetTitle()));

        connect(*i, SIGNAL(finished(RSSSite*)),
                this, SLOT(slotRSSRetrieved(RSSSite*)));
    }

    slotRefreshRSS();

    m_timer->start(m_updateFreq);
}

void RSSManager::slotRefreshRSS()
{
    if (m_sites.empty())
    {
        emit finished();
        return;
    }

    RSSSite::rssList::iterator i = m_sites.begin();
    for (; i != m_sites.end(); ++i)
    {
            (*i)->retrieve();
            m_inprogress.append(*i);
    }
}

void RSSManager::processAndInsertRSS(RSSSite *site)
{
    if (!site)
        return;

    clearRSSArticles(site->GetTitle(), site->GetType());

    ResultItem::resultList rss = site->GetVideoList();
    ResultItem::resultList::iterator it = rss.begin();
    for (; it != rss.end(); ++it)
    {
        // Insert in the DB here.
        insertRSSArticleInDB(site->GetTitle(), *it, site->GetType());
        m_inprogress.removeOne(site);
    }

    if (m_inprogress.isEmpty())
        emit finished();
}

void RSSManager::slotRSSRetrieved(RSSSite *site)
{
    markUpdated(site);
    processAndInsertRSS(site);
}


RSSSite::RSSSite(const QString& title,
                  const QString& image,
                  const ArticleType& type,
                  const QString& description,
                  const QString& url,
                  const QString& author,
                  const bool& download,
                  const QDateTime& updated) :
    QObject(),
    m_lock(QMutex::Recursive),
    m_reply(NULL),
    m_manager(NULL)
{
    m_title = title;
    m_image = image;
    m_type = type;
    m_description = description;
    m_url = url;
    m_author = author;
    m_download = download;
    m_updated = updated;
}

RSSSite::~RSSSite()
{
}

void RSSSite::insertRSSArticle(ResultItem *item)
{
    QMutexLocker locker(&m_lock);
    m_articleList.append(item);
}

void RSSSite::clearRSSArticles(void)
{
    QMutexLocker locker(&m_lock);
    m_articleList.clear();
}

void RSSSite::retrieve(void)
{
    QMutexLocker locker(&m_lock);
    m_data.resize(0);
    m_articleList.clear();
    m_urlReq = QUrl(m_url);
    if (!m_manager)
    {
        m_manager = new QNetworkAccessManager();
        connect(m_manager, SIGNAL(finished(QNetworkReply*)), this,
                       SLOT(slotCheckRedirect(QNetworkReply*)));
    }

    m_reply = m_manager->get(QNetworkRequest(m_urlReq));
}

QUrl RSSSite::redirectUrl(const QUrl& possibleRedirectUrl,
                               const QUrl& oldRedirectUrl) const
{
    QUrl redirectUrl;
    if(!possibleRedirectUrl.isEmpty() && possibleRedirectUrl != oldRedirectUrl)
        redirectUrl = possibleRedirectUrl;
    return redirectUrl;
}

void RSSSite::slotCheckRedirect(QNetworkReply* reply)
{
    QVariant possibleRedirectUrl =
         reply->attribute(QNetworkRequest::RedirectionTargetAttribute);

    QUrl urlRedirectedTo;
    urlRedirectedTo = redirectUrl(possibleRedirectUrl.toUrl(),
                                  urlRedirectedTo);

    if(!urlRedirectedTo.isEmpty())
    {
        m_manager->get(QNetworkRequest(urlRedirectedTo));
    }
    else
    {
        m_data = m_reply->readAll();
        process();
    }

    reply->deleteLater();
}

ResultItem::resultList RSSSite::GetVideoList(void) const
{
    QMutexLocker locker(&m_lock);
    return m_articleList;
}

unsigned int RSSSite::timeSinceLastUpdate(void) const
{
    QMutexLocker locker(&m_lock);

    QDateTime curTime(MythDate::current());
    unsigned int min = m_updated.secsTo(curTime)/60;
    return min;
}

void RSSSite::process(void)
{
    QMutexLocker locker(&m_lock);

    m_articleList.clear();

    if (!m_data.size())
        return;

    QDomDocument domDoc;

    if (!domDoc.setContent(m_data, true))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            "Failed to set content from downloaded XML");
        return;
    }

    //Check the type of the feed
    QString rootName = domDoc.documentElement().nodeName();
    if (rootName == "rss" || rootName == "rdf:RDF")
    {
        ResultItem::resultList items;
        Parse parser;
        items = parser.parseRSS(domDoc);

        for (ResultItem::resultList::iterator i = items.begin();
                i != items.end(); ++i)
        {
            insertRSSArticle(new ResultItem((*i)->GetTitle(),
               QString(), (*i)->GetDescription(), (*i)->GetURL(),
               (*i)->GetThumbnail(), (*i)->GetMediaURL(),
               (*i)->GetAuthor(), (*i)->GetDate(),
               (*i)->GetTime(), (*i)->GetRating(),
               (*i)->GetFilesize(), (*i)->GetPlayer(),
               (*i)->GetPlayerArguments(),
               (*i)->GetDownloader(),
               (*i)->GetDownloaderArguments(),
               (*i)->GetWidth(),
               (*i)->GetHeight(),
               (*i)->GetLanguage(),
               (*i)->GetDownloadable(),
               (*i)->GetCountries(),
               (*i)->GetSeason(),
               (*i)->GetEpisode(), false));
        }
        emit finished(this);
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Data is not valid RSS-feed");
        emit finished(this);
    }
}
