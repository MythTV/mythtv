//////////////////////////////////////////////////////////////////////////////
// Program Name: videoMetadataInfoList.h
// Created     : Apr. 21, 2011
//
// Copyright (c) 2011 Robert McNamara <rmcnamara@mythtv.org>
//
// Licensed under the GPL v2 or later, see LICENSE for details
//
//////////////////////////////////////////////////////////////////////////////

#ifndef VIDEOMETADATAINFOLIST_H_
#define VIDEOMETADATAINFOLIST_H_

#include <QVariantList>

#include "serviceexp.h"
#include "datacontracthelper.h"

#include "videoMetadataInfo.h"

namespace DTC
{

class SERVICE_PUBLIC VideoMetadataInfoList : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "version", "1.02" );

    // Q_CLASSINFO Used to augment Metadata for properties. 
    // See datacontracthelper.h for details

    Q_CLASSINFO( "VideoMetadataInfos", "type=DTC::VideoMetadataInfo");
    Q_CLASSINFO( "AsOf"              , "transient=true" );

    Q_PROPERTY( int          StartIndex     READ StartIndex      WRITE setStartIndex     )
    Q_PROPERTY( int          Count          READ Count           WRITE setCount          )
    Q_PROPERTY( int          CurrentPage    READ CurrentPage     WRITE setCurrentPage    )
    Q_PROPERTY( int          TotalPages     READ TotalPages      WRITE setTotalPages     )
    Q_PROPERTY( int          TotalAvailable READ TotalAvailable  WRITE setTotalAvailable )
    Q_PROPERTY( QDateTime    AsOf           READ AsOf            WRITE setAsOf           )
    Q_PROPERTY( QString      Version        READ Version         WRITE setVersion        )
    Q_PROPERTY( QString      ProtoVer       READ ProtoVer        WRITE setProtoVer       )

    Q_PROPERTY( QVariantList VideoMetadataInfos READ VideoMetadataInfos )

    PROPERTYIMP       ( int         , StartIndex      )
    PROPERTYIMP       ( int         , Count           )
    PROPERTYIMP       ( int         , CurrentPage     )
    PROPERTYIMP       ( int         , TotalPages      )
    PROPERTYIMP       ( int         , TotalAvailable  )
    PROPERTYIMP_REF   ( QDateTime   , AsOf            )
    PROPERTYIMP_REF   ( QString     , Version         )
    PROPERTYIMP_REF   ( QString     , ProtoVer        )

    PROPERTYIMP_RO_REF( QVariantList, VideoMetadataInfos );

    public:

        static inline void InitializeCustomTypes();

        Q_INVOKABLE VideoMetadataInfoList(QObject *parent = nullptr)
            : QObject( parent ),
              m_StartIndex    ( 0      ),
              m_Count         ( 0      ),
              m_CurrentPage   ( 0      ),
              m_TotalPages    ( 0      ),
              m_TotalAvailable( 0      )
        {
        }

        void Copy( const VideoMetadataInfoList *src )
        {
            m_StartIndex    = src->m_StartIndex     ;
            m_Count         = src->m_Count          ;
            m_TotalAvailable= src->m_TotalAvailable ;
            m_AsOf          = src->m_AsOf           ;
            m_Version       = src->m_Version        ;
            m_ProtoVer      = src->m_ProtoVer       ;

            CopyListContents< VideoMetadataInfo >( this, m_VideoMetadataInfos, src->m_VideoMetadataInfos );
        }

        VideoMetadataInfo *AddNewVideoMetadataInfo()
        {
            // We must make sure the object added to the QVariantList has
            // a parent of 'this'

            auto *pObject = new VideoMetadataInfo( this );
            m_VideoMetadataInfos.append( QVariant::fromValue<QObject *>( pObject ));

            return pObject;
        }

    private:
        Q_DISABLE_COPY(VideoMetadataInfoList);
};

inline void VideoMetadataInfoList::InitializeCustomTypes()
{
    qRegisterMetaType< VideoMetadataInfoList* >();

    VideoMetadataInfo::InitializeCustomTypes();
}

} // namespace DTC

#endif
