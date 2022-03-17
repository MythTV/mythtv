//////////////////////////////////////////////////////////////////////////////
// Program Name: enum.h
// Created     : July 25, 2014
//
// Copyright (c) 2014 David Blain <dblain@mythtv.org>
//
// Licensed under the GPL v2 or later, see LICENSE for details
//
//////////////////////////////////////////////////////////////////////////////

#ifndef ENUMLIST_H_
#define ENUMLIST_H_

#include <QVariantList>

#include "libmythservicecontracts/serviceexp.h"
#include "libmythservicecontracts/datacontracthelper.h"
#include "libmythservicecontracts/datacontracts/enumItem.h"

namespace DTC
{

class SERVICE_PUBLIC Enum : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "version", "1.0" );

    // Q_CLASSINFO Used to augment Metadata for properties.
    // See datacontracthelper.h for details

    Q_CLASSINFO( "EnumItems", "type=DTC::Enum");

    Q_PROPERTY( QString      Type      READ Type       WRITE setType   )
    Q_PROPERTY( QVariantList EnumItems READ EnumItems )


    PROPERTYIMP_REF   ( QString     , Type      )
    PROPERTYIMP_RO_REF( QVariantList, EnumItems )

    public:

        static inline void InitializeCustomTypes();

        Q_INVOKABLE explicit Enum(QObject *parent = nullptr)
            : QObject( parent )
        {
        }

        void Copy( const Enum *src )
        {
            m_Type = src->m_Type;

            CopyListContents< EnumItem >( this, m_EnumItems, src->m_EnumItems );
        }

        EnumItem *AddNewEnum()
        {
            // We must make sure the object added to the QVariantList has
            // a parent of 'this'

            auto *pObject = new EnumItem( this );
            m_EnumItems.append( QVariant::fromValue<QObject *>( pObject ));

            return pObject;
        }

    private:
        Q_DISABLE_COPY(Enum);
};

inline void Enum::InitializeCustomTypes()
{
    qRegisterMetaType< Enum* >();

    EnumItem::InitializeCustomTypes();
}

} // namespace DTC

#endif
