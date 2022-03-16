//////////////////////////////////////////////////////////////////////////////
// Program Name: connectionInfo.h
// Created     : Jan. 15, 2010
//
// Copyright (c) 2010 David Blain <dblain@mythtv.org>
//                                          
// Licensed under the GPL v2 or later, see LICENSE for details
//
//////////////////////////////////////////////////////////////////////////////

#ifndef CONNECTIONINFO_H_
#define CONNECTIONINFO_H_

#include "libmythservicecontracts/serviceexp.h"
#include "libmythservicecontracts/datacontracthelper.h"

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
    PROPERTYIMP_PTR( WOLInfo     , WOL        );

    public:

        static inline void InitializeCustomTypes();

        Q_INVOKABLE ConnectionInfo(QObject *parent = nullptr)
            : QObject        ( parent  ),
              m_Version      ( nullptr ),
              m_Database     ( nullptr ),
              m_WOL          ( nullptr )
        {
        }

        void Copy( const ConnectionInfo *src )
        {
            // We always need to make sure the child object is
            // created with the correct parent *

            if (src->m_Version)
                Version()->Copy( src->m_Version );

            if (src->m_Database)
                Database()->Copy( src->m_Database );

            if (src->m_WOL)
                WOL     ()->Copy( src->m_WOL      );
        }

    private:
        Q_DISABLE_COPY(ConnectionInfo);
};

using ConnectionInfoPtr = ConnectionInfo*;

inline void ConnectionInfo::InitializeCustomTypes()
{
    qRegisterMetaType< ConnectionInfo*  >();

    VersionInfo ::InitializeCustomTypes();
    DatabaseInfo::InitializeCustomTypes();
    WOLInfo     ::InitializeCustomTypes();
}

} // namespace DTC

#endif
