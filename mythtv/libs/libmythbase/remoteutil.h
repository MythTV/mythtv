#ifndef REMOTEUTIL_H_
#define REMOTEUTIL_H_

#include <array>
#include <vector>

#include <QDateTime>
#include <QString>
#include <QStringList>

#include "libmythbase/mythchrono.h"

#include "mythbaseexp.h"

class ProgramInfo;
class MythEvent;

using system_load_array = std::array<double,3>;

MBASE_PUBLIC std::vector<ProgramInfo *> *RemoteGetRecordedList(int sort);
MBASE_PUBLIC bool RemoteGetLoad(system_load_array &load);
MBASE_PUBLIC bool RemoteGetUptime(std::chrono::seconds &uptime);
MBASE_PUBLIC
bool RemoteGetMemStats(int &totalMB, int &freeMB, int &totalVM, int &freeVM);
MBASE_PUBLIC bool RemoteCheckFile(
    ProgramInfo *pginfo, bool checkSlaves = true);
MBASE_PUBLIC bool RemoteDeleteRecording( uint recordingID, bool forceMetadataDelete,
                                    bool forgetHistory);
MBASE_PUBLIC
bool RemoteUndeleteRecording(uint recordingID);
MBASE_PUBLIC
void RemoteGetAllScheduledRecordings(std::vector<ProgramInfo *> &scheduledlist);
MBASE_PUBLIC
void RemoteGetAllExpiringRecordings(std::vector<ProgramInfo *> &expiringlist);
MBASE_PUBLIC uint RemoteGetRecordingList(std::vector<ProgramInfo *> &reclist,
                                    QStringList &strList);
MBASE_PUBLIC std::vector<ProgramInfo *> *RemoteGetConflictList(const ProgramInfo *pginfo);
MBASE_PUBLIC QDateTime RemoteGetPreviewLastModified(const ProgramInfo *pginfo);
MBASE_PUBLIC QDateTime RemoteGetPreviewIfModified(
    const ProgramInfo &pginfo, const QString &cachefile);
MBASE_PUBLIC bool RemoteFillProgramInfo(
    ProgramInfo &pginfo, const QString &playbackhostname);
MBASE_PUBLIC QStringList RemoteRecordings(void);
MBASE_PUBLIC int RemoteGetRecordingMask(void);

MBASE_PUBLIC int RemoteCheckForRecording(const ProgramInfo *pginfo);
MBASE_PUBLIC int RemoteGetRecordingStatus(const ProgramInfo *pginfo, int overrecsecs,
                                     int underrecsecs);
MBASE_PUBLIC std::vector<ProgramInfo *> *RemoteGetCurrentlyRecordingList(void);

MBASE_PUBLIC bool RemoteGetFileList(const QString& host, const QString& path, QStringList* list,
                       QString sgroup, bool fileNamesOnly = false);
MBASE_PUBLIC bool RemoteGetActiveBackends(QStringList *list);

MBASE_PUBLIC QString RemoteDownloadFile(const QString &url,
                                   const QString &storageGroup,
                                   const QString &filename = "");
MBASE_PUBLIC QString RemoteDownloadFileNow(const QString &url,
                                      const QString &storageGroup,
                                      const QString &filename = "");

#endif // REMOTEUTIL_H_

/* vim: set expandtab tabstop=4 shiftwidth=4: */
