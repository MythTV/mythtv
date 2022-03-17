//////////////////////////////////////////////////////////////////////////////
// Program Name: jsonSerializer.cpp
// Created     : Nov. 28, 2009
//
// Purpose     : Serialization Implementation for JSON 
//                                                                            
// Copyright (c) 2005 David Blain <dblain@mythtv.org>
//                                          
// Licensed under the GPL v2 or later, see LICENSE for details
//
//////////////////////////////////////////////////////////////////////////////

#include "jsonSerializer.h"
#include "libmythbase/mythdate.h"

#include <QVariant>

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

#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
    auto type = static_cast<QMetaType::Type>(vValue.type());
#else
    auto type = vValue.typeId();
#endif
    switch( type )
    {
        case QMetaType::QVariantList: RenderList      ( vValue.toList()       );  break;
        case QMetaType::QStringList:  RenderStringList( vValue.toStringList() );  break;
        case QMetaType::QVariantMap:  RenderMap       ( vValue.toMap()        );  break;
        case QMetaType::QDateTime:
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
    sStr.replace( '\b', "\\b"  ); // ^H (\u0008)
    sStr.replace( '\f', "\\f"  ); // ^L (\u000C)
    sStr.replace( '\n', "\\n"  ); // ^J (\u000A)
    sStr.replace( "\r", "\\r"  ); // ^M (\u000D)
    sStr.replace( "\t", "\\t"  ); // ^I (\u0009)
    sStr.replace(  "/", "\\/"  );

    // Escape remaining chars from \u0000 - \u001F
    // Details at https://en.wikipedia.org/wiki/C0_and_C1_control_codes
    sStr.replace(QChar('\u0000'), "\\u0000"); // ^@ NULL
    sStr.replace(QChar('\u0001'), "\\u0001"); // ^A
    sStr.replace(QChar('\u0002'), "\\u0002"); // ^B
    sStr.replace(QChar('\u0003'), "\\u0003"); // ^C
    sStr.replace(QChar('\u0004'), "\\u0004"); // ^D
    sStr.replace(QChar('\u0005'), "\\u0005"); // ^E
    sStr.replace(QChar('\u0006'), "\\u0006"); // ^F
    sStr.replace(QChar('\u0007'), "\\u0007"); // ^G (\a)
    sStr.replace(QChar('\u000B'), "\\u000B"); // ^K (\v)
    sStr.replace(QChar('\u000E'), "\\u000E"); // ^N
    sStr.replace(QChar('\u000F'), "\\u000F"); // ^O
    sStr.replace(QChar('\u0010'), "\\u0010"); // ^P
    sStr.replace(QChar('\u0011'), "\\u0011"); // ^Q XON
    sStr.replace(QChar('\u0012'), "\\u0012"); // ^R
    sStr.replace(QChar('\u0013'), "\\u0013"); // ^S XOFF
    sStr.replace(QChar('\u0014'), "\\u0014"); // ^T
    sStr.replace(QChar('\u0015'), "\\u0015"); // ^U
    sStr.replace(QChar('\u0016'), "\\u0016"); // ^V
    sStr.replace(QChar('\u0017'), "\\u0017"); // ^W
    sStr.replace(QChar('\u0018'), "\\u0018"); // ^X
    sStr.replace(QChar('\u0019'), "\\u0019"); // ^Y
    sStr.replace(QChar('\u001A'), "\\u001A"); // ^Z
    sStr.replace(QChar('\u001B'), "\\u001B");
    sStr.replace(QChar('\u001C'), "\\u001C");
    sStr.replace(QChar('\u001D'), "\\u001D");
    sStr.replace(QChar('\u001E'), "\\u001E");
    sStr.replace(QChar('\u001F'), "\\u001F");

    return sStr;
}
