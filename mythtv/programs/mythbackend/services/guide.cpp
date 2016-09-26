//////////////////////////////////////////////////////////////////////////////
// Program Name: guide.cpp
// Created     : Mar. 7, 2011
//
// Copyright (c) 2011 David Blain <dblain@mythtv.org>
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

#include <math.h>

#include "guide.h"

#include "compat.h"
#include "mythversion.h"
#include "mythcorecontext.h"
#include "scheduler.h"
#include "autoexpire.h"
#include "channelutil.h"
#include "channelgroup.h"
#include "storagegroup.h"

#include "mythlogging.h"

extern AutoExpire  *expirer;
extern Scheduler   *sched;

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

DTC::ProgramGuide *Guide::GetProgramGuide( const QDateTime &rawStartTime ,
                                           const QDateTime &rawEndTime   ,
                                           bool             bDetails,
                                           int              nChannelGroupId,
                                           int              nStartIndex,
                                           int              nCount)
{
    if (!rawStartTime.isValid())
        throw( "StartTime is invalid" );

    if (!rawEndTime.isValid())
        throw( "EndTime is invalid" );

    QDateTime dtStartTime = rawStartTime.toUTC();
    QDateTime dtEndTime = rawEndTime.toUTC();

    if (dtEndTime < dtStartTime)
        throw( "EndTime is before StartTime");

    if (nStartIndex <= 0)
        nStartIndex = 0;

    if (nCount <= 0)
        nCount = 20000;

    // ----------------------------------------------------------------------
    // Load the channel list
    // ----------------------------------------------------------------------

    uint nTotalAvailable = 0;
    ChannelInfoList chanList = ChannelUtil::LoadChannels(nStartIndex, nCount,
                                                         nTotalAvailable, true,
                                                         ChannelUtil::kChanOrderByChanNum,
                                                         ChannelUtil::kChanGroupByCallsign,
                                                         0,
                                                         nChannelGroupId);

    // ----------------------------------------------------------------------
    // Build SQL statement for Program Listing
    // ----------------------------------------------------------------------

    ProgramList  schedList;
    MSqlBindings bindings;

    QString sWhere   = "program.chanid = :CHANID "
                       "AND program.endtime >= :STARTDATE "
                       "AND program.starttime < :ENDDATE "
                       "AND program.starttime >= :STARTDATELIMIT "
                       "AND program.manualid = 0"; // Omit 'manual' recordings scheds

    QString sGroupBy = "program.starttime, channel.channum,"
                       "channel.callsign, program.title";

    QString sOrderBy = "program.starttime";

    bindings[":STARTDATE"     ] = dtStartTime;
    bindings[":STARTDATELIMIT"] = dtStartTime.addDays(-1);
    bindings[":ENDDATE"       ] = dtEndTime;

    // ----------------------------------------------------------------------
    // Get all Pending Scheduled Programs
    // ----------------------------------------------------------------------

    // NOTE: Fetching this information directly from the schedule is
    //       significantly faster than using ProgramInfo::LoadFromScheduler()
    Scheduler *scheduler = dynamic_cast<Scheduler*>(gCoreContext->GetScheduler());
    if (scheduler)
        scheduler->GetAllPending(schedList);

    // ----------------------------------------------------------------------
    // Build Response
    // ----------------------------------------------------------------------

    DTC::ProgramGuide *pGuide = new DTC::ProgramGuide();

    ChannelInfoList::iterator chan_it;
    for (chan_it = chanList.begin(); chan_it != chanList.end(); ++chan_it)
    {
        // Create ChannelInfo Object
        DTC::ChannelInfo *pChannel   = NULL;
        pChannel = pGuide->AddNewChannel();
        FillChannelInfo( pChannel, (*chan_it), bDetails );

        // Load the list of programmes for this channel
        ProgramList  progList;
        bindings[":CHANID"] = (*chan_it).chanid;
        LoadFromProgram( progList, sWhere, sOrderBy, sOrderBy, bindings,
                         schedList );

        // Create Program objects and add them to the channel object
        ProgramList::iterator progIt;
        for( progIt = progList.begin(); progIt != progList.end(); ++progIt)
        {
            DTC::Program *pProgram = pChannel->AddNewProgram();
            FillProgramInfo( pProgram, *progIt, false, bDetails, false ); // No cast info
        }
    }

    // ----------------------------------------------------------------------

    pGuide->setStartTime    ( dtStartTime   );
    pGuide->setEndTime      ( dtEndTime     );
    pGuide->setDetails      ( bDetails      );

    pGuide->setStartIndex    ( nStartIndex     );
    pGuide->setCount         ( chanList.size() );
    pGuide->setTotalAvailable( nTotalAvailable );
    pGuide->setAsOf          ( MythDate::current() );

    pGuide->setVersion      ( MYTH_BINARY_VERSION );
    pGuide->setProtoVer     ( MYTH_PROTO_VERSION  );

    return pGuide;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

DTC::ProgramList* Guide::GetProgramList(int              nStartIndex,
                                        int              nCount,
                                        const QDateTime& rawStartTime,
                                        const QDateTime& rawEndTime,
                                        int nChanId,
                                        const QString& sTitleFilter,
                                        const QString& sCategoryFilter,
                                        const QString& sPersonFilter,
                                        const QString& sKeywordFilter,
                                        bool bOnlyNew,
                                        bool bDetails,
                                        const QString   &sSort,
                                        bool             bDescending)
{
    if (!rawStartTime.isNull() && !rawStartTime.isValid())
        throw( "StartTime is invalid" );

    if (!rawEndTime.isNull() && !rawEndTime.isValid())
        throw( "EndTime is invalid" );

    QDateTime dtStartTime = rawStartTime;
    QDateTime dtEndTime = rawEndTime;

    if (!rawEndTime.isNull() && dtEndTime < dtStartTime)
        throw( "EndTime is before StartTime");

    MSqlQuery query(MSqlQuery::InitCon());


    // ----------------------------------------------------------------------
    // Build SQL statement for Program Listing
    // ----------------------------------------------------------------------

    ProgramList  progList;
    ProgramList  schedList;
    MSqlBindings bindings;

    QString      sSQL;

    if (!sPersonFilter.isEmpty())
    {
        sSQL = ", people, credits " // LEFT JOIN
               "WHERE people.name LIKE :PersonFilter "
               "AND credits.person = people.person "
               "AND program.chanid = credits.chanid "
               "AND program.starttime = credits.starttime AND ";
        bindings[":PersonFilter"] = QString("%%1%").arg(sPersonFilter);
    }
    else
        sSQL = "WHERE ";

    sSQL +=    "visible != 0 AND program.manualid = 0 "; // Exclude programmes created purely for 'manual' recording schedules

    if (nChanId < 0)
        nChanId = 0;

    if (nChanId > 0)
    {
        sSQL += "AND program.chanid = :ChanId ";
        bindings[":ChanId"]      = nChanId;
    }

    if (dtStartTime.isNull())
        dtStartTime = QDateTime::currentDateTimeUtc();

    sSQL += " AND program.endtime >= :StartDate ";
    bindings[":StartDate"] = dtStartTime;

    if (!dtEndTime.isNull())
    {
        sSQL += "AND program.starttime <= :EndDate ";
        bindings[":EndDate"] = dtEndTime;
    }

    if (!sTitleFilter.isEmpty())
    {
        sSQL += "AND program.title LIKE :Title ";
        bindings[":Title"] = QString("%%1%").arg(sTitleFilter);
    }

    if (!sCategoryFilter.isEmpty())
    {
        sSQL += "AND program.category LIKE :Category ";
        bindings[":Category"] = sCategoryFilter;
    }

    if (!sKeywordFilter.isEmpty())
    {
        sSQL += "AND (program.title LIKE :Keyword1 "
                "OR   program.subtitle LIKE :Keyword2 "
                "OR   program.description LIKE :Keyword3) ";

        QString filter = QString("%%1%").arg(sKeywordFilter);
        bindings[":Keyword1"] = filter;
        bindings[":Keyword2"] = filter;
        bindings[":Keyword3"] = filter;
    }

    if (sSort == "starttime")
        sSQL += "ORDER BY program.starttime ";
    else if (sSort == "title")
        sSQL += "ORDER BY program.title ";
    else if (sSort == "channel")
        sSQL += "ORDER BY channel.channum ";
    else if (sSort == "duration")
        sSQL += "ORDER BY (program.endtime - program.starttime) ";
    else
        sSQL += "ORDER BY program.starttime ";

    if (bDescending)
        sSQL += "DESC ";
    else
        sSQL += "ASC ";

    // ----------------------------------------------------------------------
    // Get all Pending Scheduled Programs
    // ----------------------------------------------------------------------

    // NOTE: Fetching this information directly from the schedule is
    //       significantly faster than using ProgramInfo::LoadFromScheduler()
    Scheduler *scheduler = dynamic_cast<Scheduler*>(gCoreContext->GetScheduler());
    if (scheduler)
        scheduler->GetAllPending(schedList);

    // ----------------------------------------------------------------------

    uint nTotalAvailable = 0;
    LoadFromProgram( progList, sSQL, bindings, schedList,
                     (uint)nStartIndex, (uint)nCount, nTotalAvailable);

    // ----------------------------------------------------------------------
    // Build Response
    // ----------------------------------------------------------------------

    DTC::ProgramList *pPrograms = new DTC::ProgramList();

    nCount        = (int)progList.size();
    int nEndIndex = (int)progList.size();

    for( int n = 0; n < nEndIndex; n++)
    {
        ProgramInfo *pInfo = progList[ n ];

        DTC::Program *pProgram = pPrograms->AddNewProgram();

        FillProgramInfo( pProgram, pInfo, true, bDetails, false ); // No cast info, loading this takes far too long
    }

    // ----------------------------------------------------------------------

    pPrograms->setStartIndex    ( nStartIndex     );
    pPrograms->setCount         ( nCount          );
    pPrograms->setTotalAvailable( nTotalAvailable );
    pPrograms->setAsOf          ( MythDate::current() );
    pPrograms->setVersion       ( MYTH_BINARY_VERSION );
    pPrograms->setProtoVer      ( MYTH_PROTO_VERSION  );

    return pPrograms;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

DTC::Program* Guide::GetProgramDetails( int              nChanId,
                                        const QDateTime &rawStartTime )

{
    if (!(nChanId > 0))
        throw( "Channel ID is invalid" );
    if (!rawStartTime.isValid())
        throw( "StartTime is invalid" );

    QDateTime dtStartTime = rawStartTime.toUTC();

    // ----------------------------------------------------------------------
    // -=>TODO: Add support for getting Recorded Program Info
    // ----------------------------------------------------------------------

    // Build Response

    DTC::Program *pProgram = new DTC::Program();
    ProgramInfo  *pInfo    = LoadProgramFromProgram(nChanId, dtStartTime);

    FillProgramInfo( pProgram, pInfo, true, true, true );

    delete pInfo;

    return pProgram;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

QFileInfo Guide::GetChannelIcon( int nChanId,
                                 int nWidth  /* = 0 */,
                                 int nHeight /* = 0 */ )
{
    // Get Icon file path

    QString sFileName = ChannelUtil::GetIcon( nChanId );

    if (sFileName.isEmpty())
        return QFileInfo();

    // ------------------------------------------------------------------
    // Search for the filename
    // ------------------------------------------------------------------

    StorageGroup storage( "ChannelIcons" );
    QString sFullFileName = storage.FindFile( sFileName );

    if (sFullFileName.isEmpty())
    {
        LOG(VB_UPNP, LOG_ERR,
            QString("GetImageFile - Unable to find %1.").arg(sFileName));

        return QFileInfo();
    }

    // ----------------------------------------------------------------------
    // check to see if the file (still) exists
    // ----------------------------------------------------------------------

    if ((nWidth == 0) && (nHeight == 0))
    {
        if (QFile::exists( sFullFileName ))
        {
            return QFileInfo( sFullFileName );
        }

        LOG(VB_UPNP, LOG_ERR,
            QString("GetImageFile - File Does not exist %1.").arg(sFullFileName));

        return QFileInfo();
    }
    // -------------------------------------------------------------------

    QString sNewFileName = QString( "%1.%2x%3.png" )
                              .arg( sFullFileName )
                              .arg( nWidth    )
                              .arg( nHeight   );

    // ----------------------------------------------------------------------
    // check to see if image is already created.
    // ----------------------------------------------------------------------

    if (QFile::exists( sNewFileName ))
        return QFileInfo( sNewFileName );

    // ----------------------------------------------------------------------
    // We need to create it...
    // ----------------------------------------------------------------------

    float fAspect = 0.0;

    QImage *pImage = new QImage( sFullFileName );

    if (!pImage)
        return QFileInfo();

    if (fAspect <= 0)
           fAspect = (float)(pImage->width()) / pImage->height();

    if (fAspect == 0)
    {
        delete pImage;
        return QFileInfo();
    }

    if ( nWidth == 0 )
        nWidth = (int)rint(nHeight * fAspect);

    if ( nHeight == 0 )
        nHeight = (int)rint(nWidth / fAspect);

    QImage img = pImage->scaled( nWidth, nHeight, Qt::IgnoreAspectRatio,
                                Qt::SmoothTransformation);

    img.save( sNewFileName, "PNG" );

    delete pImage;

    return QFileInfo( sNewFileName );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

DTC::ChannelGroupList* Guide::GetChannelGroupList( bool bIncludeEmpty )
{
    ChannelGroupList list = ChannelGroup::GetChannelGroups(bIncludeEmpty);
    DTC::ChannelGroupList *pGroupList = new DTC::ChannelGroupList();

    ChannelGroupList::iterator it;
    for (it = list.begin(); it < list.end(); ++it)
    {
        DTC::ChannelGroup *pGroup = pGroupList->AddNewChannelGroup();
        FillChannelGroup(pGroup, (*it));
    }

    return pGroupList;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

QStringList Guide::GetCategoryList( ) //int nStartIndex, int nCount)
{
    QStringList catList;
    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("SELECT DISTINCT category FROM program WHERE category != '' "
                  "ORDER BY category");

    if (!query.exec())
        return catList;

    while (query.next())
    {
        catList << query.value(0).toString();
    }

    return catList;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

QStringList Guide::GetStoredSearches( const QString& sType )
{
    QStringList keywordList;
    MSqlQuery query(MSqlQuery::InitCon());

    RecSearchType iType = searchTypeFromString(sType);

    if (iType == kNoSearch)
    {
        //throw( "Invalid Type" );
        return keywordList;
    }

    query.prepare("SELECT DISTINCT phrase FROM keyword "
                  "WHERE searchtype = :TYPE "
                  "ORDER BY phrase");
    query.bindValue(":TYPE", static_cast<int>(iType));

    if (!query.exec())
        return keywordList;

    while (query.next())
    {
        keywordList << query.value(0).toString();
    }

    return keywordList;
}
