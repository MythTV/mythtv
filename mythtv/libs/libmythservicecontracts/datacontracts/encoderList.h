//////////////////////////////////////////////////////////////////////////////
// Program Name: encoderList.h
// Created     : Jan. 15, 2010
//
// Copyright (c) 2010 David Blain <dblain@mythtv.org>
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

    // We need to know the type that will ultimately be contained in 
    // any QVariantList or QVariantMap.  We do his by specifying
    // A Q_CLASSINFO entry with "<PropName>_type" as the key
    // and the type name as the value

    Q_CLASSINFO( "Encoders_type", "DTC::Encoder");

    Q_PROPERTY( QVariantList Encoders READ Encoders DESIGNABLE true )

    PROPERTYIMP_RO_REF( QVariantList, Encoders )

    public:

        static void InitializeCustomTypes()
        {
            qRegisterMetaType< EncoderList  >();
            qRegisterMetaType< EncoderList* >();

            Encoder::InitializeCustomTypes();
        }

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

#endif
