//////////////////////////////////////////////////////////////////////////////
// Program Name: frontendList.h
// Created     : May. 30, 2014
//
// Copyright (c) 2014 Stuart Morgan <smorgan@mythtv.org>
//
// Licensed under the GPL v2 or later, see LICENSE for details
//
//////////////////////////////////////////////////////////////////////////////

#ifndef FRONTENDLIST_H_
#define FRONTENDLIST_H_

#include <QVariantList>

#include "libmythservicecontracts/serviceexp.h"
#include "libmythservicecontracts/datacontracthelper.h"

#include "frontend.h"

namespace DTC
{

class SERVICE_PUBLIC FrontendList : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "version", "1.0" );

    // Q_CLASSINFO Used to augment Metadata for properties.
    // See datacontracthelper.h for details

    Q_CLASSINFO( "Frontends", "type=DTC::Frontend");

    Q_PROPERTY( QVariantList Frontends READ Frontends )

    PROPERTYIMP_RO_REF( QVariantList, Frontends );

    public:

        static inline void InitializeCustomTypes();

        Q_INVOKABLE FrontendList(QObject *parent = nullptr)
            : QObject( parent )
        {
        }

        void Copy( const FrontendList *src )
        {
            CopyListContents< Frontend >( this, m_Frontends, src->m_Frontends );
        }

        Frontend *AddNewFrontend()
        {
            // We must make sure the object added to the QVariantList has
            // a parent of 'this'

            auto *pObject = new Frontend( this );
            m_Frontends.append( QVariant::fromValue<QObject *>( pObject ));

            return pObject;
        }

    private:
        Q_DISABLE_COPY(FrontendList);
};

inline void FrontendList::InitializeCustomTypes()
{
    qRegisterMetaType< FrontendList* >();

    Frontend::InitializeCustomTypes();
}

} // namespace DTC

#endif
