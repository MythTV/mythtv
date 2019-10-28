// qt
#include <QDir>
#include <QFile>
#include <QString>

#include "mythdate.h"
#include "mythdirs.h"
#include "mythcontext.h"
#include "mythlogging.h"

#include "mythrssmanager.h"
#include "netutils.h"
#include "rssparse.h"
#include "mythsorthelper.h"

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
    }

    m_inprogress.removeOne(site);
    if (m_inprogress.isEmpty())
        emit finished();
}

void RSSManager::slotRSSRetrieved(RSSSite *site)
{
    markUpdated(site);
    processAndInsertRSS(site);
}


RSSSite::RSSSite( QString  title,
                  QString  sortTitle,
                  QString  image,
                  const ArticleType& type,
                  QString  description,
                  QString  url,
                  QString  author,
                  const bool& download,
                  QDateTime  updated) :
    m_title(std::move(title)), m_sortTitle(std::move(sortTitle)),
    m_image(std::move(image)), m_type(type),
    m_description(std::move(description)), m_url(std::move(url)),
    m_author(std::move(author)),
    m_download(download), m_updated(std::move(updated))
{
    std::shared_ptr<MythSortHelper>sh = getMythSortHelper();
    if (m_sortTitle.isEmpty() and not m_title.isEmpty())
        m_sortTitle = sh->doTitle(m_title);
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
                               const QUrl& oldRedirectUrl)
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
    {
        emit finished(this);
        return;
    }

    QDomDocument domDoc;

    if (!domDoc.setContent(m_data, true))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            "Failed to set content from downloaded XML");
        emit finished(this);
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
            insertRSSArticle(new ResultItem(
               (*i)->GetTitle(), (*i)->GetSortTitle(),
               (*i)->GetSubtitle(), (*i)->GetSortSubtitle(),
               (*i)->GetDescription(), (*i)->GetURL(),
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
    }
    else
        LOG(VB_GENERAL, LOG_ERR, LOC + "Data is not valid RSS-feed");

    emit finished(this);
}
