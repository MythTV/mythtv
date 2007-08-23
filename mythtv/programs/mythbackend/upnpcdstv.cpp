//////////////////////////////////////////////////////////////////////////////
// Program Name: upnpcdstv.cpp
//                                                                            
// Purpose - uPnp Content Directory Extention for Recorded TV  
//                                                                            
// Created By  : David Blain                    Created On : Jan. 24, 2005
// Modified By :                                Modified On:                  
//                                                                            
//////////////////////////////////////////////////////////////////////////////

#include "upnpcdstv.h"
#include "httprequest.h"
#include <qfileinfo.h>
#include <qregexp.h>
#include <qurl.h>
#include <limits.h>
#include "util.h"


/*
   Recordings                              RecTv
    - All Programs                         RecTv/All
      + <recording 1>                      RecTv/All/item?ChanId=1004&StartTime=2006-04-06T20:00:00
      + <recording 2>
      + <recording 3>
    - By Title                             RecTv/title
      - <title 1>                          RecTv/title/key=Stargate SG-1
        + <recording 1>                    RecTv/title/key=Stargate SG-1/item?ChanId=1004&StartTime=2006-04-06T20:00:00
        + <recording 2>
    - By Genre
    - By Date
    - By Channel
    - By Group
*/


UPnpCDSRootInfo UPnpCDSTv::g_RootNodes[] = 
{
    {   "All Recordings", 
        "*",
        "SELECT 0 as key, "
          "CONCAT( title, ': ', subtitle) as name, "
          "1 as children "
            "FROM recorded "
            "%1 "
            "ORDER BY starttime DESC",
        "" },

    {   "By Title", 
        "title",
        "SELECT title as id, "
          "title as name, "
          "count( title ) as children "
            "FROM recorded "
            "%1 "
            "GROUP BY title "
            "ORDER BY title", 
        "WHERE title=:KEY" },

    {   "By Genre", 
        "category", 
        "SELECT category as id, "
          "category as name, "
          "count( category ) as children "
            "FROM recorded "
            "%1 "
            "GROUP BY category "
            "ORDER BY category",
        "WHERE category=:KEY" },

    {   "By Date",
        "DATE_FORMAT(starttime, '%Y-%m-%d')",
        "SELECT  DATE_FORMAT(starttime, '%Y-%m-%d') as id, "
          "DATE_FORMAT(starttime, '%Y-%m-%d %W') as name, "
          "count( DATE_FORMAT(starttime, '%Y-%m-%d %W') ) as children "
            "FROM recorded "
            "%1 "
            "GROUP BY name "
            "ORDER BY starttime DESC",
        "WHERE DATE_FORMAT(starttime, '%Y-%m-%d') =:KEY" },

    {   "By Channel", 
        "chanid", 
        "SELECT channel.chanid as id, "
          "CONCAT(channel.channum, ' ', channel.callsign) as name, "
          "count( channum ) as children "
            "FROM channel "
                "INNER JOIN recorded ON channel.chanid = recorded.chanid "
            "%1 "
            "GROUP BY name "
            "ORDER BY channel.chanid",
        "WHERE channel.chanid=:KEY" },


    {   "By Group", 
        "recgroup", 
        "SELECT recgroup as id, "
          "recgroup as name, count( recgroup ) as children "
            "FROM recorded "
            "%1 "
            "GROUP BY recgroup "
            "ORDER BY recgroup",
        "WHERE recgroup=:KEY" }
};

int UPnpCDSTv::g_nRootCount = sizeof( g_RootNodes ) / sizeof( UPnpCDSRootInfo );

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

UPnpCDSRootInfo *UPnpCDSTv::GetRootInfo( int nIdx )
{ 
    if ((nIdx >=0 ) && ( nIdx < g_nRootCount ))
        return &(g_RootNodes[ nIdx ]); 

    return NULL;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

int UPnpCDSTv::GetRootCount()
{
    return g_nRootCount;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

QString UPnpCDSTv::GetTableName( QString /* sColumn */)
{
    return "recorded";
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

QString UPnpCDSTv::GetItemListSQL( QString /* sColumn */ )
{
    return "SELECT chanid, starttime, endtime, title, " \
                  "subtitle, description, category, "   \
                  "hostname, recgroup, filesize, "      \
                  "basename, progstart, progend "       \
           "FROM recorded ";
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void UPnpCDSTv::BuildItemQuery( MSqlQuery &query, const QStringMap &mapParams )
{
    int     nChanId    = mapParams[ "ChanId"    ].toInt();
    QString sStartTime = mapParams[ "StartTime" ];

    QString sSQL = QString( "%1 WHERE chanid=:CHANID and starttime=:STARTTIME " )
                      .arg( GetItemListSQL() );

    query.prepare( sSQL );

    query.bindValue(":CHANID"   , (int)nChanId    );
    query.bindValue(":STARTTIME", sStartTime );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void UPnpCDSTv::AddItem( const QString           &sObjectId,
                         UPnpCDSExtensionResults *pResults,
                         bool                     bAddRef,
                         MSqlQuery               &query )
{
    int            nChanid      = query.value( 0).toInt();
    QDateTime      dtStartTime  = query.value( 1).toDateTime();
    QDateTime      dtEndTime    = query.value( 2).toDateTime();
    QString        sTitle       = query.value( 3).toString();
    QString        sSubtitle    = query.value( 4).toString();
    QString        sDescription = query.value( 5).toString();
    QString        sCategory    = query.value( 6).toString();
    QString        sHostName    = query.value( 7).toString();
    QString        sRecGroup    = query.value( 8).toString();
    long long      nFileSize    = stringToLongLong( query.value( 9).toString() );
    QString        sBaseName    = query.value(10).toString();

    QDateTime      dtProgStart  = query.value(11).toDateTime();
    QDateTime      dtProgEnd    = query.value(12).toDateTime();

    // ----------------------------------------------------------------------
    // Cache Host ip Address & Port
    // ----------------------------------------------------------------------

    if (!m_mapBackendIp.contains( sHostName ))
        m_mapBackendIp[ sHostName ] = gContext->GetSettingOnHost( "BackendServerIp", sHostName);

    if (!m_mapBackendPort.contains( sHostName ))
        m_mapBackendPort[ sHostName ] = gContext->GetSettingOnHost("BackendStatusPort", sHostName);

    // ----------------------------------------------------------------------
    // Build Support Strings
    // ----------------------------------------------------------------------

    QString sName      = sTitle + ": " + sSubtitle;

    QString sURIBase   = QString( "http://%1:%2/Myth/" )
                            .arg( m_mapBackendIp  [ sHostName ] ) 
                            .arg( m_mapBackendPort[ sHostName ] );

    QString sURIParams = QString( "?ChanId=%1&amp;StartTime=%2" )
                            .arg( nChanid )
                            .arg( dtStartTime.toString(Qt::ISODate));

    QString sId        = QString( "%1/item%2")
                            .arg( sObjectId )
                            .arg( sURIParams );

    CDSObject *pItem   = CDSObject::CreateVideoItem( sId, 
                                                     sName, 
                                                     sObjectId );
    pItem->m_bRestricted  = false;
    pItem->m_bSearchable  = true;
    pItem->m_sWriteStatus = "WRITABLE";

    if ( bAddRef )
    {
        QString sRefId = QString( "%1/0/item%2")
                            .arg( m_sExtensionId )
                            .arg( sURIParams     );

        pItem->SetPropValue( "refID", sRefId );
    }

    pItem->SetPropValue( "genre"          , sCategory    );
    pItem->SetPropValue( "longDescription", sDescription );
    pItem->SetPropValue( "description"    , sSubtitle    );

    //pItem->SetPropValue( "producer"       , );
    //pItem->SetPropValue( "rating"         , );
    //pItem->SetPropValue( "actor"          , );
    //pItem->SetPropValue( "director"       , );
    //pItem->SetPropValue( "publisher"      , );
    //pItem->SetPropValue( "language"       , );
    //pItem->SetPropValue( "relation"       , );
    //pItem->SetPropValue( "region"         , );

    // ----------------------------------------------------------------------
    // Needed for Microsoft Media Player Compatibility 
    // (Won't display correct Title without them)
    // ----------------------------------------------------------------------

    pItem->SetPropValue( "creator"       , "[Unknown Author]" );
    pItem->SetPropValue( "artist"        , "[Unknown Author]" );
    pItem->SetPropValue( "album"         , "[Unknown Series]" );
    pItem->SetPropValue( "actor"         , "[Unknown Author]" );

    pResults->Add( pItem );

    // ----------------------------------------------------------------------
    // Add Video Resource Element based on File extension (HTTP)
    // ----------------------------------------------------------------------
    
    QFileInfo fInfo( sBaseName );

    QString sMimeType = HTTPRequest::GetMimeType( fInfo.extension( FALSE ));
    // DLNA string below is temp fix for ps3 seeking.
    QString sProtocol = QString( "http-get:*:%1:DLNA.ORG_OP=01;DLNA.ORG_CI=0;DLNA.OR G_FLAGS=01500000000000000000000000000000" ).arg( sMimeType  );
    QString sURI      = QString( "%1GetRecording%2").arg( sURIBase   )
                                                    .arg( sURIParams ); 

    Resource *pRes = pItem->AddResource( sProtocol, sURI );

    uint uiStart = dtProgStart.toTime_t();
    uint uiEnd   = dtProgEnd.toTime_t();
    uint uiDur   = uiEnd - uiStart;


    QString sDur = QString( "%1:%2:%3" )
                    .arg( (uiDur / 3600) % 24, 2 )
                    .arg( (uiDur / 60) % 60  , 2 )
                    .arg(  uiDur % 60        , 2 );

    pRes->AddAttribute( "duration"  , sDur      );
    pRes->AddAttribute( "size"      , longLongToString( nFileSize) );

/*
    // ----------------------------------------------------------------------
    // Add Video Resource Element based on File extension (mythtv)
    // ----------------------------------------------------------------------

    sProtocol = QString( "myth:*:%1:*"     ).arg( sMimeType  );
    sURI      = QString( "myth://%1/%2" )
                   .arg( m_mapBackendIp  [ sHostName ] )
                   .arg( sBaseName );

    pRes = pItem->AddResource( sProtocol, sURI );

    pRes->AddAttribute( "duration"  , sDur      );
    pRes->AddAttribute( "size"      , longLongToString( nFileSize) );
*/
    // ----------------------------------------------------------------------
    // Add Thumbnail Resource
    // ----------------------------------------------------------------------

    //sURI = QString( "%1GetPreviewImage%2").arg( sURIBase   )
    //                                      .arg( sURIParams ); 

    //pItem->AddResource( "http-get:*:image/png:*" , sURI );

}

