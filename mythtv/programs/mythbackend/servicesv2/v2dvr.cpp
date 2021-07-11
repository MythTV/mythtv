//////////////////////////////////////////////////////////////////////////////
// Program Name: v2dvr.cpp
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

#include "v2dvr.h"
#include "libmythbase/http/mythhttpmetaservice.h"
#include "autoexpire.h"
#include "programinfo.h"
#include "mythversion.h"
#include "mythcorecontext.h"
#include "mythscheduler.h"
#include "jobqueue.h"
#include "v2serviceUtil.h"

extern QMap<int, EncoderLink *> tvList;
extern AutoExpire  *expirer;

// This will be initialised in a thread safe manner on first use
Q_GLOBAL_STATIC_WITH_ARGS(MythHTTPMetaService, s_service,
    (DVR_HANDLE, V2Dvr::staticMetaObject, std::bind(&V2Dvr::RegisterCustomTypes)))

void V2Dvr::RegisterCustomTypes()
{
    qRegisterMetaType<V2ProgramList*>("V2ProgramList");
    qRegisterMetaType<V2Program*>("V2Program");
}

V2Dvr::V2Dvr()
  : MythHTTPService(s_service)
{
}

V2ProgramList* V2Dvr::GetExpiringList( int nStartIndex,
                                        int nCount      )
{
    pginfolist_t  infoList;

    if (expirer)
        expirer->GetAllExpiring( infoList );

    // ----------------------------------------------------------------------
    // Build Response
    // ----------------------------------------------------------------------

    auto *pPrograms = new V2ProgramList();

    nStartIndex   = (nStartIndex > 0) ? std::min( nStartIndex, (int)infoList.size() ) : 0;
    nCount        = (nCount > 0) ? std::min( nCount, (int)infoList.size() ) : infoList.size();
    int nEndIndex = std::min((nStartIndex + nCount), (int)infoList.size() );

    for( int n = nStartIndex; n < nEndIndex; n++)
    {
        ProgramInfo *pInfo = infoList[ n ];

        if (pInfo != nullptr)
        {
            V2Program *pProgram = pPrograms->AddNewProgram();

            V2FillProgramInfo( pProgram, pInfo, true );

            delete pInfo;
        }
    }

    // ----------------------------------------------------------------------

    pPrograms->setStartIndex    ( nStartIndex     );
    pPrograms->setCount         ( nCount          );
    pPrograms->setTotalAvailable( infoList.size() );
    pPrograms->setAsOf          ( MythDate::current() );
    pPrograms->setVersion       ( MYTH_BINARY_VERSION );
    pPrograms->setProtoVer      ( MYTH_PROTO_VERSION  );

    return pPrograms;
}

V2ProgramList* V2Dvr::GetRecordedList( bool           bDescending,
                                        int            nStartIndex,
                                        int            nCount,
                                        const QString &sTitleRegEx,
                                        const QString &sRecGroup,
                                        const QString &sStorageGroup,
                                        const QString &sCategory,
                                        const QString &sSort
                                      )
{
    QMap< QString, ProgramInfo* > recMap;

    if (gCoreContext->GetScheduler())
        recMap = gCoreContext->GetScheduler()->GetRecording();

    QMap< QString, uint32_t > inUseMap    = ProgramInfo::QueryInUseMap();
    QMap< QString, bool >     isJobRunning= ProgramInfo::QueryJobsRunning(JOB_COMMFLAG);

    ProgramList progList;

    int desc = 1;
    if (bDescending)
        desc = -1;

    LoadFromRecorded( progList, false, inUseMap, isJobRunning, recMap, desc, sSort );

    QMap< QString, ProgramInfo* >::iterator mit = recMap.begin();

    for (; mit != recMap.end(); mit = recMap.erase(mit))
        delete *mit;

    // ----------------------------------------------------------------------
    // Build Response
    // ----------------------------------------------------------------------

    auto *pPrograms = new V2ProgramList();
    int nAvailable = 0;

    int nMax      = (nCount > 0) ? nCount : progList.size();

    nAvailable = 0;
    nCount = 0;

    QRegularExpression rTitleRegEx
        { sTitleRegEx, QRegularExpression::CaseInsensitiveOption };

    for (auto *pInfo : progList)
    {
        if (pInfo->IsDeletePending() ||
            (!sTitleRegEx.isEmpty() && !pInfo->GetTitle().contains(rTitleRegEx)) ||
            (!sRecGroup.isEmpty() && sRecGroup != pInfo->GetRecordingGroup()) ||
            (!sStorageGroup.isEmpty() && sStorageGroup != pInfo->GetStorageGroup()) ||
            (!sCategory.isEmpty() && sCategory != pInfo->GetCategory()))
            continue;

        if ((nAvailable < nStartIndex) ||
            (nCount >= nMax))
        {
            ++nAvailable;
            continue;
        }

        ++nAvailable;
        ++nCount;

        V2Program *pProgram = pPrograms->AddNewProgram();

        V2FillProgramInfo( pProgram, pInfo, true );
    }

    // ----------------------------------------------------------------------

    pPrograms->setStartIndex    ( nStartIndex     );
    pPrograms->setCount         ( nCount          );
    pPrograms->setTotalAvailable( nAvailable      );
    pPrograms->setAsOf          ( MythDate::current() );
    pPrograms->setVersion       ( MYTH_BINARY_VERSION );
    pPrograms->setProtoVer      ( MYTH_PROTO_VERSION  );

    return pPrograms;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

V2ProgramList* V2Dvr::GetOldRecordedList( bool             bDescending,
                                           int              nStartIndex,
                                           int              nCount,
                                           const QDateTime &sStartTime,
                                           const QDateTime &sEndTime,
                                           const QString   &sTitle,
                                           const QString   &sSeriesId,
                                           int              nRecordId,
                                           const QString   &sSort)
{
    if (!sStartTime.isNull() && !sStartTime.isValid())
        throw QString("StartTime is invalid");

    if (!sEndTime.isNull() && !sEndTime.isValid())
        throw QString("EndTime is invalid");

    const QDateTime& dtStartTime = sStartTime;
    const QDateTime& dtEndTime   = sEndTime;

    if (!sEndTime.isNull() && dtEndTime < dtStartTime)
        throw QString("EndTime is before StartTime");

    // ----------------------------------------------------------------------
    // Build SQL statement for Program Listing
    // ----------------------------------------------------------------------

    ProgramList  progList;
    MSqlBindings bindings;
    QString      sSQL;

    if (!dtStartTime.isNull())
    {
        sSQL += " AND endtime >= :StartDate ";
        bindings[":StartDate"] = dtStartTime;
    }

    if (!dtEndTime.isNull())
    {
        sSQL += " AND starttime <= :EndDate ";
        bindings[":EndDate"] = dtEndTime;
    }

    QStringList clause;

    if (nRecordId > 0)
    {
        clause << "recordid = :RecordId";
        bindings[":RecordId"] = nRecordId;
    }

    if (!sTitle.isEmpty())
    {
        clause << "title = :Title";
        bindings[":Title"] = sTitle;
    }

    if (!sSeriesId.isEmpty())
    {
        clause << "seriesid = :SeriesId";
        bindings[":SeriesId"] = sSeriesId;
    }

    if (!clause.isEmpty())
    {
        sSQL += QString(" AND (%1) ").arg(clause.join(" OR "));
    }

    if (sSort == "starttime")
        sSQL += "ORDER BY starttime ";    // NOLINT(bugprone-branch-clone)
    else if (sSort == "title")
        sSQL += "ORDER BY title ";
    else
        sSQL += "ORDER BY starttime ";

    if (bDescending)
        sSQL += "DESC ";
    else
        sSQL += "ASC ";

    uint nTotalAvailable = (nStartIndex == 0) ? 1 : 0;
    LoadFromOldRecorded( progList, sSQL, bindings,
                         (uint)nStartIndex, (uint)nCount, nTotalAvailable );

    // ----------------------------------------------------------------------
    // Build Response
    // ----------------------------------------------------------------------

    auto *pPrograms = new V2ProgramList();

    nCount        = (int)progList.size();
    int nEndIndex = (int)progList.size();

    for( int n = 0; n < nEndIndex; n++)
    {
        ProgramInfo *pInfo = progList[ n ];

        V2Program *pProgram = pPrograms->AddNewProgram();

        V2FillProgramInfo( pProgram, pInfo, true );
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

V2Program* V2Dvr::GetRecorded(int RecordedId,
                               int chanid, const QDateTime &recstarttsRaw)
{
    if ((RecordedId <= 0) &&
        (chanid <= 0 || !recstarttsRaw.isValid()))
        throw QString("Recorded ID or Channel ID and StartTime appears invalid.");

    // TODO Should use RecordingInfo
    ProgramInfo pi;
    if (RecordedId > 0)
        pi = ProgramInfo(RecordedId);
    else
        pi = ProgramInfo(chanid, recstarttsRaw.toUTC());

    auto *pProgram = new V2Program();
    V2FillProgramInfo( pProgram, &pi, true );

    return pProgram;
}
