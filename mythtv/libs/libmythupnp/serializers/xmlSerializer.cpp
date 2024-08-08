/////////////////////////////////////////////////////////////////////////////
// Program Name: xmlSerializer.cpp
// Created     : Dec. 30, 2009
//
// Purpose     : Serialization Implementation for XML 
//                                                                            
// Copyright (c) 2005 David Blain <dblain@mythtv.org>
//                                          
// Licensed under the GPL v2 or later, see LICENSE for details
//
//////////////////////////////////////////////////////////////////////////////

#include "xmlSerializer.h"
#include "libmythbase/mythdate.h"

#include <utility>

#include <QMetaClassInfo>

// --------------------------------------------------------------------------
// This version should be bumped if the serializer code is changed in a way
// that changes the schema layout of the rendered XML.
// --------------------------------------------------------------------------

static constexpr const char* XML_SERIALIZER_VERSION { "1.1" };

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////

XmlSerializer::XmlSerializer( QIODevice *pDevice, QString sRequestName )
  : m_pXmlWriter(new QXmlStreamWriter( pDevice )),
    m_sRequestName(std::move(sRequestName))
{
}

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////

XmlSerializer::~XmlSerializer()
{
    if (m_pXmlWriter != nullptr)
    {
        delete m_pXmlWriter;
        m_pXmlWriter = nullptr;
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

void XmlSerializer::BeginSerialize( QString &/*sName*/ )
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

    int nIdx = -1;

    if (pMeta)
        nIdx = pMeta->indexOfClassInfo( "version" );

    if (nIdx >=0)
        m_pXmlWriter->writeAttribute( "version", pMeta->classInfo( nIdx ).value() );

    m_pXmlWriter->writeAttribute( "serializerVersion", XML_SERIALIZER_VERSION );

}

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////

void XmlSerializer::EndObject  ( const QString &/*sName*/, const QObject  */*pObject*/ )
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

    if ((pMetaProp != nullptr) &&
        (pMetaProp->isEnumType() || pMetaProp->isFlagType()))
    {
        RenderEnum ( sName, vValue, pMetaProp );
    }
    else
    {
        RenderValue( GetContentName( sName, pMetaParent, pMetaProp ), vValue );
    }

    m_pXmlWriter->writeEndElement();
}

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////

void XmlSerializer::RenderEnum( const QString       &/*sName*/ ,
                                const QVariant      &vValue,
                                const QMetaProperty *pMetaProp )
{
    QString   sValue;
    QMetaEnum metaEnum = pMetaProp->enumerator();

    if (pMetaProp->isFlagType())
        sValue = metaEnum.valueToKeys( vValue.toInt() );
    else
        sValue = metaEnum.valueToKey ( vValue.toInt() );

    // If couldn't convert to enum name, return raw value

    if (sValue.isEmpty())
        sValue = vValue.toString();

    m_pXmlWriter->writeCharacters( sValue );

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

#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
    auto type = static_cast<QMetaType::Type>(vValue.type());
#else
    auto type = vValue.typeId();
#endif
    switch( type )
    {
        case QMetaType::QVariantList:
        {
            RenderList( sName, vValue.toList() );
            break;
        }

        case QMetaType::QStringList:
        {
            RenderStringList( sName, vValue.toStringList() );
            break;
        }

        case QMetaType::QVariantMap:
        {
            RenderMap( sName, vValue.toMap() );
            break;
        }

        case QMetaType::QDateTime:
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

void XmlSerializer::RenderStringList( const QString &/*sName*/, const QStringList &list )
{
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

    if ((sName.length() > 0) && (sName.at(0) == 'Q'))
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
                                       const QMetaProperty */*pMetaProp*/ )
{
    // Try to read Name or TypeName from classinfo metadata.

    int nClassIdx = -1;

    if ( pMetaObject )
        nClassIdx = pMetaObject->indexOfClassInfo( sName.toLatin1() );

    if (nClassIdx >=0 )
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

    if ((sName.length() > 0) && (sName.at(0) == 'Q'))
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

    auto hasKey = [&sKey](const QString& o) { return o.startsWith( sKey ); };
    auto it = std::find_if(sOptions.cbegin(), sOptions.cend(), hasKey);
    if (it != sOptions.cend())
        return (*it).mid( sKey.length() );

    return {};
}
