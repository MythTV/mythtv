//////////////////////////////////////////////////////////////////////////////
// Program Name: artworkInfoList.h
// Created     : Nov. 12, 2011
//
// Copyright (c) 2011 Robert McNamara <rmcnamara@mythtv.org>
//
// Licensed under the GPL v2 or later, see COPYING for details
//
//////////////////////////////////////////////////////////////////////////////

#ifndef V2ARTWORKINFOLIST_H_
#define V2ARTWORKINFOLIST_H_

#include <QString>
#include <QVariantList>

#include "libmythbase/http/mythhttpservice.h"

#include "v2artworkInfo.h"


class V2ArtworkInfoList : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "Version", "1.0" );

    // Q_CLASSINFO Used to augment Metadata for properties.
    // See datacontracthelper.h for details

    Q_CLASSINFO( "ArtworkInfos", "type=V2ArtworkInfo");

    SERVICE_PROPERTY2( QVariantList, ArtworkInfos );

    public:

        Q_INVOKABLE V2ArtworkInfoList(QObject *parent = nullptr)
            : QObject         ( parent )
        {
        }

        void Copy( const V2ArtworkInfoList *src )
        {
            CopyListContents< V2ArtworkInfo >( this, m_ArtworkInfos, src->m_ArtworkInfos );
        }

        V2ArtworkInfo *AddNewArtworkInfo()
        {
            // We must make sure the object added to the QVariantList has
            // a parent of 'this'

            auto *pObject = new V2ArtworkInfo( this );
            m_ArtworkInfos.append( QVariant::fromValue<QObject *>( pObject ));

            return pObject;
        }

    private:
        Q_DISABLE_COPY(V2ArtworkInfoList);
};

Q_DECLARE_METATYPE(V2ArtworkInfoList*)

#endif
