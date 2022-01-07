//////////////////////////////////////////////////////////////////////////////
// Program Name: input.h
// Created     : May. 30, 2014
//
// Copyright (c) 2014 Stuart Morgan <smorgan@mythtv.org>
//
// Licensed under the GPL v2 or later, see COPYING for details
//
//////////////////////////////////////////////////////////////////////////////

#ifndef V2FRONTEND_H_
#define V2FRONTEND_H_

#include "libmythbase/http/mythhttpservice.h"

/////////////////////////////////////////////////////////////////////////////

class V2Frontend : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "version"    , "1.0" );

    SERVICE_PROPERTY2( QString    , Name            )
    SERVICE_PROPERTY2( QString    , IP              )
    SERVICE_PROPERTY2( int        , Port            )
    SERVICE_PROPERTY2( bool       , OnLine          )

    public:

        Q_INVOKABLE V2Frontend(QObject *parent = nullptr)
            : QObject( parent )
        {
        }

        void Copy( const V2Frontend *src )
        {
            m_Name            = src->m_Name;
            m_IP              = src->m_IP;
            m_Port            = src->m_Port;
            m_OnLine          = src->m_OnLine;
        }

    private:
        Q_DISABLE_COPY(V2Frontend);
};

Q_DECLARE_METATYPE(V2Frontend*)

#endif
