// -*- Mode: c++ -*-

// MythTV headers
#include "mythcorecontext.h"
#include "mythdb.h"
#include "util.h"
#include "recordinginfo.h"
#include "recordinglist.h"
#include "jobqueue.h"

RecordingList::~RecordingList(void)
{
    clear();
}

RecordingInfo *RecordingList::operator[](uint index)
{
#ifdef PGLIST_USE_LINKED_LIST
    iterator it = pglist.begin();
    for (uint i = 0; i < index; i++, it++)
    {
        if (it == pglist.end())
            return NULL;
    }
    if (it == pglist.end())
        return NULL;
    return *it;
#else
    if (index < pglist.size())
        return pglist[index];
    return NULL;
#endif
}

const RecordingInfo *RecordingList::operator[](uint index) const
{
    return (*(const_cast<RecordingList*>(this)))[index];
}

RecordingInfo *RecordingList::take(uint index)
{
#ifndef PGLIST_USE_LINKED_LIST
    if (pglist.begin() == pglist.end())
        return NULL;

    if (0 == index)
    {
        RecordingInfo *pginfo = pglist.front();
        pglist.pop_front();
        return pginfo;
    }

    if (pglist.size() == (index + 1))
    {
        RecordingInfo *pginfo = pglist.back();
        pglist.pop_back();
        return pginfo;
    }
#endif

    iterator it = pglist.begin();
    for (uint i = 0; i < index; i++, it++)
    {
        if (it == pglist.end())
            return NULL;
    }
    RecordingInfo *pginfo = *it;
    pglist.erase(it);
    return pginfo;
}

RecordingList::iterator RecordingList::erase(iterator it)
{
    if (autodelete)
        delete *it;
    return pglist.erase(it);
}

void RecordingList::clear(void)
{
    if (autodelete)
    {
        iterator it = pglist.begin();
        for (; it != pglist.end(); ++it)
            delete *it;
    }
    pglist.clear();
}

void RecordingList::sort(bool (&f)(const RecordingInfo*, const RecordingInfo*))
{
#ifdef PGLIST_USE_LINKED_LIST
    pglist.sort(f);
#else
    stable_sort(begin(), end(), f);
#endif
}

bool LoadFromScheduler(
    RecordingList &destination, bool &hasConflicts,
    QString tmptable, int recordid)
{
    destination.clear();
    hasConflicts = false;

    if (gCoreContext->IsBackend())
        return false;

    QString query;
    if (!tmptable.isEmpty())
    {
        query = QString("QUERY_GETALLPENDING %1 %2")
            .arg(tmptable).arg(recordid);
    }
    else
    {
        query = QString("QUERY_GETALLPENDING");
    }

    QStringList slist( query );
    if (!gCoreContext->SendReceiveStringList(slist) || slist.size() < 2)
    {
        VERBOSE(VB_IMPORTANT,
                "LoadFromScheduler(): Error querying master.");
        return false;
    }

    hasConflicts = slist[0].toInt();

    bool result = true;
    QStringList::const_iterator sit = slist.begin()+2;

    while (result && sit != slist.end())
    {
        RecordingInfo *p = new RecordingInfo();
        result = p->FromStringList(sit, slist.end());
        if (result)
            destination.push_back(p);
        else
            delete p;
    }

    if (destination.size() != slist[1].toUInt())
    {
        VERBOSE(VB_IMPORTANT,
                "LoadFromScheduler(): Length mismatch.");
        destination.clear();
        result = false;
    }

    return result;
}
