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

#include <QList>

#include <cmath>

#include "libmythbase/http/mythhttpmetaservice.h"
#include "v2channel.h"
#include "v2programAndChannel.h"
#include "v2recording.h"
#include "v2artworkInfoList.h"
#include "v2castMemberList.h"

#include "compat.h"
#include "mythdbcon.h"
#include "mythdirs.h"
#include "mythversion.h"
#include "mythcorecontext.h"
#include "channelutil.h"
#include "sourceutil.h"
#include "cardutil.h"
#include "mythdate.h"

#include "v2serviceUtil.h"

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

        ChannelInfo channelInfo = (*chanIt);

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
                               uint          ServiceType )
{
    if (!HAS_PARAM("ChannelID"))
        throw QString("ChannelId is required");

    if (m_request->m_queries.size() < 2 )
        throw QString("Nothing to update");

    ChannelInfo channel;
    if (!channel.Load(ChannelID))
        throw QString("ChannelId %1 doesn't exist");

    if (HAS_PARAM("MplexID"))
        channel.m_mplexId = MplexID;
    if (HAS_PARAM("SourceID"))
        channel.m_sourceId = SourceID;
    if (HAS_PARAM("CallSign"))
        channel.m_callSign = CallSign;
    if (HAS_PARAM("ChannelName"))
        channel.m_name = ChannelName;
    if (HAS_PARAM("ChannelNumber"))
        channel.m_chanNum = ChannelNumber;
    if (HAS_PARAM("ServiceID"))
        channel.m_serviceId = ServiceID;
    if (HAS_PARAM("ATSCMajorChannel"))
        channel.m_atscMajorChan = ATSCMajorChannel;
    if (HAS_PARAM("ATSCMinorChannel"))
        channel.m_atscMinorChan = ATSCMinorChannel;
    if (HAS_PARAM("UseEIT"))
        channel.m_useOnAirGuide = UseEIT;
    if (HAS_PARAM("ExtendedVisible"))
        channel.m_visible = channelVisibleTypeFromString(ExtendedVisible);
    else if (HAS_PARAM("Visible"))
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
    if (HAS_PARAM("FrequencyID"))
        channel.m_freqId = FrequencyID;
    if (HAS_PARAM("Icon"))
        channel.m_icon = Icon;
    if (HAS_PARAM("Format"))
        channel.m_tvFormat = Format;
    if (HAS_PARAM("XMLTVID"))
        channel.m_xmltvId = XMLTVID;
    if (HAS_PARAM("DefaultAuthority"))
        channel.m_defaultAuthority = DefaultAuthority;
    if (HAS_PARAM("servicetype"))
        channel.m_serviceType = ServiceType;

    bool bResult = ChannelUtil::UpdateChannel(
        channel.m_mplexId, channel.m_sourceId, channel.m_chanId,
        channel.m_callSign, channel.m_name, channel.m_chanNum,
        channel.m_serviceId, channel.m_atscMajorChan,
        channel.m_atscMinorChan, channel.m_useOnAirGuide,
        channel.m_visible, channel.m_freqId,
        channel.m_icon, channel.m_tvFormat, channel.m_xmltvId,
        channel.m_defaultAuthority, channel.m_serviceType );

    return bResult;
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
                            uint          ServiceType )
{
    ChannelVisibleType chan_visible = kChannelVisible;
    if (HAS_PARAM("ExtendedVisible"))
        chan_visible = channelVisibleTypeFromString(ExtendedVisible);
    else if (HAS_PARAM("Visible"))
        chan_visible = (Visible ? kChannelVisible : kChannelNotVisible);

    bool bResult = ChannelUtil::CreateChannel( MplexID, SourceID, ChannelID,
                             CallSign, ChannelName, ChannelNumber,
                             ServiceID, ATSCMajorChannel, ATSCMinorChannel,
                             UseEIT, chan_visible, FrequencyID,
                             Icon, Format, XMLTVID, DefaultAuthority,
                             ServiceType );

    return bResult;
}

bool V2Channel::RemoveDBChannel( uint nChannelID )
{
    bool bResult = ChannelUtil::DeleteChannel( nChannelID );

    return bResult;
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

    if (!HAS_PARAM("SourceID"))
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

    if ( HAS_PARAM("SourceName") )
        ADD_SQL(settings, bindings, "name", "SourceName", sSourceName)

    if ( HAS_PARAM("Grabber") )
        ADD_SQL(settings, bindings, "xmltvgrabber", "Grabber", sGrabber)

    if ( HAS_PARAM("UserId") )
        ADD_SQL(settings, bindings, "userid", "UserId", sUserId)

    if ( HAS_PARAM("FreqTable") )
        ADD_SQL(settings, bindings, "freqtable", "FreqTable", sFreqTable)

    if ( HAS_PARAM("LineupId") )
        ADD_SQL(settings, bindings, "lineupid", "LineupId", sLineupId)

    if ( HAS_PARAM("Password") )
        ADD_SQL(settings, bindings, "password", "Password", sPassword)

    if ( HAS_PARAM("UseEIT") )
        ADD_SQL(settings, bindings, "useeit", "UseEIT", bUseEIT)

    if (HAS_PARAM("ConfigPath"))
    {
        if (sConfigPath.isEmpty())
            settings += "configpath=NULL, "; // mythfilldatabase grabber requirement
        else
            ADD_SQL(settings, bindings, "configpath", "ConfigPath", sConfigPath)
    }

    if ( HAS_PARAM("NITId") )
        ADD_SQL(settings, bindings, "dvb_nit_id", "NITId", nNITId)

    if ( HAS_PARAM("BouquetId") )
        ADD_SQL(settings, bindings, "bouquet_id", "BouquetId", nBouquetId)

    if ( HAS_PARAM("RegionId") )
        ADD_SQL(settings, bindings, "region_id", "RegionId", nRegionId)

    if ( HAS_PARAM("ScanFrequency") )
        ADD_SQL(settings, bindings, "scanfrequency", "ScanFrequency", nScanFrequency)

    if ( HAS_PARAM("LCNOffset") )
        ADD_SQL(settings, bindings, "lcnoffset", "LCNOffset", nLCNOffset)

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

    if (!CardUtil::IsUnscanable(cardtype) &&
        !CardUtil::IsEncoder(cardtype))
    {
        throw( QString("This device is incompatible with channel fetching.") );
    }

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

    query.prepare("SELECT mplexid, sourceid, transportid, networkid, "
                  "frequency, inversion, symbolrate, fec, polarity, "
                  "modulation, bandwidth, lp_code_rate, transmission_mode, "
                  "guard_interval, visible, constellation, hierarchy, hp_code_rate, "
                  "mod_sys, rolloff, sistandard, serviceversion, updatetimestamp, "
                  "default_authority FROM dtv_multiplex WHERE sourceid = :SOURCEID "
                  "ORDER BY mplexid" );
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
        throw(QString("SourceID (%1) not found").arg(SourceID));

    return idList;
}
