//////////////////////////////////////////////////////////////////////////////
// Program Name: service.h
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

#ifndef SERVICE_H_
#define SERVICE_H_

#include <QObject>
#include <QMetaType>
#include <QVariant>
#include <QFileInfo>
#include <QDateTime>
#include <QString>

#include "serviceexp.h"

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
//
// Notes for derived classes -
//
//  * This implementation can't handle declared default parameters
//
//  * When called, any missing params are sent default values for its datatype
//
//  * Q_CLASSINFO( "<methodName>_Method", "BOTH" ) 
//    is used to determine HTTP method type.  
//    Defaults to "BOTH", available values:
//          "GET", "POST" or "BOTH"
//
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

class SERVICE_PUBLIC Service : public QObject 
{
    Q_OBJECT

    public:

        Service( QObject *parent = 0 ) : QObject( parent ) 
        {
            qRegisterMetaType< QFileInfo   >();
        }

    public:

        /////////////////////////////////////////////////////////////////////
        // This method should be overridden to handle non-QObject based custom types
        /////////////////////////////////////////////////////////////////////

        virtual QVariant ConvertToVariant  ( int nType, void *pValue );

        virtual void* ConvertToParameterPtr( int            nTypeId,
                                             const QString &sParamType, 
                                             void*          pParam,
                                             const QString &sValue );

        static bool ToBool( const QString &sVal );

};

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////

Q_DECLARE_METATYPE( QFileInfo  )

#endif
