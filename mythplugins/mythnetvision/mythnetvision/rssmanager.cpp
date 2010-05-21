// qt
#include <QString>
#include <QProcess>
#include <QFile>
#include <QDir>

#include <mythdirs.h>
#include <mythcontext.h>
#include <mythverbose.h>

#include "rssmanager.h"
#include "netutils.h"
#include "parse.h"

using namespace std;

#define LOC      QString("RSSSite: ")
#define LOC_WARN QString("RSSSite, Warning: ")
#define LOC_ERR  QString("RSSSite, Error: ")

// ---------------------------------------------------

RSSManager::RSSManager()
{
    m_updateFreq = (gCoreContext->GetNumSetting(
                       "mythNetvision.updateFreq", 6) * 3600 * 1000);

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
        VERBOSE(VB_GENERAL|VB_EXTRA, QString("MythNetvision: Updating RSS Feed %1")
                                      .arg((*i)->GetTitle()));

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

//    site->retrieve();

    clearRSSArticles(site->GetTitle());

    ResultVideo::resultList rss = site->GetVideoList();
    ResultVideo::resultList::iterator it = rss.begin();
    for (; it != rss.end(); ++it)
    {
        // Insert in the DB here.
        insertArticleInDB(site->GetTitle(), *it);
        m_inprogress.removeOne(site);
    }

    if (!m_inprogress.count())
        emit finished();
}

void RSSManager::slotRSSRetrieved(RSSSite *site)
{
    markUpdated(site);
    processAndInsertRSS(site);
}


RSSSite::RSSSite(const QString& title,
                  const QString& image,
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
    m_description = description;
    m_url = url;
    m_destDir = GetConfDir()+"/MythNetvision";
    m_author = author;
    m_download = download;
    m_updated = updated;
}

RSSSite::~RSSSite()
{
}

void RSSSite::insertRSSArticle(ResultVideo *item)
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
    m_manager = new QNetworkAccessManager();

    m_reply = m_manager->get(QNetworkRequest(m_urlReq));

    connect(m_manager, SIGNAL(finished(QNetworkReply*)), this,
                       SLOT(slotCheckRedirect(QNetworkReply*)));
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

    QUrl urlRedirectedTo = redirectUrl(possibleRedirectUrl.toUrl(),
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

ResultVideo::resultList RSSSite::GetVideoList(void) const
{
    QMutexLocker locker(&m_lock);
    return m_articleList;
}

unsigned int RSSSite::timeSinceLastUpdate(void) const
{
    QMutexLocker locker(&m_lock);

    QDateTime curTime(QDateTime::currentDateTime());
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
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Failed to set content from downloaded XML");
        return;
    }

    //Check the type of the feed
    QString rootName = domDoc.documentElement().nodeName();
    if (rootName == "rss" || rootName == "rdf:RDF")
    {
        ResultVideo::resultList items;
        Parse parser;
        items = parser.parseRSS(domDoc);

        for (ResultVideo::resultList::iterator i = items.begin();
                i != items.end(); ++i)
        {
            insertRSSArticle(new ResultVideo((*i)->GetTitle(),
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
               (*i)->GetEpisode()));
        }
        emit finished(this);
    }
    else
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Data is not valid RSS-feed");
        emit finished(this);
    }
}
