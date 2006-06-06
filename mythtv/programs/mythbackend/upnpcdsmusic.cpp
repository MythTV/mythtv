//////////////////////////////////////////////////////////////////////////////
// Program Name: upnpcdsmusic.cpp
//                                                                            
// Purpose - uPnp Content Directory Extention for Music  
//                                                                            
// Created By  : David Blain                    Created On : Jan. 24, 2005
// Modified By :                                Modified On:                  
//                                                                            
//////////////////////////////////////////////////////////////////////////////

#include "upnpcdsmusic.h"
#include "httprequest.h"
#include <qfileinfo.h>
#include <limits.h>

/*
   Music                            Music
    - All Music                     Music/All
      + <Track 1>                   Music/All/track?Id=1
      + <Track 2>
      + <Track 3>
    - PlayLists
    - By Artist                     Music/artist
      - <Artist 1>                  Music/artist/artistKey=Pink Floyd
        - <Album 1>                 Music/artist/artistKey=Pink Floyd/album/albumKey=The Wall
          + <Track 1>               Music/artist/artistKey=Pink Floyd/album/albumKey=The Wall/track?Id=1
          + <Track 2>
    - By Album                     
      - <Album 1>                  
        + <Track 1>              
        + <Track 2>
    - By Recently Added
      + <Track 1>
      + <Track 2>
    - By Genre
      - By Artist                   Music/artist
        - <Artist 1>                Music/artist/artistKey=Pink Floyd
          - <Album 1>               Music/artist/artistKey=Pink Floyd/album/albumKey=The Wall
            + <Track 1>             Music/artist/artistKey=Pink Floyd/album/albumKey=The Wall/track?Id=1
            + <Track 2>
*/


typedef struct
{
    char *title;
    char *column;
    char *sql;
    char *where;

} RootInfo;

static RootInfo g_RootNodes[] = 
{
    {   "All Music", 
        "*",
        "SELECT intid as id, "
          "title as name, "
          "1 as children "
            "FROM musicmetadata "
            "%1 "
            "ORDER BY title",
        "" },

    {   "Recently Added",
        "*",
        "SELECT intid id, "
          "title as name, "
          "1 as children "
            "FROM musicmetadata "
            "%1 "
            "ORDER BY title",
        "WHERE (DATEDIFF( CURDATE(), date_added ) <= 30 ) " },

    {   "By Album", 
        "album", 
        "SELECT album as id, "
          "album as name, "
          "count( album ) as children "
            "FROM musicmetadata "
            "%1 "
            "GROUP BY album "
            "ORDER BY album",
        "WHERE album=:KEY" },

/*

    {   "By Artist", 
        "artist",
        "SELECT artist as id, "
          "artist as name, "
          "count( distinct album ) as children "
            "FROM musicmetadata "
            "%1 "
            "GROUP BY artist "
            "ORDER BY artist", 
        "WHERE artist=:KEY" },
*/
/*
    {   "By Genre",
        "genre",
        "SELECT genre as id, "
          "genre as name, "
          "count( distinct artist ) as children "
            "FROM musicmetadata "
            "%1 "
            "GROUP BY genre "
            "ORDER BY genre",
        "WHERE genre=:KEY" },

*/
};

static const short g_nRootNodeLength = sizeof( g_RootNodes ) / sizeof( RootInfo );

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

#define SHARED_MUSIC_SQL "SELECT intid, artist, album, title, "          \
                                "genre, year, tracknum, description, "   \
                                "filename, length "                      \
                            "FROM musicmetadata "

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

template <class T> inline const T& Min( const T &x, const T &y ) 
{
    return( ( x < y ) ? x : y );
}
 
template <class T> inline const T& Max( const T &x, const T &y ) 
{
    return( ( x > y ) ? x : y );
}
                                                         
/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

UPnpCDSExtensionResults *UPnpCDSMusic::Browse( UPnpCDSBrowseRequest *pRequest )
{

    // -=>TODO: Need to add Filter & Sorting Support.
    // -=>TODO: Need to add Sub-Folder/Category Support!!!!!

    if (! pRequest->m_sObjectId.startsWith( m_sExtensionId, true ))
        return( NULL );

    // ----------------------------------------------------------------------
    // Parse out request object's path
    // ----------------------------------------------------------------------

    QStringList idPath = QStringList::split( "/", pRequest->m_sObjectId );

    if (idPath.count() == 0)
        return( NULL );

    // ----------------------------------------------------------------------
    // Process based on location in hierarchy
    // ----------------------------------------------------------------------

    UPnpCDSExtensionResults *pResults = new UPnpCDSExtensionResults();

    if (pResults != NULL)
    {
        QString sLast = idPath.last();

        pRequest->m_sParentId = RemoveToken( "/", pRequest->m_sObjectId, 1 );

        if (sLast == m_sExtensionId          ) { return( ProcessRoot ( pRequest, pResults, idPath    )); }
        if (sLast == "0"                     ) { return( ProcessAll  ( pRequest, pResults, idPath, 0 )); }
        if (sLast == "1"                     ) { return( ProcessAll  ( pRequest, pResults, idPath, 1 )); }
        if (sLast.startsWith( "key"  , true )) { return( ProcessKey  ( pRequest, pResults, idPath    )); }
        if (sLast.startsWith( "track", true )) { return( ProcessTrack( pRequest, pResults, idPath    )); }

        int nNodeIdx = sLast.toInt();

        if ((nNodeIdx > 1) && (nNodeIdx < g_nRootNodeLength))
            return( ProcessContainer( pRequest, pResults, nNodeIdx, idPath ));

        delete pResults;
    }

    return( NULL );        

}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

QString UPnpCDSMusic::RemoveToken( const QString &sToken, const QString &sStr, int num )
{
    QString sResult( "" );
    int     nPos = -1;

    for (int nIdx=0; nIdx < num; nIdx++)
    {
        if ((nPos = sStr.findRev( sToken, nPos )) == -1)
            break;
    }

    if (nPos > 0)
        sResult = sStr.left( nPos );

    return sResult;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

UPnpCDSExtensionResults *UPnpCDSMusic::ProcessRoot( UPnpCDSBrowseRequest    *pRequest, 
                                                    UPnpCDSExtensionResults *pResults, 
                                                    QStringList             &/*idPath*/ )
{


    switch( pRequest->m_eBrowseFlag )
    { 
        case CDS_BrowseMetadata:
        {
            // --------------------------------------------------------------
            // Return Root Object Only
            // --------------------------------------------------------------

            pResults->m_nTotalMatches   = 1;
            pResults->m_nUpdateID       = 1;

            CDSObject *pRoot = CDSObject::CreateContainer( m_sExtensionId, 
                                                           m_sName, 
                                                           "0");

            pRoot->SetChildCount( g_nRootNodeLength );

            pResults->Add( pRoot ); 

            break;
        }

        case CDS_BrowseDirectChildren:
        {
            pResults->m_nUpdateID     = 1;
            pResults->m_nTotalMatches = g_nRootNodeLength;
            
            if ( pRequest->m_nRequestedCount == 0)
                pRequest->m_nRequestedCount = g_nRootNodeLength;

            short nStart = Max( pRequest->m_nStartingIndex, short( 0 ));
            short nEnd   = Min( g_nRootNodeLength, short( nStart + pRequest->m_nRequestedCount));

            if (nStart < g_nRootNodeLength)
            {
                for (short nIdx = nStart; nIdx < nEnd; nIdx++)
                {
                    QString sId = QString( "%1/%2" ).arg( pRequest->m_sObjectId   )
                                                    .arg( nIdx );

                    CDSObject *pItem = CDSObject::CreateContainer( sId,  
                                                                   QObject::tr( g_RootNodes[ nIdx ].title ), 
                                                                   pRequest->m_sParentId );

                    pItem->SetChildCount( GetDistinctCount( g_RootNodes[ nIdx ].column ) );

                    pResults->Add( pItem );
                }
            }
        }

        case CDS_BrowseUnknown:
        default:
            break;
    }

    return( pResults );
}


/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

UPnpCDSExtensionResults *UPnpCDSMusic::ProcessAll ( UPnpCDSBrowseRequest    *pRequest,
                                                    UPnpCDSExtensionResults *pResults,
                                                    QStringList             &/*idPath*/,
                                                    int                      nNodeIdx )
{
    // ----------------------------------------------------------------------
    //
    // ----------------------------------------------------------------------
    
    switch( pRequest->m_eBrowseFlag )
    { 
        case CDS_BrowseMetadata:
        {
            // --------------------------------------------------------------
            // Return Container Object Only
            // --------------------------------------------------------------

            pResults->m_nTotalMatches   = 1;
            pResults->m_nUpdateID       = 1;

            CDSObject *pItem = CDSObject::CreateContainer( pRequest->m_sObjectId,  
                                                           QObject::tr( g_RootNodes[ nNodeIdx ].title ), 
                                                           m_sExtensionId );
            pItem->SetChildCount( GetDistinctCount( g_RootNodes[ nNodeIdx ].column, 
                                                    g_RootNodes[ nNodeIdx ].where  ) );

            pResults->Add( pItem ); 

            break;
        }

        case CDS_BrowseDirectChildren:
        {

            CreateMusicTracks( pRequest, pResults, nNodeIdx, "", false );

            break;
        }

        case CDS_BrowseUnknown:
        default:
            break;
    }
    

    return( pResults );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

UPnpCDSExtensionResults *UPnpCDSMusic::ProcessTrack( UPnpCDSBrowseRequest    */* pRequest */,
                                                     UPnpCDSExtensionResults *pResults, 
                                                     QStringList             &/* idPath */)
{
    pResults->m_nTotalMatches   = 0;
    pResults->m_nUpdateID       = 1;

/*
    // ----------------------------------------------------------------------
    //
    // ----------------------------------------------------------------------
    
    if ( pRequest->m_eBrowseFlag == CDS_BrowseMetadata )
    {
        // --------------------------------------------------------------
        // Return 1 VideoItem
        // --------------------------------------------------------------

        QStringMap  mapParams;
        QString     sParams = idPath.last().section( '?', 1, 1 );
            
        sParams.replace(QRegExp( "&amp;"), "&" ); 

        HTTPRequest::GetParameters( sParams, mapParams );

        int     nChanId    = mapParams[ "ChanId"    ].toInt();
        QString sStartTime = mapParams[ "StartTime" ];

        MSqlQuery query(MSqlQuery::InitCon());

        if (query.isConnected())                                                           
        {
            query.prepare( SHARED_MUSIC_SQL
                           "WHERE chanid=:CHANID and starttime=:STARTTIME " );

            query.bindValue(":CHANID"   , (int)nChanId    );
            query.bindValue(":STARTTIME", sStartTime );
            query.exec();

            if (query.isActive() && query.size() > 0)
            {
                if ( query.next() )
                {
                    pRequest->m_sObjectId = RemoveToken( "/", pRequest->m_sObjectId, 1 );

                    AddVideoItem( pRequest, pResults, false, query );
                    pResults->m_nTotalMatches = 1;
                }
            }
        }
    }
*/

    return( pResults );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

UPnpCDSExtensionResults *UPnpCDSMusic::ProcessKey( UPnpCDSBrowseRequest    *pRequest,
                                                   UPnpCDSExtensionResults *pResults, 
                                                   QStringList             &idPath )
{
    pResults->m_nTotalMatches   = 0;
    pResults->m_nUpdateID       = 1;

    // ----------------------------------------------------------------------
    //
    // ----------------------------------------------------------------------
    
    QString sKey = idPath.last().section( '=', 1, 1 );

    if (sKey.length() > 0)
    {
        QUrl::decode( sKey );

        int nNodeIdx = idPath[ idPath.count() - 2 ].toInt();

        switch( pRequest->m_eBrowseFlag )
        { 

            case CDS_BrowseMetadata:
            {                                    
                // --------------------------------------------------------------
                // Since Key is not always the title, we need to lookup title.
                // --------------------------------------------------------------

                MSqlQuery query(MSqlQuery::InitCon());

                if (query.isConnected())
                {
                    QString sSQL   = QString( g_RootNodes[ nNodeIdx ].sql )
                                        .arg( g_RootNodes[ nNodeIdx ].where );

                    query.prepare  ( sSQL );
                    query.bindValue( ":KEY", sKey );
                    query.exec();

                    if (query.isActive() && query.size() > 0)
                    {
                        if ( query.next() )
                        {
                            // ----------------------------------------------
                            // Return Container Object Only
                            // ----------------------------------------------

                            pResults->m_nTotalMatches   = 1;
                            pResults->m_nUpdateID       = 1;

                            CDSObject *pItem = CDSObject::CreateContainer( pRequest->m_sObjectId,  
                                                                           query.value(1).toString(), 
                                                                           pRequest->m_sParentId );

                            pItem->SetChildCount( GetDistinctCount( g_RootNodes[ nNodeIdx ].column  ));

                            pResults->Add( pItem ); 
                        }
                    }
                }
                break;
            }

            case CDS_BrowseDirectChildren:
            {
                CreateMusicTracks( pRequest, pResults, nNodeIdx, sKey, true );

                break;
            }

            case CDS_BrowseUnknown:
                default:
                break;

        }
    }
  
    return( pResults );                                                                           
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

UPnpCDSExtensionResults *UPnpCDSMusic::ProcessContainer( UPnpCDSBrowseRequest    *pRequest,
                                                         UPnpCDSExtensionResults *pResults,
                                                         int                      nNodeIdx,
                                                         QStringList             &/*idPath*/ )

{
    pResults->m_nUpdateID     = 1;
    pResults->m_nTotalMatches = 0;

    switch( pRequest->m_eBrowseFlag )
    { 
        case CDS_BrowseMetadata:
        {
            // --------------------------------------------------------------
            // Return Container Object Only
            // --------------------------------------------------------------

            pResults->m_nTotalMatches   = 1;
            pResults->m_nUpdateID       = 1;

            CDSObject *pItem = CDSObject::CreateContainer( pRequest->m_sObjectId,  
                                                           QObject::tr( g_RootNodes[ nNodeIdx ].title ), 
                                                           m_sExtensionId );

            pItem->SetChildCount( GetDistinctCount( g_RootNodes[ nNodeIdx ].column ));

            pResults->Add( pItem ); 

            break;
        }

        case CDS_BrowseDirectChildren:
        {
            pResults->m_nTotalMatches = GetDistinctCount( g_RootNodes[ nNodeIdx ].column );
            pResults->m_nUpdateID     = 1;

            if (pRequest->m_nRequestedCount == 0) 
                pRequest->m_nRequestedCount = SHRT_MAX;

            MSqlQuery query(MSqlQuery::InitCon());

            if (query.isConnected())
            {
                // Remove where clause placeholder.

                QString sSQL = g_RootNodes[ nNodeIdx ].sql;
                sSQL.replace( "%1", "" );

                sSQL += QString( " LIMIT %2, %3" )
                           .arg( pRequest->m_nStartingIndex  )
                           .arg( pRequest->m_nRequestedCount );

                query.prepare( sSQL );
                query.exec();

                if (query.isActive() && query.size() > 0)
                {

                    while(query.next())
                    {
                        QString sKey   = query.value(0).toString();
                        QString sTitle = query.value(1).toString();
                        long    nCount = query.value(2).toInt();

                        if (sTitle.length() == 0)
                            sTitle = "(undefined)";

                        QString sId = QString( "%1/%2/key=%3" )
                                         .arg( pRequest->m_sParentId )
                                         .arg( nNodeIdx )
                                         .arg( sKey     );

                        CDSObject *pRoot = CDSObject::CreateContainer( sId, sTitle, pRequest->m_sParentId );

                        pRoot->SetChildCount( nCount );

                        pResults->Add( pRoot ); 
                    }
                }
            }

            break;
        }

        case CDS_BrowseUnknown:
        default:
            break;

    }

    return( pResults );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

int UPnpCDSMusic::GetDistinctCount( const QString &sColumn, const QString &sWhere  )
{
    int nCount = 0;

    MSqlQuery query(MSqlQuery::InitCon());

    if (query.isConnected())
    {
        // Note: Tried to use Bind, however it would not allow me to use it
        //       for column & table names

        QString sSQL;
        
        if (sColumn == "*")
            sSQL = QString( "SELECT count( * ) FROM musicmetadata %1" ).arg( sWhere );
        else
            sSQL = QString( "SELECT count( DISTINCT %1 ) FROM musicmetadata" ).arg( sColumn );

        query.prepare( sSQL );
        query.exec();

        if (query.size() > 0)
        {
            query.next();

            nCount = query.value(0).toInt();
        }
    }

    return( nCount );
}


/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

int UPnpCDSMusic::GetCount( int nNodeIdx , const QString &sKey )
{
    int nCount = 0;

    MSqlQuery query(MSqlQuery::InitCon());

    if (query.isConnected())
    {
        // Note: Tried to use Bind, however it would not allow me to use it
        //       for column & table names

        QString sSQL = QString( "SELECT count( %1 ) FROM musicmetadata %2" )
                          .arg( g_RootNodes[ nNodeIdx ].column )
                          .arg( g_RootNodes[ nNodeIdx ].where );

        query.prepare( sSQL );
        query.bindValue( ":KEY", sKey );
        query.exec();

        if (query.size() > 0)
        {
            query.next();

            nCount = query.value(0).toInt();
        }

    }

    return( nCount );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void UPnpCDSMusic::CreateMusicTracks( UPnpCDSBrowseRequest    *pRequest,
                                      UPnpCDSExtensionResults *pResults,
                                      int                      nNodeIdx,
                                      const QString           &sKey, 
                                      bool                     bAddRef )
{

    pResults->m_nTotalMatches = GetCount( nNodeIdx, sKey );
    pResults->m_nUpdateID     = 1;

    if (pRequest->m_nRequestedCount == 0) 
        pRequest->m_nRequestedCount = SHRT_MAX;

    MSqlQuery query(MSqlQuery::InitCon());

    if (query.isConnected())
    {
        QString sSQL = QString( SHARED_MUSIC_SQL
                                "%1 LIMIT %2, %3" )
                          .arg( g_RootNodes[ nNodeIdx ].where )
                          .arg( pRequest->m_nStartingIndex    )
                          .arg( pRequest->m_nRequestedCount   );

        query.prepare  ( sSQL );
        query.bindValue(":KEY", sKey );
        query.exec();

        if (query.isActive() && query.size() > 0)
        {
//            pResults->m_nTotalMatches = query.size();

            while(query.next())
                AddMusicTrack( pRequest, pResults, bAddRef, nNodeIdx, query );
        }
    }
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void UPnpCDSMusic::AddMusicTrack( UPnpCDSBrowseRequest    *pRequest,
                                  UPnpCDSExtensionResults *pResults,
                                  bool                     bAddRef,
                                  int                      /*nNodeIdx*/,
                                  MSqlQuery               &query )
{
    QString        sName;

    int            nId          = query.value( 0).toInt();
    QString        sArtist      = query.value( 1).toString();
    QString        sAlbum       = query.value( 2).toString();
    QString        sTitle       = query.value( 3).toString();
    QString        sGenre       = query.value( 4).toString();
//    int            nYear        = query.value( 5).toInt();
    int            nTrackNum    = query.value( 6).toInt();
    QString        sDescription = query.value( 7).toString();
    QString        sFileName    = query.value( 8).toString();
    uint           nLength      = query.value( 9).toInt();

/*
    if ((nNodeIdx == 0) || (nNodeIdx == 1))
    {
        sName = QString( "%1-%2:%3" )
                   .arg( sArtist)
                   .arg( sAlbum )
                   .arg( sTitle );
    }
    else
*/
        sName = sTitle;

//cout << nId << " " << sName << endl;


    // ----------------------------------------------------------------------
    // Cache Host ip Address & Port
    // ----------------------------------------------------------------------

//    if (!m_mapBackendIp.contains( sHostName ))
//        m_mapBackendIp[ sHostName ] = gContext->GetSettingOnHost( "BackendServerIp", sHostName);
//
//    if (!m_mapBackendPort.contains( sHostName ))
//        m_mapBackendPort[ sHostName ] = gContext->GetSettingOnHost("BackendStatusPort", sHostName);

    QString sServerIp = gContext->GetSetting( "BackendServerIp"  );
    QString sPort     = gContext->GetSetting("BackendStatusPort" );

    // ----------------------------------------------------------------------
    // Build Support Strings
    // ----------------------------------------------------------------------

    QString sURIBase   = QString( "http://%1:%2/" )
                            .arg( sServerIp ) 
                            .arg( sPort     );

    QString sURIParams = QString( "?Id=%1" )
                            .arg( nId );


    QString sId        = QString( "%1/track%2")
                            .arg( pRequest->m_sObjectId )
                            .arg( sURIParams );

    CDSObject *pItem   = CDSObject::CreateMusicTrack( sId, 
                                                      sName, 
                                                      pRequest->m_sParentId );
    pItem->m_bRestricted  = true;
    pItem->m_bSearchable  = true;
    pItem->m_sWriteStatus = "NOT_WRITABLE";

    if ( bAddRef )
    {
        QString sRefId = QString( "%1/0/track%2")
                            .arg( m_sExtensionId )
                            .arg( sURIParams     );

        pItem->SetPropValue( "refID", sRefId );
    }

    pItem->SetPropValue( "genre"                , sGenre      );
    pItem->SetPropValue( "description"          , sTitle      );
    pItem->SetPropValue( "longDescription"      , sDescription);

    pItem->SetPropValue( "artist"               ,  sArtist    );
    pItem->SetPropValue( "album"                ,  sAlbum     );
    pItem->SetPropValue( "originalTrackNumber"  ,  QString::number( nTrackNum  ));

/*

    pObject->AddProperty( new Property( "publisher"       , "dc"   ));
    pObject->AddProperty( new Property( "language"        , "dc"   ));
    pObject->AddProperty( new Property( "relation"        , "dc"   ));
    pObject->AddProperty( new Property( "rights"          , "dc"   ));


    pObject->AddProperty( new Property( "playlist"            , "upnp" ));
    pObject->AddProperty( new Property( "storageMedium"       , "upnp" ));
    pObject->AddProperty( new Property( "contributor"         , "dc"   ));
    pObject->AddProperty( new Property( "date"                , "dc"   ));

*/


    pResults->Add( pItem );

    // ----------------------------------------------------------------------
    // Add Music Resource Element based on File extension (HTTP)
    // ----------------------------------------------------------------------
    
    QFileInfo fInfo( sFileName );

    QString sMimeType = HTTPRequest::GetMimeType( fInfo.extension( FALSE ));
    QString sProtocol = QString( "http-get:*:%1:*" ).arg( sMimeType  );
    QString sURI      = QString( "%1getMusic%2").arg( sURIBase   )
                                                .arg( sURIParams ); 

    Resource *pRes = pItem->AddResource( sProtocol, sURI );

    nLength /= 1000;

    QString sDur = QString( "0:%1:%2" )
                    .arg( (nLength / 60) % 60, 2 )
                    .arg( (nLength % 60)     , 2 );

    pRes->AddAttribute( "duration"  , sDur      );
}

