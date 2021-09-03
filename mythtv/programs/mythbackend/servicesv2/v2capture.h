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

#ifndef V2CAPTURE_H
#define V2CAPTURE_H

#include "libmythbase/http/mythhttpservice.h"
#include "v2captureCardList.h"

#define CAPTURE_SERVICE QString("/Capture/")
#define CAPTURE_HANDLE  QString("Capture")

class V2Capture : public MythHTTPService
{
    Q_OBJECT
    Q_CLASSINFO("Version",      "1.0")
    Q_CLASSINFO("RemoveCaptureCard",  "methods=POST;name=bool")
    Q_CLASSINFO("AddCaptureCard",     "methods=POST;name=int")
    Q_CLASSINFO("UpdateCaptureCard",  "methods=POST;name=bool")
    Q_CLASSINFO("RemoveCardInput",    "methods=POST;name=bool")
    Q_CLASSINFO("AddCardInput",       "methods=POST;name=int")
    Q_CLASSINFO("UpdateCardInput",    "methods=POST;name=bool")

  public:
    V2Capture();
   ~V2Capture()  = default;
    static void RegisterCustomTypes();

  public slots:
    V2CaptureCardList*          GetCaptureCardList ( const QString    &HostName,
                                                        const QString    &CardType  );

    V2CaptureCard*              GetCaptureCard     ( int              CardId     );

    bool                        RemoveCaptureCard  ( int              CardId     );

    int                         AddCaptureCard     ( const QString    &VideoDevice,
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
                                                        bool             DVBEITScan);

    bool                        UpdateCaptureCard  ( int              CardId,
                                                        const QString    &Setting,
                                                        const QString    &Value );

    // Card Inputs

    bool                        RemoveCardInput    ( int              CardInputId);

    int                         AddCardInput       ( uint       CardId,
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
                                                        uint       LiveTVOrder);

    bool                        UpdateCardInput    ( int              CardInputId,
                                                        const QString    &Setting,
                                                        const QString    &Value );
  private:
    Q_DISABLE_COPY(V2Capture)

};


#endif
