//////////////////////////////////////////////////////////////////////////////
// Program Name: enumItem.h
// Created     : July 25, 2014
//
// Copyright (c) 2014 David Blain <dblain@mythtv.org>
//
// Licensed under the GPL v2 or later, see LICENSE for details
//
//////////////////////////////////////////////////////////////////////////////

#ifndef ENUMS_H_
#define ENUMS_H_

#include "libmythservicecontracts/serviceexp.h"
#include "libmythservicecontracts/datacontracthelper.h"

namespace DTC
{

class SERVICE_PUBLIC EnumItem : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "version", "1.02" );

    Q_PROPERTY( QString         Key             READ Key         WRITE setKey    )
    Q_PROPERTY( int             Value           READ Value       WRITE setValue  )
    Q_PROPERTY( QString         Desc            READ Desc        WRITE setDesc   )

    PROPERTYIMP_REF( QString    , Key       )
    PROPERTYIMP    ( int        , Value     )
    PROPERTYIMP_REF( QString    , Desc      );

    public:

        static inline void InitializeCustomTypes();

        Q_INVOKABLE explicit EnumItem( QObject *parent = nullptr)
          : QObject ( parent ),
            m_Value ( 0      )
        {
        }

        void Copy( const EnumItem *src )
        {
            m_Key           = src->m_Key  ;
            m_Value         = src->m_Value;
            m_Desc          = src->m_Desc ;
        }

    private:
        Q_DISABLE_COPY(EnumItem);
};

inline void EnumItem::InitializeCustomTypes()
{
    qRegisterMetaType< EnumItem* >();
}

}  // namespace

#endif
