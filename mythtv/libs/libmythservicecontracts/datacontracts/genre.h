//////////////////////////////////////////////////////////////////////////////
// Program Name: genre.h
// Created     : Mar. 08, 2017
//
// Copyright (c) 2017 Paul Harrison <pharrison@mythtv.org>
//
// Licensed under the GPL v2 or later, see COPYING for details
//
//////////////////////////////////////////////////////////////////////////////

#ifndef GENRE_H_
#define GENRE_H_

#include <QString>

#include "serviceexp.h"
#include "datacontracthelper.h"

namespace DTC
{

/////////////////////////////////////////////////////////////////////////////

class SERVICE_PUBLIC Genre : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "version"    , "1.0" );

    Q_PROPERTY( QString Name         READ Name         WRITE setName         )

    PROPERTYIMP    ( QString    , Name           )

    public:

        static inline void InitializeCustomTypes();

    public:

        Genre(QObject *parent = 0)
            : QObject         ( parent )
        {
        }

        Genre( const Genre &src )
        {
            Copy( src );
        }

        void Copy( const Genre &src )
        {
            m_Name          = src.m_Name          ;
        }
};

} // namespace DTC

Q_DECLARE_METATYPE( DTC::Genre  )
Q_DECLARE_METATYPE( DTC::Genre* )

namespace DTC
{
inline void Genre::InitializeCustomTypes()
{
    qRegisterMetaType< Genre  >();
    qRegisterMetaType< Genre* >();
}
}

#endif
