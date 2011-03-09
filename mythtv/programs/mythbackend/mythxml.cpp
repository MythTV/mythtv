// Program Name: MythXML.cpp
//
// Purpose - Html & XML status HttpServerExtension
//
// Created By  : David Blain                    Created On : Oct. 24, 2005
// Modified By : Daniel Kristjansson            Modified On: Oct. 31, 2007
//
//////////////////////////////////////////////////////////////////////////////

#include <cmath>

#include <QTextStream>
#include <QDir>
#include <QFile>
#include <QRegExp>
#include <QBuffer>
#include <QEventLoop>
#include <QImage>

#include "compat.h"
#include "mythxml.h"
#include "backendutil.h"

#include "mythcorecontext.h"
#include "util.h"
#include "mythdbcon.h"
#include "mythdb.h"
#include "mythdirs.h"

#include "previewgenerator.h"
#include "backendutil.h"
#include "mythconfig.h"
#include "programinfo.h"
#include "channelutil.h"
#include "storagegroup.h"
#include "mythsystem.h"

#include "rssparse.h"
#include "netutils.h"
#include "netgrabbermanager.h"

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

MythXML::MythXML( UPnpDevice *pDevice , const QString &sSharePath)
  : Eventing( "MythXML", "MYTHTV_Event", sSharePath), m_bIsMaster(false)
{
    m_nPreRollSeconds = gCoreContext->GetNumSetting("RecordPreRoll", 0);

    // Add any event variables...

    //  --- none at this time.

    QString sUPnpDescPath = UPnp::g_pConfig->GetValue( "UPnP/DescXmlPath",
                                                                m_sSharePath );

    m_sServiceDescFileName = sUPnpDescPath + "MXML_scpd.xml";
    m_sControlUrl          = "/Myth";

    // Add our Service Definition to the device.

    RegisterService( pDevice );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

MythXML::~MythXML()
{
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

MythXMLMethod MythXML::GetMethod( const QString &sURI )
{
    if (sURI == "GetServDesc"           ) return MXML_GetServiceDescription;

    if (sURI == "GetInternetSearch"     ) return MXML_GetInternetSearch;
    if (sURI == "GetInternetSources"    ) return MXML_GetInternetSources;
    if (sURI == "GetInternetContent"    ) return MXML_GetInternetContent;

    return MXML_Unknown;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

bool MythXML::ProcessRequest( HttpWorkerThread *pThread, HTTPRequest *pRequest )
{
    try
    {
        if (pRequest)
        {
            if (pRequest->m_sBaseUrl == "/Myth/GetInternetSearch")
            {
                pRequest->m_sBaseUrl = m_sControlUrl;
                pRequest->m_sMethod = "GetInternetSearch";
            }
            else if (pRequest->m_sBaseUrl == "/Myth/GetInternetSources")
            {
                pRequest->m_sBaseUrl = m_sControlUrl;
                pRequest->m_sMethod = "GetInternetSources";
            }

            if (pRequest->m_sBaseUrl != m_sControlUrl)
                return false;

            VERBOSE(VB_UPNP, QString("MythXML::ProcessRequest: %1 : %2")
                    .arg(pRequest->m_sMethod)
                    .arg(pRequest->m_sRawRequest));

            switch( GetMethod( pRequest->m_sMethod ))
            {
                case MXML_GetServiceDescription:
                    pRequest->FormatFileResponse( m_sServiceDescFileName );
                    return true;

                case MXML_GetInternetSearch :
                    GetInternetSearch( pRequest );
                    return true;
                case MXML_GetInternetSources :
                    GetInternetSources( pRequest );
                    return true;
                case MXML_GetInternetContent :
                    GetInternetContent( pRequest );
                    return true;

                default:
                {
                  //  UPnp::FormatErrorResponse( pRequest,
                  //                             UPnPResult_InvalidAction );

                    return false;
                }
            }
        }
    }
    catch( ... )
    {
        VERBOSE( VB_IMPORTANT,
                 "MythXML::ProcessRequest() - Unexpected Exception" );
    }

    return false;
}

// ==========================================================================
// Request handler Methods
// ==========================================================================

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void MythXML::GetInternetSearch( HTTPRequest *pRequest )
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

    VERBOSE(VB_GENERAL, QString("MythXML::GetInternetSearch Executing "
            "Command: %1 -p %2 -S '%3'").arg(command).arg(pagenum).arg(query));

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
}

void MythXML::GetInternetSources( HTTPRequest *pRequest )
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
        MythSystem scriptcheck(commandline, QStringList("-v"),
                               kMSRunShell | kMSStdOut | kMSBuffered);
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

void MythXML::GetInternetContent( HTTPRequest *pRequest )
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

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
//
// Static Methods
//
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

void MythXML::FillProgramInfo(QDomDocument *pDoc,
                              QDomNode     &node,
                              ProgramInfo  *pInfo,
                              bool          bIncChannel /* = true */,
                              bool          bDetails    /* = true */)
{
    if ((pDoc == NULL) || (pInfo == NULL))
        return;

    // Build Program Element

    QDomElement program = pDoc->createElement( "Program" );
    node.appendChild( program );

    program.setAttribute( "startTime"   ,
                          pInfo->GetScheduledStartTime(ISODate));
    program.setAttribute( "endTime"     , pInfo->GetScheduledEndTime(ISODate));
    program.setAttribute( "title"       , pInfo->GetTitle()   );
    program.setAttribute( "subTitle"    , pInfo->GetSubtitle());
    program.setAttribute( "category"    , pInfo->GetCategory());
    program.setAttribute( "catType"     , pInfo->GetCategoryType());
    program.setAttribute( "repeat"      , pInfo->IsRepeat()   );

    if (bDetails)
    {

        program.setAttribute( "seriesId"    , pInfo->GetSeriesID()     );
        program.setAttribute( "programId"   , pInfo->GetProgramID()    );
        program.setAttribute( "stars"       , pInfo->GetStars()        );
        program.setAttribute( "fileSize"    ,
                              QString::number( pInfo->GetFilesize() ));
        program.setAttribute( "lastModified",
                              pInfo->GetLastModifiedTime(ISODate) );
        program.setAttribute( "programFlags", pInfo->GetProgramFlags() );
        program.setAttribute( "hostname"    , pInfo->GetHostname() );

        if (pInfo->GetOriginalAirDate().isValid())
            program.setAttribute( "airdate"  , pInfo->GetOriginalAirDate()
                                               .toString(Qt::ISODate) );

        QDomText textNode = pDoc->createTextNode( pInfo->GetDescription() );
        program.appendChild( textNode );

    }

    if ( bIncChannel )
    {
        // Build Channel Child Element

        QDomElement channel = pDoc->createElement( "Channel" );
        program.appendChild( channel );

        FillChannelInfo( channel, pInfo, bDetails );
    }

    // Build Recording Child Element

    if ( pInfo->GetRecordingStatus() != rsUnknown )
    {
        QDomElement recording = pDoc->createElement( "Recording" );
        program.appendChild( recording );

        recording.setAttribute( "recStatus"     ,
                                pInfo->GetRecordingStatus()   );
        recording.setAttribute( "recPriority"   ,
                                pInfo->GetRecordingPriority() );
        recording.setAttribute( "recStartTs"    ,
                                pInfo->GetRecordingStartTime(ISODate) );
        recording.setAttribute( "recEndTs"      ,
                                pInfo->GetRecordingEndTime(ISODate) );

        if (bDetails)
        {
            recording.setAttribute( "recordId"      ,
                                    pInfo->GetRecordingRuleID() );
            recording.setAttribute( "recGroup"      ,
                                    pInfo->GetRecordingGroup() );
            recording.setAttribute( "playGroup"     ,
                                    pInfo->GetPlaybackGroup() );
            recording.setAttribute( "recType"       ,
                                    pInfo->GetRecordingRuleType() );
            recording.setAttribute( "dupInType"     ,
                                    pInfo->GetDuplicateCheckSource() );
            recording.setAttribute( "dupMethod"     ,
                                    pInfo->GetDuplicateCheckMethod() );
            recording.setAttribute( "encoderId"     ,
                                    pInfo->GetCardID() );
            const RecordingInfo ri(*pInfo);
            recording.setAttribute( "recProfile"    ,
                                    ri.GetProgramRecordingProfile());
            //recording.setAttribute( "preRollSeconds", m_nPreRollSeconds );
        }
    }
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void MythXML::FillChannelInfo( QDomElement &channel,
                                  ProgramInfo *pInfo,
                                  bool         bDetails  /* = true */ )
{
    if (pInfo)
    {
/*
        QString sHostName = gCoreContext->GetHostName();
        QString sPort     = gCoreContext->GetSettingOnHost( "BackendStatusPort",
                                                        sHostName);
        QString sIconURL  = QString( "http://%1:%2/getChannelIcon?ChanId=%3" )
                                   .arg( sHostName )
                                   .arg( sPort )
                                   .arg( pInfo->chanid );
*/

        channel.setAttribute( "chanId"     , pInfo->GetChanID() );
        channel.setAttribute( "chanNum"    , pInfo->GetChanNum());
        channel.setAttribute( "callSign"   , pInfo->GetChannelSchedulingID());
        //channel.setAttribute( "iconURL"    , sIconURL           );
        channel.setAttribute( "channelName", pInfo->GetChannelName());

        if (bDetails)
        {
            channel.setAttribute( "chanFilters",
                                  pInfo->GetChannelPlaybackFilters() );
            channel.setAttribute( "sourceId"   , pInfo->GetSourceID()    );
            channel.setAttribute( "inputId"    , pInfo->GetInputID()     );
            channel.setAttribute( "commFree"   ,
                                  (pInfo->IsCommercialFree()) ? 1 : 0 );
        }
    }
}

// vim:set shiftwidth=4 tabstop=4 expandtab:
