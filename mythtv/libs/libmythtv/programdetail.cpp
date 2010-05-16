#include "programdetail.h"
#include "programinfo.h"

bool GetProgramDetailList(
    QDateTime &nextRecordingStart, bool *hasConflicts, ProgramDetailList *list)
{
    nextRecordingStart = QDateTime();

    bool dummy;
    bool *conflicts = (hasConflicts) ? hasConflicts : &dummy;

    ProgramList progList;
    if (!LoadFromScheduler(progList, *conflicts))
        return false;

    // find the earliest scheduled recording
    ProgramList::const_iterator it = progList.begin();
    for (; it != progList.end(); ++it)
    {
        if (((*it)->GetRecordingStatus() == rsWillRecord) &&
            (nextRecordingStart.isNull() ||
             nextRecordingStart > (*it)->GetRecordingStartTime()))
        {
            nextRecordingStart = (*it)->GetRecordingStartTime();
        }
    }

    if (!list)
        return true;

    // save the details of the earliest recording(s)
    for (it = progList.begin(); it != progList.end(); ++it)
    {
        if (((*it)->GetRecordingStatus()    == rsWillRecord) &&
            ((*it)->GetRecordingStartTime() == nextRecordingStart))
        {
            ProgramDetail prog;
            prog.channame  = (*it)->GetChannelName();
            prog.title     = (*it)->GetTitle();
            prog.subtitle  = (*it)->GetSubtitle();
            prog.startTime = (*it)->GetRecordingStartTime();
            prog.endTime   = (*it)->GetRecordingEndTime();
            list->push_back(prog);
        }
    }

    return true;
}
