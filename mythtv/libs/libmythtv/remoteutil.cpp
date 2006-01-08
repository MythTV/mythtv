#include <qstringlist.h>

#include "util.h"
#include "remoteutil.h"
#include "programinfo.h"
#include "mythcontext.h"
#include "remoteencoder.h"

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
        QStringList::iterator it = strlist.begin();
        while (it != strlist.end())
        {
            fsInfo.hostname = *(it++);
            fsInfo.totalSpaceKB = decodeLongLong(strlist, it);
            fsInfo.usedSpaceKB = decodeLongLong(strlist, it);
            fsInfos.push_back(fsInfo);
        }
    }

    // Consolidate hosts sharing storage
    vector<FileSystemInfo>::iterator it1, it2;
    for (it1 = fsInfos.begin(); it1 != fsInfos.end(); it1++)
    {
        it2 = it1;
        for (it2++; it2 != fsInfos.end(); it2++)
        {
            if ((it1->totalSpaceKB == it2->totalSpaceKB) &&
                (it1->usedSpaceKB*0.95 < it2->usedSpaceKB) &&
                (it1->usedSpaceKB*1.05 > it2->usedSpaceKB))
            {
                it1->hostname = it1->hostname + ", " + it2->hostname;
                fsInfos.erase(it2);
                it2 = it1;
            }
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

bool RemoteCheckFile(ProgramInfo *pginfo)
{
    QStringList strlist = "QUERY_CHECKFILE";
    pginfo->ToStringList(strlist);

    if (!gContext->SendReceiveStringList(strlist))
        return false;

    return strlist[0].toInt();
}

void RemoteStopRecording(ProgramInfo *pginfo)
{
    QStringList strlist = QString("STOP_RECORDING");
    pginfo->ToStringList(strlist);

    gContext->SendReceiveStringList(strlist);
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

        QStringList::iterator it = strList.at(1);
        for (int i = 0; i < numrecordings; i++)
        {
            ProgramInfo *pginfo = new ProgramInfo();
            pginfo->FromStringList(strList, it);
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
    
QString RemoteGetPreviewLastModified(ProgramInfo *pginfo)
{
    QString retdatetime;

    QStringList strlist = "QUERY_PIXMAP_LASTMODIFIED"; 
    pginfo->ToStringList(strlist);
    
    if (!gContext->SendReceiveStringList(strlist))
        return retdatetime;

    return strlist[0];
}

void RemoteFillProginfo(ProgramInfo *pginfo, const QString &playbackhostname)
{
    QStringList strlist = "FILL_PROGRAM_INFO";
    strlist << playbackhostname;
    pginfo->ToStringList(strlist);

    if (gContext->SendReceiveStringList(strlist))
        pginfo->FromStringList(strlist, 0);
}

int RemoteIsRecording(void)
{
    QStringList strlist = "QUERY_ISRECORDING";

    if (!gContext->SendReceiveStringList(strlist))
        return -1;

    return strlist[0].toInt();
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

