//////////////////////////////////////////////////////////////////////////////
// Program Name: musicMetadataInfo.h
// Created     : July 20, 2017
//
// Copyright (c) 2017 Paul Harrison <pharrison@mythtv.org>
//
// Licensed under the GPL v2 or later, see COPYING for details
//
//////////////////////////////////////////////////////////////////////////////

#ifndef V2MUSICMETADATAINFO_H_
#define V2MUSICMETADATAINFO_H_

#include <QString>
#include <QDateTime>

#include "libmythbase/http/mythhttpservice.h"


/////////////////////////////////////////////////////////////////////////////

class V2MusicMetadataInfo : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "Version"    , "1.00" );

    SERVICE_PROPERTY2( int        , Id                )
    SERVICE_PROPERTY2( QString    , Artist            )
    SERVICE_PROPERTY2( QString    , CompilationArtist )
    SERVICE_PROPERTY2( QString    , Album             )
    SERVICE_PROPERTY2( QString    , Title             )
    SERVICE_PROPERTY2( int        , TrackNo           )
    SERVICE_PROPERTY2( QString    , Genre             )
    SERVICE_PROPERTY2( int        , Year              )
    SERVICE_PROPERTY2( int        , PlayCount         )
    SERVICE_PROPERTY2( int        , Length            )
    SERVICE_PROPERTY2( int        , Rating            )
    SERVICE_PROPERTY2( QString    , FileName          )
    SERVICE_PROPERTY2( QString    , HostName          )
    SERVICE_PROPERTY2( QDateTime  , LastPlayed        )
    SERVICE_PROPERTY2( bool       , Compilation       )

    public:

        Q_INVOKABLE V2MusicMetadataInfo(QObject *parent = nullptr)
            : QObject( parent )
        {
        }

        void Copy( const V2MusicMetadataInfo *src )
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

Q_DECLARE_METATYPE(V2MusicMetadataInfo*)

#endif
