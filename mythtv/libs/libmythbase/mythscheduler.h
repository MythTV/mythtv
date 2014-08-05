#ifndef _MYTH_SCHEDULER_H_
#define _MYTH_SCHEDULER_H_

#include <algorithm>
#include <deque>

#include <QStringList>
#include <QMap>

class ProgramInfo;
class RecordingInfo;

typedef std::deque<RecordingInfo*> RecList;
#define SORT_RECLIST(LIST, ORDER) \
  do { std::stable_sort((LIST).begin(), (LIST).end(), ORDER); } while (0)

typedef RecList::const_iterator RecConstIter;
typedef RecList::iterator RecIter;

/** This is an generic interface to a program scheduler */
class MythScheduler
{
  public:
    MythScheduler() {}
    virtual ~MythScheduler() {}
    // Returns a string list containing the a boolean
    // value for whether there are conflicts, followed
    // by the number of ProgramInfo's represented by
    // the list, followed by ProgramInfo's serialized
    // to string lists.
    virtual void GetAllPending(QStringList &strList) const = 0;
    // Returns all the pending recording with a rsRecording, rsTuning
    // or rsFailing status (i.e. currently attempting to record.)
    virtual QMap<QString,ProgramInfo*> GetRecording(void) const = 0;
};

#endif
