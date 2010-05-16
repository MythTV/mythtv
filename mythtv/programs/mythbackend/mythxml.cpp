//////////////////////////////////////////////////////////////////////////////
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

#include "mythxml.h"
#include "backendutil.h"

#include "mythcorecontext.h"
#include "util.h"
#include "mythdbcon.h"
#include "mythdb.h"

#include "previewgenerator.h"
#include "backendutil.h"
#include "mythconfig.h"
#include "programinfo.h"

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

static QString extract_id(const QString &raw_request)
{
    QStringList idPath = raw_request.split('/', QString::SkipEmptyParts);
    if (idPath.size() < 2)
        return "";

    idPath = idPath[idPath.size() - 2].split(' ', QString::SkipEmptyParts);
    if (idPath.empty())
        return "";

    idPath = idPath[0].split('?', QString::SkipEmptyParts);
    if (idPath.empty())
        return "";

    QString sId = idPath[0];
    return (sId.startsWith("Id")) ? sId.right(sId.length() - 2) : QString();
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

MythXML::MythXML( UPnpDevice *pDevice , const QString &sSharePath)
  : Eventing( "MythXML", "MYTHTV_Event", sSharePath)
{
    m_pEncoders = &tvList;
    m_pSched    = sched;
    m_pExpirer  = expirer;

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
    if (sURI == "GetServDesc"          ) return MXML_GetServiceDescription;

    if (sURI == "GetProgramGuide"      ) return MXML_GetProgramGuide;
    if (sURI == "GetProgramDetails"    ) return MXML_GetProgramDetails;

    if (sURI == "GetHosts"             ) return MXML_GetHosts;
    if (sURI == "GetKeys"              ) return MXML_GetKeys;
    if (sURI == "GetSetting"           ) return MXML_GetSetting;
    if (sURI == "PutSetting"           ) return MXML_PutSetting;

    if (sURI == "GetChannelIcon"       ) return MXML_GetChannelIcon;
    if (sURI == "GetAlbumArt"          ) return MXML_GetAlbumArt;
    if (sURI == "GetRecorded"          ) return MXML_GetRecorded;
    if (sURI == "GetExpiring"          ) return MXML_GetExpiring;
    if (sURI == "GetPreviewImage"      ) return MXML_GetPreviewImage;
    if (sURI == "GetRecording"         ) return MXML_GetRecording;
    if (sURI == "GetVideo"             ) return MXML_GetVideo;
    if (sURI == "GetMusic"             ) return MXML_GetMusic;
    if (sURI == "GetConnectionInfo"    ) return MXML_GetConnectionInfo;
    if (sURI == "GetVideoArt"          ) return MXML_GetVideoArt;

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
            if (pRequest->m_sBaseUrl == "/Myth/GetVideo")
            {
                pRequest->m_sBaseUrl = m_sControlUrl;
                pRequest->m_sMethod = "GetVideo";
            }
            else if (pRequest->m_sBaseUrl == "/Myth/GetVideoArt")
            {
                pRequest->m_sBaseUrl = m_sControlUrl;
                pRequest->m_sMethod = "GetVideoArt";
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

                case MXML_GetProgramGuide      :
                    GetProgramGuide( pRequest );
                    return true;
                case MXML_GetProgramDetails    :
                    GetProgramDetails( pRequest );
                    return true;

                case MXML_GetHosts             :
                    GetHosts       ( pRequest );
                    return true;
                case MXML_GetKeys              :
                    GetKeys        ( pRequest );
                    return true;
                case MXML_GetSetting           :
                    GetSetting     ( pRequest );
                    return true;
                case MXML_PutSetting           :
                    PutSetting     ( pRequest );
                    return true;

                case MXML_GetChannelIcon       :
                    GetChannelIcon ( pRequest );
                    return true;
                case MXML_GetRecorded          :
                    GetRecorded    ( pRequest );
                    return true;
                case MXML_GetExpiring          :
                    GetExpiring    ( pRequest );
                    return true;
                case MXML_GetPreviewImage      :
                    GetPreviewImage( pRequest );
                    return true;

                case MXML_GetRecording         :
                    GetRecording   ( pThread, pRequest );
                    return true;
                case MXML_GetMusic             :
                    GetMusic       ( pThread, pRequest );
                    return true;
                case MXML_GetVideo             :
                    GetVideo       ( pThread, pRequest );
                    return true;

                case MXML_GetConnectionInfo    :
                    GetConnectionInfo( pRequest );
                    return true;
                case MXML_GetAlbumArt          :
                    GetAlbumArt    ( pRequest );
                    return true;
                case MXML_GetVideoArt          :
                    GetVideoArt    ( pRequest );
                    return true;


                default:
                {
                    UPnp::FormatErrorResponse( pRequest,
                                               UPnPResult_InvalidAction );

                    return true;
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

void MythXML::GetHosts( HTTPRequest *pRequest )
{
    pRequest->m_mapRespHeaders[ "Cache-Control" ] = "no-cache=\"Ext\", "
                                                    "max-age = 5000";

    MSqlQuery query(MSqlQuery::InitCon());

    if (!query.isConnected())
    {
        UPnp::FormatErrorResponse( pRequest, UPnPResult_ActionFailed,
                    "Database not open while trying to load list of hosts" );
        return;
    }

    query.prepare(
        "SELECT DISTINCTROW hostname "
        "FROM settings "
        "WHERE (not isNull( hostname ))");

    if (!query.exec())
    {
        MythDB::DBError("MythXML::GetHosts()", query);
        return;
    }

    uint        nCount = 0;
    QString     sHosts = "";
    QTextStream os( &sHosts, QIODevice::WriteOnly );

    while (query.next())
    {
        nCount++;
        os << "<Host>"
           << HTTPRequest::Encode( query.value(0).toString() )
           << "</Host>";
    }

    if (!nCount)
        return; // no hosts, should we format some response?

    os << flush;

    NameValues list;

    list.push_back(NameValue( "Count", nCount ));
    list.push_back(NameValue( "Hosts", sHosts ));

    pRequest->FormatActionResponse( list );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void MythXML::GetKeys( HTTPRequest *pRequest )
{
    pRequest->m_mapRespHeaders[ "Cache-Control" ] = "no-cache=\"Ext\", "
                                                    "max-age = 5000";

    MSqlQuery query(MSqlQuery::InitCon());

    if (!query.isConnected())
    {
        UPnp::FormatErrorResponse(
            pRequest, UPnPResult_ActionFailed,
            QString("Database not open while trying to load setting: %1")
            .arg( pRequest->m_mapParams[ "Key" ] ));
        return;
    }

    query.prepare("SELECT DISTINCTROW value FROM settings;" );

    if (!query.exec())
    {
        MythDB::DBError("MythXML::GetKeys()", query);
        return;
    }

    uint        nCount = 0;
    QString     sKeys  = "";
    QTextStream os( &sKeys, QIODevice::WriteOnly );

    while (query.next())
    {
        os << "<Key>"
           << HTTPRequest::Encode( query.value(0).toString() )
           << "</Key>";
        nCount++;
    }
    os << flush;

    if (!nCount)
        return; // no keys, should we format some response?

    NameValues list;

    list.push_back( NameValue( "Count", nCount ));
    list.push_back( NameValue( "Keys" , sKeys  ));

    pRequest->FormatActionResponse( list );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void MythXML::GetSetting( HTTPRequest *pRequest )
{
    pRequest->m_mapRespHeaders[ "Cache-Control" ] =
        "no-cache=\"Ext\", max-age = 5000";

    QString sKey      = pRequest->m_mapParams[ "Key"      ];
    QString sHostName = pRequest->m_mapParams[ "HostName" ];

    MSqlQuery query(MSqlQuery::InitCon());

    if (!query.isConnected())
    {
        UPnp::FormatErrorResponse(
            pRequest, UPnPResult_ActionFailed,
            QString("Database not open while trying to load setting: %1")
            .arg( pRequest->m_mapParams[ "Key" ] ));
    }

    uint          nCount = 0;
    QString       sXml;
    QTextStream   os( &sXml, QIODevice::WriteOnly );

    // Was a Key Supplied?
    if (!sKey.isEmpty())
    {
        query.prepare("SELECT data, hostname from settings "
                      "WHERE value = :KEY AND "
                      "(hostname = :HOSTNAME OR hostname IS NULL) "
                      "ORDER BY hostname DESC;" );

        query.bindValue(":KEY"     , sKey      );
        query.bindValue(":HOSTNAME", sHostName );

        if (!query.exec())
        {
            MythDB::DBError("MythXML::GetSetting() w/key ", query);
            return;
        }

        if (query.next())
        {
            if ( (sHostName.isEmpty()) ||
                 ((!sHostName.isEmpty()) &&
                  (sHostName == query.value(1).toString())))
            {
                nCount    = 1;
                sHostName = query.value(1).toString();

                os <<  "<Value key='"
                   << HTTPRequest::Encode( sKey   )
                   << "'>"
                   << HTTPRequest::Encode( query.value(0).toString() )
                   << "</Value>";
            }
        }

    }
    else
    {
        if (sHostName.isEmpty())
        {
            query.prepare("SELECT value, data FROM settings "
                          "WHERE (hostname IS NULL)" );
        }
        else
        {
            query.prepare("SELECT value, data FROM settings "
                          "WHERE (hostname = :HOSTNAME)" );
            query.bindValue(":HOSTNAME", sHostName );
        }

        if (!query.exec())
        {
            MythDB::DBError("MythXML::GetSetting() w/o key ", query);
            return;
        }

        while (query.next())
        {
            nCount++;
            os <<  "<Value key='"
               << HTTPRequest::Encode( query.value(0).toString() )
               << "'>"
               << HTTPRequest::Encode( query.value(1).toString() )
               << "</Value>";
        }
    }

    if (!nCount)
    {
        // Not found, so return the supplied default value
        nCount = 1;
        os <<  "<Value key='"
           << HTTPRequest::Encode( sKey   )
           << "'>"
           << HTTPRequest::Encode( pRequest->m_mapParams[ "Default" ] )
           << "</Value>";
    }

    os << flush;

    NameValues list;

    list.push_back( NameValue( "Count"   , nCount    ));
    list.push_back( NameValue( "HostName", sHostName ));
    list.push_back( NameValue( "Values",   sXml      ));

    pRequest->FormatActionResponse( list );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void MythXML::PutSetting( HTTPRequest *pRequest )
{
    pRequest->m_mapRespHeaders[ "Cache-Control" ] = "no-cache=\"Ext\", "
                                                    "max-age = 5000";

    QString sHostName = pRequest->m_mapParams[ "HostName" ];
    QString sKey      = pRequest->m_mapParams[ "Key"      ];
    QString sValue    = pRequest->m_mapParams[ "Value"    ];

    if (!sKey.isEmpty())
    {
        NameValues list;

        if ( gCoreContext->SaveSettingOnHost( sKey, sValue, sHostName ) )
            list.push_back( NameValue( "Result", "True" ));
        else
            list.push_back( NameValue( "Result", "False" ));

        pRequest->FormatActionResponse( list );
    }
    else
        UPnp::FormatErrorResponse( pRequest, UPnPResult_InvalidArgs,
                                                            "Key Required" );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void MythXML::GetProgramGuide( HTTPRequest *pRequest )
{
    pRequest->m_mapRespHeaders[ "Cache-Control" ] = "no-cache=\"Ext\", "
                                                    "max-age = 5000";

    QString sStartTime     = pRequest->m_mapParams[ "StartTime"    ];
    QString sEndTime       = pRequest->m_mapParams[ "EndTime"      ];
    int     iNumOfChannels = pRequest->m_mapParams[ "NumOfChannels"].toInt();
    int     iStartChanId   = pRequest->m_mapParams[ "StartChanId"  ].toInt();
    bool    bDetails       = pRequest->m_mapParams[ "Details"      ].toInt();
    int     iEndChanId     = iStartChanId;

    QDateTime dtStart = QDateTime::fromString( sStartTime, Qt::ISODate );
    QDateTime dtEnd   = QDateTime::fromString( sEndTime  , Qt::ISODate );

    if (!dtStart.isValid())
    {
        UPnp::FormatErrorResponse( pRequest, UPnPResult_ArgumentValueInvalid,
                                                    "StartTime is invalid" );
        return;
    }

    if (!dtEnd.isValid())
    {
        UPnp::FormatErrorResponse( pRequest, UPnPResult_ArgumentValueInvalid,
                                                        "EndTime is invalid" );
        return;
    }

    if (dtEnd < dtStart)
    {
        UPnp::FormatErrorResponse( pRequest, UPnPResult_ArgumentValueInvalid,
                                                "EndTime is before StartTime");
        return;
    }

    if (iNumOfChannels == 0)
        iNumOfChannels = 1;

    if (iNumOfChannels == -1)
        iNumOfChannels = SHRT_MAX;

    // Find the ending channel Id

    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare( "SELECT chanid FROM channel WHERE (chanid >= :STARTCHANID )"
                   " ORDER BY chanid LIMIT :NUMCHAN" );

    query.bindValue(":STARTCHANID", iStartChanId );
    query.bindValue(":NUMCHAN"    , iNumOfChannels );

    if (!query.exec())
        MythDB::DBError("Select ChanId", query);

    query.first();  iStartChanId = query.value(0).toInt();
    query.last();   iEndChanId   = query.value(0).toInt();

    // Build SQL statement for Program Listing

    MSqlBindings bindings;
    QString      sSQL = "WHERE program.chanid >= :StartChanId "
                         "AND program.chanid <= :EndChanId "
                         "AND program.endtime >= :StartDate "
                         "AND program.starttime <= :EndDate "
                        "GROUP BY program.starttime, channel.channum, "
                         "channel.callsign, program.title "
                        "ORDER BY program.chanid ";

    bindings[":StartChanId"] = iStartChanId;
    bindings[":EndChanId"  ] = iEndChanId;
    bindings[":StartDate"  ] = dtStart.toString( Qt::ISODate );
    bindings[":EndDate"    ] = dtEnd.toString( Qt::ISODate );

    // Get all Pending Scheduled Programs

    RecList      recList;
    ProgramList  schedList;

    if (m_pSched)
        m_pSched->getAllPending( &recList);

    // ----------------------------------------------------------------------
    // We need to convert from a RecList to a ProgramList
    // (ProgramList will autodelete ProgramInfo pointers)
    // ----------------------------------------------------------------------

    for (RecIter itRecList =  recList.begin();
                 itRecList != recList.end();  ++itRecList)
    {
        schedList.push_back( *itRecList );
    }

    // ----------------------------------------------------------------------

    ProgramList progList;
    LoadFromProgram( progList, sSQL, bindings, schedList, false );

    // Build Response

    QDomDocument doc;

    QDomElement channels = doc.createElement("Channels");
    doc.appendChild( channels );

    int          iChanCount = 0;
    QDomElement  channel;
    uint32_t     iCurChanId = 0;

    ProgramList::iterator it = progList.begin();
    for (; it != progList.end(); ++it)
    {
        ProgramInfo *pInfo = *it;
        if ( iCurChanId != pInfo->GetChanID() )
        {
            iChanCount++;

            iCurChanId = pInfo->GetChanID();

            // Output new Channel Node

            channel = doc.createElement( "Channel" );
            channels.appendChild( channel );

            FillChannelInfo( channel, pInfo, bDetails );
        }

        FillProgramInfo( &doc, channel, pInfo, false, bDetails );
    }

    // ----------------------------------------------------------------------

    NameValues list;

    list.push_back( NameValue( "StartTime"    , sStartTime   ));
    list.push_back( NameValue( "EndTime"      , sEndTime     ));
    list.push_back( NameValue( "StartChanId"  , iStartChanId ));
    list.push_back( NameValue( "EndChanId"    , iEndChanId   ));
    list.push_back( NameValue( "NumOfChannels", iChanCount   ));
    list.push_back( NameValue( "Details"      , bDetails     ));

    list.push_back( NameValue( "Count"        , (int)progList.size() ));
    list.push_back( NameValue( "AsOf"         , QDateTime::currentDateTime()
                                                .toString( Qt::ISODate )));
    list.push_back( NameValue( "Version"      , MYTH_BINARY_VERSION ));
    list.push_back( NameValue( "ProtoVer"     , MYTH_PROTO_VERSION  ));
    list.push_back( NameValue( "ProgramGuide" , doc.toString()      ));

    pRequest->FormatActionResponse( list );
}


/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void MythXML::GetProgramDetails( HTTPRequest *pRequest )
{
    pRequest->m_mapRespHeaders[ "Cache-Control" ] = "no-cache=\"Ext\", "
                                                    "max-age = 5000";

    QString sStartTime = pRequest->m_mapParams[ "StartTime" ];
    QString sChanId    = pRequest->m_mapParams[ "ChanId"    ];

    QDateTime dtStart = QDateTime::fromString( sStartTime, Qt::ISODate );

    if (!dtStart.isValid())
    {
        UPnp::FormatErrorResponse( pRequest, UPnPResult_ArgumentValueInvalid,
                                                    "StartTime is invalid" );
        return;
    }

    // ----------------------------------------------------------------------
    // -=>TODO: Add support for getting Recorded Program Info
    // ----------------------------------------------------------------------

    // Build add'l SQL statement for Program Listing

    MSqlBindings bindings;
    QString      sSQL = "WHERE program.chanid = :ChanId "
                          "AND program.starttime = :StartTime ";

    bindings[":ChanId"   ] = sChanId;
    bindings[":StartTime"] = dtStart.toString( Qt::ISODate );

    // Get all Pending Scheduled Programs

    RecList      recList;
    ProgramList  schedList;

    if (m_pSched)
        m_pSched->getAllPending( &recList);

    // ----------------------------------------------------------------------
    // We need to convert from a RecList to a ProgramList
    // (ProgramList will autodelete ProgramInfo pointers)
    // ----------------------------------------------------------------------

    for (RecIter itRecList =  recList.begin();
                 itRecList != recList.end(); ++itRecList)
    {
        schedList.push_back( *itRecList );
    }

    // ----------------------------------------------------------------------

    ProgramList progList;
    LoadFromProgram( progList, sSQL, bindings, schedList, false );

    ProgramList::iterator pgit = progList.begin();

    if (pgit == progList.end())
    {
        UPnp::FormatErrorResponse( pRequest, UPnPResult_ActionFailed,
                                   "Error Reading Program Info" );
        return;
    }

    // Build Response XML

    QDomDocument doc;

//    QDomElement root = doc.createElement("Programs");
//    doc.appendChild(root);

    FillProgramInfo( &doc, doc, *pgit, true );

    // ----------------------------------------------------------------------

    NameValues list;

    list.push_back( NameValue( "StartTime"    , sStartTime   ));
    list.push_back( NameValue( "ChanId"       , sChanId      ));

    list.push_back( NameValue( "Count"        , 1  ));
    list.push_back( NameValue( "AsOf"         , QDateTime::currentDateTime()
                                                .toString( Qt::ISODate )));
    list.push_back( NameValue( "Version"      , MYTH_BINARY_VERSION ));
    list.push_back( NameValue( "ProtoVer"     , MYTH_PROTO_VERSION  ));

    list.push_back( NameValue( "ProgramDetails" , doc.toString()      ));

    pRequest->FormatActionResponse( list );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void MythXML::GetChannelIcon( HTTPRequest *pRequest )
{
    pRequest->m_eResponseType   = ResponseTypeFile;

    int  iChanId   = pRequest->m_mapParams[ "ChanId"  ].toInt();

    // Optional Parameters

    int     nWidth    = pRequest->m_mapParams[ "Width"     ].toInt();
    int     nHeight   = pRequest->m_mapParams[ "Height"    ].toInt();

    // Get Icon file path
    QString iconpath = ChannelUtil::GetIcon(iChanId);
    if (!iconpath.isEmpty())
        pRequest->m_sFileName = iconpath;

    if ((nWidth <= 0) && (nHeight <= 0))
        return;  // Use default pixmap

    QString sFileName = QString( "%1.%2x%3.png" )
                                   .arg( pRequest->m_sFileName )
                                   .arg( nWidth    )
                                   .arg( nHeight   );

    // ----------------------------------------------------------------------
    // check to see if image is already created.
    // ----------------------------------------------------------------------

    if (QFile::exists( sFileName ))
    {
        pRequest->m_eResponseType   = ResponseTypeFile;
        pRequest->m_nResponseStatus = 200;
        pRequest->m_sFileName       = sFileName;
        return;
    }

    float fAspect = 0.0;

    QImage *pImage = new QImage(pRequest->m_sFileName);

    if (!pImage)
        return;

    if (fAspect <= 0)
           fAspect = (float)(pImage->width()) / pImage->height();

    if (fAspect == 0)
    {
        delete pImage;
        return;
    }

    if ( nWidth == 0 )
        nWidth = (int)rint(nHeight * fAspect);

    if ( nHeight == 0 )
        nHeight = (int)rint(nWidth / fAspect);

    QImage img = pImage->scaled( nWidth, nHeight, Qt::IgnoreAspectRatio,
                                Qt::SmoothTransformation);

    QByteArray fname = sFileName.toAscii();
    img.save( fname.constData(), "PNG" );

    delete pImage;

    pRequest->m_sFileName = sFileName;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void MythXML::GetVideoArt( HTTPRequest *pRequest )
{
    pRequest->m_eResponseType   = ResponseTypeFile;

    QString sId =  pRequest->m_mapParams[ "Id"  ];

    if (sId.isEmpty())
    {
        sId = extract_id(pRequest->m_sRawRequest);
        if (sId.isEmpty())
            return;

        pRequest->m_mapParams[ "Id" ] = sId;

    }

    VERBOSE(VB_UPNP, QString("GetVideoArt ID = %1").arg(sId));

    // Read Video poster file path from database

    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("SELECT coverart FROM upnpmedia WHERE intid = :ITEMID");
    query.bindValue(":ITEMID", sId);

    if (!query.exec())
        MythDB::DBError("GetVideoArt ", query);

    if (!query.next())
        return;

    QString sFileName = query.value(0).toString();

    // ----------------------------------------------------------------------
    // check to see if albumart image is already created.
    // ----------------------------------------------------------------------

    if (QFile::exists( sFileName ))
    {
        pRequest->m_eResponseType   = ResponseTypeFile;
        pRequest->m_nResponseStatus = 200;
        pRequest->m_sFileName = sFileName;
        return;
    }

}

void MythXML::GetAlbumArt( HTTPRequest *pRequest )
{
    pRequest->m_eResponseType   = ResponseTypeFile;

    QString sId =  pRequest->m_mapParams[ "Id"  ];

    if (sId.isEmpty())
    {
        sId = extract_id(pRequest->m_sRawRequest);
        if (sId.isEmpty())
            return;

        pRequest->m_mapParams[ "Id" ] = sId;

    }

    // Optional Parameters

    int     nWidth    = pRequest->m_mapParams[ "Width"     ].toInt();
    int     nHeight   = pRequest->m_mapParams[ "Height"    ].toInt();

    // Read AlbumArt file path from database

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT CONCAT_WS('/', music_directories.path, "
                  "music_albumart.filename) FROM music_albumart "
                  "LEFT JOIN music_directories ON "
                  "music_directories.directory_id=music_albumart.directory_id "
                  "WHERE music_albumart.albumart_id = :ARTID;");
    query.bindValue(":ARTID", sId );

    if (!query.exec())
        MythDB::DBError("Select ArtId", query);

    QString musicbasepath = gCoreContext->GetSetting("MusicLocation", "");

    if (query.next())
    {
        pRequest->m_sFileName = QString( "%1/%2" )
                        .arg( musicbasepath )
                        .arg( query.value(0).toString() );
    }

    if ((nWidth == 0) && (nHeight == 0))
        return;  // use default pixmap

    QString sFileName = QString( "%1.%2x%3.png" )
                                   .arg( pRequest->m_sFileName )
                                   .arg( nWidth    )
                                   .arg( nHeight   );

    // ----------------------------------------------------------------------
    // check to see if albumart image is already created.
    // ----------------------------------------------------------------------

    if (QFile::exists( sFileName ))
    {
        pRequest->m_eResponseType   = ResponseTypeFile;
        pRequest->m_nResponseStatus = 200;
        pRequest->m_sFileName = sFileName;
        return;
    }

    // ------------------------------------------------------------------
    // Must generate Albumart Image, Generate Image and save.
    // ------------------------------------------------------------------

    float fAspect = 0.0;

    QImage *pImage = new QImage(pRequest->m_sFileName);

    if (!pImage)
        return;

    if (fAspect <= 0)
           fAspect = (float)(pImage->width()) / pImage->height();

    if ( nWidth == 0 )
        nWidth = (int)rint(nHeight * fAspect);

    if ( nHeight == 0 )
        nHeight = (int)rint(nWidth / fAspect);

    QImage img = pImage->scaled( nWidth, nHeight, Qt::IgnoreAspectRatio,
                                Qt::SmoothTransformation);

    QByteArray fname = sFileName.toAscii();
    img.save( fname.constData(), "PNG" );

    delete pImage;

    pRequest->m_sFileName = sFileName;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void MythXML::GetRecorded( HTTPRequest *pRequest )
{
    QMap<QString,ProgramInfo*> recMap;
    if (m_pSched)
        recMap = m_pSched->GetRecording();
    QMap<QString,uint32_t> inUseMap = ProgramInfo::QueryInUseMap();
    QMap<QString,bool> isJobRunning =
        ProgramInfo::QueryJobsRunning(JOB_COMMFLAG);

    ProgramList progList;
    LoadFromRecorded( progList, false,
                      inUseMap, isJobRunning, recMap );

    QMap<QString,ProgramInfo*>::iterator mit = recMap.begin();
    for (; mit != recMap.end(); mit = recMap.erase(mit))
        delete *mit;

    // Build Response XML

    QDomDocument doc;

    QDomElement root = doc.createElement("Programs");
    doc.appendChild(root);

    ProgramList::iterator it = progList.begin();
    for (; it != progList.end(); ++it)
        FillProgramInfo( &doc, root, *it, true );

    // ----------------------------------------------------------------------

    NameValues list;

    list.push_back( NameValue( "Count"    , (int)progList.size()));
    list.push_back( NameValue( "AsOf"     , QDateTime::currentDateTime()
                                            .toString( Qt::ISODate )));
    list.push_back( NameValue( "Version"  , MYTH_BINARY_VERSION ));
    list.push_back( NameValue( "ProtoVer" , MYTH_PROTO_VERSION  ));

    list.push_back( NameValue( "Recorded" , doc.toString()      ));

    pRequest->FormatActionResponse( list );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void MythXML::GetExpiring( HTTPRequest *pRequest )
{
    pginfolist_t  infoList;

    m_pExpirer->GetAllExpiring( infoList );

    // Build Response XML

    QDomDocument doc;

    QDomElement root = doc.createElement("Programs");
    doc.appendChild(root);

    pginfolist_t::iterator it = infoList.begin();
    for (; it !=infoList.end(); ++it)
    {
        ProgramInfo *pInfo = (*it);

        if (pInfo != NULL)
        {
            FillProgramInfo( &doc, root, pInfo, true );
            delete pInfo;
        }
    }

    // ----------------------------------------------------------------------

    NameValues list;

    list.push_back( NameValue( "Count"    , (int)infoList.size()));
    list.push_back( NameValue( "AsOf"     , QDateTime::currentDateTime()
                                            .toString( Qt::ISODate )));
    list.push_back( NameValue( "Version"  , MYTH_BINARY_VERSION ));
    list.push_back( NameValue( "ProtoVer" , MYTH_PROTO_VERSION  ));

    list.push_back( NameValue( "Expiring" , doc.toString()      ));

    pRequest->FormatActionResponse( list );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void MythXML::GetPreviewImage( HTTPRequest *pRequest )
{
    QString LOC = "MythXML::GetPreviewImage() - ";

    pRequest->m_eResponseType   = ResponseTypeHTML;
    pRequest->m_nResponseStatus = 404;

    QString sChanId   = pRequest->m_mapParams[ "ChanId"    ];
    QString sStartTime= pRequest->m_mapParams[ "StartTime" ];

    // Optional Parameters

    int     nWidth    = pRequest->m_mapParams[ "Width"     ].toInt();
    int     nHeight   = pRequest->m_mapParams[ "Height"    ].toInt();
    int     nSecsIn   = pRequest->m_mapParams[ "SecsIn"    ].toInt();

    QDateTime dtStart = QDateTime::fromString( sStartTime, Qt::ISODate );

    if (!dtStart.isValid())
    {
        VERBOSE(VB_IMPORTANT,
                LOC + QString("bad start time '%1'").arg(sStartTime));
        return;
    }

    // ----------------------------------------------------------------------
    // Read Recording From Database
    // ----------------------------------------------------------------------

    ProgramInfo pginfo(sChanId.toUInt(), dtStart);

    if (!pginfo.GetChanID())
    {
        VERBOSE(VB_IMPORTANT,
                LOC + "no recording for start time " + sStartTime);
        return;
    }

    if ( pginfo.GetHostname() != gCoreContext->GetHostName())
    {
        // We only handle requests for local resources
        return;
    }

    QString sFileName     = GetPlaybackURL(&pginfo);

    // ----------------------------------------------------------------------
    // check to see if default preview image is already created.
    // ----------------------------------------------------------------------
    QString sPreviewFileName;
    if (nSecsIn <= 0)
    {
        nSecsIn = -1;
        sPreviewFileName = sFileName + ".png";
    }
    else
    {
        sPreviewFileName = QString("%1.%2.png").arg(sFileName).arg(nSecsIn);
    }

    if (!QFile::exists( sPreviewFileName ))
    {
        // ------------------------------------------------------------------
        // Must generate Preview Image, Generate Image and save.
        // ------------------------------------------------------------------
        if (!pginfo.IsLocal() && sFileName.left(1) == "/")
            pginfo.SetPathname(sFileName);

        if (!pginfo.IsLocal())
            return;

        PreviewGenerator *previewgen = new PreviewGenerator(
            &pginfo, PreviewGenerator::kLocal);
        previewgen->SetPreviewTimeAsSeconds(nSecsIn);
        previewgen->SetOutputFilename(sPreviewFileName);
        bool ok = previewgen->Run();
        if (!ok)
        {
            pRequest->m_eResponseType   = ResponseTypeFile;
            pRequest->m_nResponseStatus = 404;
            previewgen->deleteLater();
            return;
        }
        previewgen->deleteLater();
    }

    pRequest->m_eResponseType   = ResponseTypeFile;
    pRequest->m_nResponseStatus = 200;

    float fAspect = 0.0;

    QImage *pImage = new QImage(sPreviewFileName);

    if (!pImage)
        return;

    if (fAspect <= 0)
        fAspect = (float)(pImage->width()) / pImage->height();

    if (fAspect == 0)
    {
        delete pImage;
        return;
    }

    bool bDefaultPixmap = (nWidth == 0) && (nHeight == 0);

    if ( nWidth == 0 )
        nWidth = (int)rint(nHeight * fAspect);

    if ( nHeight == 0 )
        nHeight = (int)rint(nWidth / fAspect);

    if (bDefaultPixmap)
        pRequest->m_sFileName = sPreviewFileName;
    else
        pRequest->m_sFileName = QString( "%1.%2.%3x%4.png" )
                                    .arg( sFileName )
                                    .arg( nSecsIn   )
                                    .arg( nWidth    )
                                    .arg( nHeight   );

    // ----------------------------------------------------------------------
    // check to see if scaled preview image is already created.
    // ----------------------------------------------------------------------

    if (QFile::exists( pRequest->m_sFileName ))
    {
        delete pImage;
        return;
    }

    QImage img = pImage->scaled( nWidth, nHeight, Qt::IgnoreAspectRatio,
                                Qt::SmoothTransformation);

    QByteArray fname = pRequest->m_sFileName.toAscii();
    img.save( fname.constData(), "PNG" );

    makeFileAccessible(fname.constData());

    delete pImage;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void MythXML::GetRecording( HttpWorkerThread *pThread,
                            HTTPRequest      *pRequest )
{
    bool bIndexFile = false;

    pRequest->m_eResponseType   = ResponseTypeHTML;
    pRequest->m_mapRespHeaders[ "Cache-Control" ] = "no-cache=\"Ext\", "
                                                    "max-age = 5000";
    pRequest->m_nResponseStatus = 404;

    QString sChanId   = pRequest->m_mapParams[ "ChanId"    ];
    QString sStartTime= pRequest->m_mapParams[ "StartTime" ];

    if (sStartTime.isEmpty())
    {
        VERBOSE( VB_UPNP, "MythXML::GetRecording - StartTime missing.");
        return;
    }

    // ----------------------------------------------------------------------
    // DSM-320 & DSM-520 Special File Request for Index file of MPEG
    //
    // -=>TODO: Need to reverse Engineer File Format & create on the fly
    //          from the RecordedMarkup Table.
    // ----------------------------------------------------------------------

    int nIdxPos = sStartTime.lastIndexOf(".idx", -1, Qt::CaseInsensitive);

    if (nIdxPos >=0 )
    {
        bIndexFile = true;
        sStartTime = sStartTime.left( nIdxPos );
    }

    // ----------------------------------------------------------------------
    // Check to see if this is another request for the same recording...
    // ----------------------------------------------------------------------

    ThreadData *pData = static_cast<ThreadData *>(pThread->GetWorkerData());

    if (pData != NULL)
    {
        if ((pData->m_eType == ThreadData::DT_Recording) &&
               pData->IsSameRecording( sChanId, sStartTime ))
        {
            pRequest->m_sFileName = pData->m_sFileName;
        }
        else
           pData = NULL;
    }

    // ----------------------------------------------------------------------
    // New request if pData == NULL
    // ----------------------------------------------------------------------

    if (pData == NULL)
    {
        // ------------------------------------------------------------------
        // Load Program Information & build FileName
        // ------------------------------------------------------------------

        QDateTime dtStart = QDateTime::fromString( sStartTime, Qt::ISODate );

        if (!dtStart.isValid())
        {
            VERBOSE( VB_UPNP, "MythXML::GetRecording - StartTime Invalid.");
            return;
        }

        // ------------------------------------------------------------------
        // Read Recording From Database
        // ------------------------------------------------------------------

        ProgramInfo pginfo(sChanId.toUInt(), dtStart);

        if (!pginfo.GetChanID())
        {
            VERBOSE( VB_UPNP, QString( "MythXML::GetRecording - for %1, %2 "
                                       "failed" )
                                        .arg( sChanId )
                                        .arg( sStartTime ));
            return;
        }

        if ( pginfo.GetHostname() != gCoreContext->GetHostName())
        {
            // We only handle requests for local resources

            VERBOSE( VB_UPNP, QString( "MythXML::GetRecording - To access this "
                                       "recording, send request to %1." )
                     .arg( pginfo.GetHostname() ));
            return;
        }

        pRequest->m_sFileName = GetPlaybackURL(&pginfo);

        // ------------------------------------------------------------------
        // Store File information in WorkerThread Storage
        // for next request (cache)
        // ------------------------------------------------------------------

        pData = new ThreadData( sChanId, sStartTime, pRequest->m_sFileName );

        pThread->SetWorkerData( pData );
    }

    // ----------------------------------------------------------------------
    // DSM-?20 Seek table support.
    // ----------------------------------------------------------------------

    if (bIndexFile)
        pRequest->m_sFileName += ".idx";

    // ----------------------------------------------------------------------
    // check to see if the file exists
    // ----------------------------------------------------------------------

    if (QFile::exists( pRequest->m_sFileName ))
    {
        pRequest->m_eResponseType   = ResponseTypeFile;
        pRequest->m_nResponseStatus = 200;
    }
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void MythXML::GetMusic( HttpWorkerThread *pThread,
                        HTTPRequest      *pRequest )
{
    pRequest->m_eResponseType   = ResponseTypeHTML;
    pRequest->m_mapRespHeaders[ "Cache-Control" ] = "no-cache=\"Ext\", "
                                                    "max-age = 5000";
    pRequest->m_nResponseStatus = 404;

    QString sId   = pRequest->m_mapParams[ "Id"    ];

    if (sId.isEmpty())
        return;

    int nTrack = sId.toInt();

    // ----------------------------------------------------------------------
    // Check to see if this is another request for the same recording...
    // ----------------------------------------------------------------------

    ThreadData *pData = static_cast<ThreadData *>(pThread->GetWorkerData());

    if (pData != NULL)
    {
        if ((pData->m_eType == ThreadData::DT_Music) &&
               (pData->m_nTrackNumber == nTrack))
        {
            pRequest->m_sFileName = pData->m_sFileName;

        }
        else
           pData = NULL;
    }

    // ----------------------------------------------------------------------
    // New request if pData == NULL
    // ----------------------------------------------------------------------

    if (pData == NULL)
    {
        QString sBasePath = gCoreContext->GetSetting( "MusicLocation", "");

        // ------------------------------------------------------------------
        // Load Track's FileName
        // ------------------------------------------------------------------

        MSqlQuery query(MSqlQuery::InitCon());

        if (query.isConnected())
        {
            query.prepare("SELECT CONCAT_WS('/', music_directories.path, "
                           "music_songs.filename) AS filename FROM music_songs "
                           "LEFT JOIN music_directories ON "
                             "music_songs.directory_id="
                             "music_directories.directory_id "
                           "WHERE music_songs.song_id = :KEY");

            query.bindValue(":KEY", nTrack );

            if (!query.exec())
            {
                MythDB::DBError("MythXML::GetMusic()", query);
                return;
            }

            if (query.next())
            {
                pRequest->m_sFileName = QString( "%1/%2" )
                        .arg( sBasePath )
                        .arg( query.value(0).toString() );
            }
        }

        // ------------------------------------------------------------------
        // Store information in WorkerThread Storage for next request (cache)
        // ------------------------------------------------------------------

        pData = new ThreadData( nTrack, pRequest->m_sFileName );

        pThread->SetWorkerData( pData );
    }

    // ----------------------------------------------------------------------
    // check to see if the file exists
    // ----------------------------------------------------------------------

    if (QFile::exists( pRequest->m_sFileName ))
    {
        pRequest->m_eResponseType   = ResponseTypeFile;
        pRequest->m_nResponseStatus = 200;
    }
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void MythXML::GetVideo( HttpWorkerThread *pThread,
                        HTTPRequest      *pRequest )
{
    pRequest->m_eResponseType   = ResponseTypeHTML;
    pRequest->m_mapRespHeaders[ "Cache-Control" ] = "no-cache=\"Ext\", "
                                                    "max-age = 5000";
    pRequest->m_nResponseStatus = 404;

    QString sId   = pRequest->m_mapParams[ "Id" ];

    if (sId.isEmpty())
    {
        sId = extract_id(pRequest->m_sRawRequest);
        if (sId.isEmpty())
            return;

        //VERBOSE(VB_UPNP, QString("MythXML::GetVideo : %1 ").arg(sId));

        pRequest->m_mapParams[ "Id" ] = sId;
    }

    bool wantCoverArt = (pRequest->m_mapParams[ "albumArt" ] == "true");

    if (wantCoverArt)
    {
        GetVideoArt(pRequest);
        return;
    }

    // ----------------------------------------------------------------------
    // Check to see if this is another request for the same recording...
    // ----------------------------------------------------------------------

    ThreadData *pData = static_cast<ThreadData *>(pThread->GetWorkerData());

    if (pData != NULL)
    {
        if ((pData->m_eType == ThreadData::DT_Video)
            && (pData->m_sVideoID == sId ))
        {
            pRequest->m_sFileName = pData->m_sFileName;
        }
        else
           pData = NULL;
    }

    // ----------------------------------------------------------------------
    // New request if pData == NULL
    // ----------------------------------------------------------------------

    if (pData == NULL)
    {
        QString sBasePath = "";

        // ------------------------------------------------------------------
        // Load Track's FileName
        // ------------------------------------------------------------------

        MSqlQuery query(MSqlQuery::InitCon());

        if (query.isConnected())
        {
            query.prepare("SELECT filepath FROM upnpmedia WHERE intid = :KEY" );
            query.bindValue(":KEY", sId );

            if (!query.exec())
            {
                MythDB::DBError("MythXML::GetVideo()", query);
                return;
            }

            if (query.next())
            {
                pRequest->m_sFileName = QString( "%1/%2" ).arg( sBasePath )
                                        .arg( query.value(0).toString() );
            }
        }

        // ------------------------------------------------------------------
        // Store information in WorkerThread Storage for next request (cache)
        // ------------------------------------------------------------------

        pData = new ThreadData( sId, pRequest->m_sFileName );

        pThread->SetWorkerData( pData );
    }

    // ----------------------------------------------------------------------
    // check to see if the file exists
    // ----------------------------------------------------------------------

    if (QFile::exists( pRequest->m_sFileName ))
    {
        pRequest->m_eResponseType   = ResponseTypeFile;
        pRequest->m_nResponseStatus = 200;
    }
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void MythXML::GetConnectionInfo( HTTPRequest *pRequest )
{
    pRequest->m_mapRespHeaders[ "Cache-Control" ]
                         = "no-cache=\"Ext\", max-age = 5000";

    QString sPin         = pRequest->m_mapParams[ "Pin" ];
    QString sSecurityPin = gCoreContext->GetSetting( "SecurityPin", "");

    if ( sSecurityPin.isEmpty() )
    {
        UPnp::FormatErrorResponse( pRequest,
              UPnPResult_HumanInterventionRequired,
              "No Security Pin assigned. Run mythtv-setup to set one." );
        return;
    }

    if ((sSecurityPin != "0000" ) && ( sPin != sSecurityPin ))
    {
        UPnp::FormatErrorResponse( pRequest, UPnPResult_ActionNotAuthorized );
        return;
    }

    DatabaseParams params = gCoreContext->GetDatabaseParams();

    // Check for DBHostName of "localhost" and change to public name or IP

    QString sServerIP = gCoreContext->GetSetting( "BackendServerIP", "localhost" );
    QString sPeerIP   = pRequest->GetPeerAddress();

    if ((params.dbHostName == "localhost") &&
        (sServerIP         != "localhost") &&
        (sServerIP         != sPeerIP    ))
    {
        params.dbHostName = sServerIP;
    }

    QString     sXml;
    QTextStream os( &sXml, QIODevice::WriteOnly );

    os <<  "<Database>";
    os <<   "<Host>"      << params.dbHostName << "</Host>";
    os <<   "<Port>"      << params.dbPort     << "</Port>";
    os <<   "<UserName>"  << params.dbUserName << "</UserName>";
    os <<   "<Password>"  << params.dbPassword << "</Password>";
    os <<   "<Name>"      << params.dbName     << "</Name>";
    os <<   "<Type>"      << params.dbType     << "</Type>";
    os <<  "</Database>";

    os <<  "<WOL>";
    os <<   "<Enabled>"   << params.wolEnabled   << "</Enabled>";
    os <<   "<Reconnect>" << params.wolReconnect << "</Reconnect>";
    os <<   "<Retry>"     << params.wolRetry     << "</Retry>";
    os <<   "<Command>"   << params.wolCommand   << "</Command>";
    os <<  "</WOL>" << flush;

    NameValues list;

    list.push_back( NameValue( "Info", sXml ));

    pRequest->FormatActionResponse( list );
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
