//////////////////////////////////////////////////////////////////////////////
// Program Name: upnpcdsvideo.cpp
//                                                                            
// Purpose - uPnp Content Directory Extention for MythVideo Videos
//                                                                            
//////////////////////////////////////////////////////////////////////////////

#include "upnpcdsvideo.h"
#include "httprequest.h"
#include <qfileinfo.h>
#include <qregexp.h>
#include <qurl.h>
#include <limits.h>
#include "util.h"



typedef struct
{
    char *title;
    char *column;
    char *sql;
    char *where;

} RootInfo;

static RootInfo g_RootNodes[] = 
{
    {   "All Videos", 
        "*",
        "SELECT 0 as key, "
          "title as name, "
          "1 as children "
            "FROM videometadata "
            "%1 "
            "ORDER BY title DESC",
        "" },

    {   "By Genre",
        "idgenre",
        "SELECT intid as id, "
          "genre as name, "
          "count( genre ) as children "
            "FROM videogenre "
            "%1 "
            "GROUP BY genre "
            "ORDER BY genre",
        "WHERE genre=:KEY" },

     {   "By Country",
        "idcountry",
        "SELECT intid as id, "
          "country as name, "
          "count( country ) as children "
            "FROM videocountry "
            "%1 "
            "GROUP BY country "
            "ORDER BY country",
        "WHERE country=:KEY" }


};

static const short g_nRootNodeLength = sizeof( g_RootNodes ) / sizeof( RootInfo );

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

// 
#define SHARED_VIDEO_SQL "SELECT intid, title, director, plot, year, " \
                            "userrating, length, filename, coverfile, "   \
                            "category FROM videometadata " 


#define SHARED_ALLVIDEO_SQL "SELECT intid, title, director, plot, year, " \
                            "userrating, length, filename, coverfile, "   \
                            "category, idgenre, idcountry FROM videometadata " \
                            "LEFT JOIN (videometadatagenre) ON " \
                            "(videometadatagenre.idvideo = videometadata.intid) " \
                            "LEFT JOIN (videometadatacountry) ON " \
                            "(videometadatacountry.idvideo = videometadata.intid) " 

#define SHARED_GENRE_SQL "SELECT intid, title, director, plot, year, " \
                            "userrating, length, filename, coverfile, "   \
                            "category, idgenre FROM videometadata " \
                            "LEFT JOIN (videometadatagenre) ON " \
                            "(videometadatagenre.idvideo = videometadata.intid) " 

#define SHARED_COUNTRY_SQL "SELECT intid, title, director, plot, year, " \
                            "userrating, length, filename, coverfile, "   \
                            "category, idcountry FROM videometadata " \
                            "LEFT JOIN (videometadatacountry) ON " \
                            "(videometadatacountry.idvideo = videometadata.intid) " 


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

UPnpCDSExtensionResults *UPnpCDSVideo::Browse( UPnpCDSBrowseRequest *pRequest )
{

    // -=>TODO: Need to add Filter & Sorting Support.
    // -=>TODO: Need to add Sub-Folder/Category Support!!!!!

    if (! pRequest->m_sObjectId.startsWith( m_sExtensionId, true ))
        return( NULL );

    // ----------------------------------------------------------------------
    // Parse out request object's path
    // ----------------------------------------------------------------------

    QStringList idPath = QStringList::split( "/", pRequest->m_sObjectId.section('=',0,0) );

    QString key = pRequest->m_sObjectId.section('=',1);

    if (idPath.count() == 0)
        return( NULL );

    // ----------------------------------------------------------------------
    // Process based on location in hierarchy
    // ----------------------------------------------------------------------

    UPnpCDSExtensionResults *pResults = new UPnpCDSExtensionResults();

    if (pResults != NULL)
    {

        if (key)
            idPath.last().append(QString("=%1").arg(key));

        QString sLast = idPath.last();

        pRequest->m_sParentId = RemoveToken( "/", pRequest->m_sObjectId, 1 );

        if (sLast == m_sExtensionId         ) { return( ProcessRoot   ( pRequest, pResults, idPath )); }
        if (sLast == "0"                    ) { return( ProcessAll    ( pRequest, pResults, idPath )); }
        if (sLast.startsWith( "key" , true )) { return( ProcessKey    ( pRequest, pResults, idPath )); }
        if (sLast.startsWith( "item", true )) { return( ProcessItem   ( pRequest, pResults, idPath )); }

        int nNodeIdx = sLast.toInt();

        if ((nNodeIdx > 0) && (nNodeIdx < g_nRootNodeLength))
            return( ProcessContainer( pRequest, pResults, nNodeIdx, idPath ));

        delete pResults;
    }

    return( NULL );        

}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

QString UPnpCDSVideo::RemoveToken( const QString &sToken, const QString &sStr, int num )
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

UPnpCDSExtensionResults *UPnpCDSVideo::ProcessRoot( UPnpCDSBrowseRequest    *pRequest, 
                                                 UPnpCDSExtensionResults *pResults, 
                                                 QStringList             &/*idPath*/ )
{
    pResults->m_nTotalMatches   = 0;
    pResults->m_nUpdateID       = 1;

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

UPnpCDSExtensionResults *UPnpCDSVideo::ProcessAll ( UPnpCDSBrowseRequest    *pRequest,
                                                 UPnpCDSExtensionResults *pResults,
                                                 QStringList             &/*idPath*/ )
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
                                                           QObject::tr( g_RootNodes[ 0 ].title ), 
                                                           m_sExtensionId );
            pItem->SetChildCount( GetDistinctCount( g_RootNodes[ 0 ].column ) );

            pResults->Add( pItem ); 

            break;
        }

        case CDS_BrowseDirectChildren:
        {

            CreateVideoItems( pRequest, pResults, 0, "", false );

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

UPnpCDSExtensionResults *UPnpCDSVideo::ProcessItem( UPnpCDSBrowseRequest    *pRequest,
                                                 UPnpCDSExtensionResults *pResults, 
                                                 QStringList             &idPath )
{
    pResults->m_nTotalMatches   = 0;
    pResults->m_nUpdateID       = 1;

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

        int     nVideoID    = mapParams[ "VideoID"    ].toInt();

        MSqlQuery query(MSqlQuery::InitCon());

        if (query.isConnected())                                                           
        {
            query.prepare( SHARED_VIDEO_SQL
                           "WHERE intid=:VIDEOID " );

            query.bindValue(":VIDEOID"   , (int)nVideoID    );
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

    return( pResults );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

UPnpCDSExtensionResults *UPnpCDSVideo::ProcessKey( UPnpCDSBrowseRequest    *pRequest,
                                                UPnpCDSExtensionResults *pResults, 
                                                QStringList             &idPath )
{
    pResults->m_nTotalMatches   = 0;
    pResults->m_nUpdateID       = 1;

    // ----------------------------------------------------------------------
    //
    // ----------------------------------------------------------------------
    
    QString sKey = idPath.last().section( '=', 1, 1 );
    QUrl::decode( sKey );

    if (sKey.length() > 0)
    {
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
                CreateVideoItems( pRequest, pResults, nNodeIdx, sKey, true );

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

UPnpCDSExtensionResults *UPnpCDSVideo::ProcessContainer( UPnpCDSBrowseRequest    *pRequest,
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
                                         .arg( sKey );

                        CDSObject *pRoot = CDSObject::CreateContainer( sId, sTitle, pRequest->m_sParentId );

                        pRoot->SetChildCount( nCount );

                        pResults->Add( pRoot ); 
                    }
                }
            }

            break;
        }

        case CDS_BrowseUnknown:
            break;
    }

    return( pResults );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

int UPnpCDSVideo::GetDistinctCount( const QString &sColumn )
{
    int nCount = 0;

    MSqlQuery query(MSqlQuery::InitCon());

    if (query.isConnected())
    {
        // Note: Tried to use Bind, however it would not allow me to use it
        //       for column & table names

        QString sSQL;
        
        if (sColumn == "*")
            sSQL = QString( "SELECT count( %1 ) FROM videometadata" ).arg( sColumn );
        else
        {
            QString sTable;

            if (sColumn == "idgenre")
                sTable = "videometadatagenre";
            else if (sColumn == "idcountry")
                sTable = "videometadatacountry";
            else 
                sTable = "videometadata";


            sSQL = QString( "SELECT count( DISTINCT %1 ) FROM %2" )
                   .arg( sColumn )
                   .arg( sTable );
        }

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

int UPnpCDSVideo::GetCount( const QString &sColumn, const QString &sKey )
{
    int nCount = 0;

    MSqlQuery query(MSqlQuery::InitCon());

    if (query.isConnected())
    {
        // Note: Tried to use Bind, however it would not allow me to use it
        //       for column & table names

        QString sSQL;
        
        if (sColumn == "*")
            sSQL = "SELECT count( * ) FROM videometadata";
        else
        {
            QString sTable;
    
            if (sColumn == "idgenre")
                sTable = "videometadatagenre";
            else if (sColumn == "idcountry")
                sTable = "videometadatacountry";
            else
                sTable = "videometadata";

            sSQL = QString( "SELECT count( %1 ) FROM %2 WHERE %3=:KEY" )
                      .arg( sColumn )
                      .arg( sTable )
                      .arg( sColumn );
        }

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

//  -- TODO -- Going to improve this later, just a rough in right now

void UPnpCDSVideo::FillMetaMaps(void)
{
    MSqlQuery query(MSqlQuery::InitCon());

    if (query.isConnected())
    {   
        QString sSQL = "SELECT intid, genre FROM videogenre";

        query.prepare  ( sSQL );
        query.exec();

        if (query.isActive() && query.size() > 0)
        {   
            while(query.next())
                m_mapGenreNames[query.value(0).toInt()] = query.value(1).toString();
        }

        sSQL = "SELECT intid, country FROM videocountry";

        query.prepare  ( sSQL );
        query.exec();

        if (query.isActive() && query.size() > 0)
        {   
            while(query.next())
                m_mapCountryNames[query.value(0).toInt()] = query.value(1).toString();
        }

        // Forcibly map "0" to "Unknown"
        m_mapGenreNames[0] = "Unknown";
        m_mapCountryNames[0] = "Unknown";


        int nVidID,nGenreID,nCountryID = 0;

        sSQL = SHARED_GENRE_SQL;

        query.prepare  ( sSQL );
        query.exec();

        if (query.isActive() && query.size() > 0)
        {
            while(query.next())
            {   
                nVidID = query.value(0).toInt();               
                nGenreID = query.value(10).toInt();

                if (m_mapGenre.contains(nVidID))
                    m_mapGenre[nVidID] = QString("%1 / %2")
                                             .arg(m_mapGenre[nVidID])
                                             .arg(m_mapGenreNames[nGenreID]);
                else
                    m_mapGenre[nVidID] = m_mapGenreNames[nGenreID];

            }
        }

        sSQL = SHARED_COUNTRY_SQL;

        query.prepare  ( sSQL );
        query.exec();

        if (query.isActive() && query.size() > 0)
        {   
            while(query.next())
            {   
                nVidID = query.value(0).toInt();
                nCountryID = query.value(10).toInt();

                if (m_mapCountry.contains(nVidID))
                    m_mapCountry[nVidID] =  QString("%1 / %2")
                                             .arg(m_mapCountry[nVidID])
                                             .arg(m_mapCountryNames[nCountryID]);
                else
                    m_mapCountry[nVidID] = m_mapCountryNames[nCountryID];

            }
        }



    }
}


/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void UPnpCDSVideo::CreateVideoItems( UPnpCDSBrowseRequest    *pRequest,
                                  UPnpCDSExtensionResults *pResults,
                                  int                      nNodeIdx,
                                  const QString           &sKey, 
                                  bool                     bAddRef )
{
    QString sSQLBase;

    pResults->m_nTotalMatches = GetCount( g_RootNodes[ nNodeIdx ].column, sKey );
    pResults->m_nUpdateID     = 1;

    if (pRequest->m_nRequestedCount == 0) 
        pRequest->m_nRequestedCount = SHRT_MAX;

    MSqlQuery query(MSqlQuery::InitCon());

    if (query.isConnected())
    {
        QString sWhere( "" );

        if ( sKey.length() > 0)
        {
           sWhere = QString( "WHERE %1=:KEY " )
                       .arg( g_RootNodes[ nNodeIdx ].column );
        }

        if ( g_RootNodes[ nNodeIdx ].column == "idgenre" )
            sSQLBase = SHARED_GENRE_SQL;
        else if ( g_RootNodes[ nNodeIdx ].column == "idcountry" )
            sSQLBase = SHARED_COUNTRY_SQL;
        else 
            sSQLBase = SHARED_VIDEO_SQL;
   
        QString sSQL = QString( "%1 " 
                                "%2 "
                                "LIMIT %3, %4" )
                          .arg( sSQLBase )
                          .arg( sWhere )
                          .arg( pRequest->m_nStartingIndex  )
                          .arg( pRequest->m_nRequestedCount );

        query.prepare  ( sSQL );
        query.bindValue(":KEY", sKey );
        query.exec();

        if (query.isActive() && query.size() > 0)
        {
            while(query.next())
                AddVideoItem( pRequest, pResults, bAddRef, query );
        }
    }
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void UPnpCDSVideo::AddVideoItem( UPnpCDSBrowseRequest    *pRequest,
                              UPnpCDSExtensionResults *pResults,
                              bool                     bAddRef,
                              MSqlQuery               &query )
{
    int            nVidID       = query.value( 0).toInt();
    QString        sTitle       = query.value( 1).toString();
    QString        sDirector    = query.value( 2).toString();
    QString        sDescription = query.value( 3).toString();
    QString        sYear        = query.value( 4).toString();
    QString        sUserRating  = query.value( 5).toString();
  //long long      nFileSize    = stringToLongLong( query.value( 6).toString() );
    QString        sFileName    = query.value( 7).toString();
    QString        sCover       = query.value( 8).toString();
  //int            nCategory    = query.value( 9).toInt();

    // This should be a lookup from acache of the videometagenre and such tables.
    QString        sGenre       = m_mapGenre[nVidID];
    QString        sCountry     = m_mapCountry[nVidID];

    // ----------------------------------------------------------------------
    // Cache Host ip Address & Port
    // ----------------------------------------------------------------------
    QString sServerIp = gContext->GetSetting( "BackendServerIp"  );
    QString sPort     = gContext->GetSetting("BackendStatusPort" );

    // ----------------------------------------------------------------------
    // Build Support Strings
    // ----------------------------------------------------------------------

    QString sName      = sTitle;

    QString sURIBase   = QString( "http://%1:%2/" )
                            .arg( sServerIp )
                            .arg( sPort     );

    QString sURIParams = QString( "?VideoID=%1&amp;" )
                            .arg( nVidID );

    QString sId        = QString( "%1/item%2")
                            .arg( pRequest->m_sObjectId )
                            .arg( sURIParams );

    CDSObject *pItem   = CDSObject::CreateVideoItem( sId, 
                                                     sName, 
                                                     pRequest->m_sObjectId );
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

    pItem->SetPropValue( "country"        , sCountry );
    pItem->SetPropValue( "director"       , sDirector );
    pItem->SetPropValue( "year"           , sYear    );
    pItem->SetPropValue( "genre"          , sGenre    );
    //cerr << QString("Setting GENRE : %1 for item (%3) %2").arg(sGenre).arg(sTitle).arg(nVidID) << endl;
    pItem->SetPropValue( "longDescription", sDescription );
    pItem->SetPropValue( "description"    , sDescription    );

    //pItem->SetPropValue( "producer"       , );
    //pItem->SetPropValue( "rating"         , );
    //pItem->SetPropValue( "actor"          , );
    //pItem->SetPropValue( "director"       , );
    //pItem->SetPropValue( "publisher"      , );
    //pItem->SetPropValue( "language"       , );
    //pItem->SetPropValue( "relation"       , );
    //pItem->SetPropValue( "region"         , );

    pResults->Add( pItem );

    // ----------------------------------------------------------------------
    // Add Video Resource Element based on File extension (HTTP)
    // ----------------------------------------------------------------------
    
    QFileInfo fInfo( sFileName );

    QString sMimeType = HTTPRequest::GetMimeType( fInfo.extension( FALSE ));
    QString sProtocol = QString( "http-get:*:%1:*" ).arg( sMimeType  );
    QString sURI      = QString( "%1getVideo%2").arg( sURIBase   )
                                                    .arg( sURIParams ); 

    Resource *pRes = pItem->AddResource( sProtocol, sURI );

   // Wonder if we should add the filesize to videometadata. would make this cleaner since we 
   // are hitting the db for everything else.

    pRes->AddAttribute( "size"      , QString("%1").arg(fInfo.size()) );

}

