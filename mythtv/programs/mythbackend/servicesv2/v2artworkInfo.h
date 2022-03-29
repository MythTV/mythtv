//////////////////////////////////////////////////////////////////////////////
// Program Name: artworkInfo.h
// Created     : Nov. 12, 2011
//
// Copyright (c) 2011 Robert McNamara <rmcnamara@mythtv.org>
//
// Licensed under the GPL v2 or later, see COPYING for details
//
//////////////////////////////////////////////////////////////////////////////

#ifndef V2ARTWORKINFO_H_
#define V2ARTWORKINFO_H_

#include <QString>

#include "libmythbase/http/mythhttpservice.h"


/////////////////////////////////////////////////////////////////////////////

class V2ArtworkInfo : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "Version"    , "1.0" );

    SERVICE_PROPERTY2( QString, URL          )
    SERVICE_PROPERTY2( QString, FileName     )
    SERVICE_PROPERTY2( QString, StorageGroup )
    SERVICE_PROPERTY2( QString, Type         )

    public:

        Q_INVOKABLE V2ArtworkInfo(QObject *parent = nullptr)
            : QObject         ( parent )
        {
        }

        void Copy( const V2ArtworkInfo *src )
        {
            m_URL           = src->m_URL           ;
            m_FileName      = src->m_FileName      ;
            m_StorageGroup  = src->m_StorageGroup  ;
            m_Type          = src->m_Type          ;
        }

    private:
        Q_DISABLE_COPY(V2ArtworkInfo);
};

Q_DECLARE_METATYPE(V2ArtworkInfo*)

#endif
