//////////////////////////////////////////////////////////////////////////////
// Program Name: buildInfo.h
// Created     : Dec. 15, 2015
//
// Copyright (c) 2015 Bill Meek, from: 2010 David Blain <dblain@mythtv.org>
//
// Licensed under the GPL v2 or later, see COPYING for details
//
//////////////////////////////////////////////////////////////////////////////

#ifndef BUILDINFO_H_
#define BUILDINFO_H_

#include <QString>

#include "serviceexp.h"
#include "datacontracthelper.h"

namespace DTC
{

class SERVICE_PUBLIC BuildInfo : public QObject
{
    Q_OBJECT

    Q_CLASSINFO( "version"    , "1.0" );

    Q_PROPERTY( QString Version      READ Version     WRITE setVersion   )
    Q_PROPERTY( bool    LibX264      READ LibX264     WRITE setLibX264   )
    Q_PROPERTY( bool    LibDNS_SD    READ LibDNS_SD   WRITE setLibDNS_SD )

    PROPERTYIMP( QString , Version   )
    PROPERTYIMP( bool    , LibX264   )
    PROPERTYIMP( bool    , LibDNS_SD )

    public:

        static inline void InitializeCustomTypes();

    public:

        BuildInfo(QObject *parent = 0)
            : QObject    ( parent ),
              m_Version  ( ""     ),
              m_LibX264  ( false  ),
              m_LibDNS_SD( false  )
        {
        }

        BuildInfo( const BuildInfo &src )
        {
            Copy( src );
        }

        void Copy( const BuildInfo &src )
        {
            m_Version   = src.m_Version  ;
            m_LibX264   = src.m_LibX264  ;
            m_LibDNS_SD = src.m_LibDNS_SD;
        }
};

typedef BuildInfo* BuildInfoPtr;

} // namespace DTC

Q_DECLARE_METATYPE( DTC::BuildInfo  )
Q_DECLARE_METATYPE( DTC::BuildInfo* )

namespace DTC
{
inline void BuildInfo::InitializeCustomTypes()
{
    qRegisterMetaType< BuildInfo  >();
    qRegisterMetaType< BuildInfo* >();
}
}

#endif
