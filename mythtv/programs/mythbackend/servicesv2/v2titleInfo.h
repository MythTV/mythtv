////////////////////////////////////////////////////////////////////////////
// Program Name: titleInfo.h
// Created     : June 14, 2013
//
// Copyright (c) 2013 Chris Pinkham <cpinkham@mythtv.org>
//
// Licensed under the GPL v2 or later, see COPYING for details
//
////////////////////////////////////////////////////////////////////////////

#ifndef V2TITLEINFO_H_
#define V2TITLEINFO_H_

#include <QString>

#include "libmythbase/http/mythhttpservice.h"

/////////////////////////////////////////////////////////////////////////////

class V2TitleInfo : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "Version"    , "1.1" );

    SERVICE_PROPERTY2( QString    , Title            )
    SERVICE_PROPERTY2( QString    , Inetref          )
    SERVICE_PROPERTY2( int        , Count            );

    public:

        Q_INVOKABLE V2TitleInfo(QObject *parent = nullptr)
            : QObject( parent )
        {
        }

        void Copy( const V2TitleInfo *src )
        {
            m_Title             = src->m_Title             ;
            m_Inetref           = src->m_Inetref           ;
            m_Count             = src->m_Count             ;
        }

    private:
        Q_DISABLE_COPY(V2TitleInfo);
};

Q_DECLARE_METATYPE(V2TitleInfo*)

#endif
