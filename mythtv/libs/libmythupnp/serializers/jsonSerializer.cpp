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
#include <QRegularExpression>

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

void JSONSerializer::BeginSerialize( QString &/*sName*/ )
{
    m_bCommaNeeded = false;

     m_stream << "{";
}

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////

void JSONSerializer::EndSerialize()
{
    m_bCommaNeeded = false;
    
    m_stream << "}";

    m_stream.flush();
}


//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////

void JSONSerializer::BeginObject( const QString &sName, const QObject  */*pObject*/ )
{
    m_bCommaNeeded = false;
    
    m_stream << "\"" << sName << "\": {";
}

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////

void JSONSerializer::EndObject  ( const QString &/*sName*/, const QObject  */*pObject*/ )
{
    m_bCommaNeeded = false;
    
    m_stream << "}";
}

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////

void JSONSerializer::AddProperty( const QString       &sName, 
                                  const QVariant      &vValue,
                                  const QMetaObject   */*pMetaParent*/,
                                  const QMetaProperty */*pMetaProp*/ )
{
    if (m_bCommaNeeded)
        m_stream << ", ";

    m_stream << "\"" << sName << "\": ";

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

        m_stream << "{";
        SerializeObjectProperties( pObject );
        m_stream << "}";

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
            m_stream << "\"" << Encode( 
                MythDate::toString( vValue.toDateTime(), MythDate::ISODate ) )
                << "\"";
            break;
        }
        default:
        {
            m_stream << "\"" << Encode( vValue.toString() ) << "\"";
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

    m_stream << "[";

    QListIterator< QVariant > it( list );

    while (it.hasNext())
    {
        if (bFirst)
            bFirst = false;
        else
            m_stream << ",";

        RenderValue( it.next() );
    }

    m_stream << "]";
}

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////

void JSONSerializer::RenderStringList( const QStringList &list )
{
    bool bFirst = true;

    m_stream << "[";

    QListIterator< QString > it( list );

    while (it.hasNext())
    {
        if (bFirst)
            bFirst = false;
        else
            m_stream << ",";

          m_stream << "\"" << Encode( it.next() ) << "\"";
    }

    m_stream << "]";
}

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////

void JSONSerializer::RenderMap( const QVariantMap &map )
{
    bool bFirst = true;

    m_stream << "{";

    QMapIterator< QString, QVariant > it( map );

    while (it.hasNext()) 
    {
        it.next();
        
        if (bFirst)
            bFirst = false;
        else
            m_stream << ",";

        m_stream << "\"" << it.key() << "\":";
        m_stream << "\"" << Encode( it.value().toString() ) << "\"";
    }

    m_stream << "}";
}

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////

QString JSONSerializer::Encode(const QString &sIn)
{
    if (sIn.isEmpty())
        return sIn;

    QString sStr = sIn;

    sStr.replace( '\\', "\\\\" ); // This must be first
    sStr.replace( '"' , "\\\"" ); 
    sStr.replace(  "/", "\\/"  );

    // Officially we need to handle \u0000 - \u001F, but only a limited
    // number are actually used in the wild.
    QRegularExpression control_chars("([\b\f\n\r\t])");
    sStr.replace(control_chars, "\\\\1");
    sStr.replace(QChar('\u0011'), "\\u0011"); // XON  ^Q
    sStr.replace(QChar('\u0013'), "\\u0013"); // XOFF ^S

    return sStr;
}
