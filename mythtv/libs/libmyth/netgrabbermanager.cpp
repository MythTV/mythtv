// qt
#include <QString>
#include <QCoreApplication>
#include <QFile>
#include <QDir>

#include "mythdirs.h"
#include "mythcontext.h"
#include "mythsystemlegacy.h"
#include "exitcodes.h"
#include "mythdate.h"
#include "mythlogging.h"
#include "mythsystemlegacy.h"

#include "netgrabbermanager.h"
#include "netutils.h"

#define LOC      QString("NetContent: ")

using namespace std;

// ---------------------------------------------------

GrabberScript::GrabberScript(const QString& title, const QString& image,
              const ArticleType &type, const QString& author,
              const bool& search, const bool& tree,
              const QString& description, const QString& commandline,
              const double& version) :
    MThread("GrabberScript"), m_lock(QMutex::Recursive)
{
    m_title = title;
    m_image = image;
    m_type = type;
    m_author = author;
    m_search = search;
    m_tree = tree;
    m_description = description;
    m_commandline = commandline;
    m_version = version;
}

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
    getTree.Run(900);
    uint status = getTree.Wait();

    if( status == GENERIC_EXIT_CMD_NOT_FOUND )
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("Internet Content Source %1 cannot run, file missing.")
                .arg(m_title));
    else if( status == GENERIC_EXIT_OK )
    {
        LOG(VB_GENERAL, LOG_INFO, LOC +
            QString("Internet Content Source %1 completed download, "
                    "beginning processing...").arg(m_title));

        QByteArray result = getTree.ReadAll();

	QDomDocument domDoc;
        domDoc.setContent(result, true);
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
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("Internet Content Source %1 crashed while grabbing tree.")
                .arg(m_title));

    emit finished();
    RunEpilog();
}

void GrabberScript::parseDBTree(const QString &feedtitle, const QString &path,
                                const QString &pathThumb, QDomElement& domElem,
                                const ArticleType &type)
{
    QMutexLocker locker(&m_lock);

    Parse parse;
    ResultItem::resultList articles;

    // File Handling
    QDomElement fileitem = domElem.firstChildElement("item");
    while (!fileitem.isNull())
    {   // Fill the article list...
        articles.append(parse.ParseItem(fileitem));
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
            pathToUse = QString("%1/%2").arg(path).arg(dirname);

        parseDBTree(feedtitle,
                    pathToUse,
                    dirthumb,
                    subfolder,
                    type);
        diritem = diritem.nextSiblingElement("directory");
    }
}

GrabberManager::GrabberManager() :     m_lock(QMutex::Recursive)
{
    m_updateFreq = (gCoreContext->GetNumSetting(
                       "netsite.updateFreq", 24) * 3600 * 1000);
    m_timer = new QTimer();
    m_runningCount = 0;
    m_refreshAll = false;
    connect( m_timer, SIGNAL(timeout()),
                      this, SLOT(timeout()));
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
    GrabberDownloadThread *gdt = new GrabberDownloadThread(this);
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
    MThread("GrabberDownload")
{
    m_parent = parent;
    m_refreshAll = false;
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
    uint updateFreq = gCoreContext->GetNumSetting(
               "netsite.updateFreq", 24);

    while (m_scripts.count())
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
    : m_searchProcess(NULL), m_numResults(0),
      m_numReturned(0), m_numIndex(0)
{
    m_videoList.clear();
}

Search::~Search()
{
    resetSearch();

    delete m_searchProcess;
    m_searchProcess = NULL;
}


void Search::executeSearch(const QString &script, const QString &query, uint pagenum)
{
    resetSearch();

    LOG(VB_GENERAL, LOG_DEBUG, "Search::executeSearch");
    m_searchProcess = new MythSystemLegacy();

    connect(m_searchProcess, SIGNAL(finished()),
            this, SLOT(slotProcessSearchExit()));
    connect(m_searchProcess, SIGNAL(error(uint)),
            this, SLOT(slotProcessSearchExit(uint)));

    QString cmd = script;

    QStringList args;

    if (pagenum > 1)
    {
        args.append(QString("-p"));
        args.append(QString::number(pagenum));
    }

    args.append("-S");
    QString term = query;
    args.append(MythSystemLegacy::ShellEscape(term));

    LOG(VB_GENERAL, LOG_DEBUG, LOC +
        QString("Internet Search Query: %1 %2") .arg(cmd).arg(args.join(" ")));

    uint flags = kMSRunShell | kMSStdOut | kMSRunBackground;
    m_searchProcess->SetCommand(cmd, args, flags);
    m_searchProcess->Run(40);
}

void Search::resetSearch()
{
    qDeleteAll(m_videoList);
    m_videoList.clear();
}

void Search::process()
{
    Parse parse;
    m_videoList = parse.parseRSS(m_document);

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
        QDomNodeList entries = m_document.elementsByTagName("item");

        if (entries.count() == 0)
            m_numReturned = 0;
        else
            m_numReturned = entries.count();
    }

    Node = itemNode.namedItem(QString("startindex"));
    if (!Node.isNull())
    {
        m_numIndex = Node.toElement().text().toUInt();
    }
    else
        m_numIndex = 0;

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
            m_searchProcess = NULL;
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
        m_document.setContent(m_data, true);
    }

    m_searchProcess->deleteLater();
    m_searchProcess = NULL;
    emit finishedSearch(this);
}

void Search::SetData(QByteArray data)
{
    m_data = data;
    m_document.setContent(m_data, true);

}

