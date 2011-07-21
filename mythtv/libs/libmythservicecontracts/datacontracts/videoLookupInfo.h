//////////////////////////////////////////////////////////////////////////////
// Program Name: videoLookupInfo.h
// Created     : Jul. 19, 2011
//
// Copyright (c) 2011 Robert McNamara <rmcnamara@mythtv.org>
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

#ifndef VIDEOLOOKUPINFO_H_
#define VIDEOLOOKUPINFO_H_

#include <QString>
#include <QDateTime>

#include "serviceexp.h"
#include "datacontracthelper.h"

namespace DTC
{

/////////////////////////////////////////////////////////////////////////////

class SERVICE_PUBLIC VideoLookupInfo : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "version"    , "1.0" );

    Q_PROPERTY( QString         Title           READ Title            WRITE setTitle          )
    Q_PROPERTY( QString         SubTitle        READ SubTitle         WRITE setSubTitle       )
    Q_PROPERTY( int             Season          READ Season           WRITE setSeason         )
    Q_PROPERTY( int             Episode         READ Episode          WRITE setEpisode        )
    Q_PROPERTY( int             Year            READ Year             WRITE setYear        )
    Q_PROPERTY( QString         Tagline         READ Tagline          WRITE setTagline        )
    Q_PROPERTY( QString         Description     READ Description      WRITE setDescription    )
    Q_PROPERTY( QString         Certification   READ Certification    WRITE setCertification  )
    Q_PROPERTY( QString         InetRef         READ InetRef          WRITE setInetRef        )
    Q_PROPERTY( QString         HomePage        READ HomePage         WRITE setHomePage       )
    Q_PROPERTY( QDateTime       ReleaseDate     READ ReleaseDate      WRITE setReleaseDate    )
    Q_PROPERTY( float           UserRating      READ UserRating       WRITE setUserRating     )
    Q_PROPERTY( int             Length          READ Length           WRITE setLength         )

    PROPERTYIMP    ( QString    , Title          )
    PROPERTYIMP    ( QString    , SubTitle       )
    PROPERTYIMP    ( int        , Season         )
    PROPERTYIMP    ( int        , Episode        )
    PROPERTYIMP    ( int        , Year           )
    PROPERTYIMP    ( QString    , Tagline        )
    PROPERTYIMP    ( QString    , Description    )
    PROPERTYIMP    ( QString    , Certification  )
    PROPERTYIMP    ( QString    , InetRef        )
    PROPERTYIMP    ( QString    , HomePage       )
    PROPERTYIMP    ( QDateTime  , ReleaseDate    )
    PROPERTYIMP    ( float      , UserRating     )
    PROPERTYIMP    ( int        , Length         )

    public:

        static void InitializeCustomTypes()
        {
            qRegisterMetaType< VideoLookupInfo  >();
            qRegisterMetaType< VideoLookupInfo* >();
        }

    public:

        VideoLookupInfo(QObject *parent = 0)
                        : QObject         ( parent )
        {
        }

        VideoLookupInfo( const VideoLookupInfo &src )
        {
            Copy( src );
        }

        void Copy( const VideoLookupInfo &src )
        {
            m_Title            = src.m_Title            ;
            m_SubTitle         = src.m_SubTitle         ;
            m_Season           = src.m_Season           ;
            m_Episode          = src.m_Episode          ;
            m_Year             = src.m_Year             ;
            m_Tagline          = src.m_Tagline          ;
            m_Description      = src.m_Description      ;
            m_Certification    = src.m_Certification    ;
            m_InetRef          = src.m_InetRef          ;
            m_HomePage         = src.m_HomePage         ;
            m_ReleaseDate      = src.m_ReleaseDate      ;
            m_UserRating       = src.m_UserRating       ;
            m_Length           = src.m_Length           ;
        }
};

} // namespace DTC

Q_DECLARE_METATYPE( DTC::VideoLookupInfo  )
Q_DECLARE_METATYPE( DTC::VideoLookupInfo* )

#endif
