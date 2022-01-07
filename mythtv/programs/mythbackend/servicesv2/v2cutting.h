//////////////////////////////////////////////////////////////////////////////
// Program Name: cutting.h
// Created     : Mar. 09, 2014
//
// Copyright (c) 2014 team MythTV
//
// Licensed under the GPL v2 or later, see COPYING for details
//
//////////////////////////////////////////////////////////////////////////////

#ifndef V2CUTTING_H_
#define V2CUTTING_H_

#include <QString>
#include <QVariantList>

#include "libmythbase/http/mythhttpservice.h"

class V2Cutting : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "Version"    , "1.0" );

    SERVICE_PROPERTY2 ( int        , Mark   )
    SERVICE_PROPERTY2 ( qlonglong  , Offset )

    public:

        Q_INVOKABLE V2Cutting(QObject *parent = nullptr)
            : QObject( parent )
        {
        }

        void Copy( const V2Cutting *src )
        {
            m_Mark          = src->m_Mark   ;
            m_Offset        = src->m_Offset ;
        }

    private:
        Q_DISABLE_COPY(V2Cutting);
};

Q_DECLARE_METATYPE(V2Cutting*)

#endif
