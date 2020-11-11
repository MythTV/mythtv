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

    PROPERTYIMP_REF( QString, Version   )
    PROPERTYIMP_REF( QString, Branch    )
    PROPERTYIMP_REF( QString, Protocol  )
    PROPERTYIMP_REF( QString, Binary    )
    PROPERTYIMP_REF( QString, Schema    );

    public:

        static inline void InitializeCustomTypes();

        Q_INVOKABLE VersionInfo(QObject *parent = nullptr)
            : QObject   ( parent ),
              m_Version ( ""     ),
              m_Branch  ( ""     ),
              m_Protocol( ""     ),
              m_Binary  ( ""     ),
              m_Schema  ( ""     )
        {
        }

        void Copy( const VersionInfo *src )
        {
            m_Version  = src->m_Version;
            m_Branch   = src->m_Branch;
            m_Protocol = src->m_Protocol;
            m_Binary   = src->m_Binary;
            m_Schema   = src->m_Schema;
        }

    private:
        Q_DISABLE_COPY(VersionInfo);
};

using VersionInfoPtr = VersionInfo*;

inline void VersionInfo::InitializeCustomTypes()
{
    qRegisterMetaType< VersionInfo*  >();
}

} // namespace DTC

#endif
