// Program Name: internetContent.cpp
//
// Purpose - Html & XML status HttpServerExtension
//
// Created By  : David Blain                    Created On : Oct. 24, 2005
// Modified By : Daniel Kristjansson            Modified On: Oct. 31, 2007
//
//////////////////////////////////////////////////////////////////////////////

// Qt
#include <QBuffer>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QImage>
#include <QTextStream>

// MythTV
#include "libmythbase/mythcorecontext.h"
#include "libmythbase/mythdate.h"
#include "libmythbase/mythdirs.h"
#include "libmythbase/mythsystemlegacy.h"
#include "libmythbase/netgrabbermanager.h"
#include "libmythbase/rssparse.h"

// MythBackend
#include "internetContent.h"

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
                    .arg(pRequest->m_sMethod,
                         pRequest->m_sRawRequest));

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

    if (grabber.isEmpty() || query.isEmpty())
        return;

    QString command = QString("%1internetcontent/%2")
                      .arg(GetShareDir(), grabber);

    if (!QFile::exists(command))
    {
        pRequest->FormatRawResponse( QString("<HTML>Grabber %1 does "
                  "not exist!</HTML>").arg(command) );
        return;
    }

    auto *search = new Search();
    QEventLoop loop;

    QObject::connect(search, &Search::finishedSearch,
                     &loop, &QEventLoop::quit);
    QObject::connect(search, &Search::searchTimedOut,
                     &loop, &QEventLoop::quit);

    search->executeSearch(command, query, page);
    loop.exec();

    search->process();

    QDomDocument ret;
    ret.setContent(search->GetData());

    delete search;

    if (ret.isNull())
        return;

    pRequest->FormatRawResponse( ret.toString() );
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

    for (const auto & name : std::as_const(Grabbers))
    {
        QString commandline = GrabberDir + name;
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
