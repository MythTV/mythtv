//////////////////////////////////////////////////////////////////////////////
// Program Name: httpstatus.cpp
//
// Purpose - Html & XML status HttpServerExtension
//
// Created By  : David Blain                    Created On : Oct. 24, 2005
// Modified By :                                Modified On:
//
//////////////////////////////////////////////////////////////////////////////

// POSIX headers
#include <unistd.h>

// ANSI C headers
#include <cmath>
#include <cstdio>
#include <cstdlib>

// Qt headers
#include <QtGlobal>
#if QT_VERSION >= QT_VERSION_CHECK(6,0,0)
#include <QStringConverter>
#endif
#include <QTextStream>

// MythTV headers
#include "libmythbase/compat.h"
#include "libmythbase/exitcodes.h"
#include "libmythbase/mythconfig.h"
#include "libmythbase/mythcorecontext.h"
#include "libmythbase/mythdate.h"
#include "libmythbase/mythdbcon.h"
#include "libmythbase/mythlogging.h"
#include "libmythbase/mythmiscutil.h"
#include "libmythbase/mythsystemlegacy.h"
#include "libmythbase/mythversion.h"
#include "libmythtv/cardutil.h"
#include "libmythtv/jobqueue.h"
#include "libmythtv/tv.h"
#include "libmythtv/tv_rec.h"
#include "libmythupnp/ssdp.h"
#include "libmythupnp/ssdpcache.h"
#include "libmythupnp/upnp.h"

// MythBackend
#include "autoexpire.h"
#include "encoderlink.h"
#include "httpstatus.h"
#include "mainserver.h"
#include "scheduler.h"

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

HttpStatus::HttpStatus( QMap<int, EncoderLink *> *tvList, Scheduler *sched,
                        bool bIsMaster )
          : HttpServerExtension( "HttpStatus" , QString()),
            m_pSched(sched),
            m_pEncoders(tvList),
            m_bIsMaster(bIsMaster)
{
    m_nPreRollSeconds = gCoreContext->GetNumSetting("RecordPreRoll", 0);
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

HttpStatusMethod HttpStatus::GetMethod( const QString &sURI )
{
    if (sURI == "Status"               ) return( HSM_GetStatusHTML   );
    if (sURI == "GetStatusHTML"        ) return( HSM_GetStatusHTML   );
    if (sURI == "GetStatus"            ) return( HSM_GetStatusXML    );
    if (sURI == "xml"                  ) return( HSM_GetStatusXML    );

    return( HSM_Unknown );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

QStringList HttpStatus::GetBasePaths()
{
    return QStringList( "/Status" );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

bool HttpStatus::ProcessRequest( HTTPRequest *pRequest )
{
    try
    {
        if (pRequest)
        {
            if ((pRequest->m_sBaseUrl     != "/Status" ) &&
                (pRequest->m_sResourceUrl != "/Status" ))
            {
                return( false );
            }

            switch( GetMethod( pRequest->m_sMethod ))
            {
                case HSM_GetStatusXML   : GetStatusXML   ( pRequest ); return true;
                case HSM_GetStatusHTML  : GetStatusHTML  ( pRequest ); return true;

                default:
                {
                    pRequest->m_eResponseType   = ResponseTypeHTML;
                    pRequest->m_nResponseStatus = 200;

                    break;
                }
            }
        }
    }
    catch( ... )
    {
        LOG(VB_GENERAL, LOG_ERR,
            "HttpStatus::ProcessRequest() - Unexpected Exception");
    }

    return( false );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void HttpStatus::GetStatusXML( HTTPRequest *pRequest )
{
    QDomDocument doc( "Status" );

    // UTF-8 is the default, but good practice to specify it anyway
    QDomProcessingInstruction encoding =
        doc.createProcessingInstruction("xml",
                                        R"(version="1.0" encoding="UTF-8")");
    doc.appendChild(encoding);

    FillStatusXML( &doc );

    pRequest->m_eResponseType   = ResponseTypeXML;
    pRequest->m_mapRespHeaders[ "Cache-Control" ] = "no-cache=\"Ext\", max-age = 5000";

    QTextStream stream( &pRequest->m_response );
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
    stream.setCodec("UTF-8");   // Otherwise locale default is used.
#else
    stream.setEncoding(QStringConverter::Utf8);
#endif
    stream << doc.toString();
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void HttpStatus::GetStatusHTML( HTTPRequest *pRequest )
{
    pRequest->m_eResponseType = ResponseTypeHTML;
    pRequest->m_mapRespHeaders[ "Cache-Control" ] = "no-cache=\"Ext\", max-age = 5000";

    QDomDocument doc( "Status" );

    FillStatusXML( &doc );

    QTextStream stream( &pRequest->m_response );
    PrintStatus( stream, &doc );
}

static QString setting_to_localtime(const char *setting)
{
    QString origDateString = gCoreContext->GetSetting(setting);
    QDateTime origDate = MythDate::fromString(origDateString);
    return MythDate::toString(origDate, MythDate::kDateTimeFull);
}

void HttpStatus::FillStatusXML( QDomDocument *pDoc )
{
    QDateTime qdtNow          = MythDate::current();

    // Add Root Node.

    QDomElement root = pDoc->createElement("Status");
    pDoc->appendChild(root);

    root.setAttribute("date"    , MythDate::toString(
                          qdtNow, MythDate::kDateFull | MythDate::kAddYear));
    root.setAttribute("time"    ,
                      MythDate::toString(qdtNow, MythDate::kTime));
    root.setAttribute("ISODate" , qdtNow.toString(Qt::ISODate)  );
    root.setAttribute("version" , MYTH_BINARY_VERSION           );
    root.setAttribute("protoVer", MYTH_PROTO_VERSION            );

    // Add all encoders, if any

    QDomElement encoders = pDoc->createElement("Encoders");
    root.appendChild(encoders);

    int  numencoders = 0;
    bool isLocal     = true;

    TVRec::s_inputsLock.lockForRead();

    for (auto * elink : std::as_const(*m_pEncoders))
    {
        if (elink != nullptr)
        {
            TVState state = elink->GetState();
            isLocal       = elink->IsLocal();

            QDomElement encoder = pDoc->createElement("Encoder");
            encoders.appendChild(encoder);

            encoder.setAttribute("id"            , elink->GetInputID()      );
            encoder.setAttribute("local"         , static_cast<int>(isLocal));
            encoder.setAttribute("connected"     , static_cast<int>(elink->IsConnected()));
            encoder.setAttribute("state"         , state                    );
            encoder.setAttribute("sleepstatus"   , elink->GetSleepStatus()  );
            //encoder.setAttribute("lowOnFreeSpace", elink->isLowOnFreeSpace());

            if (isLocal)
                encoder.setAttribute("hostname", gCoreContext->GetHostName());
            else
                encoder.setAttribute("hostname", elink->GetHostName());

            encoder.setAttribute("devlabel",
                          CardUtil::GetDeviceLabel(elink->GetInputID()) );

            if (elink->IsConnected())
                numencoders++;

            switch (state)
            {
                case kState_WatchingLiveTV:
                case kState_RecordingOnly:
                case kState_WatchingRecording:
                {
                    ProgramInfo *pInfo = elink->GetRecording();

                    if (pInfo)
                    {
                        FillProgramInfo(pDoc, encoder, pInfo);
                        delete pInfo;
                    }

                    break;
                }

                default:
                    break;
            }
        }
    }

    TVRec::s_inputsLock.unlock();

    encoders.setAttribute("count", numencoders);

    // Add upcoming shows

    QDomElement scheduled = pDoc->createElement("Scheduled");
    root.appendChild(scheduled);

    RecList recordingList;

    if (m_pSched)
        m_pSched->GetAllPending(recordingList);

    unsigned int iNum = 10;
    unsigned int iNumRecordings = 0;

    auto itProg = recordingList.begin();
    for (; (itProg != recordingList.end()) && iNumRecordings < iNum; ++itProg)
    {
        if (((*itProg)->GetRecordingStatus() <= RecStatus::WillRecord) &&
            ((*itProg)->GetRecordingStartTime() >=
             MythDate::current()))
        {
            iNumRecordings++;
            FillProgramInfo(pDoc, scheduled, *itProg);
        }
    }

    while (!recordingList.empty())
    {
        ProgramInfo *pginfo = recordingList.back();
        delete pginfo;
        recordingList.pop_back();
    }

    scheduled.setAttribute("count", iNumRecordings);

    // Add known frontends

    QDomElement frontends = pDoc->createElement("Frontends");
    root.appendChild(frontends);

    SSDPCacheEntries *fes = SSDP::Find(
        "urn:schemas-mythtv-org:service:MythFrontend:1");
    if (fes)
    {
        EntryMap map;
        fes->GetEntryMap(map);
        fes->DecrRef();
        fes = nullptr;

        frontends.setAttribute( "count", map.size() );
        for (const auto & entry : std::as_const(map))
        {
            QDomElement fe = pDoc->createElement("Frontend");
            frontends.appendChild(fe);
            QUrl url(entry->m_sLocation);
            fe.setAttribute("name", url.host());
            fe.setAttribute("url",  url.toString(QUrl::RemovePath));
            entry->DecrRef();
        }
    }

    // Other backends

    QDomElement backends = pDoc->createElement("Backends");
    root.appendChild(backends);

    int numbes = 0;
    if (!gCoreContext->IsMasterBackend())
    {
        numbes++;
        QString masterhost = gCoreContext->GetMasterHostName();
        QString masterip   = gCoreContext->GetMasterServerIP();
        int masterport = gCoreContext->GetMasterServerStatusPort();

        QDomElement mbe = pDoc->createElement("Backend");
        backends.appendChild(mbe);
        mbe.setAttribute("type", "Master");
        mbe.setAttribute("name", masterhost);
        mbe.setAttribute("url" , masterip + ":" + QString::number(masterport));
    }

    SSDPCacheEntries *sbes = SSDP::Find(
        "urn:schemas-mythtv-org:device:SlaveMediaServer:1");
    if (sbes)
    {

        QString ipaddress = QString();
        if (!UPnp::g_IPAddrList.isEmpty())
            ipaddress = UPnp::g_IPAddrList.at(0).toString();

        EntryMap map;
        sbes->GetEntryMap(map);
        sbes->DecrRef();
        sbes = nullptr;

        for (const auto & entry : std::as_const(map))
        {
            QUrl url(entry->m_sLocation);
            if (url.host() != ipaddress)
            {
                numbes++;
                QDomElement mbe = pDoc->createElement("Backend");
                backends.appendChild(mbe);
                mbe.setAttribute("type", "Slave");
                mbe.setAttribute("name", url.host());
                mbe.setAttribute("url" , url.toString(QUrl::RemovePath));
            }
            entry->DecrRef();
        }
    }

    backends.setAttribute("count", numbes);

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

        FillProgramInfo(pDoc, job, &pginfo);
    }

    jobqueue.setAttribute( "count", jobs.size() );

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
    QString hostname;
    QString directory;
    QString isLocalstr;
    QString fsID;

    if (m_pMainServer)
        m_pMainServer->BackendQueryDiskSpace(strlist, true, m_bIsMaster);

    QDomElement total;

    // Make a temporary list to hold the per-filesystem elements so that the
    // total is always the first element.
    QList<QDomElement> fsXML;
    QStringList::const_iterator sit = strlist.cbegin();
    while (sit != strlist.cend())
    {
        hostname   = *(sit++);
        directory  = *(sit++);
        isLocalstr = *(sit++);
        fsID       = *(sit++);
        ++sit; // ignore dirID
        ++sit; // ignore blocksize
        long long iTotal     = (*(sit++)).toLongLong();
        long long iUsed      = (*(sit++)).toLongLong();;
        long long iAvail     = iTotal - iUsed;

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
            long long iLiveTV = -1;
            long long iDeleted = -1;
            long long iExpirable = -1;
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
        {
            fsXML << group;
        }
    }

    storage.appendChild(total);
    int num_elements = fsXML.size();
    for (int fs_index = 0; fs_index < num_elements; fs_index++)
    {
            storage.appendChild(fsXML[fs_index]);
    }

    // load average ---------------------

#ifdef Q_OS_ANDROID
    load.setAttribute("avg1", 0);
    load.setAttribute("avg2", 1);
    load.setAttribute("avg3", 2);
#else
    loadArray rgdAverages = getLoadAvgs();
    if (rgdAverages[0] != -1)
    {
        load.setAttribute("avg1", rgdAverages[0]);
        load.setAttribute("avg2", rgdAverages[1]);
        load.setAttribute("avg3", rgdAverages[2]);
    }
#endif

    // Guide Data ---------------------

    QDateTime GuideDataThrough;

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT MAX(endtime) FROM program WHERE manualid = 0;");

    if (query.exec() && query.next())
    {
        GuideDataThrough = MythDate::fromString(query.value(0).toString());
    }

    guide.setAttribute("start",
                       setting_to_localtime("mythfilldatabaseLastRunStart"));
    guide.setAttribute("end",
                       setting_to_localtime("mythfilldatabaseLastRunEnd"));
    guide.setAttribute("status",
        gCoreContext->GetSetting("mythfilldatabaseLastRunStatus"));
    if (gCoreContext->GetBoolSetting("MythFillGrabberSuggestsTime", false))
    {
        guide.setAttribute("next",
            gCoreContext->GetSetting("MythFillSuggestedRunTime"));
    }

    if (!GuideDataThrough.isNull())
    {
        guide.setAttribute("guideThru",
            GuideDataThrough.toString(Qt::ISODate));
        guide.setAttribute("guideDays", qdtNow.daysTo(GuideDataThrough));
    }

    // Add Miscellaneous information

    QString info_script = gCoreContext->GetSetting("MiscStatusScript");
    if ((!info_script.isEmpty()) && (info_script != "none"))
    {
        QDomElement misc = pDoc->createElement("Miscellaneous");
        root.appendChild(misc);

        uint flags = kMSRunShell | kMSStdOut;
        MythSystemLegacy ms(info_script, flags);
        ms.Run(10s);
        if (ms.Wait() != GENERIC_EXIT_OK)
        {
            LOG(VB_GENERAL, LOG_ERR,
                QString("Error running miscellaneous "
                        "status information script: %1").arg(info_script));
            return;
        }

        QByteArray input = ms.ReadAll();

        QStringList output = QString(input).split('\n',
                                                  Qt::SkipEmptyParts);
        for (const auto & line : std::as_const(output))
        {
            QDomElement info = pDoc->createElement("Information");

            QStringList list = line.split("[]:[]");
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
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void HttpStatus::PrintStatus( QTextStream &os, QDomDocument *pDoc )
{
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
    os.setCodec("UTF-8");
#else
    os.setEncoding(QStringConverter::Utf8);
#endif

    QDateTime qdtNow = MythDate::current();

    QDomElement docElem = pDoc->documentElement();

    os << "<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.0 Strict//EN\" "
       << "\"http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd\">\r\n"
       << "<html xmlns=\"http://www.w3.org/1999/xhtml\""
       << " xml:lang=\"en\" lang=\"en\">\r\n"
       << "<head>\r\n"
       << "  <meta http-equiv=\"Content-Type\""
       << "content=\"text/html; charset=UTF-8\" />\r\n"
       << "  <link rel=\"stylesheet\" href=\"/css/Status.css\" type=\"text/css\">\r\n"
       << "  <title>MythTV Status - "
       << docElem.attribute( "date", MythDate::toString(qdtNow, MythDate::kDateShort) )
       << " "
       << docElem.attribute( "time", MythDate::toString(qdtNow, MythDate::kTime) ) << " - "
       << docElem.attribute( "version", MYTH_BINARY_VERSION ) << "</title>\r\n"
       << "</head>\r\n"
       << "<body bgcolor=\"#fff\">\r\n"
       << "<div class=\"status\">\r\n"
       << "  <h1 class=\"status\">MythTV Status</h1>\r\n";

    // encoder information ---------------------

    QDomNode node = docElem.namedItem( "Encoders" );

    if (!node.isNull())
        PrintEncoderStatus( os, node.toElement() );

    // upcoming shows --------------------------

    node = docElem.namedItem( "Scheduled" );

    if (!node.isNull())
        PrintScheduled( os, node.toElement());

    // Frontends

    node = docElem.namedItem( "Frontends" );

    if (!node.isNull())
        PrintFrontends (os, node.toElement());

    // Backends

    node = docElem.namedItem( "Backends" );

    if (!node.isNull())
        PrintBackends (os, node.toElement());

    // Job Queue Entries -----------------------

    node = docElem.namedItem( "JobQueue" );

    if (!node.isNull())
        PrintJobQueue( os, node.toElement());

    // Machine information ---------------------

    node = docElem.namedItem( "MachineInfo" );

    if (!node.isNull())
        PrintMachineInfo( os, node.toElement());

    // Miscellaneous information ---------------

    node = docElem.namedItem( "Miscellaneous" );

    if (!node.isNull())
        PrintMiscellaneousInfo( os, node.toElement());

    os << "\r\n</div>\r\n</body>\r\n</html>\r\n";

}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

int HttpStatus::PrintEncoderStatus( QTextStream &os, const QDomElement& encoders )
{
    int     nNumEncoders = 0;

    if (encoders.isNull())
        return 0;

    os << "  <div class=\"content\">\r\n"
       << "    <h2 class=\"status\">Encoder Status</h2>\r\n";

    QDomNode node = encoders.firstChild();

    while (!node.isNull())
    {
        QDomElement e = node.toElement();

        if (!e.isNull())
        {
            if (e.tagName() == "Encoder")
            {
                QString sIsLocal  = (e.attribute( "local"    , "remote" )== "1")
                                                           ? "local" : "remote";
                QString sCardId   =  e.attribute( "id"       , "0"      );
                QString sHostName =  e.attribute( "hostname" , "Unknown");
                bool    bConnected=  static_cast<bool>(e.attribute( "connected", "0" ).toInt());

                bool bIsLowOnFreeSpace=static_cast<bool>(e.attribute( "lowOnFreeSpace", "0").toInt());

                QString sDevlabel = e.attribute( "devlabel", "[ UNKNOWN ]");

                os << "    Encoder " << sCardId << " " << sDevlabel
                   << " is " << sIsLocal << " on " << sHostName;

                if ((sIsLocal == "remote") && !bConnected)
                {
                    SleepStatus sleepStatus =
                        (SleepStatus) e.attribute("sleepstatus",
                            QString::number(sStatus_Undefined)).toInt();

                    if (sleepStatus == sStatus_Asleep)
                        os << " (currently asleep).<br />";
                    else
                        os << " (currently not connected).<br />";

                    node = node.nextSibling();
                    continue;
                }

                nNumEncoders++;

                TVState encState = (TVState) e.attribute( "state", "0").toInt();

                switch( encState )
                {
                    case kState_WatchingLiveTV:
                        os << " and is watching Live TV";
                        break;

                    case kState_RecordingOnly:
                    case kState_WatchingRecording:
                        os << " and is recording";
                        break;

                    default:
                        os << " and is not recording.";
                        break;
                }

                // Display first Program Element listed under the encoder

                QDomNode tmpNode = e.namedItem( "Program" );

                if (!tmpNode.isNull())
                {
                    QDomElement program  = tmpNode.toElement();

                    if (!program.isNull())
                    {
                        os << " '" << program.attribute( "title", "Unknown" ) << "'";

                        // Get Channel information

                        tmpNode = program.namedItem( "Channel" );

                        if (!tmpNode.isNull())
                        {
                            QDomElement channel = tmpNode.toElement();

                            if (!channel.isNull())
                                os <<  " on "
                                   << channel.attribute( "callSign", "unknown" );
                        }

                        // Get Recording Information (if any)

                        tmpNode = program.namedItem( "Recording" );

                        if (!tmpNode.isNull())
                        {
                            QDomElement recording = tmpNode.toElement();

                            if (!recording.isNull())
                            {
                                QDateTime endTs = MythDate::fromString(
                                    recording.attribute( "recEndTs", "" ));

                                os << ". This recording ";
                                if (endTs < MythDate::current())
                                    os << "was ";
                                else
                                    os << "is ";

                                os << "scheduled to end at "
                                   << MythDate::toString(endTs,
                                                         MythDate::kTime);
                            }
                        }
                    }

                    os << ".";
                }

                if (bIsLowOnFreeSpace)
                {
                    os << " <strong>WARNING</strong>:"
                       << " This backend is low on free disk space!";
                }

                os << "<br />\r\n";
            }
        }

        node = node.nextSibling();
    }

    os << "  </div>\r\n\r\n";

    return( nNumEncoders );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

int HttpStatus::PrintScheduled( QTextStream &os, const QDomElement& scheduled )
{
    QDateTime qdtNow          = MythDate::current();

    if (scheduled.isNull())
        return( 0 );

    int     nNumRecordings= scheduled.attribute( "count", "0" ).toInt();

    os << "  <div class=\"content\">\r\n"
       << "    <h2 class=\"status\">Schedule</h2>\r\n";

    if (nNumRecordings == 0)
    {
        os << "    There are no shows scheduled for recording.\r\n"
           << "    </div>\r\n";
        return( 0 );
    }

    os << "    The next " << nNumRecordings << " show" << (nNumRecordings == 1 ? "" : "s" )
       << " that " << (nNumRecordings == 1 ? "is" : "are")
       << " scheduled for recording:\r\n";

    os << "    <div class=\"schedule\">\r\n";

    // Iterate through all scheduled programs

    QDomNode node = scheduled.firstChild();

    while (!node.isNull())
    {
        QDomElement e = node.toElement();

        if (!e.isNull())
        {
            QDomNode recNode  = e.namedItem( "Recording" );
            QDomNode chanNode = e.namedItem( "Channel"   );

            if ((e.tagName() == "Program") && !recNode.isNull() &&
                !chanNode.isNull())
            {
                QDomElement r =  recNode.toElement();
                QDomElement c =  chanNode.toElement();

                QString   sTitle       = e.attribute( "title"   , "" );
                QString   sSubTitle    = e.attribute( "subTitle", "" );
                QDateTime airDate      = MythDate::fromString( e.attribute( "airdate" ,"" ));
                QDateTime startTs      = MythDate::fromString( e.attribute( "startTime" ,"" ));
                QDateTime endTs        = MythDate::fromString( e.attribute( "endTime"   ,"" ));
                QDateTime recStartTs   = MythDate::fromString( r.attribute( "recStartTs","" ));
//                QDateTime recEndTs     = MythDate::fromString( r.attribute( "recEndTs"  ,"" ));
                int       nPreRollSecs = r.attribute( "preRollSeconds", "0" ).toInt();
                int       nEncoderId   = r.attribute( "encoderId"     , "0" ).toInt();
                QString   sProfile     = r.attribute( "recProfile"    , ""  );
                QString   sChanName    = c.attribute( "channelName"   , ""  );
                QString   sDesc        = "";

                QDomText  text         = e.firstChild().toText();
                if (!text.isNull())
                    sDesc = text.nodeValue();

                // Build Time to recording start.

                int nTotalSecs = qdtNow.secsTo( recStartTs ) - nPreRollSecs;

                //since we're not displaying seconds

                nTotalSecs -= 60;

                int nTotalDays  =  nTotalSecs / 86400;
                int nTotalHours = (nTotalSecs / 3600)
                                - (nTotalDays * 24);
                int nTotalMins  = (nTotalSecs / 60) % 60;

                QString sTimeToStart = "in";

                sTimeToStart += QObject::tr(" %n day(s),", "", nTotalDays );
                sTimeToStart += QObject::tr(" %n hour(s) and", "", nTotalHours);
                sTimeToStart += QObject::tr(" %n minute(s)", "", nTotalMins);

                if ( nTotalHours == 0 && nTotalMins == 0)
                    sTimeToStart = QObject::tr("within one minute", "Recording starting");

                if ( nTotalSecs < 0)
                    sTimeToStart = QObject::tr("soon", "Recording starting");

                    // Output HTML

                os << "      <a href=\"#\">";
                os << MythDate::toString(recStartTs.addSecs(-nPreRollSecs),
                                         MythDate::kDateFull |
                                         MythDate::kSimplify) << " "
                   << MythDate::toString(recStartTs.addSecs(-nPreRollSecs),
                                         MythDate::kTime) << " - ";

                if (nEncoderId > 0)
                    os << "Encoder " << nEncoderId << " - ";

                os << sChanName << " - " << sTitle << "<br />"
                   << "<span><strong>" << sTitle << "</strong> ("
                   << MythDate::toString(startTs, MythDate::kTime) << "-"
                   << MythDate::toString(endTs, MythDate::kTime) << ")<br />";

                if ( !sSubTitle.isEmpty())
                    os << "<em>" << sSubTitle << "</em><br /><br />";

                if ( airDate.isValid())
                {
                    os << "Orig. Airdate: "
                       << MythDate::toString(airDate, MythDate::kDateFull |
                                                      MythDate::kAddYear)
                       << "<br /><br />";
                }

                os << sDesc << "<br /><br />"
                   << "This recording will start "  << sTimeToStart
                   << " using encoder " << nEncoderId << " with the '"
                   << sProfile << "' profile.</span></a><hr />\r\n";
            }
        }

        node = node.nextSibling();
    }
    os  << "    </div>\r\n";
    os << "  </div>\r\n\r\n";

    return( nNumRecordings );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

int HttpStatus::PrintFrontends( QTextStream &os, const QDomElement& frontends )
{
    if (frontends.isNull())
        return( 0 );

    int nNumFES= frontends.attribute( "count", "0" ).toInt();

    if (nNumFES < 1)
        return( 0 );


    os << "  <div class=\"content\">\r\n"
       << "    <h2 class=\"status\">Frontends</h2>\r\n";

    QDomNode node = frontends.firstChild();
    while (!node.isNull())
    {
        QDomElement e = node.toElement();

        if (!e.isNull())
        {
            QString name = e.attribute( "name" , "" );
            QString url  = e.attribute( "url" ,  "" );
            os << name << "&nbsp(<a href=\"" << url << "\">Status page</a>)<br />";
        }

        node = node.nextSibling();
    }

    os << "  </div>\r\n\r\n";

    return nNumFES;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

int HttpStatus::PrintBackends( QTextStream &os, const QDomElement& backends )
{
    if (backends.isNull())
        return( 0 );

    int nNumBES= backends.attribute( "count", "0" ).toInt();

    if (nNumBES < 1)
        return( 0 );


    os << "  <div class=\"content\">\r\n"
       << "    <h2 class=\"status\">Other Backends</h2>\r\n";

    QDomNode node = backends.firstChild();
    while (!node.isNull())
    {
        QDomElement e = node.toElement();

        if (!e.isNull())
        {
            QString type = e.attribute( "type",  "" );
            QString name = e.attribute( "name" , "" );
            QString url  = e.attribute( "url" ,  "" );
            os << type << ": " << name << "&nbsp(<a href=\"" << url << "\">Status page</a>)<br />";
        }

        node = node.nextSibling();
    }

    os << "  </div>\r\n\r\n";

    return nNumBES;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

int HttpStatus::PrintJobQueue( QTextStream &os, const QDomElement& jobs )
{
    if (jobs.isNull())
        return( 0 );

    int nNumJobs= jobs.attribute( "count", "0" ).toInt();

    os << "  <div class=\"content\">\r\n"
       << "    <h2 class=\"status\">Job Queue</h2>\r\n";

    if (nNumJobs != 0)
    {
        QString statusColor;
        QString jobColor;

        os << "    Jobs currently in Queue or recently ended:\r\n<br />"
           << "    <div class=\"schedule\">\r\n";


        QDomNode node = jobs.firstChild();

        while (!node.isNull())
        {
            QDomElement e = node.toElement();

            if (!e.isNull())
            {
                QDomNode progNode = e.namedItem( "Program"   );

                if ((e.tagName() == "Job") && !progNode.isNull() )
                {
                    QDomElement p =  progNode.toElement();

                    QDomNode recNode  = p.namedItem( "Recording" );
                    QDomNode chanNode = p.namedItem( "Channel"   );

                    QDomElement r =  recNode.toElement();
                    QDomElement c =  chanNode.toElement();

                    int    nType   = e.attribute( "type"  , "0" ).toInt();
                    int nStatus = e.attribute( "status", "0" ).toInt();

                    switch( nStatus )
                    {
                        case JOB_ABORTED:
                            statusColor = " class=\"jobaborted\"";
                            jobColor = "";
                            break;

                        case JOB_ERRORED:
                            statusColor = " class=\"joberrored\"";
                            jobColor = " class=\"joberrored\"";
                            break;

                        case JOB_FINISHED:
                            statusColor = " class=\"jobfinished\"";
                            jobColor = " class=\"jobfinished\"";
                            break;

                        case JOB_RUNNING:
                            statusColor = " class=\"jobrunning\"";
                            jobColor = " class=\"jobrunning\"";
                            break;

                        default:
                            statusColor = " class=\"jobqueued\"";
                            jobColor = " class=\"jobqueued\"";
                            break;
                    }

                    QString   sTitle       = p.attribute( "title"   , "" );       //.replace("\"", "&quot;");
                    QString   sSubTitle    = p.attribute( "subTitle", "" );
                    QDateTime startTs      = MythDate::fromString( p.attribute( "startTime" ,"" ));
                    QDateTime endTs        = MythDate::fromString( p.attribute( "endTime"   ,"" ));
                    QDateTime recStartTs   = MythDate::fromString( r.attribute( "recStartTs","" ));
                    QDateTime statusTime   = MythDate::fromString( e.attribute( "statusTime","" ));
                    QDateTime schedRunTime = MythDate::fromString( e.attribute( "schedTime","" ));
                    QString   sHostname    = e.attribute( "hostname", "master" );
                    QString   sComment     = "";

                    QDomText  text         = e.firstChild().toText();
                    if (!text.isNull())
                        sComment = text.nodeValue();

                    os << "<a href=\"javascript:void(0)\">"
                       << MythDate::toString(recStartTs, MythDate::kDateFull |
                                                         MythDate::kTime)
                       << " - "
                       << sTitle << " - <font" << jobColor << ">"
                       << JobQueue::JobText( nType ) << "</font><br />"
                       << "<span><strong>" << sTitle << "</strong> ("
                       << MythDate::toString(startTs, MythDate::kTime) << "-"
                       << MythDate::toString(endTs, MythDate::kTime) << ")<br />";

                    if (!sSubTitle.isEmpty())
                        os << "<em>" << sSubTitle << "</em><br /><br />";

                    os << "Job: " << JobQueue::JobText( nType ) << "<br />";

                    if (schedRunTime > MythDate::current())
                    {
                        os << "Scheduled Run Time: "
                           << MythDate::toString(schedRunTime,
                                                 MythDate::kDateFull |
                                                 MythDate::kTime)
                           << "<br />";
                    }

                    os << "Status: <font" << statusColor << ">"
                       << JobQueue::StatusText( nStatus )
                       << "</font><br />"
                       << "Status Time: "
                       << MythDate::toString(statusTime, MythDate::kDateFull |
                                                         MythDate::kTime)
                       << "<br />";

                    if ( nStatus != JOB_QUEUED)
                        os << "Host: " << sHostname << "<br />";

                    if (!sComment.isEmpty())
                        os << "<br />Comments:<br />" << sComment << "<br />";

                    os << "</span></a><hr />\r\n";
                }
            }

            node = node.nextSibling();
        }
        os << "      </div>\r\n";
    }
    else
    {
        os << "    Job Queue is currently empty.\r\n\r\n";
    }

    os << "  </div>\r\n\r\n ";

    return( nNumJobs );

}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

int HttpStatus::PrintMachineInfo( QTextStream &os, const QDomElement& info )
{
    QString   sRep;

    if (info.isNull())
        return( 0 );

    os << "<div class=\"content\">\r\n"
       << "    <h2 class=\"status\">Machine Information</h2>\r\n";

    // load average ---------------------

    QDomNode node = info.namedItem( "Load" );

    if (!node.isNull())
    {
        QDomElement e = node.toElement();

        if (!e.isNull())
        {
            double dAvg1 = e.attribute( "avg1" , "0" ).toDouble();
            double dAvg2 = e.attribute( "avg2" , "0" ).toDouble();
            double dAvg3 = e.attribute( "avg3" , "0" ).toDouble();

            os << "    <div class=\"loadstatus\">\r\n"
               << "      This machine's load average:"
               << "\r\n      <ul>\r\n        <li>"
               << "1 Minute: " << dAvg1 << "</li>\r\n"
               << "        <li>5 Minutes: " << dAvg2 << "</li>\r\n"
               << "        <li>15 Minutes: " << dAvg3
               << "</li>\r\n      </ul>\r\n"
               << "    </div>\r\n";
        }
    }

    // local drive space   ---------------------
    node = info.namedItem( "Storage" );
    QDomElement storage = node.toElement();
    node = storage.firstChild();

    // Loop once until we find id == "total".  This should be first, but a loop
    // separate from the per-filesystem details loop ensures total is first,
    // regardless.
    while (!node.isNull())
    {
        QDomElement g = node.toElement();

        if (!g.isNull() && g.tagName() == "Group")
        {
            QString id   = g.attribute("id", "" );

            if (id == "total")
            {
                int nFree    = g.attribute("free" , "0" ).toInt();
                int nTotal   = g.attribute("total", "0" ).toInt();
                int nUsed    = g.attribute("used" , "0" ).toInt();
                int nLiveTV    = g.attribute("livetv" , "0" ).toInt();
                int nDeleted   = g.attribute("deleted", "0" ).toInt();
                int nExpirable = g.attribute("expirable" , "0" ).toInt();
                QString nDir = g.attribute("dir"  , "" );

                nDir.replace(",", ", ");

                os << "      Disk Usage Summary:<br />\r\n";
                os << "      <ul>\r\n";

                os << "        <li>Total Disk Space:\r\n"
                << "          <ul>\r\n";

                os << "            <li>Total Space: ";
                sRep = QString("%L1").arg(nTotal) + " MB";
                os << sRep << "</li>\r\n";

                os << "            <li>Space Used: ";
                sRep = QString("%L1").arg(nUsed) + " MB";
                os << sRep << "</li>\r\n";

                os << "            <li>Space Free: ";
                sRep = QString("%L1").arg(nFree) + " MB";
                os << sRep << "</li>\r\n";

                if ((nLiveTV + nDeleted + nExpirable) > 0)
                {
                    os << "            <li>Space Available "
                          "After Auto-expire: ";
                    sRep = QString("%L1").arg(nUsed) + " MB";
                    sRep = QString("%L1").arg(nFree + nLiveTV +
                                      nDeleted + nExpirable) + " MB";
                    os << sRep << "\r\n";
                    os << "              <ul>\r\n";
                    os << "                <li>Space Used by LiveTV: ";
                    sRep = QString("%L1").arg(nLiveTV) + " MB";
                    os << sRep << "</li>\r\n";
                    os << "                <li>Space Used by "
                          "Deleted Recordings: ";
                    sRep = QString("%L1").arg(nDeleted) + " MB";
                    os << sRep << "</li>\r\n";
                    os << "                <li>Space Used by "
                          "Auto-expirable Recordings: ";
                    sRep = QString("%L1").arg(nExpirable) + " MB";
                    os << sRep << "</li>\r\n";
                    os << "              </ul>\r\n";
                    os << "            </li>\r\n";
                }

                os << "          </ul>\r\n"
                << "        </li>\r\n";

                os << "      </ul>\r\n";
                break;
            }
        }

        node = node.nextSibling();
    }

    // Loop again to handle per-filesystem details.
    node = storage.firstChild();

    os << "      Disk Usage Details:<br />\r\n";
    os << "      <ul>\r\n";


    while (!node.isNull())
    {
        QDomElement g = node.toElement();

        if (!g.isNull() && g.tagName() == "Group")
        {
            int nFree    = g.attribute("free" , "0" ).toInt();
            int nTotal   = g.attribute("total", "0" ).toInt();
            int nUsed    = g.attribute("used" , "0" ).toInt();
            QString nDir = g.attribute("dir"  , "" );
            QString id   = g.attribute("id"   , "" );

            nDir.replace(",", ", ");


            if (id != "total")
            {

                os << "        <li>MythTV Drive #" << id << ":"
                << "\r\n"
                << "          <ul>\r\n";

                if (nDir.contains(','))
                    os << "            <li>Directories: ";
                else
                    os << "            <li>Directory: ";

                os << nDir << "</li>\r\n";

                os << "            <li>Total Space: ";
                sRep = QString("%L1").arg(nTotal) + " MB";
                os << sRep << "</li>\r\n";

                os << "            <li>Space Used: ";
                sRep = QString("%L1").arg(nUsed) + " MB";
                os << sRep << "</li>\r\n";

                os << "            <li>Space Free: ";
                sRep = QString("%L1").arg(nFree) + " MB";
                os << sRep << "</li>\r\n";

                os << "          </ul>\r\n"
                << "        </li>\r\n";
            }

        }

        node = node.nextSibling();
    }

    os << "      </ul>\r\n";

    // Guide Info ---------------------

    node = info.namedItem( "Guide" );

    if (!node.isNull())
    {
        QDomElement e = node.toElement();

        if (!e.isNull())
        {
            int     nDays   = e.attribute( "guideDays", "0" ).toInt();
            QString sStart  = e.attribute( "start"    , ""  );
            QString sEnd    = e.attribute( "end"      , ""  );
            QString sStatus = e.attribute( "status"   , ""  );
            QDateTime next  = MythDate::fromString( e.attribute( "next"     , ""  ));
            QString sNext   = next.isNull() ? "" :
                                MythDate::toString(next, MythDate::kDateTimeFull);
            QString sMsg    = "";

            QDateTime thru  = MythDate::fromString( e.attribute( "guideThru", ""  ));

            QDomText  text  = e.firstChild().toText();

            QString mfdblrs =
                gCoreContext->GetSetting("mythfilldatabaseLastRunStart");
            QDateTime lastrunstart = MythDate::fromString(mfdblrs);

            if (!text.isNull())
                sMsg = text.nodeValue();

            os << "    Last mythfilldatabase run started on " << sStart
               << " and ";

            if (sEnd < sStart)
                os << "is ";
            else
                os << "ended on " << sEnd << ". ";

            os << sStatus << "<br />\r\n";

            if (!next.isNull() && next >= lastrunstart)
            {
                os << "    Suggested next mythfilldatabase run: "
                    << sNext << ".<br />\r\n";
            }

            if (!thru.isNull())
            {
                os << "    There's guide data until "
                   << MythDate::toString(thru, MythDate::kDatabase);

                if (nDays > 0)
                    os << " " << QObject::tr("(%n day(s))", "", nDays);

                os << ".";

                if (nDays <= 3)
                    os << " <strong>WARNING</strong>: is mythfilldatabase running?";
            }
            else
            {
                os << "    There's <strong>no guide data</strong> available! "
                   << "Have you run mythfilldatabase?";
            }
        }
    }
    os << "\r\n  </div>\r\n";

    return( 1 );
}

int HttpStatus::PrintMiscellaneousInfo( QTextStream &os, const QDomElement& info )
{
    if (info.isNull())
        return( 0 );

    // Miscellaneous information

    QDomNodeList nodes = info.elementsByTagName("Information");
    uint count = nodes.count();
    if (count > 0)
    {
        QString display;
        QString linebreak;
        //QString name, value;
        os << "<div class=\"content\">\r\n"
           << "    <h2 class=\"status\">Miscellaneous</h2>\r\n";
        for (unsigned int i = 0; i < count; i++)
        {
            QDomNode node = nodes.item(i);
            if (node.isNull())
                continue;

            QDomElement e = node.toElement();
            if (e.isNull())
                continue;

            display = e.attribute("display", "");
            //name = e.attribute("name", "");
            //value = e.attribute("value", "");

            if (display.isEmpty())
                continue;

            // Only include HTML line break if display value doesn't already
            // contain breaks.
            if (display.contains("<p>", Qt::CaseInsensitive) ||
                display.contains("<br", Qt::CaseInsensitive))
            {
                // matches <BR> or <br /
                linebreak = "\r\n";
            }
            else
            {
                linebreak = "<br />\r\n";
            }

            os << "    " << display << linebreak;
        }
        os << "</div>\r\n";
    }

    return( 1 );
}

void HttpStatus::FillProgramInfo(QDomDocument *pDoc,
                                 QDomNode     &node,
                                 ProgramInfo  *pInfo,
                                 bool          bIncChannel /* = true */,
                                 bool          bDetails    /* = true */)
{
    if ((pDoc == nullptr) || (pInfo == nullptr))
        return;

    // Build Program Element

    QDomElement program = pDoc->createElement( "Program" );
    node.appendChild( program );

    program.setAttribute( "startTime"   ,
                          pInfo->GetScheduledStartTime(MythDate::ISODate));
    program.setAttribute( "endTime"     , pInfo->GetScheduledEndTime(MythDate::ISODate));
    program.setAttribute( "title"       , pInfo->GetTitle()   );
    program.setAttribute( "subTitle"    , pInfo->GetSubtitle());
    program.setAttribute( "category"    , pInfo->GetCategory());
    program.setAttribute( "catType"     , pInfo->GetCategoryTypeString());
    program.setAttribute( "repeat"      , static_cast<int>(pInfo->IsRepeat()));

    if (bDetails)
    {

        program.setAttribute( "seriesId"    , pInfo->GetSeriesID()     );
        program.setAttribute( "programId"   , pInfo->GetProgramID()    );
        program.setAttribute( "stars"       , pInfo->GetStars()        );
        program.setAttribute( "fileSize"    ,
                              QString::number( pInfo->GetFilesize() ));
        program.setAttribute( "lastModified",
                              pInfo->GetLastModifiedTime(MythDate::ISODate) );
        program.setAttribute( "programFlags", pInfo->GetProgramFlags() );
        program.setAttribute( "hostname"    , pInfo->GetHostname() );

        if (pInfo->GetOriginalAirDate().isValid())
            program.setAttribute(
                "airdate", pInfo->GetOriginalAirDate().toString());

        QDomText textNode = pDoc->createTextNode( pInfo->GetDescription() );
        program.appendChild( textNode );

    }

    if ( bIncChannel )
    {
        // Build Channel Child Element

        QDomElement channel = pDoc->createElement( "Channel" );
        program.appendChild( channel );

        FillChannelInfo( channel, pInfo, bDetails );
    }

    // Build Recording Child Element

    if ( pInfo->GetRecordingStatus() != RecStatus::Unknown )
    {
        QDomElement recording = pDoc->createElement( "Recording" );
        program.appendChild( recording );

        recording.setAttribute( "recStatus"     ,
                                pInfo->GetRecordingStatus()   );
        recording.setAttribute( "recPriority"   ,
                                pInfo->GetRecordingPriority() );
        recording.setAttribute( "recStartTs"    ,
                                pInfo->GetRecordingStartTime(MythDate::ISODate) );
        recording.setAttribute( "recEndTs"      ,
                                pInfo->GetRecordingEndTime(MythDate::ISODate) );

        if (bDetails)
        {
            recording.setAttribute( "recordId"      ,
                                    pInfo->GetRecordingRuleID() );
            recording.setAttribute( "recGroup"      ,
                                    pInfo->GetRecordingGroup() );
            recording.setAttribute( "playGroup"     ,
                                    pInfo->GetPlaybackGroup() );
            recording.setAttribute( "recType"       ,
                                    pInfo->GetRecordingRuleType() );
            recording.setAttribute( "dupInType"     ,
                                    pInfo->GetDuplicateCheckSource() );
            recording.setAttribute( "dupMethod"     ,
                                    pInfo->GetDuplicateCheckMethod() );
            recording.setAttribute( "encoderId"     ,
                                    pInfo->GetInputID() );
            const RecordingInfo ri(*pInfo);
            recording.setAttribute( "recProfile"    ,
                                    ri.GetProgramRecordingProfile());
            //recording.setAttribute( "preRollSeconds", m_nPreRollSeconds );
        }
    }
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void HttpStatus::FillChannelInfo( QDomElement &channel,
                                  ProgramInfo *pInfo,
                                  bool         bDetails  /* = true */ )
{
    if (pInfo)
    {
/*
        QString sHostName = gCoreContext->GetHostName();
        QString sPort     = gCoreContext->GetSettingOnHost( "BackendStatusPort",
                                                        sHostName);
        QString sIconURL  = QString( "http://%1:%2/getChannelIcon?ChanId=%3" )
                                   .arg( sHostName )
                                   .arg( sPort )
                                   .arg( pInfo->chanid );
*/

        channel.setAttribute( "chanId"     , pInfo->GetChanID() );
        channel.setAttribute( "chanNum"    , pInfo->GetChanNum());
        channel.setAttribute( "callSign"   , pInfo->GetChannelSchedulingID());
        //channel.setAttribute( "iconURL"    , sIconURL           );
        channel.setAttribute( "channelName", pInfo->GetChannelName());

        if (bDetails)
        {
            channel.setAttribute( "chanFilters",
                                  pInfo->GetChannelPlaybackFilters() );
            channel.setAttribute( "sourceId"   , pInfo->GetSourceID()    );
            channel.setAttribute( "inputId"    , pInfo->GetInputID()     );
            channel.setAttribute( "commFree"   ,
                                  (pInfo->IsCommercialFree()) ? 1 : 0 );
        }
    }
}




// vim:set shiftwidth=4 tabstop=4 expandtab:
