//////////////////////////////////////////////////////////////////////////////
// Program Name: captureCard.h
// Created     : Sep. 21, 2011
//
// Copyright (c) 2011 Robert McNamara <rmcnamara@mythtv.org>
//
// Licensed under the GPL v2 or later, see COPYING for details
//
//////////////////////////////////////////////////////////////////////////////

#ifndef V2CAPTURECARD_H_
#define V2CAPTURECARD_H_

#include <QString>

#include "libmythbase/http/mythhttpservice.h"

/////////////////////////////////////////////////////////////////////////////

class V2CaptureCard : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "Version"    , "1.2" );


    SERVICE_PROPERTY2( uint       ,     CardId            )
    SERVICE_PROPERTY2( uint       ,     ParentId          )
    SERVICE_PROPERTY2( QString    ,     VideoDevice       )
    SERVICE_PROPERTY2( QString    ,     AudioDevice       )
    SERVICE_PROPERTY2( QString    ,     VBIDevice         )
    SERVICE_PROPERTY2( QString    ,     CardType          )
    SERVICE_PROPERTY2( QString    ,     DefaultInput      )
    SERVICE_PROPERTY2( uint       ,     AudioRateLimit    )
    SERVICE_PROPERTY2( QString    ,     HostName          )
    SERVICE_PROPERTY2( uint       ,     DVBSWFilter       )
    SERVICE_PROPERTY2( uint       ,     DVBSatType        )
    SERVICE_PROPERTY2( bool       ,     DVBWaitForSeqStart)
    SERVICE_PROPERTY2( bool       ,     SkipBTAudio       )
    SERVICE_PROPERTY2( bool       ,     DVBOnDemand       )
    SERVICE_PROPERTY2( uint       ,     DVBDiSEqCType     )
    SERVICE_PROPERTY2( uint       ,     FirewireSpeed     )
    SERVICE_PROPERTY2( QString    ,     FirewireModel     )
    SERVICE_PROPERTY2( uint       ,     FirewireConnection)
    SERVICE_PROPERTY2( uint       ,     SignalTimeout     )
    SERVICE_PROPERTY2( uint       ,     ChannelTimeout    )
    SERVICE_PROPERTY2( uint       ,     DVBTuningDelay    )
    SERVICE_PROPERTY2( uint       ,     Contrast          )
    SERVICE_PROPERTY2( uint       ,     Brightness        )
    SERVICE_PROPERTY2( uint       ,     Colour            )
    SERVICE_PROPERTY2( uint       ,     Hue               )
    SERVICE_PROPERTY2( uint       ,     DiSEqCId          )
    SERVICE_PROPERTY2( bool       ,     DVBEITScan        )
    SERVICE_PROPERTY2( QString    ,     InputName         )
    SERVICE_PROPERTY2( uint       ,     SourceId          )
    SERVICE_PROPERTY2( QString    ,     ExternalCommand   )
    SERVICE_PROPERTY2( QString    ,     ChangerDevice     )
    SERVICE_PROPERTY2( QString    ,     ChangerModel      )
    SERVICE_PROPERTY2( QString    ,     TuneChannel       )
    SERVICE_PROPERTY2( QString    ,     StartChannel      )
    SERVICE_PROPERTY2( QString    ,     DisplayName       )
    SERVICE_PROPERTY2( bool       ,     DishnetEit        )
    SERVICE_PROPERTY2( int        ,     RecPriority       )
    SERVICE_PROPERTY2( bool       ,     QuickTune         )
    SERVICE_PROPERTY2( uint       ,     SchedOrder        )
    SERVICE_PROPERTY2( uint       ,     LiveTVOrder       )
    SERVICE_PROPERTY2( uint       ,     RecLimit          )
    SERVICE_PROPERTY2( bool       ,     SchedGroup        );

    public:

        Q_INVOKABLE V2CaptureCard(QObject *parent = nullptr)
            : QObject          ( parent ),
              m_SignalTimeout  ( 1000   ),
              m_ChannelTimeout ( 3000   ),
              m_DVBEITScan     ( true   )
        {
        }

        void Copy( const V2CaptureCard *src )
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
        Q_DISABLE_COPY(V2CaptureCard);
};

Q_DECLARE_METATYPE(V2CaptureCard*)

#endif
