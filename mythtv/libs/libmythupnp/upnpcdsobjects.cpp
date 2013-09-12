//////////////////////////////////////////////////////////////////////////////
// Program Name: upnpcdsobjects.cpp
// Created     : Oct. 24, 2005
//
// Purpose     : uPnp Content Directory Service Object Definitions
//                                                                            
// Copyright (c) 2005 David Blain <dblain@mythtv.org>
//                                          
// Licensed under the GPL v2 or later, see COPYING for details                    
//
//////////////////////////////////////////////////////////////////////////////

#include <QTextStream>
#include <QTextCodec>
#include <QUrl>

#include "upnpcds.h"
#include "mythlogging.h"

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
    : m_nUpdateId(1), m_eType(OT_Container),
      m_sId(HTTPRequest::Encode(sId)),
      m_sParentId(HTTPRequest::Encode(sParentId)),
      m_sTitle(HTTPRequest::Encode(sTitle)),
      m_bRestricted(true), m_bSearchable(false),
      m_sWriteStatus("PROTECTED"), m_nChildCount(-1)
{
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

CDSObject::~CDSObject()
{
    while (!m_resources.empty())
    {
        delete m_resources.back();
        m_resources.pop_back();
    }

    while (!m_children.empty())
    {
        delete m_children.back();
        m_children.pop_back();
    }

    Properties::iterator it = m_properties.begin();
    for (; it != m_properties.end(); ++it)
        delete *it;
    m_properties.clear();
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

Property *CDSObject::AddProperty( Property *pProp )
{
    if (pProp)
    {
        // If this property is allowed multiple times in an object
        // e.g. Different sizes of artwork, then just insert it
        // Otherwise remove all existing instances of this property first
        // NOTE: This requires ALL instances of a property which can exist
        //       more than once to have m_bAllowMulti set to true.
        if (pProp->m_bMultiValue)
            m_properties.insertMulti(pProp->m_sName, pProp);
        else
        {
            Properties::iterator it = m_properties.find(pProp->m_sName);
            while (it != m_properties.end() && it.key() == pProp->m_sName)
            {
                delete *it;
                it = m_properties.erase(it);
            }
            m_properties[pProp->m_sName] = pProp;
        }
    }

    return pProp;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

QList<Property*> CDSObject::GetProperties( const QString &sName )
{
    QList<Property*> props;
    Properties::iterator it = m_properties.find(sName);
    while (it != m_properties.end() && it.key() == sName)
    {
        if (*it)
            props.append(*it);
        ++it;
    }

    return props;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void CDSObject::SetPropValue( const QString &sName, const QString &sValue )
{
    Properties::iterator it = m_properties.find(sName);
    if (it !=  m_properties.end() && *it)
    {
        if ((*it)->m_bMultiValue)
            LOG(VB_UPNP, LOG_WARNING,
                QString("SetPropValue(%1) called on property with bAllowMulti. "
                        "Only the last inserted property will be updated."));
        (*it)->m_sValue = HTTPRequest::Encode(sValue);
    }
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

QString CDSObject::GetPropValue(const QString &sName) const
{
    Properties::const_iterator it = m_properties.find(sName);

    if (it !=  m_properties.end() && *it)
    {
        if ((*it)->m_bMultiValue)
            LOG(VB_UPNP, LOG_WARNING,
                QString("GetPropValue(%1) called on property with bAllowMulti. "
                        "Only the last inserted property will be return."));
        return QUrl::fromPercentEncoding((*it)->m_sValue.toUtf8());
    }
    
    return "";
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

long CDSObject::GetChildCount(void) const
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

QString CDSObject::toXml( FilterMap &filter ) const
{
    QString     sXML;
    QTextStream os( &sXML, QIODevice::WriteOnly );
    os.setCodec(QTextCodec::codecForName("UTF-8"));
    toXml(os, filter);
    os << flush;
    return( sXML );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void CDSObject::toXml(QTextStream &os, FilterMap &filter) const
{
    QString sEndTag = "";
    bool    bFilter = true;

    if (filter.indexOf( "*" ) != -1)
        bFilter = false;

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

    Properties::const_iterator it = m_properties.begin();
    for (; it != m_properties.end(); ++it)
    {
        const Property *pProp = *it;

        if (pProp->m_bRequired || (pProp->m_sValue.length() > 0))
        {
            QString sName;
            
            if (pProp->m_sNameSpace.length() > 0)
                sName = pProp->m_sNameSpace + ':' + pProp->m_sName;
            else
                sName = pProp->m_sName;

            if (pProp->m_bRequired || !bFilter 
                || ( filter.indexOf( sName ) != -1))
            {
                os << "<"  << sName;

                    NameValues::const_iterator nit = pProp->m_lstAttributes.begin();
                    for (; nit != pProp->m_lstAttributes.end(); ++ nit)
                        os << " " <<(*nit).sName << "=\"" << (*nit).sValue << "\"";

                os << ">";
                os << pProp->m_sValue;
                os << "</" << sName << ">";
            }
        }
    }

    // ----------------------------------------------------------------------
    // Output any Res Elements
    // ----------------------------------------------------------------------

    Resources::const_iterator rit = m_resources.begin();
    for (; rit != m_resources.end(); ++rit)
    {
        os << "<res protocolInfo=\"" << (*rit)->m_sProtocolInfo << "\" ";

        NameValues::const_iterator nit = (*rit)->m_lstAttributes.begin();
        for (; nit != (*rit)->m_lstAttributes.end(); ++ nit)
            os << (*nit).sName << "=\"" << (*nit).sValue << "\" ";

        os << ">" << (*rit)->m_sURI;
        os << "</res>\r\n";
    }

    // ----------------------------------------------------------------------
    // Output any children
    // ----------------------------------------------------------------------

    CDSObjects::const_iterator cit = m_children.begin();
    for (; cit != m_children.end(); ++cit)
        (*cit)->toXml(os, filter);

    // ----------------------------------------------------------------------
    // Close Element Tag
    // ----------------------------------------------------------------------

    os << sEndTag;
    os << flush;
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

    pObject->AddProperty( new Property( "albumArtURI", "upnp", false, "", true)); // TN
    pObject->AddProperty( new Property( "albumArtURI", "upnp", false, "", true)); // SM
    pObject->AddProperty( new Property( "albumArtURI", "upnp", false, "", true)); // MED
    pObject->AddProperty( new Property( "albumArtURI", "upnp", false, "", true)); // LRG

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

    // Added for Microsoft Media Player Compatibility

    pObject->AddProperty( new Property( "creator"        , "dc"   ));
    pObject->AddProperty( new Property( "artist"         , "upnp" ));
    pObject->AddProperty( new Property( "album"          , "upnp" ));
    pObject->AddProperty( new Property( "date"           , "dc"   ));


    pObject->AddProperty( new Property( "albumArtURI", "upnp", false, "", true )); // TN
    pObject->AddProperty( new Property( "albumArtURI", "upnp", false, "", true )); // SM
    pObject->AddProperty( new Property( "albumArtURI", "upnp", false, "", true )); // MED
    pObject->AddProperty( new Property( "albumArtURI", "upnp", false, "", true )); // LRG
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
