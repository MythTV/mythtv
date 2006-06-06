//////////////////////////////////////////////////////////////////////////////
// Program Name: upnpcdsobjects.cpp
//                                                                            
// Purpose - uPnp Content Directory Service Object Definitions
//                                                                            
// Created By  : David Blain                    Created On : Oct. 24, 2005
// Modified By :                                Modified On:                  
//                                                                            
//////////////////////////////////////////////////////////////////////////////

#include "upnpcds.h"
#include <qurl.h>

#include <qtextstream.h>

inline QString GetBool( bool bVal ) { return( (bVal) ? "1" : "0" ); }

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
//
//
//
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

CDSObject::CDSObject( const QString sId, 
                      const QString sTitle, 
                      const QString sParentId )
{
    m_sId          = sId;
    m_sParentId    = sParentId;
    m_eType        = OT_Container;
    m_sTitle       = sTitle;
    m_bRestricted  = true;
    m_sWriteStatus = "PROTECTED";
    m_nChildCount  = 0;
    m_nUpdateId    = 1;

    HTTPRequest::Encode( m_sId       );
    HTTPRequest::Encode( m_sParentId );
    HTTPRequest::Encode( m_sTitle    );
    
    m_properties.setAutoDelete( true );
    m_children  .setAutoDelete( true );
    m_resources .setAutoDelete( true );

}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

CDSObject::~CDSObject()
{

}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

Property *CDSObject::AddProperty( Property *pProp )
{
    if (pProp)
        m_properties.insert( pProp->m_sName, pProp );

    return( pProp );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void CDSObject::SetPropValue( QString sName, QString sValue )
{
    Property *pProp = m_properties[ sName ];

    if (pProp != NULL)
    {
        pProp->m_sValue = sValue;
        HTTPRequest::Encode( pProp->m_sValue );
    }
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

QString CDSObject::GetPropValue( QString sName )
{
    Property *pProp = m_properties[ sName ];

    if (pProp != NULL)
    {
        QString sValue = pProp->m_sValue;
        QUrl::decode( sValue );
        return( sValue );
    }
    
    return( "" );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

CDSObject *CDSObject::AddChild( CDSObject *pChild )
{
    if (pChild)
    {
        pChild->m_sParentId = m_sId;
        m_children.append( pChild );
    }

    return( pChild );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

long CDSObject::GetChildCount()
{
    long nCount = m_children.count();
    if ( nCount == 0)
        return( m_nChildCount );
    
    return( nCount );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

Resource *CDSObject::AddResource( QString sProtocol, QString sURI )
{
    Resource *pRes = new Resource( sProtocol, sURI );
    
    m_resources.append( pRes );

    return( pRes );
}

/////////////////////////////////////////////////////////////////////////////
// Allows the caller to set child count without having to load children.
/////////////////////////////////////////////////////////////////////////////

void CDSObject::SetChildCount( long nCount )
{
    m_nChildCount = nCount;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

QString CDSObject::toXml()// FilterMap &filter )
{
    QString     sXML;
    QTextStream os( sXML, IO_WriteOnly );
    
    os.setEncoding( QTextStream::UnicodeUTF8 );

    toXml( os ); //, filter );

    return( sXML );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void CDSObject::toXml( QTextStream &os ) //, FilterMap &filter )
{
    QString sEndTag = "";
    bool    bFilter = false;

    //  -=>TODO: Need to add Filter Support

//    if (filter.find( "*" ) != filter.end())
//        bFilter = false;

    switch( m_eType )
    {   
        case OT_Container:
        {
            os << "<container id=\"" << m_sId << "\" parentID=\"" << m_sParentId
               << "\" childCount=\"" << GetChildCount() << "\" restricted=\"" << GetBool( m_bRestricted )
               << "\" searchable=\"" << GetBool( m_bSearchable ) << "\" >";

            sEndTag = "</container>";

            break;
        }
        case OT_Item:
        {
            os << "<item id=\"" << m_sId << "\" parentID=\"" << m_sParentId
               << "\" restricted=\"" << GetBool( m_bRestricted ) << "\" >";

            sEndTag = "</item>";

            break;
        }
        default: break;
    }

    os << "<dc:title>"   << m_sTitle << "</dc:title>";
    os << "<upnp:class>" << m_sClass << "</upnp:class>";

    // ----------------------------------------------------------------------
    // Output all Properties
    // ----------------------------------------------------------------------

    for( PropertiesIterator it( m_properties ); it.current(); ++it )
    {
        Property *pProp = it.current();

        if (pProp->m_bRequired || (pProp->m_sValue.length() > 0))
        {
            QString sName;
            
            if (pProp->m_sNameSpace.length() > 0)
                sName = pProp->m_sNameSpace + ":" + pProp->m_sName;
            else
                sName = pProp->m_sName;

            if (pProp->m_bRequired || !bFilter ) //|| ( filter.find( sName ) != filter.end())
            {
                os << "<"  << sName << ">";
                os << pProp->m_sValue;
                os << "</" << sName << ">";
            }
        }
    }

    // ----------------------------------------------------------------------
    // Output any Res Elements
    // ----------------------------------------------------------------------

    for ( Resource *pRes  = m_resources.first(); 
                    pRes != NULL; 
                    pRes  = m_resources.next() )
    {
        os << "<res protocolInfo=\"" << pRes->m_sProtocolInfo << "\" ";

        for (NameValue *pNV  = pRes->m_lstAttributes.first(); 
                        pNV != NULL; 
                        pNV  = pRes->m_lstAttributes.next())
        {                               
            os << pNV->sName << "=\"" << pNV->sValue << "\" ";
        }

        os << ">" << pRes->m_sURI;                                            
        os << "</res>\r\n";
    }

    // ----------------------------------------------------------------------
    // Output any children
    // ----------------------------------------------------------------------

    for ( CDSObject *pObject  = m_children.first(); 
                  pObject != NULL; 
                  pObject = m_children.next() )
    {
        pObject->toXml( os ); //, filter );
    }

    // ----------------------------------------------------------------------
    // Close Element Tag
    // ----------------------------------------------------------------------

    os << sEndTag;
}


/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

CDSObject *CDSObject::CreateItem( QString sId, QString sTitle, QString sParentId, CDSObject *pObject ) 
{
    if (pObject == NULL)
    {
        pObject = new CDSObject( sId, sTitle, sParentId );
        pObject->m_sClass = "object.item";
    }

    pObject->m_eType = OT_Item;

    pObject->AddProperty( new Property( "refID" ) );

    return( pObject );
}

/////////////////////////////////////////////////////////////////////////////

CDSObject *CDSObject::CreateContainer( QString sId, QString sTitle, QString sParentId, CDSObject *pObject )    
{
    if (pObject == NULL)
    {
        pObject = new CDSObject( sId, sTitle, sParentId );
        pObject->m_sClass = "object.container";
    }

    pObject->m_eType = OT_Container;

    pObject->AddProperty( new Property( "childCount"  ));                   
    pObject->AddProperty( new Property( "createClass" ));                   
    pObject->AddProperty( new Property( "searchClass" ));                   
    pObject->AddProperty( new Property( "searchable"  ));                   

    return( pObject );
}

/////////////////////////////////////////////////////////////////////////////

CDSObject *CDSObject::CreateAudioItem( QString sId, QString sTitle, QString sParentId, CDSObject *pObject )
{
    if (pObject == NULL)
    {
        pObject = new CDSObject( sId, sTitle, sParentId );
        pObject->m_sClass = "object.item.audioItem";
    }

    CreateItem( sId, sTitle, sParentId, pObject );

    pObject->AddProperty( new Property( "genre"           , "upnp" ));
    pObject->AddProperty( new Property( "description"     , "dc"   ));
    pObject->AddProperty( new Property( "longDescription" , "upnp" ));
    pObject->AddProperty( new Property( "publisher"       , "dc"   ));
    pObject->AddProperty( new Property( "language"        , "dc"   ));
    pObject->AddProperty( new Property( "relation"        , "dc"   ));
    pObject->AddProperty( new Property( "rights"          , "dc"   ));

    return( pObject );
}

/////////////////////////////////////////////////////////////////////////////

CDSObject *CDSObject::CreateMusicTrack( QString sId, QString sTitle, QString sParentId, CDSObject *pObject )
{
    if (pObject == NULL)
    {
        pObject = new CDSObject( sId, sTitle, sParentId );
        pObject->m_sClass = "object.item.audioItem.musicTrack";
    }

    CreateAudioItem( sId, sTitle, sParentId, pObject );

    pObject->AddProperty( new Property( "artist"              , "upnp" ));
    pObject->AddProperty( new Property( "album"               , "upnp" ));
    pObject->AddProperty( new Property( "originalTrackNumber" , "upnp" ));
    pObject->AddProperty( new Property( "playlist"            , "upnp" ));
    pObject->AddProperty( new Property( "storageMedium"       , "upnp" ));
    pObject->AddProperty( new Property( "contributor"         , "dc"   ));
    pObject->AddProperty( new Property( "date"                , "dc"   ));

    return( pObject );
}

/////////////////////////////////////////////////////////////////////////////

CDSObject *CDSObject::CreateAudioBroadcast( QString sId, QString sTitle, QString sParentId, CDSObject *pObject )
{
    if (pObject == NULL)
    {
        pObject = new CDSObject( sId, sTitle, sParentId );
        pObject->m_sClass = "object.item.audioItem.audioBroadcast";
    }

    CreateAudioItem( sId, sTitle, sParentId, pObject );

    pObject->AddProperty( new Property( "region"         , "upnp" ));
    pObject->AddProperty( new Property( "radioCallSign"  , "upnp" ));
    pObject->AddProperty( new Property( "radioStationID" , "upnp" ));
    pObject->AddProperty( new Property( "radioBand"      , "upnp" ));
    pObject->AddProperty( new Property( "channelNr"      , "upnp" ));

    return( pObject );
}

/////////////////////////////////////////////////////////////////////////////

CDSObject *CDSObject::CreateAudioBook( QString sId, QString sTitle, QString sParentId, CDSObject *pObject )
{
    if (pObject == NULL)
    {
        pObject = new CDSObject( sId, sTitle, sParentId );
        pObject->m_sClass = "object.item.audioItem.audioBook";
    }

    CreateAudioItem( sId, sTitle, sParentId, pObject );

    pObject->AddProperty( new Property( "storageMedium", "upnp" ));
    pObject->AddProperty( new Property( "producer"     , "upnp" ));
    pObject->AddProperty( new Property( "contributor"  , "dc"   ));
    pObject->AddProperty( new Property( "date"         , "dc"   ));

    return( pObject );
}

/////////////////////////////////////////////////////////////////////////////

CDSObject *CDSObject::CreateVideoItem( QString sId, QString sTitle, QString sParentId, CDSObject *pObject )
{
    if (pObject == NULL)
    {
        pObject = new CDSObject( sId, sTitle, sParentId );
        pObject->m_sClass = "object.item.videoItem";
    }

    CreateItem( sId, sTitle, sParentId, pObject );

    pObject->AddProperty( new Property( "genre"          , "upnp" ));
    pObject->AddProperty( new Property( "longDescription", "upnp" ));
    pObject->AddProperty( new Property( "producer"       , "upnp" ));
    pObject->AddProperty( new Property( "rating"         , "upnp" ));
    pObject->AddProperty( new Property( "actor"          , "upnp" ));
    pObject->AddProperty( new Property( "director"       , "upnp" ));
    pObject->AddProperty( new Property( "description"    , "dc"   ));
    pObject->AddProperty( new Property( "publisher"      , "dc"   ));
    pObject->AddProperty( new Property( "language"       , "dc"   ));
    pObject->AddProperty( new Property( "relation"       , "dc"   ));

    return( pObject );
}

/////////////////////////////////////////////////////////////////////////////

CDSObject *CDSObject::CreateMovie( QString sId, QString sTitle, QString sParentId, CDSObject *pObject )  
{
    if (pObject == NULL)
    {
        pObject = new CDSObject( sId, sTitle, sParentId );
        pObject->m_sClass = "object.item.videoItem.movie";
    }

    CreateVideoItem( sId, sTitle, sParentId, pObject );

    pObject->AddProperty( new Property( "storageMedium"     , "upnp" ));
    pObject->AddProperty( new Property( "DVDRegionCode"     , "upnp" ));
    pObject->AddProperty( new Property( "channelName"       , "upnp" ));
    pObject->AddProperty( new Property( "scheduledStartTime", "upnp" ));
    pObject->AddProperty( new Property( "scheduledEndTime"  , "upnp" ));

    return( pObject );
}

/////////////////////////////////////////////////////////////////////////////

CDSObject *CDSObject::CreateVideoBroadcast( QString sId, QString sTitle, QString sParentId, CDSObject *pObject )
{
    if (pObject == NULL)
    {
        pObject = new CDSObject( sId, sTitle, sParentId );
        pObject->m_sClass = "object.item.videoItem.videoBroadcast";
    }

    CreateVideoItem( sId, sTitle, sParentId, pObject );

    pObject->AddProperty( new Property( "icon"     , "upnp" ));
    pObject->AddProperty( new Property( "region"   , "upnp" ));
    pObject->AddProperty( new Property( "channelNr", "upnp" ));

    return( pObject );
}

/////////////////////////////////////////////////////////////////////////////

CDSObject *CDSObject::CreateMusicVideoClip( QString sId, QString sTitle, QString sParentId, CDSObject *pObject ) 
{
    if (pObject == NULL)
    {
        pObject = new CDSObject( sId, sTitle, sParentId );
        pObject->m_sClass = "object.item.videoItem.musicVideoClip";
    }

    CreateVideoItem( sId, sTitle, sParentId, pObject );

    pObject->AddProperty( new Property( "artist"            , "upnp" ));
    pObject->AddProperty( new Property( "storageMedium"     , "upnp" ));
    pObject->AddProperty( new Property( "album"             , "upnp" ));
    pObject->AddProperty( new Property( "scheduledStartTime", "upnp" ));
    pObject->AddProperty( new Property( "scheduledStopTime" , "upnp" ));
    pObject->AddProperty( new Property( "director"          , "upnp" ));
    pObject->AddProperty( new Property( "contributor"       , "dc"   ));
    pObject->AddProperty( new Property( "date"              , "dc"   ));

    return( pObject );
}

/////////////////////////////////////////////////////////////////////////////

CDSObject *CDSObject::CreateImageItem( QString sId, QString sTitle, QString sParentId, CDSObject *pObject )
{
    if (pObject == NULL)
    {
        pObject = new CDSObject( sId, sTitle, sParentId );
        pObject->m_sClass = "object.item.imageItem";
    }

    CreateItem( sId, sTitle, sParentId, pObject );

    pObject->AddProperty( new Property( "longDescription", "upnp" ));
    pObject->AddProperty( new Property( "storageMedium"  , "upnp" ));
    pObject->AddProperty( new Property( "rating"         , "upnp" ));
    pObject->AddProperty( new Property( "description"    , "dc"   )); 
    pObject->AddProperty( new Property( "publisher"      , "dc"   )); 
    pObject->AddProperty( new Property( "date"           , "dc"   )); 
    pObject->AddProperty( new Property( "rights"         , "dc"   ));

    return( pObject );
}

/////////////////////////////////////////////////////////////////////////////

CDSObject *CDSObject::CreatePhoto( QString sId, QString sTitle, QString sParentId, CDSObject *pObject )
{
    if (pObject == NULL)
    {
        pObject = new CDSObject( sId, sTitle, sParentId );
        pObject->m_sClass = "object.item.imageItem.photo";
    }

    CreateImageItem( sId, sTitle, sParentId, pObject );

    pObject->AddProperty( new Property( "album", "upnp" ));

    return( pObject );
}

/////////////////////////////////////////////////////////////////////////////

CDSObject *CDSObject::CreatePlaylistItem ( QString sId, QString sTitle, QString sParentId, CDSObject *pObject )
{
    if (pObject == NULL)
    {
        pObject = new CDSObject( sId, sTitle, sParentId );
        pObject->m_sClass = "object.item.playlistItem";
    }

    CreateItem( sId, sTitle, sParentId, pObject );

    pObject->AddProperty( new Property( "artist"         , "upnp" ));
    pObject->AddProperty( new Property( "genre"          , "upnp" ));
    pObject->AddProperty( new Property( "longDescription", "upnp" ));
    pObject->AddProperty( new Property( "storageMedium"  , "upnp" ));
    pObject->AddProperty( new Property( "description"    , "dc"   ));
    pObject->AddProperty( new Property( "date"           , "dc"   ));
    pObject->AddProperty( new Property( "language"       , "dc"   ));

    return( pObject );
}

/////////////////////////////////////////////////////////////////////////////

CDSObject *CDSObject::CreateTextItem( QString sId, QString sTitle, QString sParentId, CDSObject *pObject )
{
    if (pObject == NULL)
    {
        pObject = new CDSObject( sId, sTitle, sParentId );
        pObject->m_sClass = "object.item.textItem";
    }

    CreateItem( sId, sTitle, sParentId, pObject );

    pObject->AddProperty( new Property( "author"         , "upnp" ));
    pObject->AddProperty( new Property( "protection"     , "upnp" ));
    pObject->AddProperty( new Property( "longDescription", "upnp" ));
    pObject->AddProperty( new Property( "storageMedium"  , "upnp" ));
    pObject->AddProperty( new Property( "rating"         , "upnp" ));
    pObject->AddProperty( new Property( "description"    , "dc"   ));
    pObject->AddProperty( new Property( "publisher"      , "dc"   ));
    pObject->AddProperty( new Property( "contributor"    , "dc"   ));
    pObject->AddProperty( new Property( "date"           , "dc"   ));
    pObject->AddProperty( new Property( "relation"       , "dc"   ));
    pObject->AddProperty( new Property( "language"       , "dc"   ));
    pObject->AddProperty( new Property( "rights"         , "dc"   ));

    return( pObject );
}

/////////////////////////////////////////////////////////////////////////////

CDSObject *CDSObject::CreateAlbum( QString sId, QString sTitle, QString sParentId, CDSObject *pObject )
{
    if (pObject == NULL)
    {
        pObject = new CDSObject( sId, sTitle, sParentId );
        pObject->m_sClass = "object.container.album";
    }

    CreateContainer( sId, sTitle, sParentId, pObject );

    pObject->AddProperty( new Property( "storageMedium"  , "upnp" ));
    pObject->AddProperty( new Property( "longDescription", "dc"   ));
    pObject->AddProperty( new Property( "description"    , "dc"   ));
    pObject->AddProperty( new Property( "publisher"      , "dc"   ));
    pObject->AddProperty( new Property( "contributor"    , "dc"   ));
    pObject->AddProperty( new Property( "date"           , "dc"   ));
    pObject->AddProperty( new Property( "relation"       , "dc"   ));
    pObject->AddProperty( new Property( "rights"         , "dc"   ));

    return( pObject );
}

/////////////////////////////////////////////////////////////////////////////

CDSObject *CDSObject::CreateMusicAlbum( QString sId, QString sTitle, QString sParentId, CDSObject *pObject )
{
    if (pObject == NULL)
    {
        pObject = new CDSObject( sId, sTitle, sParentId );
        pObject->m_sClass = "object.container.album.musicAlbum";
    }

    CreateAlbum( sId, sTitle, sParentId, pObject );

    pObject->AddProperty( new Property( "artist"     , "upnp" ));
    pObject->AddProperty( new Property( "genre"      , "upnp" ));
    pObject->AddProperty( new Property( "producer"   , "upnp" ));
    pObject->AddProperty( new Property( "albumArtURI", "upnp" ));
    pObject->AddProperty( new Property( "toc"        , "upnp" ));
    return( pObject );
}

/////////////////////////////////////////////////////////////////////////////

CDSObject *CDSObject::CreatePhotoAlbum( QString sId, QString sTitle, QString sParentId, CDSObject *pObject )
{
    if (pObject == NULL)
    {
        pObject = new CDSObject( sId, sTitle, sParentId );
        pObject->m_sClass = "object.container.album.photoAlbum";
    }

    CreateAlbum( sId, sTitle, sParentId, pObject );

    return( pObject );
}

/////////////////////////////////////////////////////////////////////////////

CDSObject *CDSObject::CreateGenre( QString sId, QString sTitle, QString sParentId, CDSObject *pObject )
{
    if (pObject == NULL)
    {
        pObject = new CDSObject( sId, sTitle, sParentId );
        pObject->m_sClass = "object.container.genre";
    }

    CreateContainer( sId, sTitle, sParentId, pObject );

    pObject->AddProperty( new Property( "longDescription", "upnp" ));
    pObject->AddProperty( new Property( "description"    , "dc"   ));

    return( pObject );
}

/////////////////////////////////////////////////////////////////////////////

CDSObject *CDSObject::CreateMusicGenre( QString sId, QString sTitle, QString sParentId, CDSObject *pObject ) 
{
    if (pObject == NULL)
    {
        pObject = new CDSObject( sId, sTitle, sParentId );
        pObject->m_sClass = "object.container.genre.musicGenre";
    }

    CreateGenre( sId, sTitle, sParentId, pObject );

    return( pObject );
}

/////////////////////////////////////////////////////////////////////////////

CDSObject *CDSObject::CreateMovieGenre( QString sId, QString sTitle, QString sParentId, CDSObject *pObject )
{
    if (pObject == NULL)
    {
        pObject = new CDSObject( sId, sTitle, sParentId );
        pObject->m_sClass = "object.container.genre.movieGenre";
    }

    CreateGenre( sId, sTitle, sParentId, pObject );

    return( pObject );
}

/////////////////////////////////////////////////////////////////////////////

CDSObject *CDSObject::CreatePlaylistContainer( QString sId, QString sTitle, QString sParentId, CDSObject *pObject ) 
{
    if (pObject == NULL)
    {
        pObject = new CDSObject( sId, sTitle, sParentId );
        pObject->m_sClass = "object.container.playlistContainer";
    }

    CreateContainer( sId, sTitle, sParentId, pObject );

    pObject->AddProperty( new Property( "artist"         , "upnp" ));
    pObject->AddProperty( new Property( "genre"          , "upnp" ));
    pObject->AddProperty( new Property( "longDescription", "upnp" ));
    pObject->AddProperty( new Property( "producer"       , "upnp" ));
    pObject->AddProperty( new Property( "storageMedium"  , "upnp" ));
    pObject->AddProperty( new Property( "description"    , "dc"   ));  
    pObject->AddProperty( new Property( "contributor"    , "dc"   ));  
    pObject->AddProperty( new Property( "date"           , "dc"   ));  
    pObject->AddProperty( new Property( "language"       , "dc"   ));  
    pObject->AddProperty( new Property( "rights"         , "dc"   ));  

    return( pObject );
}

/////////////////////////////////////////////////////////////////////////////

CDSObject *CDSObject::CreatePerson( QString sId, QString sTitle, QString sParentId, CDSObject *pObject )
{
    if (pObject == NULL)
    {
        pObject = new CDSObject( sId, sTitle, sParentId );
        pObject->m_sClass = "object.container.person";
    }

    CreateContainer( sId, sTitle, sParentId, pObject );

    pObject->AddProperty( new Property( "language", "dc" ));

    return( pObject );
}

/////////////////////////////////////////////////////////////////////////////

CDSObject *CDSObject::CreateMusicArtist( QString sId, QString sTitle, QString sParentId, CDSObject *pObject )
{
    if (pObject == NULL)
    {
        pObject = new CDSObject( sId, sTitle, sParentId );
        pObject->m_sClass = "object.container.person.musicArtist";
    }

    CreatePerson( sId, sTitle, sParentId, pObject );

    pObject->AddProperty( new Property( "genre"               , "upnp" ));
    pObject->AddProperty( new Property( "artistDiscographyURI", "upnp" ));

    return( pObject );
}

/////////////////////////////////////////////////////////////////////////////

CDSObject *CDSObject::CreateStorageSystem( QString sId, QString sTitle, QString sParentId, CDSObject *pObject )
{
    if (pObject == NULL)
    {
        pObject = new CDSObject( sId, sTitle, sParentId );
        pObject->m_sClass = "object.container.storageSystem";
    }

    CreateContainer( sId, sTitle, sParentId, pObject );

    pObject->AddProperty( new Property( "storageTotal"       , "upnp", true )); 
    pObject->AddProperty( new Property( "storageUsed"        , "upnp", true )); 
    pObject->AddProperty( new Property( "storageFree"        , "upnp", true )); 
    pObject->AddProperty( new Property( "storageMaxPartition", "upnp", true )); 
    pObject->AddProperty( new Property( "storageMedium"      , "upnp", true )); 

    return( pObject );
}

/////////////////////////////////////////////////////////////////////////////

CDSObject *CDSObject::CreateStorageVolume( QString sId, QString sTitle, QString sParentId, CDSObject *pObject )
{
    if (pObject == NULL)
    {
        pObject = new CDSObject( sId, sTitle, sParentId );
        pObject->m_sClass = "object.container.storageVolume";
    }

    CreateContainer( sId, sTitle, sParentId, pObject );

    pObject->AddProperty( new Property( "storageTotal" , "upnp", true ));
    pObject->AddProperty( new Property( "storageUsed"  , "upnp", true ));
    pObject->AddProperty( new Property( "storageFree"  , "upnp", true ));        
    pObject->AddProperty( new Property( "storageMedium", "upnp", true ));        

    return( pObject );
}

/////////////////////////////////////////////////////////////////////////////

CDSObject *CDSObject::CreateStorageFolder( QString sId, QString sTitle, QString sParentId, CDSObject *pObject )
{
    if (pObject == NULL)
    {
        pObject = new CDSObject( sId, sTitle, sParentId );
        pObject->m_sClass = "object.container.storageFolder";
    }

    CreateContainer( sId, sTitle, sParentId, pObject );

    pObject->AddProperty( new Property( "storageUsed", "upnp", true ));

    return( pObject );
}
