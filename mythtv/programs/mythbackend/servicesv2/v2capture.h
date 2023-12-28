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
#include "v2recordingProfile.h"

#define CAPTURE_SERVICE QString("/Capture/")
#define CAPTURE_HANDLE  QString("Capture")

class V2Capture : public MythHTTPService
{
    Q_OBJECT
    Q_CLASSINFO("Version",      "1.6")
    Q_CLASSINFO("RemoveAllCaptureCards", "methods=POST;name=bool")
    Q_CLASSINFO("RemoveCaptureCard",  "methods=POST;name=bool")
    Q_CLASSINFO("AddCaptureCard",     "methods=POST;name=int")
    Q_CLASSINFO("UpdateCaptureCard",  "methods=POST;name=bool")
    Q_CLASSINFO("RemoveCardInput",    "methods=POST;name=bool")
    Q_CLASSINFO("AddCardInput",       "methods=POST;name=int")
    Q_CLASSINFO("UpdateCardInput",    "methods=POST;name=bool")
    Q_CLASSINFO("AddUserInputGroup",  "methods=POST;name=int")
    Q_CLASSINFO("LinkInputGroup",     "methods=POST;name=bool")
    Q_CLASSINFO("UnlinkInputGroup",   "methods=POST;name=bool")
    Q_CLASSINFO("SetInputMaxRecordings", "methods=POST;name=bool")
    Q_CLASSINFO("AddDiseqcTree",      "methods=POST")
    Q_CLASSINFO("UpdateDiseqcTree",   "methods=POST")
    Q_CLASSINFO("RemoveDiseqcTree",   "methods=POST")
    Q_CLASSINFO("AddDiseqcConfig",    "methods=POST")
    Q_CLASSINFO("RemoveDiseqcConfig", "methods=POST")
    Q_CLASSINFO("AddRecProfile",      "methods=POST")
    Q_CLASSINFO("DeleteRecProfile",   "methods=POST")
    Q_CLASSINFO("UpdateRecProfile",   "methods=POST")
    Q_CLASSINFO("UpdateRecProfileParam", "methods=POST")

  public:
    V2Capture();
   ~V2Capture() override  = default;
    static void RegisterCustomTypes();

  public slots:
    static V2CaptureCardList*   GetCaptureCardList ( const QString    &HostName,
                                                        const QString    &CardType  );

    static V2CaptureCard*       GetCaptureCard     ( int              CardId     );

    static V2CardSubType*       GetCardSubType     ( int              CardId     );

    static bool                 RemoveAllCaptureCards ( void );

    static bool                 RemoveCaptureCard  ( int              CardId     );

    static int                  AddCaptureCard     ( const QString    &VideoDevice,
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

    static bool                 UpdateCaptureCard  ( int              CardId,
                                                        const QString    &Setting,
                                                        const QString    &Value );

    // Card Inputs

    static bool                 RemoveCardInput    ( int              CardInputId);

    static int                  AddCardInput       ( uint       CardId,
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

    static bool                 UpdateCardInput    ( int              CardInputId,
                                                        const QString    &Setting,
                                                        const QString    &Value );

    static V2InputGroupList*    GetUserInputGroupList ( void );

    static int                  AddUserInputGroup  ( const QString & Name );

    static bool                 LinkInputGroup     ( uint InputId,
                                                     uint InputGroupId );

    static bool                 UnlinkInputGroup   ( uint InputId,
                                                     uint InputGroupId );

    static bool                 SetInputMaxRecordings ( uint InputId,
                                                        uint Max );

    static V2CardTypeList*      GetCardTypeList     ( void );

    static V2CaptureDeviceList* GetCaptureDeviceList  ( const QString  &CardType );

    static V2DiseqcTreeList*    GetDiseqcTreeList  ( void );

    static int                  AddDiseqcTree     ( uint           ParentId,
                                                    uint           Ordinal,
                                                    const QString& Type,
                                                    const QString& SubType,
                                                    const QString& Description,
                                                    uint           SwitchPorts,
                                                    float          RotorHiSpeed,
                                                    float          RotorLoSpeed,
                                                    const QString& RotorPositions,
                                                    int            LnbLofSwitch,
                                                    int            LnbLofHi,
                                                    int            LnbLofLo,
                                                    int            CmdRepeat,
                                                    bool           LnbPolInv,
                                                    int            Address,
                                                    uint           ScrUserband,
                                                    uint           ScrFrequency,
                                                    int            ScrPin);

    static bool                 UpdateDiseqcTree  ( uint           DiSEqCId,
                                                    uint           ParentId,
                                                    uint           Ordinal,
                                                    const QString& Type,
                                                    const QString& SubType,
                                                    const QString& Description,
                                                    uint           SwitchPorts,
                                                    float          RotorHiSpeed,
                                                    float          RotorLoSpeed,
                                                    const QString& RotorPositions,
                                                    int            LnbLofSwitch,
                                                    int            LnbLofHi,
                                                    int            LnbLofLo,
                                                    int            CmdRepeat,
                                                    bool           LnbPolInv,
                                                    int            Address,
                                                    uint           ScrUserband,
                                                    uint           ScrFrequency,
                                                    int            ScrPin);

  static bool                 RemoveDiseqcTree  ( uint           DiSEqCId) ;

  static V2DiseqcConfigList*  GetDiseqcConfigList  ( void );

  static bool                 AddDiseqcConfig     ( uint           CardId,
                                                    uint           DiSEqCId,
                                                    const QString& Value);

  static bool                 RemoveDiseqcConfig  ( uint           CardId);

  static V2RecProfileGroupList* GetRecProfileGroupList ( uint    GroupId,
                                                         uint    ProfileId,
                                                         bool    OnlyInUse );

  static int                  AddRecProfile      ( uint GroupId,
                                                  const QString& ProfileName,
                                                  const QString& VideoCodec,
                                                  const QString& AudioCodec );

  static bool                 DeleteRecProfile   (uint ProfileId);

  static bool                 UpdateRecProfile      ( uint ProfileId,
                                                      const QString& VideoCodec,
                                                      const QString& AudioCodec );

  static bool                 UpdateRecProfileParam ( uint ProfileId,
                                                    const QString& Name,
                                                    const QString& Value);

  private:
    Q_DISABLE_COPY(V2Capture)

};


#endif
