#include "netgrabbermanager.h"

// qt
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QString>
#include <utility>

#include "libmythbase/exitcodes.h"
#include "libmythbase/mythcorecontext.h" // for GetDurSetting TODO: excise database from MythCoreContext
#include "libmythbase/mythdate.h"
#include "libmythbase/mythdirs.h"
#include "libmythbase/mythlogging.h"
#include "libmythbase/mythsystemlegacy.h"

#include "netutils.h"

#define LOC      QString("NetContent: ")

// ---------------------------------------------------

GrabberScript::~GrabberScript()
{
    wait();
}

void GrabberScript::run()
{
    RunProlog();
    QMutexLocker locker(&m_lock);

    QString commandline = m_commandline;
    MythSystemLegacy getTree(commandline, QStringList("-T"),
                       kMSRunShell | kMSStdOut);
    getTree.Run(15min);
    uint status = getTree.Wait();

    if( status == GENERIC_EXIT_CMD_NOT_FOUND )
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("Internet Content Source %1 cannot run, file missing.")
                .arg(m_title));
    }
    else if( status == GENERIC_EXIT_OK )
    {
        LOG(VB_GENERAL, LOG_INFO, LOC +
            QString("Internet Content Source %1 completed download, "
                    "beginning processing...").arg(m_title));

        QByteArray result = getTree.ReadAll();

        QDomDocument domDoc;
#if QT_VERSION < QT_VERSION_CHECK(6,5,0)
        domDoc.setContent(result, true);
#else
        domDoc.setContent(result, QDomDocument::ParseOption::UseNamespaceProcessing);
#endif
        QDomElement root = domDoc.documentElement();
        QDomElement channel = root.firstChildElement("channel");

        clearTreeItems(m_title);

        while (!channel.isNull())
        {
            parseDBTree(m_title, QString(), QString(), channel, GetType());
            channel = channel.nextSiblingElement("channel");
        }
        markTreeUpdated(this, MythDate::current());
        LOG(VB_GENERAL, LOG_INFO, LOC +
            QString("Internet Content Source %1 completed processing, "
                    "marking as updated.").arg(m_title));
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("Internet Content Source %1 crashed while grabbing tree.")
                .arg(m_title));
    }

    // NOLINTNEXTLINE(readability-misleading-indentation)
    emit finished();
    RunEpilog();
}

void GrabberScript::parseDBTree(const QString &feedtitle, const QString &path,
                                const QString &pathThumb, QDomElement& domElem,
                                const ArticleType type)
{
    QMutexLocker locker(&m_lock);

    Parse parse;
    ResultItem::resultList articles;

    // File Handling
    QDomElement fileitem = domElem.firstChildElement("item");
    while (!fileitem.isNull())
    {   // Fill the article list...
        articles.append(Parse::ParseItem(fileitem));
        fileitem = fileitem.nextSiblingElement("item");
    }

    while (!articles.isEmpty())
    {   // Insert the articles in the DB...
        insertTreeArticleInDB(feedtitle, path,
                       pathThumb, articles.takeFirst(), type);
    }

    // Directory Handling
    QDomElement diritem = domElem.firstChildElement("directory");
    while (!diritem.isNull())
    {
        QDomElement subfolder = diritem;
        QString dirname = diritem.attribute("name");
        QString dirthumb = diritem.attribute("thumbnail");
        dirname.replace("/", "|");
        QString pathToUse;

        if (path.isEmpty())
            pathToUse = dirname;
        else
            pathToUse = QString("%1/%2").arg(path, dirname);

        parseDBTree(feedtitle,
                    pathToUse,
                    dirthumb,
                    subfolder,
                    type);
        diritem = diritem.nextSiblingElement("directory");
    }
}

GrabberManager::GrabberManager()
    : m_timer(new QTimer()),
      m_updateFreq(gCoreContext->GetDurSetting<std::chrono::hours>("netsite.updateFreq", 24h))
{
    connect( m_timer, &QTimer::timeout,
                      this, &GrabberManager::timeout);
}

GrabberManager::~GrabberManager()
{
    delete m_timer;
}

void GrabberManager::startTimer()
{
    m_timer->start(m_updateFreq);
}

void GrabberManager::stopTimer()
{
    m_timer->stop();
}

void GrabberManager::doUpdate()
{
    auto *gdt = new GrabberDownloadThread(this);
    if (m_refreshAll)
       gdt->refreshAll();
    gdt->start(QThread::LowPriority);

    m_timer->start(m_updateFreq);
}

void GrabberManager::timeout()
{
    QMutexLocker locker(&m_lock);
    doUpdate();
}

void GrabberManager::refreshAll()
{
    m_refreshAll = true;
}

GrabberDownloadThread::GrabberDownloadThread(QObject *parent) :
    MThread("GrabberDownload"),
    m_parent(parent)
{
}

GrabberDownloadThread::~GrabberDownloadThread()
{
    cancel();
    wait();
}

void GrabberDownloadThread::cancel()
{
    m_mutex.lock();
    qDeleteAll(m_scripts);
    m_scripts.clear();
    m_mutex.unlock();
}

void GrabberDownloadThread::refreshAll()
{
    m_mutex.lock();
    m_refreshAll = true;
    if (!isRunning())
        start();
    m_mutex.unlock();
}

void GrabberDownloadThread::run()
{
    RunProlog();

    m_scripts = findAllDBTreeGrabbers();
    auto updateFreq = gCoreContext->GetDurSetting<std::chrono::hours>(
               "netsite.updateFreq", 24h);

    while (!m_scripts.isEmpty())
    {
        GrabberScript *script = m_scripts.takeFirst();
        if (script && (needsUpdate(script, updateFreq) || m_refreshAll))
        {
            LOG(VB_GENERAL, LOG_INFO, LOC +
                QString("Internet Content Source %1 Updating...")
                    .arg(script->GetTitle()));
            script->run();
        }
        delete script;
    }
    emit finished();
    if (m_parent)
        QCoreApplication::postEvent(m_parent, new GrabberUpdateEvent());

    RunEpilog();
}

Search::Search()
{
    m_videoList.clear();
}

Search::~Search()
{
    resetSearch();

    delete m_searchProcess;
    m_searchProcess = nullptr;
}


void Search::executeSearch(const QString &script, const QString &query,
                           const QString &pagenum)
{
    resetSearch();

    LOG(VB_GENERAL, LOG_DEBUG, "Search::executeSearch");
    m_searchProcess = new MythSystemLegacy();

    connect(m_searchProcess, &MythSystemLegacy::finished,
            this, qOverload<>(&Search::slotProcessSearchExit));
    connect(m_searchProcess, &MythSystemLegacy::error,
            this, qOverload<uint>(&Search::slotProcessSearchExit));

    const QString& cmd = script;

    QStringList args;

    if (!pagenum.isEmpty())
    {
        args.append(QString("-p"));
        args.append(pagenum);
    }

    args.append("-S");
    const QString& term = query;
    args.append(MythSystemLegacy::ShellEscape(term));

    LOG(VB_GENERAL, LOG_INFO, LOC +
        QString("Internet Search Query: %1 %2").arg(cmd, args.join(" ")));

    uint flags = kMSRunShell | kMSStdOut | kMSRunBackground;
    m_searchProcess->SetCommand(cmd, args, flags);
    m_searchProcess->Run(40s);
}

void Search::resetSearch()
{
    qDeleteAll(m_videoList);
    m_videoList.clear();
}

void Search::process()
{
    Parse parse;
    m_videoList = Parse::parseRSS(m_document);

    QDomNodeList entries = m_document.elementsByTagName("channel");

    if (entries.count() == 0)
    {
        m_numResults = 0;
        m_numReturned = 0;
        m_numIndex = 0;
        return;
    }

    QDomNode itemNode = entries.item(0);

    QDomNode Node = itemNode.namedItem(QString("numresults"));
    if (!Node.isNull())
    {
        m_numResults = Node.toElement().text().toUInt();
    }
    else
    {
        QDomNodeList count = m_document.elementsByTagName("item");

        if (count.count() == 0)
            m_numResults = 0;
        else
            m_numResults = count.count();
    }

    Node = itemNode.namedItem(QString("returned"));
    if (!Node.isNull())
    {
        m_numReturned = Node.toElement().text().toUInt();
    }
    else
    {
        QDomNodeList items = m_document.elementsByTagName("item");

        if (items.count() == 0)
            m_numReturned = 0;
        else
            m_numReturned = items.count();
    }

    Node = itemNode.namedItem(QString("startindex"));
    if (!Node.isNull())
    {
        m_numIndex = Node.toElement().text().toUInt();
    }
    else
    {
        m_numIndex = 0;
    }

    Node = itemNode.namedItem(QString("nextpagetoken"));
    if (!Node.isNull())
    {
        m_nextPageToken = Node.toElement().text();
    }
    else
    {
        m_nextPageToken = "";
    }

    Node = itemNode.namedItem(QString("prevpagetoken"));
    if (!Node.isNull())
    {
        m_prevPageToken = Node.toElement().text();
    }
    else
    {
        m_prevPageToken = "";
    }
}

void Search::slotProcessSearchExit(uint exitcode)
{
    if (exitcode == GENERIC_EXIT_TIMEOUT)
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC + "Internet Search Timeout");

        if (m_searchProcess)
        {
            m_searchProcess->Term(true);
            m_searchProcess->deleteLater();
            m_searchProcess = nullptr;
        }
        emit searchTimedOut(this);
        return;
    }

    if (exitcode != GENERIC_EXIT_OK)
    {
        m_document.setContent(QString());
    }
    else
    {
        LOG(VB_GENERAL, LOG_INFO, LOC +
            "Internet Search Successfully Completed");

        m_data = m_searchProcess->ReadAll();
#if QT_VERSION < QT_VERSION_CHECK(6,5,0)
        m_document.setContent(m_data, true);
#else
        m_document.setContent(m_data, QDomDocument::ParseOption::UseNamespaceProcessing);
#endif
    }

    m_searchProcess->deleteLater();
    m_searchProcess = nullptr;
    emit finishedSearch(this);
}

void Search::slotProcessSearchExit(void)
{
    slotProcessSearchExit(GENERIC_EXIT_OK);
}

void Search::SetData(QByteArray data)
{
    m_data = std::move(data);
#if QT_VERSION < QT_VERSION_CHECK(6,5,0)
    m_document.setContent(m_data, true);
#else
    m_document.setContent(m_data, QDomDocument::ParseOption::UseNamespaceProcessing);
#endif
}
