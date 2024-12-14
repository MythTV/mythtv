#include "rssmanager.h"

// qt
#include <QDir>
#include <QFile>
#include <QString>

#include "libmythbase/mythcorecontext.h" // for GetDurSetting TODO: excise database from MythCoreContext
#include "libmythbase/mythdate.h"
#include "libmythbase/mythdirs.h"
#include "libmythbase/mythlogging.h"
#include "libmythbase/mythsorthelper.h"

#include "netutils.h"
#include "rssparse.h"

#define LOC      QString("RSSSite: ")

// ---------------------------------------------------

RSSManager::RSSManager()
    : m_timer(new QTimer()),
      m_updateFreq(gCoreContext->GetDurSetting<std::chrono::hours>("rss.updateFreq", 6h))
{
    connect( m_timer, &QTimer::timeout,
                      this, &RSSManager::doUpdate);
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

    for (const auto *site : std::as_const(m_sites))
    {
        LOG(VB_GENERAL, LOG_INFO, LOC +
            QString("Updating RSS Feed %1") .arg(site->GetTitle()));

        connect(site, &RSSSite::finished,
                this, &RSSManager::slotRSSRetrieved);
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

    // NOLINTNEXTLINE(modernize-loop-convert)
    for (auto i = m_sites.begin(); i != m_sites.end(); ++i)
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
    for (auto *video : std::as_const(rss))
    {
        // Insert in the DB here.
        insertRSSArticleInDB(site->GetTitle(), video, site->GetType());
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
                  ArticleType type,
                  QString  description,
                  QString  url,
                  QString  author,
                  bool download,
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
        connect(m_manager, &QNetworkAccessManager::finished, this,
                       &RSSSite::slotCheckRedirect);
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

std::chrono::minutes RSSSite::timeSinceLastUpdate(void) const
{
    QMutexLocker locker(&m_lock);

    QDateTime curTime(MythDate::current());
    auto secs = std::chrono::seconds(m_updated.secsTo(curTime));
    return duration_cast<std::chrono::minutes>(secs);
}

void RSSSite::process(void)
{
    QMutexLocker locker(&m_lock);

    m_articleList.clear();

    if (m_data.isEmpty())
    {
        emit finished(this);
        return;
    }

    QDomDocument domDoc;

#if QT_VERSION < QT_VERSION_CHECK(6,5,0)
    bool success = domDoc.setContent(m_data, true);
#else
    auto parseResult = domDoc.setContent(m_data, QDomDocument::ParseOption::UseNamespaceProcessing);
    bool success { parseResult };
#endif
    if (!success)
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
        items = Parse::parseRSS(domDoc);

        for (const auto *item : std::as_const(items))
        {
            insertRSSArticle(new ResultItem(
               item->GetTitle(), item->GetSortTitle(),
               item->GetSubtitle(), item->GetSortSubtitle(),
               item->GetDescription(), item->GetURL(),
               item->GetThumbnail(), item->GetMediaURL(),
               item->GetAuthor(), item->GetDate(),
               item->GetTime(), item->GetRating(),
               item->GetFilesize(), item->GetPlayer(),
               item->GetPlayerArguments(),
               item->GetDownloader(),
               item->GetDownloaderArguments(),
               item->GetWidth(),
               item->GetHeight(),
               item->GetLanguage(),
               item->GetDownloadable(),
               item->GetCountries(),
               item->GetSeason(),
               item->GetEpisode(), false));
        }
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Data is not valid RSS-feed");
    }

    emit finished(this);
}
