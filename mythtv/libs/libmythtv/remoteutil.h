#ifndef REMOTEUTIL_H_
#define REMOTEUTIL_H_

#include <vector>
using namespace std;

class ProgramInfo;
class MythContext;
class RemoteEncoder;

vector<ProgramInfo *> *RemoteGetRecordedList(MythContext *context, 
                                             bool deltype);

void RemoteGetFreeSpace(MythContext *context, int &totalspace, int &usedspace);

bool RemoteGetCheckFile(MythContext *context, const QString &url);

void RemoteDeleteRecording(MythContext *context, ProgramInfo *pginfo);

bool RemoteGetAllPendingRecordings(MythContext *context, 
                                   vector<ProgramInfo *> &recordinglist);

vector<ProgramInfo *> *RemoteGetConflictList(MythContext *context, 
                                             ProgramInfo *pginfo,
                                             bool removenonplaying);

void RemoteSendMessage(MythContext *context, const QString &message);

RemoteEncoder *RemoteRequestRecorder(MythContext *context);
RemoteEncoder *RemoteGetExistingRecorder(MythContext *context,
                                         ProgramInfo *pginfo);

#endif
