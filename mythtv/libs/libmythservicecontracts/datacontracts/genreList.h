//////////////////////////////////////////////////////////////////////////////
// Program Name: genreList.h
// Created     : Mar. 08, 2017
//
// Copyright (c) 2017 Paul Harrison <pharrison@mythtv.org>
//
// Licensed under the GPL v2 or later, see COPYING for details
//
//////////////////////////////////////////////////////////////////////////////

#ifndef GENRELIST_H_
#define GENRELIST_H_

#include <QString>
#include <QVariantList>

#include "serviceexp.h"
#include "datacontracthelper.h"

#include "genre.h"

namespace DTC
{

class SERVICE_PUBLIC GenreList : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "version", "1.0" );

    // Q_CLASSINFO Used to augment Metadata for properties.
    // See datacontracthelper.h for details

    Q_CLASSINFO( "GenreList", "type=DTC::Genre");

    Q_PROPERTY( QVariantList GenreList     READ Genres DESIGNABLE true )

    PROPERTYIMP_RO_REF( QVariantList, Genres );

    public:

        GenreList(QObject *parent = 0)
            : QObject         ( parent )
        {
        }

        void Copy( const GenreList *src )
        {
            CopyListContents< Genre >( this, m_Genres, src->m_Genres );
        }

        Genre *AddNewGenre()
        {
            // We must make sure the object added to the QVariantList has
            // a parent of 'this'

            Genre *pObject = new Genre( this );
            m_Genres.append( QVariant::fromValue<QObject *>( pObject ));

            return pObject;
        }

    private:
        Q_DISABLE_COPY(GenreList);
};

} // namespace DTC

#endif
