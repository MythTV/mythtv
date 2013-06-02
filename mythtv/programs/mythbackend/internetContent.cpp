// Program Name: internetContent.cpp
//
// Purpose - Html & XML status HttpServerExtension
//
// Created By  : David Blain                    Created On : Oct. 24, 2005
// Modified By : Daniel Kristjansson            Modified On: Oct. 31, 2007
//
//////////////////////////////////////////////////////////////////////////////

#include <QTextStream>
#include <QDir>
#include <QFile>
#include <QRegExp>
#include <QBuffer>
#include <QEventLoop>
#include <QImage>

#include "internetContent.h"

#include "mythcorecontext.h"
#include "mythdate.h"
#include "mythsystemlegacy.h"
#include "mythdirs.h"

#include "rssparse.h"
#include "netgrabbermanager.h"

#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
#include "netutils.h"
#endif

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

InternetContent::InternetContent( const QString &sSharePath)
        : HttpServerExtension( "InternetContent", sSharePath)
{
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

InternetContent::~InternetContent()
{
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

QStringList InternetContent::GetBasePaths()
{
    return QStringList( "/InternetContent" );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

bool InternetContent::ProcessRequest( HTTPRequest *pRequest )
{
    try
    {
        if (pRequest)
        {
            if (pRequest->m_sBaseUrl != "/InternetContent")
                return false;

            LOG(VB_UPNP, LOG_INFO,
                QString("InternetContent::ProcessRequest: %1 : %2")
                    .arg(pRequest->m_sMethod)
                    .arg(pRequest->m_sRawRequest));

            // --------------------------------------------------------------

            if (pRequest->m_sMethod == "GetInternetSearch")
            {
                GetInternetSearch( pRequest );
                return true;
            }
            
            // --------------------------------------------------------------

            if (pRequest->m_sMethod == "GetInternetSources")
            {
                GetInternetSources( pRequest );
                return true;
            }

            // --------------------------------------------------------------

            if (pRequest->m_sMethod == "GetInternetContent")
            {
                GetInternetContent( pRequest );
                return true;
            }
        }
    }
    catch( ... )
    {
        LOG(VB_GENERAL, LOG_ERR,
            "InternetContent::ProcessRequest() - Unexpected Exception" );
    }

    return false;
}

// ==========================================================================
// Request handler Methods
// ==========================================================================

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void InternetContent::GetInternetSearch( HTTPRequest *pRequest )
{
    pRequest->m_eResponseType   = ResponseTypeHTML;

    QString grabber =  pRequest->m_mapParams[ "Grabber" ];
    QString query   =  pRequest->m_mapParams[ "Query" ];
    QString page    =  pRequest->m_mapParams[ "Page" ];

    if (grabber.isEmpty() || query.isEmpty() || page.isEmpty())
        return;

    uint pagenum = page.toUInt();
    QString command = QString("%1internetcontent/%2").arg(GetShareDir())
                        .arg(grabber);

    if (!QFile::exists(command))
    {
        pRequest->FormatRawResponse( QString("<HTML>Grabber %1 does "
                  "not exist!</HTML>").arg(command) );
        return;
    }

    LOG(VB_GENERAL, LOG_INFO,
        QString("InternetContent::GetInternetSearch Executing "
                "Command: %1 -p %2 -S '%3'")
            .arg(command).arg(pagenum).arg(query));

#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
    Search *search = new Search();
    QEventLoop loop;

    QObject::connect(search, SIGNAL(finishedSearch(Search *)),
                     &loop, SLOT(quit(void)));
    QObject::connect(search, SIGNAL(searchTimedOut(Search *)),
                     &loop, SLOT(quit(void)));

    search->executeSearch(command, query, pagenum);
    loop.exec();

    search->process();

    QDomDocument ret;
    ret.setContent(search->GetData());

    delete search;

    if (ret.isNull())
        return;

    pRequest->FormatRawResponse( ret.toString() );
#else
#warning InternetContent::GetInternetSearch() not yet ported to Qt5
#endif
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void InternetContent::GetInternetSources( HTTPRequest *pRequest )
{
    pRequest->m_eResponseType   = ResponseTypeHTML;

    QString ret;
    QString GrabberDir = QString("%1/internetcontent/").arg(GetShareDir());
    QDir GrabberPath(GrabberDir);
    QStringList Grabbers = GrabberPath.entryList(QDir::Files | QDir::Executable);

    for (QStringList::const_iterator i = Grabbers.begin();
            i != Grabbers.end(); ++i)
    {
        QString commandline = GrabberDir + (*i);
        MythSystemLegacy scriptcheck(commandline, QStringList("-v"),
                               kMSRunShell | kMSStdOut);
        scriptcheck.Run();
        scriptcheck.Wait();
        QByteArray result = scriptcheck.ReadAll();

        if (!result.isEmpty() && result.toLower().startsWith("<grabber>"))
            ret += result;
    }

    NameValues list;

    list.push_back( NameValue( "InternetContent", ret ));

    pRequest->FormatActionResponse( list );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void InternetContent::GetInternetContent( HTTPRequest *pRequest )
{
    pRequest->m_eResponseType   = ResponseTypeHTML;

    QString grabber =  pRequest->m_mapParams[ "Grabber" ];

    if (grabber.isEmpty())
        return;

    QString contentDir = QString("%1internetcontent/").arg(GetShareDir());
    QString htmlFile(contentDir + grabber);

    // Try to prevent directory traversal
    QFileInfo fileInfo(htmlFile);
    if (fileInfo.canonicalFilePath().startsWith(contentDir) &&
        QFile::exists( htmlFile ))
    {
        pRequest->m_eResponseType   = ResponseTypeFile;
        pRequest->m_nResponseStatus = 200;
        pRequest->m_sFileName       = htmlFile;
    }
    else
    {
        pRequest->FormatRawResponse( QString("<HTML>File %1 does "
                  "not exist!</HTML>").arg(htmlFile) );
    }
}

// vim:set shiftwidth=4 tabstop=4 expandtab:
