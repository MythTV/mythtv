//////////////////////////////////////////////////////////////////////////////
// Program Name: VideoLookupInfo.h
// Created     : Jul. 19, 2011
//
// Copyright (c) 2011 Robert McNamara <rmcnamara@mythtv.org>
//
// Licensed under the GPL v2 or later, see COPYING for details
//
//////////////////////////////////////////////////////////////////////////////

#ifndef V2VIDEOLOOKUPINFO_H_
#define V2VIDEOLOOKUPINFO_H_

#include <QString>
#include <QDateTime>

#include "libmythbase/http/mythhttpservice.h"


/////////////////////////////////////////////////////////////////////////////

class V2ArtworkItem : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "Version"    , "1.02" );

    SERVICE_PROPERTY2 ( QString    , Type           )
    SERVICE_PROPERTY2 ( QString    , Url            )
    SERVICE_PROPERTY2 ( QString    , Thumbnail      )
    SERVICE_PROPERTY2 ( int        , Width          )
    SERVICE_PROPERTY2 ( int        , Height          );

    public:


        Q_INVOKABLE V2ArtworkItem(QObject *parent = nullptr)
                        : QObject         ( parent )
        {
        }

        void Copy( const V2ArtworkItem *src )
        {
            m_Type             = src->m_Type             ;
            m_Url              = src->m_Url              ;
            m_Thumbnail        = src->m_Thumbnail        ;
            m_Width            = src->m_Width            ;
            m_Height           = src->m_Height           ;
        }

    private:
        Q_DISABLE_COPY(V2ArtworkItem);
};

/////////////////////////////////////////////////////////////////////////////

class V2VideoLookup : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "Version"    , "1.0" );

    Q_CLASSINFO( "Artwork", "type=V2ArtworkItem");

    SERVICE_PROPERTY2 ( QString    , Title          )
    SERVICE_PROPERTY2 ( QString    , SubTitle       )
    SERVICE_PROPERTY2 ( int        , Season         )
    SERVICE_PROPERTY2 ( int        , Episode        )
    SERVICE_PROPERTY2 ( int        , Year           )
    SERVICE_PROPERTY2 ( QString    , Tagline        )
    SERVICE_PROPERTY2 ( QString    , Description    )
    SERVICE_PROPERTY2 ( QString    , Certification  )
    SERVICE_PROPERTY2 ( QString    , Inetref        )
    SERVICE_PROPERTY2 ( QString    , Collectionref  )
    SERVICE_PROPERTY2 ( QString    , HomePage       )
    SERVICE_PROPERTY2 ( QDateTime  , ReleaseDate    )
    SERVICE_PROPERTY2 ( float      , UserRating     )
    SERVICE_PROPERTY2 ( int        , Length         )
    SERVICE_PROPERTY2 ( QString    , Language       )
    SERVICE_PROPERTY2 ( QStringList, Countries      )
    SERVICE_PROPERTY2 ( float      , Popularity     )
    SERVICE_PROPERTY2 ( int        , Budget         )
    SERVICE_PROPERTY2 ( int        , Revenue        )
    SERVICE_PROPERTY2 ( QString    , IMDB           )
    SERVICE_PROPERTY2 ( QString    , TMSRef         )
    SERVICE_PROPERTY2 ( QVariantList, Artwork)

    public:


        Q_INVOKABLE V2VideoLookup(QObject *parent = nullptr)
                        : QObject         ( parent )
        {
        }

        void Copy( const V2VideoLookup *src )
        {
            m_Title            = src->m_Title            ;
            m_SubTitle         = src->m_SubTitle         ;
            m_Season           = src->m_Season           ;
            m_Episode          = src->m_Episode          ;
            m_Year             = src->m_Year             ;
            m_Tagline          = src->m_Tagline          ;
            m_Description      = src->m_Description      ;
            m_Certification    = src->m_Certification    ;
            m_Inetref          = src->m_Inetref          ;
            m_Collectionref    = src->m_Collectionref    ;
            m_HomePage         = src->m_HomePage         ;
            m_ReleaseDate      = src->m_ReleaseDate      ;
            m_UserRating       = src->m_UserRating       ;
            m_Length           = src->m_Length           ;
            m_Popularity       = src->m_Popularity       ;
            m_Budget           = src->m_Budget           ;
            m_Revenue          = src->m_Revenue          ;
            m_IMDB             = src->m_IMDB             ;
            m_TMSRef           = src->m_TMSRef           ;

            CopyListContents< V2ArtworkItem >( this, m_Artwork, src->m_Artwork );
        }

        V2ArtworkItem *AddNewArtwork()
        {
            auto *pObject = new V2ArtworkItem( this );
            m_Artwork.append( QVariant::fromValue<QObject *>( pObject ));

            return pObject;
        }

};

Q_DECLARE_METATYPE(V2ArtworkItem*)
Q_DECLARE_METATYPE(V2VideoLookup*)


#endif
