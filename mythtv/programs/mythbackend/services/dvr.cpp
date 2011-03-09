//////////////////////////////////////////////////////////////////////////////
// Program Name: dvr.cpp
// Created     : Mar. 7, 2011
//
// Copyright (c) 2011 David Blain <dblain@mythtv.org>
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

#include <QMap>

#include "dvr.h"

#include "compat.h"
#include "mythversion.h"
#include "mythcorecontext.h"
#include "scheduler.h"
#include "autoexpire.h"
#include "jobqueue.h"
#include "encoderlink.h"

#include "serviceUtil.h"

extern QMap<int, EncoderLink *> tvList;
extern AutoExpire  *expirer;
extern Scheduler   *sched;

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

DTC::ProgramList* Dvr::GetRecorded( bool bDescending,
                                    int  nStartIndex,
                                    int  nCount      )
{
    QMap< QString, ProgramInfo* > recMap;

    if (sched)
        recMap = sched->GetRecording();

    QMap< QString, uint32_t > inUseMap    = ProgramInfo::QueryInUseMap();
    QMap< QString, bool >     isJobRunning= ProgramInfo::QueryJobsRunning(JOB_COMMFLAG);

    ProgramList progList;

    LoadFromRecorded( progList, false, inUseMap, isJobRunning, recMap );

    QMap< QString, ProgramInfo* >::iterator mit = recMap.begin();

    for (; mit != recMap.end(); mit = recMap.erase(mit))
        delete *mit;

    // ----------------------------------------------------------------------
    // Build Response
    // ----------------------------------------------------------------------

    DTC::ProgramList *pPrograms = new DTC::ProgramList();

    nStartIndex = min( nStartIndex, (int)progList.size() );
    nCount      = (nCount > 0) ? min( nCount, (int)progList.size() ) : progList.size();

    for( int n = nStartIndex; n < nCount; n++)
    {
        ProgramInfo *pInfo = progList[ n ];

        DTC::Program *pProgram = pPrograms->AddNewProgram();

        FillProgramInfo( pProgram, pInfo, true );
    }

    // ----------------------------------------------------------------------

    pPrograms->setStartIndex    ( nStartIndex     );
    pPrograms->setCount         ( nCount          );
    pPrograms->setTotalAvailable( progList.size() );
    pPrograms->setAsOf          ( QDateTime::currentDateTime() );
    pPrograms->setVersion       ( MYTH_BINARY_VERSION );
    pPrograms->setProtoVer      ( MYTH_PROTO_VERSION  );

    return pPrograms;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

DTC::ProgramList* Dvr::GetExpiring( int nStartIndex, 
                                    int nCount      )
{
    pginfolist_t  infoList;

    if (expirer)
        expirer->GetAllExpiring( infoList );

    // ----------------------------------------------------------------------
    // Build Response
    // ----------------------------------------------------------------------

    DTC::ProgramList *pPrograms = new DTC::ProgramList();

    nStartIndex = min( nStartIndex, (int)infoList.size() );
    nCount      = (nCount > 0) ? min( nCount, (int)infoList.size() ) : infoList.size();

    for( int n = nStartIndex; n < nCount; n++)
    {
        ProgramInfo *pInfo = infoList[ n ];

        if (pInfo != NULL)
        {
            DTC::Program *pProgram = pPrograms->AddNewProgram();

            FillProgramInfo( pProgram, pInfo, true );

            delete pInfo;
        }
    }

    // ----------------------------------------------------------------------

    pPrograms->setStartIndex    ( nStartIndex     );
    pPrograms->setCount         ( nCount          );
    pPrograms->setTotalAvailable( infoList.size() );
    pPrograms->setAsOf          ( QDateTime::currentDateTime() );
    pPrograms->setVersion       ( MYTH_BINARY_VERSION );
    pPrograms->setProtoVer      ( MYTH_PROTO_VERSION  );

    return pPrograms;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

DTC::EncoderList* Dvr::Encoders()
{
    DTC::EncoderList* pList = new DTC::EncoderList();

    int  numencoders = 0;
    bool isLocal     = true;

    QMap<int, EncoderLink *>::Iterator iter = tvList.begin();

    for (; iter != tvList.end(); ++iter)
    {
        EncoderLink *elink = *iter;

        if (elink != NULL)
        {
            DTC::Encoder *pEncoder = pList->AddNewEncoder();
            
            pEncoder->setId            ( elink->GetCardID()       );
            pEncoder->setState         ( elink->GetState()        );
            pEncoder->setLocal         ( elink->IsLocal()         );
            pEncoder->setConnected     ( elink->IsConnected()     );
            pEncoder->setSleepStatus   ( elink->GetSleepStatus()  );
          //  pEncoder->setLowOnFreeSpace( elink->isLowOnFreeSpace());

            if (pEncoder->Local())
                pEncoder->setHostName( gCoreContext->GetHostName() );
            else
                pEncoder->setHostName( elink->GetHostName() );

            switch ( pEncoder->State() )
            {
                case kState_WatchingLiveTV:
                case kState_RecordingOnly:
                case kState_WatchingRecording:
                {
                    ProgramInfo  *pInfo = elink->GetRecording();

                    if (pInfo)
                    {
                        DTC::Program *pProgram = pEncoder->Recording();

                        FillProgramInfo( pProgram, pInfo, false, true );

                        delete pInfo;
                    }

                    break;
                }

                default:
                    break;
            }
        }
    }
    return pList;
}




/*

    // Add upcoming shows

    QDomElement scheduled = pDoc->createElement("Scheduled");
    root.appendChild(scheduled);

    RecList recordingList;

    if (m_pSched)
        m_pSched->getAllPending(&recordingList);

    unsigned int iNum = 10;
    unsigned int iNumRecordings = 0;

    RecConstIter itProg = recordingList.begin();
    for (; (itProg != recordingList.end()) && iNumRecordings < iNum; ++itProg)
    {
        if (((*itProg)->GetRecordingStatus() <= rsWillRecord) &&
            ((*itProg)->GetRecordingStartTime() >=
             QDateTime::currentDateTime()))
        {
            iNumRecordings++;
            MythXML::FillProgramInfo(pDoc, scheduled, *itProg);
        }
    }

    while (!recordingList.empty())
    {
        ProgramInfo *pginfo = recordingList.back();
        delete pginfo;
        recordingList.pop_back();
    }

    scheduled.setAttribute("count", iNumRecordings);






=================================================
    // Add Job Queue Entries

    QDomElement jobqueue = pDoc->createElement("JobQueue");
    root.appendChild(jobqueue);

    QMap<int, JobQueueEntry> jobs;
    QMap<int, JobQueueEntry>::Iterator it;

    JobQueue::GetJobsInQueue(jobs,
                             JOB_LIST_NOT_DONE | JOB_LIST_ERROR |
                             JOB_LIST_RECENT);

    for (it = jobs.begin(); it != jobs.end(); ++it)
    {
        ProgramInfo pginfo((*it).chanid, (*it).recstartts);
        if (!pginfo.GetChanID())
            continue;

        QDomElement job = pDoc->createElement("Job");
        jobqueue.appendChild(job);

        job.setAttribute("id"        , (*it).id         );
        job.setAttribute("chanId"    , (*it).chanid     );
        job.setAttribute("startTime" ,
                         (*it).recstartts.toString(Qt::ISODate));
        job.setAttribute("startTs"   , (*it).startts    );
        job.setAttribute("insertTime",
                         (*it).inserttime.toString(Qt::ISODate));
        job.setAttribute("type"      , (*it).type       );
        job.setAttribute("cmds"      , (*it).cmds       );
        job.setAttribute("flags"     , (*it).flags      );
        job.setAttribute("status"    , (*it).status     );
        job.setAttribute("statusTime",
                         (*it).statustime.toString(Qt::ISODate));
        job.setAttribute("schedTime" ,
                         (*it).schedruntime.toString(Qt::ISODate));
        job.setAttribute("args"      , (*it).args       );

        if ((*it).hostname.isEmpty())
            job.setAttribute("hostname", QObject::tr("master"));
        else
            job.setAttribute("hostname",(*it).hostname);

        QDomText textNode = pDoc->createTextNode((*it).comment);
        job.appendChild(textNode);

        MythXML::FillProgramInfo(pDoc, job, &pginfo);
    }

    jobqueue.setAttribute( "count", jobs.size() );




=================================================
    // Add Machine information


    QDomElement mInfo   = pDoc->createElement("MachineInfo");
    QDomElement storage = pDoc->createElement("Storage"    );
    QDomElement load    = pDoc->createElement("Load"       );
    QDomElement guide   = pDoc->createElement("Guide"      );

    root.appendChild (mInfo  );
    mInfo.appendChild(storage);
    mInfo.appendChild(load   );
    mInfo.appendChild(guide  );

    // drive space   ---------------------

    QStringList strlist;
    QString dirs;
    QString hostname;
    QString directory;
    QString isLocalstr;
    QString fsID;
    QString ids;
    long long iTotal = -1, iUsed = -1, iAvail = -1;

    if (m_pMainServer)
        m_pMainServer->BackendQueryDiskSpace(strlist, true, m_bIsMaster);

    QDomElement total;

    // Make a temporary list to hold the per-filesystem elements so that the
    // total is always the first element.
    QList<QDomElement> fsXML;
    QStringList::const_iterator sit = strlist.begin();
    while (sit != strlist.end())
    {
        hostname   = *(sit++);
        directory  = *(sit++);
        isLocalstr = *(sit++);
        fsID       = *(sit++);
        sit++; // ignore dirID
        sit++; // ignore blocksize
        iTotal     = decodeLongLong(strlist, sit);
        iUsed      = decodeLongLong(strlist, sit);
        iAvail     = iTotal - iUsed;

        if (fsID == "-2")
            fsID = "total";

        QDomElement group = pDoc->createElement("Group");

        group.setAttribute("id"   , fsID );
        group.setAttribute("total", (int)(iTotal>>10) );
        group.setAttribute("used" , (int)(iUsed>>10)  );
        group.setAttribute("free" , (int)(iAvail>>10) );
        group.setAttribute("dir"  , directory );

        if (fsID == "total")
        {
            long long iLiveTV = -1, iDeleted = -1, iExpirable = -1;
            MSqlQuery query(MSqlQuery::InitCon());
            query.prepare("SELECT SUM(filesize) FROM recorded "
                          " WHERE recgroup = :RECGROUP;");

            query.bindValue(":RECGROUP", "LiveTV");
            if (query.exec() && query.next())
            {
                iLiveTV = query.value(0).toLongLong();
            }
            query.bindValue(":RECGROUP", "Deleted");
            if (query.exec() && query.next())
            {
                iDeleted = query.value(0).toLongLong();
            }
            query.prepare("SELECT SUM(filesize) FROM recorded "
                          " WHERE autoexpire = 1 "
                          "   AND recgroup NOT IN ('LiveTV', 'Deleted');");
            if (query.exec() && query.next())
            {
                iExpirable = query.value(0).toLongLong();
            }
            group.setAttribute("livetv", (int)(iLiveTV>>20) );
            group.setAttribute("deleted", (int)(iDeleted>>20) );
            group.setAttribute("expirable", (int)(iExpirable>>20) );
            total = group;
        }
        else
            fsXML << group;
    }

    storage.appendChild(total);
    int num_elements = fsXML.size();
    for (int fs_index = 0; fs_index < num_elements; fs_index++)
    {
            storage.appendChild(fsXML[fs_index]);
    }

    // load average ---------------------

    double rgdAverages[3];

    if (getloadavg(rgdAverages, 3) != -1)
    {
        load.setAttribute("avg1", rgdAverages[0]);
        load.setAttribute("avg2", rgdAverages[1]);
        load.setAttribute("avg3", rgdAverages[2]);
    }

=================================================

    // Guide Data ---------------------

    QDateTime GuideDataThrough;

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT MAX(endtime) FROM program WHERE manualid = 0;");

    if (query.exec() && query.isActive() && query.size())
    {
        query.next();

        if (query.isValid())
            GuideDataThrough = QDateTime::fromString(query.value(0).toString(),
                                                     Qt::ISODate);
    }

    guide.setAttribute("start", gCoreContext->GetSetting("mythfilldatabaseLastRunStart"));
    guide.setAttribute("end", gCoreContext->GetSetting("mythfilldatabaseLastRunEnd"));
    guide.setAttribute("status", gCoreContext->GetSetting("mythfilldatabaseLastRunStatus"));
    if (gCoreContext->GetNumSetting("MythFillGrabberSuggestsTime", 0))
    {
        guide.setAttribute("next",
            gCoreContext->GetSetting("MythFillSuggestedRunTime"));
    }

    if (!GuideDataThrough.isNull())
    {
        guide.setAttribute("guideThru", QDateTime(GuideDataThrough).toString(Qt::ISODate));
        guide.setAttribute("guideDays", qdtNow.daysTo(GuideDataThrough));
    }

    QDomText dataDirectMessage = pDoc->createTextNode(gCoreContext->GetSetting("DataDirectMessage"));
    guide.appendChild(dataDirectMessage);

    // Add Miscellaneous information

    QString info_script = gCoreContext->GetSetting("MiscStatusScript");
    if ((!info_script.isEmpty()) && (info_script != "none"))
    {
        QDomElement misc = pDoc->createElement("Miscellaneous");
        root.appendChild(misc);

        uint flags = kMSRunShell | kMSStdOut | kMSBuffered;
        MythSystem ms(info_script, flags);
        ms.Run(10);
        if (ms.Wait() != GENERIC_EXIT_OK)
        {
            VERBOSE(VB_IMPORTANT, QString("Error running miscellaneous "
                    "status information script: %1").arg(info_script));
            return;
        }
    
        QByteArray input = ms.ReadAll();

        QStringList output = QString(input).split('\n', 
                                                  QString::SkipEmptyParts);

        QStringList::iterator iter;
        for (iter = output.begin(); iter != output.end(); ++iter)
        {
            QDomElement info = pDoc->createElement("Information");

            QStringList list = (*iter).split("[]:[]");
            unsigned int size = list.size();
            unsigned int hasAttributes = 0;

            if ((size > 0) && (!list[0].isEmpty()))
            {
                info.setAttribute("display", list[0]);
                hasAttributes++;
            }
            if ((size > 1) && (!list[1].isEmpty()))
            {
                info.setAttribute("name", list[1]);
                hasAttributes++;
            }
            if ((size > 2) && (!list[2].isEmpty()))
            {
                info.setAttribute("value", list[2]);
                hasAttributes++;
            }

            if (hasAttributes > 0)
                misc.appendChild(info);
        }
    }
*/