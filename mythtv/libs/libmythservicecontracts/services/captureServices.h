//////////////////////////////////////////////////////////////////////////////
// Program Name: captureServices.h
// Created     : Sep. 21, 2011
//
// Purpose - Capture Card Services API Interface definition
//
// Copyright (c) 2011 Robert McNamara <rmcnamara@mythtv.org>
//
// This library is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; either
// version 2.1 of the License, or at your option any later version of the LGPL.
//
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public
// License along with this library.  If not, see <http://www.gnu.org/licenses/>.
//
//////////////////////////////////////////////////////////////////////////////

#ifndef CAPTURESERVICES_H_
#define CAPTURESERVICES_H_

#include <QFileInfo>
#include <QStringList>

#include "service.h"

#include "datacontracts/captureCard.h"
#include "datacontracts/captureCardList.h"

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
    Q_CLASSINFO( "version"    , "1.3" );
    Q_CLASSINFO( "RemoveCaptureCard_Method",                 "POST" )
    Q_CLASSINFO( "AddCaptureCard_Method",                    "POST" )
    Q_CLASSINFO( "UpdateCaptureCard_Method",                 "POST" )
    Q_CLASSINFO( "RemoveCardInput_Method",                   "POST" )
    Q_CLASSINFO( "AddCardInput_Method",                      "POST" )
    Q_CLASSINFO( "UpdateCardInput_Method",                   "POST" )

    public:

        // Must call InitializeCustomTypes for each unique Custom Type used
        // in public slots below.

        CaptureServices( QObject *parent = 0 ) : Service( parent )
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
                                                                 const QString    &DefaultInput,
                                                                 const uint       AudioRateLimit,
                                                                 const QString    &HostName,
                                                                 const uint       DVBSWFilter,
                                                                 const uint       DVBSatType,
                                                                 bool             DVBWaitForSeqStart,
                                                                 bool             SkipBTAudio,
                                                                 bool             DVBOnDemand,
                                                                 const uint       DVBDiSEqCType,
                                                                 const uint       FirewireSpeed,
                                                                 const QString    &FirewireModel,
                                                                 const uint       FirewireConnection,
                                                                 const uint       SignalTimeout,
                                                                 const uint       ChannelTimeout,
                                                                 const uint       DVBTuningDelay,
                                                                 const uint       Contrast,
                                                                 const uint       Brightness,
                                                                 const uint       Colour,
                                                                 const uint       Hue,
                                                                 const uint       DiSEqCId,
                                                                 bool             DVBEITScan) = 0;

        virtual bool                        UpdateCaptureCard  ( int              CardId,
                                                                 const QString    &Setting,
                                                                 const QString    &Value ) = 0;

        // Card Inputs

        virtual bool                        RemoveCardInput    ( int              CardInputId) = 0;

        virtual int                         AddCardInput       ( const uint CardId,
                                                                 const uint SourceId,
                                                                 const QString &InputName,
                                                                 const QString &ExternalCommand,
                                                                 const QString &ChangerDevice,
                                                                 const QString &ChangerModel,
                                                                 const QString &HostName,
                                                                 const QString &TuneChan,
                                                                 const QString &StartChan,
                                                                 const QString &DisplayName,
                                                                 bool          DishnetEIT,
                                                                 const uint RecPriority,
                                                                 const uint Quicktune,
                                                                 const uint SchedOrder,
                                                                 const uint LiveTVOrder) = 0;

        virtual bool                        UpdateCardInput    ( int              CardInputId,
                                                                 const QString    &Setting,
                                                                 const QString    &Value ) = 0;
};

#endif

