//////////////////////////////////////////////////////////////////////////////
// Program Name: inputList.h
// Created     : Nov. 20, 2013
//
// Copyright (c) 2013 Stuart Morgan <smorgan@mythtv.org>
//
// Licensed under the GPL v2 or later, see COPYING for details
//
//////////////////////////////////////////////////////////////////////////////

#ifndef V2INPUTLIST_H_
#define V2INPUTLIST_H_

#include <QVariantList>

#include "libmythbase/http/mythhttpservice.h"

#include "v2input.h"

class V2InputList : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "Version", "1.0" );
    Q_CLASSINFO( "Inputs", "type=V2Input");

    SERVICE_PROPERTY2( QVariantList, Inputs );

    public:

        Q_INVOKABLE V2InputList(QObject *parent = nullptr)
            : QObject( parent )
        {
        }

        void Copy( const V2InputList *src )
        {
            CopyListContents< V2Input >( this, m_Inputs, src->m_Inputs );
        }

        V2Input *AddNewInput()
        {
            // We must make sure the object added to the QVariantList has
            // a parent of 'this'

            auto *pObject = new V2Input( this );
            m_Inputs.append( QVariant::fromValue<QObject *>( pObject ));

            return pObject;
        }

    private:
        Q_DISABLE_COPY(V2InputList);
};

Q_DECLARE_METATYPE(V2InputList*)

#endif
