//////////////////////////////////////////////////////////////////////////////
// Program Name: encoderList.h
// Created     : Jan. 15, 2010
//
// Copyright (c) 2010 David Blain <dblain@mythtv.org>
//
// Licensed under the GPL v2 or later, see COPYING for details
//
//////////////////////////////////////////////////////////////////////////////

#ifndef V2ENCODERLIST_H_
#define V2ENCODERLIST_H_

#include <QVariantList>

#include "libmythbase/http/mythhttpservice.h"

#include "v2encoder.h"

class V2EncoderList : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "Version", "1.0" );

    // Q_CLASSINFO Used to augment Metadata for properties.
    // See datacontracthelper.h for details

    Q_CLASSINFO( "Encoders", "type=V2Encoder");

    SERVICE_PROPERTY2( QVariantList, Encoders );

    public:

        Q_INVOKABLE V2EncoderList(QObject *parent = nullptr)
            : QObject( parent )
        {
        }

        void Copy( const V2EncoderList *src )
        {
            CopyListContents< V2Encoder >( this, m_Encoders, src->m_Encoders );
        }
        // This is needed so that common routines can get a non-const m_Encoders
        // reference
        QVariantList& GetEncoders() {return m_Encoders;}
        V2Encoder *AddNewEncoder()
        {
            // We must make sure the object added to the QVariantList has
            // a parent of 'this'

            auto *pObject = new V2Encoder( this );
            m_Encoders.append( QVariant::fromValue<QObject *>( pObject ));

            return pObject;
        }

    private:
        Q_DISABLE_COPY(V2EncoderList);
};

Q_DECLARE_METATYPE(V2EncoderList*)

#endif
