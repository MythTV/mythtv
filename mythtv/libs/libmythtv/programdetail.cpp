#include "programdetail.h"
#include "programlist.h"
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
        if (((*it)->recstatus == rsWillRecord) &&
            (nextRecordingStart.isNull() ||
             nextRecordingStart > (*it)->recstartts))
        {
            nextRecordingStart = (*it)->recstartts;
        }
    }

    if (!list)
        return true;

    // save the details of the earliest recording(s)
    for (it = progList.begin(); it != progList.end(); ++it)
    {
        if (((*it)->recstatus  == rsWillRecord) &&
            ((*it)->recstartts == nextRecordingStart))
        {
            ProgramDetail prog;
            prog.channame  = (*it)->channame;
            prog.title     = (*it)->title;
            prog.subtitle  = (*it)->subtitle;
            prog.startTime = (*it)->recstartts;
            prog.endTime   = (*it)->recendts;
            list->push_back(prog);
        }
    }

    return true;
}
