#include "mythscheduler.h"
#include "mthread.h"
#include "recordinginfo.h"

QMap<uint,RecordingInfo*> gFakeRecordings;

class Scheduler : public MThread, public MythScheduler
{
    // These have to match the signatures of the real scheduler.
    QMap<QString,ProgramInfo*> GetRecording(void) const override;
    RecordingInfo* GetRecording(uint recordedid) const;
};

QMap<QString,ProgramInfo*> Scheduler::GetRecording(void) const
{
    return {};
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
RecordingInfo* Scheduler::GetRecording(uint recordedid) const
{
    if (recordedid == 0)
        return nullptr;
    if (!gFakeRecordings.contains(recordedid))
        gFakeRecordings.insert(recordedid, new RecordingInfo);
    return gFakeRecordings[recordedid];
}
