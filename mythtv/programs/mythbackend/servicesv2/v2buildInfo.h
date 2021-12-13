//////////////////////////////////////////////////////////////////////////////
// Program Name: buildInfo.h
// Created     : Dec. 15, 2015
//
// Copyright (c) 2015 Bill Meek, from: 2010 David Blain <dblain@mythtv.org>
//
// Licensed under the GPL v2 or later, see COPYING for details
//
//////////////////////////////////////////////////////////////////////////////

#ifndef V2BUILDINFO_H_
#define V2BUILDINFO_H_

#include "libmythbase/http/mythhttpservice.h"

class V2BuildInfo : public QObject
{
    Q_OBJECT

    Q_CLASSINFO( "version"    , "1.0" );

    SERVICE_PROPERTY2( QString , Version   )
    SERVICE_PROPERTY2( bool    , LibX264   )
    SERVICE_PROPERTY2( bool    , LibDNS_SD );

    public:

        Q_INVOKABLE V2BuildInfo(QObject *parent = nullptr)
            : QObject    ( parent ),
              m_Version  ( ""     )
        {
        }

        void Copy( const V2BuildInfo *src )
        {
            m_Version   = src->m_Version  ;
            m_LibX264   = src->m_LibX264  ;
            m_LibDNS_SD = src->m_LibDNS_SD;
        }

    private:
        Q_DISABLE_COPY(V2BuildInfo);
};

using BuildInfoPtr = V2BuildInfo*;

Q_DECLARE_METATYPE(V2BuildInfo*)

#endif
