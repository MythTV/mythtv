//////////////////////////////////////////////////////////////////////////////
// Program Name: channel.h
// Created     : Apr. 8, 2011
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

#ifndef V2CHANNEL_H
#define V2CHANNEL_H

#include "libmythbase/http/mythhttpservice.h"
#include "v2channelInfoList.h"
#include "v2videoSourceList.h"
#include "v2videoMultiplexList.h"
#include "v2lineup.h"
#include "v2grabber.h"
#include "v2commMethod.h"
#include "v2channelScan.h"
#include "v2channelRestore.h"

#define CHANNEL_SERVICE QString("/Channel/")
#define CHANNEL_HANDLE  QString("Channel")


class V2Channel : public MythHTTPService
{
    Q_OBJECT
    Q_CLASSINFO("Version",      "1.12")
    Q_CLASSINFO("UpdateDBChannel",        "methods=POST;name=bool")
    Q_CLASSINFO("AddDBChannel",           "methods=POST;name=bool")
    Q_CLASSINFO("RemoveDBChannel",        "methods=POST;name=bool")
    Q_CLASSINFO("UpdateVideoSource",      "methods=POST;name=bool")
    Q_CLASSINFO("AddVideoSource",         "methods=POST;name=int")
    Q_CLASSINFO("RemoveAllVideoSources",  "methods=POST;name=bool")
    Q_CLASSINFO("RemoveVideoSource",      "methods=POST;name=bool")
    Q_CLASSINFO("FetchChannelsFromSource","methods=GET,POST;name=int")
    Q_CLASSINFO("GetAvailableChanid",     "methods=GET,POST;name=int")
    Q_CLASSINFO("GetXMLTVIdList",         "methods=GET,POST,HEAD;name=StringList")
    Q_CLASSINFO("StartScan",              "methods=POST;name=bool")
    Q_CLASSINFO("StopScan",               "methods=POST;name=bool")
    Q_CLASSINFO("SendScanDialogResponse", "methods=POST;name=bool")
    Q_CLASSINFO("SaveRestoreData",        "methods=POST;name=bool")
    Q_CLASSINFO("CopyIconToBackend",      "methods=POST")

    public:
        V2Channel();
        ~V2Channel() override = default;
        static void RegisterCustomTypes();

    public slots:

        /* Channel Methods */

        static V2ChannelInfoList* GetChannelInfoList ( uint      SourceID,
                                                     uint      ChannelGroupID,
                                                     uint      StartIndex,
                                                     uint      Count,
                                                     bool      OnlyVisible,
                                                     bool      Details,
                                                     bool      OrderByName,
                                                     bool      GroupByCallsign,
                                                     bool      OnlyTunable );

        static V2ChannelInfo*  GetChannelInfo      ( uint     ChanID     );

        bool                   UpdateDBChannel     ( uint          MplexID,
                                                     uint          SourceID,
                                                     uint          ChannelID,
                                                     const QString &CallSign,
                                                     const QString &ChannelName,
                                                     const QString &ChannelNumber,
                                                     uint          ServiceID,
                                                     uint          ATSCMajorChannel,
                                                     uint          ATSCMinorChannel,
                                                     bool          UseEIT,
                                                     bool          Visible,
                                                     const QString &ExtendedVisible,
                                                     const QString &FrequencyID,
                                                     const QString &Icon,
                                                     const QString &Format,
                                                     const QString &XMLTVID,
                                                     const QString &DefaultAuthority,
                                                     uint          ServiceType,
                                                     int           RecPriority,
                                                     int           TimeOffset,
                                                     int           CommMethod );

        bool                   AddDBChannel        ( uint          MplexID,
                                                     uint          SourceID,
                                                     uint          ChannelID,
                                                     const QString &CallSign,
                                                     const QString &ChannelName,
                                                     const QString &ChannelNumber,
                                                     uint          ServiceID,
                                                     uint          ATSCMajorChannel,
                                                     uint          ATSCMinorChannel,
                                                     bool          UseEIT,
                                                     bool          Visible,
                                                     const QString &ExtendedVisible,
                                                     const QString &FrequencyID,
                                                     const QString &Icon,
                                                     const QString &Format,
                                                     const QString &XMLTVID,
                                                     const QString &DefaultAuthority,
                                                     uint          ServiceType,
                                                     int           RecPriority,
                                                     int           TimeOffset,
                                                     int           CommMethod );

        static bool            RemoveDBChannel     ( uint          ChannelID );

        static uint           GetAvailableChanid   ( void );

        static V2CommMethodList* GetCommMethodList  ( void );

        /* Video Source Methods */

        static V2VideoSourceList* GetVideoSourceList     ( void );

        static V2VideoSource*     GetVideoSource         ( uint SourceID );

        bool                      UpdateVideoSource      ( uint          SourceID,
                                                           const QString &SourceName,
                                                           const QString &Grabber,
                                                           const QString &UserId,
                                                           const QString &FreqTable,
                                                           const QString &LineupId,
                                                           const QString &Password,
                                                           bool          UseEIT,
                                                           const QString &ConfigPath,
                                                           int           NITId,
                                                           uint          BouquetId,
                                                           uint          RegionId,
                                                           uint          ScanFrequency,
                                                           uint          LCNOffset );

        static int                AddVideoSource         ( const QString &SourceName,
                                                           const QString &Grabber,
                                                           const QString &UserId,
                                                           const QString &FreqTable,
                                                           const QString &LineupId,
                                                           const QString &Password,
                                                           bool          UseEIT,
                                                           const QString &ConfigPath,
                                                           int           NITId,
                                                           uint          BouquetId,
                                                           uint          RegionId,
                                                           uint          ScanFrequency,
                                                           uint          LCNOffset );

        static bool               RemoveAllVideoSources  ( void );
        static bool               RemoveVideoSource      ( uint SourceID );

        static V2LineupList*      GetDDLineupList        ( const QString &Source,
                                                           const QString &UserId,
                                                           const QString &Password );

        static int                FetchChannelsFromSource( uint       SourceId,
                                                           uint       CardId,
                                                           bool       WaitForFinish );

        /* Multiplex Methods */

        static V2VideoMultiplexList* GetVideoMultiplexList( uint SourceID,
                                                           uint StartIndex,
                                                           uint Count      );

        static V2VideoMultiplex*  GetVideoMultiplex      ( uint MplexID    );

        static QStringList        GetXMLTVIdList         ( uint SourceID );

        static V2GrabberList*     GetGrabberList  (  );

        static QStringList        GetFreqTableList         (  );

        static bool               StartScan             (   uint CardId,
                                                            const QString &DesiredServices,
                                                            bool FreeToAirOnly,
                                                            bool ChannelNumbersOnly,
                                                            bool CompleteChannelsOnly,
                                                            bool FullChannelSearch,
                                                            bool RemoveDuplicates,
                                                            bool AddFullTS,
                                                            bool TestDecryptable,
                                                            const QString &ScanType,
                                                            const QString &FreqTable,
                                                            const QString &Modulation,
                                                            const QString &FirstChan,
                                                            const QString &LastChan,
                                                            uint ScanId,
                                                            bool IgnoreSignalTimeout,
                                                            bool FollowNITSetting,
                                                            uint MplexId,
                                                            const QString &Frequency,
                                                            const QString &Bandwidth,
                                                            const QString &Polarity,
                                                            const QString &SymbolRate,
                                                            const QString &Inversion,
                                                            const QString &Constellation,
                                                            const QString &ModSys,
                                                            const QString &CodeRateLP,
                                                            const QString &CodeRateHP,
                                                            const QString &FEC,
                                                            const QString &TransmissionMode,
                                                            const QString &GuardInterval,
                                                            const QString &Hierarchy,
                                                            const QString &RollOff);

        static V2ScanStatus*      GetScanStatus            (  );

        static bool               StopScan                ( uint Cardid);

        static V2ScanList*        GetScanList             ( uint SourceId);

        static bool               SendScanDialogResponse  ( uint Cardid,
                                                            const QString &DialogString,
                                                            int DialogButton );

        static V2ChannelRestore*  GetRestoreData          ( uint SourceId,
                                                            bool XmltvId,
                                                            bool Icon,
                                                            bool Visible);

        static bool               SaveRestoreData         ( uint SourceId);

        static bool               CopyIconToBackend        (  const QString& Url,
                                                             const QString& ChanId);
    private:
        Q_DISABLE_COPY(V2Channel)

};


#endif
