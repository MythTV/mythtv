//////////////////////////////////////////////////////////////////////////////
// Program Name: serializer.h
// Created     : Nov. 28, 2009
//
// Purpose     : Serialization Abstract Class
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

#ifndef __SERIALIZER_H__
#define __SERIALIZER_H__

#include "upnpexp.h"
#include "upnputil.h"

#include <QList>
#include <QMetaType>

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
//
//
//
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

class UPNP_PUBLIC Serializer
{
    protected:

        virtual void BeginSerialize() {}
        virtual void EndSerialize  () {}

        virtual void BeginObject( const QString &sName, const QObject  *pObject ) = 0;
        virtual void EndObject  ( const QString &sName, const QObject  *pObject ) = 0;
        virtual void AddProperty( const QString &sName, const QVariant &vValue  ) = 0;

        //////////////////////////////////////////////////////////////////////

        void SerializeObject          ( const QObject *pObject, const QString &sName );
        void SerializeObjectProperties( const QObject *pObject );

    public:

        void Serialize( const QObject *pObject, const QString &_sName = QString() );
        void Serialize( const QVariant &vValue, const QString &sName );

        //////////////////////////////////////////////////////////////////////
        // Helper Methods
        //////////////////////////////////////////////////////////////////////

        virtual QString GetContentType () = 0;
        virtual void    AddHeaders     ( QStringMap &headers );


        Serializer()
        {
            qRegisterMetaType< QList<QObject*> >("QList<QObject*>");
        }
};

Q_DECLARE_METATYPE( QList<QObject*> )

#endif

