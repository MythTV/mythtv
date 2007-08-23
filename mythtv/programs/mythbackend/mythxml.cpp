//////////////////////////////////////////////////////////////////////////////
// Program Name: MythXML.cpp
//                                                                            
// Purpose - Html & XML status HttpServerExtension
//                                                                            
// Created By  : David Blain                    Created On : Oct. 24, 2005
// Modified By :                                Modified On:                  
//                                                                            
//////////////////////////////////////////////////////////////////////////////

#include "mythxml.h"

#include "libmyth/mythcontext.h"
#include "libmyth/util.h"
#include "libmyth/mythdbcon.h"

#include <qtextstream.h>
#include <qdir.h>
#include <qfile.h>
#include <qregexp.h>
#include <qbuffer.h>
#include <math.h>

#include "../../config.h"
#ifdef HAVE_LMSENSORS 
    #define LMSENSOR_DEFAULT_CONFIG_FILE "/etc/sensors.conf" 
    #include <sensors/sensors.h> 
    #include <sensors/chips.h> 
#endif 

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

MythXML::MythXML( UPnpDevice *pDevice ) : Eventing( "MythXML", "MYTHTV_Event" )
{
    m_pEncoders = &tvList;
    m_pSched    = sched;
    m_pExpirer  = expirer;

    m_nPreRollSeconds = gContext->GetNumSetting("RecordPreRoll", 0);

    // Add any event variables...

    //  --- none at this time.

    QString sUPnpDescPath = UPnp::g_pConfig->GetValue( "UPnP/DescXmlPath", m_sSharePath );

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

    return( MXML_Unknown );
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
            if (pRequest->m_sBaseUrl != m_sControlUrl)
                return( false );

            switch( GetMethod( pRequest->m_sMethod ))
            {
                case MXML_GetServiceDescription: pRequest->FormatFileResponse( m_sServiceDescFileName ); return true;

                case MXML_GetProgramGuide      : GetProgramGuide( pRequest ); return true;
                case MXML_GetProgramDetails    : GetProgramDetails( pRequest ); return true;

                case MXML_GetHosts             : GetHosts       ( pRequest ); return true;
                case MXML_GetKeys              : GetKeys        ( pRequest ); return true;
                case MXML_GetSetting           : GetSetting     ( pRequest ); return true;
                case MXML_PutSetting           : PutSetting     ( pRequest ); return true;
                                                                              
                case MXML_GetChannelIcon       : GetChannelIcon ( pRequest ); return true;
                case MXML_GetRecorded          : GetRecorded    ( pRequest ); return true;
                case MXML_GetExpiring          : GetExpiring    ( pRequest ); return true;
                case MXML_GetPreviewImage      : GetPreviewImage( pRequest ); return true;

                case MXML_GetRecording         : GetRecording   ( pThread, pRequest ); return true;
                case MXML_GetMusic             : GetMusic       ( pThread, pRequest ); return true;
                case MXML_GetVideo             : GetVideo       ( pThread, pRequest ); return true;

                case MXML_GetConnectionInfo    : GetConnectionInfo( pRequest ); return true;
                case MXML_GetAlbumArt          : GetAlbumArt    ( pRequest ); return true;

                default: 
                {
                    UPnp::FormatErrorResponse( pRequest, UPnPResult_InvalidAction );

                    return true;
                }
            }
        }
    }
    catch( ... )
    {
        VERBOSE( VB_IMPORTANT, "MythXML::ProcessRequest() - Unexpected Exception" );
    }

    return( false );
}           

// ==========================================================================
// Request handler Methods
// ==========================================================================

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void MythXML::GetHosts( HTTPRequest *pRequest )
{
    pRequest->m_mapRespHeaders[ "Cache-Control" ] = "no-cache=\"Ext\", max-age = 5000";

    MSqlQuery query(MSqlQuery::InitCon());

    if (query.isConnected())
    {
        query.prepare("SELECT DISTINCTROW hostname "
                        "FROM settings WHERE (not isNull( hostname ));" );
        query.exec();

        if (query.isActive() && query.size() > 0)
        {
             QString     sHosts;
             QTextStream os( &sHosts, IO_WriteOnly );

            while(query.next())
            {
                QString sHost = query.value(0).toString();

                os << "<Host>" 
                   << HTTPRequest::Encode( sHost )
                   << "</Host>";
            }

            NameValueList list;

            list.append( new NameValue( "Count", query.size() ));
            list.append( new NameValue( "Hosts", sHosts       ));

            pRequest->FormatActionResponse( &list );

        }
    }
    else
        UPnp::FormatErrorResponse( pRequest, UPnPResult_ActionFailed, "Database not open while trying to load list of hosts" );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void MythXML::GetKeys( HTTPRequest *pRequest )
{
    pRequest->m_mapRespHeaders[ "Cache-Control" ] = "no-cache=\"Ext\", max-age = 5000";

    MSqlQuery query(MSqlQuery::InitCon());

    if (query.isConnected())
    {
        query.prepare("SELECT DISTINCTROW value FROM settings;" );
        query.exec();

        if (query.isActive() && query.size() > 0)
        {
             QString     sKeys;
             QTextStream os( &sKeys, IO_WriteOnly );

            while(query.next())
            {
                QString sKey = query.value(0).toString();

                os << "<Key>" 
                   << HTTPRequest::Encode( sKey )
                   << "</Key>";
            }

            NameValueList list;

            list.append( new NameValue( "Count", query.size() ));
            list.append( new NameValue( "Keys" , sKeys        ));

            pRequest->FormatActionResponse( &list );
        }
    }
    else
        UPnp::FormatErrorResponse( pRequest, 
                                   UPnPResult_ActionFailed, 
                                   QString("Database not open while trying to load setting: %1")
                                      .arg( pRequest->m_mapParams[ "Key" ] ));
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void MythXML::GetSetting( HTTPRequest *pRequest )
{
    pRequest->m_mapRespHeaders[ "Cache-Control" ] = "no-cache=\"Ext\", max-age = 5000";

    QString sKey      = pRequest->m_mapParams[ "Key"      ];
    QString sHostName = pRequest->m_mapParams[ "HostName" ];
    QString sValue;

    MSqlQuery query(MSqlQuery::InitCon());

    if (query.isConnected())
    {
        NameValueList list;

        // Was a Key Supplied?

        QString       sXml;
        QTextStream   os( &sXml, IO_WriteOnly );

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

                    os <<  "<Value key='" 
                       << HTTPRequest::Encode( sKey   )
                       << "'>"
                       << HTTPRequest::Encode( sValue )
                       << "</Value>";

                    list.append( new NameValue( "Count"   , 1         ));
                    list.append( new NameValue( "HostName", sHostName ));
                    list.append( new NameValue( "Values", sXml        ));

                    pRequest->FormatActionResponse( &list );

                    return;
                }
            }

        }
        else
        {

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

                while(query.next())
                {
                    sKey      = query.value(0).toString();
                    sValue    = query.value(1).toString();

                    os <<  "<Value key='" 
                       << HTTPRequest::Encode( sKey   )
                       << "'>"
                       << HTTPRequest::Encode( sValue )
                       << "</Value>";
                }


                list.append( new NameValue( "Count"   , query.size()));
                list.append( new NameValue( "HostName", sHostName   ));
                list.append( new NameValue( "Values"  , sXml        ));

                pRequest->FormatActionResponse( &list );

                return;
            }

        }

        // Not found, so return the supplied default value

        os <<  "<Value key='" 
           << HTTPRequest::Encode( sKey   )
           << "'>"
           << HTTPRequest::Encode( pRequest->m_mapParams[ "Default" ] )
           << "</Value>";


        list.append( new NameValue( "Count"   , 1        ));
        list.append( new NameValue( "HostName", sHostName));
        list.append( new NameValue( "Values", sXml       ));

        pRequest->FormatActionResponse( &list );
    }
    else
        UPnp::FormatErrorResponse( pRequest, 
                                   UPnPResult_ActionFailed, 
                                   QString("Database not open while trying to load setting: %1")
                                      .arg( pRequest->m_mapParams[ "Key" ] ));
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void MythXML::PutSetting( HTTPRequest *pRequest )
{
    pRequest->m_mapRespHeaders[ "Cache-Control" ] = "no-cache=\"Ext\", max-age = 5000";

    QString sHostName = pRequest->m_mapParams[ "HostName" ];
    QString sKey      = pRequest->m_mapParams[ "Key"      ];
    QString sValue    = pRequest->m_mapParams[ "Value"    ];

    if (sKey.length() > 0)
    {
        NameValueList list;

        gContext->SaveSettingOnHost( sKey, sValue, sHostName );

        list.append( new NameValue( "Result", "True" ));

        pRequest->FormatActionResponse( &list );
    }
    else
        UPnp::FormatErrorResponse( pRequest, UPnPResult_InvalidArgs, "Key Required" );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void MythXML::GetProgramGuide( HTTPRequest *pRequest )
{
    pRequest->m_mapRespHeaders[ "Cache-Control" ] = "no-cache=\"Ext\", max-age = 5000";

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
        UPnp::FormatErrorResponse( pRequest, UPnPResult_ArgumentValueInvalid, "StartTime is invalid" );
        return;
    }
        
    if (!dtEnd.isValid()) 
    { 
        UPnp::FormatErrorResponse( pRequest, UPnPResult_ArgumentValueInvalid, "EndTime is invalid" );
        return;
    }

    if (dtEnd < dtStart) 
    { 
        UPnp::FormatErrorResponse( pRequest, UPnPResult_ArgumentValueInvalid, "EndTime is before StartTime");
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

    // Build Response

    QDomDocument doc;                        

    QDomElement channels = doc.createElement("Channels");
    doc.appendChild( channels );

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

            FillChannelInfo( channel, pInfo, bDetails );
        }

        FillProgramInfo( &doc, channel, pInfo, false, bDetails );

        pInfo = progList.next();

    }

    // ----------------------------------------------------------------------

    NameValueList list;

    list.append( new NameValue( "StartTime"    , sStartTime   ));
    list.append( new NameValue( "EndTime"      , sEndTime     ));
    list.append( new NameValue( "StartChanId"  , iStartChanId ));
    list.append( new NameValue( "EndChanId"    , iEndChanId   ));
    list.append( new NameValue( "NumOfChannels", iChanCount   ));
    list.append( new NameValue( "Details"      , bDetails     ));

    list.append( new NameValue( "Count"        , (int)progList.count() ));
    list.append( new NameValue( "AsOf"         , QDateTime::currentDateTime().toString( Qt::ISODate )));
    list.append( new NameValue( "Version"      , MYTH_BINARY_VERSION ));
    list.append( new NameValue( "ProtoVer"     , MYTH_PROTO_VERSION  ));
    list.append( new NameValue( "ProgramGuide" , doc.toString()      ));

    pRequest->FormatActionResponse( &list );

}


/////////////////////////////////////////////////////////////////////////////
//                  
/////////////////////////////////////////////////////////////////////////////

void MythXML::GetProgramDetails( HTTPRequest *pRequest )
{
    pRequest->m_mapRespHeaders[ "Cache-Control" ] = "no-cache=\"Ext\", max-age = 5000";

    QString sStartTime = pRequest->m_mapParams[ "StartTime" ];
    QString sChanId    = pRequest->m_mapParams[ "ChanId"    ];

    QDateTime dtStart = QDateTime::fromString( sStartTime, Qt::ISODate );

    if (!dtStart.isValid()) 
    { 
        UPnp::FormatErrorResponse( pRequest, UPnPResult_ArgumentValueInvalid, "StartTime is invalid" );
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
                 itRecList != recList.end();   itRecList++)
    {
        schedList.append( *itRecList );
    }

    // ----------------------------------------------------------------------

    ProgramList progList;

    progList.FromProgram( sSQL, bindings, schedList );

    ProgramInfo *pInfo = progList.first();

    if (pInfo==NULL)
    { 
        UPnp::FormatErrorResponse( pRequest, UPnPResult_ActionFailed, "Error Reading Program Info" );
        return;
    }

    // Build Response XML

    QDomDocument doc;

//    QDomElement root = doc.createElement("Programs");
//    doc.appendChild(root);

    FillProgramInfo( &doc, doc, pInfo, true );

    // ----------------------------------------------------------------------

    NameValueList list;

    list.append( new NameValue( "StartTime"    , sStartTime   ));
    list.append( new NameValue( "ChanId"       , sChanId      ));

    list.append( new NameValue( "Count"        , 1  ));
    list.append( new NameValue( "AsOf"         , QDateTime::currentDateTime().toString( Qt::ISODate )));
    list.append( new NameValue( "Version"      , MYTH_BINARY_VERSION ));
    list.append( new NameValue( "ProtoVer"     , MYTH_PROTO_VERSION  ));

    list.append( new NameValue( "ProgramDetails" , doc.toString()      ));

    pRequest->FormatActionResponse( &list );
}

/////////////////////////////////////////////////////////////////////////////
//                  
/////////////////////////////////////////////////////////////////////////////

void MythXML::GetChannelIcon( HTTPRequest *pRequest )
{
    bool bDefaultPixmap = false;

    pRequest->m_eResponseType   = ResponseTypeFile;

    int  iChanId   = pRequest->m_mapParams[ "ChanId"  ].toInt();

    // Optional Parameters

    int     nWidth    = pRequest->m_mapParams[ "Width"     ].toInt();
    int     nHeight   = pRequest->m_mapParams[ "Height"    ].toInt();

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

    if ((nWidth == 0) && (nHeight == 0))
    {
        bDefaultPixmap = true;
    }

    QString sFileName;

    if (bDefaultPixmap)
    {
        return;
    }
    else
        sFileName = QString( "%1.%2x%3.png" )
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
        pRequest->m_sFileName = sFileName;
        return;
    }

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

    QImage img = pImage->smoothScale( nWidth, nHeight);

    img.save( sFileName.ascii(), "PNG" );

    delete pImage;

    pRequest->m_sFileName = sFileName;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void MythXML::GetAlbumArt( HTTPRequest *pRequest )
{
    bool bDefaultPixmap = false;

    pRequest->m_eResponseType   = ResponseTypeFile;

    int  iId   = pRequest->m_mapParams[ "Id"  ].toInt();

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
    query.bindValue(":ARTID", iId );

    if (!query.exec() || !query.isActive())
        MythContext::DBError("Select ArtId", query);

    QString musicbasepath = gContext->GetSetting("MusicLocation", "");

    if (query.size() > 0)
    {
        query.first();

        pRequest->m_sFileName       = musicbasepath + query.value(0).toString();
    }

    if ((nWidth == 0) && (nHeight == 0))
    {
        bDefaultPixmap = true;
    }

    QString sFileName;

    if (bDefaultPixmap)
    {
        return;
    }
    else
        sFileName = QString( "%1.%2x%3.png" )
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

    QImage img = pImage->smoothScale( nWidth, nHeight);

    img.save( sFileName.ascii(), "PNG" );

    delete pImage;

    pRequest->m_sFileName = sFileName;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void MythXML::GetRecorded( HTTPRequest *pRequest )
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

    QDomDocument doc;

    QDomElement root = doc.createElement("Programs");
    doc.appendChild(root);

    ProgramInfo *pInfo = progList.first();

    while (pInfo != NULL)
    {
        FillProgramInfo( &doc, root, pInfo, true );

        pInfo = progList.next();
    }

    // ----------------------------------------------------------------------

    NameValueList list;

    list.append( new NameValue( "Count"    , (int)progList.count()));
    list.append( new NameValue( "AsOf"     , QDateTime::currentDateTime().toString( Qt::ISODate )));
    list.append( new NameValue( "Version"  , MYTH_BINARY_VERSION ));
    list.append( new NameValue( "ProtoVer" , MYTH_PROTO_VERSION  ));

    list.append( new NameValue( "Recorded" , doc.toString()      ));

    pRequest->FormatActionResponse( &list );

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
    for (; it !=infoList.end(); it++)
    {
        ProgramInfo *pInfo = (*it);

        if (pInfo != NULL)
        {
            FillProgramInfo( &doc, root, pInfo, true );
            delete pInfo;
        }
    }

    // ----------------------------------------------------------------------

    NameValueList list;

    list.append( new NameValue( "Count"    , (int)infoList.size()));
    list.append( new NameValue( "AsOf"     , QDateTime::currentDateTime().toString( Qt::ISODate )));
    list.append( new NameValue( "Version"  , MYTH_BINARY_VERSION ));
    list.append( new NameValue( "ProtoVer" , MYTH_PROTO_VERSION  ));

    list.append( new NameValue( "Expiring" , doc.toString()      ));

    pRequest->FormatActionResponse( &list );

}

/////////////////////////////////////////////////////////////////////////////
//                  
/////////////////////////////////////////////////////////////////////////////

void MythXML::GetPreviewImage( HTTPRequest *pRequest )
{
    bool bDefaultPixmap = false;

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

    if ((nWidth == 0) && (nHeight == 0))
    {
        bDefaultPixmap = true;
        nWidth         = gContext->GetNumSetting("PreviewPixmapWidth", 160);
        nHeight        = gContext->GetNumSetting("PreviewPixmapHeight", 120);
    }

    // ----------------------------------------------------------------------
    // Determine Time the image should be extracted from
    // ----------------------------------------------------------------------

    if (nSecsIn == 0)
    {
        nSecsIn = gContext->GetNumSetting("PreviewPixmapOffset", 64) +
                  gContext->GetNumSetting("RecordPreRoll",0);
    }

    // ----------------------------------------------------------------------
    // If a specific size/time is requested, don't use cached image.
    // ----------------------------------------------------------------------
    // -=>TODO: should cache custom sized images... 
    //          would need to decide how to delete
    // ----------------------------------------------------------------------

    QString sFileName     = pInfo->GetPlaybackURL();

    if (bDefaultPixmap)
        pRequest->m_sFileName = sFileName + ".png";
    else
        pRequest->m_sFileName = QString( "%1.%2x%3x%4.png" )
                                   .arg( sFileName )
                                   .arg( nWidth    )
                                   .arg( nHeight   )
                                   .arg( nSecsIn   );

    // ----------------------------------------------------------------------
    // check to see if preview image is already created.
    // ----------------------------------------------------------------------

    if (QFile::exists( pRequest->m_sFileName ))
    {
        pRequest->m_eResponseType   = ResponseTypeFile;
        pRequest->m_nResponseStatus = 200;
        return;
    }

    // ------------------------------------------------------------------
    // Must generate Preview Image, Generate Image and save.
    // ------------------------------------------------------------------

    float fAspect = 0.0;

    QImage *pImage = GeneratePreviewImage( pInfo, 
                                           sFileName, 
                                           nSecsIn, 
                                           fAspect );

    if (pImage == NULL)
    {
        delete pInfo;
        return;
    }

    // ------------------------------------------------------------------

    if (bDefaultPixmap)
    {

       if (fAspect <= 0)
           fAspect = (float)(nWidth) / nHeight;

       if (fAspect > nWidth / nHeight)
           nHeight = (int)rint(nWidth / fAspect);
       else
           nWidth = (int)rint(nHeight * fAspect);
    }
    else
    {
        if ( nWidth == 0 )
            nWidth = (int)rint(nHeight * fAspect);

        if ( nHeight == 0 )
            nHeight = (int)rint(nWidth / fAspect);

        /*
        QByteArray aBytes;
        QBuffer    buffer( aBytes );

        buffer.open( IO_WriteOnly );
        img.save( &buffer, "PNG" );

        pRequest->m_eResponseType     = ResponseTypeOther;
        pRequest->m_sResponseTypeText = pRequest->GetMimeType( "png" );
        pRequest->m_nResponseStatus   = 200;

        pRequest->m_response.writeRawBytes( aBytes.data(), aBytes.size() );
        */
    }

    QImage img = pImage->smoothScale( nWidth, nHeight);

    img.save( pRequest->m_sFileName.ascii(), "PNG" );

    delete pImage;

    if (pInfo)
        delete pInfo;

    pRequest->m_eResponseType   = ResponseTypeFile;
    pRequest->m_nResponseStatus = 200;
}

/////////////////////////////////////////////////////////////////////////////
//                  
/////////////////////////////////////////////////////////////////////////////

QImage *MythXML::GeneratePreviewImage( ProgramInfo   *pInfo,
                                       const QString &sFileName,
                                       int            nSecsIn,
                                       float         &fAspect )
{

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
        return NULL;

    // ------------------------------------------------------------------
    // Generate Preview Image and save.
    // ------------------------------------------------------------------

    int   nLen    = 0, nWidth = 0, nHeight = 0;

    unsigned char *pData = (unsigned char *)pEncoder->GetScreenGrab( pInfo, 
                                                                     sFileName, 
                                                                     nSecsIn,
                                                                     nLen, 
                                                                     nWidth,
                                                                     nHeight, 
                                                                     fAspect);
    if (!pData)
        return NULL;

    return new QImage( pData, nWidth, nHeight, 32, NULL, 65536 * 65536, QImage::LittleEndian );
}

/////////////////////////////////////////////////////////////////////////////
//                  
/////////////////////////////////////////////////////////////////////////////

void MythXML::GetRecording( HttpWorkerThread *pThread, 
                            HTTPRequest      *pRequest )
{
    bool bIndexFile = false;

    pRequest->m_eResponseType   = ResponseTypeHTML;
    pRequest->m_mapRespHeaders[ "Cache-Control" ] = "no-cache=\"Ext\", max-age = 5000";
    pRequest->m_nResponseStatus = 404;

    QString sChanId   = pRequest->m_mapParams[ "ChanId"    ];
    QString sStartTime= pRequest->m_mapParams[ "StartTime" ];

    if (sStartTime.length() == 0)
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

        ProgramInfo *pInfo = ProgramInfo::GetProgramFromRecorded( sChanId, dtStart );

        if (pInfo==NULL)
        {
            VERBOSE( VB_UPNP, QString( "MythXML::GetRecording - GetProgramFromRecorded( %1, %2 ) returned NULL" )
                                 .arg( sChanId )
                                 .arg( sStartTime ));
            return;
        }

        if ( pInfo->hostname != gContext->GetHostName())
        {
            // We only handle requests for local resources   

            VERBOSE( VB_UPNP, QString( "MythXML::GetRecording - To access this recording, send request to %1." )
                                 .arg( pInfo->hostname ));

            delete pInfo;

            return;     
        }

        pRequest->m_sFileName = pInfo->GetPlaybackURL();

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

void MythXML::GetMusic( HttpWorkerThread *pThread, 
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
        QString sBasePath = gContext->GetSetting( "MusicLocation", "");

        // ------------------------------------------------------------------
        // Load Track's FileName
        // ------------------------------------------------------------------

        MSqlQuery query(MSqlQuery::InitCon());

        if (query.isConnected())
        {
            query.prepare("SELECT music_songs.filename AS filename FROM music_songs "
                           "WHERE music_songs.song_id = :KEY");

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

void MythXML::GetVideo( HttpWorkerThread *pThread,
                        HTTPRequest      *pRequest )
{
    pRequest->m_eResponseType   = ResponseTypeHTML;
    pRequest->m_mapRespHeaders[ "Cache-Control" ] = "no-cache=\"Ext\", max-age = 5000";
    pRequest->m_nResponseStatus = 404;

    QString sId   = pRequest->m_mapParams[ "Id" ];

    if (sId.length() == 0) 
        return;

    // ----------------------------------------------------------------------
    // Check to see if this is another request for the same recording...
    // ----------------------------------------------------------------------

    ThreadData *pData = (ThreadData *)pThread->GetWorkerData();

    if (pData != NULL)
    {
        if ((pData->m_eType == ThreadData::DT_Video) && (pData->m_sVideoID == sId ))
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
            query.prepare("SELECT filename FROM videometadata WHERE intid = :KEY" );
            query.bindValue(":KEY", sId );
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
    pRequest->m_mapRespHeaders[ "Cache-Control" ] = "no-cache=\"Ext\", max-age = 5000";

    QString sPin         = pRequest->m_mapParams[ "Pin" ];
    QString sSecurityPin = gContext->GetSetting( "SecurityPin", "");

    if (( sSecurityPin.length() != 0 ) && ( sPin != sSecurityPin )) 
    {
        UPnp::FormatErrorResponse( pRequest, UPnPResult_ActionNotAuthorized );
        return;
    }

    DatabaseParams params = gContext->GetDatabaseParams();

    // Check for DBHostName of "localhost" and change to public name or IP 

    QString sServerIP = gContext->GetSetting( "BackendServerIP", "localhost" );
    QString sPeerIP   = pRequest->GetPeerAddress();

    if ((params.dbHostName == "localhost") && 
        (sServerIP         != "localhost") &&
        (sServerIP         != sPeerIP    ))
    {
        params.dbHostName = sServerIP;
    }

    QString     sXml;
    QTextStream os( &sXml, IO_WriteOnly );

    os <<  "<Database>";
    os <<   "<Host>"      << params.dbHostName << "</Host>";
//    os <<   "<Port>"      << params.dbPort     << "</Port>";
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
    os <<  "</WOL>";

    NameValueList list;

    list.append( new NameValue( "Info", sXml ));

    pRequest->FormatActionResponse( &list );

    return;
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

    program.setAttribute( "startTime"   , pInfo->startts.toString(Qt::ISODate));
    program.setAttribute( "endTime"     , pInfo->endts.toString(Qt::ISODate));
    program.setAttribute( "title"       , pInfo->title        );
    program.setAttribute( "subTitle"    , pInfo->subtitle     );
    program.setAttribute( "category"    , pInfo->category     );
    program.setAttribute( "catType"     , pInfo->catType      );
    program.setAttribute( "repeat"      , pInfo->repeat       );

    if (bDetails)
    {

        program.setAttribute( "seriesId"    , pInfo->seriesid     );
        program.setAttribute( "programId"   , pInfo->programid    );
        program.setAttribute( "stars"       , pInfo->stars        );
        program.setAttribute( "fileSize"    , longLongToString( pInfo->filesize ));
        program.setAttribute( "lastModified", pInfo->lastmodified.toString(Qt::ISODate) );
        program.setAttribute( "programFlags", pInfo->programflags );
        program.setAttribute( "hostname"    , pInfo->hostname     );

        if (pInfo->hasAirDate)
            program.setAttribute( "airdate"  , pInfo->originalAirDate.toString(Qt::ISODate) );

        QDomText textNode = pDoc->createTextNode( pInfo->description );
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

    if ( pInfo->recstatus != rsUnknown )
    { 
        QDomElement recording = pDoc->createElement( "Recording" );
        program.appendChild( recording );

        recording.setAttribute( "recStatus"     , pInfo->recstatus   );
        recording.setAttribute( "recPriority"   , pInfo->recpriority );
        recording.setAttribute( "recStartTs"    , pInfo->recstartts.toString(Qt::ISODate));
        recording.setAttribute( "recEndTs"      , pInfo->recendts.toString(Qt::ISODate));

        if (bDetails)
        {
            recording.setAttribute( "recordId"      , pInfo->recordid    );
            recording.setAttribute( "recGroup"      , pInfo->recgroup    );
            recording.setAttribute( "playGroup"     , pInfo->playgroup   );
            recording.setAttribute( "recType"       , pInfo->rectype     );
            recording.setAttribute( "dupInType"     , pInfo->dupin       );
            recording.setAttribute( "dupMethod"     , pInfo->dupmethod   );
            recording.setAttribute( "encoderId"     , pInfo->cardid      );
            recording.setAttribute( "recProfile"    , pInfo->GetProgramRecordingProfile());
  //          recording.setAttribute( "preRollSeconds", m_nPreRollSeconds );
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
        QString sHostName = gContext->GetHostName();
        QString sPort     = gContext->GetSettingOnHost( "BackendStatusPort", sHostName);
        QString sIconURL  = QString( "http://%1:%2/getChannelIcon?ChanId=%3" )
                                   .arg( sHostName )
                                   .arg( sPort )
                                   .arg( pInfo->chanid );
*/

        channel.setAttribute( "chanId"     , pInfo->chanid      );
        channel.setAttribute( "chanNum"    , pInfo->chanstr     );
        channel.setAttribute( "callSign"   , pInfo->chansign    );
//        channel.setAttribute( "iconURL"    , sIconURL           );
        channel.setAttribute( "channelName", pInfo->channame    );

        if (bDetails)
        {
            channel.setAttribute( "chanFilters", pInfo->chanOutputFilters );
            channel.setAttribute( "sourceId"   , pInfo->sourceid    );
            channel.setAttribute( "inputId"    , pInfo->inputid     );
            channel.setAttribute( "commFree"   , pInfo->chancommfree);
        }
    }
}

// vim:set shiftwidth=4 tabstop=4 expandtab:
