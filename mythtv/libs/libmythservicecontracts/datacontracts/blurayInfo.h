//////////////////////////////////////////////////////////////////////////////
// Program Name: blurayInfo.h
// Created     : Apr. 22, 2011
//
// Copyright (c) 2011 Robert McNamara <rmcnamara@mythtv.org>
//
// Licensed under the GPL v2 or later, see COPYING for details
//
//////////////////////////////////////////////////////////////////////////////

#ifndef BLURAYINFO_H_
#define BLURAYINFO_H_

#include <QString>

#include "serviceexp.h"
#include "datacontracthelper.h"

namespace DTC
{

/////////////////////////////////////////////////////////////////////////////

class SERVICE_PUBLIC BlurayInfo : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "version"    , "1.0" );

    Q_PROPERTY( QString         Path                 READ Path                 WRITE setPath                 )
    Q_PROPERTY( QString         Title                READ Title                WRITE setTitle                )
    Q_PROPERTY( QString         AltTitle             READ AltTitle             WRITE setAltTitle             )
    Q_PROPERTY( QString         DiscLang             READ DiscLang             WRITE setDiscLang             )
    Q_PROPERTY( uint            DiscNum              READ DiscNum              WRITE setDiscNum              )
    Q_PROPERTY( uint            TotalDiscNum         READ TotalDiscNum         WRITE setTotalDiscNum         )
    Q_PROPERTY( uint            TitleCount           READ TitleCount           WRITE setTitleCount           )
    Q_PROPERTY( uint            ThumbCount           READ ThumbCount           WRITE setThumbCount           )
    Q_PROPERTY( QString         ThumbPath            READ ThumbPath            WRITE setThumbPath            )
    Q_PROPERTY( bool            TopMenuSupported     READ TopMenuSupported     WRITE setTopMenuSupported     )
    Q_PROPERTY( bool            FirstPlaySupported   READ FirstPlaySupported   WRITE setFirstPlaySupported   )
    Q_PROPERTY( uint            NumHDMVTitles        READ NumHDMVTitles        WRITE setNumHDMVTitles        )
    Q_PROPERTY( uint            NumBDJTitles         READ NumBDJTitles         WRITE setNumBDJTitles         )
    Q_PROPERTY( uint            NumUnsupportedTitles READ NumUnsupportedTitles WRITE setNumUnsupportedTitles )
    Q_PROPERTY( bool            AACSDetected         READ AACSDetected         WRITE setAACSDetected         )
    Q_PROPERTY( bool            LibAACSDetected      READ LibAACSDetected      WRITE setLibAACSDetected      )
    Q_PROPERTY( bool            AACSHandled          READ AACSHandled          WRITE setAACSHandled          )
    Q_PROPERTY( bool            BDPlusDetected       READ BDPlusDetected       WRITE setBDPlusDetected       )
    Q_PROPERTY( bool            LibBDPlusDetected    READ LibBDPlusDetected    WRITE setLibBDPlusDetected    )
    Q_PROPERTY( bool            BDPlusHandled        READ BDPlusHandled        WRITE setBDPlusHandled        )

    PROPERTYIMP    ( QString    , Path                 )
    PROPERTYIMP    ( QString    , Title                )
    PROPERTYIMP    ( QString    , AltTitle             )
    PROPERTYIMP    ( QString    , DiscLang             )
    PROPERTYIMP    ( uint       , DiscNum              )
    PROPERTYIMP    ( uint       , TotalDiscNum         )
    PROPERTYIMP    ( uint       , TitleCount           )
    PROPERTYIMP    ( uint       , ThumbCount           )
    PROPERTYIMP    ( QString    , ThumbPath            )
    PROPERTYIMP    ( bool       , TopMenuSupported     )
    PROPERTYIMP    ( bool       , FirstPlaySupported   )
    PROPERTYIMP    ( uint       , NumHDMVTitles        )
    PROPERTYIMP    ( uint       , NumBDJTitles         )
    PROPERTYIMP    ( uint       , NumUnsupportedTitles )
    PROPERTYIMP    ( bool       , AACSDetected         )
    PROPERTYIMP    ( bool       , LibAACSDetected      )
    PROPERTYIMP    ( bool       , AACSHandled          )
    PROPERTYIMP    ( bool       , BDPlusDetected       )
    PROPERTYIMP    ( bool       , LibBDPlusDetected    )
    PROPERTYIMP    ( bool       , BDPlusHandled        )

    public:

        static inline void InitializeCustomTypes();

    public:

        BlurayInfo(QObject *parent = 0)
                 : QObject         ( parent    ),
                   m_Path          ( QString() )
        {
        }

        BlurayInfo( const BlurayInfo &src )
        {
            Copy( src );
        }

        void Copy( const BlurayInfo &src )
        {
            m_Path          = src.m_Path            ;
            m_Title         = src.m_Title           ;
            m_AltTitle      = src.m_AltTitle        ;
            m_DiscLang      = src.m_DiscLang        ;
            m_DiscNum       = src.m_DiscNum         ;
            m_TotalDiscNum  = src.m_TotalDiscNum    ;
            m_TitleCount    = src.m_TitleCount      ;
            m_ThumbCount    = src.m_ThumbCount      ;
            m_ThumbPath     = src.m_ThumbPath       ;
        }
};

} // namespace DTC

Q_DECLARE_METATYPE( DTC::BlurayInfo  )
Q_DECLARE_METATYPE( DTC::BlurayInfo* )

namespace DTC
{
inline void BlurayInfo::InitializeCustomTypes()
{
    qRegisterMetaType< BlurayInfo  >();
    qRegisterMetaType< BlurayInfo* >();
}
}

#endif
