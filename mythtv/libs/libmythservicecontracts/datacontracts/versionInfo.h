//////////////////////////////////////////////////////////////////////////////
// Program Name: versionInfo.h
// Created     : Jul. 26, 2011
//
// Copyright (c) 2011 Raymond Wagner <rwagner@mythtv.org>
//                      
// Licensed under the GPL v2 or later, see COPYING for details                    
//
//////////////////////////////////////////////////////////////////////////////

#ifndef VERSIONINFO_H_
#define VERSIONINFO_H_

#include <QString>

#include "serviceexp.h" 
#include "datacontracthelper.h"

namespace DTC
{

class SERVICE_PUBLIC VersionInfo : public QObject
{
    Q_OBJECT

    Q_CLASSINFO( "version"    , "1.0" );
    Q_CLASSINFO( "defaultProp", "Command" );

    Q_PROPERTY( QString  Version      READ Version   WRITE setVersion   )
    Q_PROPERTY( QString  Branch       READ Branch    WRITE setBranch    )
    Q_PROPERTY( QString  Protocol     READ Protocol  WRITE setProtocol  )
    Q_PROPERTY( QString  Binary       READ Binary    WRITE setBinary    )
    Q_PROPERTY( QString  Schema       READ Schema    WRITE setSchema    )

    PROPERTYIMP( QString,  Version   )
    PROPERTYIMP( QString,  Branch    )
    PROPERTYIMP( QString,  Protocol  )
    PROPERTYIMP( QString,  Binary    )
    PROPERTYIMP( QString,  Schema    )

    public:

        static inline void InitializeCustomTypes();

    public:

        VersionInfo(QObject *parent = 0)
            : QObject   ( parent ),
              m_Version ( ""     ),
              m_Branch  ( ""     ),
              m_Protocol( ""     ),
              m_Binary  ( ""     ),
              m_Schema  ( ""     )
        {
        }
        
        VersionInfo( const VersionInfo &src ) 
        {
            Copy( src );
        }

        void Copy( const VersionInfo &src )
        {
            m_Version  = src.m_Version;
            m_Branch   = src.m_Branch;
            m_Protocol = src.m_Protocol;
            m_Binary   = src.m_Binary;
            m_Schema   = src.m_Schema;
        }
};

typedef VersionInfo* VersionInfoPtr;

} // namespace DTC

Q_DECLARE_METATYPE( DTC::VersionInfo  )
Q_DECLARE_METATYPE( DTC::VersionInfo* )

namespace DTC
{
inline void VersionInfo::InitializeCustomTypes()
{
    qRegisterMetaType< VersionInfo   >();
    qRegisterMetaType< VersionInfo*  >();
}
}

#endif
