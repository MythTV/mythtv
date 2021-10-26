//////////////////////////////////////////////////////////////////////////////
// Program Name: encoderList.h
// Created     : Jan. 15, 2010
//
// Copyright (c) 2010 David Blain <dblain@mythtv.org>
//                                          
// Licensed under the GPL v2 or later, see LICENSE for details
//
//////////////////////////////////////////////////////////////////////////////

#ifndef ENCODERLIST_H_
#define ENCODERLIST_H_

#include <QVariantList>

#include "serviceexp.h" 
#include "datacontracthelper.h"

#include "encoder.h"

namespace DTC
{

class SERVICE_PUBLIC EncoderList : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "version", "1.0" );

    // Q_CLASSINFO Used to augment Metadata for properties. 
    // See datacontracthelper.h for details

    Q_CLASSINFO( "Encoders", "type=DTC::Encoder");

    Q_PROPERTY( QVariantList Encoders READ Encoders )

    PROPERTYIMP_RO_REF( QVariantList, Encoders );

    public:

        static inline void InitializeCustomTypes();

        Q_INVOKABLE EncoderList(QObject *parent = nullptr)
            : QObject( parent )               
        {
        }

        void Copy( const EncoderList *src )
        {
            CopyListContents< Encoder >( this, m_Encoders, src->m_Encoders );
        }

        Encoder *AddNewEncoder()
        {
            // We must make sure the object added to the QVariantList has
            // a parent of 'this'

            auto *pObject = new Encoder( this );
            m_Encoders.append( QVariant::fromValue<QObject *>( pObject ));

            return pObject;
        }

    private:
        Q_DISABLE_COPY(EncoderList);
};

inline void EncoderList::InitializeCustomTypes()
{
    qRegisterMetaType< EncoderList* >();

    Encoder::InitializeCustomTypes();
}

} // namespace DTC

#endif
