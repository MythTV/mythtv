//////////////////////////////////////////////////////////////////////////////
// Program Name: input.h
// Created     : Nov. 20, 2013
//
// Copyright (c) 2013 Stuart Morgan <smorgan@mythtv.org>
//
// Licensed under the GPL v2 or later, see COPYING for details
//
//////////////////////////////////////////////////////////////////////////////

#ifndef V2POWERPRIORITY_H
#define V2POWERPRIORITY_H

#include <QString>

#include "libmythbase/http/mythhttpservice.h"

/////////////////////////////////////////////////////////////////////////////

class V2PowerPriority : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "Version"    , "1.0" );

    SERVICE_PROPERTY2( QString    , PriorityName  )
    SERVICE_PROPERTY2( int        , RecPriority   )
    SERVICE_PROPERTY2( QString    , SelectClause  )

    public:

        Q_INVOKABLE V2PowerPriority(QObject *parent = nullptr)
            : QObject( parent )
        {
        }

    private:
        Q_DISABLE_COPY(V2PowerPriority);
};

Q_DECLARE_METATYPE(V2PowerPriority*)

class V2PowerPriorityList : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "Version", "1.0" );

    // Q_CLASSINFO Used to augment Metadata for properties.
    // See datacontracthelper.h for details

    Q_CLASSINFO( "PowerPriorities", "type=V2PowerPriority");

    SERVICE_PROPERTY2( QVariantList, PowerPriorities );

    public:

        Q_INVOKABLE V2PowerPriorityList(QObject *parent = nullptr)
            : QObject( parent )
        {
        }

        // This is needed so that common routines can get a non-const m_PowerPriorities
        // reference
        QVariantList& GetPowerPriorities() {return m_PowerPriorities;}
        V2PowerPriority *AddNewPowerPriority()
        {
            // We must make sure the object added to the QVariantList has
            // a parent of 'this'

            auto *pObject = new V2PowerPriority( this );
            m_PowerPriorities.append( QVariant::fromValue<QObject *>( pObject ));

            return pObject;
        }

    private:
        Q_DISABLE_COPY(V2PowerPriorityList);
};

Q_DECLARE_METATYPE(V2PowerPriorityList*)

#endif
