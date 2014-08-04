//////////////////////////////////////////////////////////////////////////////
// Program Name: enumItem.h
// Created     : July 25, 2014
//
// Copyright (c) 2014 David Blain <dblain@mythtv.org>
//
// Licensed under the GPL v2 or later, see COPYING for details
//
//////////////////////////////////////////////////////////////////////////////

#ifndef ENUMS_H_
#define ENUMS_H_

#include "serviceexp.h"
#include "datacontracthelper.h"

namespace DTC
{

class SERVICE_PUBLIC EnumItem : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "version", "1.02" );

    Q_PROPERTY( QString         Key             READ Key         WRITE setKey    )
    Q_PROPERTY( int             Value           READ Value       WRITE setValue  )
    Q_PROPERTY( QString         Desc            READ Desc        WRITE setDesc   )

    PROPERTYIMP    ( QString    , Key       )
    PROPERTYIMP    ( int        , Value     )
    PROPERTYIMP    ( QString    , Desc      )

    public:

        static inline void InitializeCustomTypes();

    public:

        EnumItem( QObject *parent = 0)
          : QObject ( parent ),
            m_Value ( 0      )
        {
        }

        EnumItem( const EnumItem &src )
        {
            Copy( src );
        }

        void Copy( const EnumItem &src )
        {
            m_Key           = src.m_Key  ;
            m_Value         = src.m_Value;
            m_Desc          = src.m_Desc ;
        }
};

}  // namespace

Q_DECLARE_METATYPE( DTC::EnumItem  )
Q_DECLARE_METATYPE( DTC::EnumItem* )

namespace DTC
{
inline void EnumItem::InitializeCustomTypes()
{
    qRegisterMetaType< EnumItem  >();
    qRegisterMetaType< EnumItem* >();
}
}  // namespace



#endif
