#include <qstringlist.h>

#include "remoteutil.h"
#include "programinfo.h"
#include "mythcontext.h"
#include "remoteencoder.h"

vector<ProgramInfo *> *RemoteGetRecordedList(MythContext *context, bool deltype)
{
    QString str = "QUERY_RECORDINGS ";
    if (deltype)
        str += "Delete";
    else
        str += "Play";

    QStringList strlist = str;

    context->SendReceiveStringList(strlist);

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

void RemoteGetFreeSpace(MythContext *context, int &totalspace, int &usedspace)
{
    QStringList strlist = QString("QUERY_FREESPACE");

    context->SendReceiveStringList(strlist);

    totalspace = strlist[0].toInt();
    usedspace = strlist[1].toInt();
}

bool RemoteGetCheckFile(MythContext *context, const QString &url)
{
    QString str = "QUERY_CHECKFILE " + url;
    QStringList strlist = str;

    context->SendReceiveStringList(strlist);

    bool exists = strlist[0].toInt();
    return exists;
}

void RemoteDeleteRecording(MythContext *context, ProgramInfo *pginfo)
{
    QStringList strlist = QString("DELETE_RECORDING");
    pginfo->ToStringList(strlist);

    context->SendReceiveStringList(strlist);
}

bool RemoteGetAllPendingRecordings(MythContext *context, 
                                   vector<ProgramInfo *> &recordinglist)
{
    QStringList strlist = QString("QUERY_GETALLPENDING");

    context->SendReceiveStringList(strlist);

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

vector<ProgramInfo *> *RemoteGetConflictList(MythContext *context,
                                             ProgramInfo *pginfo,
                                             bool removenonplaying)
{
    QString cmd = QString("QUERY_GETCONFLICTING %1").arg(removenonplaying);
    QStringList strlist = cmd;
    pginfo->ToStringList(strlist);

    context->SendReceiveStringList(strlist);

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

RemoteEncoder *RemoteRequestRecorder(MythContext *context)
{
    QStringList strlist = "GET_FREE_RECORDER";

    context->SendReceiveStringList(strlist);

    int num = strlist[0].toInt();
    QString hostname = strlist[1];
    int port = strlist[2].toInt();

    return new RemoteEncoder(num, hostname, port);
}

RemoteEncoder *RemoteGetExistingRecorder(MythContext *context,
                                         ProgramInfo *pginfo)
{
    QStringList strlist = "GET_RECORDER_NUM";
    pginfo->ToStringList(strlist);

    context->SendReceiveStringList(strlist);

    int num = strlist[0].toInt();
    QString hostname = strlist[1];
    int port = strlist[2].toInt();

    return new RemoteEncoder(num, hostname, port);
}

void RemoteSendMessage(MythContext *context, const QString &message)
{
    QStringList strlist = "MESSAGE";
    strlist << message;

    context->SendReceiveStringList(strlist);
}
