//////////////////////////////////////////////////////////////////////////////
// Program Name: UPnpCDSObjects.h
// Created     : Oct. 24, 2005
//
// Purpose     : uPnp Content Directory Service Object Definitions
//
// Copyright (c) 2005 David Blain <dblain@mythtv.org>
//
// Licensed under the GPL v2 or later, see LICENSE for details
//
//////////////////////////////////////////////////////////////////////////////

#ifndef UPNPCDSOBJECTS_H
#define UPNPCDSOBJECTS_H

#include <utility>

#include <QDateTime>
#include <QList>
#include <QMap>
#include <QString>

#include "libmythbase/referencecounter.h"

#include "upnpexp.h"
#include "httprequest.h"

class CDSObject;
class QTextStream;

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////

enum ObjectTypes : std::uint8_t
{
    OT_Undefined  = 0,
    OT_Container  = 1,
    OT_Item       = 2,
    OT_Res        = 3
};

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////

class Property
{
    public:

        QString  m_sName;
        QString  m_sNameSpace;
        bool     m_bRequired   {false};
        bool     m_bMultiValue {false};
        NameValues      m_lstAttributes;

    public:

        explicit Property( QString sName,
                           QString sNameSpace  = "",
                           bool           bRequired   = false,
                           const QString &sValue      = "",
                           bool           bMultiValue = false
                         )
          : m_sName(std::move(sName)),
            m_sNameSpace(std::move(sNameSpace)),
            m_bRequired(bRequired),
            m_bMultiValue(bMultiValue)
        {
            m_sValue      = HTTPRequest::Encode(sValue);
        }

        void SetValue(const QString &value)
        {
            m_sValue = value;
        }

        QString GetValue(void) const
        {
            return m_sValue;
        }

        QString GetEncodedValue(void) const
        {
            return HTTPRequest::Encode(m_sValue);
        }

        void AddAttribute( const QString &sName,
                           const QString &sValue )
        {
            m_lstAttributes.push_back(NameValue(sName, HTTPRequest::Encode(sValue)));
        }

    protected:
        QString  m_sValue;
};

using Properties = QMultiMap<QString,Property*>;
using CDSObjects = QList<CDSObject*>;

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////

class Resource
{
    public:

        QString         m_sProtocolInfo;
        QString         m_sURI;

        NameValues      m_lstAttributes;

    public:

        Resource( QString sProtocolInfo,
                  const QString &sURI )
          : m_sProtocolInfo(std::move(sProtocolInfo))
        {
            m_sURI          = HTTPRequest::Encode(sURI);
        }

        void AddAttribute( const QString &sName,
                           const QString &sValue )
        {
            m_lstAttributes.push_back(NameValue(sName, HTTPRequest::Encode(sValue)));
        }
};

using Resources = QList<Resource*>;

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////

class ContainerClass
{
    public:

        QString         m_sClass;
        QString         m_sName;
        bool            m_bIncludeDerived;

    public:

        ContainerClass( QString sClass,
                        QString sName,
                        bool           bIncludeDerived )
          : m_sClass(std::move(sClass)),
            m_sName(std::move(sName)),
            m_bIncludeDerived(bIncludeDerived)
        {
        }
};

using Classes = QList<ContainerClass*>;

/**
 * NOTE FilterMap contains a list of what should be included, not what should
 *      be excluded.
 *
 *      The client is expected either to indicate that everything should be
 *      returned with an asterix, or to supply a comma seperated list of
 *      the only the named properties and attributes.
 *
 *      @ - Attributes are denoted by format \<element\>@\<attribute\>
 *
 *      # - The use of a hash at the end of a name indicates that this
 *          property and all it's children and attributes should be returned.
 *
 *      Inclusion of an attribute in the filter list implies the inclusion
 *      of it's parent element and value.
 *      e.g. filter="res\@size" implies \<res size="{size}"\>{url}\</res\>
 *      However optional tags such as res\@duration which are not named will
 *      be omitted.
 *
 *      'Required' properties must always be included irrespective of
 *      any filter!
 *
 *      See UPnP MediaServer, ContentDirectory Service Section 2.3.18, 2013
 */
using FilterMap = QStringList;

//////////////////////////////////////////////////////////////////////////////

class UPNP_PUBLIC CDSObject : public ReferenceCounter
{
    public:
        short           m_nUpdateId            {1};

        ObjectTypes     m_eType                {OT_Container};

        // Required

        QString         m_sId;
        QString         m_sParentId;
        QString         m_sTitle;
        QString         m_sClass;
        bool            m_bRestricted          {true};
        bool            m_bSearchable          {false};

        // Optional

        QString         m_sCreator;
        QString         m_sWriteStatus         {"PROTECTED"};

        // Only appropriate for Container Classes

        Classes         m_SearchClass;
        Classes         m_CreateClass;

        //

        Properties      m_properties;
        CDSObjects      m_children;
        uint32_t        m_nChildCount          {0};
        uint32_t        m_nChildContainerCount {0};

        Resources       m_resources;


    public:

        explicit      CDSObject( const QString &sId = "-1",
                                 const QString &sTitle = "",
                                 const QString &sParentId = "-1" );
        ~CDSObject() override;

        Property         *AddProperty( Property *pProp  );
        QList<Property*>  GetProperties( const QString &sName );
        CDSObject        *AddChild   ( CDSObject   *pChild );
        CDSObjects        GetChildren( void ) const { return m_children; }
        CDSObject        *GetChild   ( const QString &sID );

        ContainerClass *AddSearchClass( ContainerClass *pClass );
        ContainerClass *AddCreateClass( ContainerClass *pClass );

        void          SetPropValue( const QString &sName, const QString &sValue,
                                    const QString &type = "" );
        QString       GetPropValue( const QString &sName ) const;
        QString       toXml      ( FilterMap &filter,
                                   bool ignoreChildren = false ) const;
        void          toXml      ( QTextStream &os, FilterMap &filter,
                                   bool ignoreChildren = false ) const;

        uint32_t      GetChildCount( void ) const;
        void          SetChildCount( uint32_t nCount );

        uint32_t      GetChildContainerCount( void ) const;
        void          SetChildContainerCount( uint32_t nCount );

        Resource     *AddResource( const QString& sProtocol, const QString& sURI );

    public:

        static  CDSObject *CreateItem             ( const QString& sId, const QString& sTitle, const QString& sParentId, CDSObject *pObject = nullptr );
        static  CDSObject *CreateContainer        ( const QString& sId, const QString& sTitle, const QString& sParentId, CDSObject *pObject = nullptr );
        static  CDSObject *CreateAudioItem        ( const QString& sId, const QString& sTitle, const QString& sParentId, CDSObject *pObject = nullptr );
        static  CDSObject *CreateMusicTrack       ( const QString& sId, const QString& sTitle, const QString& sParentId, CDSObject *pObject = nullptr );
        static  CDSObject *CreateAudioBroadcast   ( const QString& sId, const QString& sTitle, const QString& sParentId, CDSObject *pObject = nullptr );
        static  CDSObject *CreateAudioBook        ( const QString& sId, const QString& sTitle, const QString& sParentId, CDSObject *pObject = nullptr );
        static  CDSObject *CreateVideoItem        ( const QString& sId, const QString& sTitle, const QString& sParentId, CDSObject *pObject = nullptr );
        static  CDSObject *CreateMovie            ( const QString& sId, const QString& sTitle, const QString& sParentId, CDSObject *pObject = nullptr );
        static  CDSObject *CreateVideoBroadcast   ( const QString& sId, const QString& sTitle, const QString& sParentId, CDSObject *pObject = nullptr );
        static  CDSObject *CreateMusicVideoClip   ( const QString& sId, const QString& sTitle, const QString& sParentId, CDSObject *pObject = nullptr );
        static  CDSObject *CreateImageItem        ( const QString& sId, const QString& sTitle, const QString& sParentId, CDSObject *pObject = nullptr );
        static  CDSObject *CreatePhoto            ( const QString& sId, const QString& sTitle, const QString& sParentId, CDSObject *pObject = nullptr );
        static  CDSObject *CreatePlaylistItem     ( const QString& sId, const QString& sTitle, const QString& sParentId, CDSObject *pObject = nullptr );
        static  CDSObject *CreateTextItem         ( const QString& sId, const QString& sTitle, const QString& sParentId, CDSObject *pObject = nullptr );
        static  CDSObject *CreateAlbum            ( const QString& sId, const QString& sTitle, const QString& sParentId, CDSObject *pObject = nullptr );
        static  CDSObject *CreateMusicAlbum       ( const QString& sId, const QString& sTitle, const QString& sParentId, CDSObject *pObject = nullptr );
        static  CDSObject *CreatePhotoAlbum       ( const QString& sId, const QString& sTitle, const QString& sParentId, CDSObject *pObject = nullptr );
        static  CDSObject *CreateGenre            ( const QString& sId, const QString& sTitle, const QString& sParentId, CDSObject *pObject = nullptr );
        static  CDSObject *CreateMusicGenre       ( const QString& sId, const QString& sTitle, const QString& sParentId, CDSObject *pObject = nullptr );
        static  CDSObject *CreateMovieGenre       ( const QString& sId, const QString& sTitle, const QString& sParentId, CDSObject *pObject = nullptr );
        static  CDSObject *CreatePlaylistContainer( const QString& sId, const QString& sTitle, const QString& sParentId, CDSObject *pObject = nullptr );
        static  CDSObject *CreatePerson           ( const QString& sId, const QString& sTitle, const QString& sParentId, CDSObject *pObject = nullptr );
        static  CDSObject *CreateMusicArtist      ( const QString& sId, const QString& sTitle, const QString& sParentId, CDSObject *pObject = nullptr );
        static  CDSObject *CreateStorageSystem    ( const QString& sId, const QString& sTitle, const QString& sParentId, CDSObject *pObject = nullptr );
        static  CDSObject *CreateStorageVolume    ( const QString& sId, const QString& sTitle, const QString& sParentId, CDSObject *pObject = nullptr );
        static  CDSObject *CreateStorageFolder    ( const QString& sId, const QString& sTitle, const QString& sParentId, CDSObject *pObject = nullptr );

    private:
        static bool FilterContains( const FilterMap &filter, const QString &name ) ;

};

#endif // UPNPCDSOBJECTS_H
