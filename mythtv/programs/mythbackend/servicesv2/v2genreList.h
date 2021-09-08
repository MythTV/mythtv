//////////////////////////////////////////////////////////////////////////////
// Program Name: genreList.h
// Created     : Mar. 08, 2017
//
// Copyright (c) 2017 Paul Harrison <pharrison@mythtv.org>
//
// Licensed under the GPL v2 or later, see COPYING for details
//
//////////////////////////////////////////////////////////////////////////////

#ifndef V2GENRELIST_H_
#define V2GENRELIST_H_

#include <QString>
#include <QVariantList>

#include "libmythbase/http/mythhttpservice.h"


#include "v2genre.h"

class  V2GenreList : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "Version", "1.0" );

    // Q_CLASSINFO Used to augment Metadata for properties.
    // See mythhttpservice.h for details

    Q_CLASSINFO( "GenreList", "type=V2Genre");

    Q_PROPERTY( QVariantList GenreList     READ Genres USER true )

    SERVICE_PROPERTY_RO_REF( QVariantList, Genres );

    public:

        Q_INVOKABLE V2GenreList(QObject *parent = nullptr)
            : QObject         ( parent )
        {
        }

        void Copy( const V2GenreList *src )
        {
            CopyListContents< V2Genre >( this, m_Genres, src->m_Genres );
        }

        V2Genre *AddNewGenre()
        {
            // We must make sure the object added to the QVariantList has
            // a parent of 'this'

            auto *pObject = new V2Genre( this );
            m_Genres.append( QVariant::fromValue<QObject *>( pObject ));

            return pObject;
        }

    private:
        Q_DISABLE_COPY(V2GenreList);
};

Q_DECLARE_METATYPE(V2GenreList*)

#endif
