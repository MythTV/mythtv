//////////////////////////////////////////////////////////////////////////////
// Program Name: cutList.h
// Created     : Mar. 09, 2014
//
// Copyright (c) 2014 team MythTV
//
// Licensed under the GPL v2 or later, see COPYING for details
//
//////////////////////////////////////////////////////////////////////////////

#ifndef CUTLIST_H_
#define CUTLIST_H_

#include <QString>
#include <QVariantList>

#include "serviceexp.h"
#include "datacontracthelper.h"

#include "cutting.h"

namespace DTC
{

class SERVICE_PUBLIC CutList : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "version", "1.0" );

    // Q_CLASSINFO Used to augment Metadata for properties.
    // See datacontracthelper.h for details

    Q_CLASSINFO( "Cuttings", "type=DTC::Cutting");

    Q_PROPERTY( QVariantList Cuttings READ Cuttings DESIGNABLE true )

    PROPERTYIMP_RO_REF( QVariantList, Cuttings );

    public:

        CutList(QObject *parent = 0)
            : QObject         ( parent )
        {
        }

        void Copy( const CutList *src )
        {
            CopyListContents< Cutting >( this, m_Cuttings, src->m_Cuttings );
        }

        Cutting *AddNewCutting()
        {
            // We must make sure the object added to the QVariantList has
            // a parent of 'this'

            Cutting *pObject = new Cutting( this );
            m_Cuttings.append( QVariant::fromValue<QObject *>( pObject ));

            return pObject;
        }

    private:
        Q_DISABLE_COPY(CutList);
};

} // namespace DTC

#endif
