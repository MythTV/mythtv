//////////////////////////////////////////////////////////////////////////////
// Program Name: connectionInfo.h
// Created     : Jan. 15, 2010
//
// Copyright (c) 2010 David Blain <dblain@mythtv.org>
//                                          
// Licensed under the GPL v2 or later, see COPYING for details
//
//////////////////////////////////////////////////////////////////////////////

#ifndef CONNECTIONINFO_H_
#define CONNECTIONINFO_H_

#include "serviceexp.h" 
#include "datacontracthelper.h"

#include "versionInfo.h"
#include "databaseInfo.h"
#include "wolInfo.h"

namespace DTC
{

class SERVICE_PUBLIC ConnectionInfo : public QObject
{
    Q_OBJECT

    Q_CLASSINFO( "version"    , "1.1" );

    Q_PROPERTY( QObject*    Version     READ Version    )
    Q_PROPERTY( QObject*    Database    READ Database   )
    Q_PROPERTY( QObject*    WOL         READ WOL        )

    PROPERTYIMP_PTR( VersionInfo , Version    )
    PROPERTYIMP_PTR( DatabaseInfo, Database   )
    PROPERTYIMP_PTR( WOLInfo     , WOL        )

    public:

        static inline void InitializeCustomTypes();

    public:

        ConnectionInfo(QObject *parent = 0) 
            : QObject        ( parent ),
              m_Version      ( NULL   ),
              m_Database     ( NULL   ),
              m_WOL          ( NULL   )             
        {
        }
        
        ConnectionInfo( const ConnectionInfo &src ) 
            : m_Version   ( NULL ),
              m_Database  ( NULL ),
              m_WOL       ( NULL )                  
        {
            Copy( src );
        }

        void Copy( const ConnectionInfo &src ) 
        {
            // We always need to make sure the child object is
            // created with the correct parent *

            if (src.m_Version)
                Version()->Copy( src.m_Version );

            if (src.m_Database)
                Database()->Copy( src.m_Database );

            if (src.m_WOL)
                WOL     ()->Copy( src.m_WOL      );
        }
};

typedef ConnectionInfo* ConnectionInfoPtr;

} // namespace DTC

Q_DECLARE_METATYPE( DTC::ConnectionInfo  )
Q_DECLARE_METATYPE( DTC::ConnectionInfo* )

namespace DTC
{
inline void ConnectionInfo::InitializeCustomTypes()
{
    qRegisterMetaType< ConnectionInfo   >();
    qRegisterMetaType< ConnectionInfo*  >();

    VersionInfo ::InitializeCustomTypes();
    DatabaseInfo::InitializeCustomTypes();
    WOLInfo     ::InitializeCustomTypes();
}
}

#endif
