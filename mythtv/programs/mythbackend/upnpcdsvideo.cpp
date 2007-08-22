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

UPnpCDSRootInfo UPnpCDSVideo::g_RootNodes[] = 
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

int UPnpCDSVideo::g_nRootCount = sizeof( g_RootNodes ) / sizeof( UPnpCDSRootInfo );

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

UPnpCDSRootInfo *UPnpCDSVideo::GetRootInfo( int nIdx )
{ 
    if ((nIdx >=0 ) && ( nIdx < g_nRootCount ))
        return &(g_RootNodes[ nIdx ]); 

    return NULL;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

int UPnpCDSVideo::GetRootCount()
{
    return g_nRootCount;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

QString UPnpCDSVideo::GetTableName( QString sColumn )
{
    if (sColumn == "idgenre"  )   return "videometadatagenre";
    if (sColumn == "idcountry")   return "videometadatacountry";

    return "videometadata";
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

QString UPnpCDSVideo::GetItemListSQL( QString sColumn )
{
    if ( sColumn == "idgenre" )
    {
        return "SELECT intid, title, director, plot, year, " \
                            "userrating, length, filename, coverfile, " \
                            "category, idgenre FROM videometadata " \
                            "LEFT JOIN (videometadatagenre) ON " \
                            "(videometadatagenre.idvideo = videometadata.intid) ";
    }

    if ( sColumn == "idcountry" )
    {
        return  "SELECT intid, title, director, plot, year, " \
                 "userrating, length, filename, coverfile, "   \
                 "category, idcountry FROM videometadata " \
                 "LEFT JOIN (videometadatacountry) ON " \
                 "(videometadatacountry.idvideo = videometadata.intid) ";
    }

    return "SELECT intid, title, director, plot, year, "  \
              "userrating, length, filename, coverfile, " \
              "category FROM videometadata ";
}

/*
#define SHARED_ALLVIDEO_SQL "SELECT intid, title, director, plot, year, " \
                            "userrating, length, filename, coverfile, "   \
                            "category, idgenre, idcountry FROM videometadata " \
                            "LEFT JOIN (videometadatagenre) ON " \
                            "(videometadatagenre.idvideo = videometadata.intid) " \
                            "LEFT JOIN (videometadatacountry) ON " \
                            "(videometadatacountry.idvideo = videometadata.intid) " 
*/

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void UPnpCDSVideo::BuildItemQuery( MSqlQuery &query, const QStringMap &mapParams )
{
    int nVideoID = mapParams[ "Id" ].toInt();

    QString sSQL = QString( "%1 WHERE intid=:VIDEOID " ).arg( GetItemListSQL( ) );

    query.prepare( sSQL );

    query.bindValue( ":VIDEOID", (int)nVideoID    );
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

        sSQL = GetItemListSQL( "idgenre" );

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

        sSQL = GetItemListSQL( "idcountry" );

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

void UPnpCDSVideo::AddItem( const QString           &sObjectId,
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

    QString sURIBase   = QString( "http://%1:%2/Myth/" )
                            .arg( sServerIp )
                            .arg( sPort     );

    QString sURIParams = QString( "?Id=%1&amp;" )
                            .arg( nVidID );

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
    QString sProtocol = QString( "http-get:*:%1:DLNA.ORG_OP=01;DLNA.ORG_CI=0;DLNA.OR G_FLAGS=01500000000000000000000000000000" ).arg( sMimeType  );
    QString sURI      = QString( "%1GetVideo%2").arg( sURIBase   )
                                                    .arg( sURIParams ); 

    Resource *pRes = pItem->AddResource( sProtocol, sURI );

   // Wonder if we should add the filesize to videometadata. would make this cleaner since we 
   // are hitting the db for everything else.

    pRes->AddAttribute( "size"      , QString("%1").arg(fInfo.size()) );

}

