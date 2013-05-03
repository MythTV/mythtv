//////////////////////////////////////////////////////////////////////////////
// Program Name: captureCardList.h
// Created     : Sep. 21, 2011
//
// Copyright (c) 2011 Robert McNamara <rmcnamara@mythtv.org>
//
// Licensed under the GPL v2 or later, see COPYING for details
//
//////////////////////////////////////////////////////////////////////////////

#ifndef CAPTURECARDLIST_H_
#define CAPTURECARDLIST_H_

#include <QString>

#include "serviceexp.h"
#include "datacontracthelper.h"

#include "captureCard.h"

namespace DTC
{

class SERVICE_PUBLIC CaptureCardList : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "version", "1.0" );

    // Q_CLASSINFO Used to augment Metadata for properties. 
    // See datacontracthelper.h for details

    Q_CLASSINFO( "CaptureCards", "type=DTC::CaptureCard");

    Q_PROPERTY( QVariantList CaptureCards READ CaptureCards DESIGNABLE true )

    PROPERTYIMP_RO_REF( QVariantList, CaptureCards )

    public:

        static inline void InitializeCustomTypes();

    public:

        CaptureCardList(QObject *parent = 0)
            : QObject( parent )
        {
        }

        CaptureCardList( const CaptureCardList &src )
        {
            Copy( src );
        }

        void Copy( const CaptureCardList &src )
        {
            CopyListContents< CaptureCard >( this, m_CaptureCards, src.m_CaptureCards );
        }

        CaptureCard *AddNewCaptureCard()
        {
            // We must make sure the object added to the QVariantList has
            // a parent of 'this'

            CaptureCard *pObject = new CaptureCard( this );
            m_CaptureCards.append( QVariant::fromValue<QObject *>( pObject ));

            return pObject;
        }

};

} // namespace DTC

Q_DECLARE_METATYPE( DTC::CaptureCardList  )
Q_DECLARE_METATYPE( DTC::CaptureCardList* )

namespace DTC
{
inline void CaptureCardList::InitializeCustomTypes()
{
    qRegisterMetaType< CaptureCardList  >();
    qRegisterMetaType< CaptureCardList* >();

    CaptureCard::InitializeCustomTypes();
}
}

#endif
