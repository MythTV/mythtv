//////////////////////////////////////////////////////////////////////////////
// Program Name: videoStreamInfo.h
// Created     : May. 30, 2020
//
// Copyright (c) 2020 Peter Bennett <pbennett@mythtv.org>
//
// Licensed under the GPL v2 or later, see COPYING for details
//
//////////////////////////////////////////////////////////////////////////////

#ifndef V2VIDEOSTREAMINFO_H_
#define V2VIDEOSTREAMINFO_H_

#include <QString>
#include <QDateTime>

#include "libmythbase/http/mythhttpservice.h"

/////////////////////////////////////////////////////////////////////////////

class V2VideoStreamInfo : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "Version"    , "1.00" );

    SERVICE_PROPERTY2( QString    , CodecType      )
    SERVICE_PROPERTY2( QString    , CodecName      )
    SERVICE_PROPERTY2( int        , Width          )
    SERVICE_PROPERTY2( int        , Height         )
    SERVICE_PROPERTY2( float      , AspectRatio    )
    SERVICE_PROPERTY2( QString    , FieldOrder     )
    SERVICE_PROPERTY2( float      , FrameRate      )
    SERVICE_PROPERTY2( float      , AvgFrameRate   )
    SERVICE_PROPERTY2( int        , Channels       )
    SERVICE_PROPERTY2( qlonglong  , Duration       )

    public:

        Q_INVOKABLE V2VideoStreamInfo(QObject *parent = nullptr)
            : QObject( parent )
        {
        }

        void Copy( const V2VideoStreamInfo *src )
        {
            m_CodecType         = src->m_CodecType    ;
            m_CodecName         = src->m_CodecName    ;
            m_Width             = src->m_Width        ;
            m_Height            = src->m_Height       ;
            m_AspectRatio       = src->m_AspectRatio  ;
            m_FieldOrder        = src->m_FieldOrder   ;
            m_FrameRate         = src->m_FrameRate    ;
            m_AvgFrameRate      = src->m_AvgFrameRate ;
            m_Channels          = src->m_Channels     ;
            m_Duration          = src->m_Duration     ;
        }

    private:
        Q_DISABLE_COPY(V2VideoStreamInfo);
};

Q_DECLARE_METATYPE(V2VideoStreamInfo*)

#endif
