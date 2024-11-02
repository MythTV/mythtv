//////////////////////////////////////////////////////////////////////////////
// Program Name: input.h
// Created     : Nov. 20, 2013
//
// Copyright (c) 2013 Stuart Morgan <smorgan@mythtv.org>
//
// Licensed under the GPL v2 or later, see COPYING for details
//
//////////////////////////////////////////////////////////////////////////////

#ifndef V2PLAYGROUP_H
#define V2PLAYGROUP_H

#include <QString>

#include "libmythbase/http/mythhttpservice.h"

/////////////////////////////////////////////////////////////////////////////

class V2PlayGroup : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "Version"    , "1.0" );

    SERVICE_PROPERTY2( QString    , Name        )
    SERVICE_PROPERTY2( QString    , TitleMatch  )
    SERVICE_PROPERTY2( int        , SkipAhead   )
    SERVICE_PROPERTY2( int        , SkipBack    )
    SERVICE_PROPERTY2( int        , TimeStretch )
    SERVICE_PROPERTY2( int        , Jump        )

    public:

        Q_INVOKABLE V2PlayGroup(QObject *parent = nullptr)
            : QObject( parent )
        {
        }

    private:
        Q_DISABLE_COPY(V2PlayGroup);
};

Q_DECLARE_METATYPE(V2PlayGroup*)

#endif
