//////////////////////////////////////////////////////////////////////////////
// Program Name: UPnpCDSObjects.h
// Created     : Oct. 24, 2005
//
// Purpose     : uPnp Content Directory Service Object Definitions
//
// Copyright (c) 2005 David Blain <mythtv@theblains.net>
//
// This library is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; either
// version 2.1 of the License, or at your option any later version of the LGPL.
//
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public
// License along with this library.  If not, see <http://www.gnu.org/licenses/>.
//
//////////////////////////////////////////////////////////////////////////////

#ifndef __UPNPCDSOBJECTS_H_
#define __UPNPCDSOBJECTS_H_

#include <QDateTime>
#include <QString>
#include <QList>
#include <QMap>

#include "upnpexp.h"

class CDSObject;
class QTextStream;

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////

typedef enum
{
    OT_Undefined  = 0,
    OT_Container  = 1,
    OT_Item       = 2,
    OT_Res        = 3

} ObjectTypes;

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////

class Property
{
    public:

        QString  m_sName;
        QString  m_sNameSpace;
        bool     m_bRequired;
        QString  m_sValue;
        NameValues      m_lstAttributes;

    public:

        explicit Property( const QString &sName,
                           const QString &sNameSpace = "",
                           bool           bRequired  = false,
                           const QString &sValue     = "" )
        {
            m_sName      = sName;
            m_sNameSpace = sNameSpace;
            m_bRequired  = bRequired;
            m_sValue     = sValue;
        }

        void AddAttribute( const QString &sName,
                           const QString &sValue )
        {
            m_lstAttributes.push_back(NameValue(sName, sValue));
        }
};

typedef QMap<QString,Property*> Properties;
typedef QList<CDSObject*>       CDSObjects;

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

        Resource( const QString &sProtocolInfo,
                  const QString &sURI )
        {
            m_sProtocolInfo = sProtocolInfo;
            m_sURI          = sURI;
        }

        void AddAttribute( const QString &sName,
                           const QString &sValue )
        {
            m_lstAttributes.push_back(NameValue(sName, sValue));
        }
};

typedef QList<Resource*> Resources;

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

        ContainerClass( const QString &sClass,
                        const QString &sName,
                        bool           bIncludeDerived )
        {
            m_sClass          = sClass;
            m_sName           = sName;
            m_bIncludeDerived = bIncludeDerived;
        }
};

typedef QList<ContainerClass*> Classes;

typedef QStringList FilterMap;

//////////////////////////////////////////////////////////////////////////////

class UPNP_PUBLIC CDSObject
{
    public:
        short           m_nUpdateId;

        ObjectTypes     m_eType;

        // Required

        QString         m_sId;
        QString         m_sParentId;
        QString         m_sTitle;
        QString         m_sClass;
        bool            m_bRestricted;
        bool            m_bSearchable;

        // Optional

        QString         m_sCreator;
        QString         m_sWriteStatus;

        // Only appropriate for Container Classes

        Classes         m_SearchClass;
        Classes         m_CreateClass;

        //

        Properties      m_properties;
        CDSObjects      m_children;
        long            m_nChildCount;

        Resources       m_resources;


    public:

        explicit      CDSObject( const QString sId = "-1",
                                 const QString sTitle = "",
                                 const QString sParentId = "-1" );
        virtual      ~CDSObject();

        Property     *AddProperty( Property *pProp  );
        Property     *GetProperty( QString sName );
        CDSObject    *AddChild   ( CDSObject   *pChild );

        ContainerClass *AddSearchClass( ContainerClass *pClass );
        ContainerClass *AddCreateClass( ContainerClass *pClass );

        void          SetPropValue( QString sName, QString sValue );
        QString       GetPropValue( const QString &sName ) const;
        QString       toXml      ( FilterMap &filter ) const;
        void          toXml      ( QTextStream &os, FilterMap &filter ) const;

        long          GetChildCount( void ) const;
        void          SetChildCount( long nCount );

        Resource     *AddResource( QString sProtocol, QString sURI );

    public:

        static  CDSObject *CreateItem             ( QString sId, QString sTitle, QString sParentId, CDSObject *pObject = NULL );
        static  CDSObject *CreateContainer        ( QString sId, QString sTitle, QString sParentId, CDSObject *pObject = NULL );
        static  CDSObject *CreateAudioItem        ( QString sId, QString sTitle, QString sParentId, CDSObject *pObject = NULL );
        static  CDSObject *CreateMusicTrack       ( QString sId, QString sTitle, QString sParentId, CDSObject *pObject = NULL );
        static  CDSObject *CreateAudioBroadcast   ( QString sId, QString sTitle, QString sParentId, CDSObject *pObject = NULL );
        static  CDSObject *CreateAudioBook        ( QString sId, QString sTitle, QString sParentId, CDSObject *pObject = NULL );
        static  CDSObject *CreateVideoItem        ( QString sId, QString sTitle, QString sParentId, CDSObject *pObject = NULL );
        static  CDSObject *CreateMovie            ( QString sId, QString sTitle, QString sParentId, CDSObject *pObject = NULL );
        static  CDSObject *CreateVideoBroadcast   ( QString sId, QString sTitle, QString sParentId, CDSObject *pObject = NULL );
        static  CDSObject *CreateMusicVideoClip   ( QString sId, QString sTitle, QString sParentId, CDSObject *pObject = NULL );
        static  CDSObject *CreateImageItem        ( QString sId, QString sTitle, QString sParentId, CDSObject *pObject = NULL );
        static  CDSObject *CreatePhoto            ( QString sId, QString sTitle, QString sParentId, CDSObject *pObject = NULL );
        static  CDSObject *CreatePlaylistItem     ( QString sId, QString sTitle, QString sParentId, CDSObject *pObject = NULL );
        static  CDSObject *CreateTextItem         ( QString sId, QString sTitle, QString sParentId, CDSObject *pObject = NULL );
        static  CDSObject *CreateAlbum            ( QString sId, QString sTitle, QString sParentId, CDSObject *pObject = NULL );
        static  CDSObject *CreateMusicAlbum       ( QString sId, QString sTitle, QString sParentId, CDSObject *pObject = NULL );
        static  CDSObject *CreatePhotoAlbum       ( QString sId, QString sTitle, QString sParentId, CDSObject *pObject = NULL );
        static  CDSObject *CreateGenre            ( QString sId, QString sTitle, QString sParentId, CDSObject *pObject = NULL );
        static  CDSObject *CreateMusicGenre       ( QString sId, QString sTitle, QString sParentId, CDSObject *pObject = NULL );
        static  CDSObject *CreateMovieGenre       ( QString sId, QString sTitle, QString sParentId, CDSObject *pObject = NULL );
        static  CDSObject *CreatePlaylistContainer( QString sId, QString sTitle, QString sParentId, CDSObject *pObject = NULL );
        static  CDSObject *CreatePerson           ( QString sId, QString sTitle, QString sParentId, CDSObject *pObject = NULL );
        static  CDSObject *CreateMusicArtist      ( QString sId, QString sTitle, QString sParentId, CDSObject *pObject = NULL );
        static  CDSObject *CreateStorageSystem    ( QString sId, QString sTitle, QString sParentId, CDSObject *pObject = NULL );
        static  CDSObject *CreateStorageVolume    ( QString sId, QString sTitle, QString sParentId, CDSObject *pObject = NULL );
        static  CDSObject *CreateStorageFolder    ( QString sId, QString sTitle, QString sParentId, CDSObject *pObject = NULL );

};

#endif
