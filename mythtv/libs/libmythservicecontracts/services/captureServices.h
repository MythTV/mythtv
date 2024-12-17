//////////////////////////////////////////////////////////////////////////////
// Program Name: captureServices.h
// Created     : Sep. 21, 2011
//
// Purpose - Capture Card Services API Interface definition
//
// Copyright (c) 2011 Robert McNamara <rmcnamara@mythtv.org>
//
// Licensed under the GPL v2 or later, see LICENSE for details
//
//////////////////////////////////////////////////////////////////////////////

#ifndef CAPTURESERVICES_H_
#define CAPTURESERVICES_H_

#include <QFileInfo>
#include <QStringList>

#include "libmythservicecontracts/service.h"

#include "libmythservicecontracts/datacontracts/captureCard.h"
#include "libmythservicecontracts/datacontracts/captureCardList.h"

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
//
// Notes -
//
//  * This implementation can't handle declared default parameters
//
//  * When called, any missing params are sent default values for its datatype
//
//  * Q_CLASSINFO( "<methodName>_Method", ...) is used to determine HTTP method
//    type.  Defaults to "BOTH", available values:
//          "GET", "POST" or "BOTH"
//
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

class SERVICE_PUBLIC CaptureServices : public Service
{
    Q_OBJECT
    Q_CLASSINFO( "version"    , "1.4" );
    Q_CLASSINFO( "RemoveCaptureCard_Method",                 "POST" )
    Q_CLASSINFO( "AddCaptureCard_Method",                    "POST" )
    Q_CLASSINFO( "UpdateCaptureCard_Method",                 "POST" )
    Q_CLASSINFO( "RemoveCardInput_Method",                   "POST" )
    Q_CLASSINFO( "AddCardInput_Method",                      "POST" )
    Q_CLASSINFO( "UpdateCardInput_Method",                   "POST" )

    public:

        // Must call InitializeCustomTypes for each unique Custom Type used
        // in public slots below.

        CaptureServices( QObject *parent = nullptr ) : Service( parent )
        {
            DTC::CaptureCard::InitializeCustomTypes();
            DTC::CaptureCardList::InitializeCustomTypes();
        }

    public slots:

        virtual DTC::CaptureCardList*       GetCaptureCardList ( const QString    &HostName,
                                                                 const QString    &CardType  ) = 0;

        virtual DTC::CaptureCard*           GetCaptureCard     ( int              CardId     ) = 0;

        virtual bool                        RemoveCaptureCard  ( int              CardId     ) = 0;

        virtual int                         AddCaptureCard     ( const QString    &VideoDevice,
                                                                 const QString    &AudioDevice,
                                                                 const QString    &VBIDevice,
                                                                 const QString    &CardType,
                                                                 uint             AudioRateLimit,
                                                                 const QString    &HostName,
                                                                 uint             DVBSWFilter,
                                                                 uint             DVBSatType,
                                                                 bool             DVBWaitForSeqStart,
                                                                 bool             SkipBTAudio,
                                                                 bool             DVBOnDemand,
                                                                 uint             DVBDiSEqCType,
                                                                 uint             FirewireSpeed,
                                                                 const QString    &FirewireModel,
                                                                 uint             FirewireConnection,
                                                                 uint             SignalTimeout,
                                                                 uint             ChannelTimeout,
                                                                 uint             DVBTuningDelay,
                                                                 uint             Contrast,
                                                                 uint             Brightness,
                                                                 uint             Colour,
                                                                 uint             Hue,
                                                                 uint             DiSEqCId,
                                                                 bool             DVBEITScan) = 0;

        virtual bool                        UpdateCaptureCard  ( int              CardId,
                                                                 const QString    &Setting,
                                                                 const QString    &Value ) = 0;

        // Card Inputs

        virtual bool                        RemoveCardInput    ( int              CardInputId) = 0;

        virtual int                         AddCardInput       ( uint       CardId,
                                                                 uint       SourceId,
                                                                 const QString &InputName,
                                                                 const QString &ExternalCommand,
                                                                 const QString &ChangerDevice,
                                                                 const QString &ChangerModel,
                                                                 const QString &HostName,
                                                                 const QString &TuneChan,
                                                                 const QString &StartChan,
                                                                 const QString &DisplayName,
                                                                 bool          DishnetEIT,
                                                                 uint       RecPriority,
                                                                 uint       Quicktune,
                                                                 uint       SchedOrder,
                                                                 uint       LiveTVOrder) = 0;

        virtual bool                        UpdateCardInput    ( int              CardInputId,
                                                                 const QString    &Setting,
                                                                 const QString    &Value ) = 0;
};

#endif
