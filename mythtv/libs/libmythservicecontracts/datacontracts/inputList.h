//////////////////////////////////////////////////////////////////////////////
// Program Name: inputList.h
// Created     : Nov. 20, 2013
//
// Copyright (c) 2013 Stuart Morgan <smorgan@mythtv.org>
//
// Licensed under the GPL v2 or later, see COPYING for details
//
//////////////////////////////////////////////////////////////////////////////

#ifndef INPUTLIST_H_
#define INPUTLIST_H_

#include <QVariantList>

#include "serviceexp.h"
#include "datacontracthelper.h"

#include "input.h"

namespace DTC
{

class SERVICE_PUBLIC InputList : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "version", "1.0" );

    // Q_CLASSINFO Used to augment Metadata for properties.
    // See datacontracthelper.h for details

    Q_CLASSINFO( "Inputs", "type=DTC::Input");

    Q_PROPERTY( QVariantList Inputs READ Inputs DESIGNABLE true )

    PROPERTYIMP_RO_REF( QVariantList, Inputs )

    public:

        static inline void InitializeCustomTypes();

    public:

        InputList(QObject *parent = 0)
            : QObject( parent )
        {
        }

        InputList( const InputList &src )
        {
            Copy( src );
        }

        void Copy( const InputList &src )
        {
            CopyListContents< Input >( this, m_Inputs, src.m_Inputs );
        }

        Input *AddNewInput()
        {
            // We must make sure the object added to the QVariantList has
            // a parent of 'this'

            Input *pObject = new Input( this );
            m_Inputs.append( QVariant::fromValue<QObject *>( pObject ));

            return pObject;
        }

};

} // namespace DTC

Q_DECLARE_METATYPE( DTC::InputList  )
Q_DECLARE_METATYPE( DTC::InputList* )

namespace DTC
{
inline void InputList::InitializeCustomTypes()
{
    qRegisterMetaType< InputList  >();
    qRegisterMetaType< InputList* >();

    Input::InitializeCustomTypes();
}
}

#endif
