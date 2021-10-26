//////////////////////////////////////////////////////////////////////////////
// Program Name: captureCard.h
// Created     : Sep. 21, 2011
//
// Copyright (c) 2011 Robert McNamara <rmcnamara@mythtv.org>
//
// Licensed under the GPL v2 or later, see LICENSE for details
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
    Q_CLASSINFO( "version"    , "1.2" );

    Q_PROPERTY( uint            CardId              READ CardId             WRITE setCardId             )
    Q_PROPERTY( uint            ParentId            READ ParentId           WRITE setParentId           )
    Q_PROPERTY( QString         VideoDevice         READ VideoDevice        WRITE setVideoDevice        )
    Q_PROPERTY( QString         AudioDevice         READ AudioDevice        WRITE setAudioDevice        )
    Q_PROPERTY( QString         VBIDevice           READ VBIDevice          WRITE setVBIDevice          )
    Q_PROPERTY( QString         CardType            READ CardType           WRITE setCardType           )
    Q_PROPERTY( QString         DefaultInput        READ DefaultInput       WRITE setDefaultInput       )
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
    Q_PROPERTY( QString         InputName           READ InputName          WRITE setInputName          )
    Q_PROPERTY( uint            SourceId            READ SourceId           WRITE setSourceId           )
    Q_PROPERTY( QString         ExternalCommand     READ ExternalCommand    WRITE setExternalCommand    )
    Q_PROPERTY( QString         ChangerDevice       READ ChangerDevice      WRITE setChangerDevice      )
    Q_PROPERTY( QString         ChangerModel        READ ChangerModel       WRITE setChangerModel       )
    Q_PROPERTY( QString         TuneChannel         READ TuneChannel        WRITE setTuneChannel        )
    Q_PROPERTY( QString         StartChannel        READ StartChannel       WRITE setStartChannel       )
    Q_PROPERTY( QString         DisplayName         READ DisplayName        WRITE setDisplayName        )
    Q_PROPERTY( bool            DishnetEit          READ DishnetEit         WRITE setDishnetEit         )
    Q_PROPERTY( int             RecPriority         READ RecPriority        WRITE setRecPriority        )
    Q_PROPERTY( bool            QuickTune           READ QuickTune          WRITE setQuickTune          )
    Q_PROPERTY( uint            SchedOrder          READ SchedOrder         WRITE setSchedOrder         )
    Q_PROPERTY( uint            LiveTVOrder         READ LiveTVOrder        WRITE setLiveTVOrder        )
    Q_PROPERTY( uint            RecLimit            READ RecLimit           WRITE setRecLimit           )
    Q_PROPERTY( bool            SchedGroup          READ SchedGroup         WRITE setSchedGroup         )

    PROPERTYIMP    ( uint       ,     CardId            )
    PROPERTYIMP    ( uint       ,     ParentId          )
    PROPERTYIMP_REF( QString    ,     VideoDevice       )
    PROPERTYIMP_REF( QString    ,     AudioDevice       )
    PROPERTYIMP_REF( QString    ,     VBIDevice         )
    PROPERTYIMP_REF( QString    ,     CardType          )
    PROPERTYIMP_REF( QString    ,     DefaultInput      )
    PROPERTYIMP    ( uint       ,     AudioRateLimit    )
    PROPERTYIMP_REF( QString    ,     HostName          )
    PROPERTYIMP    ( uint       ,     DVBSWFilter       )
    PROPERTYIMP    ( uint       ,     DVBSatType        )
    PROPERTYIMP    ( bool       ,     DVBWaitForSeqStart)
    PROPERTYIMP    ( bool       ,     SkipBTAudio       )
    PROPERTYIMP    ( bool       ,     DVBOnDemand       )
    PROPERTYIMP    ( uint       ,     DVBDiSEqCType     )
    PROPERTYIMP    ( uint       ,     FirewireSpeed     )
    PROPERTYIMP_REF( QString    ,     FirewireModel     )
    PROPERTYIMP    ( uint       ,     FirewireConnection)
    PROPERTYIMP    ( uint       ,     SignalTimeout     )
    PROPERTYIMP    ( uint       ,     ChannelTimeout    )
    PROPERTYIMP    ( uint       ,     DVBTuningDelay    )
    PROPERTYIMP    ( uint       ,     Contrast          )
    PROPERTYIMP    ( uint       ,     Brightness        )
    PROPERTYIMP    ( uint       ,     Colour            )
    PROPERTYIMP    ( uint       ,     Hue               )
    PROPERTYIMP    ( uint       ,     DiSEqCId          )
    PROPERTYIMP    ( bool       ,     DVBEITScan        )
    PROPERTYIMP_REF( QString    ,     InputName         )
    PROPERTYIMP    ( uint       ,     SourceId          )
    PROPERTYIMP_REF( QString    ,     ExternalCommand   )
    PROPERTYIMP_REF( QString    ,     ChangerDevice     )
    PROPERTYIMP_REF( QString    ,     ChangerModel      )
    PROPERTYIMP_REF( QString    ,     TuneChannel       )
    PROPERTYIMP_REF( QString    ,     StartChannel      )
    PROPERTYIMP_REF( QString    ,     DisplayName       )
    PROPERTYIMP    ( bool       ,     DishnetEit        )
    PROPERTYIMP    ( int        ,     RecPriority       )
    PROPERTYIMP    ( bool       ,     QuickTune         )
    PROPERTYIMP    ( uint       ,     SchedOrder        )
    PROPERTYIMP    ( uint       ,     LiveTVOrder       )
    PROPERTYIMP    ( uint       ,     RecLimit          )
    PROPERTYIMP    ( bool       ,     SchedGroup        );

    public:

        static inline void InitializeCustomTypes();

        Q_INVOKABLE CaptureCard(QObject *parent = nullptr)
            : QObject         ( parent ), m_CardId(0), m_ParentId(0),
            m_AudioRateLimit(0), m_DVBSWFilter(0),
            m_DVBSatType(0), m_DVBWaitForSeqStart(false),
            m_SkipBTAudio(false), m_DVBOnDemand(false),
            m_DVBDiSEqCType(0), m_FirewireSpeed(0),
            m_FirewireConnection(0), m_SignalTimeout(1000),
            m_ChannelTimeout(3000), m_DVBTuningDelay(0),
            m_Contrast(0), m_Brightness(0), m_Colour(0),
            m_Hue(0), m_DiSEqCId(0), m_DVBEITScan(true),
            m_SourceId(0), m_DishnetEit(false), m_RecPriority(0),
            m_QuickTune(false), m_SchedOrder(0), m_LiveTVOrder(0),
            m_RecLimit(0), m_SchedGroup(false)
        {
        }

        void Copy( const CaptureCard *src )
        {
            m_CardId             = src->m_CardId;
            m_ParentId           = src->m_ParentId;
            m_VideoDevice        = src->m_VideoDevice;
            m_AudioDevice        = src->m_AudioDevice;
            m_CardType           = src->m_CardType;
            m_DefaultInput       = src->m_DefaultInput;
            m_AudioRateLimit     = src->m_AudioRateLimit;
            m_HostName           = src->m_HostName;
            m_DVBSWFilter        = src->m_DVBSWFilter;
            m_DVBSatType         = src->m_DVBSatType;
            m_DVBWaitForSeqStart = src->m_DVBWaitForSeqStart;
            m_SkipBTAudio        = src->m_SkipBTAudio;
            m_DVBOnDemand        = src->m_DVBOnDemand;
            m_DVBDiSEqCType      = src->m_DVBDiSEqCType;
            m_FirewireSpeed      = src->m_FirewireSpeed;
            m_FirewireModel      = src->m_FirewireModel;
            m_FirewireConnection = src->m_FirewireConnection;
            m_SignalTimeout      = src->m_SignalTimeout;
            m_ChannelTimeout     = src->m_ChannelTimeout;
            m_DVBTuningDelay     = src->m_DVBTuningDelay;
            m_Contrast           = src->m_Contrast;
            m_Brightness         = src->m_Brightness;
            m_Colour             = src->m_Colour;
            m_Hue                = src->m_Hue;
            m_DiSEqCId           = src->m_DiSEqCId;
            m_DVBEITScan         = src->m_DVBEITScan;
            m_InputName          = src->m_InputName;
            m_SourceId           = src->m_SourceId;
            m_ExternalCommand    = src->m_ExternalCommand;
            m_ChangerDevice      = src->m_ChangerDevice;
            m_ChangerModel       = src->m_ChangerModel;
            m_TuneChannel        = src->m_TuneChannel;
            m_StartChannel       = src->m_StartChannel;
            m_DisplayName        = src->m_DisplayName;
            m_DishnetEit         = src->m_DishnetEit;
            m_RecPriority        = src->m_RecPriority;
            m_QuickTune          = src->m_QuickTune;
            m_SchedOrder         = src->m_SchedOrder;
            m_LiveTVOrder        = src->m_LiveTVOrder;
            m_RecLimit           = src->m_RecLimit;
            m_SchedGroup         = src->m_SchedGroup;
        }

    private:
        Q_DISABLE_COPY(CaptureCard);
};

inline void CaptureCard::InitializeCustomTypes()
{
    qRegisterMetaType< CaptureCard* >();
}

} // namespace DTC

#endif
