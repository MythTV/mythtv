// qt
#include <QString>
#include <QProcess>
#include <QFile>
#include <QDir>

#include <mythdirs.h>
#include <mythcontext.h>
#include <mythverbose.h>

#include "rssmanager.h"
#include "rssdbutil.h"
#include "parse.h"

using namespace std;

#define LOC      QString("RSSSite: ")
#define LOC_WARN QString("RSSSite, Warning: ")
#define LOC_ERR  QString("RSSSite, Error: ")

// ---------------------------------------------------

RSSManager::RSSManager()
{
    m_updateFreq = (gContext->GetNumSetting(
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
//            processAndInsertRSS(*i);
    }
}

void RSSManager::processAndInsertRSS(RSSSite *site)
{
    if (!site)
        return;

    site->process();

    clearRSSArticles(site->GetTitle());

    ResultVideo::resultList rss = site->GetVideoList();
    ResultVideo::resultList::iterator it = rss.begin();
    for (; it != rss.end(); ++it)
    {
        // Insert in the DB here.
        insertArticleInDB(site->GetTitle(), *it);
    }

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
    m_state(RSSSite::Success),
    m_errorString(QString::null),
    m_updateErrorString(QString::null)
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
    QMutexLocker locker(&m_lock);
    MythHttpPool::GetSingleton()->RemoveListener(this);
}

void RSSSite::deleteLater()
{
    QMutexLocker locker(&m_lock);
    MythHttpPool::GetSingleton()->RemoveListener(this);
    m_articleList.clear();
    QObject::deleteLater();
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
    stop();
    m_state = RSSSite::Retrieving;
    m_data.resize(0);
    m_errorString = QString::null;
    m_updateErrorString = QString::null;
    m_articleList.clear();
    m_urlReq = QUrl(m_url);
    MythHttpPool::GetSingleton()->AddUrlRequest(m_urlReq, this);
}

void RSSSite::stop(void)
{
    QMutexLocker locker(&m_lock);
    MythHttpPool::GetSingleton()->RemoveUrlRequest(m_urlReq, this);
}

bool RSSSite::successful(void) const
{
    QMutexLocker locker(&m_lock);
    return (m_state == RSSSite::Success);
}

QString RSSSite::errorMsg(void) const
{
    QMutexLocker locker(&m_lock);
    return m_errorString;
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

void RSSSite::Update(QHttp::Error      error,
                      const QString    &error_str,
                      const QUrl       &url,
                      uint              http_status_id,
                      const QString    &http_status_str,
                      const QByteArray &data)
{
    QMutexLocker locker(&m_lock);

    if (url != m_urlReq)
    {
        return;
    }

    if (QHttp::NoError != error)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "HTTP Connection Error" +
                QString("\n\t\t\tExplanation: %1: %2")
                .arg(error).arg(error_str));

        m_state = RSSSite::RetrieveFailed;
        m_updateErrorString = QString("%1: %2").arg(error).arg(error_str);
        emit finished(this);
        return;
    }

    if (200 != http_status_id)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "HTTP Protocol Error" +
                QString("\n\t\t\tExplanation: %1: %2")
                .arg(http_status_id).arg(http_status_str));

        m_state = RSSSite::RetrieveFailed;
        m_updateErrorString =
            QString("%1: %2").arg(http_status_id).arg(http_status_str);
        emit finished(this);
        return;
    }

    m_updateErrorString = QString::null;
    m_data = data;

    if (m_title.isEmpty())
    {
        m_state = RSSSite::WriteFailed;
    }
    else
    {
        m_updated = QDateTime::currentDateTime();
        m_state = RSSSite::Success;
    }
    emit finished(this);
}

void RSSSite::process(void)
{
    QMutexLocker locker(&m_lock);

    m_articleList.clear();

    m_errorString = "";
    if (RetrieveFailed == m_state)
        m_errorString = tr("Retrieve Failed. ")+"\n";

    QDomDocument domDoc;

    if (!domDoc.setContent(m_data, true))
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Failed to set content from xmlfile");
        m_errorString += tr("Failed to read downloaded file.");
        if (!m_updateErrorString.isEmpty())
            m_errorString += "\n" + m_updateErrorString;
        return;
    }

    if (RetrieveFailed == m_state)
    {
        m_errorString += tr("Showing Cached News.");
        if (!m_updateErrorString.isEmpty())
            m_errorString += "\n" + m_updateErrorString;
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
        return;
    }
    else
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "XML-file is not valid RSS-feed");
        m_errorString += tr("XML-file is not valid RSS-feed");
        return;
    }
}
