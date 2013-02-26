//////////////////////////////////////////////////////////////////////////////
// Program Name: encoderList.h
// Created     : Jan. 15, 2010
//
// Copyright (c) 2010 David Blain <dblain@mythtv.org>
//                                          
// Licensed under the GPL v2 or later, see COPYING for details
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

    Q_PROPERTY( QVariantList Encoders READ Encoders DESIGNABLE true )

    PROPERTYIMP_RO_REF( QVariantList, Encoders )

    public:

        static inline void InitializeCustomTypes();

    public:

        EncoderList(QObject *parent = 0) 
            : QObject( parent )               
        {
        }
        
        EncoderList( const EncoderList &src ) 
        {
            Copy( src );
        }

        void Copy( const EncoderList &src )
        {
            CopyListContents< Encoder >( this, m_Encoders, src.m_Encoders );
        }

        Encoder *AddNewEncoder()
        {
            // We must make sure the object added to the QVariantList has
            // a parent of 'this'

            Encoder *pObject = new Encoder( this );
            m_Encoders.append( QVariant::fromValue<QObject *>( pObject ));

            return pObject;
        }

};

} // namespace DTC

Q_DECLARE_METATYPE( DTC::EncoderList  )
Q_DECLARE_METATYPE( DTC::EncoderList* )

namespace DTC
{
inline void EncoderList::InitializeCustomTypes()
{
    qRegisterMetaType< EncoderList  >();
    qRegisterMetaType< EncoderList* >();

    Encoder::InitializeCustomTypes();
}
}

#endif
