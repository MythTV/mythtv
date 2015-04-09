//////////////////////////////////////////////////////////////////////////////
// Program Name: cutting.h
// Created     : Mar. 09, 2014
//
// Copyright (c) 2014 team MythTV
//
// Licensed under the GPL v2 or later, see COPYING for details
//
//////////////////////////////////////////////////////////////////////////////

#ifndef CUTTING_H_
#define CUTTING_H_

#include <QString>
#include <QVariantList>

#include "serviceexp.h"
#include "datacontracthelper.h"

namespace DTC
{

class SERVICE_PUBLIC Cutting : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "version"    , "1.0" );

    Q_PROPERTY( int Mark          READ Mark         WRITE setMark   )
    Q_PROPERTY( qlonglong Offset  READ Offset       WRITE setOffset )

    PROPERTYIMP    ( int        , Mark   )
    PROPERTYIMP    ( qlonglong  , Offset )

    public:

        static inline void InitializeCustomTypes();

    public:

        Cutting(QObject *parent = 0)
            : QObject         ( parent )
        {
        }

        Cutting( const Cutting &src )
        {
            Copy( src );
        }

        void Copy( const Cutting &src )
        {
            m_Mark          = src.m_Mark   ;
            m_Offset        = src.m_Offset ;
        }
};

} // namespace DTC

Q_DECLARE_METATYPE( DTC::Cutting  )
Q_DECLARE_METATYPE( DTC::Cutting* )

namespace DTC
{
inline void Cutting::InitializeCustomTypes()
{
    qRegisterMetaType< Cutting  >();
    qRegisterMetaType< Cutting* >();
}
}

#endif
