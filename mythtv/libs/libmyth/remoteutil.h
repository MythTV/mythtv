#ifndef REMOTEUTIL_H_
#define REMOTEUTIL_H_

#include <QStringList>
#include <QDateTime>

#include <vector>
using namespace std;

#include "mythexp.h"

class ProgramInfo;
class MythEvent;

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
MPUBLIC vector<FileSystemInfo> RemoteGetFreeSpace(void);
MPUBLIC bool RemoteGetLoad(float load[3]);
MPUBLIC bool RemoteGetUptime(time_t &uptime);
MPUBLIC
bool RemoteGetMemStats(int &totalMB, int &freeMB, int &totalVM, int &freeVM);
MPUBLIC bool RemoteCheckFile(
    const ProgramInfo *pginfo, bool checkSlaves = true);
MPUBLIC bool RemoteDeleteRecording(
    uint chanid, const QDateTime &recstartts, bool forceMetadataDelete,
    bool forgetHistory);
MPUBLIC
bool RemoteUndeleteRecording(uint chanid, const QDateTime &recstartts);
MPUBLIC
void RemoteGetAllScheduledRecordings(vector<ProgramInfo *> &scheduledlist);
MPUBLIC
void RemoteGetAllExpiringRecordings(vector<ProgramInfo *> &expiringlist);
MPUBLIC uint RemoteGetRecordingList(vector<ProgramInfo *> &reclist,
                                    QStringList &strList);
MPUBLIC vector<ProgramInfo *> *RemoteGetConflictList(const ProgramInfo *pginfo);
MPUBLIC void RemoteSendMessage(const QString &message);
MPUBLIC void RemoteSendEvent(const MythEvent &event);
MPUBLIC vector<uint> RemoteRequestFreeRecorderList(void);
MPUBLIC QDateTime RemoteGetPreviewLastModified(const ProgramInfo *pginfo);
MPUBLIC QDateTime RemoteGetPreviewIfModified(
    const ProgramInfo &pginfo, const QString &cachefile);
MPUBLIC bool RemoteFillProgramInfo(
    ProgramInfo &pginfo, const QString &playbackhostname);
MPUBLIC QStringList RemoteRecordings(void);
MPUBLIC int RemoteGetRecordingMask(void);
MPUBLIC int RemoteGetFreeRecorderCount(void);

MPUBLIC int RemoteCheckForRecording(const ProgramInfo *pginfo);
MPUBLIC int RemoteGetRecordingStatus(const ProgramInfo *pginfo, int overrecsecs,
                                     int underrecsecs);
MPUBLIC vector<ProgramInfo *> *RemoteGetCurrentlyRecordingList(void);

MPUBLIC bool RemoteGetFileList(QString host, QString path, QStringList* list,
                       QString sgroup, bool fileNamesOnly = false);

#endif

/* vim: set expandtab tabstop=4 shiftwidth=4: */
