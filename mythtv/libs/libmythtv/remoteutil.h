#ifndef REMOTEUTIL_H_
#define REMOTEUTIL_H_

#include <vector>
using namespace std;

class ProgramInfo;
class RemoteEncoder;

vector<ProgramInfo *> *RemoteGetRecordedList(bool deltype);
void RemoteGetFreeSpace(int &totalspace, int &usedspace);
bool RemoteCheckFile(ProgramInfo *pginfo);
void RemoteDeleteRecording(ProgramInfo *pginfo);
bool RemoteGetAllPendingRecordings(vector<ProgramInfo *> &recordinglist);
vector<ProgramInfo *> *RemoteGetConflictList(ProgramInfo *pginfo,
                                             bool removenonplaying);
void RemoteSendMessage(const QString &message);
RemoteEncoder *RemoteRequestRecorder(void);
RemoteEncoder *RemoteGetExistingRecorder(ProgramInfo *pginfo);
void RemoteGeneratePreviewPixmap(ProgramInfo *pginfo);

#endif
