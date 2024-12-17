//////////////////////////////////////////////////////////////////////////////
// Program Name: genre.h
// Created     : Mar. 08, 2017
//
// Copyright (c) 2017 Paul Harrison <pharrison@mythtv.org>
//
// Licensed under the GPL v2 or later, see LICENSE for details
//
//////////////////////////////////////////////////////////////////////////////

#ifndef GENRE_H_
#define GENRE_H_

#include <QString>

#include "libmythservicecontracts/serviceexp.h"
#include "libmythservicecontracts/datacontracthelper.h"

namespace DTC
{

/////////////////////////////////////////////////////////////////////////////

class SERVICE_PUBLIC Genre : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "version"    , "1.0" );

    Q_PROPERTY( QString Name         READ Name         WRITE setName         )

    PROPERTYIMP_REF( QString    , Name           );

    public:

        static inline void InitializeCustomTypes();

        Q_INVOKABLE Genre(QObject *parent = nullptr)
            : QObject         ( parent )
        {
        }

        void Copy( const Genre *src )
        {
            m_Name          = src->m_Name          ;
        }

    private:
        Q_DISABLE_COPY(Genre);
};

inline void Genre::InitializeCustomTypes()
{
    qRegisterMetaType< Genre* >();
}

} // namespace DTC

#endif
