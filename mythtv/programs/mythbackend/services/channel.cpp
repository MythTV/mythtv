//////////////////////////////////////////////////////////////////////////////
// Program Name: channel.cpp
// Created     : Apr. 8, 2011
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

#include <QList>

#include <math.h>

#include "channel.h"

#include "compat.h"
#include "mythdbcon.h"
#include "mythversion.h"
#include "mythcorecontext.h"
#include "channelutil.h"
#include "sourceutil.h"

#include "serviceUtil.h"

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

DTC::ChannelInfoList* Channel::GetChannelInfoList( int nSourceID,
                                                   int nStartIndex,
                                                   int nCount )
{
    vector<uint> chanList;

    chanList = ChannelUtil::GetChanIDs(nSourceID);

    // ----------------------------------------------------------------------
    // Build Response
    // ----------------------------------------------------------------------

    DTC::ChannelInfoList *pChannelInfos = new DTC::ChannelInfoList();

    nStartIndex   = min( nStartIndex, (int)chanList.size() );
    nCount        = (nCount > 0) ? min( nCount, (int)chanList.size() ) : chanList.size();
    int nEndIndex = nStartIndex + nCount;

    for( int n = nStartIndex; n < nEndIndex; n++)
    {
        DTC::ChannelInfo *pChannelInfo = pChannelInfos->AddNewChannelInfo();

        int chanid = chanList.at(n);
        QString channum = ChannelUtil::GetChanNum(chanid);
        QString format, modulation, freqtable, freqid, dtv_si_std,
                xmltvid, default_authority;
        int finetune, program_number;
        uint64_t frequency;
        uint atscmajor, atscminor, transportid, networkid, mplexid;
        bool commfree = false;
        bool eit = false;
        bool visible = true;

        if (ChannelUtil::GetExtendedChannelData( nSourceID, channum, format, modulation,
                            freqtable, freqid, finetune, frequency,
                            dtv_si_std, program_number, atscmajor,
                            atscminor, transportid, networkid, mplexid,
                            commfree, eit, visible, xmltvid, default_authority ))
        {
            pChannelInfo->setChanId(chanid);
            pChannelInfo->setChanNum(channum);
            pChannelInfo->setCallSign(ChannelUtil::GetCallsign(chanid));
            pChannelInfo->setIconURL(ChannelUtil::GetIcon(chanid)); //Wrong!
            pChannelInfo->setChannelName(ChannelUtil::GetServiceName(chanid));
            pChannelInfo->setMplexId(mplexid);
            pChannelInfo->setServiceId(program_number);
            pChannelInfo->setATSCMajorChan(atscmajor);
            pChannelInfo->setATSCMinorChan(atscminor);
            pChannelInfo->setFormat(format);
            pChannelInfo->setModulation(modulation);
            pChannelInfo->setFrequencyTable(freqtable);
            pChannelInfo->setFineTune(finetune);
            pChannelInfo->setFrequency((long)frequency);
            pChannelInfo->setFrequencyId(freqid);
            pChannelInfo->setSIStandard(dtv_si_std);
            pChannelInfo->setTransportId(transportid);
            pChannelInfo->setNetworkId(networkid);
            pChannelInfo->setChanFilters(ChannelUtil::GetVideoFilters(nSourceID, channum));
            pChannelInfo->setSourceId(nSourceID);
            pChannelInfo->setCommFree(commfree);
            pChannelInfo->setUseEIT(eit);
            pChannelInfo->setVisible(visible);
            pChannelInfo->setXMLTVID(xmltvid);
            pChannelInfo->setDefaultAuth(default_authority);
        }
    }

    int curPage = 0, totalPages = 0;
    if (nCount == 0)
        totalPages = 1;
    else
        totalPages = (int)ceil((float)chanList.size() / nCount);

    if (totalPages == 1)
        curPage = 1;
    else
    {
        curPage = (int)ceil((float)nStartIndex / nCount) + 1;
    }

    pChannelInfos->setStartIndex    ( nStartIndex     );
    pChannelInfos->setCount         ( nCount          );
    pChannelInfos->setCurrentPage   ( curPage         );
    pChannelInfos->setTotalPages    ( totalPages      );
    pChannelInfos->setTotalAvailable( chanList.size() );
    pChannelInfos->setAsOf          ( QDateTime::currentDateTime() );
    pChannelInfos->setVersion       ( MYTH_BINARY_VERSION );
    pChannelInfos->setProtoVer      ( MYTH_PROTO_VERSION  );

    return pChannelInfos;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

DTC::ChannelInfo* Channel::GetChannelInfo( int nChanID )
{
    if (nChanID == 0)
        throw( QString("Channel ID appears invalid."));

    DTC::ChannelInfo *pChannelInfo = new DTC::ChannelInfo();

    QString channum = ChannelUtil::GetChanNum(nChanID);
    uint sourceid = ChannelUtil::GetSourceIDForChannel(nChanID);
    QString format, modulation, freqtable, freqid, dtv_si_std,
            xmltvid, default_authority;
    int finetune, program_number;
    uint64_t frequency;
    uint atscmajor, atscminor, transportid, networkid, mplexid;
    bool commfree = false;
    bool eit = false;
    bool visible = true;

    if (ChannelUtil::GetExtendedChannelData( sourceid, channum, format, modulation,
                            freqtable, freqid, finetune, frequency,
                            dtv_si_std, program_number, atscmajor,
                            atscminor, transportid, networkid, mplexid,
                            commfree, eit, visible, xmltvid, default_authority ))
    {
        pChannelInfo->setChanId(nChanID);
        pChannelInfo->setChanNum(channum);
        pChannelInfo->setCallSign(ChannelUtil::GetCallsign(nChanID));
        pChannelInfo->setIconURL(ChannelUtil::GetIcon(nChanID));
        pChannelInfo->setChannelName(ChannelUtil::GetServiceName(nChanID));
        pChannelInfo->setMplexId(mplexid);
        pChannelInfo->setServiceId(program_number);
        pChannelInfo->setATSCMajorChan(atscmajor);
        pChannelInfo->setATSCMinorChan(atscminor);
        pChannelInfo->setFormat(format);
        pChannelInfo->setModulation(modulation);
        pChannelInfo->setFrequencyTable(freqtable);
        pChannelInfo->setFineTune(finetune);
        pChannelInfo->setFrequency((long)frequency);
        pChannelInfo->setFrequencyId(freqid);
        pChannelInfo->setSIStandard(dtv_si_std);
        pChannelInfo->setTransportId(transportid);
        pChannelInfo->setNetworkId(networkid);
        pChannelInfo->setChanFilters(ChannelUtil::GetVideoFilters(sourceid, channum));
        pChannelInfo->setSourceId(sourceid);
        pChannelInfo->setCommFree(commfree);
        pChannelInfo->setUseEIT(eit);
        pChannelInfo->setVisible(visible);
        pChannelInfo->setXMLTVID(xmltvid);
        pChannelInfo->setDefaultAuth(default_authority);
    }
    else
        throw( QString("Channel ID appears invalid."));

    return pChannelInfo;
}

bool Channel::UpdateDBChannel( uint          MplexID,
                               uint          SourceID,
                               uint          ChannelID,
                               const QString &CallSign,
                               const QString &ChannelName,
                               const QString &ChannelNumber,
                               uint          ServiceID,
                               uint          ATSCMajorChannel,
                               uint          ATSCMinorChannel,
                               bool          UseEIT,
                               bool          visible,
                               const QString &FrequencyID,
                               const QString &Icon,
                               const QString &Format,
                               const QString &XMLTVID,
                               const QString &DefaultAuthority )
{
    bool bResult = false;

    bResult = ChannelUtil::UpdateChannel( MplexID, SourceID, ChannelID,
                             CallSign, ChannelName, ChannelNumber,
                             ServiceID, ATSCMajorChannel, ATSCMinorChannel,
                             UseEIT, !visible, false, FrequencyID,
                             Icon, Format, XMLTVID, DefaultAuthority );

    return bResult;
}

bool Channel::CreateDBChannel( uint          MplexID,
                               uint          SourceID,
                               uint          ChannelID,
                               const QString &CallSign,
                               const QString &ChannelName,
                               const QString &ChannelNumber,
                               uint          ServiceID,
                               uint          ATSCMajorChannel,
                               uint          ATSCMinorChannel,
                               bool          UseEIT,
                               bool          visible,
                               const QString &FrequencyID,
                               const QString &Icon,
                               const QString &Format,
                               const QString &XMLTVID,
                               const QString &DefaultAuthority )
{
    bool bResult = false;

    bResult = ChannelUtil::CreateChannel( MplexID, SourceID, ChannelID,
                             CallSign, ChannelName, ChannelNumber,
                             ServiceID, ATSCMajorChannel, ATSCMinorChannel,
                             UseEIT, !visible, false, FrequencyID,
                             Icon, Format, XMLTVID, DefaultAuthority );

    return bResult;
}

bool Channel::DeleteDBChannel( uint nChannelID )
{
    bool bResult = false;

    bResult = ChannelUtil::DeleteChannel( nChannelID );

    return bResult;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

DTC::VideoSourceList* Channel::GetVideoSourceList()
{
    MSqlQuery query(MSqlQuery::InitCon());

    if (!query.isConnected())
        throw( QString("Database not open while trying to list "
                       "Video Sources."));

    query.prepare("SELECT sourceid, name, xmltvgrabber, userid, "
                  "freqtable, lineupid, password, useeit, configpath, "
                  "dvb_nit_id FROM videosource "
                  "ORDER BY sourceid" );

    if (!query.exec())
    {
        MythDB::DBError("MythAPI::GetVideoSourceList()", query);

        throw( QString( "Database Error executing query." ));
    }

    // ----------------------------------------------------------------------
    // return the results of the query
    // ----------------------------------------------------------------------

    DTC::VideoSourceList* pList = new DTC::VideoSourceList();

    while (query.next())
    {

        DTC::VideoSource *pVideoSource = pList->AddNewVideoSource();

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
    }

    pList->setAsOf          ( QDateTime::currentDateTime() );
    pList->setVersion       ( MYTH_BINARY_VERSION );
    pList->setProtoVer      ( MYTH_PROTO_VERSION  );

    return pList;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

DTC::VideoSource* Channel::GetVideoSource( uint nSourceID )
{
    MSqlQuery query(MSqlQuery::InitCon());

    if (!query.isConnected())
        throw( QString("Database not open while trying to list "
                       "Video Sources."));

    query.prepare("SELECT name, xmltvgrabber, userid, "
                  "freqtable, lineupid, password, useeit, configpath, "
                  "dvb_nit_id FROM videosource WHERE sourceid = :SOURCEID "
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

    DTC::VideoSource *pVideoSource = new DTC::VideoSource();

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
    }

    return pVideoSource;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

bool Channel::UpdateVideoSource( uint nSourceId,
                                 const QString &sSourceName,
                                 const QString &sGrabber,
                                 const QString &sUserId,
                                 const QString &sFreqTable,
                                 const QString &sLineupId,
                                 const QString &sPassword,
                                 bool          bUseEIT,
                                 const QString &sConfigPath,
                                 int           nNITId )
{
    bool bResult = false;

    bResult = SourceUtil::UpdateSource(nSourceId, sSourceName, sGrabber, sUserId, sFreqTable,
                                       sLineupId, sPassword, bUseEIT, sConfigPath,
                                       nNITId);

    return bResult;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

bool Channel::CreateVideoSource( const QString &sSourceName,
                                 const QString &sGrabber,
                                 const QString &sUserId,
                                 const QString &sFreqTable,
                                 const QString &sLineupId,
                                 const QString &sPassword,
                                 bool          bUseEIT,
                                 const QString &sConfigPath,
                                 int           nNITId )
{
    bool bResult = false;

    bResult = SourceUtil::CreateSource(sSourceName, sGrabber, sUserId, sFreqTable,
                                       sLineupId, sPassword, bUseEIT, sConfigPath,
                                       nNITId);

    return bResult;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

bool Channel::DeleteVideoSource( uint nSourceID )
{
    bool bResult = false;

    bResult = SourceUtil::DeleteSource( nSourceID );

    return bResult;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

DTC::VideoMultiplexList* Channel::GetVideoMultiplexList( int nSourceID,
                                                         int nStartIndex,
                                                         int nCount )
{
    MSqlQuery query(MSqlQuery::InitCon());

    if (!query.isConnected())
        throw( QString("Database not open while trying to list "
                       "Video Sources."));

    query.prepare("SELECT mplexid, sourceid, transportid, networkid, "
                  "frequency, inversion, symbolrate, fec, polarity, "
                  "msystem, modulation, bandwidth, lp_code_rate, transmission_mode, "
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

    int muxCount = query.size();

    // ----------------------------------------------------------------------
    // Build Response
    // ----------------------------------------------------------------------

    DTC::VideoMultiplexList *pVideoMultiplexes = new DTC::VideoMultiplexList();

    nStartIndex   = min( nStartIndex, muxCount );
    nCount        = (nCount > 0) ? min( nCount, muxCount ) : muxCount;
    int nEndIndex = nStartIndex + nCount;

    for( int n = nStartIndex; n < nEndIndex; n++)
    {
        if (query.seek(n))
        {
            DTC::VideoMultiplex *pVideoMultiplex = pVideoMultiplexes->AddNewVideoMultiplex();

            pVideoMultiplex->setMplexId(            query.value(0).toInt()          );
            pVideoMultiplex->setSourceId(           query.value(1).toInt()          );
            pVideoMultiplex->setTransportId(        query.value(2).toInt()          );
            pVideoMultiplex->setNetworkId(          query.value(3).toInt()          );
            pVideoMultiplex->setFrequency(          query.value(4).toLongLong()     );
            pVideoMultiplex->setInversion(          query.value(5).toString()       );
            pVideoMultiplex->setSymbolRate(         query.value(6).toLongLong()     );
            pVideoMultiplex->setFEC(                query.value(7).toString()       );
            pVideoMultiplex->setPolarity(           query.value(8).toString()       );
            pVideoMultiplex->setMSystem(            query.value(9).toString()       );
            pVideoMultiplex->setModulation(         query.value(10).toString()      );
            pVideoMultiplex->setBandwidth(          query.value(11).toString()      );
            pVideoMultiplex->setLPCodeRate(         query.value(12).toString()      );
            pVideoMultiplex->setTransmissionMode(   query.value(13).toString()      );
            pVideoMultiplex->setGuardInterval(      query.value(14).toString()      );
            pVideoMultiplex->setVisible(            query.value(15).toBool()        );
            pVideoMultiplex->setConstellation(      query.value(16).toString()      );
            pVideoMultiplex->setHierarchy(          query.value(17).toString()      );
            pVideoMultiplex->setHPCodeRate(         query.value(18).toString()      );
            pVideoMultiplex->setModulationSystem(   query.value(19).toString()      );
            pVideoMultiplex->setRollOff(            query.value(20).toString()      );
            pVideoMultiplex->setSIStandard(         query.value(21).toString()      );
            pVideoMultiplex->setServiceVersion(     query.value(22).toInt()         );
            pVideoMultiplex->setUpdateTimeStamp(    query.value(23).toDateTime()    );
            pVideoMultiplex->setDefaultAuthority(   query.value(24).toString()      );
        }
    }

    int curPage = 0, totalPages = 0;
    if (nCount == 0)
        totalPages = 1;
    else
        totalPages = (int)ceil((float)muxCount / nCount);

    if (totalPages == 1)
        curPage = 1;
    else
    {
        curPage = (int)ceil((float)nStartIndex / nCount) + 1;
    }

    pVideoMultiplexes->setStartIndex    ( nStartIndex     );
    pVideoMultiplexes->setCount         ( nCount          );
    pVideoMultiplexes->setCurrentPage   ( curPage         );
    pVideoMultiplexes->setTotalPages    ( totalPages      );
    pVideoMultiplexes->setTotalAvailable( muxCount        );
    pVideoMultiplexes->setAsOf          ( QDateTime::currentDateTime() );
    pVideoMultiplexes->setVersion       ( MYTH_BINARY_VERSION );
    pVideoMultiplexes->setProtoVer      ( MYTH_PROTO_VERSION  );

    return pVideoMultiplexes;
}

DTC::VideoMultiplex* Channel::GetVideoMultiplex( int nMplexID )
{
    MSqlQuery query(MSqlQuery::InitCon());

    if (!query.isConnected())
        throw( QString("Database not open while trying to list "
                       "Video Multiplex."));

    query.prepare("SELECT sourceid, transportid, networkid, "
                  "frequency, inversion, symbolrate, fec, polarity, "
                  "msystem, modulation, bandwidth, lp_code_rate, transmission_mode, "
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

    DTC::VideoMultiplex *pVideoMultiplex = new DTC::VideoMultiplex();

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
        pVideoMultiplex->setMSystem(            query.value(8).toString()       );
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
        pVideoMultiplex->setUpdateTimeStamp(    query.value(22).toDateTime()    );
        pVideoMultiplex->setDefaultAuthority(   query.value(23).toString()      );
    }

    return pVideoMultiplex;
}

