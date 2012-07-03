//////////////////////////////////////////////////////////////////////////////
// Program Name: jsonSerializer.cpp
// Created     : Nov. 28, 2009
//
// Purpose     : Serialization Implementation for JSON 
//                                                                            
// Copyright (c) 2005 David Blain <dblain@mythtv.org>
//                                          
// Licensed under the GPL v2 or later, see COPYING for details                    
//
//////////////////////////////////////////////////////////////////////////////

#include "jsonSerializer.h"
#include "mythdate.h"

#include <QTextCodec>
#include <QVariant>

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////

JSONSerializer::JSONSerializer( QIODevice *pDevice, const QString &sRequestName )
               : m_Stream( pDevice ), m_bCommaNeeded( false )
{
}

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////

JSONSerializer::~JSONSerializer()
{
}

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////

QString JSONSerializer::GetContentType()
{
    return "application/json";
}


//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////

void JSONSerializer::BeginSerialize( QString &sName )
{
    m_bCommaNeeded = false;

    m_Stream << "{";
}

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////

void JSONSerializer::EndSerialize()
{
    m_bCommaNeeded = false;
    
    m_Stream << "}";

    m_Stream.flush();
}


//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////

void JSONSerializer::BeginObject( const QString &sName, const QObject  *pObject )
{
    m_bCommaNeeded = false;
    
    m_Stream << "\"" << sName << "\": {";
}

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////

void JSONSerializer::EndObject  ( const QString &sName, const QObject  *pObject )
{
    m_bCommaNeeded = false;
    
    m_Stream << "}";
}

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////

void JSONSerializer::AddProperty( const QString       &sName, 
                                  const QVariant      &vValue,
                                  const QMetaObject   *pMetaParent,
                                  const QMetaProperty *pMetaProp )
{
    if (m_bCommaNeeded)
        m_Stream << ", ";

    m_Stream << "\"" << sName << "\": ";

    RenderValue( vValue );

    m_bCommaNeeded = true;
}

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////

void JSONSerializer::RenderValue( const QVariant &vValue )
{
    
    // -----------------------------------------------------------------------
    // See if this value is actually a child object
    // -----------------------------------------------------------------------

    if ( vValue.canConvert< QObject* >()) 
    { 
        const QObject *pObject = vValue.value< QObject* >(); 

        bool bSavedCommaNeeded = m_bCommaNeeded;
        m_bCommaNeeded = false;

        m_Stream << "{";
        SerializeObjectProperties( pObject );
        m_Stream << "}";

        m_bCommaNeeded = bSavedCommaNeeded;

        return;
    }
    
    // -----------------------------------------------------------------------
    // Handle QVariant special cases...
    // -----------------------------------------------------------------------

    switch( vValue.type() )
    {
        case QVariant::List:        RenderList      ( vValue.toList()       );  break;
        case QVariant::StringList:  RenderStringList( vValue.toStringList() );  break;
        case QVariant::Map:         RenderMap       ( vValue.toMap()        );  break;
        case QVariant::DateTime:
        {
            m_Stream << "\"" << Encode( 
                MythDate::toString( vValue.toDateTime(), MythDate::ISODate ) )
                << "\"";
            break;
        }
        default:
        {
            m_Stream << "\"" << Encode( vValue.toString() ) << "\"";
            break;
        }
    }
}

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////

void JSONSerializer::RenderList( const QVariantList &list )
{
    bool bFirst = true;

    m_Stream << "[";

    QListIterator< QVariant > it( list );

    while (it.hasNext())
    {
        if (bFirst)
            bFirst = false;
        else
            m_Stream << ",";

        RenderValue( it.next() );
    }

    m_Stream << "]";
}

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////

void JSONSerializer::RenderStringList( const QStringList &list )
{
    bool bFirst = true;

    m_Stream << "[";

    QListIterator< QString > it( list );

    while (it.hasNext())
    {
        if (bFirst)
            bFirst = false;
        else
            m_Stream << ",";

          m_Stream << "\"" << Encode( it.next() ) << "\"";
    }

    m_Stream << "]";
}

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////

void JSONSerializer::RenderMap( const QVariantMap &map )
{
    bool bFirst = true;

    m_Stream << "{";

    QMapIterator< QString, QVariant > it( map );

    while (it.hasNext()) 
    {
        it.next();
        
        if (bFirst)
            bFirst = false;
        else
            m_Stream << ",";

        m_Stream << "\"" << it.key() << "\":";
        m_Stream << "\"" << Encode( it.value().toString() ) << "\"";
    }

    m_Stream << "}";
}

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////

QString JSONSerializer::Encode(const QString &sIn)
{
    if (sIn.isEmpty())
        return sIn;

    QString sStr = sIn;

    // -=>TODO: Would it be better to just loop through string once and build 
    // new string with encoded chars instead of calling replace multiple times?
    // It might perform better.

    sStr.replace( '\\', "\\\\" ); // This must be first
    sStr.replace( '"' , "\\\"" ); 

    sStr.replace( '\b', "\\b"  );
    sStr.replace( '\f', "\\f"  );
    sStr.replace( '\n', "\\n"  );
    sStr.replace( "\r", "\\r"  );
    sStr.replace( "\t", "\\t"  );
    sStr.replace(  "/", "\\/"  );

    // we don't handle hex values yet...
    /*
    if(ch>='\u0000' && ch<='\u001F')
        sb.append("\\u####"); 
    */
    return sStr;
}
