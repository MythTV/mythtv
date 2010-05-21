#include <QtAlgorithms>
#include <QFile>
#include <QDataStream>
#include <QDomDocument>
#include <QProcess>
#include <QDomImplementation>
#include <QHash>

#include "parse.h"
#include "search.h"

#include <mythcontext.h>
#include <mythdirs.h>

using namespace std;

//========================================================================================
//          Search Construction, Destruction
//========================================================================================

Search::Search()
    : m_searchProcess(NULL)
{
    m_videoList.clear();
    m_searchtimer = new QTimer();

    m_searchtimer->setSingleShot(true);
}

Search::~Search()
{
    resetSearch();

    delete m_searchProcess;
    m_searchProcess = NULL;

    if (m_searchtimer)
    {
        m_searchtimer->disconnect();
        m_searchtimer->deleteLater();
        m_searchtimer = NULL;
    }
}


//========================================================================================
//          Public Function
//========================================================================================

void Search::executeSearch(const QString &script, const QString &query, uint pagenum)
{
    resetSearch();

    m_searchProcess = new QProcess();

    connect(m_searchProcess, SIGNAL(finished(int, QProcess::ExitStatus)),
                  SLOT(slotProcessSearchExit(int, QProcess::ExitStatus)));
    connect(m_searchtimer, SIGNAL(timeout()), this, SLOT(slotSearchTimeout()));

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

    m_searchtimer->start(40 * 1000);
    m_searchProcess->start(cmd, args);
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

//========================================================================================
//          Slots
//========================================================================================


void Search::slotProcessSearchExit(int exitcode, QProcess::ExitStatus exitstatus)
{
    if (m_searchtimer)
        m_searchtimer->stop();

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
