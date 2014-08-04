//////////////////////////////////////////////////////////////////////////////
// Program Name: enum.h
// Created     : July 25, 2014
//
// Copyright (c) 2014 David Blain <dblain@mythtv.org>
//
// Licensed under the GPL v2 or later, see COPYING for details
//
//////////////////////////////////////////////////////////////////////////////

#ifndef ENUMLIST_H_
#define ENUMLIST_H_

#include <QVariantList>

#include "serviceexp.h"
#include "datacontracthelper.h"

#include "enumItem.h"

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
    Q_PROPERTY( QVariantList EnumItems READ EnumItems  DESIGNABLE true )


    PROPERTYIMP       ( QString     , Type      )
    PROPERTYIMP_RO_REF( QVariantList, EnumItems )

    public:

        static inline void InitializeCustomTypes();

    public:

        Enum(QObject *parent = 0)
            : QObject( parent )
        {
        }

        Enum( const Enum &src )
        {
            Copy( src );
        }

        void Copy( const Enum &src )
        {
            m_Type = src.m_Type;

            CopyListContents< EnumItem >( this, m_EnumItems, src.m_EnumItems );
        }

        EnumItem *AddNewEnum()
        {
            // We must make sure the object added to the QVariantList has
            // a parent of 'this'

            EnumItem *pObject = new EnumItem( this );
            m_EnumItems.append( QVariant::fromValue<QObject *>( pObject ));

            return pObject;
        }

};

} // namespace DTC

Q_DECLARE_METATYPE( DTC::Enum  )
Q_DECLARE_METATYPE( DTC::Enum* )

namespace DTC
{
inline void Enum::InitializeCustomTypes()
{
    qRegisterMetaType< Enum  >();
    qRegisterMetaType< Enum* >();

    EnumItem::InitializeCustomTypes();
}
}

#endif
