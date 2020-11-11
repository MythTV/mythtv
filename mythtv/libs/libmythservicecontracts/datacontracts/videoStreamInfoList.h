//////////////////////////////////////////////////////////////////////////////
// Program Name: videoStreamInfoList.h
// Created     : May. 30, 2020
//
// Copyright (c) 2011 Peter Bennett <pbennett@mythtv.org>
//
// Licensed under the GPL v2 or later, see COPYING for details
//
//////////////////////////////////////////////////////////////////////////////

#ifndef VIDEOSTREAMINFOLIST_H_
#define VIDEOSTREAMINFOLIST_H_

#include <QVariantList>

#include "serviceexp.h"
#include "datacontracthelper.h"

#include "videoStreamInfo.h"

namespace DTC
{

class SERVICE_PUBLIC VideoStreamInfoList : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "version", "1.00" );

    // Q_CLASSINFO Used to augment Metadata for properties.
    // See datacontracthelper.h for details

    Q_CLASSINFO( "VideoStreamInfos", "type=DTC::VideoStreamInfo");
    Q_CLASSINFO( "AsOf"            , "transient=true" );

    Q_PROPERTY( int          Count          READ Count           WRITE setCount          )
    Q_PROPERTY( QDateTime    AsOf           READ AsOf            WRITE setAsOf           )
    Q_PROPERTY( QString      Version        READ Version         WRITE setVersion        )
    Q_PROPERTY( QString      ProtoVer       READ ProtoVer        WRITE setProtoVer       )
    Q_PROPERTY( int          ErrorCode      READ ErrorCode       WRITE setErrorCode      )
    Q_PROPERTY( QString      ErrorMsg       READ ErrorMsg        WRITE setErrorMsg       )

    Q_PROPERTY( QVariantList VideoStreamInfos READ VideoStreamInfos )

    PROPERTYIMP       ( int         , Count           )
    PROPERTYIMP_REF   ( QDateTime   , AsOf            )
    PROPERTYIMP_REF   ( QString     , Version         )
    PROPERTYIMP_REF   ( QString     , ProtoVer        )
    PROPERTYIMP       ( int         , ErrorCode       )
    PROPERTYIMP_REF   ( QString     , ErrorMsg        )

    PROPERTYIMP_RO_REF( QVariantList, VideoStreamInfos );

    public:

        static inline void InitializeCustomTypes();

        Q_INVOKABLE VideoStreamInfoList(QObject *parent = nullptr)
            : QObject( parent ),
              m_Count         ( 0      )
        {
        }

        void Copy( const VideoStreamInfoList *src )
        {
            m_Count         = src->m_Count          ;
            m_AsOf          = src->m_AsOf           ;
            m_Version       = src->m_Version        ;
            m_ProtoVer      = src->m_ProtoVer       ;
            m_ErrorCode     = src->m_ErrorCode      ;
            m_ErrorMsg      = src->m_ErrorMsg       ;

            CopyListContents< VideoStreamInfo >( this, m_VideoStreamInfos, src->m_VideoStreamInfos );
        }

        VideoStreamInfo *AddNewVideoStreamInfo()
        {
            // We must make sure the object added to the QVariantList has
            // a parent of 'this'

            auto *pObject = new VideoStreamInfo( this );
            m_VideoStreamInfos.append( QVariant::fromValue<QObject *>( pObject ));

            return pObject;
        }

    private:
        Q_DISABLE_COPY(VideoStreamInfoList);
};

inline void VideoStreamInfoList::InitializeCustomTypes()
{
    qRegisterMetaType< VideoStreamInfoList* >();

    VideoStreamInfo::InitializeCustomTypes();
}

} // namespace DTC

#endif
