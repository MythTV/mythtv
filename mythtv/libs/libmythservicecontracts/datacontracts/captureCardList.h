//////////////////////////////////////////////////////////////////////////////
// Program Name: captureCardList.h
// Created     : Sep. 21, 2011
//
// Copyright (c) 2011 Robert McNamara <rmcnamara@mythtv.org>
//
// This library is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; either
// version 2.1 of the License, or at your option any later version of the LGPL.
//
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public
// License along with this library.  If not, see <http://www.gnu.org/licenses/>.
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

    // We need to know the type that will ultimately be contained in
    // any QVariantList or QVariantMap.  We do this by specifying
    // A Q_CLASSINFO entry with "<PropName>_type" as the key
    // and the type name as the value

    Q_CLASSINFO( "CaptureCards_type", "DTC::CaptureCard");

    Q_PROPERTY( QVariantList CaptureCards READ CaptureCards DESIGNABLE true )

    PROPERTYIMP_RO_REF( QVariantList, CaptureCards )

    public:

        static void InitializeCustomTypes()
        {
            qRegisterMetaType< CaptureCardList  >();
            qRegisterMetaType< CaptureCardList* >();

            CaptureCard::InitializeCustomTypes();
        }

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

#endif
