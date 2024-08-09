//////////////////////////////////////////////////////////////////////////////
// Program Name: channel.cpp
// Created     : Apr. 8, 2011
//
// Copyright (c) 2011 Robert McNamara <rmcnamara@mythtv.org>
// Copyright (c) 2013 MythTV Developers
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

// C++
#include <algorithm>
#include <cmath>

// Qt
#include <QList>
#include <QTemporaryFile>

// MythTV
#include "libmythbase/http/mythhttpmetaservice.h"
#include "libmythbase/compat.h"
#include "libmythbase/mythdbcon.h"
#include "libmythbase/mythdirs.h"
#include "libmythbase/mythversion.h"
#include "libmythbase/mythcorecontext.h"
#include "libmythbase/programtypes.h"
#include "libmythbase/mythdownloadmanager.h"
#include "libmythtv/channelutil.h"
#include "libmythtv/channelscan/scanwizardconfig.h"
#include "libmythtv/channelscan/channelscanner_web.h"
#include "libmythtv/channelscan/scaninfo.h"
#include "libmythtv/channelscan/channelimporter.h"
#include "libmythtv/sourceutil.h"
#include "libmythtv/cardutil.h"
#include "libmythbase/mythdate.h"
#include "libmythtv/frequencies.h"
#include "libmythbase/mythsystemlegacy.h"
#include "libmythtv/restoredata.h"
#include "libmythtv/scheduledrecording.h"

// MythBackend
#include "v2artworkInfoList.h"
#include "v2castMemberList.h"
#include "v2channel.h"
#include "v2programAndChannel.h"
#include "v2recording.h"
#include "v2serviceUtil.h"
#include "v2grabber.h"
#include "v2freqtable.h"

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

Q_GLOBAL_STATIC_WITH_ARGS(MythHTTPMetaService, s_service,
(CHANNEL_HANDLE, V2Channel::staticMetaObject, &V2Channel::RegisterCustomTypes))

void V2Channel::RegisterCustomTypes()
{
    qRegisterMetaType<V2ChannelInfoList*>("V2ChannelInfoList");
    qRegisterMetaType<V2ChannelInfo*>("V2ChannelInfo");
    qRegisterMetaType<V2VideoSourceList*>("V2VideoSourceList");
    qRegisterMetaType<V2VideoSource*>("V2VideoSource");
    qRegisterMetaType<V2LineupList*>("V2LineupList");
    qRegisterMetaType<V2Lineup*>("V2Lineup");
    qRegisterMetaType<V2VideoMultiplexList*>("V2VideoMultiplexList");
    qRegisterMetaType<V2VideoMultiplex*>("V2VideoMultiplex");
    qRegisterMetaType<V2Program*>("V2Program");
    qRegisterMetaType<V2RecordingInfo*>("V2RecordingInfo");
    qRegisterMetaType<V2ArtworkInfoList*>("V2ArtworkInfoList");
    qRegisterMetaType<V2ArtworkInfo*>("V2ArtworkInfo");
    qRegisterMetaType<V2CastMemberList*>("V2CastMemberList");
    qRegisterMetaType<V2CastMember*>("V2CastMember");
    qRegisterMetaType<V2Grabber*>("V2Grabber");
    qRegisterMetaType<V2GrabberList*>("V2GrabberList");
    qRegisterMetaType<V2FreqTableList*>("V2FreqTableList");
    qRegisterMetaType<V2CommMethodList*>("V2CommMethodList");
    qRegisterMetaType<V2CommMethod*>("V2CommMethod");
    qRegisterMetaType<V2ScanStatus*>("V2ScanStatus");
    qRegisterMetaType<V2Scan*>("V2Scan");
    qRegisterMetaType<V2ScanList*>("V2ScanList");
    qRegisterMetaType<V2ChannelRestore*>("V2ChannelRestore");
}

V2Channel::V2Channel() : MythHTTPService(s_service)
{
}



V2ChannelInfoList* V2Channel::GetChannelInfoList( uint nSourceID,
                                                   uint nChannelGroupID,
                                                   uint nStartIndex,
                                                   uint nCount,
                                                   bool bOnlyVisible,
                                                   bool bDetails,
                                                   bool bOrderByName,
                                                   bool bGroupByCallsign,
                                                   bool bOnlyTunable )
{
    ChannelInfoList chanList;

    uint nTotalAvailable = 0;

    chanList = ChannelUtil::LoadChannels( 0, 0, nTotalAvailable, bOnlyVisible,
                                          bOrderByName ? ChannelUtil::kChanOrderByName : ChannelUtil::kChanOrderByChanNum,
                                          bGroupByCallsign ? ChannelUtil::kChanGroupByCallsign : ChannelUtil::kChanGroupByChanid,
                                          nSourceID, nChannelGroupID, false, "",
                                          "", bOnlyTunable);

    // ----------------------------------------------------------------------
    // Build Response
    // ----------------------------------------------------------------------

    auto *pChannelInfos = new V2ChannelInfoList();

    nStartIndex = (nStartIndex > 0) ? std::min( nStartIndex, nTotalAvailable ) : 0;
    nCount      = (nCount > 0) ? std::min(nCount, (nTotalAvailable - nStartIndex)) :
                                             (nTotalAvailable - nStartIndex);

    ChannelInfoList::iterator chanIt;
    auto chanItBegin = chanList.begin() + nStartIndex;
    auto chanItEnd   = chanItBegin      + nCount;

    for( chanIt = chanItBegin; chanIt < chanItEnd; ++chanIt )
    {
        V2ChannelInfo *pChannelInfo = pChannelInfos->AddNewChannelInfo();

        const ChannelInfo& channelInfo = (*chanIt);

        if (!V2FillChannelInfo(pChannelInfo, channelInfo, bDetails))
        {
            delete pChannelInfo;
            delete pChannelInfos;
            throw( QString("V2Channel ID appears invalid."));
        }
    }

    int nCurPage = 0;
    int nTotalPages = 0;
    if (nCount == 0)
        nTotalPages = 1;
    else
        nTotalPages = (int)std::ceil((float)nTotalAvailable / nCount);

    if (nTotalPages == 1)
        nCurPage = 1;
    else
    {
        nCurPage = (int)std::ceil((float)nStartIndex / nCount) + 1;
    }

    pChannelInfos->setStartIndex    ( nStartIndex     );
    pChannelInfos->setCount         ( nCount          );
    pChannelInfos->setCurrentPage   ( nCurPage        );
    pChannelInfos->setTotalPages    ( nTotalPages     );
    pChannelInfos->setTotalAvailable( nTotalAvailable );
    pChannelInfos->setAsOf          ( MythDate::current() );
    pChannelInfos->setVersion       ( MYTH_BINARY_VERSION );
    pChannelInfos->setProtoVer      ( MYTH_PROTO_VERSION  );

    return pChannelInfos;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

V2ChannelInfo* V2Channel::GetChannelInfo( uint nChanID )
{
    if (nChanID == 0)
        throw( QString("V2Channel ID appears invalid."));

    auto *pChannelInfo = new V2ChannelInfo();

    if (!V2FillChannelInfo(pChannelInfo, nChanID, true))
    {
        // throw causes a crash on linux and we can't know in advance
        // that a channel id from an old recording rule is invalid
        //throw( QString("V2Channel ID appears invalid."));
    }

    return pChannelInfo;
}

bool V2Channel::UpdateDBChannel( uint          MplexID,
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
                               int           CommMethod )
{
    if (!HAS_PARAMv2("ChannelID"))
        throw QString("ChannelId is required");

    if (m_request->m_queries.size() < 2 )
        throw QString("Nothing to update");

    ChannelInfo channel;
    if (!channel.Load(ChannelID))
        throw QString("ChannelId %1 doesn't exist");

    if (HAS_PARAMv2("MplexID"))
        channel.m_mplexId = MplexID;
    if (HAS_PARAMv2("SourceID"))
        channel.m_sourceId = SourceID;
    if (HAS_PARAMv2("CallSign"))
        channel.m_callSign = CallSign;
    if (HAS_PARAMv2("ChannelName"))
        channel.m_name = ChannelName;
    if (HAS_PARAMv2("ChannelNumber"))
        channel.m_chanNum = ChannelNumber;
    if (HAS_PARAMv2("ServiceID"))
        channel.m_serviceId = ServiceID;
    if (HAS_PARAMv2("ATSCMajorChannel"))
        channel.m_atscMajorChan = ATSCMajorChannel;
    if (HAS_PARAMv2("ATSCMinorChannel"))
        channel.m_atscMinorChan = ATSCMinorChannel;
    if (HAS_PARAMv2("UseEIT"))
        channel.m_useOnAirGuide = UseEIT;
    if (HAS_PARAMv2("ExtendedVisible"))
        channel.m_visible = channelVisibleTypeFromString(ExtendedVisible);
    else if (HAS_PARAMv2("Visible"))
    {
        if (channel.m_visible == kChannelVisible ||
            channel.m_visible == kChannelNotVisible)
        {
            channel.m_visible =
                (Visible ? kChannelVisible : kChannelNotVisible);
        }
        else if ((channel.m_visible == kChannelAlwaysVisible && !Visible) ||
                 (channel.m_visible == kChannelNeverVisible && Visible))
        {
            throw QString("Can't override Always/NeverVisible");
        }
    }
    if (HAS_PARAMv2("FrequencyID"))
        channel.m_freqId = FrequencyID;
    if (HAS_PARAMv2("Icon"))
        channel.m_icon = Icon;
    if (HAS_PARAMv2("Format"))
        channel.m_tvFormat = Format;
    if (HAS_PARAMv2("XMLTVID"))
        channel.m_xmltvId = XMLTVID;
    if (HAS_PARAMv2("DefaultAuthority"))
        channel.m_defaultAuthority = DefaultAuthority;
    if (HAS_PARAMv2("servicetype"))
        channel.m_serviceType = ServiceType;
    if (HAS_PARAMv2("RecPriority"))
        channel.m_recPriority = RecPriority;
    if (HAS_PARAMv2("TimeOffset"))
        channel.m_tmOffset = TimeOffset;
    if (HAS_PARAMv2("CommMethod"))
        channel.m_commMethod = CommMethod;
    bool bResult = ChannelUtil::UpdateChannel(
        channel.m_mplexId, channel.m_sourceId, channel.m_chanId,
        channel.m_callSign, channel.m_name, channel.m_chanNum,
        channel.m_serviceId, channel.m_atscMajorChan,
        channel.m_atscMinorChan, channel.m_useOnAirGuide,
        channel.m_visible, channel.m_freqId,
        channel.m_icon, channel.m_tvFormat, channel.m_xmltvId,
        channel.m_defaultAuthority, channel.m_serviceType,
        channel.m_recPriority, channel.m_tmOffset, channel.m_commMethod );

    if (bResult)
        ScheduledRecording::RescheduleMatch(0, 0, 0,
                             QDateTime(), "UpdateDBChannel");
    return bResult;
}


uint V2Channel::GetAvailableChanid ( void ) {
    uint chanId = 0;

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT MAX(chanid) FROM channel");
    if (!query.exec())
    {
        MythDB::DBError("V2Channel::AddDBChannel", query);
        throw( QString( "Database Error executing query." ));
    }
    if (query.next())
        chanId = query.value(0).toUInt() + 1;
    chanId = std::max<uint>(chanId, 1000);
    return chanId;
}

bool V2Channel::AddDBChannel( uint          MplexID,
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
                            int           CommMethod )
{
    ChannelVisibleType chan_visible = kChannelVisible;
    if (HAS_PARAMv2("ExtendedVisible"))
        chan_visible = channelVisibleTypeFromString(ExtendedVisible);
    else if (HAS_PARAMv2("Visible"))
        chan_visible = (Visible ? kChannelVisible : kChannelNotVisible);

    bool bResult = ChannelUtil::CreateChannel( MplexID, SourceID, ChannelID,
                             CallSign, ChannelName, ChannelNumber,
                             ServiceID, ATSCMajorChannel, ATSCMinorChannel,
                             UseEIT, chan_visible, FrequencyID,
                             Icon, Format, XMLTVID, DefaultAuthority,
                             ServiceType, RecPriority, TimeOffset, CommMethod );

    return bResult;
}

bool V2Channel::RemoveDBChannel( uint nChannelID )
{
    bool bResult = ChannelUtil::DeleteChannel( nChannelID );

    return bResult;
}


V2CommMethodList* V2Channel::GetCommMethodList  ( void )
{
    auto* pList = new V2CommMethodList();

    std::deque<int> tmp = GetPreferredSkipTypeCombinations();
    tmp.push_front(COMM_DETECT_UNINIT);
    tmp.push_back(COMM_DETECT_COMMFREE);

    for (int pref : tmp)
    {
        V2CommMethod* pCommMethod = pList->AddNewCommMethod();
        pCommMethod->setCommMethod(pref);
        pCommMethod->setLocalizedName(SkipTypeToString(pref));
    }
    return pList;
}


/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

V2VideoSourceList* V2Channel::GetVideoSourceList()
{
    MSqlQuery query(MSqlQuery::InitCon());

    if (!query.isConnected())
        throw( QString("Database not open while trying to list "
                       "Video Sources."));

    query.prepare("SELECT sourceid, name, xmltvgrabber, userid, "
                  "freqtable, lineupid, password, useeit, configpath, "
                  "dvb_nit_id, bouquet_id, region_id, scanfrequency, "
                  "lcnoffset FROM videosource "
                  "ORDER BY sourceid" );

    if (!query.exec())
    {
        MythDB::DBError("MythAPI::GetVideoSourceList()", query);

        throw( QString( "Database Error executing query." ));
    }

    // ----------------------------------------------------------------------
    // return the results of the query
    // ----------------------------------------------------------------------

    auto* pList = new V2VideoSourceList();

    while (query.next())
    {

        V2VideoSource *pVideoSource = pList->AddNewVideoSource();

        pVideoSource->setId            ( query.value(0).toInt()       );
        pVideoSource->setSourceName    ( query.value(1).toString()    );
        pVideoSource->setGrabber       ( query.value(2).toString()    );
        pVideoSource->setUserId        ( query.value(3).toString()    );
        pVideoSource->setFreqTable     ( query.value(4).toString()    );
        pVideoSource->setLineupId      ( query.value(5).toString()    );
        pVideoSource->setPassword      ( query.value(6).toString()    );
        pVideoSource->setUseEIT        ( query.value(7).toBool()      );
        pVideoSource->setConfigPath    ( query.value(8).toString()    );
        pVideoSource->setNITId         ( query.value(9).toInt()       );
        pVideoSource->setBouquetId     ( query.value(10).toUInt()     );
        pVideoSource->setRegionId      ( query.value(11).toUInt()     );
        pVideoSource->setScanFrequency ( query.value(12).toUInt()     );
        pVideoSource->setLCNOffset     ( query.value(13).toUInt()     );
    }

    pList->setAsOf          ( MythDate::current() );
    pList->setVersion       ( MYTH_BINARY_VERSION );
    pList->setProtoVer      ( MYTH_PROTO_VERSION  );

    return pList;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

V2VideoSource* V2Channel::GetVideoSource( uint nSourceID )
{
    MSqlQuery query(MSqlQuery::InitCon());

    if (!query.isConnected())
        throw( QString("Database not open while trying to list "
                       "Video Sources."));

    query.prepare("SELECT name, xmltvgrabber, userid, "
                  "freqtable, lineupid, password, useeit, configpath, "
                  "dvb_nit_id, bouquet_id, region_id, scanfrequency, "
                  "lcnoffset "
                  "FROM videosource WHERE sourceid = :SOURCEID "
                  "ORDER BY sourceid" );
    query.bindValue(":SOURCEID", nSourceID);

    if (!query.exec())
    {
        MythDB::DBError("MythAPI::GetVideoSource()", query);

        throw( QString( "Database Error executing query." ));
    }

    // ----------------------------------------------------------------------
    // return the results of the query
    // ----------------------------------------------------------------------

    auto *pVideoSource = new V2VideoSource();

    if (query.next())
    {
        pVideoSource->setId            ( nSourceID                    );
        pVideoSource->setSourceName    ( query.value(0).toString()    );
        pVideoSource->setGrabber       ( query.value(1).toString()    );
        pVideoSource->setUserId        ( query.value(2).toString()    );
        pVideoSource->setFreqTable     ( query.value(3).toString()    );
        pVideoSource->setLineupId      ( query.value(4).toString()    );
        pVideoSource->setPassword      ( query.value(5).toString()    );
        pVideoSource->setUseEIT        ( query.value(6).toBool()      );
        pVideoSource->setConfigPath    ( query.value(7).toString()    );
        pVideoSource->setNITId         ( query.value(8).toInt()       );
        pVideoSource->setBouquetId     ( query.value(9).toUInt()      );
        pVideoSource->setRegionId      ( query.value(10).toUInt()     );
        pVideoSource->setScanFrequency ( query.value(11).toUInt()     );
        pVideoSource->setLCNOffset     ( query.value(12).toUInt()     );
    }

    return pVideoSource;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

bool V2Channel::UpdateVideoSource( uint          nSourceId,
                                 const QString &sSourceName,
                                 const QString &sGrabber,
                                 const QString &sUserId,
                                 const QString &sFreqTable,
                                 const QString &sLineupId,
                                 const QString &sPassword,
                                 bool          bUseEIT,
                                 const QString &sConfigPath,
                                 int           nNITId,
                                 uint          nBouquetId,
                                 uint          nRegionId,
                                 uint          nScanFrequency,
                                 uint          nLCNOffset )
{

    if (!HAS_PARAMv2("SourceID"))
    {
        LOG(VB_GENERAL, LOG_ERR, "SourceId is required");
        return false;
    }

    if (!SourceUtil::IsSourceIDValid(nSourceId))
    {
        LOG(VB_GENERAL, LOG_ERR, QString("SourceId %1 doesn't exist")
            .arg(nSourceId));
        return false;
    }

    if (m_request->m_queries.size() < 2 )
    {
        LOG(VB_GENERAL, LOG_ERR, QString("SourceId=%1 was the only parameter")
            .arg(nSourceId));
        return false;
    }

    MSqlBindings bindings;
    MSqlBindings::const_iterator it;
    QString settings;

    if ( HAS_PARAMv2("SourceName") )
        ADD_SQLv2(settings, bindings, "name", "SourceName", sSourceName);

    if ( HAS_PARAMv2("Grabber") )
        ADD_SQLv2(settings, bindings, "xmltvgrabber", "Grabber", sGrabber);

    if ( HAS_PARAMv2("UserId") )
        ADD_SQLv2(settings, bindings, "userid", "UserId", sUserId);

    if ( HAS_PARAMv2("FreqTable") )
        ADD_SQLv2(settings, bindings, "freqtable", "FreqTable", sFreqTable);

    if ( HAS_PARAMv2("LineupId") )
        ADD_SQLv2(settings, bindings, "lineupid", "LineupId", sLineupId);

    if ( HAS_PARAMv2("Password") )
        ADD_SQLv2(settings, bindings, "password", "Password", sPassword);

    if ( HAS_PARAMv2("UseEIT") )
        ADD_SQLv2(settings, bindings, "useeit", "UseEIT", bUseEIT);

    if (HAS_PARAMv2("ConfigPath"))
    {
        if (sConfigPath.isEmpty())
            settings += "configpath=NULL, "; // mythfilldatabase grabber requirement
        else
            ADD_SQLv2(settings, bindings, "configpath", "ConfigPath", sConfigPath);
    }

    if ( HAS_PARAMv2("NITId") )
        ADD_SQLv2(settings, bindings, "dvb_nit_id", "NITId", nNITId);

    if ( HAS_PARAMv2("BouquetId") )
        ADD_SQLv2(settings, bindings, "bouquet_id", "BouquetId", nBouquetId);

    if ( HAS_PARAMv2("RegionId") )
        ADD_SQLv2(settings, bindings, "region_id", "RegionId", nRegionId);

    if ( HAS_PARAMv2("ScanFrequency") )
        ADD_SQLv2(settings, bindings, "scanfrequency", "ScanFrequency", nScanFrequency);

    if ( HAS_PARAMv2("LCNOffset") )
        ADD_SQLv2(settings, bindings, "lcnoffset", "LCNOffset", nLCNOffset);

    if ( settings.isEmpty() )
    {
        LOG(VB_GENERAL, LOG_ERR, "No valid parameters were passed");
        return false;
    }

    settings.chop(2);

    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare(QString("UPDATE videosource SET %1 WHERE sourceid=:SOURCEID")
                  .arg(settings));
    bindings[":SOURCEID"] = nSourceId;

    for (it = bindings.cbegin(); it != bindings.cend(); ++it)
        query.bindValue(it.key(), it.value());

    if (!query.exec())
    {
        MythDB::DBError("MythAPI::UpdateVideoSource()", query);

        throw( QString( "Database Error executing query." ));
    }

    return true;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

int  V2Channel::AddVideoSource( const QString &sSourceName,
                              const QString &sGrabber,
                              const QString &sUserId,
                              const QString &sFreqTable,
                              const QString &sLineupId,
                              const QString &sPassword,
                              bool          bUseEIT,
                              const QString &sConfigPath,
                              int           nNITId,
                              uint          nBouquetId,
                              uint          nRegionId,
                              uint          nScanFrequency,
                              uint          nLCNOffset )
{
    int nResult = SourceUtil::CreateSource(sSourceName, sGrabber, sUserId, sFreqTable,
                                       sLineupId, sPassword, bUseEIT, sConfigPath,
                                       nNITId, nBouquetId, nRegionId, nScanFrequency,
                                       nLCNOffset);

    return nResult;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

bool V2Channel::RemoveAllVideoSources( void )
{
    bool bResult = SourceUtil::DeleteAllSources();

    return bResult;
}

bool V2Channel::RemoveVideoSource( uint nSourceID )
{
    bool bResult = SourceUtil::DeleteSource( nSourceID );

    return bResult;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

V2LineupList* V2Channel::GetDDLineupList( const QString &/*sSource*/,
                                           const QString &/*sUserId*/,
                                           const QString &/*sPassword*/ )
{
    auto *pLineups = new V2LineupList();
    return pLineups;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

int V2Channel::FetchChannelsFromSource( const uint nSourceId,
                                      const uint nCardId,
                                      bool       bWaitForFinish )
{
    if ( nSourceId < 1 || nCardId < 1)
        throw( QString("A source ID and card ID are both required."));

    int nResult = 0;

    QString cardtype = CardUtil::GetRawInputType(nCardId);

    // These tests commented because they prevent fetching channels for CETON
    // cable card device, which is compatible with fetching and requires fetching.
    // if (!CardUtil::IsUnscanable(cardtype) &&
    //     !CardUtil::IsEncoder(cardtype))
    // {
    //     throw( QString("This device is incompatible with channel fetching.") );
    // }

    SourceUtil::UpdateChannelsFromListings(nSourceId, cardtype, bWaitForFinish);

    if (bWaitForFinish)
        nResult = SourceUtil::GetChannelCount(nSourceId);

    return nResult;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

V2VideoMultiplexList* V2Channel::GetVideoMultiplexList( uint nSourceID,
                                                         uint nStartIndex,
                                                         uint nCount )
{
    MSqlQuery query(MSqlQuery::InitCon());

    if (!query.isConnected())
        throw( QString("Database not open while trying to list "
                       "Video Sources."));
    QString where;
    if (nSourceID > 0)
        where = "WHERE sourceid = :SOURCEID";
    QString sql = QString("SELECT mplexid, sourceid, transportid, networkid, "
                  "frequency, inversion, symbolrate, fec, polarity, "
                  "modulation, bandwidth, lp_code_rate, transmission_mode, "
                  "guard_interval, visible, constellation, hierarchy, hp_code_rate, "
                  "mod_sys, rolloff, sistandard, serviceversion, updatetimestamp, "
                  "default_authority FROM dtv_multiplex %1 "
                  "ORDER BY mplexid").arg(where);

    query.prepare(sql);
    if (nSourceID > 0)
        query.bindValue(":SOURCEID", nSourceID);

    if (!query.exec())
    {
        MythDB::DBError("MythAPI::GetVideoMultiplexList()", query);

        throw( QString( "Database Error executing query." ));
    }

    uint muxCount = (uint)query.size();

    // ----------------------------------------------------------------------
    // Build Response
    // ----------------------------------------------------------------------

    auto *pVideoMultiplexes = new V2VideoMultiplexList();

    nStartIndex   = (nStartIndex > 0) ? std::min( nStartIndex, muxCount ) : 0;
    nCount        = (nCount > 0) ? std::min( nCount, muxCount ) : muxCount;
    int nEndIndex = std::min((nStartIndex + nCount), muxCount );

    for( int n = nStartIndex; n < nEndIndex; n++)
    {
        if (query.seek(n))
        {
            V2VideoMultiplex *pVideoMultiplex = pVideoMultiplexes->AddNewVideoMultiplex();

            pVideoMultiplex->setMplexId(            query.value(0).toInt()          );
            pVideoMultiplex->setSourceId(           query.value(1).toInt()          );
            pVideoMultiplex->setTransportId(        query.value(2).toInt()          );
            pVideoMultiplex->setNetworkId(          query.value(3).toInt()          );
            pVideoMultiplex->setFrequency(          query.value(4).toLongLong()     );
            pVideoMultiplex->setInversion(          query.value(5).toString()       );
            pVideoMultiplex->setSymbolRate(         query.value(6).toLongLong()     );
            pVideoMultiplex->setFEC(                query.value(7).toString()       );
            pVideoMultiplex->setPolarity(           query.value(8).toString()       );
            pVideoMultiplex->setModulation(         query.value(9).toString()       );
            pVideoMultiplex->setBandwidth(          query.value(10).toString()      );
            pVideoMultiplex->setLPCodeRate(         query.value(11).toString()      );
            pVideoMultiplex->setTransmissionMode(   query.value(12).toString()      );
            pVideoMultiplex->setGuardInterval(      query.value(13).toString()      );
            pVideoMultiplex->setVisible(            query.value(14).toBool()        );
            pVideoMultiplex->setConstellation(      query.value(15).toString()      );
            pVideoMultiplex->setHierarchy(          query.value(16).toString()      );
            pVideoMultiplex->setHPCodeRate(         query.value(17).toString()      );
            pVideoMultiplex->setModulationSystem(   query.value(18).toString()      );
            pVideoMultiplex->setRollOff(            query.value(19).toString()      );
            pVideoMultiplex->setSIStandard(         query.value(20).toString()      );
            pVideoMultiplex->setServiceVersion(     query.value(21).toInt()         );
            pVideoMultiplex->setUpdateTimeStamp(
                MythDate::as_utc(query.value(22).toDateTime()));
            pVideoMultiplex->setDefaultAuthority(   query.value(23).toString()      );

            // Code from libs/libmythtv/channelscan/multiplexsetting.cpp:53
            QString DisplayText;
            if (query.value(9).toString() == "8vsb")    //modulation
            {
                QString ChannelNumber =
                    // value(4) = frequency
                    QString("Freq %1").arg(query.value(4).toInt());
                int findFrequency = (query.value(4).toInt() / 1000) - 1750;
                for (const auto & list : gChanLists[0].list)
                {
                    if ((list.freq <= findFrequency + 200) &&
                        (list.freq >= findFrequency - 200))
                    {
                        ChannelNumber = QString("%1").arg(list.name);
                    }
                }
                //: %1 is the channel number
                DisplayText = tr("ATSC Channel %1").arg(ChannelNumber);
            }
            else
            {
                DisplayText = QString("%1 Hz (%2) (%3) (%4)")
                    .arg(query.value(4).toString(),     // frequency
                        query.value(6).toString(),      // symbolrate
                        query.value(3).toString(),      // networkid
                        query.value(2).toString());     // transportid
            }
            pVideoMultiplex->setDescription(DisplayText);
        }
    }

    int curPage = 0;
    int totalPages = 0;
    if (nCount == 0)
        totalPages = 1;
    else
        totalPages = (int)std::ceil((float)muxCount / nCount);

    if (totalPages == 1)
        curPage = 1;
    else
    {
        curPage = (int)std::ceil((float)nStartIndex / nCount) + 1;
    }

    pVideoMultiplexes->setStartIndex    ( nStartIndex     );
    pVideoMultiplexes->setCount         ( nCount          );
    pVideoMultiplexes->setCurrentPage   ( curPage         );
    pVideoMultiplexes->setTotalPages    ( totalPages      );
    pVideoMultiplexes->setTotalAvailable( muxCount        );
    pVideoMultiplexes->setAsOf          ( MythDate::current() );
    pVideoMultiplexes->setVersion       ( MYTH_BINARY_VERSION );
    pVideoMultiplexes->setProtoVer      ( MYTH_PROTO_VERSION  );

    return pVideoMultiplexes;
}

V2VideoMultiplex* V2Channel::GetVideoMultiplex( uint nMplexID )
{
    MSqlQuery query(MSqlQuery::InitCon());

    if (!query.isConnected())
        throw( QString("Database not open while trying to list "
                       "Video Multiplex."));

    query.prepare("SELECT sourceid, transportid, networkid, "
                  "frequency, inversion, symbolrate, fec, polarity, "
                  "modulation, bandwidth, lp_code_rate, transmission_mode, "
                  "guard_interval, visible, constellation, hierarchy, hp_code_rate, "
                  "mod_sys, rolloff, sistandard, serviceversion, updatetimestamp, "
                  "default_authority FROM dtv_multiplex WHERE mplexid = :MPLEXID "
                  "ORDER BY mplexid" );
    query.bindValue(":MPLEXID", nMplexID);

    if (!query.exec())
    {
        MythDB::DBError("MythAPI::GetVideoMultiplex()", query);

        throw( QString( "Database Error executing query." ));
    }

    auto *pVideoMultiplex = new V2VideoMultiplex();

    if (query.next())
    {
        pVideoMultiplex->setMplexId(            nMplexID                        );
        pVideoMultiplex->setSourceId(           query.value(0).toInt()          );
        pVideoMultiplex->setTransportId(        query.value(1).toInt()          );
        pVideoMultiplex->setNetworkId(          query.value(2).toInt()          );
        pVideoMultiplex->setFrequency(          query.value(3).toLongLong()     );
        pVideoMultiplex->setInversion(          query.value(4).toString()       );
        pVideoMultiplex->setSymbolRate(         query.value(5).toLongLong()     );
        pVideoMultiplex->setFEC(                query.value(6).toString()       );
        pVideoMultiplex->setPolarity(           query.value(7).toString()       );
        pVideoMultiplex->setModulation(         query.value(8).toString()       );
        pVideoMultiplex->setBandwidth(          query.value(9).toString()       );
        pVideoMultiplex->setLPCodeRate(         query.value(10).toString()      );
        pVideoMultiplex->setTransmissionMode(   query.value(11).toString()      );
        pVideoMultiplex->setGuardInterval(      query.value(12).toString()      );
        pVideoMultiplex->setVisible(            query.value(13).toBool()        );
        pVideoMultiplex->setConstellation(      query.value(14).toString()      );
        pVideoMultiplex->setHierarchy(          query.value(15).toString()      );
        pVideoMultiplex->setHPCodeRate(         query.value(16).toString()      );
        pVideoMultiplex->setModulationSystem(   query.value(17).toString()      );
        pVideoMultiplex->setRollOff(            query.value(18).toString()      );
        pVideoMultiplex->setSIStandard(         query.value(19).toString()      );
        pVideoMultiplex->setServiceVersion(     query.value(20).toInt()         );
        pVideoMultiplex->setUpdateTimeStamp(
            MythDate::as_utc(query.value(21).toDateTime()));
        pVideoMultiplex->setDefaultAuthority(   query.value(22).toString()      );
    }

    return pVideoMultiplex;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

QStringList V2Channel::GetXMLTVIdList( uint SourceID )
{
    MSqlQuery query(MSqlQuery::InitCon());

    if (!query.isConnected())
        throw( QString("Database not open while trying to get source name."));

    query.prepare("SELECT name FROM videosource WHERE sourceid = :SOURCEID ");
    query.bindValue(":SOURCEID", SourceID);

    if (!query.exec())
    {
        MythDB::DBError("MythAPI::GetXMLTVIdList()", query);

        throw( QString( "Database Error executing query." ));
    }

    QStringList idList;

    if (query.next())
    {
        QString sourceName = query.value(0).toString();

        QString xmltvFile = GetConfDir() + '/' + sourceName + ".xmltv";

        if (QFile::exists(xmltvFile))
        {
            QFile file(xmltvFile);
            if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
                return idList;

            while (!file.atEnd())
            {
                QByteArray line = file.readLine();

                if (line.startsWith("channel="))
                {
                    QString id = line.mid(8, -1).trimmed();
                    idList.append(id);
                }
            }

            idList.sort();
        }
    }
    else
    {
        throw(QString("SourceID (%1) not found").arg(SourceID));
    }

    return idList;
}

// Prevent concurrent access to tv_find_grabbers
static QMutex  lockGrabber;

V2GrabberList* V2Channel::GetGrabberList  (  )
{
    // ----------------------------------------------------------------------
    // Build Response
    // ----------------------------------------------------------------------

    QMutexLocker lock(&lockGrabber);

    auto *pGrabberList = new V2GrabberList();

    // Add default grabbers
    V2Grabber *pGrabber = pGrabberList->AddNewGrabber();
    pGrabber->setProgram("eitonly");
    pGrabber->setDisplayName(QObject::tr("Transmitted guide only (EIT)"));
    pGrabber = pGrabberList->AddNewGrabber();
    pGrabber->setProgram("/bin/true");
    pGrabber->setDisplayName(QObject::tr("No grabber"));

    QStringList args;
    args += "baseline";

    MythSystemLegacy find_grabber_proc("tv_find_grabbers", args,
                                        kMSStdOut | kMSRunShell);
    find_grabber_proc.Run(25s);
    LOG(VB_GENERAL, LOG_INFO,
        "Running 'tv_find_grabbers " + args.join(" ") + "'.");
    uint status = find_grabber_proc.Wait();

    if (status == GENERIC_EXIT_OK)
    {
        QTextStream ostream(find_grabber_proc.ReadAll());
        while (!ostream.atEnd())
        {
            QString grabber_list(ostream.readLine());
            QStringList grabber_split =
                grabber_list.split("|", Qt::SkipEmptyParts);
            const QString& grabber_name = grabber_split[1];
            QFileInfo grabber_file(grabber_split[0]);
            QString program = grabber_file.fileName();

            if (!pGrabberList->containsProgram(program))
            {
                pGrabber = pGrabberList->AddNewGrabber();
                pGrabber->setProgram(program);
                pGrabber->setDisplayName(grabber_name);
            }
            LOG(VB_GENERAL, LOG_DEBUG, "Found " + grabber_split[0]);
        }
        LOG(VB_GENERAL, LOG_INFO, "Finished running tv_find_grabbers");
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR, "Failed to run tv_find_grabbers");
    }

    return pGrabberList;
}

QStringList V2Channel::GetFreqTableList (  )
{
    // Channel Frequency Table
    QStringList freqList;
    freqList.append("default");
    for (const auto & freqEntry : gChanLists)
    {
        freqList.append(freqEntry.name);
    }
    return freqList;

}

bool  V2Channel::StartScan (uint CardId,
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
                            const QString &RollOff)
{
    ChannelScannerWeb * pScanner = ChannelScannerWeb::getInstance();
    if (pScanner->m_status == "RUNNING")
        return false;

    return pScanner->StartScan (CardId,
                                DesiredServices,
                                FreeToAirOnly,
                                ChannelNumbersOnly,
                                CompleteChannelsOnly,
                                FullChannelSearch,
                                RemoveDuplicates,
                                AddFullTS,
                                TestDecryptable,
                                ScanType,
                                FreqTable,
                                Modulation,
                                FirstChan,
                                LastChan,
                                ScanId,
                                IgnoreSignalTimeout,
                                FollowNITSetting,
                                MplexId,
                                Frequency,
                                Bandwidth,
                                Polarity,
                                SymbolRate,
                                Inversion,
                                Constellation,
                                ModSys,
                                CodeRateLP,
                                CodeRateHP,
                                FEC,
                                TransmissionMode,
                                GuardInterval,
                                Hierarchy,
                                RollOff);
}


V2ScanStatus*     V2Channel::GetScanStatus ()
{
    auto *pStatus = new V2ScanStatus();
    ChannelScannerWeb * pScanner = ChannelScannerWeb::getInstance();
    pStatus->setCardId(pScanner->m_cardid);
    pStatus->setStatus(pScanner->m_status);
    pStatus->setSignalLock(pScanner->m_statusLock);
    pStatus->setProgress(pScanner->m_statusProgress);
    pStatus->setSignalNoise(pScanner->m_statusSnr);
    pStatus->setSignalStrength(pScanner->m_statusSignalStrength);
    pStatus->setStatusLog(pScanner->m_statusLog);
    pStatus->setStatusText(pScanner->m_statusText);
    pStatus->setStatusTitle(pScanner->m_statusTitleText);
    pStatus->setDialogMsg(pScanner->m_dlgMsg);
    pStatus->setDialogButtons(pScanner->m_dlgButtons);
    pStatus->setDialogInputReq(pScanner->m_dlgInputReq);

    return pStatus;
}


bool  V2Channel::StopScan  ( uint Cardid )
{
    ChannelScannerWeb * pScanner = ChannelScannerWeb::getInstance();
    if (pScanner->m_status != "RUNNING" || pScanner->m_cardid != Cardid)
        return false;
    pScanner->stopScan();
    return true;
}

V2ScanList*  V2Channel::GetScanList  ( uint SourceId)
{
    auto scanList = LoadScanList(SourceId);
    auto * pResult = new V2ScanList();
    for (const ScanInfo &scan : scanList)
    {
        auto *pItem = pResult->AddNewScan();
        pItem->setScanId    (scan.m_scanid);
        pItem->setCardId    (scan.m_cardid);
        pItem->setSourceId  (scan.m_sourceid);
        pItem->setProcessed (scan.m_processed);
        pItem->setScanDate  (scan.m_scandate);
    }
    return pResult;
}

bool  V2Channel::SendScanDialogResponse ( uint Cardid,
                                          const QString &DialogString,
                                          int DialogButton )
{
    ChannelScannerWeb * pScanner = ChannelScannerWeb::getInstance();
    if (pScanner->m_cardid == Cardid)
    {
        pScanner->m_dlgString = DialogString;
        pScanner->m_dlgButton = DialogButton;
        pScanner->m_dlgButtons.clear();
        pScanner->m_dlgInputReq = false;
        pScanner->m_dlgMsg = "";
        pScanner->m_waitCondition.wakeOne();
        return true;
    }
    return false;
}

V2ChannelRestore* V2Channel::GetRestoreData ( uint SourceId,
                                    bool XmltvId,
                                    bool Icon,
                                    bool Visible)
{
    auto * pResult = new V2ChannelRestore();
    RestoreData* rd = RestoreData::getInstance(SourceId);
    QString result = rd->doRestore(XmltvId, Icon, Visible);
    if (result.isEmpty())
        throw( QString("GetRestoreData failed."));
    pResult->setNumChannels(rd->m_num_channels);
    pResult->setNumXLMTVID(rd->m_num_xmltvid);
    pResult->setNumIcon(rd->m_num_icon);
    pResult->setNumVisible(rd->m_num_visible);
    return pResult;
}

bool V2Channel::SaveRestoreData ( uint SourceId )
{
    RestoreData* rd = RestoreData::getInstance(SourceId);
    bool result = rd->doSave();
    RestoreData::freeInstance();
    return result;
}


bool V2Channel::CopyIconToBackend(const QString& Url, const QString& ChanId)
{
    QString filename = Url.section('/', -1);

    QString dirpath = GetConfDir();
    QDir configDir(dirpath);
    if (!configDir.exists() && !configDir.mkdir(dirpath))
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Could not create %1").arg(dirpath));
    }

    QString channelDir = QString("%1/%2").arg(configDir.absolutePath(),
                                           "/channels");
    QDir strChannelDir(channelDir);
    if (!strChannelDir.exists() && !strChannelDir.mkdir(channelDir))
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString("Could not create %1").arg(channelDir));
    }
    channelDir += "/";

    QString filePath = channelDir + filename;

    // If we get to this point we've already checked whether the icon already
    // exist locally, we want to download anyway to fix a broken image or
    // get the latest version of the icon

    QTemporaryFile tmpFile(filePath);
    if (!tmpFile.open())
    {
        LOG(VB_GENERAL, LOG_INFO, "Icon Download: Couldn't create temporary file");
        return false;
    }

    bool fRet = GetMythDownloadManager()->download(Url, tmpFile.fileName());

    if (!fRet)
    {
        LOG(VB_GENERAL, LOG_INFO,
            QString("Download for icon %1 failed").arg(filename));
        return false;
    }

    QImage icon(tmpFile.fileName());
    if (icon.isNull())
    {
        LOG(VB_GENERAL, LOG_INFO,
            QString("Downloaded icon for %1 isn't a valid image").arg(filename));
        return false;
    }

    // Remove any existing icon
    QFile file(filePath);
    file.remove();

    // Rename temporary file & prevent it being cleaned up
    tmpFile.rename(filePath);
    tmpFile.setAutoRemove(false);

    MSqlQuery query(MSqlQuery::InitCon());
    QString  qstr = "UPDATE channel SET icon = :ICON "
                    "WHERE chanid = :CHANID";

    query.prepare(qstr);
    query.bindValue(":ICON", filename);
    query.bindValue(":CHANID", ChanId);

    if (!query.exec())
    {
        MythDB::DBError("Error inserting channel icon", query);
        return false;
    }

    return fRet;
}
