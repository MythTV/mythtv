//////////////////////////////////////////////////////////////////////////////
// Program Name: videoMetadataInfoList.h
// Created     : Apr. 21, 2011
//
// Copyright (c) 2011 Robert McNamara <rmcnamara@mythtv.org>
//
// Licensed under the GPL v2 or later, see COPYING for details
//
//////////////////////////////////////////////////////////////////////////////

#ifndef V2VIDEOMETADATAINFOLIST_H_
#define V2VIDEOMETADATAINFOLIST_H_

#include <QVariantList>

#include "libmythbase/http/mythhttpservice.h"

#include "v2videoMetadataInfo.h"

class V2VideoMetadataInfoList : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "Version", "1.02" );

    Q_CLASSINFO( "VideoMetadataInfos", "type=V2VideoMetadataInfo");
    Q_CLASSINFO( "AsOf"              , "transient=true" );

    SERVICE_PROPERTY2       ( int         , StartIndex      )
    SERVICE_PROPERTY2       ( int         , Count           )
    SERVICE_PROPERTY2       ( int         , CurrentPage     )
    SERVICE_PROPERTY2       ( int         , TotalPages      )
    SERVICE_PROPERTY2       ( int         , TotalAvailable  )
    SERVICE_PROPERTY2   ( QDateTime   , AsOf            )
    SERVICE_PROPERTY2   ( QString     , Version         )
    SERVICE_PROPERTY2   ( QString     , ProtoVer        )
    SERVICE_PROPERTY2   (QVariantList, VideoMetadataInfos)

    public:

        Q_INVOKABLE V2VideoMetadataInfoList(QObject *parent = nullptr)
            : QObject( parent )
        {
        }

        void Copy( const V2VideoMetadataInfoList *src )
        {
            m_StartIndex    = src->m_StartIndex     ;
            m_Count         = src->m_Count          ;
            m_TotalAvailable= src->m_TotalAvailable ;
            m_AsOf          = src->m_AsOf           ;
            m_Version       = src->m_Version        ;
            m_ProtoVer      = src->m_ProtoVer       ;

            CopyListContents< V2VideoMetadataInfo >( this, m_VideoMetadataInfos, src->m_VideoMetadataInfos );
        }

        V2VideoMetadataInfo *AddNewVideoMetadataInfo()
        {
            // We must make sure the object added to the QVariantList has
            // a parent of 'this'

            auto *pObject = new V2VideoMetadataInfo( this );
            m_VideoMetadataInfos.append( QVariant::fromValue<QObject *>( pObject ));

            return pObject;
        }

    private:
        Q_DISABLE_COPY(V2VideoMetadataInfoList);
};

Q_DECLARE_METATYPE(V2VideoMetadataInfoList*)


#endif
