//////////////////////////////////////////////////////////////////////////////
// Program Name: videoMetadataInfo.h
// Created     : Apr. 21, 2011
//
// Copyright (c) 2010 Robert McNamara <rmcnamara@mythtv.org>
// Copyright (c) 2013 Stuart Morgan   <smorgan@mythtv.org>
//
// Licensed under the GPL v2 or later, see COPYING for details
//
//////////////////////////////////////////////////////////////////////////////

#ifndef VIDEOMETADATAINFO_H_
#define VIDEOMETADATAINFO_H_

#include <QString>
#include <QDateTime>

#include "serviceexp.h"
#include "datacontracthelper.h"
#include "artworkInfoList.h"
#include "castMemberList.h"
#include "genreList.h"

namespace DTC
{

/////////////////////////////////////////////////////////////////////////////

class SERVICE_PUBLIC VideoMetadataInfo : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "version"    , "2.01" );

    Q_PROPERTY( int             Id              READ Id               WRITE setId             )
    Q_PROPERTY( QString         Title           READ Title            WRITE setTitle          )
    Q_PROPERTY( QString         SubTitle        READ SubTitle         WRITE setSubTitle       )
    Q_PROPERTY( QString         Tagline         READ Tagline          WRITE setTagline        )
    Q_PROPERTY( QString         Director        READ Director         WRITE setDirector       )
    Q_PROPERTY( QString         Studio          READ Studio           WRITE setStudio         )
    Q_PROPERTY( QString         Description     READ Description      WRITE setDescription    )
    Q_PROPERTY( QString         Certification   READ Certification    WRITE setCertification  )
    Q_PROPERTY( QString         Inetref         READ Inetref          WRITE setInetref        )
    Q_PROPERTY( int             Collectionref   READ Collectionref    WRITE setCollectionref  )
    Q_PROPERTY( QString         HomePage        READ HomePage         WRITE setHomePage       )
    Q_PROPERTY( QDateTime       ReleaseDate     READ ReleaseDate      WRITE setReleaseDate    )
    Q_PROPERTY( QDateTime       AddDate         READ AddDate          WRITE setAddDate        )
    Q_PROPERTY( float           UserRating      READ UserRating       WRITE setUserRating     )
    Q_PROPERTY( int             ChildID         READ ChildID          WRITE setChildID        )
    Q_PROPERTY( int             Length          READ Length           WRITE setLength         )
    Q_PROPERTY( int             PlayCount       READ PlayCount        WRITE setPlayCount      )
    Q_PROPERTY( int             Season          READ Season           WRITE setSeason         )
    Q_PROPERTY( int             Episode         READ Episode          WRITE setEpisode        )
    Q_PROPERTY( int             ParentalLevel   READ ParentalLevel    WRITE setParentalLevel  )
    Q_PROPERTY( bool            Visible         READ Visible          WRITE setVisible        )
    Q_PROPERTY( bool            Watched         READ Watched          WRITE setWatched        )
    Q_PROPERTY( bool            Processed       READ Processed        WRITE setProcessed      )
    Q_PROPERTY( QString         ContentType     READ ContentType      WRITE setContentType    )
    Q_PROPERTY( QString         FileName        READ FileName         WRITE setFileName       )
    Q_PROPERTY( QString         Hash            READ Hash             WRITE setHash           )
    Q_PROPERTY( QString         HostName        READ HostName         WRITE setHostName       )
    Q_PROPERTY( QString         Coverart        READ Coverart         WRITE setCoverart       )
    Q_PROPERTY( QString         Fanart          READ Fanart           WRITE setFanart         )
    Q_PROPERTY( QString         Banner          READ Banner           WRITE setBanner         )
    Q_PROPERTY( QString         Screenshot      READ Screenshot       WRITE setScreenshot     )
    Q_PROPERTY( QString         Trailer         READ Trailer          WRITE setTrailer        )

    Q_PROPERTY( QObject*        Artwork         READ Artwork     )
    Q_PROPERTY( QObject*        Cast            READ Cast        )
    Q_PROPERTY( QObject*        Genres          READ Genres      )

    PROPERTYIMP    ( int        , Id             )
    PROPERTYIMP_REF( QString    , Title          )
    PROPERTYIMP_REF( QString    , SubTitle       )
    PROPERTYIMP_REF( QString    , Tagline        )
    PROPERTYIMP_REF( QString    , Director       )
    PROPERTYIMP_REF( QString    , Studio         )
    PROPERTYIMP_REF( QString    , Description    )
    PROPERTYIMP_REF( QString    , Certification  )
    PROPERTYIMP_REF( QString    , Inetref        )
    PROPERTYIMP    ( int        , Collectionref  )
    PROPERTYIMP_REF( QString    , HomePage       )
    PROPERTYIMP_REF( QDateTime  , ReleaseDate    )
    PROPERTYIMP_REF( QDateTime  , AddDate        )
    PROPERTYIMP    ( float      , UserRating     )
    PROPERTYIMP    ( int        , ChildID        )
    PROPERTYIMP    ( int        , Length         )
    PROPERTYIMP    ( int        , PlayCount      )
    PROPERTYIMP    ( int        , Season         )
    PROPERTYIMP    ( int        , Episode        )
    PROPERTYIMP    ( int        , ParentalLevel  )
    PROPERTYIMP    ( bool       , Visible        )
    PROPERTYIMP    ( bool       , Watched        )
    PROPERTYIMP    ( bool       , Processed      )
    PROPERTYIMP_REF( QString    , ContentType    )
    PROPERTYIMP_REF( QString    , FileName       )
    PROPERTYIMP_REF( QString    , Hash           )
    PROPERTYIMP_REF( QString    , HostName       )
    PROPERTYIMP_REF( QString    , Coverart       )
    PROPERTYIMP_REF( QString    , Fanart         )
    PROPERTYIMP_REF( QString    , Banner         )
    PROPERTYIMP_REF( QString    , Screenshot     )
    PROPERTYIMP_REF( QString    , Trailer        )

    PROPERTYIMP_PTR( ArtworkInfoList , Artwork   )
    PROPERTYIMP_PTR( CastMemberList  , Cast      )
    PROPERTYIMP_PTR( GenreList       , Genres    )

    PROPERTYIMP    ( bool      , SerializeArtwork)
    PROPERTYIMP    ( bool      , SerializeCast   )
    PROPERTYIMP    ( bool      , SerializeGenres )

    public:

        static inline void InitializeCustomTypes();

        Q_INVOKABLE VideoMetadataInfo(QObject *parent = nullptr)
                        : QObject         ( parent ),
                          m_Id            ( 0      ),
                          m_Collectionref ( 0      ),
                          m_UserRating    ( 0      ),
                          m_ChildID       ( 0      ),
                          m_Length        ( 0      ),
                          m_PlayCount     ( 0      ),
                          m_Season        ( 0      ),
                          m_Episode       ( 0      ),
                          m_ParentalLevel ( 0      ),
                          m_Visible       ( false  ),
                          m_Watched       ( false  ),
                          m_Processed     ( false  ),
                          m_Artwork       ( nullptr ),
                          m_Cast          ( nullptr ),
                          m_Genres        ( nullptr ),
                          m_SerializeArtwork( true ),
                          m_SerializeCast   ( true ),
                          m_SerializeGenres ( true )
        {
        }

        void Copy( const VideoMetadataInfo *src )
        {
            m_Id               = src->m_Id;
            m_SerializeArtwork = src->m_SerializeArtwork;
            m_SerializeCast    = src->m_SerializeCast;
            m_SerializeGenres  = src->m_SerializeGenres;

            if ( src->m_Artwork != nullptr)
                Artwork()->Copy( src->m_Artwork );

            if (src->m_Cast != nullptr)
                Cast()->Copy( src->m_Cast );

            if (src->m_Genres != nullptr)
                Genres()->Copy( src->m_Genres );

        }

    private:
        Q_DISABLE_COPY(VideoMetadataInfo);
};

inline void VideoMetadataInfo::InitializeCustomTypes()
{
    qRegisterMetaType< VideoMetadataInfo* >();

    if (qMetaTypeId<DTC::ArtworkInfoList*>() == QMetaType::UnknownType)
        ArtworkInfoList::InitializeCustomTypes();

    if (qMetaTypeId<DTC::CastMemberList*>() == QMetaType::UnknownType)
        CastMemberList::InitializeCustomTypes();

    if (qMetaTypeId<DTC::GenreList*>() == QMetaType::UnknownType)
        GenreList::InitializeCustomTypes();
}

} // namespace DTC

#endif
