//////////////////////////////////////////////////////////////////////////////
// Program Name: service.cpp
// Created     : Jan. 19, 2010
//
// Purpose     : Base class for all Web Services
//
// Copyright (c) 2010 David Blain <dblain@mythtv.org>
//
// Licensed under the GPL v2 or later, see LICENSE for details
//
//////////////////////////////////////////////////////////////////////////////

#include "service.h"
#include <QMetaEnum>
#include <QJsonDocument>
#include <QJsonObject>

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////

QVariant Service::ConvertToVariant( int nType, void *pValue )
{
    // -=>NOTE: This assumes any UserType will be derived from QObject...
    //          (Exception for QFileInfo )

    if ( nType == qMetaTypeId<QFileInfo>() )
        return QVariant::fromValue< QFileInfo >( *((QFileInfo *)pValue) );

    if (nType > QMetaType::User)
    {
        QObject *pObj = *((QObject **)pValue);

        return QVariant::fromValue<QObject*>( pObj );
    }

#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
    return { nType, pValue };
#else
    return QVariant( QMetaType(nType), pValue );
#endif
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
        case QMetaType::QChar       : *(( QChar          *)pParam) = ( sValue.length() > 0) ? sValue.at( 0 )            : QChar(0); break;

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
#if QT_VERSION < QT_VERSION_CHECK(6,5,0)
            dt.setTimeSpec( Qt::UTC );
#else
            dt.setTimeZone( QTimeZone(QTimeZone::UTC) );
#endif
            *(( QDateTime      *)pParam) = dt;
            break;
        }
        case QMetaType::QTime       : *(( QTime          *)pParam) = QTime::fromString    ( sValue, Qt::ISODate ); break;
        case QMetaType::QDate       : *(( QDate          *)pParam) = QDate::fromString    ( sValue, Qt::ISODate ); break;
        case QMetaType::QJsonObject :
        {
            QJsonDocument doc = QJsonDocument::fromJson(sValue.toUtf8());

            // check validity of the document
            if(!doc.isNull())
            {
                if(doc.isObject())
                    *(( QJsonObject *)pParam) = doc.object();
            }
            else
            {
                throw QString("Invalid JSON: %1").arg(sValue);
            }

            break;
        }
        default:

            // --------------------------------------------------------------
            // Need to deal with Enums.  For now, assume any type that contains :: is an enum.
            // --------------------------------------------------------------

            int nLastIdx = sParamType.lastIndexOf( "::" );

            if (nLastIdx == -1)
                break;

            QString sParentFQN = sParamType.mid( 0, nLastIdx );
            QString sEnumName  = sParamType.mid( nLastIdx+2  );

            // --------------------------------------------------------------
            // Create Parent object so we can get to its metaObject
            // --------------------------------------------------------------

#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
            int nParentId = QMetaType::type( sParentFQN.toUtf8() );

            auto *pParentClass = (QObject *)QMetaType::create( nParentId );
            if (pParentClass == nullptr)
                break;

            const QMetaObject *pMetaObject = pParentClass->metaObject();

            QMetaType::destroy( nParentId, pParentClass );
#else
            const QMetaObject *pMetaObject =
                QMetaType::fromName( sParentFQN.toUtf8() ).metaObject();
#endif

            // --------------------------------------------------------------
            // Now look up enum
            // --------------------------------------------------------------

            int nEnumIdx = pMetaObject->indexOfEnumerator( sEnumName.toUtf8());

            if (nEnumIdx < 0 )
                break;

            QMetaEnum metaEnum = pMetaObject->enumerator( nEnumIdx );

            *(( int *)pParam) = metaEnum.keyToValue( sValue.toUtf8() );

            break;
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
