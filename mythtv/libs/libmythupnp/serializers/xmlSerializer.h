//////////////////////////////////////////////////////////////////////////////
// Program Name: xmlSerializer.h
// Created     : Dec. 30, 2009
//
// Purpose     : Serialization Implementation for XML 
//                                                                            
// Copyright (c) 2005 David Blain <dblain@mythtv.org>
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

#ifndef __XMLSERIALIZER_H__
#define __XMLSERIALIZER_H__

#include <QXmlStreamWriter>
#include <QVariant>
#include <QIODevice>
#include <QStringList>

#include "upnpexp.h"
#include "serializer.h"

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
//
//
//
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

class UPNP_PUBLIC XmlSerializer : public Serializer
{

    protected:

        QXmlStreamWriter *m_pXmlWriter;
        QString           m_sRequestName;

        virtual void BeginSerialize( QString &sName );
        virtual void EndSerialize  ();

        virtual void BeginObject( const QString &sName, const QObject  *pObject );
        virtual void EndObject  ( const QString &sName, const QObject  *pObject );

        virtual void AddProperty( const QString       &sName, 
                                  const QVariant      &vValue,
                                  const QMetaObject   *pMetaParent,
                                  const QMetaProperty *pMetaProp );

        void    RenderValue     ( const QString &sName, const QVariant     &vValue );

        void    RenderStringList( const QString &sName, const QStringList  &list );
        void    RenderList      ( const QString &sName, const QVariantList &list );
        void    RenderMap       ( const QString &sName, const QVariantMap  &map  );

        QString GetItemName     ( const QString &sName );

        QString GetContentName  ( const QString        &sName, 
                                  const QMetaObject   *pMetaObject,
                                  const QMetaProperty *pMetaProp );

    public:

        bool     PropertiesAsAttributes;

                 XmlSerializer( QIODevice *pDevice, const QString &sRequestName );
        virtual ~XmlSerializer();

        virtual QString GetContentType();

};

#endif
