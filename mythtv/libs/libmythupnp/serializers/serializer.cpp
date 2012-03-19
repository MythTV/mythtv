//////////////////////////////////////////////////////////////////////////////
// Program Name: serializer.cpp
// Created     : Dec. 30, 2009
//
// Purpose     : Serialization Abstract Class 
//                                                                            
// Copyright (c) 2005 David Blain <dblain@mythtv.org>
//                                          
// Licensed under the GPL v2 or later, see COPYING for details                    
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

        if (sName.at(0) == 'Q')
            sName = sName.mid( 1 );
    }

    // ---------------------------------------------------------------

    BeginSerialize( sName );

    SerializeObject( pObject, sName );

    EndSerialize();
}

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////

void Serializer::Serialize( const QVariant &vValue, const QString &_sName )
{
    QString sName( _sName );

    if (sName.at(0) == 'Q')
        sName = sName.mid( 1 );

    BeginSerialize( sName );

    AddProperty( sName, vValue, NULL, NULL );

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

                AddProperty( sPropName, value, pMetaObject, &metaProperty );
            }
        }
    }
}

