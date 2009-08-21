#include <unistd.h>

#include <QFile>

#include "remoteutil.h"
#include "programinfo.h"
#include "mythcontext.h"
#include "decodeencode.h"

vector<ProgramInfo *> *RemoteGetRecordedList(bool deltype)
{
    QString str = "QUERY_RECORDINGS ";
    if (deltype)
        str += "Delete";
    else
        str += "Play";

    QStringList strlist(str);

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
    QStringList strlist(QString("QUERY_FREE_SPACE_LIST"));

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
            fsInfo.blocksize = (*(it++)).toInt();
            fsInfo.totalSpaceKB = decodeLongLong(strlist, it);
            fsInfo.usedSpaceKB = decodeLongLong(strlist, it);
            fsInfos.push_back(fsInfo);
        }
    }

    return fsInfos;
}

bool RemoteGetLoad(float load[3])
{
    QStringList strlist(QString("QUERY_LOAD"));

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
    QStringList strlist(QString("QUERY_UPTIME"));

    if (!gContext->SendReceiveStringList(strlist))
        return false;

    if (!strlist[0].at(0).isNumber())
        return false;

    if (sizeof(time_t) == sizeof(int))
        uptime = strlist[0].toUInt();
    else if (sizeof(time_t) == sizeof(long))
        uptime = strlist[0].toULong();
    else if (sizeof(time_t) == sizeof(long long))
        uptime = strlist[0].toULongLong();

    return true;
}

bool RemoteGetMemStats(int &totalMB, int &freeMB, int &totalVM, int &freeVM)
{
    QStringList strlist(QString("QUERY_MEMSTATS"));

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
    QStringList strlist("QUERY_CHECKFILE");
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

bool RemoteDeleteRecording(ProgramInfo *pginfo, bool forgetHistory,
                           bool forceMetadataDelete)
{
    bool result = true;
    QStringList strlist;

    if (forceMetadataDelete)
        strlist.append(QString("FORCE_DELETE_RECORDING"));
    else
        strlist.append(QString("DELETE_RECORDING"));
    pginfo->ToStringList(strlist);

    gContext->SendReceiveStringList(strlist);

    if (strlist[0].toInt() == -2)
        result = false;

    if (forgetHistory)
    {
        strlist = QStringList(QString("FORGET_RECORDING"));
        pginfo->ToStringList(strlist);

        gContext->SendReceiveStringList(strlist);
    }

    return result;
}

bool RemoteUndeleteRecording(const ProgramInfo *pginfo)
{
    bool result = false;

    bool undelete_possible = 
            gContext->GetNumSetting("AutoExpireInsteadOfDelete", 0);

    if (!undelete_possible)
        return result;

    QStringList strlist(QString("UNDELETE_RECORDING"));
    pginfo->ToStringList(strlist);

    gContext->SendReceiveStringList(strlist);

    if (strlist[0].toInt() == 0)
        result = true;

    return result;
}

void RemoteGetAllScheduledRecordings(vector<ProgramInfo *> &scheduledlist)
{
    QStringList strList(QString("QUERY_GETALLSCHEDULED"));
    RemoteGetRecordingList(&scheduledlist, strList);
}

void RemoteGetAllExpiringRecordings(vector<ProgramInfo *> &expiringlist)
{
    QStringList strList(QString("QUERY_GETEXPIRING"));
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

        QStringList::const_iterator it = strList.begin() + 1;
        for (int i = 0; i < numrecordings; i++)
        {
            ProgramInfo *pginfo = new ProgramInfo();
            pginfo->FromStringList(it, strList.end());
            reclist->push_back(pginfo);
        }
    }

    return numrecordings;
}

vector<ProgramInfo *> *RemoteGetConflictList(const ProgramInfo *pginfo)
{
    QString cmd = QString("QUERY_GETCONFLICTING");
    QStringList strlist( cmd );
    pginfo->ToStringList(strlist);

    vector<ProgramInfo *> *retlist = new vector<ProgramInfo *>;

    RemoteGetRecordingList(retlist, strlist);
    return retlist;
}

vector<uint> RemoteRequestFreeRecorderList(void)
{
    vector<uint> list;

    QStringList strlist("GET_FREE_RECORDER_LIST");

    if (!gContext->SendReceiveStringList(strlist, true))
        return list;

    QStringList::const_iterator it = strlist.begin();
    for (; it != strlist.end(); ++it) 
        list.push_back((*it).toUInt());

    return list;
}

void RemoteSendMessage(const QString &message)
{
    QStringList strlist( "MESSAGE" );
    strlist << message;

    gContext->SendReceiveStringList(strlist);
}

void RemoteGeneratePreviewPixmap(const ProgramInfo *pginfo)
{
    QStringList strlist( "QUERY_GENPIXMAP" );
    pginfo->ToStringList(strlist);

    gContext->SendReceiveStringList(strlist);
}
    
QDateTime RemoteGetPreviewLastModified(const ProgramInfo *pginfo)
{
    QDateTime retdatetime;

    QStringList strlist( "QUERY_PIXMAP_LASTMODIFIED" );
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
    QStringList strlist( "FILL_PROGRAM_INFO" );
    strlist << playbackhostname;
    pginfo->ToStringList(strlist);

    if (gContext->SendReceiveStringList(strlist))
        pginfo->FromStringList(strlist, 0);
}

QStringList RemoteRecordings(void)
{
    QStringList strlist("QUERY_ISRECORDING");

    if (!gContext->SendReceiveStringList(strlist, false, false))
    {
        QStringList empty;
        empty << "0" << "0";
        return empty;
    }

    return strlist;
}

int RemoteGetRecordingMask(void)
{
    int mask = 0;

    QString cmd = "QUERY_ISRECORDING";

    QStringList strlist( cmd );

    if (!gContext->SendReceiveStringList(strlist))
        return mask;

    if (strlist.empty())
        return 0;

    int recCount = strlist[0].toInt();

    for (int i = 0, j = 0; j < recCount; i++)
    {
        cmd = QString("QUERY_RECORDER %1").arg(i + 1);

        strlist = QStringList( cmd );
        strlist << "IS_RECORDING";

        if (gContext->SendReceiveStringList(strlist) && !strlist.empty())
        {
            if (strlist[0].toInt())
            {
                mask |= 1<<i;
                j++;       // count active recorder
            }
        }
        else
        {
            break;
        }
    }

    return mask;
}

int RemoteGetFreeRecorderCount(void)
{
    QStringList strlist( "GET_FREE_RECORDER_COUNT" );

    if (!gContext->SendReceiveStringList(strlist, true))
        return 0;

    if (strlist.empty())
        return 0;

    if (strlist[0] == "UNKNOWN_COMMAND")
    {
        cerr << "Unknown command GET_FREE_RECORDER_COUNT, upgrade "
                "your backend version." << endl;
        return 0;
    }

    return strlist[0].toInt();
}

/**
 * Get recorder for a programme.
 *
 * \return recordernum if pginfo recording in progress, else 0
 */
int RemoteCheckForRecording(const ProgramInfo *pginfo)
{
    QStringList strlist( QString("CHECK_RECORDING") );
    pginfo->ToStringList(strlist);

    if (gContext->SendReceiveStringList(strlist) && !strlist.empty())
        return strlist[0].toInt();

    return 0;
}

/**
 * Get status of an individual programme (with pre-post roll?).
 *
 * \retval  0  Not Recording
 * \retval  1  Recording
 * \retval  2  Under-Record
 * \retval  3  Over-Record
 */
int RemoteGetRecordingStatus(
    const ProgramInfo *pginfo, int overrecsecs, int underrecsecs)
{
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

/**
 * \brief return list of currently recording shows
 */
vector<ProgramInfo *> *RemoteGetCurrentlyRecordingList(void)
{
    QString str = "QUERY_RECORDINGS ";
    str += "Recording";
    QStringList strlist( str );

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
        if (p->recstatus == rsRecording || (p->recstatus == rsRecorded
                                            && p->recgroup == "LiveTV"))
            reclist->push_back(new ProgramInfo(*p));
    }
    
    while (!info->empty())
    {
        delete info->back();
        info->pop_back();
    }
    if (info)
        delete info;

    return reclist; 
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
