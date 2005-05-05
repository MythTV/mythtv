#ifndef REMOTEUTIL_H_
#define REMOTEUTIL_H_

#include <vector>
#include <qstring.h>
using namespace std;

class ProgramInfo;
class RemoteEncoder;

vector<ProgramInfo *> *RemoteGetRecordedList(bool deltype);
void RemoteGetFreeSpace(int &totalspace, int &usedspace);
bool RemoteGetLoad(float load[3]);
bool RemoteGetUptime(time_t &uptime);
bool RemoteGetMemStats(int &totalMB, int &freeMB, int &totalVM, int &freeVM);
bool RemoteCheckFile(ProgramInfo *pginfo);
void RemoteStopRecording(ProgramInfo *pginfo);
bool RemoteDeleteRecording(ProgramInfo *pginfo, bool forgetHistory,
                           bool forceMetadataDelete = false);
bool RemoteReactivateRecording(ProgramInfo *pginfo);
void RemoteGetAllScheduledRecordings(vector<ProgramInfo *> &scheduledlist);
vector<ProgramInfo *> *RemoteGetConflictList(ProgramInfo *pginfo);
void RemoteSendMessage(const QString &message);
RemoteEncoder *RemoteRequestRecorder(void);
RemoteEncoder *RemoteRequestNextFreeRecorder(int curr);
RemoteEncoder *RemoteGetExistingRecorder(ProgramInfo *pginfo);
RemoteEncoder *RemoteGetExistingRecorder(int recordernum);
void RemoteGeneratePreviewPixmap(ProgramInfo *pginfo);
QString RemoteGetPreviewLastModified(ProgramInfo *pginfo);
void RemoteFillProginfo(ProgramInfo *pginfo, const QString &playbackhostname);
int RemoteIsRecording(void);
int RemoteGetRecordingMask(void);
int RemoteGetFreeRecorderCount(void);

int RemoteCheckForRecording(ProgramInfo *pginfo);
int RemoteGetRecordingStatus(ProgramInfo *pginfo, int overrecsecs, 
                             int underrecsecs);

#endif
