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

#include "mythlogging.h"

extern AutoExpire  *expirer;
extern Scheduler   *sched;

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

DTC::ProgramGuide *Guide::GetProgramGuide( const QDateTime &rawStartTime ,
                                           const QDateTime &rawEndTime   ,
                                           int              nStartChanId,
                                           int              nNumChannels,
                                           bool             bDetails,
                                           int              nChannelGroupId )
{     
    if (!rawStartTime.isValid())
        throw( "StartTime is invalid" );

    if (!rawEndTime.isValid())
        throw( "EndTime is invalid" );

    QDateTime dtStartTime = rawStartTime.toUTC();
    QDateTime dtEndTime = rawEndTime.toUTC();

    if (dtEndTime < dtStartTime)
        throw( "EndTime is before StartTime");

    if (nNumChannels == 0)
        nNumChannels = SHRT_MAX;

    // ----------------------------------------------------------------------
    // Find the ending channel Id
    // ----------------------------------------------------------------------

    int nEndChanId = nStartChanId;

    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare( "SELECT chanid FROM channel WHERE (chanid >= :STARTCHANID )"
                   " ORDER BY chanid LIMIT :NUMCHAN" );

    query.bindValue(":STARTCHANID", nStartChanId );
    query.bindValue(":NUMCHAN"    , nNumChannels );

    if (!query.exec())
        MythDB::DBError("Select ChanId", query);

    query.first();  nStartChanId = query.value(0).toInt();
    query.last();   nEndChanId   = query.value(0).toInt();

    // ----------------------------------------------------------------------
    // Build SQL statement for Program Listing
    // ----------------------------------------------------------------------

    ProgramList  progList;
    ProgramList  schedList;
    MSqlBindings bindings;

    // lpad is to allow natural sorting of numbers
    QString      sSQL;

    if (nChannelGroupId > 0)
    {
        sSQL = "LEFT JOIN channelgroup ON program.chanid = channelgroup.chanid "
                         "WHERE channelgroup.grpid = :CHANGRPID AND ";
        bindings[":CHANGRPID"  ] = nChannelGroupId;
    }
    else
        sSQL = "WHERE ";

    sSQL +=     "visible != 0 "
                "AND program.chanid >= :StartChanId "
                "AND program.chanid <= :EndChanId "
                "AND program.endtime >= :StartDate "
                "AND program.starttime <= :EndDate "
                "AND program.manualid = 0 " // Exclude programmes created purely for 'manual' recording schedules
                "GROUP BY program.starttime, channel.chanid "
                "ORDER BY LPAD(CAST(channum AS UNSIGNED), 10, 0), "
                "         LPAD(channum,  10, 0),             "
                "         callsign,                          "
                "         LPAD(program.chanid, 10, 0),       "
                "         program.starttime ";

    bindings[":StartChanId"] = nStartChanId;
    bindings[":EndChanId"  ] = nEndChanId;
    bindings[":StartDate"  ] = dtStartTime;
    bindings[":EndDate"    ] = dtEndTime;

    // ----------------------------------------------------------------------
    // Get all Pending Scheduled Programs
    // ----------------------------------------------------------------------

    bool hasConflicts;
    LoadFromScheduler(schedList, hasConflicts);

    // ----------------------------------------------------------------------

    LoadFromProgram( progList, sSQL, bindings, schedList );

    // ----------------------------------------------------------------------
    // Build Response
    // ----------------------------------------------------------------------

    DTC::ProgramGuide *pGuide = new DTC::ProgramGuide();

    int               nChanCount = 0;
    uint              nCurChanId = 0;
    DTC::ChannelInfo *pChannel   = NULL;
    QString           sCurCallsign;
    uint              nSkipChanId = 0;

    for( uint n = 0; n < progList.size(); n++)
    {
        ProgramInfo *pInfo = progList[ n ];

        if ( nSkipChanId == pInfo->GetChanID())
            continue;

        if ( nCurChanId != pInfo->GetChanID() )
        {
            nChanCount++;

            nCurChanId = pInfo->GetChanID();

            // Filter out channels with the same callsign, keeping just the
            // first seen
            if (sCurCallsign == pInfo->GetChannelSchedulingID())
            {
                nSkipChanId = pInfo->GetChanID();
                continue;
            }

            pChannel = pGuide->AddNewChannel();

            FillChannelInfo( pChannel, pInfo->GetChanID(), bDetails );

            sCurCallsign = pChannel->CallSign();
        }
        
        DTC::Program *pProgram = pChannel->AddNewProgram();

        FillProgramInfo( pProgram, pInfo, false, bDetails, false ); // No cast info
    }

    // ----------------------------------------------------------------------

    pGuide->setStartTime    ( dtStartTime   );
    pGuide->setEndTime      ( dtEndTime     );
    pGuide->setStartChanId  ( nStartChanId  );
    pGuide->setEndChanId    ( nEndChanId    );
    pGuide->setNumOfChannels( nChanCount    );
    pGuide->setDetails      ( bDetails      );
    
    pGuide->setCount        ( progList.size());
    pGuide->setAsOf         ( MythDate::current() );
    
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

    if (!rawStartTime.isNull() && !rawEndTime.isValid())
        throw( "EndTime is invalid" );

    QDateTime dtStartTime = rawStartTime;
    QDateTime dtEndTime = rawEndTime;

    if (dtEndTime < dtStartTime)
        throw( "EndTime is before StartTime");

    MSqlQuery query(MSqlQuery::InitCon());

    // ----------------------------------------------------------------------
    // Check that the requested channel exists
    // ----------------------------------------------------------------------
    if (nChanId < 0)
        nChanId = 0;

//     if (nChanId > 0)
//     {
//         query.prepare( "SELECT chanid FROM channel WHERE chanid = :CHANID " );
//
//         query.bindValue(":CHANID", nChanId );
//
//         if (!query.exec())
//             MythDB::DBError("Select ChanId", query);
//
//         if (!query.next())
//             throw( "ChanId is invalid");
//     }

    // ----------------------------------------------------------------------
    // Build SQL statement for Program Listing
    // ----------------------------------------------------------------------

    ProgramList  progList;
    ProgramList  schedList;
    MSqlBindings bindings;

    // lpad is to allow natural sorting of numbers
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

    sSQL +=     "GROUP BY program.starttime, channel.chanid ";

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

    bool hasConflicts;
    LoadFromScheduler(schedList, hasConflicts);

    // ----------------------------------------------------------------------

    LoadFromProgram( progList, sSQL, bindings, schedList );

    // ----------------------------------------------------------------------
    // Build Response
    // ----------------------------------------------------------------------

    DTC::ProgramList *pPrograms = new DTC::ProgramList();

    nStartIndex   = min( nStartIndex, (int)progList.size() );
    nCount        = (nCount > 0) ? min( nCount, (int)progList.size() ) : progList.size();
    int nEndIndex = min((nStartIndex + nCount), (int)progList.size() );

    for( int n = nStartIndex; n < nEndIndex; n++)
    {
        ProgramInfo *pInfo = progList[ n ];

        DTC::Program *pProgram = pPrograms->AddNewProgram();

        FillProgramInfo( pProgram, pInfo, true, bDetails, false ); // No cast info, loading this takes far too longs
    }

    // ----------------------------------------------------------------------

    pPrograms->setStartIndex    ( nStartIndex     );
    pPrograms->setCount         ( nCount          );
    pPrograms->setTotalAvailable( progList.size() );
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

    query.prepare("SELECT DISTINCT category FROM program ORDER BY category");

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

