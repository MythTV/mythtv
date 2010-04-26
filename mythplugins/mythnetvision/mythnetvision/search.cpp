#include <QtAlgorithms>
#include <QFile>
#include <QDataStream>
#include <QDomDocument>
#include <QProcess>
#include <QDomImplementation>
#include <QHash>

#include "parse.h"
#include "search.h"

#include <mythtv/mythcontext.h>
#include <mythtv/mythdirs.h>

using namespace std;

//========================================================================================
//          Search Construction, Destruction
//========================================================================================

Search::Search()
    : m_searchProcess(NULL)
{
    m_videoList.clear();
    m_timer = new QTimer();

    m_timer->setSingleShot(true);
}

Search::~Search()
{
    reset();

    delete m_searchProcess;
    m_searchProcess = NULL;

    if (m_timer)
    {
        m_timer->disconnect();
        m_timer->deleteLater();
        m_timer = NULL;
    }
}


//========================================================================================
//          Public Function
//========================================================================================

void Search::executeSearch(const QString &script, const QString &query, uint pagenum)
{
    reset();

    m_searchProcess = new QProcess();

    connect(m_searchProcess, SIGNAL(finished(int, QProcess::ExitStatus)),
                  SLOT(slotProcessSearchExit(int, QProcess::ExitStatus)));
    connect(m_timer, SIGNAL(timeout()), this, SLOT(slotSearchTimeout()));

    QString cmd = script;

    QStringList args;

    if (pagenum > 1)
    {
        args.append(QString("-p"));
        args.append(QString::number(pagenum));
    }

    args.append("-S");
    args.append(query);

    VERBOSE(VB_GENERAL|VB_EXTRA, QString("MythNetVision Query: %1 %2")
                                        .arg(cmd).arg(args.join(" ")));

    m_timer->start(40 * 1000);
    m_searchProcess->start(cmd, args);
}

void Search::reset()
{
    qDeleteAll(m_videoList);
    m_videoList.clear();
}

uint Search::numResults()
{
    return parseNumResults(m_document);
}

uint Search::numReturned()
{
    return parseNumReturned(m_document);
}

uint Search::numIndex()
{
    return parseNumIndex(m_document);
}

void Search::process()
{
    Parse parse;
    m_videoList = parse.parseRSS(m_document);
}

uint Search::parseNumResults(QDomDocument domDoc)
{
    QDomNodeList entries = domDoc.elementsByTagName("channel");

    if (entries.count() == 0)
        return 0;

    QDomNode itemNode = entries.item(0);

    QDomNode Node = itemNode.namedItem(QString("numresults"));
    if (!Node.isNull())
    {
        return Node.toElement().text().toUInt();
    }
    else
    {
        QDomNodeList count = domDoc.elementsByTagName("item");

        if (count.count() == 0)
            return 0;
        else
            return count.count();
    }
}

uint Search::parseNumReturned(QDomDocument domDoc)
{
    QDomNodeList entries = domDoc.elementsByTagName("channel");

    if (entries.count() == 0)
        return 0;

    QDomNode itemNode = entries.item(0);

    QDomNode Node = itemNode.namedItem(QString("returned"));
    if (!Node.isNull())
    {
        return Node.toElement().text().toUInt();
    }
    else
    {
        QDomNodeList entries = domDoc.elementsByTagName("item");

        if (entries.count() == 0)
            return 0;
        else
            return entries.count();
    }
}

uint Search::parseNumIndex(QDomDocument domDoc)
{
    QDomNodeList entries = domDoc.elementsByTagName("channel");

    if (entries.count() == 0)
        return 0;

    QDomNode itemNode = entries.item(0);

    QDomNode Node = itemNode.namedItem(QString("startindex"));
    if (!Node.isNull())
    {
        return Node.toElement().text().toUInt();
    }
    else
        return 0;
}

//========================================================================================
//          Manage video list
//========================================================================================


ResultVideo::resultList Search::GetVideoList()
{
    return m_videoList;
}


//========================================================================================
//          Slots
//========================================================================================


void Search::slotProcessSearchExit(int exitcode, QProcess::ExitStatus exitstatus)
{
    if (m_timer)
        m_timer->stop();

    if ((exitstatus != QProcess::NormalExit) ||
        (exitcode != 0))
    {
        m_document.setContent(QString());
    }
    else
    {
        VERBOSE(VB_GENERAL|VB_EXTRA, "MythNetVision: Script Execution Successfully Completed");

        m_data = m_searchProcess->readAllStandardOutput();
        m_document.setContent(m_data, true);
    }

    m_searchProcess->deleteLater();
    m_searchProcess = NULL;
    emit finishedSearch(this);
}

void Search::slotSearchTimeout()
{
    VERBOSE(VB_GENERAL|VB_EXTRA, "MythNetVision: Search Timeout");

    if (m_searchProcess)
    {
        m_searchProcess->close();
        m_searchProcess->deleteLater();
        m_searchProcess = NULL;
    }
    emit searchTimedOut(this);
}
