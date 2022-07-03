#ifndef MYTH_SCHEDULER_H
#define MYTH_SCHEDULER_H

#include <deque>

#include <QStringList>
#include <QMap>

class ProgramInfo;
class RecordingInfo;

using RecList = std::deque<RecordingInfo*>;
using RecConstIter = RecList::const_iterator;
using RecIter = RecList::iterator;

/** This is an generic interface to a program scheduler */
class MythScheduler
{
  public:
    MythScheduler() = default;
    virtual ~MythScheduler() = default;
    // Returns a string list containing the a boolean
    // value for whether there are conflicts, followed
    // by the number of ProgramInfo's represented by
    // the list, followed by ProgramInfo's serialized
    // to string lists.
    virtual void GetAllPending(QStringList &strList) const = 0;
    // Returns all the pending recording with a RecStatus::Recording, RecStatus::Tuning
    // or RecStatus::Failing status (i.e. currently attempting to record.)
    virtual QMap<QString,ProgramInfo*> GetRecording(void) const = 0;
};

#endif // MYTH_SCHEDULER_H
