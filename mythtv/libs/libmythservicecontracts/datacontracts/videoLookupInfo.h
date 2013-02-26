//////////////////////////////////////////////////////////////////////////////
// Program Name: VideoLookupInfo.h
// Created     : Jul. 19, 2011
//
// Copyright (c) 2011 Robert McNamara <rmcnamara@mythtv.org>
//
// Licensed under the GPL v2 or later, see COPYING for details
//
//////////////////////////////////////////////////////////////////////////////

#ifndef VIDEOLOOKUPINFO_H_
#define VIDEOLOOKUPINFO_H_

#include <QString>
#include <QDateTime>

#include "serviceexp.h"
#include "datacontracthelper.h"

namespace DTC
{

/////////////////////////////////////////////////////////////////////////////

class SERVICE_PUBLIC ArtworkItem : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "version"    , "1.02" );

    Q_PROPERTY( QString         Type            READ Type             WRITE setType           )
    Q_PROPERTY( QString         Url             READ Url              WRITE setUrl            )
    Q_PROPERTY( QString         Thumbnail       READ Thumbnail        WRITE setThumbnail      )
    Q_PROPERTY( int             Width           READ Width            WRITE setWidth          )
    Q_PROPERTY( int             Height          READ Height           WRITE setHeight         )

    PROPERTYIMP    ( QString    , Type           )
    PROPERTYIMP    ( QString    , Url            )
    PROPERTYIMP    ( QString    , Thumbnail      )
    PROPERTYIMP    ( int        , Width          )
    PROPERTYIMP    ( int        , Height          )

    public:

        static inline void InitializeCustomTypes();

    public:

        ArtworkItem(QObject *parent = 0)
                        : QObject         ( parent )
        {
        }

        ArtworkItem( const ArtworkItem &src )
        {
            Copy( src );
        }

        void Copy( const ArtworkItem &src )
        {
            m_Type             = src.m_Type             ;
            m_Url              = src.m_Url              ;
            m_Thumbnail        = src.m_Thumbnail        ;
            m_Width            = src.m_Width            ;
            m_Height           = src.m_Height           ;
        }
};

/////////////////////////////////////////////////////////////////////////////

class SERVICE_PUBLIC VideoLookup : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "version"    , "1.0" );

    Q_CLASSINFO( "Artwork", "type=DTC::ArtworkItem");

    Q_PROPERTY( QString         Title           READ Title            WRITE setTitle          )
    Q_PROPERTY( QString         SubTitle        READ SubTitle         WRITE setSubTitle       )
    Q_PROPERTY( int             Season          READ Season           WRITE setSeason         )
    Q_PROPERTY( int             Episode         READ Episode          WRITE setEpisode        )
    Q_PROPERTY( int             Year            READ Year             WRITE setYear           )
    Q_PROPERTY( QString         Tagline         READ Tagline          WRITE setTagline        )
    Q_PROPERTY( QString         Description     READ Description      WRITE setDescription    )
    Q_PROPERTY( QString         Certification   READ Certification    WRITE setCertification  )
    Q_PROPERTY( QString         Inetref         READ Inetref          WRITE setInetref        )
    Q_PROPERTY( QString         Collectionref   READ Collectionref    WRITE setCollectionref  )
    Q_PROPERTY( QString         HomePage        READ HomePage         WRITE setHomePage       )
    Q_PROPERTY( QDateTime       ReleaseDate     READ ReleaseDate      WRITE setReleaseDate    )
    Q_PROPERTY( float           UserRating      READ UserRating       WRITE setUserRating     )
    Q_PROPERTY( int             Length          READ Length           WRITE setLength         )
    Q_PROPERTY( QString         Language        READ Language         WRITE setLanguage       )
    Q_PROPERTY( QStringList     Countries       READ Countries        WRITE setCountries      )
    Q_PROPERTY( int             Popularity      READ Popularity       WRITE setPopularity     )
    Q_PROPERTY( int             Budget          READ Budget           WRITE setBudget         )
    Q_PROPERTY( int             Revenue         READ Revenue          WRITE setRevenue        )
    Q_PROPERTY( QString         IMDB            READ IMDB             WRITE setIMDB           )
    Q_PROPERTY( QString         TMSRef          READ TMSRef           WRITE setTMSRef         )

    Q_PROPERTY( QVariantList Artwork    READ Artwork DESIGNABLE true )

    PROPERTYIMP    ( QString    , Title          )
    PROPERTYIMP    ( QString    , SubTitle       )
    PROPERTYIMP    ( int        , Season         )
    PROPERTYIMP    ( int        , Episode        )
    PROPERTYIMP    ( int        , Year           )
    PROPERTYIMP    ( QString    , Tagline        )
    PROPERTYIMP    ( QString    , Description    )
    PROPERTYIMP    ( QString    , Certification  )
    PROPERTYIMP    ( QString    , Inetref        )
    PROPERTYIMP    ( QString    , Collectionref  )
    PROPERTYIMP    ( QString    , HomePage       )
    PROPERTYIMP    ( QDateTime  , ReleaseDate    )
    PROPERTYIMP    ( float      , UserRating     )
    PROPERTYIMP    ( int        , Length         )
    PROPERTYIMP    ( QString    , Language       )
    PROPERTYIMP    ( QStringList, Countries      )
    PROPERTYIMP    ( int        , Popularity     )
    PROPERTYIMP    ( int        , Budget         )
    PROPERTYIMP    ( int        , Revenue        )
    PROPERTYIMP    ( QString    , IMDB           )
    PROPERTYIMP    ( QString    , TMSRef         )

    PROPERTYIMP_RO_REF( QVariantList, Artwork)

    public:

        static inline void InitializeCustomTypes();

    public:

        VideoLookup(QObject *parent = 0)
                        : QObject         ( parent )
        {
        }

        VideoLookup( const VideoLookup &src )
        {
            Copy( src );
        }

        void Copy( const VideoLookup &src )
        {
            m_Title            = src.m_Title            ;
            m_SubTitle         = src.m_SubTitle         ;
            m_Season           = src.m_Season           ;
            m_Episode          = src.m_Episode          ;
            m_Year             = src.m_Year             ;
            m_Tagline          = src.m_Tagline          ;
            m_Description      = src.m_Description      ;
            m_Certification    = src.m_Certification    ;
            m_Inetref          = src.m_Inetref          ;
            m_Collectionref    = src.m_Collectionref    ;
            m_HomePage         = src.m_HomePage         ;
            m_ReleaseDate      = src.m_ReleaseDate      ;
            m_UserRating       = src.m_UserRating       ;
            m_Length           = src.m_Length           ;

            CopyListContents< ArtworkItem >( this, m_Artwork, src.m_Artwork );
        }

        ArtworkItem *AddNewArtwork()
        {
            ArtworkItem *pObject = new ArtworkItem( this );
            Artwork().append( QVariant::fromValue<QObject *>( pObject ));

            return pObject;
        }

};

} // namespace DTC

Q_DECLARE_METATYPE( DTC::VideoLookup  )
Q_DECLARE_METATYPE( DTC::VideoLookup* )
Q_DECLARE_METATYPE( DTC::ArtworkItem  )
Q_DECLARE_METATYPE( DTC::ArtworkItem* )

namespace DTC
{
inline void ArtworkItem::InitializeCustomTypes()
{
    qRegisterMetaType< ArtworkItem    >();
    qRegisterMetaType< ArtworkItem*   >();
}

inline void VideoLookup::InitializeCustomTypes()
{
    qRegisterMetaType< VideoLookup  >();
    qRegisterMetaType< VideoLookup* >();

    ArtworkItem::InitializeCustomTypes();
}
}

#endif
