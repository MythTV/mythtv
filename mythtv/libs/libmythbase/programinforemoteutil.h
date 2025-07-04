#ifndef PROGRAMINFO_REMOTEUTIL_H
#define PROGRAMINFO_REMOTEUTIL_H

#include <vector>

#include <QDateTime>
#include <QString>
#include <QStringList>

#include "mythbaseexp.h"

class ProgramInfo;

MBASE_PUBLIC std::vector<ProgramInfo *> *RemoteGetRecordedList(int sort);
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

MBASE_PUBLIC int RemoteCheckForRecording(const ProgramInfo *pginfo);
MBASE_PUBLIC int RemoteGetRecordingStatus(const ProgramInfo *pginfo, int overrecsecs,
                                     int underrecsecs);
MBASE_PUBLIC std::vector<ProgramInfo *> *RemoteGetCurrentlyRecordingList(void);

#endif // PROGRAMINFO_REMOTEUTIL_H

/* vim: set expandtab tabstop=4 shiftwidth=4: */
