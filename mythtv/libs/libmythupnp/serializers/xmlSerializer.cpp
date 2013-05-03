//////////////////////////////////////////////////////////////////////////////
// Program Name: xmlSerializer.cpp
// Created     : Dec. 30, 2009
//
// Purpose     : Serialization Implementation for XML 
//                                                                            
// Copyright (c) 2005 David Blain <dblain@mythtv.org>
//                                          
// Licensed under the GPL v2 or later, see COPYING for details                    
//
//////////////////////////////////////////////////////////////////////////////

#include "xmlSerializer.h"
#include "mythdate.h"

#include <QMetaClassInfo>

// --------------------------------------------------------------------------
// This version should be bumped if the serializer code is changed in a way
// that changes the schema layout of the rendered XML.
// --------------------------------------------------------------------------

#define XML_SERIALIZER_VERSION "1.1"

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////

XmlSerializer::XmlSerializer( QIODevice *pDevice, const QString &sRequestName )
              : m_bIsRoot( true ), PropertiesAsAttributes( true )
{
    m_pXmlWriter   = new QXmlStreamWriter( pDevice );
    m_sRequestName = sRequestName;
}

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////

XmlSerializer::~XmlSerializer()
{
    if (m_pXmlWriter != NULL)
    {
        delete m_pXmlWriter;
        m_pXmlWriter = NULL;
    }
}

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////

QString XmlSerializer::GetContentType()
{
    return "text/xml";        
}

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////

void XmlSerializer::BeginSerialize( QString &sName ) 
{
    m_pXmlWriter->writeStartDocument( "1.0" );
//    m_pXmlWriter->writeStartElement( m_sRequestName + "Response" );
}

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////

void XmlSerializer::EndSerialize()
{
    m_pXmlWriter->writeEndDocument();
}

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////

void XmlSerializer::BeginObject( const QString &sName, const QObject  *pObject )
{
    m_pXmlWriter->writeStartElement( sName );

    if (m_bIsRoot)
    {
        m_pXmlWriter->writeAttribute( "xmlns:xsi", "http://www.w3.org/2001/XMLSchema-instance" );

        m_bIsRoot = false;
    }

    const QMetaObject *pMeta = pObject->metaObject();

    int nIdx = pMeta->indexOfClassInfo( "version" );

    if (nIdx >=0)
        m_pXmlWriter->writeAttribute( "version", pMeta->classInfo( nIdx ).value() );

    m_pXmlWriter->writeAttribute( "serializerVersion", XML_SERIALIZER_VERSION );

}

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////

void XmlSerializer::EndObject  ( const QString &sName, const QObject  *pObject )
{
    m_pXmlWriter->writeEndElement();
}

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////

void XmlSerializer::AddProperty( const QString       &sName, 
                                 const QVariant      &vValue,
                                 const QMetaObject   *pMetaParent,
                                 const QMetaProperty *pMetaProp )
{
    m_pXmlWriter->writeStartElement( sName );
    RenderValue( GetContentName( sName, pMetaParent, pMetaProp ), vValue );
    m_pXmlWriter->writeEndElement();
}

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////

void XmlSerializer::RenderValue( const QString &sName, const QVariant &vValue )
{

    // -----------------------------------------------------------------------
    // See if this value is actually a QObject
    // -----------------------------------------------------------------------

    if ( vValue.canConvert< QObject* >()) 
    { 
        const QObject *pObject = vValue.value< QObject* >(); 

        SerializeObjectProperties( pObject );
        return;
    }

    // -----------------------------------------------------------------------
    // Handle QVariant special cases...
    // -----------------------------------------------------------------------

    switch( vValue.type() )
    {
        case QVariant::List:
        {
            RenderList( sName, vValue.toList() );
            break;
        }

        case QVariant::StringList:
        {
            RenderStringList( sName, vValue.toStringList() );
            break;
        }

        case QVariant::Map:
        {
            RenderMap( sName, vValue.toMap() );
            break;
        }

        case QVariant::DateTime:
        {
            QDateTime dt( vValue.toDateTime() );

            if (dt.isNull())
                m_pXmlWriter->writeAttribute( "xsi:nil", "true" );

            m_pXmlWriter->writeCharacters( 
                MythDate::toString( dt, MythDate::ISODate ) );

            break;
        }

        default:
        {
            m_pXmlWriter->writeCharacters( vValue.toString() );
            break;
        }
    }

}

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////

void XmlSerializer::RenderList( const QString &sName, const QVariantList &list )
{
//    QString sItemName;

    QListIterator< QVariant > it( list );

    while (it.hasNext())
    {
        QVariant vValue = it.next();

//        if (sItemName.isEmpty())
//            sItemName = GetItemName( QMetaType::typeName( vValue.userType() ) );

        m_pXmlWriter->writeStartElement( sName );
        RenderValue( sName, vValue );
        m_pXmlWriter->writeEndElement();
    }
}

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////

void XmlSerializer::RenderStringList( const QString &sName, const QStringList &list )
{
    QString sItemName = GetItemName( sName );

    QListIterator< QString > it( list );

    while (it.hasNext())
    {
        m_pXmlWriter->writeStartElement( "String" );
        m_pXmlWriter->writeCharacters ( it.next() );
        m_pXmlWriter->writeEndElement();
    }
}

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////

void XmlSerializer::RenderMap( const QString &sName, const QVariantMap &map )
{

    QMapIterator< QString, QVariant > it( map );

    QString sItemName = GetItemName( sName );

    while (it.hasNext()) 
    {
        it.next();

        m_pXmlWriter->writeStartElement( sItemName );

        m_pXmlWriter->writeStartElement( "Key" );
        m_pXmlWriter->writeCharacters( it.key() );
        m_pXmlWriter->writeEndElement();

        m_pXmlWriter->writeStartElement( "Value" );
        RenderValue( sItemName, it.value() );
        m_pXmlWriter->writeEndElement();

/*
        m_pXmlWriter->writeAttribute   ( "key", it.key() );
        RenderValue( sItemName, it.value() );
        //m_pXmlWriter->writeCharacters  ( it.value().toString() );
*/
        m_pXmlWriter->writeEndElement();
    }
}

//////////////////////////////////////////////////////////////////////////////
// -=>TODO: There should be a better way to handle this... 
//          boy do I miss C#'s Custom Attributes... maybe leverage MOC somehow
//////////////////////////////////////////////////////////////////////////////

QString XmlSerializer::GetItemName( const QString &sName )
{
    QString sTypeName( sName );

    if (sName.at(0) == 'Q')
        sTypeName = sName.mid( 1 );

    sTypeName.remove( "DTC::"    );
    sTypeName.remove( QChar('*') );

    return sTypeName;
}

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////

QString XmlSerializer::GetContentName( const QString        &sName, 
                                       const QMetaObject   *pMetaObject,
                                       const QMetaProperty *pMetaProp )
{
    // Try to read Name or TypeName from classinfo metadata.

    int nClassIdx = pMetaObject->indexOfClassInfo( sName.toLatin1() );

    if (nClassIdx >=0)
    {
        QString     sOptionData = pMetaObject->classInfo( nClassIdx ).value();
        QStringList sOptions    = sOptionData.split( ';' );
        
        QString sNameOption = FindOptionValue( sOptions, "name" );

        if (sNameOption.isEmpty())
            sNameOption = FindOptionValue( sOptions, "type" );

        if (!sNameOption.isEmpty())
            return GetItemName(  sNameOption );
    }

    // Neither found, so lets use the type name (slightly modified).

    QString sTypeName( sName );    

    if (sName.at(0) == 'Q')        
        sTypeName = sName.mid( 1 );    

    sTypeName.remove( "DTC::"    );    
    sTypeName.remove( QChar('*') );

    return sTypeName;
}

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////

QString XmlSerializer::FindOptionValue( const QStringList &sOptions, const QString &sName )
{
    QString sKey = sName + "=";

    for (int nIdx = 0; nIdx < sOptions.size(); ++nIdx)
    {
        if (sOptions.at( nIdx ).startsWith( sKey ))
            return sOptions.at( nIdx ).mid( sKey.length() );
    }

    return QString();
}
