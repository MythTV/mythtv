//////////////////////////////////////////////////////////////////////////////
// Program Name: videoStreamInfoList.h
// Created     : May. 30, 2020
//
// Copyright (c) 2011 Peter Bennett <pbennett@mythtv.org>
//
// Licensed under the GPL v2 or later, see COPYING for details
//
//////////////////////////////////////////////////////////////////////////////

#ifndef V2VIDEOSTREAMINFOLIST_H_
#define V2VIDEOSTREAMINFOLIST_H_

#include <QVariantList>

#include "libmythbase/http/mythhttpservice.h"

#include "v2videoStreamInfo.h"

class V2VideoStreamInfoList : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "Version", "1.00" );

    // Q_CLASSINFO Used to augment Metadata for properties.

    Q_CLASSINFO( "VideoStreamInfos", "type=V2VideoStreamInfo");
    Q_CLASSINFO( "AsOf"            , "transient=true" );

    SERVICE_PROPERTY2( int         , Count           )
    SERVICE_PROPERTY2( QDateTime   , AsOf            )
    SERVICE_PROPERTY2( QString     , Version         )
    SERVICE_PROPERTY2( QString     , ProtoVer        )
    SERVICE_PROPERTY2( int         , ErrorCode       )
    SERVICE_PROPERTY2( QString     , ErrorMsg        )

    SERVICE_PROPERTY2( QVariantList, VideoStreamInfos );

    public:

        Q_INVOKABLE V2VideoStreamInfoList(QObject *parent = nullptr)
            : QObject( parent )
        {
        }

        void Copy( const V2VideoStreamInfoList *src )
        {
            m_Count         = src->m_Count          ;
            m_AsOf          = src->m_AsOf           ;
            m_Version       = src->m_Version        ;
            m_ProtoVer      = src->m_ProtoVer       ;
            m_ErrorCode     = src->m_ErrorCode      ;
            m_ErrorMsg      = src->m_ErrorMsg       ;

            CopyListContents< V2VideoStreamInfo >( this, m_VideoStreamInfos, src->m_VideoStreamInfos );
        }

        V2VideoStreamInfo *AddNewVideoStreamInfo()
        {
            // We must make sure the object added to the QVariantList has
            // a parent of 'this'

            auto *pObject = new V2VideoStreamInfo( this );
            m_VideoStreamInfos.append( QVariant::fromValue<QObject *>( pObject ));

            return pObject;
        }

    private:
        Q_DISABLE_COPY(V2VideoStreamInfoList);
};

Q_DECLARE_METATYPE(V2VideoStreamInfoList*)

#endif
