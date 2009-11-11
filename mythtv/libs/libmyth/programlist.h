// -*- Mode: c++ -*-

#ifndef _PROGRAMLIST_H_
#define _PROGRAMLIST_H_

// playbackbox.cpp & viewschdiff.cpp viewscheduled::filllist() can
// be more efficient with a linked list everything else is more efficient
// with a deque. With a deque erase() is O(1) only at the front and back
// of the list while the linked list erase() is always O(1), but all other
// operations are faster with the deque.

//#define PGLIST_USE_LINKED_LIST

// C++ headers
#ifdef PGLIST_USE_LINKED_LIST
#include <list>
class ProgramInfo;
typedef std::list<ProgramInfo*> PGList;
#else
#include <deque>
class ProgramInfo;
typedef std::deque<ProgramInfo*> PGList;
#endif
using namespace std;

// Qt headers
#include <QString>

// MythTV headers
#include "mythexp.h"
#include "mythdbcon.h"

class ProgramList;
MPUBLIC bool LoadFromProgram(
    ProgramList        &destination,
    const QString      &sql,
    const MSqlBindings &bindings,
    const ProgramList  &schedList,
    bool                oneChanid);


MPUBLIC bool LoadFromOldRecorded(
    ProgramList        &destination,
    const QString      &sql,
    const MSqlBindings &bindings);

MPUBLIC bool LoadFromRecorded(
    ProgramList        &destination,
    bool                orderDescending, 
    bool                possiblyInProgressRecordingsOnly,
    const ProgramList  &schedList);

MPUBLIC bool LoadFromScheduler(
    ProgramList        &destination,
    bool               &hasConflicts,
    QString             altTable = "",
    int                 recordid = -1);

MPUBLIC bool LoadFromScheduler(ProgramList &destination);

/** \class ProgramList
 *  \brief List of ProgramInfo instances, with helper functions.
 */
class MPUBLIC ProgramList
{
    friend class RecordingList;
  public:
    ProgramList(bool auto_delete = true) : autodelete(auto_delete) {}
    ~ProgramList();

    typedef PGList::iterator iterator;
    typedef PGList::const_iterator const_iterator;

    ProgramInfo *operator[](uint index);
    const ProgramInfo *operator[](uint index) const;

    ProgramInfo *take(uint i);
    iterator erase(iterator it);
    void clear(void);

    iterator begin(void)             { return pglist.begin(); }
    iterator end(void)               { return pglist.end();   }
    const_iterator begin(void) const { return pglist.begin(); }
    const_iterator end(void)   const { return pglist.end();   }

    void sort(bool (&f)(const ProgramInfo*, const ProgramInfo*));
    bool empty(void) const { return pglist.empty(); }
    size_t size(void) const { return pglist.size(); }
    void push_front(ProgramInfo *pginfo) { pglist.push_front(pginfo); }
    void push_back(ProgramInfo *pginfo) { pglist.push_back(pginfo); }

    // compatibility with old Q3PtrList
    void setAutoDelete(bool auto_delete) { autodelete = auto_delete; }

  protected:
    PGList pglist;
    bool   autodelete;
};

#endif // _PROGRAMLIST_H_

/* vim: set expandtab tabstop=4 shiftwidth=4: */
