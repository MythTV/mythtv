//////////////////////////////////////////////////////////////////////////////
// Program Name: envInfo.h
// Created     : Dec. 15, 2015
//
// Copyright (c) 2015 Bill Meek, from: 2010 David Blain <dblain@mythtv.org>
//
// Licensed under the GPL v2 or later, see LICENSE for details
//
//////////////////////////////////////////////////////////////////////////////

#ifndef ENVINFO_H_
#define ENVINFO_H_

#include <QString>

#include "serviceexp.h"
#include "datacontracthelper.h"

namespace DTC
{

class SERVICE_PUBLIC EnvInfo : public QObject
{
    Q_OBJECT

    Q_CLASSINFO( "version"    , "1.0" );

    Q_PROPERTY( QString LANG        READ LANG         WRITE setLANG        )
    Q_PROPERTY( QString LCALL       READ LCALL        WRITE setLCALL       )
    Q_PROPERTY( QString LCCTYPE     READ LCCTYPE      WRITE setLCCTYPE     )
    Q_PROPERTY( QString HOME        READ HOME         WRITE setHOME        )
    Q_PROPERTY( QString MYTHCONFDIR READ MYTHCONFDIR  WRITE setMYTHCONFDIR )

    PROPERTYIMP_REF( QString, LANG        )
    PROPERTYIMP_REF( QString, LCALL       )
    PROPERTYIMP_REF( QString, LCCTYPE     )
    PROPERTYIMP_REF( QString, HOME        )
    PROPERTYIMP_REF( QString, MYTHCONFDIR );

    public:

        static inline void InitializeCustomTypes();

        Q_INVOKABLE EnvInfo(QObject *parent = nullptr)
            : QObject       ( parent ),
              m_LANG        ( ""     ),
              m_LCALL       ( ""     ),
              m_LCCTYPE     ( ""     ),
              m_HOME        ( ""     ),
              m_MYTHCONFDIR ( ""     )
        {
        }

        void Copy( const EnvInfo *src )
        {
            m_LANG        = src->m_LANG;
            m_LCALL       = src->m_LCALL;
            m_LCCTYPE     = src->m_LCCTYPE;
            m_HOME        = src->m_HOME;
            m_MYTHCONFDIR = src->m_MYTHCONFDIR;
        }

    private:
        Q_DISABLE_COPY(EnvInfo);
};

using EnvInfoPtr = EnvInfo*;

inline void EnvInfo::InitializeCustomTypes()
{
    qRegisterMetaType< EnvInfo* >();
}

} // namespace DTC

#endif
