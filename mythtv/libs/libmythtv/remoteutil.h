#ifndef REMOTEUTIL_H_
#define REMOTEUTIL_H_

#include <qptrlist.h>
#include <qstring.h>

#include <vector>
#include <qstring.h>
#include <qstringlist.h>
using namespace std;

#include "mythexp.h"

class ProgramInfo;
class RemoteEncoder;
class InputInfo;
class TunedInputInfo;

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
    long long totalSpaceKB;
    long long usedSpaceKB;
    long long liveTVSpaceKB;
    long long freeSpaceKB;
    int weight;
};

/// recording status stuff
class TunerStatus
{
  public:
    uint      id;
    bool      isRecording;
    QString   channame;
    QString   title;
    QString   subtitle;
    QDateTime startTime;
    QDateTime endTime;
};

MPUBLIC uint RemoteGetState(uint cardid);
MPUBLIC uint RemoteGetFlags(uint cardid);
MPUBLIC vector<ProgramInfo *> *RemoteGetRecordedList(bool deltype);
MPUBLIC vector<FileSystemInfo> RemoteGetFreeSpace();
MPUBLIC bool RemoteGetLoad(float load[3]);
MPUBLIC bool RemoteGetUptime(time_t &uptime);
MPUBLIC
bool RemoteGetMemStats(int &totalMB, int &freeMB, int &totalVM, int &freeVM);
MPUBLIC bool RemoteCheckFile(ProgramInfo *pginfo, bool checkSlaves = true);
MPUBLIC bool RemoteRecordPending(
    uint cardid, const ProgramInfo *pginfo, int secsleft, bool hasLater);
MPUBLIC void RemoteStopRecording(ProgramInfo *pginfo);
MPUBLIC bool RemoteStopLiveTV(uint cardid);
MPUBLIC bool RemoteStopRecording(uint cardid);
MPUBLIC
bool RemoteDeleteRecording(ProgramInfo *pginfo, bool forgetHistory,
                           bool forceMetadataDelete = false);
MPUBLIC
bool RemoteUndeleteRecording(ProgramInfo *pginfo);
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
MPUBLIC vector<uint> RemoteRequestFreeRecorderList(void);
MPUBLIC vector<InputInfo> RemoteRequestFreeInputList(
    uint cardid, vector<uint> excluded_cardids);
MPUBLIC InputInfo RemoteRequestBusyInputID(uint cardid);
MPUBLIC void RemoteCancelNextRecording(uint cardid, bool cancel);
MPUBLIC void RemoteGeneratePreviewPixmap(ProgramInfo *pginfo);
MPUBLIC QDateTime RemoteGetPreviewLastModified(ProgramInfo *pginfo);
MPUBLIC void RemoteFillProginfo(ProgramInfo *pginfo,
                                const QString &playbackhostname);
MPUBLIC bool RemoteIsBusy(uint cardid, TunedInputInfo &busy_input);
MPUBLIC QStringList RemoteRecordings(void);
MPUBLIC int RemoteGetRecordingMask(void);
MPUBLIC int RemoteGetFreeRecorderCount(void);

MPUBLIC int RemoteCheckForRecording(ProgramInfo *pginfo);
MPUBLIC int RemoteGetRecordingStatus(ProgramInfo *pginfo, int overrecsecs,
                                     int underrecsecs);
MPUBLIC bool RemoteGetRecordingStatus(
    QPtrList<TunerStatus> *tunerList, bool list_inactive);
MPUBLIC vector<ProgramInfo *> *RemoteGetCurrentlyRecordingList(void);

#endif

/* vim: set expandtab tabstop=4 shiftwidth=4: */
