// qt
#include <QString>
#include <QCoreApplication>
#include <QProcess>
#include <QFile>
#include <QDir>

#include <mythdirs.h>
#include <mythcontext.h>
#include <mythverbose.h>

#include "grabbermanager.h"
#include "treedbutil.h"

using namespace std;

// ---------------------------------------------------

GrabberScript::GrabberScript(const QString& title, const QString& image,
              const bool& search, const bool& tree,
              const QString& commandline) :
        m_lock(QMutex::Recursive)
{
    m_title = title;
    m_image = image;
    m_search = search;
    m_tree = tree;
    m_commandline = commandline;
}

GrabberScript::~GrabberScript()
{
}

void GrabberScript::run()
{
    QMutexLocker locker(&m_lock);

    QString commandline = m_commandline;
    m_getTree.setReadChannel(QProcess::StandardOutput);

    if (QFile(commandline).exists())
    {
        m_getTree.start(commandline, QStringList() << "-T");
        m_getTree.waitForFinished(900000);
        QDomDocument domDoc;

        if (QProcess::NormalExit != m_getTree.exitStatus())
        {
            VERBOSE(VB_IMPORTANT, QString("Script %1 crashed while grabbing tree.")
                              .arg(m_title));
            emit finished();
            return;
        }

        VERBOSE(VB_IMPORTANT, QString("MythNetvision: Script %1 completed download.")
                                  .arg(m_title));

        QByteArray result = m_getTree.readAll();

        domDoc.setContent(result, true);
        QDomElement root = domDoc.documentElement();
        QDomElement channel = root.firstChildElement("channel");

        clearTreeItems(m_title);

        while (!channel.isNull())
        {
            parseDBTree(m_title, QString(), QString(), channel);
            channel = channel.nextSiblingElement("channel");
        }
        markTreeUpdated(this, QDateTime::currentDateTime());
        emit finished();
    }
    else
        emit finished();
}

void GrabberScript::parseDBTree(const QString &feedtitle, const QString &path,
                                const QString &pathThumb, QDomElement& domElem)
{
    QMutexLocker locker(&m_lock);

    Parse parse;

    // File Handling
    QDomElement fileitem = domElem.firstChildElement("item");
    while (!fileitem.isNull())
    {
        insertTreeArticleInDB(feedtitle, path,
                       pathThumb, parse.ParseItem(fileitem));
        fileitem = fileitem.nextSiblingElement("item");
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
                    subfolder);
        diritem = diritem.nextSiblingElement("directory");
    }
}

GrabberManager::GrabberManager() :     m_lock(QMutex::Recursive)
{
    m_updateFreq = (gCoreContext->GetNumSetting(
                       "mythNetvision.updateFreq", 24) * 3600 * 1000);
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

GrabberDownloadThread::GrabberDownloadThread(QObject *parent)
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
    m_mutex.unlock();
}

void GrabberDownloadThread::run()
{
    m_scripts = findAllDBTreeGrabbers();
    uint updateFreq = gCoreContext->GetNumSetting(
               "mythNetvision.updateFreq", 24);

    while (m_scripts.count())
    {
        GrabberScript *script = m_scripts.takeFirst();
        if (script && (needsUpdate(script, updateFreq) || m_refreshAll))
        {
            VERBOSE(VB_IMPORTANT, QString("MythNetvision: Script %1 Updating...")
                                  .arg(script->GetTitle()));
            script->run();
        }
        delete script;
    }
    QCoreApplication::postEvent(m_parent, new GrabberUpdateEvent());
}

