//////////////////////////////////////////////////////////////////////////////
// Program Name: markup.h
// Created     : Apr. 4, 2021
//
// Copyright (c) 2021 team MythTV
//
// Licensed under the GPL v2 or later, see COPYING for details
//
//////////////////////////////////////////////////////////////////////////////

#ifndef V2MARKUP_H_
#define V2MARKUP_H_

#include <QString>
#include <QVariantList>

#include "libmythbase/http/mythhttpservice.h"

/////////////////////////////////////////////////////////////////////////////

class V2Markup : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "Version"    , "1.0" );

    SERVICE_PROPERTY2( QString   , Type  )
    SERVICE_PROPERTY2( quint64   , Frame )
    SERVICE_PROPERTY2( QString   , Data  )

    public:

        Q_INVOKABLE V2Markup(QObject *parent = nullptr)
            : QObject(parent)
        {
        }

        void Copy( const V2Markup *src )
        {
            m_Type          = src->m_Type  ;
            m_Frame         = src->m_Frame ;
            m_Data          = src->m_Data  ;
        }

    private:
        Q_DISABLE_COPY(V2Markup);
};

Q_DECLARE_METATYPE(V2Markup*)

#endif
