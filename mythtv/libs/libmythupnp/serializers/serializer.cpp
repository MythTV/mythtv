//////////////////////////////////////////////////////////////////////////////
// Program Name: serializer.cpp
// Created     : Dec. 30, 2009
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

#include "serializer.h"

#include <QMetaObject>
#include <QMetaProperty>

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////

void Serializer::AddHeaders( QStringMap &headers )
{
    headers[ "Cache-Control" ] = "no-cache=\"Ext\", "
                                 "max-age = 5000";
}

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////

void Serializer::Serialize( const QObject *pObject, const QString &_sName )
{
    QString sName = _sName;

    if (sName.isEmpty())
        sName = pObject->objectName();

    if (sName.isEmpty())
    {
        sName = pObject->metaObject()->className();

        sName = sName.section( ":", -1 );
    }

    // ---------------------------------------------------------------

    BeginSerialize();

    SerializeObject( pObject, sName );

    EndSerialize();
}

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////

void Serializer::Serialize( const QVariant &vValue, const QString &sName )
{
    BeginSerialize();

    AddProperty( sName, vValue );

    EndSerialize();
}

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////

void Serializer::SerializeObject( const QObject *pObject, const QString &sName )
{
    BeginObject( sName, pObject );

    SerializeObjectProperties( pObject );

    EndObject( sName, pObject );
}

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////

void Serializer::SerializeObjectProperties( const QObject *pObject )
{
    if (pObject != NULL)
    {
        const QMetaObject *pMetaObject = pObject->metaObject();

        int nCount = pMetaObject->propertyCount();

        for (int nIdx=0; nIdx < nCount; ++nIdx ) 
        {
            QMetaProperty metaProperty = pMetaObject->property( nIdx );

            if (metaProperty.isDesignable( pObject ))
            {
                const char *pszPropName = metaProperty.name();
                QString     sPropName( pszPropName );

                if ( sPropName.compare( "objectName" ) == 0)
                    continue;

                QVariant value( pObject->property( pszPropName ) );

                AddProperty( sPropName, value );
            }
        }
    }
}

