//////////////////////////////////////////////////////////////////////////////
// Program Name: logInfo.h
// Created     : Dec. 15, 2015
//
// Copyright (c) 2015 Bill Meek, from: 2010 David Blain <dblain@mythtv.org>
//
// Licensed under the GPL v2 or later, see COPYING for details
//
//////////////////////////////////////////////////////////////////////////////

#ifndef V2LOGINFO_H_
#define V2LOGINFO_H_

#include "libmythbase/http/mythhttpservice.h"

class V2LogInfo : public QObject
{
    Q_OBJECT

    Q_CLASSINFO( "Version"    , "1.0" );
    Q_CLASSINFO( "defaultProp", "LogArgs" );

    SERVICE_PROPERTY2( QString,  LogArgs   );

    public:

        Q_INVOKABLE V2LogInfo(QObject *parent = nullptr)
            : QObject    ( parent ),
              m_LogArgs  ( ""     )
        {
        }

        void Copy( const V2LogInfo *src )
        {
            m_LogArgs   = src->m_LogArgs  ;
        }

    private:
        Q_DISABLE_COPY(V2LogInfo);
};

using LogInfoPtr = V2LogInfo*;

Q_DECLARE_METATYPE(V2LogInfo*)

#endif // LOGINFO_H_
