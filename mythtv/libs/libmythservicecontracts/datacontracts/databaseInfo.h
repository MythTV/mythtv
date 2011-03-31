//////////////////////////////////////////////////////////////////////////////
// Program Name: databaseInfo.h
// Created     : Jan. 15, 2010
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

#ifndef DATABASEINFO_H_
#define DATABASEINFO_H_

#include <QString>

#include "serviceexp.h" 
#include "datacontracthelper.h"

namespace DTC
{

class SERVICE_PUBLIC DatabaseInfo : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "version", "1.0" );

    Q_PROPERTY( QString Host          READ Host            WRITE setHost           )
    Q_PROPERTY( bool    Ping          READ Ping            WRITE setPing           )
    Q_PROPERTY( int     Port          READ Port            WRITE setPort           )
    Q_PROPERTY( QString UserName      READ UserName        WRITE setUserName       )
    Q_PROPERTY( QString Password      READ Password        WRITE setPassword       )
    Q_PROPERTY( QString Name          READ Name            WRITE setName           )
    Q_PROPERTY( QString Type          READ Type            WRITE setType           )
    Q_PROPERTY( bool    LocalEnabled  READ LocalEnabled    WRITE setLocalEnabled   )
    Q_PROPERTY( QString LocalHostName READ LocalHostName   WRITE setLocalHostName  )

    PROPERTYIMP( QString, Host          )
    PROPERTYIMP( bool   , Ping          )
    PROPERTYIMP( int    , Port          )
    PROPERTYIMP( QString, UserName      )
    PROPERTYIMP( QString, Password      )
    PROPERTYIMP( QString, Name          )
    PROPERTYIMP( QString, Type          )
    PROPERTYIMP( bool   , LocalEnabled  )
    PROPERTYIMP( QString, LocalHostName )

    public:

        static void InitializeCustomTypes()
        {
            qRegisterMetaType< DatabaseInfo   >();
            qRegisterMetaType< DatabaseInfo*  >();
        }

    public:

        DatabaseInfo(QObject *parent = 0) 
            : QObject       ( parent ),
              m_Ping        ( false  ),
              m_Port        ( 0      ),
              m_LocalEnabled( false  ) 
        {
        }
        
        DatabaseInfo( const DatabaseInfo &src ) 
        {
            Copy( src );
        }

        void Copy( const DatabaseInfo &src ) 
        {
            m_Host         = src.m_Host         ;
            m_Ping         = src.m_Ping         ;
            m_Port         = src.m_Port         ;
            m_UserName     = src.m_UserName     ;
            m_Password     = src.m_Password     ;
            m_Name         = src.m_Name         ;
            m_Type         = src.m_Type         ;
            m_LocalEnabled = src.m_LocalEnabled ;
            m_LocalHostName= src.m_LocalHostName;
        }
};

typedef DatabaseInfo * DatabaseInfoPtr;

} // namespace DTC

Q_DECLARE_METATYPE( DTC::DatabaseInfo  )
Q_DECLARE_METATYPE( DTC::DatabaseInfo* )

#endif
