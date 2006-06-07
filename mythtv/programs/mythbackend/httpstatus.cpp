//////////////////////////////////////////////////////////////////////////////
// Program Name: httpstatus.cpp
//                                                                            
// Purpose - Html & XML status HttpServerExtension
//                                                                            
// Created By  : David Blain                    Created On : Oct. 24, 2005
// Modified By :                                Modified On:                  
//                                                                            
//////////////////////////////////////////////////////////////////////////////

#include "httpstatus.h"

#include "libmyth/mythcontext.h"
#include "libmyth/util.h"
#include "libmyth/mythdbcon.h"

#include <qtextstream.h>
#include <qdir.h>
#include <qfile.h>
#include <qregexp.h>
#include <math.h>

#ifdef HAVE_LMSENSORS 
    #define LMSENSOR_DEFAULT_CONFIG_FILE "/etc/sensors.conf" 
    #include <sensors/sensors.h> 
    #include <sensors/chips.h> 
#endif 

// This is necessary for GCC 3.3, which has llabs(long long) but not abs(...) 
inline  long long  myAbs(long long  n)  { return n >= 0 ? n : -n; } 

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

HttpStatus::HttpStatus( QMap<int, EncoderLink *> *tvList, Scheduler *sched, bool bIsMaster )
          : HttpServerExtension( "HttpStatus" )
{
    m_pEncoders = tvList;
    m_pSched    = sched;
    m_bIsMaster = bIsMaster;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

HttpStatus::~HttpStatus()
{
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

HttpStatusMethod HttpStatus::GetMethod( const QString &sURI )
{
    if (sURI == ""                     ) return( HSM_GetStatusHTML   );
    if (sURI == "getStatusHTML"        ) return( HSM_GetStatusHTML   );
    if (sURI == "getStatus"            ) return( HSM_GetStatusXML    );
    if (sURI == "xml"                  ) return( HSM_GetStatusXML    );
    if (sURI == "getProgramGuide"      ) return( HSM_GetProgramGuide );

    if (sURI == "getHosts"             ) return( HSM_GetHosts        );
    if (sURI == "getKeys"              ) return( HSM_GetKeys         );
    if (sURI == "getSetting"           ) return( HSM_GetSetting      );
    if (sURI == "putSetting"           ) return( HSM_PutSetting      );

    if (sURI == "getChannelIcon"       ) return( HSM_GetChannelIcon  );
    if (sURI == "getRecorded"          ) return( HSM_GetRecorded     );
    if (sURI == "getPreviewImage"      ) return( HSM_GetPreviewImage );
    if (sURI == "getRecording"         ) return( HSM_GetRecording    );
    if (sURI == "getMusic"             ) return( HSM_GetMusic        );

    if (sURI == "getDeviceDesc"        ) return( HSM_GetDeviceDesc   );
    if (sURI == "getCDSDesc"           ) return( HSM_GetCDSDesc      );
    if (sURI == "getCMGRDesc"          ) return( HSM_GetCMGRDesc     );

    if (sURI == "*"                    ) return( HSM_Asterisk        );

    return( HSM_Unknown );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

bool HttpStatus::ProcessRequest( HttpWorkerThread *pThread, HTTPRequest *pRequest )
{
    try
    {
        if (pRequest)
        {
            if (pRequest->m_sBaseUrl != "/")
                return( false );

            switch( GetMethod( pRequest->m_sMethod ))
            {
                case HSM_GetStatusXML   : GetStatusXML   ( pRequest ); return( true );
                case HSM_GetStatusHTML  : GetStatusHTML  ( pRequest ); return( true ); 

                case HSM_GetProgramGuide: GetProgramGuide( pRequest ); return( true );

                case HSM_GetHosts       : GetHosts       ( pRequest ); return( true );
                case HSM_GetKeys        : GetKeys        ( pRequest ); return( true );
                case HSM_GetSetting     : GetSetting     ( pRequest ); return( true );
                case HSM_PutSetting     : PutSetting     ( pRequest ); return( true );
                                                                       
                case HSM_GetChannelIcon : GetChannelIcon ( pRequest ); return( true );
                case HSM_GetRecorded    : GetRecorded    ( pRequest ); return( true );
                case HSM_GetPreviewImage: GetPreviewImage( pRequest ); return( true );

                case HSM_GetRecording   : GetRecording   ( pThread, pRequest ); return( true );
                case HSM_GetMusic       : GetMusic       ( pThread, pRequest ); return( true );
                default: break;
            }
        }
    }
    catch( ... )
    {
        cerr << "HttpStatus::ProcessRequest() - Unexpected Exception" << endl;
    }

    return( false );
}           

// ==========================================================================
// Request handler Methods
// ==========================================================================

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void HttpStatus::GetHosts( HTTPRequest *pRequest )
{
    pRequest->m_eResponseType = ResponseTypeXML;
    pRequest->m_mapRespHeaders[ "Cache-Control" ] = "no-cache=\"Ext\", max-age = 5000";

    MSqlQuery query(MSqlQuery::InitCon());

    if (query.isConnected())
    {
        query.prepare("SELECT DISTINCTROW hostname "
                        "FROM settings WHERE (not isNull( hostname ));" );
        query.exec();

        if (query.isActive() && query.size() > 0)
        {
            pRequest->m_response << "<Hosts count='" << query.size() << "'>";
            
            while(query.next())
            {
                QString sHost = query.value(0).toString();

                pRequest->m_response << "<Host>" 
                                     << HTTPRequest::Encode( sHost )
                                     << "</Host>";
            }

            pRequest->m_response << "</Hosts>";

        }
    }
    else
    {
        QString sMsg = "Database not open while trying to load list of hosts";
         
        VERBOSE(VB_IMPORTANT, sMsg );

        pRequest->m_response << "<Error>" + sMsg + "</Error>";
        pRequest->m_nResponseStatus = 500;

    }
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void HttpStatus::GetKeys( HTTPRequest *pRequest )
{
    pRequest->m_eResponseType   = ResponseTypeXML;
    pRequest->m_mapRespHeaders[ "Cache-Control" ] = "no-cache=\"Ext\", max-age = 5000";

    MSqlQuery query(MSqlQuery::InitCon());

    if (query.isConnected())
    {
        query.prepare("SELECT DISTINCTROW value FROM settings;" );
        query.exec();

        if (query.isActive() && query.size() > 0)
        {
            pRequest->m_response << "<Keys count='" << query.size() << "'>";
            
            while(query.next())
            {
                QString sKey = query.value(0).toString();

                pRequest->m_response << "<Key>" 
                                     << HTTPRequest::Encode( sKey )
                                     << "</Key>";
            }

            pRequest->m_response << "</Keys>";

        }
    }
    else
    {
        QString sMsg = QString("Database not open while trying to load setting: %1")
                               .arg( pRequest->m_mapParams[ "Key" ] );
         
        VERBOSE(VB_IMPORTANT, sMsg );

        pRequest->m_response << "<Error>" + HTTPRequest::Encode( sMsg ) + "</Error>";
        pRequest->m_nResponseStatus = 500;

    }
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void HttpStatus::GetSetting( HTTPRequest *pRequest )
{
    pRequest->m_eResponseType   = ResponseTypeXML;
    pRequest->m_mapRespHeaders[ "Cache-Control" ] = "no-cache=\"Ext\", max-age = 5000";

    QString sKey      = pRequest->m_mapParams[ "Key"      ];
    QString sHostName = pRequest->m_mapParams[ "HostName" ];
    QString sValue;

    MSqlQuery query(MSqlQuery::InitCon());

    if (query.isConnected())
    {
        // Was a Key Supplied?

        if (sKey.length() > 0)
        {
            query.prepare("SELECT data, hostname from settings "
                            "WHERE value = :KEY AND "
                                 "(hostname = :HOSTNAME OR hostname IS NULL) "
                            "ORDER BY hostname DESC;" );

            query.bindValue(":KEY"     , sKey      );
            query.bindValue(":HOSTNAME", sHostName );
            query.exec();

            if (query.isActive() && query.size() > 0)
            {
                query.next();

                if ( (sHostName.length() == 0) || 
                    ((sHostName.length() >  0) && 
                     (sHostName == query.value(1).toString())))
                {
                    sValue    = query.value(0).toString();
                    sHostName = query.value(1).toString();

                    pRequest->m_response <<  "<Value hostName='" 
                                         << HTTPRequest::Encode( sHostName )
                                         << "' key='" 
                                         << HTTPRequest::Encode( sKey   ) 
                                         << "'>"
                                         << HTTPRequest::Encode( sValue ) 
                                         << "</Value>";
                    return;
                }
            }

        }
        else
        {

            VERBOSE(VB_IMPORTANT, "HostName:" );
            VERBOSE(VB_IMPORTANT, sHostName );

            if (sHostName.length() == 0)
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

            query.exec();

            if (query.isActive() && query.size() > 0)
            {
                pRequest->m_response << "<Values hostName='" 
                                     << HTTPRequest::Encode( sHostName )
                                     << "' count='" << query.size() << "'>";
            
                while(query.next())
                {
                    sKey      = query.value(0).toString();
                    sValue    = query.value(1).toString();

                    pRequest->m_response <<  "<Value key='" 
                                         << HTTPRequest::Encode( sKey   )
                                         << "'>"
                                         << HTTPRequest::Encode( sValue )
                                         << "</Value>";
                }

                pRequest->m_response << "</Values>";
            
                return;
            }

        }

        // Not found, so return the supplied default value

        pRequest->m_response << "<Value hostName='' key='" 
                             << HTTPRequest::Encode( sKey ) << "'>" 
                             << HTTPRequest::Encode( pRequest->m_mapParams[ "Default" ] )
                             << "</Value>";
    }
    else
    {
        QString sMsg = QString("Database not open while trying to load setting: %1")
                               .arg( pRequest->m_mapParams[ "Key" ] );
         
        VERBOSE(VB_IMPORTANT, sMsg );

        pRequest->m_response << "<Error>" << HTTPRequest::Encode( sMsg ) << "</Error>";
        pRequest->m_nResponseStatus = 500;
    }
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void HttpStatus::PutSetting( HTTPRequest *pRequest )
{
    pRequest->m_eResponseType   = ResponseTypeXML;
    pRequest->m_mapRespHeaders[ "Cache-Control" ] = "no-cache=\"Ext\", max-age = 5000";

    QString sHostName = pRequest->m_mapParams[ "HostName" ];
    QString sKey      = pRequest->m_mapParams[ "Key"      ];
    QString sValue    = pRequest->m_mapParams[ "Value"    ];

    if (sKey.length() > 0)
    {
        // -=>TODO: MythContext::SaveSettingOnHost does not handle NULL hostnames.
        //          This code should be removed when fixed in MythContext.

        if (sHostName.length() == 0)
        {

            MSqlQuery query(MSqlQuery::InitCon());

            query.prepare("DELETE FROM settings WHERE value = :KEY "
                      "AND hostname IS NULL;");
            query.bindValue(":KEY", sKey);

            if (!query.exec() || !query.isActive())
                MythContext::DBError("Clear setting", query);
        }

        // End hack.

        gContext->SaveSettingOnHost( sKey, sValue, sHostName );

        pRequest->m_response <<  "<Success/>";
    }
    else
        pRequest->m_response <<  "<Error>Key Required</Error>";

}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void HttpStatus::GetProgramGuide( HTTPRequest *pRequest )
{
    pRequest->m_eResponseType = ResponseTypeXML;
    pRequest->m_mapRespHeaders[ "Cache-Control" ] = "no-cache=\"Ext\", max-age = 5000";

    QString sStartTime     = pRequest->m_mapParams[ "StartTime"    ];
    QString sEndTime       = pRequest->m_mapParams[ "EndTime"      ];
    int     iNumOfChannels = pRequest->m_mapParams[ "NumOfChannels"].toInt();
    int     iStartChanId   = pRequest->m_mapParams[ "StartChanId"  ].toInt();
    int     iEndChanId     = iStartChanId;

    QDateTime dtStart = QDateTime::fromString( sStartTime, Qt::ISODate );
    QDateTime dtEnd   = QDateTime::fromString( sEndTime  , Qt::ISODate );

    if (!dtStart.isValid()) 
    { 
        pRequest->m_response <<  "<Error>StartTime is invalid</Error>";
        return;
    }
        
    if (!dtEnd.isValid()) 
    { 
        pRequest->m_response <<  "<Error>EndTime is invalid</Error>";
        return;
    }

    if (dtEnd < dtStart) 
    { 
        pRequest->m_response <<  "<Error>EndTime is before StartTime</Error>";
        return;
    }

    if (iNumOfChannels == 0)
        iNumOfChannels = 1;

    // Find the ending channel Id

    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare( "SELECT chanid FROM channel WHERE (chanid >= :STARTCHANID )"
                   " ORDER BY chanid LIMIT :NUMCHAN" );
    
    query.bindValue(":STARTCHANID", iStartChanId );
    query.bindValue(":NUMCHAN"    , iNumOfChannels );

    if (!query.exec() || !query.isActive())
        MythContext::DBError("Select ChanId", query);

    query.first();  iStartChanId = query.value(0).toInt();
    query.last();   iEndChanId   = query.value(0).toInt();
        
    // Build add'l SQL statement for Program Listing

    MSqlBindings bindings;
    QString      sSQL = "WHERE program.chanid >= :StartChanId "
                         "AND program.chanid <= :EndChanId "
                         "AND program.starttime >= :StartDate "
                         "AND program.endtime <= :EndDate "
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
                 itRecList != recList.end();   itRecList++)
    {
        schedList.append( *itRecList );
    }

    // ----------------------------------------------------------------------

    ProgramList progList;

    progList.FromProgram( sSQL, bindings, schedList );

    // Build Response XML

    QDomDocument doc( "ProgramGuide" );                        

    QDomElement root = doc.createElement("ProgramGuide");
    doc.appendChild(root);

    root.setAttribute("asOf"      , QDateTime::currentDateTime().toString( Qt::ISODate ));
    root.setAttribute("version"   , MYTH_BINARY_VERSION           );
    root.setAttribute("protoVer"  , MYTH_PROTO_VERSION            );
    root.setAttribute("totalCount", progList.count()              );

    // ----------------------------------------------------------------------

    QDomElement channels = doc.createElement("Channels");
    root.appendChild( channels );

    int          iChanCount = 0;
    QDomElement  channel;
    QString      sCurChanId = "";
    ProgramInfo *pInfo      = progList.first();

    while (pInfo != NULL)
    {
        if ( sCurChanId != pInfo->chanid )
        {
            iChanCount++;

            sCurChanId = pInfo->chanid;

            // Ouput new Channel Node

            channel = doc.createElement( "Channel" );
            channels.appendChild( channel );

            FillChannelInfo( channel, pInfo );
        }

        FillProgramInfo( &doc, channel, pInfo, false );

        pInfo = progList.next();

    }

    root.setAttribute("startTime"    , sStartTime   );
    root.setAttribute("endTime"      , sEndTime     );
    root.setAttribute("startChanId"  , iStartChanId );
    root.setAttribute("endChanId"    , iEndChanId   );
    root.setAttribute("numOfChannels", iChanCount   );

    pRequest->m_response << doc.toString();
}

/////////////////////////////////////////////////////////////////////////////
//                  
/////////////////////////////////////////////////////////////////////////////

void HttpStatus::GetChannelIcon( HTTPRequest *pRequest )
{
    pRequest->m_eResponseType   = ResponseTypeFile;

    int  iChanId   = pRequest->m_mapParams[ "ChanId"  ].toInt();

    // Read Icon file path from database

    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare( "SELECT icon FROM channel WHERE (chanid = :CHANID )" );
    query.bindValue(":CHANID", iChanId );

    if (!query.exec() || !query.isActive())
        MythContext::DBError("Select ChanId", query);

    if (query.size() > 0)
    {
        query.first();  

        pRequest->m_sFileName       = query.value(0).toString();
    }
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void HttpStatus::GetRecorded( HTTPRequest *pRequest )
{
    bool    bDescending = pRequest->m_mapParams[ "Descending"  ].toInt();

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
                 itRecList != recList.end();   itRecList++)
    {
        schedList.append( *itRecList );
    }

    // ----------------------------------------------------------------------

    ProgramList progList;

    progList.FromRecorded( bDescending, &schedList ); 

    // Build Response XML

    QDomDocument doc( "Recorded" );                        

    QDomElement root = doc.createElement("Recorded");
    doc.appendChild(root);

    root.setAttribute("asOf"      , QDateTime::currentDateTime().toString( Qt::ISODate ));
    root.setAttribute("version"   , MYTH_BINARY_VERSION           );
    root.setAttribute("protoVer"  , MYTH_PROTO_VERSION            );
    root.setAttribute("totalCount", progList.count()              );

    ProgramInfo *pInfo = progList.first();

    while (pInfo != NULL)
    {
        FillProgramInfo( &doc, root, pInfo, true );

        pInfo = progList.next();
    }

    pRequest->m_mapRespHeaders[ "Cache-Control" ] = "no-cache=\"Ext\", max-age = 5000";
    pRequest->m_eResponseType = ResponseTypeXML;
    pRequest->m_response << doc.toString();

}

/////////////////////////////////////////////////////////////////////////////
//                  
/////////////////////////////////////////////////////////////////////////////

void HttpStatus::GetPreviewImage( HTTPRequest *pRequest )
{
    pRequest->m_eResponseType   = ResponseTypeHTML;
    pRequest->m_nResponseStatus = 404;

    // -=>TODO: Add Parameters to allow various sizes & times

    QString sChanId   = pRequest->m_mapParams[ "ChanId"    ];
    QString sStartTime= pRequest->m_mapParams[ "StartTime" ];

    QDateTime dtStart = QDateTime::fromString( sStartTime, Qt::ISODate );

    if (!dtStart.isValid())
        return;

    // ----------------------------------------------------------------------
    // Read Recording From Database
    // ----------------------------------------------------------------------

    ProgramInfo *pInfo = ProgramInfo::GetProgramFromRecorded( sChanId, dtStart );

    if (pInfo==NULL)
        return;

    if ( pInfo->hostname != gContext->GetHostName())
    {
        // We only handle requests for local resources   

        delete pInfo;

        return;     
    }

    // ----------------------------------------------------------------------
    // check to see if preview image is already created.
    // ----------------------------------------------------------------------

    QString sFileName     = pInfo->GetRecordFilename( gContext->GetFilePrefix() );
    pRequest->m_sFileName = sFileName + ".png";

    if (!QFile::exists( pRequest->m_sFileName ))
    {
        // Must generate Preview Image 

        // Find first Local encoder

        EncoderLink *pEncoder = NULL;

        for ( QMap<int, EncoderLink *>::Iterator it = m_pEncoders->begin();
              it != m_pEncoders->end();
              ++it )
        {
            if (it.data()->IsLocal())
            {
                pEncoder = it.data();
                break;
            }
        }

        if ( pEncoder == NULL)
        {
            delete pInfo;
            return;
        }

        // ------------------------------------------------------------------
        // Generate Preview Image and save.
        // ------------------------------------------------------------------

        int len = 0;
        int width = 0, height = 0;
        float aspect = 0;
        int secondsin = gContext->GetNumSetting("PreviewPixmapOffset", 64) +
                        gContext->GetNumSetting("RecordPreRoll",0);

        unsigned char *data = (unsigned char *)pEncoder->GetScreenGrab(pInfo, 
                                                                    sFileName, 
                                                                    secondsin,
                                                                    len, width,
                                                                    height, aspect);


        if (!data)
        {
            delete pInfo;
            return;
        }

        QImage img(data, width, height, 32, NULL, 65536 * 65536, QImage::LittleEndian);

        float ppw = gContext->GetNumSetting("PreviewPixmapWidth", 160);
        float pph = gContext->GetNumSetting("PreviewPixmapHeight", 120);

        if (aspect <= 0)
            aspect = ((float) width) / height;

        if (aspect > ppw / pph)
            pph = rint(ppw / aspect);
        else
            ppw = rint(pph * aspect);

        img = img.smoothScale((int) ppw, (int) pph);

        img.save( pRequest->m_sFileName.ascii(), "PNG" );
    }

    if (pInfo)
        delete pInfo;

    if (QFile::exists( pRequest->m_sFileName ))
    {
        pRequest->m_eResponseType   = ResponseTypeFile;
        pRequest->m_nResponseStatus = 200;
    }

}

/////////////////////////////////////////////////////////////////////////////
//                  
/////////////////////////////////////////////////////////////////////////////

void HttpStatus::GetRecording( HttpWorkerThread *pThread, 
                               HTTPRequest      *pRequest )
{
    bool bIndexFile = false;

    pRequest->m_eResponseType   = ResponseTypeHTML;
    pRequest->m_mapRespHeaders[ "Cache-Control" ] = "no-cache=\"Ext\", max-age = 5000";
    pRequest->m_nResponseStatus = 404;

    QString sChanId   = pRequest->m_mapParams[ "ChanId"    ];
    QString sStartTime= pRequest->m_mapParams[ "StartTime" ];

    if (sStartTime.length() == 0)
        return;

    // ----------------------------------------------------------------------
    // DSM-320 & DSM-520 Special File Request for Index file of MPEG
    //
    // -=>TODO: Need to reverse Engineer File Format & create on the fly
    //          from the RecordedMarkup Table.
    // ----------------------------------------------------------------------

    int nIdxPos = sStartTime.findRev( ".idx", -1, FALSE );

    if (nIdxPos >=0 )
    {
        bIndexFile = true;
        sStartTime = sStartTime.left( nIdxPos );
    }

    // ----------------------------------------------------------------------
    // Check to see if this is another request for the same recording...
    // ----------------------------------------------------------------------

    ThreadData *pData = (ThreadData *)pThread->GetWorkerData();

    if ((pData != NULL) && (pData->m_eType == ThreadData::DT_Recording))
    {
        if (pData->IsSameRecording( sChanId, sStartTime ))
           pRequest->m_sFileName = pData->m_sFileName;
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
            return;

        // ------------------------------------------------------------------
        // Read Recording From Database
        // ------------------------------------------------------------------

        ProgramInfo *pInfo = ProgramInfo::GetProgramFromRecorded( sChanId, dtStart );

        if (pInfo==NULL)
            return;

        if ( pInfo->hostname != gContext->GetHostName())
        {
            // We only handle requests for local resources   

            delete pInfo;

            return;     
        }

        pRequest->m_sFileName = pInfo->GetRecordFilename( gContext->GetFilePrefix() );

        delete pInfo;

        // ------------------------------------------------------------------
        // Store File information in WorkerThread Storage for next request (cache)
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

void HttpStatus::GetMusic( HttpWorkerThread *pThread, 
                           HTTPRequest      *pRequest )
{
    pRequest->m_eResponseType   = ResponseTypeHTML;
    pRequest->m_mapRespHeaders[ "Cache-Control" ] = "no-cache=\"Ext\", max-age = 5000";
    pRequest->m_nResponseStatus = 404;

    QString sId   = pRequest->m_mapParams[ "Id"    ];

    if (sId.length() == 0)
        return;

    int nTrack = sId.toInt();

    // ----------------------------------------------------------------------
    // Check to see if this is another request for the same recording...
    // ----------------------------------------------------------------------

    ThreadData *pData = (ThreadData *)pThread->GetWorkerData();

    if ((pData != NULL) && (pData->m_eType == ThreadData::DT_Music))
    {
        if (pData->m_nTrackNumber == nTrack )
           pRequest->m_sFileName = pData->m_sFileName;
        else
           pData = NULL;
    }

    // ----------------------------------------------------------------------
    // New request if pData == NULL
    // ----------------------------------------------------------------------

    if (pData == NULL)
    {
        QString sBasePath = gContext->GetSetting( "MusicLocation", "");

        // ------------------------------------------------------------------
        // Load Track's FileName
        // ------------------------------------------------------------------

        MSqlQuery query(MSqlQuery::InitCon());

        if (query.isConnected())
        {
            query.prepare("SELECT filename FROM musicmetadata WHERE intid = :KEY" );
            query.bindValue(":KEY", nTrack );
            query.exec();

            if (query.isActive() && query.size() > 0)
            {
                query.first();  
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

void HttpStatus::GetStatusXML( HTTPRequest *pRequest )
{
    QDomDocument doc( "Status" );                        

    FillStatusXML( &doc );

    pRequest->m_eResponseType   = ResponseTypeXML;
    pRequest->m_mapRespHeaders[ "Cache-Control" ] = "no-cache=\"Ext\", max-age = 5000";
    pRequest->m_response << doc.toString();
}

/////////////////////////////////////////////////////////////////////////////
//                  
/////////////////////////////////////////////////////////////////////////////

void HttpStatus::GetStatusHTML( HTTPRequest *pRequest )
{
    pRequest->m_eResponseType = ResponseTypeHTML;
    pRequest->m_mapRespHeaders[ "Cache-Control" ] = "no-cache=\"Ext\", max-age = 5000";

    QDomDocument doc( "Status" );                        

    FillStatusXML( &doc );

    PrintStatus( pRequest->m_response, &doc );
}

void HttpStatus::FillStatusXML( QDomDocument *pDoc )
{
    QString   dateFormat   = gContext->GetSetting("DateFormat", "M/d/yyyy");

    if (dateFormat.find(QRegExp("yyyy")) < 0)
        dateFormat += " yyyy";

    QString   shortdateformat = gContext->GetSetting("ShortDateFormat", "M/d");
    QString   timeformat      = gContext->GetSetting("TimeFormat", "h:mm AP");
    QDateTime qdtNow          = QDateTime::currentDateTime();

    // Add Root Node.

    QDomElement root = pDoc->createElement("Status");
    pDoc->appendChild(root);

    root.setAttribute("date"    , qdtNow.toString(dateFormat));
    root.setAttribute("time"    , qdtNow.toString(timeformat)   );
    root.setAttribute("version" , MYTH_BINARY_VERSION           );
    root.setAttribute("protoVer", MYTH_PROTO_VERSION            );

    // Add all encoders, if any

    QDomElement encoders = pDoc->createElement("Encoders");
    root.appendChild(encoders);

    int  numencoders = 0;
    bool isLocal     = true;

    QMap<int, EncoderLink *>::Iterator iter = m_pEncoders->begin();

    for (; iter != m_pEncoders->end(); ++iter)
    {
        EncoderLink *elink = iter.data();

        if (elink != NULL)
        {
            isLocal = elink->IsLocal();

            QDomElement encoder = pDoc->createElement("Encoder");
            encoders.appendChild(encoder);

            encoder.setAttribute("id"            , elink->GetCardID()       );
            encoder.setAttribute("local"         , isLocal                  );
            encoder.setAttribute("connected"     , elink->IsConnected()     );
            encoder.setAttribute("state"         , elink->GetState()        );
            //encoder.setAttribute("lowOnFreeSpace", elink->isLowOnFreeSpace());

            if (isLocal)
                encoder.setAttribute("hostname", gContext->GetHostName());
            else
                encoder.setAttribute("hostname", elink->GetHostName());

            if (elink->IsConnected())
                numencoders++;

            switch (elink->GetState())
            {
                case kState_WatchingLiveTV:
                case kState_RecordingOnly:
                case kState_WatchingRecording:
                {
                    ProgramInfo *pInfo = elink->GetRecording();

                    if (pInfo)
                    {
                        FillProgramInfo(pDoc, encoder, pInfo);
                        delete pInfo;
                    }

                    break;
                }

                default:
                    break;
            }
        }
    }

    encoders.setAttribute("count", numencoders);

    // Add upcoming shows

    QDomElement scheduled = pDoc->createElement("Scheduled");
    root.appendChild(scheduled);

    list<ProgramInfo *> recordingList;

    if (m_pSched)
        m_pSched->getAllPending(&recordingList);

    unsigned int iNum = 10;
    unsigned int iNumRecordings = 0;

    list<ProgramInfo *>::iterator itProg = recordingList.begin();
    for (; (itProg != recordingList.end()) && iNumRecordings < iNum; itProg++)
    {
        if (((*itProg)->recstatus  <= rsWillRecord) &&
            ((*itProg)->recstartts >= QDateTime::currentDateTime()))
        {
            iNumRecordings++;
            FillProgramInfo(pDoc, scheduled, *itProg);
        }
    }

    while (recordingList.size() > 0)
    {
        ProgramInfo *pginfo = recordingList.back();
        delete pginfo;
        recordingList.pop_back();
    }

    scheduled.setAttribute("count", iNumRecordings);

    // Add Job Queue Entries

    QDomElement jobqueue = pDoc->createElement("JobQueue");
    root.appendChild(jobqueue);

    QMap<int, JobQueueEntry> jobs;
    QMap<int, JobQueueEntry>::Iterator it;

    JobQueue::GetJobsInQueue(jobs,
                             JOB_LIST_NOT_DONE | JOB_LIST_ERROR |
                             JOB_LIST_RECENT);

    if (jobs.size())
    {
        for (it = jobs.begin(); it != jobs.end(); ++it)
        {
            ProgramInfo *pInfo;

            pInfo = ProgramInfo::GetProgramFromRecorded(it.data().chanid,
                                                        it.data().starttime);

            if (!pInfo)
                continue;

            QDomElement job = pDoc->createElement("Job");
            jobqueue.appendChild(job);

            job.setAttribute("id"        , it.data().id         );
            job.setAttribute("chanId"    , it.data().chanid     );
            job.setAttribute("startTime" , it.data().starttime.toString(Qt::ISODate));
            job.setAttribute("startTs"   , it.data().startts    );
            job.setAttribute("insertTime", it.data().inserttime.toString(Qt::ISODate));
            job.setAttribute("type"      , it.data().type       );
            job.setAttribute("cmds"      , it.data().cmds       );
            job.setAttribute("flags"     , it.data().flags      );
            job.setAttribute("status"    , it.data().status     );
            job.setAttribute("statusTime", it.data().statustime.toString(Qt::ISODate));
            job.setAttribute("args"      , it.data().args       );

            if (it.data().hostname == "")
                job.setAttribute("hostname", QObject::tr("master"));
            else
                job.setAttribute("hostname",it.data().hostname);

            QDomText textNode = pDoc->createTextNode(it.data().comment);
            job.appendChild(textNode);

            FillProgramInfo(pDoc, job, pInfo);

            delete pInfo;
        }
    }

    jobqueue.setAttribute( "count", jobs.size() );

    // Add Machine information

    QDomElement mInfo   = pDoc->createElement("MachineInfo");
    QDomElement storage = pDoc->createElement("Storage"    );
    QDomElement load    = pDoc->createElement("Load"       );
    QDomElement thermal = pDoc->createElement("Thermal"    );
    QDomElement guide   = pDoc->createElement("Guide"      );

    root.appendChild (mInfo  );
    mInfo.appendChild(storage);
    mInfo.appendChild(load   );
    mInfo.appendChild(guide  );

    // drive space   --------------------- 
 
    long long iTotal = -1, iUsed = -1, iAvail = -1; 
 
    iAvail = getDiskSpace( gContext->GetFilePrefix(), iTotal, iUsed); 
 
    storage.setAttribute("_local_:total", (int)(iTotal>>10)); 
    storage.setAttribute("_local_:used" , (int)(iUsed>>10)); 
    storage.setAttribute("_local_:free" , (int)(iAvail>>10)); 
 
    if (m_bIsMaster) 
    { 
        long long mTotal =  0, mUsed =  0, mAvail =  0; 
        long long gTotal =  0, gUsed =  0, gAvail =  0; 
        QString hosts = "_local_"; 
        QMap <QString, bool> backendsCounted; 
        QString encoderHost; 
        QMap<int, EncoderLink *>::Iterator eit; 
 
        gTotal = iTotal; 
        gUsed  = iUsed; 
        gAvail = iAvail; 
 
        for (eit = m_pEncoders->begin(); eit != m_pEncoders->end(); ++eit) 
        { 
            encoderHost = eit.data()->GetHostName(); 
            if (eit.data()->IsConnected() && 
                !eit.data()->IsLocal() && 
                !backendsCounted.contains(encoderHost)) 
            { 
                backendsCounted[encoderHost] = true; 
                hosts += "," + encoderHost; 
 
                eit.data()->GetFreeDiskSpace(mTotal, mUsed); 
                mAvail = mTotal - mUsed; 
 
                storage.setAttribute(encoderHost + ":total", (int)(mTotal>>10)); 
                storage.setAttribute(encoderHost + ":used" , (int)(mUsed>>10)); 
                storage.setAttribute(encoderHost + ":free" , (int)(mAvail>>10)); 
 
                if ((mTotal == iTotal) && 
                    (myAbs(mAvail - iAvail) < (iAvail * 0.05))) 
                { 
                    storage.setAttribute(encoderHost + ":shared" , 1); 
                } 
                else 
                { 
                    storage.setAttribute(encoderHost + ":shared" , 0); 
                    gTotal += mTotal; 
                    gUsed  += mUsed; 
                    gAvail += mAvail; 
                } 
            } 
        } 
        storage.setAttribute("_total_:total", (int)(gTotal>>10)); 
        storage.setAttribute("_total_:used" , (int)(gUsed>>10)); 
        storage.setAttribute("_total_:free" , (int)(gAvail>>10)); 
 
        if (hosts != "_local_") 
            hosts += ",_total_"; 
        storage.setAttribute("slaves", hosts); 
    } 

    // load average ---------------------

    double rgdAverages[3];

    if (getloadavg(rgdAverages, 3) != -1)
    {
        load.setAttribute("avg1", rgdAverages[0]);
        load.setAttribute("avg2", rgdAverages[1]);
        load.setAttribute("avg3", rgdAverages[2]);
    }

 
    //temperature ----------------- 
    // Try ACPI first, then lmsensor 2nd 
    QDir dir("/proc/acpi/thermal_zone"); 
    bool found_acpi = false; 
    QString acpiTempDir; 
    if (dir.exists()) 
    { 
        QStringList lst = dir.entryList(); 
        QRegExp rxp = QRegExp ("TH?M?", TRUE, FALSE); 
        QString line, temp; 
        for (QStringList::Iterator it = lst.begin(); it != lst.end(); ++it) 
        { 
            if ( (*it).contains(rxp)) 
            { 
                acpiTempDir = dir.absFilePath(*it); 
            } 
        } 
        
        QFile acpiTempFile(acpiTempDir.append("/temperature")); 
        if (acpiTempFile.open(IO_ReadOnly)) 
        { 
            QTextStream stream (&acpiTempFile); 
            line = stream.readLine(); 
            rxp = QRegExp ("(\\d+)", TRUE, FALSE); 
            if (rxp.search(line) != -1 ) 
            { 
                temp = rxp.cap(1); 
                temp += " &#8451;"; // print degress Celsius  
                mInfo.appendChild(thermal); 
                thermal.setAttribute("temperature", temp); 
                found_acpi = true; 
            } 
        }  
        acpiTempFile.close(); 
    }                                                  

#ifdef HAVE_LMSENSORS 
    if (!found_acpi) 
    { 
        int chip_nr, a, b; 
        char *label = NULL; 
        double value; 
        const sensors_chip_name *chip; 
        const sensors_feature_data *data; 
        char* lmsensorConfigName = LMSENSOR_DEFAULT_CONFIG_FILE; 
        a = b = 0; 
        FILE *lmsensorConfigFile = fopen(lmsensorConfigName, "r"); 
        sensors_init(lmsensorConfigFile); 
        fclose(lmsensorConfigFile); 
        for (chip_nr = 0 ; (chip = sensors_get_detected_chips(&chip_nr)) ; ) 
        { 
            while ((data = sensors_get_all_features(*chip, &a, &b))) 
            { 
                if ((!sensors_get_label(*chip, data->number, &label)) &&  
                    (!sensors_get_feature(*chip, data->number, &value))) 
                { 
                    // Find label matching "CPU Temp" or "Temp/CPU" 
                    QRegExp rxp = QRegExp ("(CPU.+Temp)|(Temp.+CPU)", FALSE, FALSE); 
                    if (rxp.search(QString(label)) != -1  && value > 0) 
                    { 
                        QString temp = QString("%1").arg(value); 
                        temp += " &#8451;"; 
                        mInfo.appendChild(thermal); 
                        thermal.setAttribute("temperature", temp); 
                    } 
                } 
            } 
        }  
        sensors_cleanup(); 
    } 
#endif 

    // Guide Data ---------------------

    QDateTime GuideDataThrough;

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT max(endtime) FROM program;");

    if (query.exec() && query.isActive() && query.size())
    {
        query.next();

        if (query.isValid())
            GuideDataThrough = QDateTime::fromString(query.value(0).toString(),
                                                     Qt::ISODate);
    }

    guide.setAttribute("start", gContext->GetSetting("mythfilldatabaseLastRunStart"));
    guide.setAttribute("end", gContext->GetSetting("mythfilldatabaseLastRunEnd"));
    guide.setAttribute("status", gContext->GetSetting("mythfilldatabaseLastRunStatus"));
    if (gContext->GetNumSetting("MythFillGrabberSuggestsTime", 0))
    {
        guide.setAttribute("next",
            gContext->GetSetting("MythFillSuggestedRunTime"));
    }

    if (!GuideDataThrough.isNull())
    {
        guide.setAttribute("guideThru", QDateTime(GuideDataThrough).toString(Qt::ISODate));
        guide.setAttribute("guideDays", qdtNow.daysTo(GuideDataThrough));
    }

    QDomText dataDirectMessage = pDoc->createTextNode(gContext->GetSetting("DataDirectMessage"));
    guide.appendChild(dataDirectMessage);
}

void HttpStatus::FillProgramInfo(QDomDocument *pDoc, QDomElement &e, 
                                 ProgramInfo *pInfo, bool bIncChannel /* = true */)
{
    if ((pDoc == NULL) || (pInfo == NULL))
        return;

    // Build Program Element

    QDomElement program = pDoc->createElement( "Program" );
    e.appendChild( program );

    program.setAttribute( "seriesId"    , pInfo->seriesid     );
    program.setAttribute( "programId"   , pInfo->programid    );
    program.setAttribute( "title"       , pInfo->title        );
    program.setAttribute( "subTitle"    , pInfo->subtitle     );
    program.setAttribute( "category"    , pInfo->category     );
    program.setAttribute( "catType"     , pInfo->catType      );
    program.setAttribute( "startTime"   , pInfo->startts.toString(Qt::ISODate));
    program.setAttribute( "endTime"     , pInfo->endts.toString(Qt::ISODate));
    program.setAttribute( "repeat"      , pInfo->repeat       );
    program.setAttribute( "stars"       , pInfo->stars        );
    program.setAttribute( "fileSize"    , longLongToString( pInfo->filesize ));
    program.setAttribute( "lastModified", pInfo->lastmodified.toString(Qt::ISODate) );
    program.setAttribute( "programFlags", pInfo->programflags );
    program.setAttribute( "hostname"    , pInfo->hostname     );

    if (pInfo->hasAirDate)
        program.setAttribute( "airdate"  , pInfo->originalAirDate.toString(Qt::ISODate) );

    QDomText textNode = pDoc->createTextNode( pInfo->description );
    program.appendChild( textNode );

    if ( bIncChannel )
    {
        // Build Channel Child Element
        
        QDomElement channel = pDoc->createElement( "Channel" );
        program.appendChild( channel );

        FillChannelInfo( channel, pInfo );
    }

    // Build Recording Child Element

    QDomElement recording = pDoc->createElement( "Recording" );
    program.appendChild( recording );

    recording.setAttribute( "recordId"      , pInfo->recordid    );
    recording.setAttribute( "recStartTs"    , pInfo->recstartts.toString(Qt::ISODate));
    recording.setAttribute( "recEndTs"      , pInfo->recendts.toString(Qt::ISODate));
    recording.setAttribute( "recStatus"     , pInfo->recstatus   );
    recording.setAttribute( "recPriority"   , pInfo->recpriority );
    recording.setAttribute( "recGroup"      , pInfo->recgroup    );
    recording.setAttribute( "playGroup"     , pInfo->playgroup   );
    recording.setAttribute( "recType"       , pInfo->rectype     );
    recording.setAttribute( "dupInType"     , pInfo->dupin       );
    recording.setAttribute( "dupMethod"     , pInfo->dupmethod   );

    recording.setAttribute( "encoderId"     , pInfo->cardid      );

    recording.setAttribute( "recProfile"    , pInfo->GetProgramRecordingProfile());

    recording.setAttribute( "preRollSeconds", gContext->GetNumSetting("RecordPreRoll", 0));
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void HttpStatus::FillChannelInfo( QDomElement &channel, ProgramInfo *pInfo )
{
    if (pInfo)
    {
        QString sHostName = gContext->GetHostName();
        QString sPort     = gContext->GetSettingOnHost( "BackendStatusPort", sHostName);
        QString sIconURL  = QString( "http://%1:%2/getChannelIcon?ChanId=%3" )
                                   .arg( sHostName )
                                   .arg( sPort )
                                   .arg( pInfo->chanid );

        channel.setAttribute( "chanId"     , pInfo->chanid      );
        channel.setAttribute( "callSign"   , pInfo->chansign    );
        channel.setAttribute( "iconURL"    , sIconURL           );
        channel.setAttribute( "channelName", pInfo->channame    );
        channel.setAttribute( "chanFilters", pInfo->chanOutputFilters );
        channel.setAttribute( "sourceId"   , pInfo->sourceid    );
        channel.setAttribute( "inputId"    , pInfo->inputid     );
        channel.setAttribute( "commFree"   , pInfo->chancommfree);
    }
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void HttpStatus::PrintStatus( QTextStream &os, QDomDocument *pDoc )
{
    
    QString shortdateformat = gContext->GetSetting("ShortDateFormat", "M/d");
    QString timeformat      = gContext->GetSetting("TimeFormat", "h:mm AP");

    os.setEncoding(QTextStream::UnicodeUTF8);

    QDateTime qdtNow = QDateTime::currentDateTime();

    QDomElement docElem = pDoc->documentElement();

    os << "<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.0 Strict//EN\" "
       << "\"http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd\">\r\n"
       << "<html xmlns=\"http://www.w3.org/1999/xhtml\""
       << " xml:lang=\"en\" lang=\"en\">\r\n"
       << "<head>\r\n"
       << "  <meta http-equiv=\"Content-Type\""
       << "content=\"text/html; charset=UTF-8\" />\r\n"
       << "  <style type=\"text/css\" title=\"Default\" media=\"all\">\r\n"
       << "  <!--\r\n"
       << "  body {\r\n"
       << "    background-color:#fff;\r\n"
       << "    font:11px verdana, arial, helvetica, sans-serif;\r\n"
       << "    margin:20px;\r\n"
       << "  }\r\n"
       << "  h1 {\r\n"
       << "    font-size:28px;\r\n"
       << "    font-weight:900;\r\n"
       << "    color:#ccc;\r\n"
       << "    letter-spacing:0.5em;\r\n"
       << "    margin-bottom:30px;\r\n"
       << "    width:650px;\r\n"
       << "    text-align:center;\r\n"
       << "  }\r\n"
       << "  h2 {\r\n"
       << "    font-size:18px;\r\n"
       << "    font-weight:800;\r\n"
       << "    color:#360;\r\n"
       << "    border:none;\r\n"
       << "    letter-spacing:0.3em;\r\n"
       << "    padding:0px;\r\n"
       << "    margin-bottom:10px;\r\n"
       << "    margin-top:0px;\r\n"
       << "  }\r\n"
       << "  hr {\r\n"
       << "    display:none;\r\n"
       << "  }\r\n"
       << "  div.content {\r\n"
       << "    width:650px;\r\n"
       << "    border-top:1px solid #000;\r\n"
       << "    border-right:1px solid #000;\r\n"
       << "    border-bottom:1px solid #000;\r\n"
       << "    border-left:10px solid #000;\r\n"
       << "    padding:10px;\r\n"
       << "    margin-bottom:30px;\r\n"
       << "    -moz-border-radius:8px 0px 0px 8px;\r\n"
       << "  }\r\n"
       << "  div#schedule a {\r\n"
       << "    display:block;\r\n"
       << "    color:#000;\r\n"
       << "    text-decoration:none;\r\n"
       << "    padding:.2em .8em;\r\n"
       << "    border:thin solid #fff;\r\n"
       << "    width:350px;\r\n"
       << "  }\r\n"
       << "  div#schedule a span {\r\n"
       << "    display:none;\r\n"
       << "  }\r\n"
       << "  div#schedule a:hover {\r\n"
       << "    background-color:#F4F4F4;\r\n"
       << "    border-top:thin solid #000;\r\n"
       << "    border-bottom:thin solid #000;\r\n"
       << "    border-left:thin solid #000;\r\n"
       << "    cursor:default;\r\n"
       << "  }\r\n"
       << "  div#schedule a:hover span {\r\n"
       << "    display:block;\r\n"
       << "    position:absolute;\r\n"
       << "    background-color:#F4F4F4;\r\n"
       << "    color:#000;\r\n"
       << "    left:400px;\r\n"
       << "    margin-top:-20px;\r\n"
       << "    width:280px;\r\n"
       << "    padding:5px;\r\n"
       << "    border:thin dashed #000;\r\n"
       << "  }\r\n"
       << "  div.loadstatus {\r\n"
       << "    width:325px;\r\n"
       << "    height:7em;\r\n"
       << "    float:right;\r\n"
       << "  }\r\n"
       << "  .jobfinished { color: #0000ff; }\r\n"
       << "  .jobaborted { color: #7f0000; }\r\n"
       << "  .joberrored { color: #ff0000; }\r\n"
       << "  .jobrunning { color: #005f00; }\r\n"
       << "  .jobqueued  {  }\r\n"
       << "  -->\r\n"
       << "  </style>\r\n"
       << "  <title>MythTV Status - " 
       << docElem.attribute( "date", qdtNow.toString(shortdateformat)  )
       << " " 
       << docElem.attribute( "time", qdtNow.toString(timeformat) ) << " - "
       << docElem.attribute( "version", MYTH_BINARY_VERSION ) << "</title>\r\n"
       << "</head>\r\n"
       << "<body>\r\n\r\n"
       << "  <h1>MythTV Status</h1>\r\n";

    int nNumEncoders = 0;

    // encoder information ---------------------

    QDomNode node = docElem.namedItem( "Encoders" );

    if (!node.isNull())
        nNumEncoders = PrintEncoderStatus( os, node.toElement() );

    // upcoming shows --------------------------

    node = docElem.namedItem( "Scheduled" );

    if (!node.isNull())
        PrintScheduled( os, node.toElement());

    // Job Queue Entries -----------------------

    node = docElem.namedItem( "JobQueue" );

    if (!node.isNull())
        PrintJobQueue( os, node.toElement());


    // Machine information ---------------------

    node = docElem.namedItem( "MachineInfo" );

    if (!node.isNull())
        PrintMachineInfo( os, node.toElement());

    os << "\r\n</body>\r\n</html>\r\n";

}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

int HttpStatus::PrintEncoderStatus( QTextStream &os, QDomElement encoders )
{
    QString timeformat   = gContext->GetSetting("TimeFormat", "h:mm AP");
    int     nNumEncoders = 0;

    if (encoders.isNull())
        return 0;

    os << "  <div class=\"content\">\r\n"
       << "    <h2>Encoder status</h2>\r\n";

    QDomNode node = encoders.firstChild();

    while (!node.isNull())
    {
        QDomElement e = node.toElement();

        if (!e.isNull())
        {
            if (e.tagName() == "Encoder")
            {
                QString sIsLocal  = (e.attribute( "local"    , "remote" )== "1")
                                                           ? "local" : "remote";
                QString sCardId   =  e.attribute( "id"       , "0"      );
                QString sHostName =  e.attribute( "hostname" , "Unknown");
                bool    bConnected=  e.attribute( "connected", "0"      ).toInt();

                bool bIsLowOnFreeSpace=e.attribute( "lowOnFreeSpace", "0").toInt();
                                     
                os << "    Encoder " << sCardId << " is " << sIsLocal 
                   << " on " << sHostName;

                if ((sIsLocal == "remote") && !bConnected)
                {
                    os << " (currently not connected).<br />";

                    node = node.nextSibling();
                    continue;
                }

                nNumEncoders++;

                TVState encState = (TVState) e.attribute( "state", "0").toInt();

                switch( encState )
                {   
                    case kState_WatchingLiveTV:
                        os << " and is watching Live TV";
                        break;

                    case kState_RecordingOnly:
                    case kState_WatchingRecording:
                        os << " and is recording";
                        break;

                    default:
                        os << " and is not recording.";
                        break;
                }

                // Display first Program Element listed under the encoder

                QDomNode tmpNode = e.namedItem( "Program" );

                if (!tmpNode.isNull())
                {
                    QDomElement program  = tmpNode.toElement();  

                    if (!program.isNull())
                    {
                        os << ": '" << program.attribute( "title", "Unknown" ) << "'";

                        // Get Channel information
                        
                        tmpNode = program.namedItem( "Channel" );

                        if (!tmpNode.isNull())
                        {
                            QDomElement channel = tmpNode.toElement();

                            if (!channel.isNull())
                                os <<  " on "  
                                   << channel.attribute( "callSign", "unknown" );
                        }

                        // Get Recording Information (if any)
                        
                        tmpNode = program.namedItem( "Recording" );

                        if (!tmpNode.isNull())
                        {
                            QDomElement recording = tmpNode.toElement();

                            if (!recording.isNull())
                            {
                                QDateTime endTs = QDateTime::fromString( 
                                         recording.attribute( "recEndTs", "" ),
                                         Qt::ISODate );

                                os << ". This recording will end "
                                   << "at " << endTs.toString(timeformat);
                            }
                        }
                    }

                    os << ".";
                }

                if (bIsLowOnFreeSpace)
                {
                    os << " <strong>WARNING</strong>:"
                       << " This backend is low on free disk space!";
                }

                os << "<br />\r\n";
            }
        }

        node = node.nextSibling();
    }

    os << "  </div>\r\n\r\n";

    return( nNumEncoders );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

int HttpStatus::PrintScheduled( QTextStream &os, QDomElement scheduled )
{
    QDateTime qdtNow          = QDateTime::currentDateTime();
    QString   shortdateformat = gContext->GetSetting("ShortDateFormat", "M/d");
    QString   timeformat      = gContext->GetSetting("TimeFormat", "h:mm AP");

    if (scheduled.isNull())
        return( 0 );

    int     nNumRecordings= scheduled.attribute( "count", "0" ).toInt();
    
    os << "  <div class=\"content\">\r\n"
       << "    <h2>Schedule</h2>\r\n";

    if (nNumRecordings == 0)
    {
        os << "    There are no shows scheduled for recording.\r\n"
           << "    </div>\r\n";
        return( 0 );
    }

    os << "    The next " << nNumRecordings << " show" << (nNumRecordings == 1 ? "" : "s" )
       << " that " << (nNumRecordings == 1 ? "is" : "are") 
       << " scheduled for recording:\r\n";

    os << "    <div id=\"schedule\">\r\n";

    // Iterate through all scheduled programs

    QDomNode node = scheduled.firstChild();

    while (!node.isNull())
    {
        QDomElement e = node.toElement();

        if (!e.isNull())
        {
            QDomNode recNode  = e.namedItem( "Recording" );
            QDomNode chanNode = e.namedItem( "Channel"   );

            if ((e.tagName() == "Program") && !recNode.isNull() && !chanNode.isNull())
            {
                QDomElement r =  recNode.toElement();
                QDomElement c =  chanNode.toElement();

                QString   sTitle       = e.attribute( "title"   , "" );    
                QString   sSubTitle    = e.attribute( "subTitle", "" );
                QDateTime startTs      = QDateTime::fromString( e.attribute( "startTime" ,"" ), Qt::ISODate );
                QDateTime endTs        = QDateTime::fromString( e.attribute( "endTime"   ,"" ), Qt::ISODate );
                QDateTime recStartTs   = QDateTime::fromString( r.attribute( "recStartTs","" ), Qt::ISODate );
//                QDateTime recEndTs     = QDateTime::fromString( r.attribute( "recEndTs"  ,"" ), Qt::ISODate );
                int       nPreRollSecs = r.attribute( "preRollSeconds", "0" ).toInt();
                int       nEncoderId   = r.attribute( "encoderId"     , "0" ).toInt();
                QString   sProfile     = r.attribute( "recProfile"    , ""  );
                QString   sChanName    = c.attribute( "channelName"   , ""  );
                QString   sDesc        = "";

                QDomText  text         = e.firstChild().toText();
                if (!text.isNull())
                    sDesc = text.nodeValue();

                // Build Time to recording start.

                int nTotalSecs = qdtNow.secsTo( recStartTs ) - nPreRollSecs;

                //since we're not displaying seconds
    
                nTotalSecs -= 60;          

                int nTotalDays  =  nTotalSecs / 86400;
                int nTotalHours = (nTotalSecs / 3600)
                                - (nTotalDays * 24);
                int nTotalMins  = (nTotalSecs / 60) % 60;

                QString sTimeToStart = "in";

                if ( nTotalDays > 1 )
                    sTimeToStart += QString(" %1 days,").arg( nTotalDays );
                else if ( nTotalDays == 1 )
                    sTimeToStart += (" 1 day,");

                if ( nTotalHours != 1)
                    sTimeToStart += QString(" %1 hours and").arg( nTotalHours );
                else if (nTotalHours == 1)
                    sTimeToStart += " 1 hour and";
 
                if ( nTotalMins != 1)
                    sTimeToStart += QString(" %1 minutes").arg( nTotalMins );
                else
                    sTimeToStart += " 1 minute";

                if ( nTotalHours == 0 && nTotalMins == 0)
                    sTimeToStart = "within one minute";

                if ( nTotalSecs < 0)
                    sTimeToStart = "soon";

                    // Output HTML

                os << "      <a href=\"#\">";
                if (shortdateformat.find("ddd") == -1) {
                    // If day-of-week not already present somewhere, prepend it.
                    os << recStartTs.addSecs(-nPreRollSecs).toString("ddd")
                        << " ";
                }
                os << recStartTs.addSecs(-nPreRollSecs).toString(shortdateformat) << " "
                   << recStartTs.addSecs(-nPreRollSecs).toString(timeformat) << " - ";

                if (nEncoderId > 0)
                    os << "Encoder " << nEncoderId << " - ";

                os << sChanName << " - " << sTitle << "<br />"
                   << "<span><strong>" << sTitle << "</strong> ("
                   << startTs.toString(timeformat) << "-"
                   << endTs.toString(timeformat) << ")<br />";

                if ( !sSubTitle.isNull() && !sSubTitle.isEmpty())
                    os << "<em>" << sSubTitle << "</em><br /><br />";

                os << sDesc << "<br /><br />"
                   << "This recording will start "  << sTimeToStart
                   << " using encoder " << nEncoderId << " with the '"
                   << sProfile << "' profile.</span></a><hr />\r\n";
            }
        }

        node = node.nextSibling();
    }
    os  << "    </div>\r\n";
    os << "  </div>\r\n\r\n";

    return( nNumRecordings );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

int HttpStatus::PrintJobQueue( QTextStream &os, QDomElement jobs )
{
    QString   shortdateformat = gContext->GetSetting("ShortDateFormat", "M/d");
    QString   timeformat      = gContext->GetSetting("TimeFormat", "h:mm AP");

    if (jobs.isNull())
        return( 0 );

    int nNumJobs= jobs.attribute( "count", "0" ).toInt();
    
    os << "  <div class=\"content\">\r\n"
       << "    <h2>Job Queue</h2>\r\n";

    if (nNumJobs != 0)
    {
        QString statusColor;
        QString jobColor;
        QString timeDateFormat;

        timeDateFormat = gContext->GetSetting("DateFormat", "ddd MMMM d") +
                         " " + gContext->GetSetting("TimeFormat", "h:mm AP");

        os << "    Jobs currently in Queue or recently ended:\r\n<br />"
           << "    <div id=\"schedule\">\r\n";

        
        QDomNode node = jobs.firstChild();

        while (!node.isNull())
        {
            QDomElement e = node.toElement();

            if (!e.isNull())
            {
                QDomNode progNode = e.namedItem( "Program"   );

                if ((e.tagName() == "Job") && !progNode.isNull() )
                {
                    QDomElement p =  progNode.toElement();

                    QDomNode recNode  = p.namedItem( "Recording" );
                    QDomNode chanNode = p.namedItem( "Channel"   );

                    QDomElement r =  recNode.toElement();
                    QDomElement c =  chanNode.toElement();

                    int    nType   = e.attribute( "type"  , "0" ).toInt();
                    int nStatus = e.attribute( "status", "0" ).toInt();

                    switch( nStatus )
                    {
                        case JOB_ABORTED:
                            statusColor = " class=\"jobaborted\"";
                            jobColor = "";
                            break;

                        case JOB_ERRORED:
                            statusColor = " class=\"joberrored\"";
                            jobColor = " class=\"joberrored\"";
                            break;

                        case JOB_FINISHED:
                            statusColor = " class=\"jobfinished\"";
                            jobColor = " class=\"jobfinished\"";
                            break;

                        case JOB_RUNNING:
                            statusColor = " class=\"jobrunning\"";
                            jobColor = " class=\"jobrunning\"";
                            break;

                        default:
                            statusColor = " class=\"jobqueued\"";
                            jobColor = " class=\"jobqueued\"";
                            break;
                    }

                    QString   sTitle       = p.attribute( "title"   , "" );       //.replace(QRegExp("\""), "&quot;");
                    QString   sSubTitle    = p.attribute( "subTitle", "" );
                    QDateTime startTs      = QDateTime::fromString( p.attribute( "startTime" ,"" ), Qt::ISODate );
                    QDateTime endTs        = QDateTime::fromString( p.attribute( "endTime"   ,"" ), Qt::ISODate );
                    QDateTime recStartTs   = QDateTime::fromString( r.attribute( "recStartTs","" ), Qt::ISODate );
                    QDateTime statusTime   = QDateTime::fromString( e.attribute( "statusTime","" ), Qt::ISODate );
                    QString   sHostname    = e.attribute( "hostname", "master" );
                    QString   sComment     = "";

                    QDomText  text         = e.firstChild().toText();
                    if (!text.isNull())
                        sComment = text.nodeValue();

                    os << "<a href=\"#\">"
                       << recStartTs.toString("ddd") << " "
                       << recStartTs.toString(shortdateformat) << " "
                       << recStartTs.toString(timeformat) << " - "
                       << sTitle << " - <font" << jobColor << ">"
                       << JobQueue::JobText( nType ) << "</font><br />"
                       << "<span><strong>" << sTitle << "</strong> ("
                       << startTs.toString(timeformat) << "-"
                       << endTs.toString(timeformat) << ")<br />";

                    if ( !sSubTitle.isNull() && !sSubTitle.isEmpty())
                        os << "<em>" << sSubTitle << "</em><br /><br />";

                    os << "Job: " << JobQueue::JobText( nType ) << "<br />"
                       << "Status: <font" << statusColor << ">"
                       << JobQueue::StatusText( nStatus )
                       << "</font><br />"
                       << "Status Time: "
                       << statusTime.toString(timeDateFormat)
                       << "<br />";

                    if ( nStatus != JOB_QUEUED)
                        os << "Host: " << sHostname << "<br />";

                    if (!sComment.isNull() && !sComment.isEmpty())
                        os << "<br />Comments:<br />" << sComment << "<br />";

                    os << "</span></a><hr />\r\n";
                }
            }

            node = node.nextSibling();
        }
        os << "      </div>\r\n";
    }
    else
        os << "    Job Queue is currently empty.\r\n\r\n";

    os << "  </div>\r\n\r\n ";

    return( nNumJobs );

}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

int HttpStatus::PrintMachineInfo( QTextStream &os, QDomElement info )
{
    QString   shortdateformat = gContext->GetSetting("ShortDateFormat", "M/d");
    QString   timeformat      = gContext->GetSetting("TimeFormat", "h:mm AP");
    QString   sRep;

    if (info.isNull())
        return( 0 );

    os << "<div class=\"content\">\r\n"
       << "    <h2>Machine information</h2>\r\n";

    // load average ---------------------

    QDomNode node = info.namedItem( "Load" );

    if (!node.isNull())
    {    
        QDomElement e = node.toElement();

        if (!e.isNull())
        {
            double dAvg1 = e.attribute( "avg1" , "0" ).toDouble();
            double dAvg2 = e.attribute( "avg2" , "0" ).toDouble();
            double dAvg3 = e.attribute( "avg3" , "0" ).toDouble();

            os << "    <div class=\"loadstatus\">\r\n"
               << "      This machine's load average:"
               << "\r\n      <ul>\r\n        <li>"
               << "1 Minute: " << dAvg1 << "</li>\r\n"
               << "        <li>5 Minutes: " << dAvg2 << "</li>\r\n"
               << "        <li>15 Minutes: " << dAvg3
               << "</li>\r\n      </ul>\r\n"
               << "    </div>\r\n";    
        }
    }

    // local drive space   ---------------------
    
    node = info.namedItem( "Storage" );

    if (!node.isNull())
    {    
        QDomElement e = node.toElement();

        if (!e.isNull())
        {
            QString slaves = e.attribute("slaves", "_local_");
            QStringList tokens = QStringList::split(",", slaves);

            os << "      Disk Usage:<br />\r\n";
            os << "      <ul>\r\n";

            for (unsigned int i = 0; i < tokens.size(); i++)
            {
                int nFree = e.attribute(tokens[i] + ":free" , "0" ).toInt();
                int nTotal= e.attribute(tokens[i] + ":total", "0" ).toInt();
                int nUsed = e.attribute(tokens[i] + ":used" , "0" ).toInt();

                if (slaves == "_local_")
                {
                    // do nothing
                }
                else if (tokens[i] == "_local_")
                {
                    os << "        <li>Master Backend:\r\n"
                       << "          <ul>\r\n";
                }
                else if (tokens[i] == "_total_")
                {
                    os << "        <li>Total Disk Space:\r\n"
                       << "          <ul>\r\n";
                }
                else
                {
                    os << "        <li>" << tokens[i] << ": ";

                    if (e.attribute(tokens[i] + ":shared", "0").toInt())
                        os << " (Shared with master)";

                    os << "\r\n"
                       << "          <ul>\r\n";
                }

                os << "            <li>Total Space: ";
                sRep.sprintf( "%d,%03d MB ", (nTotal) / 1000, (nTotal) % 1000);
                os << sRep << "</li>\r\n";

                os << "            <li>Space Used: ";
                sRep.sprintf( "%d,%03d MB ", (nUsed) / 1000, (nUsed) % 1000);
                os << sRep << "</li>\r\n";

                os << "            <li>Space Free: ";
                sRep.sprintf( "%d,%03d MB ", (nFree) / 1000, (nFree) % 1000);
                os << sRep << "</li>\r\n";

                if (slaves != "_local_")
                    os << "          </ul>\r\n"
                       << "        </li>\r\n";
            }
            os << "      </ul>\r\n";
        }
    }

   // ACPI temperature ------------------

    node = info.namedItem( "Thermal" );

    if (!node.isNull())
    {
        QDomElement e = node.toElement();

        if (!e.isNull())
        {
            QString temperature = e.attribute( "temperature" , "0" );

            os << "      Current CPU temperature: "
               << temperature
               << ".<br />\r\n";
        }
    }
	
    // Guide Info ---------------------

    node = info.namedItem( "Guide" );

    if (!node.isNull())
    {    
        QDomElement e = node.toElement();

        if (!e.isNull())
        {
            QString datetimefmt = "yyyy-MM-dd hh:mm";
            int     nDays   = e.attribute( "guideDays", "0" ).toInt();
            QString sStart  = e.attribute( "start"    , ""  );
            QString sEnd    = e.attribute( "end"      , ""  );
            QString sStatus = e.attribute( "status"   , ""  );
            QDateTime next  = QDateTime::fromString( e.attribute( "next"     , ""  ), Qt::ISODate);
            QString sNext   = next.isNull() ? "" : next.toString(datetimefmt);
            QString sMsg    = "";

            QDateTime thru  = QDateTime::fromString( e.attribute( "guideThru", ""  ), Qt::ISODate);

            QDomText  text  = e.firstChild().toText();

            if (!text.isNull())
                sMsg = text.nodeValue();

            os << "    Last mythfilldatabase run started on " << sStart
               << " and ";

            if (sEnd < sStart)   
                os << "is ";
            else 
                os << "ended on " << sEnd << ". ";

            os << sStatus << "<br />\r\n";    

            if (!next.isNull() && sNext >= sStart)
            {
                os << "    Suggested next mythfilldatabase run: "
                    << sNext << ".<br />\r\n";
            }

            if (!thru.isNull())
            {
                os << "    There's guide data until "
                   << QDateTime( thru ).toString(datetimefmt);

                if (nDays > 0)
                    os << " (" << nDays << " day" << (nDays == 1 ? "" : "s" ) << ")";

                os << ".";

                if (nDays <= 3)
                    os << " <strong>WARNING</strong>: is mythfilldatabase running?";
            }
            else
                os << "    There's <strong>no guide data</strong> available! "
                   << "Have you run mythfilldatabase?";

            if (!sMsg.isNull() && !sMsg.isEmpty())
                os << "<br />\r\nDataDirect Status: " << sMsg;
        }
    }
    os << "\r\n  </div>\r\n";

    return( 1 );
}

// vim:set shiftwidth=4 tabstop=4 expandtab:
