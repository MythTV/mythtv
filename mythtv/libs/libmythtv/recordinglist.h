// -*- Mode: c++ -*-

#ifndef _RECORDINGLIST_H_
#define _RECORDINGLIST_H_

// playbackbox.cpp & viewschdiff.cpp viewscheduled::filllist() can
// be more efficient with a linked list everything else is more efficient
// with a deque. With a deque erase() is O(1) only at the front and back
// of the list while the linked list erase() is always O(1), but all other
// operations are faster with the deque.

//#define PGLIST_USE_LINKED_LIST

// C++ headers
#ifdef PGLIST_USE_LINKED_LIST
#include <list>
class RecordingInfo;
typedef std::list<RecordingInfo*> RIList;
#else
#include <deque>
class RecordingInfo;
typedef std::deque<RecordingInfo*> RIList;
#endif
using namespace std;

// Qt headers
#include <QString>

// MythTV headers
#include "mythexp.h"
#include "mythdbcon.h"

class RecordingList;
MPUBLIC bool LoadFromScheduler(
    RecordingList      &destination,
    bool               &hasConflicts,
    QString             altTable = "",
    int                 recordid = -1);

/** \class RecordingList
 *  \brief List of RecordingInfo instances, with helper functions.
 */
class MPUBLIC RecordingList
{
  public:
    RecordingList(bool auto_delete = true) : autodelete(auto_delete) {}
    ~RecordingList();

    typedef RIList::iterator iterator;
    typedef RIList::const_iterator const_iterator;

    RecordingInfo *operator[](uint index);
    const RecordingInfo *operator[](uint index) const;

    RecordingInfo *take(uint i);
    iterator erase(iterator it);
    void clear(void);

    iterator begin(void)             { return pglist.begin(); }
    iterator end(void)               { return pglist.end();   }
    const_iterator begin(void) const { return pglist.begin(); }
    const_iterator end(void)   const { return pglist.end();   }

    void sort(bool (&f)(const RecordingInfo*, const RecordingInfo*));
    bool empty(void) const { return pglist.empty(); }
    size_t size(void) const { return pglist.size(); }
    void push_front(RecordingInfo *pginfo) { pglist.push_front(pginfo); }
    void push_back(RecordingInfo *pginfo) { pglist.push_back(pginfo); }

    // compatibility with old Q3PtrList
    void setAutoDelete(bool auto_delete) { autodelete = auto_delete; }

  protected:
    RIList pglist;
    bool   autodelete;
};

#endif // _RECORDINGLIST_H_

/* vim: set expandtab tabstop=4 shiftwidth=4: */
