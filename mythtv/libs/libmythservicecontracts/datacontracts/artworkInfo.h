//////////////////////////////////////////////////////////////////////////////
// Program Name: artworkInfo.h
// Created     : Nov. 12, 2011
//
// Copyright (c) 2011 Robert McNamara <rmcnamara@mythtv.org>
//
// Licensed under the GPL v2 or later, see COPYING for details
//
//////////////////////////////////////////////////////////////////////////////

#ifndef ARTWORKINFO_H_
#define ARTWORKINFO_H_

#include <QString>

#include "serviceexp.h"
#include "datacontracthelper.h"

namespace DTC
{

/////////////////////////////////////////////////////////////////////////////

class SERVICE_PUBLIC ArtworkInfo : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "version"    , "1.0" );

    Q_PROPERTY( QString URL          READ URL          WRITE setURL          )
    Q_PROPERTY( QString FileName     READ FileName     WRITE setFileName     )
    Q_PROPERTY( QString StorageGroup READ StorageGroup WRITE setStorageGroup )
    Q_PROPERTY( QString Type         READ Type         WRITE setType         )

    PROPERTYIMP    ( QString    , URL            )
    PROPERTYIMP    ( QString    , FileName       )
    PROPERTYIMP    ( QString    , StorageGroup   )
    PROPERTYIMP    ( QString    , Type           )

    public:

        static inline void InitializeCustomTypes();

    public:

        ArtworkInfo(QObject *parent = 0)
            : QObject         ( parent )
        {
        }

        ArtworkInfo( const ArtworkInfo &src )
        {
            Copy( src );
        }

        void Copy( const ArtworkInfo &src )
        {
            m_URL           = src.m_URL           ;
            m_FileName      = src.m_FileName      ;
            m_StorageGroup  = src.m_StorageGroup  ;
            m_Type          = src.m_Type          ;
        }
};

} // namespace DTC

Q_DECLARE_METATYPE( DTC::ArtworkInfo  )
Q_DECLARE_METATYPE( DTC::ArtworkInfo* )

namespace DTC
{
inline void ArtworkInfo::InitializeCustomTypes()
{
    qRegisterMetaType< ArtworkInfo  >();
    qRegisterMetaType< ArtworkInfo* >();
}
}

#endif
