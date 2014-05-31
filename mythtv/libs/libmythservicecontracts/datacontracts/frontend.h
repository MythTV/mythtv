//////////////////////////////////////////////////////////////////////////////
// Program Name: input.h
// Created     : May. 30, 2014
//
// Copyright (c) 2014 Stuart Morgan <smorgan@mythtv.org>
//
// Licensed under the GPL v2 or later, see COPYING for details
//
//////////////////////////////////////////////////////////////////////////////

#ifndef FRONTEND_H_
#define FRONTEND_H_

#include <QString>

#include "serviceexp.h"
#include "datacontracthelper.h"

namespace DTC
{

/////////////////////////////////////////////////////////////////////////////

class SERVICE_PUBLIC Frontend : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "version"    , "1.0" );

    Q_PROPERTY( QString  Name    READ  Name    WRITE  setName   )
    Q_PROPERTY( QString  IP      READ  IP      WRITE  setIP     )
    Q_PROPERTY( int      OnLine  READ  OnLine  WRITE  setOnLine )

    PROPERTYIMP    ( QString    , Name            )
    PROPERTYIMP    ( QString    , IP              )
    PROPERTYIMP    ( bool       , OnLine          )

    public:
        static inline void InitializeCustomTypes();

    public:

        Frontend(QObject *parent = 0)
            : QObject         ( parent ),
            m_OnLine(false)
        {
        }

        Frontend( const Frontend &src )
        {
            Copy( src );
        }

        void Copy( const Frontend &src )
        {
            m_Name            = src.m_Name;
            m_IP              = src.m_IP;
            m_OnLine = src.m_OnLine;
        }
};

} // namespace DTC

Q_DECLARE_METATYPE( DTC::Frontend  )
Q_DECLARE_METATYPE( DTC::Frontend* )

namespace DTC
{
inline void Frontend::InitializeCustomTypes()
{
    qRegisterMetaType< Frontend  >();
    qRegisterMetaType< Frontend* >();
}
}

#endif
