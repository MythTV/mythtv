//////////////////////////////////////////////////////////////////////////////
// Program Name: musicMetadataInfo.h
// Created     : July 20, 2017
//
// Copyright (c) 2017 Paul Harrison <pharrison@mythtv.org>
//
// Licensed under the GPL v2 or later, see COPYING for details
//
//////////////////////////////////////////////////////////////////////////////

#ifndef MUSICMETADATAINFO_H_
#define MUSICMETADATAINFO_H_

#include <QString>
#include <QDateTime>

#include "serviceexp.h"
#include "datacontracthelper.h"

namespace DTC
{

/////////////////////////////////////////////////////////////////////////////

class SERVICE_PUBLIC MusicMetadataInfo : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "version"    , "1.00" );

    Q_PROPERTY( int             Id                  READ Id                 WRITE setId                 )
    Q_PROPERTY( QString         Artist              READ Artist             WRITE setArtist             )
    Q_PROPERTY( QString         CompilationArtist   READ CompilationArtist  WRITE setCompilationArtist  )
    Q_PROPERTY( QString         Album               READ Album              WRITE setAlbum              )
    Q_PROPERTY( QString         Title               READ Title              WRITE setTitle              )
    Q_PROPERTY( int             TrackNo             READ TrackNo            WRITE setTrackNo            )
    Q_PROPERTY( QString         Genre               READ Genre              WRITE setGenre              )
    Q_PROPERTY( int             Year                READ Year               WRITE setYear               )
    Q_PROPERTY( int             PlayCount           READ PlayCount          WRITE setPlayCount          )
    Q_PROPERTY( int             Length              READ Length             WRITE setLength             )
    Q_PROPERTY( int             Rating              READ Rating             WRITE setRating             )
    Q_PROPERTY( QString         FileName            READ FileName           WRITE setFileName           )
    Q_PROPERTY( QString         HostName            READ HostName           WRITE setHostName           )
    Q_PROPERTY( QDateTime       LastPlayed          READ LastPlayed         WRITE setLastPlayed         )
    Q_PROPERTY( bool            Compilation         READ Compilation        WRITE setCompilation        )

    PROPERTYIMP    ( int        , Id                )
    PROPERTYIMP_REF( QString    , Artist            )
    PROPERTYIMP_REF( QString    , CompilationArtist )
    PROPERTYIMP_REF( QString    , Album             )
    PROPERTYIMP_REF( QString    , Title             )
    PROPERTYIMP    ( int        , TrackNo           )
    PROPERTYIMP_REF( QString    , Genre             )
    PROPERTYIMP    ( int        , Year              )
    PROPERTYIMP    ( int        , PlayCount         )
    PROPERTYIMP    ( int        , Length            )
    PROPERTYIMP    ( int        , Rating            )
    PROPERTYIMP_REF( QString    , FileName          )
    PROPERTYIMP_REF( QString    , HostName          )
    PROPERTYIMP_REF( QDateTime  , LastPlayed        )
    PROPERTYIMP    ( bool       , Compilation       )

    public:

        static inline void InitializeCustomTypes();

        Q_INVOKABLE MusicMetadataInfo(QObject *parent = nullptr)
                        : QObject         ( parent ),
                          m_Id            ( 0      ),
                          m_TrackNo       ( 0      ),
                          m_Year          ( 0      ),
                          m_PlayCount     ( 0      ),
                          m_Length        ( 0      ),
                          m_Rating        ( 0      ),
                          m_Compilation   ( false  )
        {
        }

        void Copy( const MusicMetadataInfo *src )
        {
            m_Id                 = src->m_Id;
            m_Artist             = src->m_Artist;
            m_CompilationArtist  = src->m_CompilationArtist;
            m_Album              = src->m_Album;
            m_Title              = src->m_Title;
            m_TrackNo            = src->m_TrackNo;
            m_Genre              = src->m_Genre;
            m_Year               = src->m_Year;
            m_PlayCount          = src->m_PlayCount;
            m_Length             = src->m_Length;
            m_Rating             = src->m_Rating;
            m_FileName           = src->m_FileName;
            m_HostName           = src->m_HostName;
            m_LastPlayed         = src->m_LastPlayed;
            m_Compilation        = src->m_Compilation;
        }
};

inline void MusicMetadataInfo::InitializeCustomTypes()
{
    qRegisterMetaType< MusicMetadataInfo* >();
}

} // namespace DTC

#endif
