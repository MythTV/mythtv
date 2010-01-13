// qt
#include <QString>
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

    m_getTree = new QProcess;

    connect(m_getTree, SIGNAL(finished(int, QProcess::ExitStatus)),
                      this, SLOT(updateTreeFinished()));    

    QString commandline = m_commandline;
    m_getTree->setReadChannel(QProcess::StandardOutput);
    if (QFile(commandline).exists())
        m_getTree->start(commandline, QStringList() << "-T");
    else
        emit finished();
}

void GrabberScript::updateTreeFinished()
{
    QMutexLocker locker(&m_lock);

    QDomDocument domDoc;

    if (QProcess::NormalExit != m_getTree->exitStatus())
    {
        VERBOSE(VB_IMPORTANT, QString("Script %1 crashed while grabbing tree.")
                              .arg(m_title));
        emit finished();
        return;
    }

    VERBOSE(VB_IMPORTANT, QString("MythNetvision: Script %1 completed download.")
                              .arg(m_title));

    QByteArray result = m_getTree->readAll();

    domDoc.setContent(result, true);
    QDomElement root = domDoc.documentElement();
    QDomElement channel = root.firstChildElement("channel");

    clearTreeItems(m_title);

    while (!channel.isNull())
    {
        parseDBTree(m_title, QString(), QString(), channel);
        channel = channel.nextSiblingElement("channel");
    }
    emit finished();
}

void GrabberScript::parseDBTree(const QString &feedtitle, const QString &path,
                                const QString &pathThumb, QDomElement& domElem)
{
    QMutexLocker locker(&m_lock);

    Parse *parse = new Parse();

    // File Handling
    QDomElement fileitem = domElem.firstChildElement("item");
    while (!fileitem.isNull())
    {
        insertTreeArticleInDB(feedtitle, path,
                       pathThumb, parse->ParseItem(fileitem));
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
    m_updateFreq = (gContext->GetNumSetting(
                       "mythNetvision.updateFreq", 6) * 3600 * 1000);
    m_timer = new QTimer();
    m_runningCount = 0;
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
    m_scripts = findAllDBTreeGrabbers();

    GrabberScript *s;
    for (int x = 0; x < m_scripts.count(); x++)
    {
        s = m_scripts.at(x);
        VERBOSE(VB_IMPORTANT, QString("MythNetvision: Updating Script %1")
                              .arg(s->GetTitle()));
        connect(s, SIGNAL(finished()),
                          this, SLOT(grabberFinished()));
        m_runningCount++;
        s->run();
    }

    m_timer->start(m_updateFreq);
}

void GrabberManager::grabberFinished()
{
    QMutexLocker locker(&m_lock);

    m_runningCount--;
    if (m_runningCount == 0)
        emit finished();
}

void GrabberManager::timeout()
{
    QMutexLocker locker(&m_lock);
    doUpdate();
}

