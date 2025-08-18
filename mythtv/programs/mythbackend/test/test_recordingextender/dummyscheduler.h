#ifndef MYTHBACKEND_TEST_DUMMYSCHEDULER_H
#define MYTHBACKEND_TEST_DUMMYSCHEDULER_H

#include "libmythbase/mthread.h"
#include "../../scheduler.h"
#include "libmythtv/recordinginfo.h"

class TestScheduler : public Scheduler
{
public:
    TestScheduler() : Scheduler(false, nullptr) {};

    // These have to match the signatures of the real scheduler.
    QMap<QString,ProgramInfo*> GetRecording(void) const override;
    RecordingInfo* GetRecording(uint recordedid) const override;
};

#endif // MYTHBACKEND_TEST_DUMMYSCHEDULER_H
