//////////////////////////////////////////////////////////////////////////////
// Program Name: markup.h
// Created     : Apr. 4, 2021
//
// Copyright (c) 2021 team MythTV
//
// Licensed under the GPL v2 or later, see LICENSE for details
//
//////////////////////////////////////////////////////////////////////////////

#ifndef _MARKUP_H_
#define _MARKUP_H_

#include <QString>
#include <QVariantList>

#include "libmythservicecontracts/serviceexp.h"
#include "libmythservicecontracts/datacontracthelper.h"

namespace DTC
{
Q_NAMESPACE

/////////////////////////////////////////////////////////////////////////////

class SERVICE_PUBLIC Markup : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "version"    , "1.0" );

    Q_PROPERTY( QString  Type    READ Type         WRITE setType  )
    Q_PROPERTY( quint64  Frame   READ Frame        WRITE setFrame )
    Q_PROPERTY( QString  Data    READ Data         WRITE setData  )

    PROPERTYIMP_REF( QString   , Type  )
    PROPERTYIMP    ( quint64   , Frame )
    PROPERTYIMP_REF( QString   , Data  )

    public:

        static inline void InitializeCustomTypes();

        Q_INVOKABLE Markup(QObject *parent = nullptr)
            : QObject(parent), m_Frame(0)
        {
        }

        void Copy( const Markup *src )
        {
            m_Type          = src->m_Type  ;
            m_Frame         = src->m_Frame ;
            m_Data          = src->m_Data  ;
        }

    private:
        Q_DISABLE_COPY(Markup);
};

inline void Markup::InitializeCustomTypes()
{
    qRegisterMetaType< Markup* >();
}

} // namespace DTC

#endif
