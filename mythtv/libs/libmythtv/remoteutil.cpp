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
    int offset = 1;

    for (int i = 0; i < numrecordings; i++)
    {
        ProgramInfo *pginfo = new ProgramInfo();
        pginfo->FromStringList(strlist, offset);
        info->push_back(pginfo);

        offset += NUMPROGRAMLINES;
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

void RemoteDeleteRecording(ProgramInfo *pginfo)
{
    QStringList strlist = QString("DELETE_RECORDING");
    pginfo->ToStringList(strlist);

    gContext->SendReceiveStringList(strlist);
}

bool RemoteGetAllPendingRecordings(vector<ProgramInfo *> &recordinglist)
{
    QStringList strlist = QString("QUERY_GETALLPENDING");

    if (!gContext->SendReceiveStringList(strlist))
        return false;

    bool conflicting = strlist[0].toInt();
    int numrecordings = strlist[1].toInt();

    int offset = 2;

    for (int i = 0; i < numrecordings; i++)
    {
        ProgramInfo *pginfo = new ProgramInfo();
        pginfo->FromStringList(strlist, offset);
        recordinglist.push_back(pginfo);

        offset += NUMPROGRAMLINES;
    }

    return conflicting;
}

void RemoteGetAllScheduledRecordings(vector<ProgramInfo *> &scheduledlist)
{
    QStringList strlist = QString("QUERY_GETALLSCHEDULED");

    if (!gContext->SendReceiveStringList(strlist))
        return;

    int numrecordings = strlist[0].toInt();
    int offset = 1;

    for (int i = 0; i < numrecordings; i++)
    {
        ProgramInfo *pginfo = new ProgramInfo();
        pginfo->FromStringList(strlist, offset);
        scheduledlist.push_back(pginfo);

        offset += NUMPROGRAMLINES;
    }
}

vector<ProgramInfo *> *RemoteGetConflictList(ProgramInfo *pginfo,
                                             bool removenonplaying)
{
    QString cmd = QString("QUERY_GETCONFLICTING %1").arg(removenonplaying);
    QStringList strlist = cmd;
    pginfo->ToStringList(strlist);

    if (!gContext->SendReceiveStringList(strlist))
        return NULL;

    int numrecordings = strlist[0].toInt();
    int offset = 1;

    vector<ProgramInfo *> *retlist = new vector<ProgramInfo *>;

    for (int i = 0; i < numrecordings; i++)
    {
        ProgramInfo *pginfo = new ProgramInfo();
        pginfo->FromStringList(strlist, offset);
        retlist->push_back(pginfo);

        offset += NUMPROGRAMLINES;
    }

    return retlist;
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

