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

// C++
#include <algorithm>
#include <cmath>

// MythTV
#include "libmythbase/compat.h"
#include "libmythbase/http/mythhttpmetaservice.h"
#include "libmythbase/mythcorecontext.h"
#include "libmythbase/mythlogging.h"
#include "libmythbase/mythversion.h"
#include "libmythbase/storagegroup.h"
#include "libmythtv/channelgroup.h"
#include "libmythtv/channelutil.h"

// MythBackend
#include "autoexpire.h"
#include "scheduler.h"
#include "v2artworkInfoList.h"
#include "v2castMemberList.h"
#include "v2guide.h"

// Qt6 has made the QFileInfo::QFileInfo(QString) constructor
// explicit, which means that it is no longer possible to use an
// initializer list to construct a QFileInfo. Disable that clang-tidy
// check for this file so it can still be run on the rest of the file
// in the project.
//
// NOLINTBEGIN(modernize-return-braced-init-list)

extern AutoExpire  *expirer;
extern Scheduler   *sched;

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

// This will be initialised in a thread safe manner on first use
Q_GLOBAL_STATIC_WITH_ARGS(MythHTTPMetaService, s_service,
    (GUIDE_HANDLE, V2Guide::staticMetaObject, &V2Guide::RegisterCustomTypes))

void V2Guide::RegisterCustomTypes()
{
    qRegisterMetaType< QFileInfo >();
    qRegisterMetaType<V2ProgramGuide*>("V2ProgramGuide");
    qRegisterMetaType<V2ProgramList*>("V2ProgramList");
    qRegisterMetaType<V2Program*>("V2Program");
    qRegisterMetaType<V2ChannelGroupList*>("V2ChannelGroupList");
    qRegisterMetaType<V2ChannelGroup*>("V2ChannelGroup");
    qRegisterMetaType<V2ChannelInfo*>("V2ChannelInfo");
    qRegisterMetaType<V2RecordingInfo*>("V2RecordingInfo");
    qRegisterMetaType<V2ArtworkInfoList*>("V2ArtworkInfoList");
    qRegisterMetaType<V2ArtworkInfo*>("V2ArtworkInfo");
    qRegisterMetaType<V2CastMemberList*>("V2CastMemberList");
    qRegisterMetaType<V2CastMember*>("V2CastMember");
}

V2Guide::V2Guide() : MythHTTPService(s_service)
{
}

V2ProgramGuide *V2Guide::GetProgramGuide( const QDateTime &rawStartTime,
                                           const QDateTime &rawEndTime,
                                           bool             bDetails,
                                           int              nChannelGroupId,
                                           int              nStartIndex,
                                           int              nCount,
                                           bool             bWithInvisible)
{
    if (!rawStartTime.isValid())
        throw QString( "StartTime is invalid" );

    if (!rawEndTime.isValid())
        throw QString( "EndTime is invalid" );

    QDateTime dtStartTime = rawStartTime.toUTC();
    QDateTime dtEndTime = rawEndTime.toUTC();

    if (dtEndTime < dtStartTime)
        throw QString( "EndTime is before StartTime");

    nStartIndex = std::max(nStartIndex, 0);

    if (nCount <= 0)
        nCount = 20000;

    // ----------------------------------------------------------------------
    // Load the channel list
    // ----------------------------------------------------------------------

    uint nTotalAvailable = 0;
    ChannelInfoList chanList = ChannelUtil::LoadChannels(nStartIndex, nCount,
                                                         nTotalAvailable,
                                                         !bWithInvisible,
                                                         ChannelUtil::kChanOrderByChanNum,
                                                         ChannelUtil::kChanGroupByCallsignAndChannum,
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

#if 0
    QString sGroupBy = "program.starttime, channel.channum,"
                       "channel.callsign, program.title";
#endif

    QString sOrderBy = "program.starttime";

    bindings[":STARTDATE"     ] = dtStartTime;
    bindings[":STARTDATELIMIT"] = dtStartTime.addDays(-1);
    bindings[":ENDDATE"       ] = dtEndTime;

    // ----------------------------------------------------------------------
    // Get all Pending Scheduled Programs
    // ----------------------------------------------------------------------

    // NOTE: Fetching this information directly from the schedule is
    //       significantly faster than using ProgramInfo::LoadFromScheduler()
    auto *scheduler = dynamic_cast<Scheduler*>(gCoreContext->GetScheduler());
    if (scheduler)
        scheduler->GetAllPending(schedList);

    // ----------------------------------------------------------------------
    // Build Response
    // ----------------------------------------------------------------------

    auto *pGuide = new V2ProgramGuide();

    ChannelInfoList::iterator chan_it;
    for (chan_it = chanList.begin(); chan_it != chanList.end(); ++chan_it)
    {
        // Create ChannelInfo Object
        V2ChannelInfo *pChannel = pGuide->AddNewChannel();
        V2FillChannelInfo( pChannel, (*chan_it), bDetails );

        // Load the list of programmes for this channel
        ProgramList  progList;
        bindings[":CHANID"] = (*chan_it).m_chanId;
        LoadFromProgram( progList, sWhere, sOrderBy, sOrderBy, bindings,
                         schedList );

        // Create Program objects and add them to the channel object
        ProgramList::iterator progIt;
        for( progIt = progList.begin(); progIt != progList.end(); ++progIt)
        {
            V2Program *pProgram = pChannel->AddNewProgram();
            V2FillProgramInfo( pProgram, *progIt, false, bDetails, false ); // No cast info
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

V2ProgramList* V2Guide::GetProgramList(int              nStartIndex,
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
                                        bool             bDescending,
                                        bool             bWithInvisible,
                                        const QString& sCatType,
                                        const QString& sGroupBy)
{
    if (!rawStartTime.isNull() && !rawStartTime.isValid())
        throw QString( "StartTime is invalid" );

    if (!rawEndTime.isNull() && !rawEndTime.isValid())
        throw QString( "EndTime is invalid" );

    QDateTime dtStartTime = rawStartTime;
    const QDateTime& dtEndTime = rawEndTime;

    if (!rawEndTime.isNull() && dtEndTime < dtStartTime)
        throw QString( "EndTime is before StartTime");

    ProgGroupBy::Type nGroupBy = ProgGroupBy::ChanNum;
    if (!sGroupBy.isEmpty())
    {
        // Handle ProgGroupBy enum name
        auto meta = QMetaEnum::fromType<ProgGroupBy::Type>();
        bool ok = false;
        nGroupBy = ProgGroupBy::Type(meta.keyToValue(sGroupBy.toLocal8Bit(), &ok));
        if (!ok)
            throw QString( "GroupBy is invalid" );
    }

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
    {
        sSQL = "WHERE ";
    }

    if (bOnlyNew)
    {
        sSQL = "LEFT JOIN oldprogram ON oldprogram.oldtitle = program.title "
                + sSQL
                + "oldprogram.oldtitle IS NULL AND ";
    }

    sSQL += "deleted IS NULL AND ";

    if (!bWithInvisible)
        sSQL += "visible > 0 AND ";

    sSQL += "program.manualid = 0 "; // Exclude programmes created purely for 'manual' recording schedules

    nChanId = std::max(nChanId, 0);

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

    if (!sCatType.isEmpty())
    {
        sSQL += "AND program.category_type LIKE :CatType ";
        bindings[":CatType"] = sCatType;
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
        sSQL += "ORDER BY program.starttime ";        // NOLINT(bugprone-branch-clone)
    else if (sSort == "title")
        sSQL += "ORDER BY program.title ";
    else if (sSort == "channel")
        sSQL += "ORDER BY channel.channum ";
    else if (sSort == "duration")
        sSQL += "ORDER BY (program.endtime - program.starttime) ";
    else
        sSQL += "ORDER BY program.starttime, channel.channum + 0 ";

    if (bDescending)
        sSQL += "DESC ";
    else
        sSQL += "ASC ";

    // ----------------------------------------------------------------------
    // Get all Pending Scheduled Programs
    // ----------------------------------------------------------------------

    // NOTE: Fetching this information directly from the schedule is
    //       significantly faster than using ProgramInfo::LoadFromScheduler()
    auto *scheduler = dynamic_cast<Scheduler*>(gCoreContext->GetScheduler());
    if (scheduler)
        scheduler->GetAllPending(schedList);

    // ----------------------------------------------------------------------

    uint nTotalAvailable = 0;
    LoadFromProgram( progList, sSQL, bindings, schedList,
                     (uint)nStartIndex, (uint)nCount, nTotalAvailable,
                     nGroupBy);

    // ----------------------------------------------------------------------
    // Build Response
    // ----------------------------------------------------------------------

    auto *pPrograms = new V2ProgramList();

    nCount        = (int)progList.size();
    int nEndIndex = (int)progList.size();

    for( int n = 0; n < nEndIndex; n++)
    {
        ProgramInfo *pInfo = progList[ n ];

        if (bOnlyNew)
        {
            // Eliminate duplicate titles
            bool duplicate = false;
            for (int back = n-1 ; back >= 0 ; back--) {
                auto *prior = progList[back];
                if (prior != nullptr) {
                    if (pInfo -> GetTitle() == prior -> GetTitle()) {
                        duplicate = true;
                        break;
                    }
                }
            }
            if (duplicate)
                continue;
        }

        V2Program *pProgram = pPrograms->AddNewProgram();

        V2FillProgramInfo( pProgram, pInfo, true, bDetails, false ); // No cast info, loading this takes far too long
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

V2Program* V2Guide::GetProgramDetails( int              nChanId,
                                        const QDateTime &rawStartTime )

{
    if (!(nChanId > 0))
        throw QString( "Channel ID is invalid" );
    if (!rawStartTime.isValid())
        throw QString( "StartTime is invalid" );

    QDateTime dtStartTime = rawStartTime.toUTC();

    // ----------------------------------------------------------------------
    // -=>TODO: Add support for getting Recorded Program Info
    // ----------------------------------------------------------------------

    // Build Response

    auto *pProgram = new V2Program();
    ProgramInfo  *pInfo    = LoadProgramFromProgram(nChanId, dtStartTime);

    V2FillProgramInfo( pProgram, pInfo, true, true, true );

    delete pInfo;

    return pProgram;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

QFileInfo V2Guide::GetChannelIcon( int nChanId,
                                 int nWidth  /* = 0 */,
                                 int nHeight /* = 0 */,
                                 const QString &FileName )
{

    QString sFileName;
    // Get Icon file path

    if (FileName.isEmpty())
        sFileName = ChannelUtil::GetIcon( nChanId );
    else
        sFileName = FileName;

    if (sFileName.isEmpty())
    {
        LOG(VB_UPNP, LOG_ERR,
            QString("GetImageFile - ChanId %1 doesn't exist or isn't visible")
                    .arg(nChanId));
        return {};
    }

    // ------------------------------------------------------------------
    // Search for the filename
    // ------------------------------------------------------------------

    StorageGroup storage( "ChannelIcons" );
    QString sFullFileName = storage.FindFile( sFileName );

    if (sFullFileName.isEmpty())
    {
        LOG(VB_UPNP, LOG_ERR,
            QString("GetImageFile - Unable to find %1.").arg(sFileName));

        return {};
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

        return {};
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

    QString sChannelsDirectory = QFileInfo( sNewFileName ).absolutePath();

    if (!QFileInfo( sChannelsDirectory ).isWritable())
    {
        LOG(VB_UPNP, LOG_ERR, QString("GetImageFile - no write access to: %1")
            .arg( sChannelsDirectory ));
        return {};
    }

    auto *pImage = new QImage( sFullFileName );

    if (!pImage)
    {
        LOG(VB_UPNP, LOG_ERR, QString("GetImageFile - can't create image: %1")
            .arg( sFullFileName ));
        return {};
    }

    float fAspect = (float)(pImage->width()) / pImage->height();
    if (fAspect == 0)
    {
        LOG(VB_UPNP, LOG_ERR, QString("GetImageFile - zero aspect"));
        delete pImage;
        return {};
    }

    if ( nWidth == 0 )
        nWidth = (int)std::rint(nHeight * fAspect);

    if ( nHeight == 0 )
        nHeight = (int)std::rint(nWidth / fAspect);

    QImage img = pImage->scaled( nWidth, nHeight, Qt::IgnoreAspectRatio,
                                Qt::SmoothTransformation);

    if (img.isNull())
    {
        LOG(VB_UPNP, LOG_ERR, QString("SaveImageFile - unable to scale. "
            "See if %1 is really an image.").arg( sFullFileName ));
        delete pImage;
        return {};
    }

    if (!img.save( sNewFileName, "PNG" ))
    {
        LOG(VB_UPNP, LOG_ERR, QString("SaveImageFile - failed, %1")
            .arg( sNewFileName ));
        delete pImage;
        return {};
    }

    delete pImage;

    return QFileInfo( sNewFileName );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

V2ChannelGroupList* V2Guide::GetChannelGroupList( bool bIncludeEmpty )
{
    ChannelGroupList list = ChannelGroup::GetChannelGroups(bIncludeEmpty);
    auto *pGroupList = new V2ChannelGroupList();

    ChannelGroupList::iterator it;
    for (it = list.begin(); it < list.end(); ++it)
    {
        V2ChannelGroup *pGroup = pGroupList->AddNewChannelGroup();
        V2FillChannelGroup(pGroup, (*it));
    }

    return pGroupList;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

QStringList V2Guide::GetCategoryList( ) //int nStartIndex, int nCount)
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

QStringList V2Guide::GetStoredSearches( const QString& sType )
{
    QStringList keywordList;
    MSqlQuery query(MSqlQuery::InitCon());

    RecSearchType iType = searchTypeFromString(sType);

    if (iType == kNoSearch)
    {
        //throw QString( "Invalid Type" );
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

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

bool V2Guide::AddToChannelGroup   ( int nChannelGroupId,
                                  int nChanId )
{
    bool bResult = false;

    if (!(nChanId > 0))
        throw QString( "Channel ID is invalid" );

    bResult = ChannelGroup::AddChannel(nChanId, nChannelGroupId);

    return bResult;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

bool V2Guide::RemoveFromChannelGroup ( int nChannelGroupId,
                                     int nChanId )
{
    bool bResult = false;

    if (!(nChanId > 0))
        throw QString( "Channel ID is invalid" );

    bResult = ChannelGroup::DeleteChannel(nChanId, nChannelGroupId);

    return bResult;
}

int V2Guide::AddChannelGroup       ( const QString &Name)
{
    return ChannelGroup::AddChannelGroup(Name);
}

bool V2Guide::RemoveChannelGroup  ( const QString &Name )
{
    return ChannelGroup::RemoveChannelGroup(Name);
}

bool V2Guide::UpdateChannelGroup  ( const QString & oldName, const QString & newName)
{
    return ChannelGroup::UpdateChannelGroup(oldName, newName);
}


// NOLINTEND(modernize-return-braced-init-list)
