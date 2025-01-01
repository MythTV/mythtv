#include "dummyscheduler.h"

QMap<uint,RecordingInfo*> gFakeRecordings;

// Dummy functions so we don't have to link against scheduler.o, which
// pulls in a ton of other requirements.
Scheduler::Scheduler([[maybe_unused]] bool runthread,
                     [[maybe_unused]] QMap<int, EncoderLink *> *_tvList,
                     [[maybe_unused]] const QString& tmptable,
                     [[maybe_unused]] Scheduler *master_sched) :
    MThread("Scheduler")
{}
// This is not the real scheduler destructor, so we can't use the
// =default notation.  NOLINTNEXTLINE(modernize-use-equals-default)
Scheduler::~Scheduler()
{
}
void Scheduler::GetAllPending([[maybe_unused]] QStringList &strList) const
{
}
QMap<QString,ProgramInfo*> Scheduler::GetRecording(void) const
{
    return {};
};
RecordingInfo* Scheduler::GetRecording([[maybe_unused]] uint recordedid) const
{
    return nullptr;
};
void Scheduler::run(void)
{
}

//
// Our test functions.
//
QMap<QString,ProgramInfo*> TestScheduler::GetRecording(void) const
{
    return {};
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
RecordingInfo* TestScheduler::GetRecording(uint recordedid) const
{
    if (recordedid == 0)
        return nullptr;
    if (!gFakeRecordings.contains(recordedid))
        gFakeRecordings.insert(recordedid, new RecordingInfo);
    return gFakeRecordings[recordedid];
}
