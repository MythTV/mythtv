//////////////////////////////////////////////////////////////////////////////
// Program Name: upnpcdsobjects.cpp
// Created     : Oct. 24, 2005
//
// Purpose     : uPnp Content Directory Service Object Definitions
//                                                                            
// Copyright (c) 2005 David Blain <dblain@mythtv.org>
//                                          
// Licensed under the GPL v2 or later, see LICENSE for details
//
//////////////////////////////////////////////////////////////////////////////
#include "upnpcdsobjects.h"

#include <QtGlobal>
#if QT_VERSION >= QT_VERSION_CHECK(6,0,0)
#include <QStringConverter>
#else
#include <QTextCodec>
#endif
#include <QTextStream>
#include <QUrl>

#include "libmythbase/mythlogging.h"

inline QString GetBool( bool bVal ) { return( (bVal) ? "1" : "0" ); }

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
//
//
//
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

CDSObject::CDSObject( const QString &sId, 
                      const QString &sTitle, 
                      const QString &sParentId )
    : ReferenceCounter("CDSObject", false),
      m_sId(HTTPRequest::Encode(sId)),
      m_sParentId(HTTPRequest::Encode(sParentId)),
      m_sTitle(HTTPRequest::Encode(sTitle))
{
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

CDSObject::~CDSObject()
{
    while (!m_resources.empty())
    {
        delete m_resources.takeLast();
    }

    while (!m_children.empty())
    {
        m_children.takeLast()->DecrRef();
    }

    // NOLINTNEXTLINE(modernize-loop-convert)
    for (auto it = m_properties.begin(); it != m_properties.end(); ++it)
    {
        delete *it;
        *it = nullptr;
    }
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
            m_properties.insert(pProp->m_sName, pProp);
        else
            m_properties.replace(pProp->m_sName, pProp);
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

void CDSObject::SetPropValue( const QString &sName, const QString &sValue,
                              const QString &sType )
{
    Properties::iterator it = m_properties.find(sName);
    if (it !=  m_properties.end() && *it)
    {
        if ((*it)->m_bMultiValue)
        {
            LOG(VB_UPNP, LOG_WARNING,
                QString("SetPropValue(%1) called on property with bAllowMulti. "
                        "Only the last inserted property will be updated.").arg(sName));
        }
        (*it)->SetValue(sValue);

        if (!sType.isEmpty())
            (*it)->AddAttribute( "type", sType );
    }
    else
    {
        LOG(VB_UPNP, LOG_WARNING,
                QString("SetPropValue(%1) called with non-existent property.").arg(sName));
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
        {
            LOG(VB_UPNP, LOG_WARNING,
                QString("GetPropValue(%1) called on property with bAllowMulti. "
                        "Only the last inserted property will be return."));
        }
        return (*it)->GetValue().toUtf8();
    }
    
    return "";
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

CDSObject *CDSObject::AddChild( CDSObject *pChild )
{
    if (pChild && !m_children.contains(pChild))
    {
        pChild->IncrRef();
        m_nChildCount++;
        if (pChild->m_eType == OT_Container)
            m_nChildContainerCount++;
        pChild->m_sParentId = m_sId;
        m_children.append( pChild );
    }

    return( pChild );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

CDSObject *CDSObject::GetChild( const QString &sID )
{
    CDSObject *pChild = nullptr;
    CDSObjects::iterator it;
    for (it = m_children.begin(); it != m_children.end(); ++it)
    {
        pChild = *it;
        if (!pChild)
            continue;

        if (pChild->m_sId == sID)
            return pChild;
    }

    return nullptr;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

Resource *CDSObject::AddResource( const QString& sProtocol, const QString& sURI )
{
    auto *pRes = new Resource( sProtocol, sURI );
    
    m_resources.append( pRes );

    return( pRes );
}


/**
 * \brief Return the number of children in this container
 */

uint32_t CDSObject::GetChildCount(void) const
{
    uint32_t nCount = m_children.count();
    if (nCount == 0)
        return( m_nChildCount );

    return( nCount );
}

/**
 * \brief Allows the caller to set childCount without having to load children.
 */

void CDSObject::SetChildCount( uint32_t nCount )
{
    m_nChildCount = nCount;
}

/**
 * \brief Return the number of child containers in this container
 *
 * Per the UPnP Content Directory Service spec returning the number of
 * containers lets the client determine the number of items by subtracting
 * this value from the childCount
 */

uint32_t CDSObject::GetChildContainerCount(void) const
{
    return m_nChildContainerCount;
}

/**
 * \brief Allows the caller to set childContainerCount without having to load
 *        children.
 */

void CDSObject::SetChildContainerCount( uint32_t nCount )
{
    m_nChildContainerCount = nCount;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

QString CDSObject::toXml( FilterMap &filter,
                          bool ignoreChildren ) const
{
    QString     sXML;
    QTextStream os( &sXML, QIODevice::WriteOnly );
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
    os.setCodec(QTextCodec::codecForName("UTF-8"));
#else
    os.setEncoding(QStringConverter::Utf8);
#endif
    toXml(os, filter, ignoreChildren);
    os << Qt::flush;
    return( sXML );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void CDSObject::toXml( QTextStream &os, FilterMap &filter,
                       bool ignoreChildren ) const
{
    QString sEndTag = "";

    /**
     * NOTE FilterMap contains a list of what should be included, not what should
     *      be excluded.
     *
     *      The client is expected either to indicate that everything should be
     *      returned with an asterix, or to supply a comma seperated list of
     *      the only the named properties and attributes.
     *
     *      @ - Attributes are denoted by format <element>@<attribute>
     *
     *      # - The use of a hash at the end of a name indicates that this
     *          property and all it's children and attributes should be returned.
     *
     *      Inclusion of an attribute in the filter list implies the inclusion
     *      of it's parent element and value.
     *      e.g. filter="res@size" implies <res size="{size}">{url}</res>
     *      However optional tags such as res@duration which are not named will
     *      be omitted.
     *
     *      'Required' properties must always be included irrespective of
     *      any filter!
     *
     *      See UPnP MediaServer, ContentDirectory Service Section 2.3.18, 2013
     */
    bool    bFilter = true;

    if (filter.contains("*"))
        bFilter = false;

    switch( m_eType )
    {   
        case OT_Container:
        {
            if (bFilter && filter.contains("container#"))
                bFilter = false;

            os << "<container id=\"" << m_sId
               << "\" parentID=\"" << m_sParentId
               << "\" restricted=\"" << GetBool( m_bRestricted );

            if (!bFilter || filter.contains("@searchable"))
                os << "\" searchable=\"" << GetBool( m_bSearchable );

            if (!bFilter || filter.contains("@childCount"))
                os << "\" childCount=\"" << GetChildCount();

            if (!bFilter || filter.contains("@childContainerCount"))
                os << "\" childContainerCount=\"" << GetChildContainerCount();

            os << "\" >" << Qt::endl;

            sEndTag = "</container>";

            break;
        }
        case OT_Item:
        {
            if (bFilter && filter.contains("item#"))
                bFilter = false;

            os << "<item id=\"" << m_sId
               << "\" parentID=\"" << m_sParentId
               << "\" restricted=\"" << GetBool( m_bRestricted )
               << "\" >" << Qt::endl;

            sEndTag = "</item>";

            break;
        }
        default: break;
    }

    os << "<dc:title>"   << m_sTitle << "</dc:title>" << Qt::endl;
    os << "<upnp:class>" << m_sClass << "</upnp:class>" << Qt::endl;

    // ----------------------------------------------------------------------
    // Output all Properties
    // ----------------------------------------------------------------------

    for (auto *pProp : std::as_const(m_properties))
    {
        if (pProp->m_bRequired || (!pProp->GetValue().isEmpty()))
        {
            QString sName;
            
            if (!pProp->m_sNameSpace.isEmpty())
                sName = pProp->m_sNameSpace + ':' + pProp->m_sName;
            else
                sName = pProp->m_sName;

            if (pProp->m_bRequired ||
                (!bFilter) ||
                FilterContains(filter, sName))
            {
                bool filterAttributes = true;
                if (!bFilter || filter.contains(QString("%1#").arg(sName)))
                    filterAttributes = false;

                os << "<"  << sName;

                for (const auto & attr : std::as_const(pProp->m_lstAttributes))
                {
                    QString filterName = QString("%1@%2").arg(sName,
                                                              attr.m_sName);
                    if (attr.m_bRequired  || !filterAttributes ||
                        filter.contains(filterName))
                        os << " " << attr.m_sName << "=\"" << attr.m_sValue << "\"";
                }

                os << ">";
                os << pProp->GetEncodedValue();
                os << "</" << sName << ">" << Qt::endl;
            }
        }
    }

    // ----------------------------------------------------------------------
    // Output any Res Elements
    // ----------------------------------------------------------------------

    if (!bFilter || filter.contains("res"))
    {
        bool filterAttributes = true;
        if (!bFilter || filter.contains("res#"))
            filterAttributes = false;
        for (auto *resource : std::as_const(m_resources))
        {
            os << "<res protocolInfo=\"" << resource->m_sProtocolInfo << "\" ";

            QString filterName;
            for (const auto & attr : std::as_const(resource->m_lstAttributes))
            {
                filterName = QString("res@%1").arg(attr.m_sName);
                if (attr.m_bRequired  || !filterAttributes ||
                    filter.contains(filterName))
                    os << attr.m_sName << "=\"" << attr.m_sValue << "\" ";
            }

            os << ">" << resource->m_sURI;
            os << "</res>" << Qt::endl;
        }
    }

    // ----------------------------------------------------------------------
    // Output any children
    // ----------------------------------------------------------------------

    if (!ignoreChildren)
    {
        for (auto *cit : m_children)
            cit->toXml(os, filter);
    }

    // ----------------------------------------------------------------------
    // Close Element Tag
    // ----------------------------------------------------------------------

    os << sEndTag << Qt::endl;
    os << Qt::flush;
}


/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

CDSObject *CDSObject::CreateItem( const QString& sId, const QString& sTitle, const QString& sParentId, CDSObject *pObject )
{
    if (pObject == nullptr)
    {
        pObject = new CDSObject( sId, sTitle, sParentId );
        pObject->m_sClass = "object.item";
    }

    pObject->m_eType = OT_Item;
    pObject->m_bSearchable = false; // UPnP - CDS B.1.5 @searchable

    pObject->AddProperty( new Property( "refID" ) );

    return( pObject );
}

/////////////////////////////////////////////////////////////////////////////

CDSObject *CDSObject::CreateContainer( const QString& sId, const QString& sTitle, const QString& sParentId, CDSObject *pObject )
{
    if (pObject == nullptr)
    {
        pObject = new CDSObject( sId, sTitle, sParentId );
        pObject->m_sClass = "object.container";
    }

    pObject->m_eType = OT_Container;
    pObject->m_bSearchable = true; // DLNA requirement - 7.4.3.5.9

    pObject->AddProperty( new Property( "childCount"  ));                   
    pObject->AddProperty( new Property( "createClass" ));                   
    pObject->AddProperty( new Property( "searchClass" ));                   
    pObject->AddProperty( new Property( "searchable"  ));

    pObject->AddProperty( new Property( "creator", "dc" ));
    pObject->AddProperty( new Property( "date"   , "dc" ));

    pObject->AddProperty( new Property( "longDescription", "upnp"   ));
    pObject->AddProperty( new Property( "description"    , "dc"   ));

    return( pObject );
}

/////////////////////////////////////////////////////////////////////////////

CDSObject *CDSObject::CreateAudioItem( const QString& sId, const QString& sTitle, const QString& sParentId, CDSObject *pObject )
{
    if (pObject == nullptr)
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

    pObject->AddProperty( new Property( "creator"         , "dc" ));
    pObject->AddProperty( new Property( "date"            , "dc" ));

    return( pObject );
}

/////////////////////////////////////////////////////////////////////////////

CDSObject *CDSObject::CreateMusicTrack( const QString& sId, const QString& sTitle, const QString& sParentId, CDSObject *pObject )
{
    if (pObject == nullptr)
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

    pObject->AddProperty( new Property( "playbackCount"       , "upnp" ));
    pObject->AddProperty( new Property( "lastPlaybackTime"    , "upnp" ));

    pObject->AddProperty( new Property( "albumArtURI", "upnp", false, "", true)); // TN
    pObject->AddProperty( new Property( "albumArtURI", "upnp", false, "", true)); // SM
    pObject->AddProperty( new Property( "albumArtURI", "upnp", false, "", true)); // MED
    pObject->AddProperty( new Property( "albumArtURI", "upnp", false, "", true)); // LRG

#if 0
    pObject->AddProperty( new Property( "publisher"       , "dc"   ));
    pObject->AddProperty( new Property( "language"        , "dc"   ));
    pObject->AddProperty( new Property( "relation"        , "dc"   ));
    pObject->AddProperty( new Property( "rights"          , "dc"   ));


    pObject->AddProperty( new Property( "playlist"            , "upnp" ));
    pObject->AddProperty( new Property( "storageMedium"       , "upnp" ));
    pObject->AddProperty( new Property( "contributor"         , "dc"   ));
#endif

    return( pObject );
}

/////////////////////////////////////////////////////////////////////////////

CDSObject *CDSObject::CreateAudioBroadcast( const QString& sId, const QString& sTitle, const QString& sParentId, CDSObject *pObject )
{
    if (pObject == nullptr)
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

CDSObject *CDSObject::CreateAudioBook( const QString& sId, const QString& sTitle, const QString& sParentId, CDSObject *pObject )
{
    if (pObject == nullptr)
    {
        pObject = new CDSObject( sId, sTitle, sParentId );
        pObject->m_sClass = "object.item.audioItem.audioBook";
    }

    CreateAudioItem( sId, sTitle, sParentId, pObject );

    pObject->AddProperty( new Property( "storageMedium", "upnp" ));
    pObject->AddProperty( new Property( "producer"     , "upnp" ));
    pObject->AddProperty( new Property( "contributor"  , "dc"   ));

    return( pObject );
}

/////////////////////////////////////////////////////////////////////////////

CDSObject *CDSObject::CreateVideoItem( const QString& sId, const QString& sTitle, const QString& sParentId, CDSObject *pObject )
{
    if (pObject == nullptr)
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
    pObject->AddProperty( new Property( "episodeNumber"  , "upnp" ));
    pObject->AddProperty( new Property( "episodeCount"   , "upnp" ));
    pObject->AddProperty( new Property( "seriesTitle"    , "upnp" ));
    pObject->AddProperty( new Property( "programTitle"   , "upnp" ));
    pObject->AddProperty( new Property( "description"    , "dc"   ));
    pObject->AddProperty( new Property( "publisher"      , "dc"   ));
    pObject->AddProperty( new Property( "language"       , "dc"   ));
    pObject->AddProperty( new Property( "relation"       , "dc"   ));

    pObject->AddProperty( new Property( "creator"        , "dc" ));
    pObject->AddProperty( new Property( "date"           , "dc" ));

    pObject->AddProperty( new Property( "channelID"      , "upnp" ));
    pObject->AddProperty( new Property( "callSign"       , "upnp" ));
    pObject->AddProperty( new Property( "channelNr"      , "upnp" ));
    pObject->AddProperty( new Property( "channelName"    , "upnp" ));

    pObject->AddProperty( new Property( "scheduledStartTime", "upnp" ));
    pObject->AddProperty( new Property( "scheduledEndTime"  , "upnp" ));
    pObject->AddProperty( new Property( "scheduledDuration" , "upnp" ));
    pObject->AddProperty( new Property( "srsRecordScheduleID" , "upnp" ));
    pObject->AddProperty( new Property( "recordedStartDateTime"   , "upnp" ));
    pObject->AddProperty( new Property( "recordedDuration"        , "upnp" ));
    pObject->AddProperty( new Property( "recordedDayOfWeek"       , "upnp" ));

    pObject->AddProperty( new Property( "programID"      , "upnp" ));
    pObject->AddProperty( new Property( "seriesID"       , "upnp" ));

    // Added for Microsoft Media Player Compatibility
    pObject->AddProperty( new Property( "artist"         , "upnp" ));
    pObject->AddProperty( new Property( "album"          , "upnp" ));
    //

    pObject->AddProperty( new Property( "albumArtURI", "upnp", false, "", true )); // TN
    pObject->AddProperty( new Property( "albumArtURI", "upnp", false, "", true )); // SM
    pObject->AddProperty( new Property( "albumArtURI", "upnp", false, "", true )); // MED
    pObject->AddProperty( new Property( "albumArtURI", "upnp", false, "", true )); // LRG
    return( pObject );
}

/////////////////////////////////////////////////////////////////////////////

CDSObject *CDSObject::CreateMovie( const QString& sId, const QString& sTitle, const QString& sParentId, CDSObject *pObject )
{
    if (pObject == nullptr)
    {
        pObject = new CDSObject( sId, sTitle, sParentId );
        pObject->m_sClass = "object.item.videoItem.movie";
    }

    CreateVideoItem( sId, sTitle, sParentId, pObject );

    pObject->AddProperty( new Property( "storageMedium"     , "upnp" ));
    pObject->AddProperty( new Property( "DVDRegionCode"     , "upnp" ));

    return( pObject );
}

/////////////////////////////////////////////////////////////////////////////

CDSObject *CDSObject::CreateVideoBroadcast( const QString& sId, const QString& sTitle, const QString& sParentId, CDSObject *pObject )
{
    if (pObject == nullptr)
    {
        pObject = new CDSObject( sId, sTitle, sParentId );
        pObject->m_sClass = "object.item.videoItem.videoBroadcast";
    }

    CreateVideoItem( sId, sTitle, sParentId, pObject );

    pObject->AddProperty( new Property( "icon"     , "upnp" ));
    pObject->AddProperty( new Property( "region"   , "upnp" ));

    return( pObject );
}

/////////////////////////////////////////////////////////////////////////////

CDSObject *CDSObject::CreateMusicVideoClip( const QString& sId, const QString& sTitle, const QString& sParentId, CDSObject *pObject )
{
    if (pObject == nullptr)
    {
        pObject = new CDSObject( sId, sTitle, sParentId );
        pObject->m_sClass = "object.item.videoItem.musicVideoClip";
    }

    CreateVideoItem( sId, sTitle, sParentId, pObject );

    pObject->AddProperty( new Property( "artist"            , "upnp" ));
    pObject->AddProperty( new Property( "storageMedium"     , "upnp" ));
    pObject->AddProperty( new Property( "album"             , "upnp" ));
    pObject->AddProperty( new Property( "contributor"       , "dc"   ));

    return( pObject );
}

/////////////////////////////////////////////////////////////////////////////

CDSObject *CDSObject::CreateImageItem( const QString& sId, const QString& sTitle, const QString& sParentId, CDSObject *pObject )
{
    if (pObject == nullptr)
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
    pObject->AddProperty( new Property( "rights"         , "dc"   ));

    pObject->AddProperty( new Property( "creator"        , "dc" ));
    pObject->AddProperty( new Property( "date"           , "dc" ));

    return( pObject );
}

/////////////////////////////////////////////////////////////////////////////

CDSObject *CDSObject::CreatePhoto( const QString& sId, const QString& sTitle, const QString& sParentId, CDSObject *pObject )
{
    if (pObject == nullptr)
    {
        pObject = new CDSObject( sId, sTitle, sParentId );
        pObject->m_sClass = "object.item.imageItem.photo";
    }

    CreateImageItem( sId, sTitle, sParentId, pObject );

    pObject->AddProperty( new Property( "album", "upnp" ));

    return( pObject );
}

/////////////////////////////////////////////////////////////////////////////

CDSObject *CDSObject::CreatePlaylistItem ( const QString& sId, const QString& sTitle, const QString& sParentId, CDSObject *pObject )
{
    if (pObject == nullptr)
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
    pObject->AddProperty( new Property( "language"       , "dc"   ));

    return( pObject );
}

/////////////////////////////////////////////////////////////////////////////

CDSObject *CDSObject::CreateTextItem( const QString& sId, const QString& sTitle, const QString& sParentId, CDSObject *pObject )
{
    if (pObject == nullptr)
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
    pObject->AddProperty( new Property( "relation"       , "dc"   ));
    pObject->AddProperty( new Property( "language"       , "dc"   ));
    pObject->AddProperty( new Property( "rights"         , "dc"   ));

    return( pObject );
}

/////////////////////////////////////////////////////////////////////////////

CDSObject *CDSObject::CreateAlbum( const QString& sId, const QString& sTitle, const QString& sParentId, CDSObject *pObject )
{
    if (pObject == nullptr)
    {
        pObject = new CDSObject( sId, sTitle, sParentId );
        pObject->m_sClass = "object.container.album";
    }

    CreateContainer( sId, sTitle, sParentId, pObject );

    pObject->AddProperty( new Property( "storageMedium"  , "upnp" ));
    pObject->AddProperty( new Property( "publisher"      , "dc"   ));
    pObject->AddProperty( new Property( "contributor"    , "dc"   ));
    pObject->AddProperty( new Property( "relation"       , "dc"   ));
    pObject->AddProperty( new Property( "rights"         , "dc"   ));

    // Artwork
    pObject->AddProperty( new Property( "albumArtURI", "upnp", false, "", true)); // TN
    pObject->AddProperty( new Property( "albumArtURI", "upnp", false, "", true)); // SM
    pObject->AddProperty( new Property( "albumArtURI", "upnp", false, "", true)); // MED
    pObject->AddProperty( new Property( "albumArtURI", "upnp", false, "", true)); // LRG

    return( pObject );
}

/////////////////////////////////////////////////////////////////////////////

CDSObject *CDSObject::CreateMusicAlbum( const QString& sId, const QString& sTitle, const QString& sParentId, CDSObject *pObject )
{
    if (pObject == nullptr)
    {
        pObject = new CDSObject( sId, sTitle, sParentId );
        pObject->m_sClass = "object.container.album.musicAlbum";
    }

    CreateAlbum( sId, sTitle, sParentId, pObject );

    pObject->AddProperty( new Property( "artist"     , "upnp" ));
    pObject->AddProperty( new Property( "genre"      , "upnp" ));
    pObject->AddProperty( new Property( "producer"   , "upnp" ));
    pObject->AddProperty( new Property( "toc"        , "upnp" ));

    return( pObject );
}

/////////////////////////////////////////////////////////////////////////////

CDSObject *CDSObject::CreatePhotoAlbum( const QString& sId, const QString& sTitle, const QString& sParentId, CDSObject *pObject )
{
    if (pObject == nullptr)
    {
        pObject = new CDSObject( sId, sTitle, sParentId );
        pObject->m_sClass = "object.container.album.photoAlbum";
    }

    CreateAlbum( sId, sTitle, sParentId, pObject );

    return( pObject );
}

/////////////////////////////////////////////////////////////////////////////

CDSObject *CDSObject::CreateGenre( const QString& sId, const QString& sTitle, const QString& sParentId, CDSObject *pObject )
{
    if (pObject == nullptr)
    {
        pObject = new CDSObject( sId, sTitle, sParentId );
        pObject->m_sClass = "object.container.genre";
    }

    CreateContainer( sId, sTitle, sParentId, pObject );

    return( pObject );
}

/////////////////////////////////////////////////////////////////////////////

CDSObject *CDSObject::CreateMusicGenre( const QString& sId, const QString& sTitle, const QString& sParentId, CDSObject *pObject )
{
    if (pObject == nullptr)
    {
        pObject = new CDSObject( sId, sTitle, sParentId );
        pObject->m_sClass = "object.container.genre.musicGenre";
    }

    CreateGenre( sId, sTitle, sParentId, pObject );

    return( pObject );
}

/////////////////////////////////////////////////////////////////////////////

CDSObject *CDSObject::CreateMovieGenre( const QString& sId, const QString& sTitle, const QString& sParentId, CDSObject *pObject )
{
    if (pObject == nullptr)
    {
        pObject = new CDSObject( sId, sTitle, sParentId );
        pObject->m_sClass = "object.container.genre.movieGenre";
    }

    CreateGenre( sId, sTitle, sParentId, pObject );

    return( pObject );
}

/////////////////////////////////////////////////////////////////////////////

CDSObject *CDSObject::CreatePlaylistContainer( const QString& sId, const QString& sTitle, const QString& sParentId, CDSObject *pObject )
{
    if (pObject == nullptr)
    {
        pObject = new CDSObject( sId, sTitle, sParentId );
        pObject->m_sClass = "object.container.playlistContainer";
    }

    CreateContainer( sId, sTitle, sParentId, pObject );

    pObject->AddProperty( new Property( "artist"         , "upnp" ));
    pObject->AddProperty( new Property( "genre"          , "upnp" ));
    pObject->AddProperty( new Property( "producer"       , "upnp" ));
    pObject->AddProperty( new Property( "storageMedium"  , "upnp" ));
    pObject->AddProperty( new Property( "contributor"    , "dc"   ));  
    pObject->AddProperty( new Property( "language"       , "dc"   ));  
    pObject->AddProperty( new Property( "rights"         , "dc"   ));  

    return( pObject );
}

/////////////////////////////////////////////////////////////////////////////

CDSObject *CDSObject::CreatePerson( const QString& sId, const QString& sTitle, const QString& sParentId, CDSObject *pObject )
{
    if (pObject == nullptr)
    {
        pObject = new CDSObject( sId, sTitle, sParentId );
        pObject->m_sClass = "object.container.person";
    }

    CreateContainer( sId, sTitle, sParentId, pObject );

    pObject->AddProperty( new Property( "language", "dc" ));

    return( pObject );
}

/////////////////////////////////////////////////////////////////////////////

CDSObject *CDSObject::CreateMusicArtist( const QString& sId, const QString& sTitle, const QString& sParentId, CDSObject *pObject )
{
    if (pObject == nullptr)
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

CDSObject *CDSObject::CreateStorageSystem( const QString& sId, const QString& sTitle, const QString& sParentId, CDSObject *pObject )
{
    if (pObject == nullptr)
    {
        pObject = new CDSObject( sId, sTitle, sParentId );
        pObject->m_sClass = "object.container.storageSystem";
    }

    CreateContainer( sId, sTitle, sParentId, pObject );

    pObject->AddProperty( new Property( "storageTotal"       , "upnp", true, "-1"  ));
    pObject->AddProperty( new Property( "storageUsed"        , "upnp", true, "-1"  ));
    pObject->AddProperty( new Property( "storageFree"        , "upnp", true, "-1"  ));
    pObject->AddProperty( new Property( "storageMaxPartition", "upnp", true, "-1"  ));
    pObject->AddProperty( new Property( "storageMedium"      , "upnp", true, "HDD" ));

    return( pObject );
}

/////////////////////////////////////////////////////////////////////////////

CDSObject *CDSObject::CreateStorageVolume( const QString& sId, const QString& sTitle, const QString& sParentId, CDSObject *pObject )
{
    if (pObject == nullptr)
    {
        pObject = new CDSObject( sId, sTitle, sParentId );
        pObject->m_sClass = "object.container.storageVolume";
    }

    CreateContainer( sId, sTitle, sParentId, pObject );

    pObject->AddProperty( new Property( "storageTotal" , "upnp", true, "-1"  ));
    pObject->AddProperty( new Property( "storageUsed"  , "upnp", true, "-1"  ));
    pObject->AddProperty( new Property( "storageFree"  , "upnp", true, "-1"  ));
    pObject->AddProperty( new Property( "storageMedium", "upnp", true, "HDD" ));

    return( pObject );
}

/////////////////////////////////////////////////////////////////////////////

CDSObject *CDSObject::CreateStorageFolder( const QString& sId, const QString& sTitle, const QString& sParentId, CDSObject *pObject )
{
    if (pObject == nullptr)
    {
        pObject = new CDSObject( sId, sTitle, sParentId );
        pObject->m_sClass = "object.container.storageFolder";
    }

    CreateContainer( sId, sTitle, sParentId, pObject );

    pObject->AddProperty( new Property( "storageUsed", "upnp", true, "-1" ));

    return( pObject );
}

bool CDSObject::FilterContains(const FilterMap &filter, const QString& name)
{
    // ContentDirectory Service, 2013 UPnP Forum
    // 2.3.18 A_ARG_TYPE_Filter

    // Exact match
    if (filter.contains(name, Qt::CaseInsensitive))
        return true;

    // # signfies that this property and all it's children must be returned
    // This is presently implemented higher up to save time

    // If an attribute is required then it's parent must also be returned
    QString dependentAttribute = QString("%1@").arg(name);
    QStringList matches = filter.filter(name, Qt::CaseInsensitive);
    QStringList::iterator it;
    for (it = matches.begin(); it != matches.end(); ++it)
    {
        if ((*it).startsWith(dependentAttribute))
            return true;
    }

    return false;
}

