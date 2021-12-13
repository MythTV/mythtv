//////////////////////////////////////////////////////////////////////////////
// Program Name: musicMetadataInfoList.h
// Created     : July 20, 2017
//
// Copyright (c) 2017 Paul Harrison <pharrison@mythtv.org>
//
// Licensed under the GPL v2 or later, see COPYING for details
//
//////////////////////////////////////////////////////////////////////////////

#ifndef V2MUSICMETADATAINFOLIST_H_
#define V2MUSICMETADATAINFOLIST_H_

#include <QVariantList>

#include "libmythbase/http/mythhttpservice.h"
#include "v2musicMetadataInfo.h"


class V2MusicMetadataInfoList : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "Version", "1.00" );

    Q_CLASSINFO( "MusicMetadataInfos", "type=V2MusicMetadataInfo");
    Q_CLASSINFO( "AsOf"              , "transient=true" );

    SERVICE_PROPERTY2( int         , StartIndex      )
    SERVICE_PROPERTY2( int         , Count           )
    SERVICE_PROPERTY2( int         , CurrentPage     )
    SERVICE_PROPERTY2( int         , TotalPages      )
    SERVICE_PROPERTY2( int         , TotalAvailable  )
    SERVICE_PROPERTY2( QDateTime   , AsOf            )
    SERVICE_PROPERTY2( QString     , Version         )
    SERVICE_PROPERTY2( QString     , ProtoVer        )
    SERVICE_PROPERTY2( QVariantList, MusicMetadataInfos )

    public:

        Q_INVOKABLE V2MusicMetadataInfoList(QObject *parent = nullptr)
            : QObject( parent )
        {
        }

        void Copy( const V2MusicMetadataInfoList *src )
        {
            m_StartIndex    = src->m_StartIndex     ;
            m_Count         = src->m_Count          ;
            m_TotalAvailable= src->m_TotalAvailable ;
            m_AsOf          = src->m_AsOf           ;
            m_Version       = src->m_Version        ;
            m_ProtoVer      = src->m_ProtoVer       ;

            CopyListContents< V2MusicMetadataInfo >( this, m_MusicMetadataInfos, src->m_MusicMetadataInfos );
        }

        V2MusicMetadataInfo *AddNewMusicMetadataInfo()
        {
            // We must make sure the object added to the QVariantList has
            // a parent of 'this'

            auto *pObject = new V2MusicMetadataInfo( this );
            m_MusicMetadataInfos.append( QVariant::fromValue<QObject *>( pObject ));

            return pObject;
        }

};

Q_DECLARE_METATYPE(V2MusicMetadataInfoList*)

#endif
