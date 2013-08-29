//////////////////////////////////////////////////////////////////////////////
// Program Name: capture.h
// Created     : Sep. 21, 2011
//
// Copyright (c) 2011 Robert McNamara <rmcnamara@mythtv.org>
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
//////////////////////////////////////////////////////////////////////////////

#ifndef CAPTURE_H
#define CAPTURE_H

#include <QScriptEngine>

#include "services/captureServices.h"

class Capture : public CaptureServices
{
    Q_OBJECT

    public:

        Q_INVOKABLE Capture( QObject *parent = 0 ) {}

    public:

        DTC::CaptureCardList*       GetCaptureCardList ( const QString    &HostName,
                                                         const QString    &CardType  );

        DTC::CaptureCard*           GetCaptureCard     ( int              CardId     );

        bool                        RemoveCaptureCard  ( int              CardId     );

        int                         AddCaptureCard     ( const QString    &VideoDevice,
                                                         const QString    &AudioDevice,
                                                         const QString    &VBIDevice,
                                                         const QString    &CardType,
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
                                                         bool             DVBEITScan);

        bool                        UpdateCaptureCard  ( int              CardId,
                                                         const QString    &Setting,
                                                         const QString    &Value );

        // Card Inputs

        bool                        RemoveCardInput    ( int              CardInputId);

        int                         AddCardInput       ( const uint CardId,
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
                                                         const uint LiveTVOrder);

        bool                        UpdateCardInput    ( int              CardInputId,
                                                         const QString    &Setting,
                                                         const QString    &Value );

};

// --------------------------------------------------------------------------
// The following class wrapper is due to a limitation in Qt Script Engine.  It
// requires all methods that return pointers to user classes that are derived from
// QObject actually return QObject* (not the user class *).  If the user class pointer
// is returned, the script engine treats it as a QVariant and doesn't create a
// javascript prototype wrapper for it.
//
// This class allows us to keep the rich return types in the main API class while
// offering the script engine a class it can work with.
//
// Only API Classes that return custom classes needs to implement these wrappers.
//
// We should continue to look for a cleaning solution to this problem.
// --------------------------------------------------------------------------

class ScriptableCapture : public QObject
{
    Q_OBJECT

    private:

        Capture    m_obj;

    public:

        Q_INVOKABLE ScriptableCapture( QObject *parent = 0 ) : QObject( parent ) {}

    public slots:

        bool RemoveCaptureCard  ( int              Id         )
        {
            return m_obj.RemoveCaptureCard( Id );
        }

        bool                        AddCaptureCard     ( const QString    &VideoDevice,
                                                         const QString    &AudioDevice,
                                                         const QString    &VBIDevice,
                                                         const QString    &CardType,
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
                                                         bool             DVBEITScan)
        {
            return m_obj.AddCaptureCard( VideoDevice, AudioDevice, VBIDevice, CardType,
                                AudioRateLimit, HostName, DVBSWFilter,
                                DVBSatType, DVBWaitForSeqStart, SkipBTAudio, DVBOnDemand,
                                DVBDiSEqCType, FirewireSpeed, FirewireModel, FirewireConnection,
                                SignalTimeout, ChannelTimeout, DVBTuningDelay, Contrast, Brightness,
                                Colour, Hue, DiSEqCId, DVBEITScan);
        }
};


Q_SCRIPT_DECLARE_QMETAOBJECT( ScriptableCapture, QObject*);

#endif
