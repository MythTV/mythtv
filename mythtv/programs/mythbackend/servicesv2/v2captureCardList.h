//////////////////////////////////////////////////////////////////////////////
// Program Name: captureCardList.h
// Created     : Sep. 21, 2011
//
// Copyright (c) 2011 Robert McNamara <rmcnamara@mythtv.org>
//
// Licensed under the GPL v2 or later, see COPYING for details
//
//////////////////////////////////////////////////////////////////////////////

#ifndef V2CAPTURECARDLIST_H_
#define V2CAPTURECARDLIST_H_

#include <QString>
#include <QVariantList>

#include "libmythbase/http/mythhttpservice.h"

#include "v2captureCard.h"

class V2CaptureCardList : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "Version", "1.0" );
    Q_CLASSINFO( "CaptureCards", "type=V2CaptureCard");

    SERVICE_PROPERTY2( QVariantList, CaptureCards );

    public:

        Q_INVOKABLE V2CaptureCardList(QObject *parent = nullptr)
            : QObject( parent )
        {
        }

        void Copy( const V2CaptureCardList *src )
        {
            CopyListContents< V2CaptureCard >( this, m_CaptureCards, src->m_CaptureCards );
        }

        V2CaptureCard *AddNewCaptureCard()
        {
            // We must make sure the object added to the QVariantList has
            // a parent of 'this'

            auto *pObject = new V2CaptureCard( this );
            m_CaptureCards.append( QVariant::fromValue<QObject *>( pObject ));

            return pObject;
        }

    private:
        Q_DISABLE_COPY(V2CaptureCardList);
};

Q_DECLARE_METATYPE(V2CaptureCardList*)

#endif
