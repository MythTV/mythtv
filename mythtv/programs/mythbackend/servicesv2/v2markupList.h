//////////////////////////////////////////////////////////////////////////////
// Program Name: markupList.h
// Created     : Apr. 4, 2021
//
// Copyright (c) 2021 team MythTV
//
// Licensed under the GPL v2 or later, see COPYING for details
//
//////////////////////////////////////////////////////////////////////////////

#ifndef V2MARKUPLIST_H_
#define V2MARKUPLIST_H_

#include <QString>
#include <QVariantList>

#include "libmythbase/http/mythhttpservice.h"

#include "v2markup.h"

class V2MarkupList : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "Version", "1.0" );

    Q_CLASSINFO( "Mark", "type=V2Markup");
    Q_CLASSINFO( "Seek", "type=V2Markup");

    SERVICE_PROPERTY2( QVariantList, Mark );
    SERVICE_PROPERTY2( QVariantList, Seek );

    public:

        Q_INVOKABLE V2MarkupList(QObject *parent = nullptr)
            : QObject         ( parent )
        {
        }

        void Copy( const V2MarkupList *src )
        {
            CopyListContents< V2Markup >( this, m_Mark, src->m_Mark );
            CopyListContents< V2Markup >( this, m_Seek, src->m_Seek );
        }

        V2Markup *AddNewMarkup()
        {
            // We must make sure the object added to the QVariantList has
            // a parent of 'this'

            auto *pObject = new V2Markup( this );
            m_Mark.append( QVariant::fromValue<QObject *>( pObject ));

            return pObject;
        }

        V2Markup *AddNewSeek()
        {
            // We must make sure the object added to the QVariantList has
            // a parent of 'this'

            auto *pObject = new V2Markup( this );
            m_Seek.append( QVariant::fromValue<QObject *>( pObject ));

            return pObject;
        }

    private:
        Q_DISABLE_COPY(V2MarkupList);
};

Q_DECLARE_METATYPE(V2MarkupList*)

#endif
