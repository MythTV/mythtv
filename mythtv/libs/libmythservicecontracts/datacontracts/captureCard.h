//////////////////////////////////////////////////////////////////////////////
// Program Name: captureCard.h
// Created     : Sep. 21, 2011
//
// Copyright (c) 2011 Robert McNamara <rmcnamara@mythtv.org>
//
// Licensed under the GPL v2 or later, see COPYING for details
//
//////////////////////////////////////////////////////////////////////////////

#ifndef CAPTURECARD_H_
#define CAPTURECARD_H_

#include <QString>

#include "serviceexp.h"
#include "datacontracthelper.h"

namespace DTC
{

/////////////////////////////////////////////////////////////////////////////

class SERVICE_PUBLIC CaptureCard : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "version"    , "1.1" );

    Q_PROPERTY( uint            CardId              READ CardId             WRITE setCardId             )
    Q_PROPERTY( QString         VideoDevice         READ VideoDevice        WRITE setVideoDevice        )
    Q_PROPERTY( QString         AudioDevice         READ AudioDevice        WRITE setAudioDevice        )
    Q_PROPERTY( QString         VBIDevice           READ VBIDevice          WRITE setVBIDevice          )
    Q_PROPERTY( QString         CardType            READ CardType           WRITE setCardType           )
    Q_PROPERTY( uint            AudioRateLimit      READ AudioRateLimit     WRITE setAudioRateLimit     )
    Q_PROPERTY( QString         HostName            READ HostName           WRITE setHostName           )
    Q_PROPERTY( uint            DVBSWFilter         READ DVBSWFilter        WRITE setDVBSWFilter        )
    Q_PROPERTY( uint            DVBSatType          READ DVBSatType         WRITE setDVBSatType         )
    Q_PROPERTY( bool            DVBWaitForSeqStart  READ DVBWaitForSeqStart WRITE setDVBWaitForSeqStart )
    Q_PROPERTY( bool            SkipBTAudio         READ SkipBTAudio        WRITE setSkipBTAudio        )
    Q_PROPERTY( bool            DVBOnDemand         READ DVBOnDemand        WRITE setDVBOnDemand        )
    Q_PROPERTY( uint            DVBDiSEqCType       READ DVBDiSEqCType      WRITE setDVBDiSEqCType      )
    Q_PROPERTY( uint            FirewireSpeed       READ FirewireSpeed      WRITE setFirewireSpeed      )
    Q_PROPERTY( QString         FirewireModel       READ FirewireModel      WRITE setFirewireModel      )
    Q_PROPERTY( uint            FirewireConnection  READ FirewireConnection WRITE setFirewireConnection )
    Q_PROPERTY( uint            SignalTimeout       READ SignalTimeout      WRITE setSignalTimeout      )
    Q_PROPERTY( uint            ChannelTimeout      READ ChannelTimeout     WRITE setChannelTimeout     )
    Q_PROPERTY( uint            DVBTuningDelay      READ DVBTuningDelay     WRITE setDVBTuningDelay     )
    Q_PROPERTY( uint            Contrast            READ Contrast           WRITE setContrast           )
    Q_PROPERTY( uint            Brightness          READ Brightness         WRITE setBrightness         )
    Q_PROPERTY( uint            Colour              READ Colour             WRITE setColour             )
    Q_PROPERTY( uint            Hue                 READ Hue                WRITE setHue                )
    Q_PROPERTY( uint            DiSEqCId            READ DiSEqCId           WRITE setDiSEqCId           )
    Q_PROPERTY( bool            DVBEITScan          READ DVBEITScan         WRITE setDVBEITScan         )

    PROPERTYIMP( uint       ,     CardId            )
    PROPERTYIMP( QString    ,     VideoDevice       )
    PROPERTYIMP( QString    ,     AudioDevice       )
    PROPERTYIMP( QString    ,     VBIDevice         )
    PROPERTYIMP( QString    ,     CardType          )
    PROPERTYIMP( uint       ,     AudioRateLimit    )
    PROPERTYIMP( QString    ,     HostName          )
    PROPERTYIMP( uint       ,     DVBSWFilter       )
    PROPERTYIMP( uint       ,     DVBSatType        )
    PROPERTYIMP( bool       ,     DVBWaitForSeqStart)
    PROPERTYIMP( bool       ,     SkipBTAudio       )
    PROPERTYIMP( bool       ,     DVBOnDemand       )
    PROPERTYIMP( uint       ,     DVBDiSEqCType     )
    PROPERTYIMP( uint       ,     FirewireSpeed     )
    PROPERTYIMP( QString    ,     FirewireModel     )
    PROPERTYIMP( uint       ,     FirewireConnection)
    PROPERTYIMP( uint       ,     SignalTimeout     )
    PROPERTYIMP( uint       ,     ChannelTimeout    )
    PROPERTYIMP( uint       ,     DVBTuningDelay    )
    PROPERTYIMP( uint       ,     Contrast          )
    PROPERTYIMP( uint       ,     Brightness        )
    PROPERTYIMP( uint       ,     Colour            )
    PROPERTYIMP( uint       ,     Hue               )
    PROPERTYIMP( uint       ,     DiSEqCId          )
    PROPERTYIMP( bool       ,     DVBEITScan        )

    public:

        static inline void InitializeCustomTypes();

    public:

        CaptureCard(QObject *parent = 0)
            : QObject         ( parent ), m_CardId(0),
            m_AudioRateLimit(0), m_DVBSWFilter(0),
            m_DVBSatType(0), m_DVBWaitForSeqStart(false),
            m_SkipBTAudio(false), m_DVBOnDemand(false),
            m_DVBDiSEqCType(0), m_FirewireSpeed(0),
            m_FirewireConnection(0), m_SignalTimeout(1000),
            m_ChannelTimeout(3000), m_DVBTuningDelay(0),
            m_Contrast(0), m_Brightness(0), m_Colour(0),
            m_Hue(0), m_DiSEqCId(0), m_DVBEITScan(1)
        {
        }

        CaptureCard( const CaptureCard &src )
        {
            Copy( src );
        }

        void Copy( const CaptureCard &src )
        {
            m_CardId             = src.m_CardId;
            m_VideoDevice        = src.m_VideoDevice;
            m_AudioDevice        = src.m_AudioDevice;
            m_CardType           = src.m_CardType;
            m_AudioRateLimit     = src.m_AudioRateLimit;
            m_HostName           = src.m_HostName;
            m_DVBSWFilter        = src.m_DVBSWFilter;
            m_DVBSatType         = src.m_DVBSatType;
            m_DVBWaitForSeqStart = src.m_DVBWaitForSeqStart;
            m_SkipBTAudio        = src.m_SkipBTAudio;
            m_DVBOnDemand        = src.m_DVBOnDemand;
            m_DVBDiSEqCType      = src.m_DVBDiSEqCType;
            m_FirewireSpeed      = src.m_FirewireSpeed;
            m_FirewireModel      = src.m_FirewireModel;
            m_FirewireConnection = src.m_FirewireConnection;
            m_SignalTimeout      = src.m_SignalTimeout;
            m_ChannelTimeout     = src.m_ChannelTimeout;
            m_DVBTuningDelay     = src.m_DVBTuningDelay;
            m_Contrast           = src.m_Contrast;
            m_Brightness         = src.m_Brightness;
            m_Colour             = src.m_Colour;
            m_Hue                = src.m_Hue;
            m_DiSEqCId           = src.m_DiSEqCId;
            m_DVBEITScan         = src.m_DVBEITScan;
        }
};

} // namespace DTC

Q_DECLARE_METATYPE( DTC::CaptureCard  )
Q_DECLARE_METATYPE( DTC::CaptureCard* )

namespace DTC
{
inline void CaptureCard::InitializeCustomTypes()
{
    qRegisterMetaType< CaptureCard  >();
    qRegisterMetaType< CaptureCard* >();
}
}

#endif
