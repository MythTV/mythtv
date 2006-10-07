#ifndef REMOTEUTIL_H_
#define REMOTEUTIL_H_

#include <vector>
#include <qstring.h>
using namespace std;

#include "mythexp.h"

class ProgramInfo;
class RemoteEncoder;

/** \class FileSystemInfo
 *  \brief Holds hostname, total space, and used space in kilobytes.
 */
class MPUBLIC FileSystemInfo
{
  public:
    QString hostname;
    long long totalSpaceKB;
    long long usedSpaceKB;
};

MPUBLIC vector<ProgramInfo *> *RemoteGetRecordedList(bool deltype);
MPUBLIC vector<FileSystemInfo> RemoteGetFreeSpace();
MPUBLIC bool RemoteGetLoad(float load[3]);
MPUBLIC bool RemoteGetUptime(time_t &uptime);
MPUBLIC
bool RemoteGetMemStats(int &totalMB, int &freeMB, int &totalVM, int &freeVM);
MPUBLIC bool RemoteCheckFile(ProgramInfo *pginfo);
MPUBLIC void RemoteStopRecording(ProgramInfo *pginfo);
MPUBLIC
bool RemoteDeleteRecording(ProgramInfo *pginfo, bool forgetHistory,
                           bool forceMetadataDelete = false);
MPUBLIC
void RemoteGetAllScheduledRecordings(vector<ProgramInfo *> &scheduledlist);
MPUBLIC
void RemoteGetAllExpiringRecordings(vector<ProgramInfo *> &expiringlist);
MPUBLIC int RemoteGetRecordingList(vector<ProgramInfo *> *reclist,
                                   QStringList &strList);
MPUBLIC vector<ProgramInfo *> *RemoteGetConflictList(ProgramInfo *pginfo);
MPUBLIC void RemoteSendMessage(const QString &message);
MPUBLIC RemoteEncoder *RemoteRequestRecorder(void);
MPUBLIC RemoteEncoder *RemoteRequestNextFreeRecorder(int curr);
MPUBLIC
RemoteEncoder *RemoteRequestFreeRecorderFromList(QStringList &qualifiedRecorders);
MPUBLIC RemoteEncoder *RemoteGetExistingRecorder(ProgramInfo *pginfo);
MPUBLIC RemoteEncoder *RemoteGetExistingRecorder(int recordernum);
MPUBLIC void RemoteGeneratePreviewPixmap(ProgramInfo *pginfo);
MPUBLIC QString RemoteGetPreviewLastModified(ProgramInfo *pginfo);
MPUBLIC void RemoteFillProginfo(ProgramInfo *pginfo,
                                const QString &playbackhostname);
MPUBLIC int RemoteIsRecording(void);
MPUBLIC int RemoteGetRecordingMask(void);
MPUBLIC int RemoteGetFreeRecorderCount(void);

MPUBLIC int RemoteCheckForRecording(ProgramInfo *pginfo);
MPUBLIC int RemoteGetRecordingStatus(ProgramInfo *pginfo, int overrecsecs,
                                     int underrecsecs);

#endif
