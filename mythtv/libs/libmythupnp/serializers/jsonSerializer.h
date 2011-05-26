//////////////////////////////////////////////////////////////////////////////
// Program Name: jsonSerializer.h
// Created     : Nov. 28, 2009
//
// Purpose     : Serialization Implementation for JSON
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

#ifndef __JSONSERIALIZER_H__
#define __JSONSERIALIZER_H__

#include <QTextStream>
#include <QStringList>
#include <QVariantMap>

#include "upnpexp.h"
#include "serializer.h"

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
//
//
//
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

class UPNP_PUBLIC JSONSerializer : public Serializer
{

    protected:

        QTextStream   m_Stream;
        bool          m_bCommaNeeded;

        virtual void BeginSerialize( QString &sName );
        virtual void EndSerialize  ();

        virtual void BeginObject( const QString &sName, const QObject  *pObject );
        virtual void EndObject  ( const QString &sName, const QObject  *pObject );

        virtual void AddProperty( const QString       &sName, 
                                  const QVariant      &vValue,
                                  const QMetaObject   *pMetaParent,
                                  const QMetaProperty *pMetaProp );


        void RenderValue     ( const QVariant     &vValue );

        void RenderStringList( const QStringList  &list );
        void RenderList      ( const QVariantList &list );
        void RenderMap       ( const QVariantMap  &map  );

        QString Encode       ( const QString &sIn );

    public:

                 JSONSerializer( QIODevice *pDevice, const QString &sRequestName );
        virtual ~JSONSerializer();

        virtual QString GetContentType();

};

#endif
