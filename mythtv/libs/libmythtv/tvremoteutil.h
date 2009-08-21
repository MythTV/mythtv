#ifndef _TV_REMOTE_UTIL_H_
#define _TV_REMOTE_UTIL_H_

#include <QStringList>
#include <QDateTime>

#include <vector>
using namespace std;

#include "mythexp.h"

class ProgramInfo;
class RemoteEncoder;
class InputInfo;
class TunedInputInfo;

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
MPUBLIC bool RemoteRecordPending(
    uint cardid, const ProgramInfo *pginfo, int secsleft, bool hasLater);
MPUBLIC bool RemoteStopLiveTV(uint cardid);
MPUBLIC bool RemoteStopRecording(uint cardid);
MPUBLIC void RemoteStopRecording(const ProgramInfo *pginfo);
MPUBLIC void RemoteCancelNextRecording(uint cardid, bool cancel);
MPUBLIC RemoteEncoder *RemoteRequestRecorder(void);
MPUBLIC RemoteEncoder *RemoteRequestNextFreeRecorder(int curr);
MPUBLIC
RemoteEncoder *RemoteRequestFreeRecorderFromList(
    const QStringList &qualifiedRecorders);
MPUBLIC RemoteEncoder *RemoteGetExistingRecorder(const ProgramInfo *pginfo);
MPUBLIC RemoteEncoder *RemoteGetExistingRecorder(int recordernum);
MPUBLIC vector<InputInfo> RemoteRequestFreeInputList(
    uint cardid, const vector<uint> &excluded_cardids);
MPUBLIC InputInfo RemoteRequestBusyInputID(uint cardid);
MPUBLIC bool RemoteIsBusy(uint cardid, TunedInputInfo &busy_input);

MPUBLIC bool RemoteGetRecordingStatus(
    vector<TunerStatus> *tunerList, bool list_inactive);

#endif // _TV_REMOTE_UTIL_H_

/* vim: set expandtab tabstop=4 shiftwidth=4: */
