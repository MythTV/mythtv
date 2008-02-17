#include <qdeepcopy.h>
#include <qfile.h>
#include <qstringlist.h>
#include <unistd.h>

#include "util.h"
#include "remoteutil.h"
#include "cardutil.h"
#include "inputinfo.h"
#include "programinfo.h"
#include "mythcontext.h"
#include "remoteencoder.h"
#include "tv_rec.h"

uint RemoteGetFlags(uint cardid)
{
    if (gContext->IsBackend())
    {
        const TVRec *rec = TVRec::GetTVRec(cardid);
        if (rec)
            return rec->GetFlags();
    }

    QStringList strlist = QString("QUERY_REMOTEENCODER %1").arg(cardid);
    strlist << "GET_FLAGS";
    if (!gContext->SendReceiveStringList(strlist) || strlist.empty())
        return 0;

    return strlist[0].toInt();
}

uint RemoteGetState(uint cardid)
{
    if (gContext->IsBackend())
    {
        const TVRec *rec = TVRec::GetTVRec(cardid);
        if (rec)
            return rec->GetState();
    }

    QStringList strlist = QString("QUERY_REMOTEENCODER %1").arg(cardid);
    strlist << "GET_STATE";
    if (!gContext->SendReceiveStringList(strlist) || strlist.empty())
        return kState_ChangingState;

    return strlist[0].toInt();
}

vector<ProgramInfo *> *RemoteGetRecordedList(bool deltype)
{
    QString str = "QUERY_RECORDINGS ";
    if (deltype)
        str += "Delete";
    else
        str += "Play";

    QStringList strlist = str;

    vector<ProgramInfo *> *info = new vector<ProgramInfo *>;

    if (!RemoteGetRecordingList(info, strlist))
    {
        delete info;
        return NULL;
    }
 
    return info;
}

/** \fn RemoteGetFreeSpace()
 *  \brief Returns total and used space in kilobytes for each backend.
 */
vector<FileSystemInfo> RemoteGetFreeSpace()
{
    FileSystemInfo fsInfo;
    vector<FileSystemInfo> fsInfos;
    QStringList strlist = QString("QUERY_FREE_SPACE_LIST");

    if (gContext->SendReceiveStringList(strlist))
    {
        QStringList::const_iterator it = strlist.begin();
        while (it != strlist.end())
        {
            fsInfo.hostname = *(it++);
            fsInfo.directory = *(it++);
            fsInfo.isLocal = (*(it++)).toInt();
            fsInfo.fsID = (*(it++)).toInt();
            fsInfo.dirID = (*(it++)).toInt();
            fsInfo.totalSpaceKB = decodeLongLong(strlist, it);
            fsInfo.usedSpaceKB = decodeLongLong(strlist, it);
            fsInfos.push_back(fsInfo);
        }
    }

    return fsInfos;
}

bool RemoteGetLoad(float load[3])
{
    QStringList strlist = QString("QUERY_LOAD");

    if (gContext->SendReceiveStringList(strlist))
    {
        load[0] = strlist[0].toFloat();
        load[1] = strlist[1].toFloat();
        load[2] = strlist[2].toFloat();
        return true;
    }

    return false;
}

bool RemoteGetUptime(time_t &uptime)
{
    QStringList strlist = QString("QUERY_UPTIME");

    if (!gContext->SendReceiveStringList(strlist))
        return false;

    if (!strlist[0].at(0).isNumber())
        return false;

    if (sizeof(time_t) == sizeof(int))
        uptime = strlist[0].toUInt();
    else if (sizeof(time_t) == sizeof(long))
        uptime = strlist[0].toULong();
#if QT_VERSION >= 0x030200
    else if (sizeof(time_t) == sizeof(long long))
        uptime = strlist[0].toULongLong();
#endif

    return true;
}

bool RemoteGetMemStats(int &totalMB, int &freeMB, int &totalVM, int &freeVM)
{
    QStringList strlist = QString("QUERY_MEMSTATS");

    if (gContext->SendReceiveStringList(strlist))
    {
        totalMB = strlist[0].toInt();
        freeMB  = strlist[1].toInt();
        totalVM = strlist[2].toInt();
        freeVM  = strlist[3].toInt();
        return true;
    }

    return false;
}

bool RemoteCheckFile(ProgramInfo *pginfo, bool checkSlaves)
{
    QStringList strlist = "QUERY_CHECKFILE";
    strlist << QString::number((int)checkSlaves);
    pginfo->ToStringList(strlist);

    if ((!gContext->SendReceiveStringList(strlist)) ||
        (!strlist[0].toInt()))
        return false;

    // Only modify the pathname if the recording file is available locally on
    // this host
    QString localpath = strlist[1];
    QFile checkFile(localpath);
    if (checkFile.exists())
        pginfo->pathname = localpath;

    return true;
}

bool RemoteRecordPending(uint cardid, const ProgramInfo *pginfo,
                         int secsleft, bool hasLater)
{
    if (gContext->IsBackend())
    {
        TVRec *rec = TVRec::GetTVRec(cardid);
        if (rec)
        {
            rec->RecordPending(pginfo, secsleft, hasLater);
            return true;
        }
    }

    QStringList strlist = QString("QUERY_REMOTEENCODER %1").arg(cardid);
    strlist << "RECORD_PENDING";
    strlist << QString::number(secsleft);
    strlist << QString::number(hasLater);
    pginfo->ToStringList(strlist);

    if (!gContext->SendReceiveStringList(strlist) || strlist.empty())
        return false;

    return strlist[0].upper() == "OK";
}

void RemoteStopRecording(ProgramInfo *pginfo)
{
    QStringList strlist = QString("STOP_RECORDING");
    pginfo->ToStringList(strlist);

    gContext->SendReceiveStringList(strlist);
}

bool RemoteStopLiveTV(uint cardid)
{
    if (gContext->IsBackend())
    {
        TVRec *rec = TVRec::GetTVRec(cardid);
        if (rec)
        {
            rec->StopLiveTV();
            return true;
        }
    }

    QStringList strlist = QString("QUERY_REMOTEENCODER %1").arg(cardid);
    strlist << "STOP_LIVETV";

    if (!gContext->SendReceiveStringList(strlist) || strlist.empty())
        return false;

    return strlist[0].upper() == "OK";
}

bool RemoteStopRecording(uint cardid)
{
    if (gContext->IsBackend())
    {
        TVRec *rec = TVRec::GetTVRec(cardid);
        if (rec)
        {
            rec->StopRecording();
            return true;
        }
    }

    QStringList strlist = QString("QUERY_REMOTEENCODER %1").arg(cardid);
    strlist << "STOP_RECORDING";

    if (!gContext->SendReceiveStringList(strlist) || strlist.empty())
        return false;

    return strlist[0].upper() == "OK";
}

bool RemoteDeleteRecording(ProgramInfo *pginfo, bool forgetHistory,
                           bool forceMetadataDelete)
{
    bool result = true;
    QStringList strlist;

    if (forceMetadataDelete)
        strlist = QString("FORCE_DELETE_RECORDING");
    else
        strlist = QString("DELETE_RECORDING");
    pginfo->ToStringList(strlist);

    gContext->SendReceiveStringList(strlist);

    if (strlist[0].toInt() == -2)
        result = false;

    if (forgetHistory)
    {
        strlist = QString("FORGET_RECORDING");
        pginfo->ToStringList(strlist);

        gContext->SendReceiveStringList(strlist);
    }

    return result;
}

bool RemoteUndeleteRecording(ProgramInfo *pginfo)
{
    bool result = false;

    bool undelete_possible = 
            gContext->GetNumSetting("AutoExpireInsteadOfDelete", 0);

    if (!undelete_possible)
        return result;

    QStringList strlist;

    strlist = QString("UNDELETE_RECORDING");
    pginfo->ToStringList(strlist);

    gContext->SendReceiveStringList(strlist);

    if (strlist[0].toInt() == 0)
        result = true;

    return result;
}

void RemoteGetAllScheduledRecordings(vector<ProgramInfo *> &scheduledlist)
{
    QStringList strList = QString("QUERY_GETALLSCHEDULED");
    RemoteGetRecordingList(&scheduledlist, strList);
}

void RemoteGetAllExpiringRecordings(vector<ProgramInfo *> &expiringlist)
{
    QStringList strList = QString("QUERY_GETEXPIRING");
    RemoteGetRecordingList(&expiringlist, strList);
}

int RemoteGetRecordingList(vector<ProgramInfo *> *reclist, QStringList &strList)
{
    if (!gContext->SendReceiveStringList(strList))
        return 0;

    int numrecordings = strList[0].toInt();

    if (numrecordings > 0)
    {
        if (numrecordings * NUMPROGRAMLINES + 1 > (int)strList.size())
        {
            cerr << "length mismatch between programinfo\n";
            return 0;
        }

        QStringList::const_iterator it = strList.at(1);
        for (int i = 0; i < numrecordings; i++)
        {
            ProgramInfo *pginfo = new ProgramInfo();
            pginfo->FromStringList(it, strList.end());
            reclist->push_back(pginfo);
        }
    }

    return numrecordings;
}

vector<ProgramInfo *> *RemoteGetConflictList(ProgramInfo *pginfo)
{
    QString cmd = QString("QUERY_GETCONFLICTING");
    QStringList strlist = cmd;
    pginfo->ToStringList(strlist);

    vector<ProgramInfo *> *retlist = new vector<ProgramInfo *>;

    RemoteGetRecordingList(retlist, strlist);
    return retlist;
}

RemoteEncoder *RemoteRequestNextFreeRecorder(int curr)
{
    QStringList strlist = "GET_NEXT_FREE_RECORDER";
    strlist << QString("%1").arg(curr);

    if (!gContext->SendReceiveStringList(strlist, true))
        return NULL;

    int num = strlist[0].toInt();
    QString hostname = strlist[1];
    int port = strlist[2].toInt();

    return new RemoteEncoder(num, hostname, port);
}

// Takes a QStringList of recorder numbers. Returns the first available
// free recorder from this list.
RemoteEncoder *RemoteRequestFreeRecorderFromList(QStringList &qualifiedRecorders)
{
    QStringList strlist = "GET_FREE_RECORDER_LIST";

    if (!gContext->SendReceiveStringList(strlist, true))
        return NULL;

    for (QStringList::iterator recIter = qualifiedRecorders.begin();
         recIter != qualifiedRecorders.end(); ++recIter) 
    {
        if (strlist.find(*recIter) == strlist.end()) 
        {
            // did not find it in the free recorder list. We
            // move on to check the next recorder
            continue;
        }
        // at this point we found a free recorder that fulfills our request
        return RemoteGetExistingRecorder((*recIter).toInt());
    }
    // didn't find anything. just return NULL.
    return NULL;
}

RemoteEncoder *RemoteRequestRecorder(void)
{
    QStringList strlist = "GET_FREE_RECORDER";

    if (!gContext->SendReceiveStringList(strlist, true))
        return NULL;

    int num = strlist[0].toInt();
    QString hostname = strlist[1];
    int port = strlist[2].toInt();

    return new RemoteEncoder(num, hostname, port);
}

RemoteEncoder *RemoteGetExistingRecorder(ProgramInfo *pginfo)
{
    QStringList strlist = "GET_RECORDER_NUM";
    pginfo->ToStringList(strlist);

    if (!gContext->SendReceiveStringList(strlist))
        return NULL;

    int num = strlist[0].toInt();
    QString hostname = strlist[1];
    int port = strlist[2].toInt();

    return new RemoteEncoder(num, hostname, port);
}

RemoteEncoder *RemoteGetExistingRecorder(int recordernum)
{
    QStringList strlist = "GET_RECORDER_FROM_NUM";
    strlist << QString("%1").arg(recordernum);

    if (!gContext->SendReceiveStringList(strlist))
        return NULL;

    QString hostname = strlist[0];
    int port = strlist[1].toInt();

    return new RemoteEncoder(recordernum, hostname, port);
}

vector<uint> RemoteRequestFreeRecorderList(void)
{
    vector<uint> list;

    QStringList strlist = "GET_FREE_RECORDER_LIST";

    if (!gContext->SendReceiveStringList(strlist, true))
        return list;

    QStringList::const_iterator it = strlist.begin();
    for (; it != strlist.end(); ++it) 
        list.push_back((*it).toUInt());

    return list;
}

vector<InputInfo> RemoteRequestFreeInputList(uint cardid,
                                             vector<uint> excluded_cardids)
{
    vector<InputInfo> list;

    QStringList strlist = QString("QUERY_RECORDER %1").arg(cardid);
    strlist << "GET_FREE_INPUTS";
    for (uint i = 0; i < excluded_cardids.size(); i++)
        strlist << QString::number(excluded_cardids[i]);

    if (!gContext->SendReceiveStringList(strlist))
        return list;

    QStringList::const_iterator it = strlist.begin();
    if ((it == strlist.end()) || (*it == "EMPTY_LIST"))
        return list;

    while (it != strlist.end())
    {
        InputInfo info;
        if (!info.FromStringList(it, strlist.end()))
            break;
        list.push_back(info);
    }

    return list;
}

InputInfo RemoteRequestBusyInputID(uint cardid)
{
    InputInfo blank;

    QStringList strlist = QString("QUERY_RECORDER %1").arg(cardid);
    strlist << "GET_BUSY_INPUT";

    if (!gContext->SendReceiveStringList(strlist))
        return blank;

    QStringList::const_iterator it = strlist.begin();
    if ((it == strlist.end()) || (*it == "EMPTY_LIST"))
        return blank;

    InputInfo info;
    if (info.FromStringList(it, strlist.end()))
        return info;

    return blank;
}

void RemoteCancelNextRecording(uint cardid, bool cancel)
{
    QStringList strlist = QString("QUERY_RECORDER %1").arg(cardid);
    strlist << "CANCEL_NEXT_RECORDING";
    strlist << QString::number((cancel) ? 1 : 0);
                          
    gContext->SendReceiveStringList(strlist);
}

void RemoteSendMessage(const QString &message)
{
    QStringList strlist = "MESSAGE";
    strlist << message;

    gContext->SendReceiveStringList(strlist);
}

void RemoteGeneratePreviewPixmap(ProgramInfo *pginfo)
{
    QStringList strlist = "QUERY_GENPIXMAP";
    pginfo->ToStringList(strlist);

    gContext->SendReceiveStringList(strlist);
}
    
QDateTime RemoteGetPreviewLastModified(ProgramInfo *pginfo)
{
    QDateTime retdatetime;

    QStringList strlist = "QUERY_PIXMAP_LASTMODIFIED"; 
    pginfo->ToStringList(strlist);
    
    if (!gContext->SendReceiveStringList(strlist))
        return retdatetime;

    if (!strlist.empty() && strlist[0] != "BAD")
    {
        uint timet = strlist[0].toUInt();
        retdatetime.setTime_t(timet);
    }
        
    return retdatetime;
}

void RemoteFillProginfo(ProgramInfo *pginfo, const QString &playbackhostname)
{
    QStringList strlist = "FILL_PROGRAM_INFO";
    strlist << playbackhostname;
    pginfo->ToStringList(strlist);

    if (gContext->SendReceiveStringList(strlist))
        pginfo->FromStringList(strlist, 0);
}

bool RemoteIsBusy(uint cardid, TunedInputInfo &busy_input)
{
    //VERBOSE(VB_IMPORTANT, QString("RemoteIsBusy(%1) %2")
    //        .arg(cardid).arg(gContext->IsBackend() ? "be" : "fe"));

    busy_input.Clear();

    if (gContext->IsBackend())
    {
        const TVRec *rec = TVRec::GetTVRec(cardid);
        if (rec)
            return rec->IsBusy(&busy_input);
    }

    QStringList strlist = QString("QUERY_REMOTEENCODER %1").arg(cardid);
    strlist << "IS_BUSY";
    if (!gContext->SendReceiveStringList(strlist) || strlist.empty())
        return true;

    QStringList::const_iterator it = strlist.begin();
    bool state = (*it).toInt();
    it++;
    busy_input.FromStringList(it, strlist.end());

    return state;
}

QStringList RemoteRecordings(void)
{
    QStringList strlist = "QUERY_ISRECORDING";
    QStringList empty;

    empty << "0" << "0";

    if (!gContext->SendReceiveStringList(strlist, false, false))
        return empty;

    return strlist;
}

int RemoteGetRecordingMask(void)
{
    int mask = 0;

    QString cmd = "QUERY_ISRECORDING";

    QStringList strlist = cmd;

    if (!gContext->SendReceiveStringList(strlist))
        return mask;

    int recCount = strlist[0].toInt();

    for (int i = 0, j = 0; j < recCount; i++)
    {
        cmd = QString("QUERY_RECORDER %1").arg(i + 1);

        strlist = cmd;
        strlist << "IS_RECORDING";

        if (gContext->SendReceiveStringList(strlist))
        {
            if (strlist[0].toInt())
            {
                mask |= 1<<i;
                j++;       // count active recorder
            }
        }
    }

    return mask;
}

int RemoteGetFreeRecorderCount(void)
{
    QStringList strlist = "GET_FREE_RECORDER_COUNT";

    if (!gContext->SendReceiveStringList(strlist, true))
        return 0;

    if (strlist[0] == "UNKNOWN_COMMAND")
    {
        cerr << "Unknown command GET_FREE_RECORDER_COUNT, upgrade "
                "your backend version." << endl;
        return 0;
    }

    return strlist[0].toInt();
}

int RemoteCheckForRecording(ProgramInfo *pginfo)
{  //returns recordernum if pginfo recording in progress, else 0
    QStringList strlist = QString("CHECK_RECORDING");
    pginfo->ToStringList(strlist);

    gContext->SendReceiveStringList(strlist);

    return strlist[0].toInt();
}

int RemoteGetRecordingStatus(ProgramInfo *pginfo, int overrecsecs,
                             int underrecsecs)
{ //returns 0: not recording, 1: recording, 2: underrecord, 3: overrecord
    QDateTime curtime = QDateTime::currentDateTime();

    int retval = 0;

    if (pginfo)
    {
        if (curtime >= pginfo->startts.addSecs(-underrecsecs) &&
            curtime < pginfo->endts.addSecs(overrecsecs))
        {
            if (curtime >= pginfo->startts && curtime < pginfo->endts)
                retval = 1;
            else if (curtime < pginfo->startts && 
                     RemoteCheckForRecording(pginfo) > 0)
                retval = 2;
            else if (curtime > pginfo->endts && 
                     RemoteCheckForRecording(pginfo) > 0)
                retval = 3;
        }
    }

    return retval;
}

bool RemoteGetRecordingStatus(
    QPtrList<TunerStatus> *tunerList, bool list_inactive)
{
    bool isRecording = false;
    vector<uint> cardlist = CardUtil::GetCardList();

    if (tunerList)
        tunerList->clear();

    for (uint i = 0; i < cardlist.size(); i++)
    {
        QString     status      = "";
        uint        cardid      = cardlist[i];
        int         state       = kState_ChangingState;
        QString     channelName = "";
        QString     title       = "";
        QString     subtitle    = "";
        QDateTime   dtStart     = QDateTime();
        QDateTime   dtEnd       = QDateTime();
        QStringList strlist;

        QString cmd = QString("QUERY_REMOTEENCODER %1").arg(cardid);

        while (state == kState_ChangingState)
        {
            strlist = cmd;
            strlist << "GET_STATE";
            gContext->SendReceiveStringList(strlist);

            if (strlist.empty())
                break;

            state = strlist[0].toInt();
            if (kState_ChangingState == state)
                usleep(5000);
        }

        if (kState_RecordingOnly == state || kState_WatchingRecording == state)
        {
            isRecording |= true;

            if (!tunerList)
                break;

            strlist = QString("QUERY_RECORDER %1").arg(cardid);
            strlist << "GET_RECORDING";
            gContext->SendReceiveStringList(strlist);

            ProgramInfo progInfo;
            QStringList::const_iterator it = strlist.constBegin();
            progInfo.FromStringList(it, strlist.constEnd());

            title       = progInfo.title;
            subtitle    = progInfo.subtitle;
            channelName = progInfo.channame;
            dtStart     = progInfo.startts;
            dtEnd       = progInfo.endts;
        }
        else if (!list_inactive)
            continue;

        if (tunerList)
        {
            TunerStatus *tuner = new TunerStatus;
            tuner->id          = cardid;
            tuner->isRecording = ((kState_RecordingOnly     == state) ||
                                  (kState_WatchingRecording == state));
            tuner->channame    = channelName;
            tuner->title       = (kState_ChangingState == state) ?
                QObject::tr("Error querying recorder state") : title;
            tuner->subtitle    = subtitle;
            tuner->startTime   = dtStart;
            tuner->endTime     = dtEnd;
            tunerList->append(tuner);
        }
    }

    return isRecording;
}


/*
 * \brief return list of currently recording shows
 */
vector<ProgramInfo *> *RemoteGetCurrentlyRecordingList(void)
{
    QString str = "QUERY_RECORDINGS ";
    str += "Recording";
    QStringList strlist = str;

    vector<ProgramInfo *> *reclist = new vector<ProgramInfo *>;
    vector<ProgramInfo *> *info = new vector<ProgramInfo *>;
    if (!RemoteGetRecordingList(info, strlist))
    {
        if (info)
            delete info;
        return reclist;
    }

    ProgramInfo *p = NULL;
    vector<ProgramInfo *>::iterator it = info->begin();
    // make sure whatever remotegetrecordinglist returned
    // only has rsRecording shows
    for ( ; it != info->end(); it++)
    {
        p = *it;
        if (p->recstatus == rsRecording)
            reclist->push_back(new ProgramInfo(*p));
    }
    
    if (info)
        delete info;

    return reclist; 
}
/* vim: set expandtab tabstop=4 shiftwidth=4: */
