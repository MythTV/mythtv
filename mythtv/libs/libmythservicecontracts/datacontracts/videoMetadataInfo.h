//////////////////////////////////////////////////////////////////////////////
// Program Name: videoMetadataInfo.h
// Created     : Apr. 21, 2011
//
// Copyright (c) 2010 Robert McNamara <rmcnamara@mythtv.org>
//
// This library is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; either
// version 2.1 of the License, or at your option any later version of the LGPL.
//
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public
// License along with this library.  If not, see <http://www.gnu.org/licenses/>.
//
//////////////////////////////////////////////////////////////////////////////

#ifndef VIDEOMETADATAINFO_H_
#define VIDEOMETADATAINFO_H_

#include <QString>
#include <QDateTime>

#include "serviceexp.h"
#include "datacontracthelper.h"

namespace DTC
{

/////////////////////////////////////////////////////////////////////////////

class SERVICE_PUBLIC VideoMetadataInfo : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "version"    , "1.0" );

    Q_PROPERTY( int             Id              READ Id               WRITE setId             )
    Q_PROPERTY( QString         Title           READ Title            WRITE setTitle          )
    Q_PROPERTY( QString         SubTitle        READ SubTitle         WRITE setSubTitle       )
    Q_PROPERTY( QString         Tagline         READ Tagline          WRITE setTagline        )
    Q_PROPERTY( QString         Director        READ Director         WRITE setDirector       )
    Q_PROPERTY( QString         Studio          READ Studio           WRITE setStudio         )
    Q_PROPERTY( QString         Description     READ Description      WRITE setDescription    )
    Q_PROPERTY( QString         Certification   READ Certification    WRITE setCertification  )
    Q_PROPERTY( QString         InetRef         READ InetRef          WRITE setInetRef        )
    Q_PROPERTY( QString         HomePage        READ HomePage         WRITE setHomePage       )
    Q_PROPERTY( QDateTime       ReleaseDate     READ ReleaseDate      WRITE setReleaseDate    )
    Q_PROPERTY( QDateTime       AddDate         READ AddDate          WRITE setAddDate        )
    Q_PROPERTY( float           UserRating      READ UserRating       WRITE setUserRating     )
    Q_PROPERTY( int             Length          READ Length           WRITE setLength         )
    Q_PROPERTY( int             Season          READ Season           WRITE setSeason         )
    Q_PROPERTY( int             Episode         READ Episode          WRITE setEpisode        )
    Q_PROPERTY( int             ParentalLevel   READ ParentalLevel    WRITE setParentalLevel  )
    Q_PROPERTY( bool            Visible         READ Visible          WRITE setVisible        )
    Q_PROPERTY( bool            Watched         READ Watched          WRITE setWatched        )
    Q_PROPERTY( bool            Processed       READ Processed        WRITE setProcessed      )
    Q_PROPERTY( QString         FileName        READ FileName         WRITE setFileName       )
    Q_PROPERTY( QString         Hash            READ Hash             WRITE setHash           )
    Q_PROPERTY( QString         Host            READ Host             WRITE setHost           )
    Q_PROPERTY( QString         Coverart        READ Coverart         WRITE setCoverart       )
    Q_PROPERTY( QString         Fanart          READ Fanart           WRITE setFanart         )
    Q_PROPERTY( QString         Banner          READ Banner           WRITE setBanner         )
    Q_PROPERTY( QString         Screenshot      READ Screenshot       WRITE setScreenshot     )
    Q_PROPERTY( QString         Trailer         READ Trailer          WRITE setTrailer        )

    PROPERTYIMP    ( int        , Id             )
    PROPERTYIMP    ( QString    , Title          )
    PROPERTYIMP    ( QString    , SubTitle       )
    PROPERTYIMP    ( QString    , Tagline        )
    PROPERTYIMP    ( QString    , Director       )
    PROPERTYIMP    ( QString    , Studio         )
    PROPERTYIMP    ( QString    , Description    )
    PROPERTYIMP    ( QString    , Certification  )
    PROPERTYIMP    ( QString    , InetRef        )
    PROPERTYIMP    ( QString    , HomePage       )
    PROPERTYIMP    ( QDateTime  , ReleaseDate    )
    PROPERTYIMP    ( QDateTime  , AddDate        )
    PROPERTYIMP    ( float      , UserRating     )
    PROPERTYIMP    ( int        , Length         )
    PROPERTYIMP    ( int        , Season         )
    PROPERTYIMP    ( int        , Episode        )
    PROPERTYIMP    ( int        , ParentalLevel  )
    PROPERTYIMP    ( bool       , Visible        )
    PROPERTYIMP    ( bool       , Watched        )
    PROPERTYIMP    ( bool       , Processed      )
    PROPERTYIMP    ( QString    , FileName       )
    PROPERTYIMP    ( QString    , Hash           )
    PROPERTYIMP    ( QString    , Host           )
    PROPERTYIMP    ( QString    , Coverart       )
    PROPERTYIMP    ( QString    , Fanart         )
    PROPERTYIMP    ( QString    , Banner         )
    PROPERTYIMP    ( QString    , Screenshot     )
    PROPERTYIMP    ( QString    , Trailer        )

    public:

        static void InitializeCustomTypes()
        {
            qRegisterMetaType< VideoMetadataInfo  >();
            qRegisterMetaType< VideoMetadataInfo* >();
        }

    public:

        VideoMetadataInfo(QObject *parent = 0)
                        : QObject         ( parent ),
                          m_Id            ( 0      )
        {
        }

        VideoMetadataInfo( const VideoMetadataInfo &src )
        {
            Copy( src );
        }

        void Copy( const VideoMetadataInfo &src )
        {
            m_Id            = src.m_Id            ;
        }
};

} // namespace DTC

Q_DECLARE_METATYPE( DTC::VideoMetadataInfo  )
Q_DECLARE_METATYPE( DTC::VideoMetadataInfo* )

#endif
