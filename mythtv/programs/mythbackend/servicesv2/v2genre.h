//////////////////////////////////////////////////////////////////////////////
// Program Name: genre.h
// Created     : Mar. 08, 2017
//
// Copyright (c) 2017 Paul Harrison <pharrison@mythtv.org>
//
// Licensed under the GPL v2 or later, see COPYING for details
//
//////////////////////////////////////////////////////////////////////////////

#ifndef V2GENRE_H_
#define V2GENRE_H_

#include <QString>

#include "libmythbase/http/mythhttpservice.h"


/////////////////////////////////////////////////////////////////////////////

class V2Genre : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "Version"    , "1.0" );

    SERVICE_PROPERTY2( QString    , Name           );

    public:

        // static inline void InitializeCustomTypes();

        Q_INVOKABLE V2Genre(QObject *parent = nullptr)
            : QObject         ( parent )
        {
        }

        void Copy( const V2Genre *src )
        {
            m_Name          = src->m_Name          ;
        }

    private:
        Q_DISABLE_COPY(V2Genre);
};

Q_DECLARE_METATYPE(V2Genre*)

#endif
