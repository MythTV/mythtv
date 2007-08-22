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
      + <Track 1>                   Music/All/item?Id=1
      + <Track 2>
      + <Track 3>
    - PlayLists
    - By Artist                     Music/artist
      - <Artist 1>                  Music/artist/artistKey=Pink Floyd
        - <Album 1>                 Music/artist/artistKey=Pink Floyd/album/albumKey=The Wall
          + <Track 1>               Music/artist/artistKey=Pink Floyd/album/albumKey=The Wall/item?Id=1
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
            + <Track 1>             Music/artist/artistKey=Pink Floyd/album/albumKey=The Wall/item?Id=1
            + <Track 2>
*/

UPnpCDSRootInfo UPnpCDSMusic::g_RootNodes[] = 
{
    {   "All Music",
        "*",
        "SELECT song_id as id, "
          "name, "
          "1 as children "
            "FROM music_songs song "
            "%1 "
            "ORDER BY name",
        "" },

/*
This is currently broken... need to handle list of items with single parent (like 'All Music')

    {   "Recently Added",
        "*",
        "SELECT song_id id, "
          "name, "
          "1 as children "
            "FROM music_songs song "
            "%1 "
            "ORDER BY name",
        "WHERE (DATEDIFF( CURDATE(), date_modified ) <= 30 ) " },
*/
    {   "By Album",
        "song.album_id",
        "SELECT a.album_id as id, "
          "a.album_name as name, "
          "count( song.album_id ) as children "
            "FROM music_songs song join music_albums a on a.album_id = song.album_id "
            "%1 "
            "GROUP BY a.album_id "
            "ORDER BY a.album_name",
        "WHERE song.album_id=:KEY" },
/*

    {   "By Artist",
        "artist_id",
        "SELECT a.artist_id as id, "
          "a.artist_name as name, "
          "count( distinct song.artist_id ) as children "
            "FROM music_songs song join music_artists a on a.artist_id = song.artist_id "
            "%1 "
            "GROUP BY a.artist_id "
            "ORDER BY a.artist_name",
        "WHERE song.artist_id=:KEY" },
*/
/*
{   "By Genre",
        "genre_id",
        "SELECT g.genre_id as id, "
          "genre as name, "
          "count( distinct song.genre_id ) as children "
            "FROM music_songs song join music_genres g on g.genre_id = song.genre_id "
            "%1 "
            "GROUP BY g.genre_id "
            "ORDER BY g.genre",
        "WHERE song.genre_id=:KEY" },

*/
};

int UPnpCDSMusic::g_nRootCount = sizeof( g_RootNodes ) / sizeof( UPnpCDSRootInfo );

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

UPnpCDSRootInfo *UPnpCDSMusic::GetRootInfo( int nIdx )
{ 
    if ((nIdx >=0 ) && ( nIdx < g_nRootCount ))
        return &(g_RootNodes[ nIdx ]); 

    return NULL;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

int UPnpCDSMusic::GetRootCount()
{
    return g_nRootCount;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

QString UPnpCDSMusic::GetTableName( QString sColumn )
{
    return "music_songs song";
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

QString UPnpCDSMusic::GetItemListSQL( QString /* sColumn */ )
{
    return "SELECT song.song_id as intid, artist.artist_name as artist, "         \
               "album.album_name as album, song.name as title, "                  \
               "genre.genre, song.year, song.track as tracknum, "                 \
               "song.description, song.filename, song.length "                    \
            "FROM music_songs song "                                              \
               " join music_artists artist on artist.artist_id = song.artist_id " \
               " join music_albums album on album.album_id = song.album_id "      \
               " join music_genres genre on  genre.genre_id = song.genre_id ";
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void UPnpCDSMusic::BuildItemQuery( MSqlQuery &query, const QStringMap &mapParams )
{
    int     nId    = mapParams[ "Id" ].toInt();

    QString sSQL = QString( "%1 WHERE song.song_id=:ID " )
                      .arg( GetItemListSQL() );

    query.prepare( sSQL );

    query.bindValue(":ID"   , (int)nId    );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void UPnpCDSMusic::AddItem( const QString           &sObjectId,
                            UPnpCDSExtensionResults *pResults,
                            bool                     bAddRef, 
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

    QString sServerIp = gContext->GetSetting( "BackendServerIp"   );
    QString sPort     = gContext->GetSetting( "BackendStatusPort" );

    // ----------------------------------------------------------------------
    // Build Support Strings
    // ----------------------------------------------------------------------

    QString sURIBase   = QString( "http://%1:%2/Myth/" )
                            .arg( sServerIp ) 
                            .arg( sPort     );

    QString sURIParams = QString( "?Id=%1" )
                            .arg( nId );


    QString sId        = QString( "%1/item%2")
                            .arg( sObjectId )
                            .arg( sURIParams );

    CDSObject *pItem   = CDSObject::CreateMusicTrack( sId, 
                                                      sName, 
                                                      sObjectId );
    pItem->m_bRestricted  = true;
    pItem->m_bSearchable  = true;
    pItem->m_sWriteStatus = "NOT_WRITABLE";

    if ( bAddRef )
    {
        QString sRefId = QString( "%1/0/item%2")
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
    QString sProtocol = QString( "http-get:*:%1:DLNA.ORG_OP=01;DLNA.ORG_CI=0;DLNA.OR G_FLAGS=01500000000000000000000000000000" ).arg( sMimeType  );
    QString sURI      = QString( "%1GetMusic%2").arg( sURIBase   )
                                                .arg( sURIParams ); 

    Resource *pRes = pItem->AddResource( sProtocol, sURI );

    nLength /= 1000;

    QString sDur = QString( "0:%1:%2" )
                    .arg( (nLength / 60) % 60, 2 )
                    .arg( (nLength % 60)     , 2 );

    pRes->AddAttribute( "duration"  , sDur      );
}

