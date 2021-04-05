//////////////////////////////////////////////////////////////////////////////
// Program Name: markupList.h
// Created     : Apr. 4, 2021
//
// Copyright (c) 2021 team MythTV
//
// Licensed under the GPL v2 or later, see COPYING for details
//
//////////////////////////////////////////////////////////////////////////////

#ifndef MARKUPLIST_H_
#define MARKUPLIST_H_

#include <QString>
#include <QVariantList>

#include "serviceexp.h"
#include "datacontracthelper.h"

#include "markup.h"

namespace DTC
{

class SERVICE_PUBLIC MarkupList : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "version", "1.0" );

    // Q_CLASSINFO Used to augment Metadata for properties.
    // See datacontracthelper.h for details

    Q_CLASSINFO( "Markups", "type=DTC::Markup");

    Q_PROPERTY( QVariantList Mark READ Mark )
    Q_PROPERTY( QVariantList Seek READ Seek )

    PROPERTYIMP_RO_REF( QVariantList, Mark );
    PROPERTYIMP_RO_REF( QVariantList, Seek );

    public:

        static inline void InitializeCustomTypes();

        Q_INVOKABLE MarkupList(QObject *parent = nullptr)
            : QObject         ( parent )
        {
        }

        void Copy( const MarkupList *src )
        {
            CopyListContents< Markup >( this, m_Mark, src->m_Mark );
            CopyListContents< Markup >( this, m_Seek, src->m_Seek );
        }

        Markup *AddNewMarkup()
        {
            // We must make sure the object added to the QVariantList has
            // a parent of 'this'

            auto *pObject = new Markup( this );
            m_Mark.append( QVariant::fromValue<QObject *>( pObject ));

            return pObject;
        }

        Markup *AddNewSeek()
        {
            // We must make sure the object added to the QVariantList has
            // a parent of 'this'

            auto *pObject = new Markup( this );
            m_Seek.append( QVariant::fromValue<QObject *>( pObject ));

            return pObject;
        }

    private:
        Q_DISABLE_COPY(MarkupList);
};

inline void MarkupList::InitializeCustomTypes()
{
    qRegisterMetaType< MarkupList* >();

    Markup::InitializeCustomTypes();
}

} // namespace DTC

#endif
