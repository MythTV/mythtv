//////////////////////////////////////////////////////////////////////////////
// Program Name: input.h
// Created     : May. 30, 2014
//
// Copyright (c) 2014 Stuart Morgan <smorgan@mythtv.org>
//
// Licensed under the GPL v2 or later, see LICENSE for details
//
//////////////////////////////////////////////////////////////////////////////

#ifndef FRONTEND_H_
#define FRONTEND_H_

#include <QString>

#include "libmythservicecontracts/serviceexp.h"
#include "libmythservicecontracts/datacontracthelper.h"

namespace DTC
{

/////////////////////////////////////////////////////////////////////////////

class SERVICE_PUBLIC Frontend : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "version"    , "1.0" );

    Q_PROPERTY( QString  Name    READ  Name    WRITE  setName   )
    Q_PROPERTY( QString  IP      READ  IP      WRITE  setIP     )
    Q_PROPERTY( int      Port    READ  Port    WRITE  setPort   )
    Q_PROPERTY( int      OnLine  READ  OnLine  WRITE  setOnLine )

    PROPERTYIMP_REF( QString    , Name            )
    PROPERTYIMP_REF( QString    , IP              )
    PROPERTYIMP    ( int        , Port            )
    PROPERTYIMP    ( bool       , OnLine          )

    public:

        static inline void InitializeCustomTypes();

        Q_INVOKABLE Frontend(QObject *parent = nullptr)
            : QObject         ( parent ),
            m_Port(0), m_OnLine(false)
        {
        }

        void Copy( const Frontend *src )
        {
            m_Name            = src->m_Name;
            m_IP              = src->m_IP;
            m_Port            = src->m_Port;
            m_OnLine          = src->m_OnLine;
        }

    private:
        Q_DISABLE_COPY(Frontend);
};

inline void Frontend::InitializeCustomTypes()
{
    qRegisterMetaType< Frontend* >();
}

} // namespace DTC

#endif
