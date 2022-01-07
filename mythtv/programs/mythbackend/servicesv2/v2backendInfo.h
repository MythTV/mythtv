//////////////////////////////////////////////////////////////////////////////
// Program Name: backendInfo.h
// Created     : Dec. 15, 2015
//
// Copyright (c) 2015 Bill Meek, from: 2010 David Blain <dblain@mythtv.org>
//
// Licensed under the GPL v2 or later, see COPYING for details
//
//////////////////////////////////////////////////////////////////////////////

#ifndef V2BACKENDINFO_H_
#define V2BACKENDINFO_H_

#include "libmythbase/http/mythhttpservice.h"
#include "v2buildInfo.h"
#include "v2envInfo.h"
#include "v2logInfo.h"

class V2BackendInfo : public QObject
{
    Q_OBJECT

    Q_CLASSINFO( "version"    , "1.0" );

    Q_PROPERTY( QObject*    Build   READ Build USER true)
    Q_PROPERTY( QObject*    Env     READ Env   USER true)
    Q_PROPERTY( QObject*    Log     READ Log   USER true)

    SERVICE_PROPERTY_PTR( V2BuildInfo, Build )
    SERVICE_PROPERTY_PTR( V2EnvInfo,   Env   )
    SERVICE_PROPERTY_PTR( V2LogInfo,   Log   );

    public:

        Q_INVOKABLE V2BackendInfo(QObject *parent = nullptr)
            : QObject( parent )
        {
        }

        void Copy( const V2BackendInfo *src )
        {
            // We always need to make sure the child object is
            // created with the correct parent *

            if (src->m_Build)
                Build()->Copy( src->m_Build );

            if (src->m_Env)
                Env()->Copy( src->m_Env  );

            if (src->m_Log)
                Log()->Copy( src->m_Log  );

        }

    private:
        Q_DISABLE_COPY(V2BackendInfo);
};

using BackendInfoPtr = V2BackendInfo*;

Q_DECLARE_METATYPE(V2BackendInfo*)

#endif
