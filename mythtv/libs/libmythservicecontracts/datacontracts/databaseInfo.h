//////////////////////////////////////////////////////////////////////////////
// Program Name: databaseInfo.h
// Created     : Jan. 15, 2010
//
// Copyright (c) 2010 David Blain <dblain@mythtv.org>
//                                          
// Licensed under the GPL v2 or later, see LICENSE for details
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

    PROPERTYIMP_REF( QString, Host          )
    PROPERTYIMP    ( bool   , Ping          )
    PROPERTYIMP    ( int    , Port          )
    PROPERTYIMP_REF( QString, UserName      )
    PROPERTYIMP_REF( QString, Password      )
    PROPERTYIMP_REF( QString, Name          )
    PROPERTYIMP_REF( QString, Type          )
    PROPERTYIMP    ( bool   , LocalEnabled  )
    PROPERTYIMP_REF( QString, LocalHostName );

    public:

        static inline void InitializeCustomTypes();

        Q_INVOKABLE DatabaseInfo(QObject *parent = nullptr)
            : QObject       ( parent ),
              m_Ping        ( false  ),
              m_Port        ( 0      ),
              m_LocalEnabled( false  ) 
        {
        }

        void Copy( const DatabaseInfo *src )
        {
            m_Host         = src->m_Host         ;
            m_Ping         = src->m_Ping         ;
            m_Port         = src->m_Port         ;
            m_UserName     = src->m_UserName     ;
            m_Password     = src->m_Password     ;
            m_Name         = src->m_Name         ;
            m_Type         = src->m_Type         ;
            m_LocalEnabled = src->m_LocalEnabled ;
            m_LocalHostName= src->m_LocalHostName;
        }

    private:
        Q_DISABLE_COPY(DatabaseInfo);
};

using DatabaseInfoPtr = DatabaseInfo*;

inline void DatabaseInfo::InitializeCustomTypes()
{
    qRegisterMetaType< DatabaseInfo*  >();
}

} // namespace DTC

#endif
