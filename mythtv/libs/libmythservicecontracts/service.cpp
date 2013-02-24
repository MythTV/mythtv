//////////////////////////////////////////////////////////////////////////////
// Program Name: service.cpp
// Created     : Jan. 19, 2010
//
// Purpose     : Base class for all Web Services 
//                                                                            
// Copyright (c) 2010 David Blain <dblain@mythtv.org>
//                                          
// Licensed under the GPL v2 or later, see COPYING for details
//
//////////////////////////////////////////////////////////////////////////////

#include "service.h"

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////

QVariant Service::ConvertToVariant( int nType, void *pValue )
{
    // -=>NOTE: This assumes any UserType will be derived from QObject...
    //          (Exception for QFileInfo )

    if ( nType == QMetaType::type( "QFileInfo" ))
        return QVariant::fromValue< QFileInfo >( *((QFileInfo *)pValue) );

    if (nType > QMetaType::User)
    {
        QObject *pObj = *((QObject **)pValue);

        return QVariant::fromValue<QObject*>( pObj );
    }

    return QVariant( nType, pValue );
}


//////////////////////////////////////////////////////////////////////////////
// 
//////////////////////////////////////////////////////////////////////////////

void* Service::ConvertToParameterPtr( int            nTypeId,
                                      const QString &sParamType, 
                                      void*          pParam,
                                      const QString &sValue )
{      
    // -=>NOTE: Only intrinsic or Qt type conversions should be here
    //          All others should be added to overridden implementation.

    switch( nTypeId )
    {
        case QMetaType::Bool        : *(( bool           *)pParam) = ToBool( sValue );         break;

        case QMetaType::Char        : *(( char           *)pParam) = ( sValue.length() > 0) ? sValue.at( 0 ).toLatin1() : 0; break;
        case QMetaType::UChar       : *(( unsigned char  *)pParam) = ( sValue.length() > 0) ? sValue.at( 0 ).toLatin1() : 0; break;
        case QMetaType::QChar       : *(( QChar          *)pParam) = ( sValue.length() > 0) ? sValue.at( 0 )           : 0; break;

        case QMetaType::Short       : *(( short          *)pParam) = sValue.toShort     (); break;
        case QMetaType::UShort      : *(( ushort         *)pParam) = sValue.toUShort    (); break;

        case QMetaType::Int         : *(( int            *)pParam) = sValue.toInt       (); break;
        case QMetaType::UInt        : *(( uint           *)pParam) = sValue.toUInt      (); break;

        case QMetaType::Long        : *(( long           *)pParam) = sValue.toLong      (); break;
        case QMetaType::ULong       : *(( ulong          *)pParam) = sValue.toULong     (); break;

        case QMetaType::LongLong    : *(( qlonglong      *)pParam) = sValue.toLongLong  (); break;
        case QMetaType::ULongLong   : *(( qulonglong     *)pParam) = sValue.toULongLong (); break;

        case QMetaType::Double      : *(( double         *)pParam) = sValue.toDouble    (); break;
        case QMetaType::Float       : *(( float          *)pParam) = sValue.toFloat     (); break;

        case QMetaType::QString     : *(( QString        *)pParam) = sValue;                break;
        case QMetaType::QByteArray  : *(( QByteArray     *)pParam) = sValue.toUtf8      (); break;

        case QMetaType::QDateTime   :
        {
            QDateTime dt = QDateTime::fromString( sValue, Qt::ISODate );
            dt.setTimeSpec( Qt::UTC );
            *(( QDateTime      *)pParam) = dt;
            break;
        }
        case QMetaType::QTime       : *(( QTime          *)pParam) = QTime::fromString    ( sValue, Qt::ISODate ); break;
        case QMetaType::QDate       : *(( QDate          *)pParam) = QDate::fromString    ( sValue, Qt::ISODate ); break;
    }

    return pParam;
}

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////

bool Service::ToBool( const QString &sVal )
{
    if (sVal.compare( "1", Qt::CaseInsensitive ) == 0)
        return true;

    if (sVal.compare( "y", Qt::CaseInsensitive ) == 0)
        return true;

    if (sVal.compare( "true", Qt::CaseInsensitive ) == 0)
        return true;

    return false;
}
