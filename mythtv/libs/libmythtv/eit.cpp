// -*- Mode: c++ -*-

// C++ includes
#include <algorithm>
using namespace std;

// Qt includes
#include <qdeepcopy.h>

// MythTV includes
#include "mythdbcon.h"
#include "eit.h"

void Event::Reset()
{
    ServiceID = 0;
    TransportID = 0;
    NetworkID = 0;
    ATSC = false;
    clearEventValues();
}

void Event::deepCopy(const Event &e)
{
    SourcePID       = e.SourcePID;
    TransportID     = e.TransportID;
    NetworkID       = e.NetworkID;
    ServiceID       = e.ServiceID;
    EventID         = e.EventID;
    StartTime       = e.StartTime;
    EndTime         = e.EndTime;
    OriginalAirDate = e.OriginalAirDate;
    Stereo          = e.Stereo;
    HDTV            = e.HDTV;
    SubTitled       = e.SubTitled;
    ETM_Location    = e.ETM_Location;
    ATSC            = e.ATSC;
    PartNumber      = e.PartNumber;
    PartTotal       = e.PartTotal;

    // QString is not thread safe, so we must make deep copies
    LanguageCode       = QDeepCopy<QString>(e.LanguageCode);
    Event_Name         = QDeepCopy<QString>(e.Event_Name);
    Event_Subtitle     = QDeepCopy<QString>(e.Event_Subtitle);
    Description        = QDeepCopy<QString>(e.Description);
    ContentDescription = QDeepCopy<QString>(e.ContentDescription);
    Year               = QDeepCopy<QString>(e.Year);
    CategoryType       = QDeepCopy<QString>(e.CategoryType);
    Actors             = QDeepCopy<QStringList>(e.Actors);

    Credits.clear();
    QValueList<Person>::const_iterator pit = e.Credits.begin();
    for (; pit != e.Credits.end(); ++pit)
        Credits.push_back(Person(QDeepCopy<QString>((*pit).role),
                                 QDeepCopy<QString>((*pit).name)));
}

void Event::clearEventValues()
{
    SourcePID    = 0;
    LanguageCode = "";
    Event_Name   = "";
    Description  = "";
    EventID      = 0;
    ETM_Location = 0;
    Event_Subtitle     = "";
    ContentDescription = "";
    Year         = "";
    SubTitled    = false;
    Stereo       = false;
    HDTV         = false;
    ATSC         = false;
    PartNumber   = 0;
    PartTotal    = 0;
    CategoryType = "";
    OriginalAirDate = QDate();
    Actors.clear();
    Credits.clear();
}

uint DBEvent::UpdateDB(MSqlQuery &query, int match_threshold) const
{
    vector<DBEvent> programs;
    uint count = GetOverlappingPrograms(query, programs);
    int  match = INT_MIN;
    int  i     = -1;

    if (count)
        match = GetMatch(programs, i);

    if ((match < match_threshold) && (i >= 0))
    {
        VERBOSE(VB_IMPORTANT, QString("match[%1]: %2 '%3' vs. '%4'")
                .arg(i).arg(match).arg(title).arg(programs[i].title));
    }

    if (match >= match_threshold)
        return UpdateDB(query, programs, i);
    else if (!count)
        return InsertDB(query);
    else
        return UpdateDB(query, programs, -1);
}

uint DBEvent::GetOverlappingPrograms(MSqlQuery &query,
                                     vector<DBEvent> &programs) const
{
    uint count = 0;
    query.prepare(
        "SELECT title,     subtitle, description,           "
        "       starttime, endtime,  closecaptioned, stereo "
        "FROM program "
        "WHERE chanid   = :CHANID AND "
        "      manualid = 0       AND "
        "      ( ( starttime >= :STIME1 AND starttime <  :ETIME1 ) OR "
        "        ( endtime   >  :STIME2 AND endtime   <= :ETIME2 ) )");
    query.bindValue(":CHANID", chanid);
    query.bindValue(":STIME1", starttime);
    query.bindValue(":ETIME1", endtime);
    query.bindValue(":STIME2", starttime);
    query.bindValue(":ETIME2", endtime);

    if (!query.exec() || !query.isActive())
    {
        MythContext::DBError("GetOverlappingPrograms 1", query);
        return 0;
    }

    while (query.next())
    {
        DBEvent prog(chanid,
                     query.value(0).toString(),   query.value(1).toString(),
                     query.value(2).toString(),
                     query.value(3).toDateTime(), query.value(4).toDateTime(),
                     fixup,
                     query.value(5).toBool(),     query.value(6).toBool());

        programs.push_back(prog);
        count++;
    }

    return count;
}

static int score_match(const QString &a, const QString &b)
{
    if (a.isEmpty() || b.isEmpty())
        return 0;
    else if (a == b)
        return 1000;

    QString A = a.stripWhiteSpace().upper();
    QString B = b.stripWhiteSpace().upper();
    if (A == B)
        return 1000;

    QStringList al, bl;
    al.split("", A);
    if (!al.size())
        return 0;

    bl.split("", B);
    if (!bl.size())
        return 0;

    QStringList::const_iterator ait = al.begin();
    QStringList::const_iterator bit = bl.begin();
    int score = 0;
    for (; (ait != al.end()) && (bit != bl.end()); ait++)
    {
        QStringList::const_iterator bit2 = bit;
        int dist = 0;
        int bscore = 0;
        for (; bit2 != bl.end(); bit2++)
        {
            if (*ait == *bit)
            {
                bscore = max(1000, 2000 - (dist * 500));
                break;
            }
            dist++;
        }
        if (bscore && dist < 3)
        {
            for (int i = 0; (i < dist) && bit != bl.end(); i++)
                bit++;
        }
        score += bscore;
    }
    score /= al.size();

    return max(1000, score);
}

int DBEvent::GetMatch(const vector<DBEvent> &programs, int &bestmatch) const
{
    bestmatch = -1;
    int match_val = INT_MIN;

    for (uint i = 0; i < programs.size(); i++)
    {
        int mv = 0;
        mv -= abs(starttime.secsTo(programs[i].starttime));
        mv -= abs(endtime.secsTo(programs[i].endtime));
        mv += score_match(title, programs[i].title) * 10;
        mv += score_match(subtitle, programs[i].subtitle);
        mv += score_match(description, programs[i].description);

        if (mv > match_val)
        {
            bestmatch = i;
            match_val = mv;
        }
    }

    return match_val;
}

uint DBEvent::UpdateDB(MSqlQuery &q, const vector<DBEvent> &p, int match) const
{
    // adjust/delete overlaps;
    bool ok = true;
    for (uint i = 0; i < p.size(); i++)
    {
        if (i != (uint)match)
            ok &= MoveOutOfTheWayDB(q, p[i]);
    }

    // if we failed to move programs out of the way, don't insert new ones..
    if (!ok)
        return 0;

    // if no match, insert current item
    if ((match < 0) || ((uint)match >= p.size()))
        return InsertDB(q);

    // update matched item with current data
    return UpdateDB(q, p[match]);
}

uint DBEvent::UpdateDB(MSqlQuery &query, const DBEvent &match) const
{
    QString ltitle    = title;
    QString lsubtitle = subtitle;
    QString ldesc     = description;

    if (match.title.length() >= ltitle.length())
        ltitle = match.title;

    if (match.subtitle.length() >= lsubtitle.length())
        lsubtitle = match.subtitle;

    if (match.description.length() >= ldesc.length())
        ldesc = match.description;

    bool lcc     = captioned | match.captioned;
    bool lstereo = stereo    | match.stereo;

    query.prepare(
        "UPDATE program "
        "SET title          = :TITLE,     subtitle    = :SUBTITLE, "
        "    description    = :DESC, "
        "    starttime      = :STARTTIME, endtime     = :ENDTIME,  "
        "    closecaptioned = :CC,        stereo      = :STEREO,   "
        "    listingsource  = :LSOURCE "
        "WHERE chanid    = :CHANID AND "
        "      starttime = :OLDSTART ");

    query.bindValue(":CHANID",      chanid);
    query.bindValue(":OLDSTART",    match.starttime);
    query.bindValue(":TITLE",       ltitle.utf8());
    query.bindValue(":SUBTITLE",    lsubtitle.utf8());
    query.bindValue(":DESCRIPTION", ldesc.utf8());
    query.bindValue(":STARTTIME",   starttime);
    query.bindValue(":ENDTIME",     endtime);
    query.bindValue(":CC",          lcc);
    query.bindValue(":STEREO",      lstereo);
    query.bindValue(":LSOURCE",     1);

    if (!query.exec())
    {
        MythContext::DBError("InsertDB", query);
        return 0;
    }

    return 1;
}

static bool delete_program(MSqlQuery &query, uint chanid, const QDateTime &st)
{
    query.prepare(
        "DELETE from program "
        "WHERE chanid    = :CHANID AND "
        "      starttime = :STARTTIME");

    query.bindValue(":CHANID",    chanid);
    query.bindValue(":STARTTIME", st);

    if (!query.exec())
    {
        MythContext::DBError("delete_program", query);
        return false;
    }
    return true;
}

static bool change_program(MSqlQuery &query, uint chanid, const QDateTime &st,
                           const QDateTime &new_st, const QDateTime &new_end)
{
    query.prepare(
        "UPDATE program "
        "SET starttime = :NEWSTART, "
        "    endtime   = :NEWEND "
        "WHERE chanid    = :CHANID AND "
        "      starttime = :OLDSTART");

    query.bindValue(":CHANID",   chanid);
    query.bindValue(":OLDSTART", st);
    query.bindValue(":NEWSTART", new_st);
    query.bindValue(":NEWEND",   new_end);
    
    if (!query.exec())
    {
        MythContext::DBError("change_program", query);
        return false;
    }
    return true;    
}

bool DBEvent::MoveOutOfTheWayDB(MSqlQuery &query, const DBEvent &prog) const
{
    if (prog.starttime >= starttime && prog.endtime <= endtime)
    {
        // inside current program
        return delete_program(query, chanid, prog.starttime);
    }
    else if (prog.starttime < starttime && prog.endtime > starttime)
    {
        // starts before, but ends during our program
        return change_program(query, chanid, prog.starttime,
                              prog.starttime, starttime);
    }
    else if (prog.starttime > starttime && prog.endtime > starttime)
    {
        // starts during, but ends after our program
        return change_program(query, chanid, prog.starttime,
                              endtime, prog.endtime);
    }
    // must be non-conflicting...
    return true;
}

uint DBEvent::InsertDB(MSqlQuery &query) const
{
    query.prepare(
        "REPLACE INTO program ("
        "  chanid,       title,    subtitle,        description, "
        "  starttime,    endtime,  closecaptioned,  stereo, "
        "  listingsource ) "
        "VALUES ("
        " :CHANID,    :TITLE,   :SUBTITLE,       :DESCRIPTION, "
        " :STARTTIME, :ENDTIME, :CC,             :STEREO, "
        " :LSOURCE ) ");

    query.bindValue(":CHANID",      chanid);
    query.bindValue(":TITLE",       title.utf8());
    query.bindValue(":SUBTITLE",    subtitle.utf8());
    query.bindValue(":DESCRIPTION", description.utf8());
    query.bindValue(":STARTTIME",   starttime);
    query.bindValue(":ENDTIME",     endtime);
    query.bindValue(":CC",          captioned);
    query.bindValue(":STEREO",      stereo);
    query.bindValue(":LSOURCE",     1);

    if (!query.exec())
    {
        MythContext::DBError("InsertDB", query);
        return 0;
    }
    return 1;
}
