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

    gContext->SendReceiveStringList(strlist);

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

    gContext->SendReceiveStringList(strlist);

    totalspace = strlist[0].toInt();
    usedspace = strlist[1].toInt();
}

bool RemoteCheckFile(ProgramInfo *pginfo)
{
    QStringList strlist = "QUERY_CHECKFILE";
    pginfo->ToStringList(strlist);

    gContext->SendReceiveStringList(strlist);

    bool exists = strlist[0].toInt();
    return exists;
}

void RemoteQueueTranscode(ProgramInfo *pginfo)
{
    QStringList strlist = QString("QUEUE_TRANSCODE");
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

    gContext->SendReceiveStringList(strlist);

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

    gContext->SendReceiveStringList(strlist);

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

    gContext->SendReceiveStringList(strlist);

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

    gContext->SendReceiveStringList(strlist);

    int num = strlist[0].toInt();
    QString hostname = strlist[1];
    int port = strlist[2].toInt();

    return new RemoteEncoder(num, hostname, port);
}

RemoteEncoder *RemoteGetExistingRecorder(ProgramInfo *pginfo)
{
    QStringList strlist = "GET_RECORDER_NUM";
    pginfo->ToStringList(strlist);

    gContext->SendReceiveStringList(strlist);

    int num = strlist[0].toInt();
    QString hostname = strlist[1];
    int port = strlist[2].toInt();

    return new RemoteEncoder(num, hostname, port);
}

RemoteEncoder *RemoteGetExistingRecorder(int recordernum)
{
    QStringList strlist = "GET_RECORDER_FROM_NUM";
    strlist << QString("%1").arg(recordernum);

    gContext->SendReceiveStringList(strlist);

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

    gContext->SendReceiveStringList(strlist);

    pginfo->FromStringList(strlist, 0);
}

