#ifndef PROGRAMINFO_REMOTEUTIL_H
#define PROGRAMINFO_REMOTEUTIL_H

#include <vector>

#include <QDateTime>
#include <QString>
#include <QStringList>

#include "mythtvexp.h"

class ProgramInfo;

MTV_PUBLIC bool RemoteDeleteRecording( uint recordingID, bool forceMetadataDelete,
                                    bool forgetHistory);
MTV_PUBLIC bool RemoteUndeleteRecording(uint recordingID);

MTV_PUBLIC std::vector<ProgramInfo *> *RemoteGetRecordedList(int sort);
MTV_PUBLIC void RemoteGetAllScheduledRecordings(std::vector<ProgramInfo *> &scheduledlist);
MTV_PUBLIC void RemoteGetAllExpiringRecordings(std::vector<ProgramInfo *> &expiringlist);
MTV_PUBLIC std::vector<ProgramInfo *> *RemoteGetConflictList(const ProgramInfo *pginfo);

MTV_PUBLIC QDateTime RemoteGetPreviewIfModified(const ProgramInfo &pginfo,
                                                const QString &cachefile);

#endif // PROGRAMINFO_REMOTEUTIL_H

/* vim: set expandtab tabstop=4 shiftwidth=4: */
