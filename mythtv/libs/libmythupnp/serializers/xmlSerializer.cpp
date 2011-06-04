//////////////////////////////////////////////////////////////////////////////
// Program Name: xmlSerializer.cpp
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

#include "xmlSerializer.h"

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
              : PropertiesAsAttributes( true )
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
    if (sName != "Description")
    {
        m_pXmlWriter->writeStartElement( sName );

        RenderValue( GetContentName( sName, pMetaParent, pMetaProp ), vValue );

        m_pXmlWriter->writeEndElement();
    }
    else
        RenderValue( sName, vValue );
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
        m_pXmlWriter->writeAttribute   ( "key", it.key() );
        m_pXmlWriter->writeCharacters  ( it.value().toString() );
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
    QString sMethodClassInfo = sName + "_type";

    int nClassIdx = pMetaObject->indexOfClassInfo( sMethodClassInfo.toAscii() );

    if (nClassIdx >=0)
        return GetItemName( pMetaObject->classInfo( nClassIdx ).value() );

    return sName;
}
