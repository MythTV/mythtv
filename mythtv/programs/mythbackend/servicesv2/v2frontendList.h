//////////////////////////////////////////////////////////////////////////////
// Program Name: frontendList.h
// Created     : May. 30, 2014
//
// Copyright (c) 2014 Stuart Morgan <smorgan@mythtv.org>
//
// Licensed under the GPL v2 or later, see COPYING for details
//
//////////////////////////////////////////////////////////////////////////////

#ifndef V2FRONTENDLIST_H_
#define V2FRONTENDLIST_H_

#include "libmythbase/http/mythhttpservice.h"
#include "v2frontend.h"


class V2FrontendList : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "version", "1.0" );

    // Q_CLASSINFO Used to augment Metadata for properties.
    // See datacontracthelper.h for details

    Q_CLASSINFO( "Frontends", "type=V2Frontend");

    SERVICE_PROPERTY2( QVariantList, Frontends );

    public:

        Q_INVOKABLE V2FrontendList(QObject *parent = nullptr)
            : QObject( parent )
        {
        }

        void Copy( const V2FrontendList *src )
        {
            CopyListContents< V2Frontend >( this, m_Frontends, src->m_Frontends );
        }

        // This is needed so that common routines can get a non-const m_Encoders
        // reference
        QVariantList& GetFrontends() {return m_Frontends;}
        V2Frontend *AddNewFrontend()
        {
            // We must make sure the object added to the QVariantList has
            // a parent of 'this'

            auto *pObject = new V2Frontend( this );
            m_Frontends.append( QVariant::fromValue<QObject *>( pObject ));

            return pObject;
        }

    private:
        Q_DISABLE_COPY(V2FrontendList);
};

Q_DECLARE_METATYPE(V2FrontendList*)

#endif
