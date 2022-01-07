#ifndef REMOTEUTIL_H_
#define REMOTEUTIL_H_

#include <ctime>

#include <QStringList>
#include <QDateTime>

#include <array>
#include <vector>

#include "mythexp.h"

class ProgramInfo;
class MythEvent;

using system_load_array = std::array<double,3>;

MPUBLIC std::vector<ProgramInfo *> *RemoteGetRecordedList(int sort);
MPUBLIC bool RemoteGetLoad(system_load_array &load);
MPUBLIC bool RemoteGetUptime(std::chrono::seconds &uptime);
MPUBLIC
bool RemoteGetMemStats(int &totalMB, int &freeMB, int &totalVM, int &freeVM);
MPUBLIC bool RemoteCheckFile(
    ProgramInfo *pginfo, bool checkSlaves = true);
MPUBLIC bool RemoteDeleteRecording( uint recordingID, bool forceMetadataDelete,
                                    bool forgetHistory);
MPUBLIC
bool RemoteUndeleteRecording(uint recordingID);
MPUBLIC
void RemoteGetAllScheduledRecordings(std::vector<ProgramInfo *> &scheduledlist);
MPUBLIC
void RemoteGetAllExpiringRecordings(std::vector<ProgramInfo *> &expiringlist);
MPUBLIC uint RemoteGetRecordingList(std::vector<ProgramInfo *> &reclist,
                                    QStringList &strList);
MPUBLIC std::vector<ProgramInfo *> *RemoteGetConflictList(const ProgramInfo *pginfo);
MPUBLIC QDateTime RemoteGetPreviewLastModified(const ProgramInfo *pginfo);
MPUBLIC QDateTime RemoteGetPreviewIfModified(
    const ProgramInfo &pginfo, const QString &cachefile);
MPUBLIC bool RemoteFillProgramInfo(
    ProgramInfo &pginfo, const QString &playbackhostname);
MPUBLIC QStringList RemoteRecordings(void);
MPUBLIC int RemoteGetRecordingMask(void);

MPUBLIC int RemoteCheckForRecording(const ProgramInfo *pginfo);
MPUBLIC int RemoteGetRecordingStatus(const ProgramInfo *pginfo, int overrecsecs,
                                     int underrecsecs);
MPUBLIC std::vector<ProgramInfo *> *RemoteGetCurrentlyRecordingList(void);

MPUBLIC bool RemoteGetFileList(const QString& host, const QString& path, QStringList* list,
                       QString sgroup, bool fileNamesOnly = false);
MPUBLIC bool RemoteGetActiveBackends(QStringList *list);

#endif

/* vim: set expandtab tabstop=4 shiftwidth=4: */
