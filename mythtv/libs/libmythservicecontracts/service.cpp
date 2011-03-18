//////////////////////////////////////////////////////////////////////////////
// Program Name: service.cpp
// Created     : Jan. 19, 2010
//
// Purpose     : Base class for all Web Services 
//                                                                            
// Copyright (c) 2010 David Blain <dblain@mythtv.org>
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

#include "service.h"

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////

QVariant Service::ConvertToVariant( const QString &sTypeName, void *pValue )
{
    // -=>NOTE: Only intrinsic or Qt type conversions should be here
    //          All others should be added to overridden implementation.


    if ( sTypeName.compare( "QFileInfo*", Qt::CaseInsensitive ) == 0 )
    {
        QFileInfo *pInfo = reinterpret_cast< QFileInfo *>( pValue );

        return QVariant::fromValue<QFileInfo*>( pInfo );
    }

    // -=>NOTE: Default to being a QObject or derivative... 
    //          Will CRASH if NOT derived from QObject!!!

    QObject *pObjResult = reinterpret_cast< QObject *>( pValue );

    return QVariant::fromValue<QObject*>( pObjResult );
}

//////////////////////////////////////////////////////////////////////////////
// 
//////////////////////////////////////////////////////////////////////////////

void* Service::ConvertToParameterPtr( const QString &sParamType, 
                                      const QString &sValue )
{      
    // -=>NOTE: Only intrinsic or Qt type conversions should be here
    //          All others should be added to overridden implementation.

    if (sParamType == "QString")
        return new QString( sValue );

    if (sParamType == "QDateTime")
    {
        if ( sValue.isEmpty())
            return new QDateTime();

        return new QDateTime( QDateTime::fromString( sValue, Qt::ISODate ));
    } 

    if (sParamType == "int")
        return new int( sValue.toInt() );

    if (sParamType == "bool")
        return new bool( ToBool( sValue ));

    return NULL;
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
