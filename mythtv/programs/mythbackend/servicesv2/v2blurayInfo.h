//////////////////////////////////////////////////////////////////////////////
// Program Name: blurayInfo.h
// Created     : Apr. 22, 2011
//
// Copyright (c) 2011 Robert McNamara <rmcnamara@mythtv.org>
//
// Licensed under the GPL v2 or later, see COPYING for details
//
//////////////////////////////////////////////////////////////////////////////

#ifndef V2BLURAYINFO_H_
#define V2BLURAYINFO_H_

#include <QString>

#include "libmythbase/http/mythhttpservice.h"

/////////////////////////////////////////////////////////////////////////////

class V2BlurayInfo : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "Version"    , "1.0" );

    SERVICE_PROPERTY2( QString    , Path                 )
    SERVICE_PROPERTY2( QString    , Title                )
    SERVICE_PROPERTY2( QString    , AltTitle             )
    SERVICE_PROPERTY2( QString    , DiscLang             )
    SERVICE_PROPERTY2( uint       , DiscNum              )
    SERVICE_PROPERTY2( uint       , TotalDiscNum         )
    SERVICE_PROPERTY2( uint       , TitleCount           )
    SERVICE_PROPERTY2( uint       , ThumbCount           )
    SERVICE_PROPERTY2( QString    , ThumbPath            )
    SERVICE_PROPERTY2( bool       , TopMenuSupported     )
    SERVICE_PROPERTY2( bool       , FirstPlaySupported   )
    SERVICE_PROPERTY2( uint       , NumHDMVTitles        )
    SERVICE_PROPERTY2( uint       , NumBDJTitles         )
    SERVICE_PROPERTY2( uint       , NumUnsupportedTitles )
    SERVICE_PROPERTY2( bool       , AACSDetected         )
    SERVICE_PROPERTY2( bool       , LibAACSDetected      )
    SERVICE_PROPERTY2( bool       , AACSHandled          )
    SERVICE_PROPERTY2( bool       , BDPlusDetected       )
    SERVICE_PROPERTY2( bool       , LibBDPlusDetected    )
    SERVICE_PROPERTY2( bool       , BDPlusHandled        )

    public:

        Q_INVOKABLE V2BlurayInfo(QObject *parent = nullptr)
            : QObject( parent )
        {
        }

        void Copy( const V2BlurayInfo *src )
        {
            m_Path                 = src->m_Path                 ;
            m_Title                = src->m_Title                ;
            m_AltTitle             = src->m_AltTitle             ;
            m_DiscLang             = src->m_DiscLang             ;
            m_DiscNum              = src->m_DiscNum              ;
            m_TotalDiscNum         = src->m_TotalDiscNum         ;
            m_TitleCount           = src->m_TitleCount           ;
            m_ThumbCount           = src->m_ThumbCount           ;
            m_ThumbPath            = src->m_ThumbPath            ;
            m_TopMenuSupported     = src->m_TopMenuSupported     ;
            m_FirstPlaySupported   = src->m_FirstPlaySupported   ;
            m_NumHDMVTitles        = src->m_NumHDMVTitles        ;
            m_NumBDJTitles         = src->m_NumBDJTitles         ;
            m_NumUnsupportedTitles = src->m_NumUnsupportedTitles ;
            m_AACSDetected         = src->m_AACSDetected         ;
            m_LibAACSDetected      = src->m_LibAACSDetected      ;
            m_AACSHandled          = src->m_AACSHandled          ;
            m_BDPlusDetected       = src->m_BDPlusDetected       ;
            m_LibBDPlusDetected    = src->m_LibBDPlusDetected    ;
            m_BDPlusHandled        = src->m_BDPlusHandled        ;
        }

    private:
        Q_DISABLE_COPY(V2BlurayInfo);
};

 Q_DECLARE_METATYPE(V2BlurayInfo*)

#endif
