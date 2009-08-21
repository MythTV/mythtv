#ifndef REMOTEUTIL_H_
#define REMOTEUTIL_H_

#include <QStringList>
#include <QDateTime>

#include <vector>
using namespace std;

#include "mythexp.h"

class ProgramInfo;

/** \class FileSystemInfo
 *  \brief Holds hostname, total space, and used space in kilobytes.
 */
class MPUBLIC FileSystemInfo
{
  public:
    QString hostname;
    QString directory;
    bool isLocal;
    int fsID;
    int dirID;
    int blocksize;
    long long totalSpaceKB;
    long long usedSpaceKB;
    long long freeSpaceKB;
    int weight;
};

MPUBLIC vector<ProgramInfo *> *RemoteGetRecordedList(bool deltype);
MPUBLIC vector<FileSystemInfo> RemoteGetFreeSpace();
MPUBLIC bool RemoteGetLoad(float load[3]);
MPUBLIC bool RemoteGetUptime(time_t &uptime);
MPUBLIC
bool RemoteGetMemStats(int &totalMB, int &freeMB, int &totalVM, int &freeVM);
MPUBLIC bool RemoteCheckFile(ProgramInfo *pginfo, bool checkSlaves = true);
MPUBLIC
bool RemoteDeleteRecording(ProgramInfo *pginfo, bool forgetHistory,
                           bool forceMetadataDelete = false);
MPUBLIC
bool RemoteUndeleteRecording(const ProgramInfo *pginfo);
MPUBLIC
void RemoteGetAllScheduledRecordings(vector<ProgramInfo *> &scheduledlist);
MPUBLIC
void RemoteGetAllExpiringRecordings(vector<ProgramInfo *> &expiringlist);
MPUBLIC int RemoteGetRecordingList(vector<ProgramInfo *> *reclist,
                                   QStringList &strList);
MPUBLIC vector<ProgramInfo *> *RemoteGetConflictList(const ProgramInfo *pginfo);
MPUBLIC void RemoteSendMessage(const QString &message);
MPUBLIC vector<uint> RemoteRequestFreeRecorderList(void);
MPUBLIC void RemoteGeneratePreviewPixmap(const ProgramInfo *pginfo);
MPUBLIC QDateTime RemoteGetPreviewLastModified(const ProgramInfo *pginfo);
MPUBLIC void RemoteFillProginfo(ProgramInfo *pginfo,
                                const QString &playbackhostname);
MPUBLIC QStringList RemoteRecordings(void);
MPUBLIC int RemoteGetRecordingMask(void);
MPUBLIC int RemoteGetFreeRecorderCount(void);

MPUBLIC int RemoteCheckForRecording(const ProgramInfo *pginfo);
MPUBLIC int RemoteGetRecordingStatus(const ProgramInfo *pginfo, int overrecsecs,
                                     int underrecsecs);
MPUBLIC vector<ProgramInfo *> *RemoteGetCurrentlyRecordingList(void);

#endif

/* vim: set expandtab tabstop=4 shiftwidth=4: */
