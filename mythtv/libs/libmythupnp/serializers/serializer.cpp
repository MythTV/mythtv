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
    
    headers[ "ETag" ] = "\"" + m_hash.result().toHex() + "\"";

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

        if ((sName.length() > 0) && (sName.at(0) == 'Q'))
            sName = sName.mid( 1 );
    }

    // ---------------------------------------------------------------

    m_hash.reset();

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

    if ((sName.length() > 0) && sName.at(0) == 'Q')
        sName = sName.mid( 1 );

    m_hash.reset();

    BeginSerialize( sName );

    AddProperty( sName, vValue, NULL, NULL );

    EndSerialize();
}

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////

void Serializer::SerializeObject( const QObject *pObject, const QString &sName )
{
    m_hash.addData( sName.toUtf8() );

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
                
                bool bHash = false;

                if (ReadPropertyMetadata( pObject, 
                                          sPropName, 
                                          "transient").toLower() != "true" )
                {
                    bHash = true;
                    m_hash.addData( sPropName.toUtf8() );
                }

                QVariant value( pObject->property( pszPropName ) );

                if (bHash && !value.canConvert< QObject* >()) 
                {
                    m_hash.addData( value.toString().toUtf8() );
                }

                AddProperty( sPropName, value, pMetaObject, &metaProperty );
            }
        }
    }
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

QString Serializer::ReadPropertyMetadata( const QObject *pObject, 
                                                QString  sPropName, 
                                                QString  sKey )
{
    const QMetaObject *pMeta = pObject->metaObject();

    int nIdx = pMeta->indexOfClassInfo( sPropName.toUtf8() );

    if (nIdx >=0)
    {
        QString     sMetadata = pMeta->classInfo( nIdx ).value();
        QStringList sOptions  = sMetadata.split( ';' );

        QString     sFullKey  = sKey + "=";

        for (int nIdx = 0; nIdx < sOptions.size(); ++nIdx)
        {
            if (sOptions.at( nIdx ).startsWith( sFullKey ))
                return sOptions.at( nIdx ).mid( sFullKey.length() );
        }
    }

    return QString();
}


