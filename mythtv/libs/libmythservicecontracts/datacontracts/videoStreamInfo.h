//////////////////////////////////////////////////////////////////////////////
// Program Name: videoStreamInfo.h
// Created     : May. 30, 2020
//
// Copyright (c) 2020 Peter Bennett <pbennett@mythtv.org>
//
// Licensed under the GPL v2 or later, see COPYING for details
//
//////////////////////////////////////////////////////////////////////////////

#ifndef VIDEOSTREAMINFO_H_
#define VIDEOSTREAMINFO_H_

#include <QString>
#include <QDateTime>

#include "serviceexp.h"
#include "datacontracthelper.h"

namespace DTC
{

/////////////////////////////////////////////////////////////////////////////

class SERVICE_PUBLIC VideoStreamInfo : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "version"    , "1.00" );

    Q_PROPERTY( QString         CodecType       READ CodecType        WRITE setCodecType      )
    Q_PROPERTY( QString         CodecName       READ CodecName        WRITE setCodecName      )
    Q_PROPERTY( int             Width           READ Width            WRITE setWidth          )
    Q_PROPERTY( int             Height          READ Height           WRITE setHeight         )
    Q_PROPERTY( float           AspectRatio     READ AspectRatio      WRITE setAspectRatio    )
    Q_PROPERTY( QString         FieldOrder      READ FieldOrder       WRITE setFieldOrder     )
    Q_PROPERTY( float           FrameRate       READ FrameRate        WRITE setFrameRate      )
    Q_PROPERTY( float           AvgFrameRate    READ AvgFrameRate     WRITE setAvgFrameRate   )
    Q_PROPERTY( int             Channels        READ Channels         WRITE setChannels       )
    Q_PROPERTY( qlonglong       Duration        READ Duration         WRITE setDuration       )

    PROPERTYIMP_REF( QString    , CodecType      )
    PROPERTYIMP_REF( QString    , CodecName      )
    PROPERTYIMP    ( int        , Width          )
    PROPERTYIMP    ( int        , Height         )
    PROPERTYIMP    ( float      , AspectRatio    )
    PROPERTYIMP_REF( QString    , FieldOrder     )
    PROPERTYIMP    ( float      , FrameRate      )
    PROPERTYIMP    ( float      , AvgFrameRate   )
    PROPERTYIMP    ( int        , Channels       )
    PROPERTYIMP    ( qlonglong  , Duration       )

    public:

        static inline void InitializeCustomTypes();

        Q_INVOKABLE VideoStreamInfo(QObject *parent = nullptr)
                        : QObject         ( parent ),
                          m_Width         ( 0      ),
                          m_Height        ( 0      ),
                          m_AspectRatio   ( 0      ),
                          m_FrameRate     ( 0      ),
                          m_AvgFrameRate  ( 0      ),
                          m_Channels      ( 0      ),
                          m_Duration      ( 0      )
        {
        }

        void Copy( const VideoStreamInfo *src )
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
        Q_DISABLE_COPY(VideoStreamInfo);
};

inline void VideoStreamInfo::InitializeCustomTypes()
{
    qRegisterMetaType< VideoStreamInfo* >();
}

} // namespace DTC

#endif
