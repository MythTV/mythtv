#include <qstringlist.h>

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

    if (!gContext->SendReceiveStringList(strlist))
        return NULL;

    int numrecordings = strlist[0].toInt();

    vector<ProgramInfo *> *info = new vector<ProgramInfo *>;

    if (numrecordings > 0)
    {
        if (numrecordings * NUMPROGRAMLINES + 1 > (int)strlist.size())
        {
            cerr << "length mismatch between programinfo\n";
            return info;
        }

        QStringList::iterator it = strlist.at(1);
        for (int i = 0; i < numrecordings; i++)
        {
            ProgramInfo *pginfo = new ProgramInfo();
            pginfo->FromStringList(strlist, it);
            info->push_back(pginfo);
        }
    } 
 
    return info;
}

void RemoteGetFreeSpace(int &totalspace, int &usedspace)
{
    QStringList strlist = QString("QUERY_FREESPACE");

    if (gContext->SendReceiveStringList(strlist))
    {
        totalspace = strlist[0].toInt();
        usedspace = strlist[1].toInt();
    }
    else
    {
        totalspace = 0;
        usedspace = 0;
    }
}

bool RemoteCheckFile(ProgramInfo *pginfo)
{
    QStringList strlist = "QUERY_CHECKFILE";
    pginfo->ToStringList(strlist);

    if (!gContext->SendReceiveStringList(strlist))
        return false;

    return strlist[0].toInt();
}

void RemoteQueueTranscode(ProgramInfo *pginfo, int state)
{
    QStringList strlist;
    if (state & TRANSCODE_STOP)
        strlist = QString("QUEUE_TRANSCODE_STOP");
    else
        strlist = QString("QUEUE_TRANSCODE_CUTLIST");

    pginfo->ToStringList(strlist);

    gContext->SendReceiveStringList(strlist);
}

void RemoteStopRecording(ProgramInfo *pginfo)
{
    QStringList strlist = QString("STOP_RECORDING");
    pginfo->ToStringList(strlist);

    gContext->SendReceiveStringList(strlist);
}

void RemoteDeleteRecording(ProgramInfo *pginfo, bool forgetHistory)
{
    QStringList strlist;
    strlist = QString("DELETE_RECORDING");
    pginfo->ToStringList(strlist);

    gContext->SendReceiveStringList(strlist);

    if (forgetHistory)
    {
        strlist = QString("FORGET_RECORDING");
        pginfo->ToStringList(strlist);

        gContext->SendReceiveStringList(strlist);
    }
}

bool RemoteReactivateRecording(ProgramInfo *pginfo)
{
    QStringList strlist = QString("REACTIVATE_RECORDING");
    pginfo->ToStringList(strlist);

    if (!gContext->SendReceiveStringList(strlist))
        return false;

    return strlist[0].toInt();
}

bool RemoteGetAllPendingRecordings(vector<ProgramInfo *> &recordinglist)
{
    QStringList strlist = QString("QUERY_GETALLPENDING");

    if (!gContext->SendReceiveStringList(strlist))
        return false;

    bool conflicting = strlist[0].toInt();
    int numrecordings = strlist[1].toInt();

    if (numrecordings > 0)
    {
        if (numrecordings * NUMPROGRAMLINES + 2 > (int)strlist.size())
        {
            cerr << "length mismatch between programinfo\n";
            return conflicting;
        }

        QStringList::iterator it = strlist.at(2);
        for (int i = 0; i < numrecordings; i++)
        {
            ProgramInfo *pginfo = new ProgramInfo();
            pginfo->FromStringList(strlist, it);
            recordinglist.push_back(pginfo);
        }
    }

    return conflicting;
}

void RemoteGetAllScheduledRecordings(vector<ProgramInfo *> &scheduledlist)
{
    QStringList strlist = QString("QUERY_GETALLSCHEDULED");

    if (!gContext->SendReceiveStringList(strlist))
        return;

    int numrecordings = strlist[0].toInt();

    if (numrecordings > 0)
    {
        if (numrecordings * NUMPROGRAMLINES + 1 > (int)strlist.size())
        {
            cerr << "length mismatch between programinfo\n";
            return;
        }

        QStringList::iterator it = strlist.at(1);
        for (int i = 0; i < numrecordings; i++)
        {
            ProgramInfo *pginfo = new ProgramInfo();
            pginfo->FromStringList(strlist, it);
            scheduledlist.push_back(pginfo);
        }
    }
}

vector<ProgramInfo *> *RemoteGetConflictList(ProgramInfo *pginfo)
{
    QString cmd = QString("QUERY_GETCONFLICTING");
    QStringList strlist = cmd;
    pginfo->ToStringList(strlist);

    if (!gContext->SendReceiveStringList(strlist))
        return NULL;

    int numrecordings = strlist[0].toInt();

    vector<ProgramInfo *> *retlist = new vector<ProgramInfo *>;

    if (numrecordings > 0)
    {
        if (numrecordings * NUMPROGRAMLINES + 1 > (int)strlist.size())
        {
            cerr << "length mismatch between programinfo\n";
            return retlist;
        }

        QStringList::iterator it = strlist.at(1);

        for (int i = 0; i < numrecordings; i++)
        {
            ProgramInfo *pginfo = new ProgramInfo();
            pginfo->FromStringList(strlist, it);
            retlist->push_back(pginfo);
        }
    }

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

