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

namespace DTC
{

/////////////////////////////////////////////////////////////////////////////

class SERVICE_PUBLIC VideoMetadataInfo : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "version"    , "2.0" );

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

    Q_PROPERTY( QObject*        Artwork         READ Artwork     DESIGNABLE SerializeArtwork  )
    Q_PROPERTY( QObject*        Cast            READ Cast        DESIGNABLE SerializeCast     )

    PROPERTYIMP    ( int        , Id             )
    PROPERTYIMP    ( QString    , Title          )
    PROPERTYIMP    ( QString    , SubTitle       )
    PROPERTYIMP    ( QString    , Tagline        )
    PROPERTYIMP    ( QString    , Director       )
    PROPERTYIMP    ( QString    , Studio         )
    PROPERTYIMP    ( QString    , Description    )
    PROPERTYIMP    ( QString    , Certification  )
    PROPERTYIMP    ( QString    , Inetref        )
    PROPERTYIMP    ( int        , Collectionref  )
    PROPERTYIMP    ( QString    , HomePage       )
    PROPERTYIMP    ( QDateTime  , ReleaseDate    )
    PROPERTYIMP    ( QDateTime  , AddDate        )
    PROPERTYIMP    ( float      , UserRating     )
    PROPERTYIMP    ( int        , Length         )
    PROPERTYIMP    ( int        , PlayCount      )
    PROPERTYIMP    ( int        , Season         )
    PROPERTYIMP    ( int        , Episode        )
    PROPERTYIMP    ( int        , ParentalLevel  )
    PROPERTYIMP    ( bool       , Visible        )
    PROPERTYIMP    ( bool       , Watched        )
    PROPERTYIMP    ( bool       , Processed      )
    PROPERTYIMP    ( QString    , ContentType    )
    PROPERTYIMP    ( QString    , FileName       )
    PROPERTYIMP    ( QString    , Hash           )
    PROPERTYIMP    ( QString    , HostName       )
    PROPERTYIMP    ( QString    , Coverart       )
    PROPERTYIMP    ( QString    , Fanart         )
    PROPERTYIMP    ( QString    , Banner         )
    PROPERTYIMP    ( QString    , Screenshot     )
    PROPERTYIMP    ( QString    , Trailer        )

    PROPERTYIMP_PTR( ArtworkInfoList, Artwork    )
    PROPERTYIMP_PTR( CastMemberList , Cast       )

    PROPERTYIMP    ( bool      , SerializeArtwork)
    PROPERTYIMP    ( bool      , SerializeCast   )

    public:

        static inline void InitializeCustomTypes();

    public:

        VideoMetadataInfo(QObject *parent = 0)
                        : QObject         ( parent ),
                          m_Id            ( 0      ),
                          m_Collectionref ( 0      ),
                          m_UserRating    ( 0      ),
                          m_Length        ( 0      ),
                          m_PlayCount     ( 0      ),
                          m_Season        ( 0      ),
                          m_Episode       ( 0      ),
                          m_ParentalLevel ( 0      ),
                          m_Visible       ( false  ),
                          m_Watched       ( false  ),
                          m_Processed     ( false  ),
                          m_Artwork       ( NULL   ),
                          m_Cast          ( NULL   ),
                          m_SerializeArtwork( true ),
                          m_SerializeCast   ( true )
        {
        }

        VideoMetadataInfo( const VideoMetadataInfo &src )
        {
            Copy( src );
        }

        void Copy( const VideoMetadataInfo &src )
        {
            m_Id               = src.m_Id;
            m_SerializeArtwork = src.m_SerializeArtwork;
            m_SerializeCast    = src.m_SerializeCast;

            if ( src.m_Artwork != NULL)
                Artwork()->Copy( src.m_Artwork );

            if (src.m_Cast != NULL)
                Cast()->Copy( src.m_Cast );
        }
};

} // namespace DTC

Q_DECLARE_METATYPE( DTC::VideoMetadataInfo  )
Q_DECLARE_METATYPE( DTC::VideoMetadataInfo* )

namespace DTC
{
inline void VideoMetadataInfo::InitializeCustomTypes()
{
    qRegisterMetaType< VideoMetadataInfo  >();
    qRegisterMetaType< VideoMetadataInfo* >();

    if (QMetaType::type( "DTC::ArtworkInfoList" ) == 0)
        ArtworkInfoList::InitializeCustomTypes();

    if (QMetaType::type( "DTC::CastMemberList" ) == 0)
        CastMemberList::InitializeCustomTypes();
}
}

#endif
