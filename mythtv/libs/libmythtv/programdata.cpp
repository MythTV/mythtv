// -*- Mode: c++ -*-

#include <limits.h>

// C++ includes
#include <algorithm>
using namespace std;

// MythTV headers
#include "programdata.h"
#include "channelutil.h"
#include "mythdb.h"
#include "mythlogging.h"
#include "dvbdescriptors.h"

#define LOC      QString("ProgramData: ")

static const char *roles[] =
{
    "",
    "actor",     "director",    "producer", "executive_producer",
    "writer",    "guest_star",  "host",     "adapter",
    "presenter", "commentator", "guest",
};

static QString denullify(const QString &str)
{
    return str.isNull() ? "" : str;
}

static QVariant denullify(const QDateTime &dt)
{
    return dt.isNull() ? QVariant("0000-00-00 00:00:00") : QVariant(dt);
}

DBPerson::DBPerson(const DBPerson &other) :
    role(other.role), name(other.name)
{
    name.squeeze();
}

DBPerson::DBPerson(Role _role, const QString &_name) :
    role(_role), name(_name)
{
    name.squeeze();
}

DBPerson::DBPerson(const QString &_role, const QString &_name) :
    role(kUnknown), name(_name)
{
    if (!_role.isEmpty())
    {
        for (uint i = 0; i < sizeof(roles) / sizeof(char *); i++)
        {
            if (_role == QString(roles[i]))
                role = (Role) i;
        }
    }
    name.squeeze();
}

QString DBPerson::GetRole(void) const
{
    if ((role < kActor) || (role > kGuest))
        return "guest";
    return roles[role];
}

uint DBPerson::InsertDB(MSqlQuery &query, uint chanid,
                        const QDateTime &starttime) const
{
    uint personid = GetPersonDB(query);
    if (!personid && InsertPersonDB(query))
        personid = GetPersonDB(query);

    return InsertCreditsDB(query, personid, chanid, starttime);
}

uint DBPerson::GetPersonDB(MSqlQuery &query) const
{
    query.prepare(
        "SELECT person "
        "FROM people "
        "WHERE name = :NAME");
    query.bindValue(":NAME", name);

    if (!query.exec())
        MythDB::DBError("get_person", query);
    else if (query.next())
        return query.value(0).toUInt();

    return 0;
}

uint DBPerson::InsertPersonDB(MSqlQuery &query) const
{
    query.prepare(
        "INSERT IGNORE INTO people (name) "
        "VALUES (:NAME);");
    query.bindValue(":NAME", name);

    if (query.exec())
        return 1;

    MythDB::DBError("insert_person", query);
    return 0;
}

uint DBPerson::InsertCreditsDB(MSqlQuery &query, uint personid, uint chanid,
                               const QDateTime &starttime) const
{
    if (!personid)
        return 0;

    query.prepare(
        "REPLACE INTO credits "
        "       ( person,  chanid,  starttime,  role) "
        "VALUES (:PERSON, :CHANID, :STARTTIME, :ROLE) ");
    query.bindValue(":PERSON",    personid);
    query.bindValue(":CHANID",    chanid);
    query.bindValue(":STARTTIME", starttime);
    query.bindValue(":ROLE",      GetRole());

    if (query.exec())
        return 1;

    MythDB::DBError("insert_credits", query);
    return 0;
}

DBEvent &DBEvent::operator=(const DBEvent &other)
{
    if (this == &other)
        return *this;

    title           = other.title;
    subtitle        = other.subtitle;
    description     = other.description;
    category        = other.category;
    starttime       = other.starttime;
    endtime         = other.endtime;
    airdate         = other.airdate;
    originalairdate = other.originalairdate;

    if (credits != other.credits)
    {
        if (credits)
        {
            delete credits;
            credits = NULL;
        }

        if (other.credits)
        {
            credits = new DBCredits;
            credits->insert(credits->end(),
                            other.credits->begin(),
                            other.credits->end());
        }
    }

    partnumber      = other.partnumber;
    parttotal       = other.parttotal;
    syndicatedepisodenumber = other.syndicatedepisodenumber;
    subtitleType    = other.subtitleType;
    audioProps      = other.audioProps;
    videoProps      = other.videoProps;
    stars           = other.stars;
    categoryType    = other.categoryType;
    seriesId        = other.seriesId;
    programId       = other.programId;
    inetref         = other.inetref;
    previouslyshown = other.previouslyshown;
    ratings         = other.ratings;
    listingsource   = other.listingsource;
    season          = other.season;
    episode         = other.episode;
    totalepisodes   = other.totalepisodes;

    Squeeze();

    return *this;
}

void DBEvent::Squeeze(void)
{
    title.squeeze();
    subtitle.squeeze();
    description.squeeze();
    category.squeeze();
    syndicatedepisodenumber.squeeze();
    seriesId.squeeze();
    programId.squeeze();
    inetref.squeeze();
}

void DBEvent::AddPerson(DBPerson::Role role, const QString &name)
{
    if (!credits)
        credits = new DBCredits;

    credits->push_back(DBPerson(role, name));
}

void DBEvent::AddPerson(const QString &role, const QString &name)
{
    if (!credits)
        credits = new DBCredits;

    credits->push_back(DBPerson(role, name));
}

bool DBEvent::HasTimeConflict(const DBEvent &o) const
{
    return ((starttime <= o.starttime && o.starttime < endtime) ||
            (o.endtime <= endtime     && starttime   < o.endtime));
}

// Processing new EIT entry starts here
uint DBEvent::UpdateDB(
    MSqlQuery &query, uint chanid, int match_threshold) const
{
    // List the program that we are going to add
    LOG(VB_EIT, LOG_DEBUG,
        QString("EIT: new program: %1 %2 '%3' chanid %4")
                .arg(starttime.toString(Qt::ISODate))
                .arg(endtime.toString(Qt::ISODate))
                .arg(title.left(35))
                .arg(chanid));

    // Do not insert or update when the program is in the past
    QDateTime now = QDateTime::currentDateTimeUtc();
    if (endtime < now)
    {
        LOG(VB_EIT, LOG_DEBUG,
            QString("EIT: skip '%1' endtime is in the past")
                    .arg(title.left(35)));
        return 0;
    }

    // Get all programs already in the database that overlap
    // with our new program.
    vector<DBEvent> programs;
    uint count = GetOverlappingPrograms(query, chanid, programs);
    int  match = INT_MIN;
    int  i     = -1;

    // If there are no programs already in the database that overlap
    // with our new program then we can simply insert it in the database.
    if (!count)
        return InsertDB(query, chanid);

    // List all overlapping programs with start- and endtime.
    for (uint j=0; j<count; j++)
    {
        LOG(VB_EIT, LOG_DEBUG,
            QString("EIT: overlap[%1] : %2 %3 '%4'")
                .arg(j)
                .arg(programs[j].starttime.toString(Qt::ISODate))
                .arg(programs[j].endtime.toString(Qt::ISODate))
                .arg(programs[j].title.left(35)));
    }

    // Determine which of the overlapping programs is a match with
    // our new program; if we have a match then our new program is considered
    // to be an update of the matching program.
    // The 2nd parameter "i" is the index of the best matching program.
    match = GetMatch(programs, i);

    // Update an existing program or insert a new program.
    if (match >= match_threshold)
    {
	// We have a good match; update program[i] in the database
	// with the new program data and move the overlapping programs
	// out of the way.
        LOG(VB_EIT, LOG_DEBUG,
            QString("EIT: accept match[%1]: %2 '%3' vs. '%4'")
                .arg(i).arg(match).arg(title.left(35))
                .arg(programs[i].title.left(35)));
        return UpdateDB(query, chanid, programs, i);
    }
    else
    {
	// If we are here then either we have a match but the match is
	// not good enough (the "i >= 0" case) or we did not find
	// a match at all.
        if (i >= 0)
        {
            LOG(VB_EIT, LOG_DEBUG,
                QString("EIT: reject match[%1]: %2 '%3' vs. '%4'")
                    .arg(i).arg(match).arg(title.left(35))
                    .arg(programs[i].title.left(35)));
        }

	// Move the overlapping programs out of the way and
	// insert the new program.
        return UpdateDB(query, chanid, programs, -1);
    }
}

// Get all programs in the database that overlap with our new program.
// We check for three ways in which we can have an overlap:
// (1)   Start of old program is inside our new program:
//           old program starts at or after our program AND
//           old program starts before end of our program;
//       e.g. new program  s-------------e
//            old program      s-------------e
//         or old program      s-----e
//       This is the STIME1/ETIME1 comparison.
// (2)   End of old program is inside our new program:
//           old program ends after our program starts AND
//           old program ends before end of our program
//       e.g. new program      s-------------e
//            old program  s-------------e
//         or old program          s-----e
//       This is the STIME2/ETIME2 comparison.
// (3)   We can have a new program is "inside" the old program:
//           old program starts before our program AND
//          old program ends after end of our program
//       e.g. new program      s---------e
//            old program  s-----------------e
//       This is the STIME3/ETIME3 comparison.
//
uint DBEvent::GetOverlappingPrograms(
    MSqlQuery &query, uint chanid, vector<DBEvent> &programs) const
{
    uint count = 0;
    query.prepare(
        "SELECT title,          subtitle,      description, "
        "       category,       category_type, "
        "       starttime,      endtime, "
        "       subtitletypes+0,audioprop+0,   videoprop+0, "
        "       seriesid,       programid, "
        "       partnumber,     parttotal, "
        "       syndicatedepisodenumber, "
        "       airdate,        originalairdate, "
        "       previouslyshown,listingsource, "
        "       stars+0, "
        "       season,         episode,       totalepisodes, "
        "       inetref "
        "FROM program "
        "WHERE chanid   = :CHANID AND "
        "      manualid = 0       AND "
        "      ( ( starttime >= :STIME1 AND starttime <  :ETIME1 ) OR "
        "        ( endtime   >  :STIME2 AND endtime   <= :ETIME2 ) OR "
        "        ( starttime <  :STIME3 AND endtime   >  :ETIME3 ) )");
    query.bindValue(":CHANID", chanid);
    query.bindValue(":STIME1", starttime);
    query.bindValue(":ETIME1", endtime);
    query.bindValue(":STIME2", starttime);
    query.bindValue(":ETIME2", endtime);
    query.bindValue(":STIME3", starttime);
    query.bindValue(":ETIME3", endtime);

    if (!query.exec())
    {
        MythDB::DBError("GetOverlappingPrograms 1", query);
        return 0;
    }

    while (query.next())
    {
        ProgramInfo::CategoryType category_type =
            string_to_myth_category_type(query.value(4).toString());

        DBEvent prog(
            query.value(0).toString(),
            query.value(1).toString(),
            query.value(2).toString(),
            query.value(3).toString(),
            category_type,
            MythDate::as_utc(query.value(5).toDateTime()),
            MythDate::as_utc(query.value(6).toDateTime()),
            query.value(7).toUInt(),
            query.value(8).toUInt(),
            query.value(9).toUInt(),
            query.value(19).toDouble(),
            query.value(10).toString(),
            query.value(11).toString(),
            query.value(18).toUInt(),
            query.value(20).toUInt(),  // Season
            query.value(21).toUInt(),  // Episode
            query.value(22).toUInt()); // Total Episodes

        prog.inetref    = query.value(23).toString();
        prog.partnumber = query.value(12).toUInt();
        prog.parttotal  = query.value(13).toUInt();
        prog.syndicatedepisodenumber = query.value(14).toString();
        prog.airdate    = query.value(15).toUInt();
        prog.originalairdate  = query.value(16).toDate();
        prog.previouslyshown  = query.value(17).toBool();

        programs.push_back(prog);
        count++;
    }

    return count;
}


static int score_words(const QStringList &al, const QStringList &bl)
{
    QStringList::const_iterator ait = al.begin();
    QStringList::const_iterator bit = bl.begin();
    int score = 0;
    for (; (ait != al.end()) && (bit != bl.end()); ++ait)
    {
        QStringList::const_iterator bit2 = bit;
        int dist = 0;
        int bscore = 0;
        for (; bit2 != bl.end(); ++bit2)
        {
            if (*ait == *bit)
            {
                bscore = max(1000, 2000 - (dist * 500));
                // lower score for short words
                if (ait->length() < 5)
                    bscore /= 5 - ait->length();
                break;
            }
            dist++;
        }
        if (bscore && dist < 3)
        {
            for (int i = 0; (i < dist) && bit != bl.end(); i++)
                ++bit;
        }
        score += bscore;
    }

    return score / al.size();
}

static int score_match(const QString &a, const QString &b)
{
    if (a.isEmpty() || b.isEmpty())
        return 0;
    else if (a == b)
        return 1000;

    QString A = a.simplified().toUpper();
    QString B = b.simplified().toUpper();
    if (A == B)
        return 1000;

    QStringList al, bl;
    al = A.split(" ", QString::SkipEmptyParts);
    if (al.isEmpty())
        return 0;

    bl = B.split(" ", QString::SkipEmptyParts);
    if (bl.isEmpty())
        return 0;

    // score words symmetrically
    int score = (score_words(al, bl) + score_words(bl, al)) / 2;

    return min(900, score);
}

int DBEvent::GetMatch(const vector<DBEvent> &programs, int &bestmatch) const
{
    bestmatch = -1;
    int match_val = INT_MIN;
    int overlap = 0;
    int duration = starttime.secsTo(endtime);

    for (uint i = 0; i < programs.size(); i++)
    {
        int mv = 0;
        int duration_loop = programs[i].starttime.secsTo(programs[i].endtime);

        mv -= abs(starttime.secsTo(programs[i].starttime));
        mv -= abs(endtime.secsTo(programs[i].endtime));
        mv -= abs(duration - duration_loop);
        mv += score_match(title, programs[i].title) * 10;
        mv += score_match(subtitle, programs[i].subtitle);
        mv += score_match(description, programs[i].description);

        /* determine overlap of both programs
         * we don't know which one starts first */
        if (starttime < programs[i].starttime)
            overlap = programs[i].starttime.secsTo(endtime);
        else if (starttime > programs[i].starttime)
            overlap = starttime.secsTo(programs[i].endtime);
        else
        {
            if (endtime <= programs[i].endtime)
                overlap = starttime.secsTo(endtime);
            else
                overlap = starttime.secsTo(programs[i].endtime);
        }

        /* scale the score depending on the overlap length
         * full score is preserved if the overlap is at least 1/2 of the length
         * of the shorter program */
        if (overlap > 0)
        {
            /* crappy providers apparently have events without duration
             * ensure that the minimal duration is 2 second to avoid
             * multiplying and more importantly dividing by zero */
            int min_dur = max(2, min(duration, duration_loop));
            overlap = min(overlap, min_dur/2);
            mv *= overlap * 2;
            mv /= min_dur;
        }
        else
        {
            LOG(VB_GENERAL, LOG_ERR,
                QString("Unexpected result: shows don't "
                        "overlap\n\t%1: %2 - %3\n\t%4: %5 - %6")
                    .arg(title.left(35))
                    .arg(starttime.toString(Qt::ISODate))
                    .arg(endtime.toString(Qt::ISODate))
                    .arg(programs[i].title.left(35), 35)
                    .arg(programs[i].starttime.toString(Qt::ISODate))
                    .arg(programs[i].endtime.toString(Qt::ISODate))
                );
        }

        if (mv > match_val)
        {
            LOG(VB_EIT, LOG_DEBUG,
                QString("GM : '%1' new best match '%2' with score %3")
                    .arg(title.left(35))
                    .arg(programs[i].title.left(35)).arg(mv));
            bestmatch = i;
            match_val = mv;
        }
    }

    return match_val;
}

uint DBEvent::UpdateDB(
    MSqlQuery &q, uint chanid, const vector<DBEvent> &p, int match) const
{
    // Adjust/delete overlaps;
    bool ok = true;
    for (uint i = 0; i < p.size(); i++)
    {
        if (i != (uint)match)
            ok &= MoveOutOfTheWayDB(q, chanid, p[i]);
    }

    // If we failed to move programs out of the way, don't insert new ones..
    if (!ok)
    {
        LOG(VB_EIT, LOG_DEBUG,
            QString("EIT: cannot insert '%1' MoveOutOfTheWayDB failed")
                    .arg(title.left(35)));
        return 0;
    }

    // No match, insert current item
    if ((match < 0) || ((uint)match >= p.size()))
    {
        LOG(VB_EIT, LOG_DEBUG,
            QString("EIT: insert '%1'")
                    .arg(title.left(35)));
        return InsertDB(q, chanid);
    }

    // Changing a starttime of a program that is being recorded can
    // start another recording of the same program.
    // Therefore we skip updates that change a starttime in the past
    // unless the endtime is later.
    if (starttime != p[match].starttime)
    {
        QDateTime now = QDateTime::currentDateTimeUtc();
        if (starttime < now && endtime <= p[match].endtime)
        {
            LOG(VB_EIT, LOG_DEBUG,
                QString("EIT:  skip '%1' starttime is in the past")
                        .arg(title.left(35)));
	    return 0;
        }
    }

    // Update matched item with current data
    LOG(VB_EIT, LOG_DEBUG,
         QString("EIT: update '%1' with '%2'")
                 .arg(p[match].title.left(35))
                 .arg(title.left(35)));
    return UpdateDB(q, chanid, p[match]);
}

// Update matched item with current data.
//
uint DBEvent::UpdateDB(
    MSqlQuery &query, uint chanid, const DBEvent &match)  const
{
    QString  ltitle     = title;
    QString  lsubtitle  = subtitle;
    QString  ldesc      = description;
    QString  lcategory  = category;
    uint16_t lairdate   = airdate;
    QString  lprogramId = programId;
    QString  lseriesId  = seriesId;
    QString  linetref   = inetref;
    QDate loriginalairdate = originalairdate;

    if (match.title.length() >= ltitle.length())
        ltitle = match.title;

    if (match.subtitle.length() >= lsubtitle.length())
        lsubtitle = match.subtitle;

    if (match.description.length() >= ldesc.length())
        ldesc = match.description;

    if (lcategory.isEmpty() && !match.category.isEmpty())
        lcategory = match.category;

    if (!lairdate && !match.airdate)
        lairdate = match.airdate;

    if (!loriginalairdate.isValid() && match.originalairdate.isValid())
        loriginalairdate = match.originalairdate;

    if (lprogramId.isEmpty() && !match.programId.isEmpty())
        lprogramId = match.programId;

    if (lseriesId.isEmpty() && !match.seriesId.isEmpty())
        lseriesId = match.seriesId;

    if (linetref.isEmpty() && !match.inetref.isEmpty())
        linetref= match.inetref;

    ProgramInfo::CategoryType tmp = categoryType;
    if (!categoryType && match.categoryType)
        tmp = match.categoryType;

    QString lcattype = myth_category_type_to_string(tmp);

    unsigned char lsubtype = subtitleType | match.subtitleType;
    unsigned char laudio   = audioProps   | match.audioProps;
    unsigned char lvideo   = videoProps   | match.videoProps;

    uint lpartnumber =
        (!partnumber && match.partnumber) ? match.partnumber : partnumber;
    uint lparttotal =
        (!parttotal  && match.parttotal ) ? match.parttotal  : parttotal;

    bool lpreviouslyshown = previouslyshown | match.previouslyshown;

    uint32_t llistingsource = listingsource | match.listingsource;

    QString lsyndicatedepisodenumber = syndicatedepisodenumber;
    if (lsyndicatedepisodenumber.isEmpty() &&
        !match.syndicatedepisodenumber.isEmpty())
        lsyndicatedepisodenumber = match.syndicatedepisodenumber;

    query.prepare(
        "UPDATE program "
        "SET title          = :TITLE,     subtitle      = :SUBTITLE, "
        "    description    = :DESC, "
        "    category       = :CATEGORY,  category_type = :CATTYPE, "
        "    starttime      = :STARTTIME, endtime       = :ENDTIME, "
        "    closecaptioned = :CC,        subtitled     = :HASSUBTITLES, "
        "    stereo         = :STEREO,    hdtv          = :HDTV, "
        "    subtitletypes  = :SUBTYPE, "
        "    audioprop      = :AUDIOPROP, videoprop     = :VIDEOPROP, "
        "    partnumber     = :PARTNO,    parttotal     = :PARTTOTAL, "
        "    syndicatedepisodenumber = :SYNDICATENO, "
        "    airdate        = :AIRDATE,   originalairdate=:ORIGAIRDATE, "
        "    listingsource  = :LSOURCE, "
        "    seriesid       = :SERIESID,  programid     = :PROGRAMID, "
        "    previouslyshown = :PREVSHOWN, inetref      = :INETREF "
        "WHERE chanid    = :CHANID AND "
        "      starttime = :OLDSTART ");

    query.bindValue(":CHANID",      chanid);
    query.bindValue(":OLDSTART",    match.starttime);
    query.bindValue(":TITLE",       denullify(ltitle));
    query.bindValue(":SUBTITLE",    denullify(lsubtitle));
    query.bindValue(":DESC",        denullify(ldesc));
    query.bindValue(":CATEGORY",    denullify(lcategory));
    query.bindValue(":CATTYPE",     lcattype);
    query.bindValue(":STARTTIME",   starttime);
    query.bindValue(":ENDTIME",     endtime);
    query.bindValue(":CC",          lsubtype & SUB_HARDHEAR ? true : false);
    query.bindValue(":HASSUBTITLES",lsubtype & SUB_NORMAL   ? true : false);
    query.bindValue(":STEREO",      laudio   & AUD_STEREO   ? true : false);
    query.bindValue(":HDTV",        lvideo   & VID_HDTV     ? true : false);
    query.bindValue(":SUBTYPE",     lsubtype);
    query.bindValue(":AUDIOPROP",   laudio);
    query.bindValue(":VIDEOPROP",   lvideo);
    query.bindValue(":PARTNO",      lpartnumber);
    query.bindValue(":PARTTOTAL",   lparttotal);
    query.bindValue(":SYNDICATENO", denullify(lsyndicatedepisodenumber));
    query.bindValue(":AIRDATE",     lairdate?QString::number(lairdate):"0000");
    query.bindValue(":ORIGAIRDATE", loriginalairdate);
    query.bindValue(":LSOURCE",     llistingsource);
    query.bindValue(":SERIESID",    denullify(lseriesId));
    query.bindValue(":PROGRAMID",   denullify(lprogramId));
    query.bindValue(":PREVSHOWN",   lpreviouslyshown);
    query.bindValue(":INETREF",     linetref);

    if (!query.exec())
    {
        MythDB::DBError("InsertDB", query);
        return 0;
    }

    if (credits)
    {
        for (uint i = 0; i < credits->size(); i++)
            (*credits)[i].InsertDB(query, chanid, starttime);
    }
    
    QList<EventRating>::const_iterator j = ratings.begin();
    for (; j != ratings.end(); ++j)
    {
        query.prepare(
            "INSERT INTO programrating "
            "       ( chanid, starttime, system, rating) "
            "VALUES (:CHANID, :START,    :SYS,  :RATING)");
        query.bindValue(":CHANID", chanid);
        query.bindValue(":START",  starttime);
        query.bindValue(":SYS",    (*j).system);
        query.bindValue(":RATING", (*j).rating);

        if (!query.exec())
            MythDB::DBError("programrating insert", query);
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
        MythDB::DBError("delete_program", query);
        return false;
    }

    query.prepare(
        "DELETE from credits "
        "WHERE chanid    = :CHANID AND "
        "      starttime = :STARTTIME");

    query.bindValue(":CHANID",    chanid);
    query.bindValue(":STARTTIME", st);

    if (!query.exec())
    {
        MythDB::DBError("delete_credits", query);
        return false;
    }

    return true;
}

static bool program_exists(MSqlQuery &query, uint chanid, const QDateTime &st)
{
    query.prepare(
        "SELECT title FROM program "
        "WHERE chanid    = :CHANID AND "
        "      starttime = :OLDSTART");
    query.bindValue(":CHANID",   chanid);
    query.bindValue(":OLDSTART", st);
    if (!query.exec())
    {
        MythDB::DBError("program_exists", query);
    }
    if (query.next())
    {
        return true;
    }
    return false;
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
        MythDB::DBError("change_program", query);
        return false;
    }

    query.prepare(
        "UPDATE credits "
        "SET starttime = :NEWSTART "
        "WHERE chanid    = :CHANID AND "
        "      starttime = :OLDSTART");

    query.bindValue(":CHANID",   chanid);
    query.bindValue(":OLDSTART", st);
    query.bindValue(":NEWSTART", new_st);

    if (!query.exec())
    {
        MythDB::DBError("change_credits", query);
        return false;
    }

    return true;
}

// Move the program "prog" (3rd parameter) out of the way
// because it overlaps with our new program.
bool DBEvent::MoveOutOfTheWayDB(
    MSqlQuery &query, uint chanid, const DBEvent &prog) const
{
    if (prog.starttime >= starttime && prog.endtime <= endtime)
    {
        // Old program completely inside our new program.
	// Delete the old program completely.
        LOG(VB_EIT, LOG_DEBUG,
            QString("EIT: delete '%1' %2 - %3")
                    .arg(prog.title.left(35))
                    .arg(prog.starttime.toString(Qt::ISODate))
                    .arg(prog.endtime.toString(Qt::ISODate)));
        return delete_program(query, chanid, prog.starttime);
    }
    else if (prog.starttime < starttime && prog.endtime > starttime)
    {
        // Old program starts before, but ends during or after our new program.
	// Adjust the end time of the old program to the start time
	// of our new program.
	// This will leave a hole after our new program when the end time of
	// the old program was after the end time of the new program!!
        LOG(VB_EIT, LOG_DEBUG,
            QString("EIT: change '%1' endtime to %2")
                    .arg(prog.title.left(35))
                    .arg(starttime.toString(Qt::ISODate)));
        return change_program(query, chanid, prog.starttime,
                              prog.starttime, // Keep the start time
			      starttime);     // New end time is our start time
    }
    else if (prog.starttime < endtime && prog.endtime > endtime)
    {
        // Old program starts during, but ends after our new program.
	// Adjust the starttime of the old program to the end time
	// of our new program.
	// If there is already a program starting just when our
	// new program ends we cannot move the old program
	// so then we have to delete the old program.
        if (program_exists(query, chanid, endtime))
        {
            LOG(VB_EIT, LOG_DEBUG,
                QString("EIT: delete '%1' %2 - %3")
                        .arg(prog.title.left(35))
                        .arg(prog.starttime.toString(Qt::ISODate))
                        .arg(prog.endtime.toString(Qt::ISODate)));
            return delete_program(query, chanid, prog.starttime);
        }
        LOG(VB_EIT, LOG_DEBUG,
            QString("EIT: change '%1' starttime to %2")
                    .arg(prog.title.left(35))
                    .arg(endtime.toString(Qt::ISODate)));

        return change_program(query, chanid, prog.starttime,
                              endtime,        // New start time is our endtime
			      prog.endtime);  // Keep the end time
    }
    // must be non-conflicting...
    return true;
}

uint DBEvent::InsertDB(MSqlQuery &query, uint chanid) const
{
    query.prepare(
        "REPLACE INTO program ("
        "  chanid,         title,          subtitle,        description, "
        "  category,       category_type, "
        "  starttime,      endtime, "
        "  closecaptioned, stereo,         hdtv,            subtitled, "
        "  subtitletypes,  audioprop,      videoprop, "
        "  stars,          partnumber,     parttotal, "
        "  syndicatedepisodenumber, "
        "  airdate,        originalairdate,listingsource, "
        "  seriesid,       programid,      previouslyshown, "
        "  season,         episode,        totalepisodes, "
        "  inetref ) "
        "VALUES ("
        " :CHANID,        :TITLE,         :SUBTITLE,       :DESCRIPTION, "
        " :CATEGORY,      :CATTYPE, "
        " :STARTTIME,     :ENDTIME, "
        " :CC,            :STEREO,        :HDTV,           :HASSUBTITLES, "
        " :SUBTYPES,      :AUDIOPROP,     :VIDEOPROP, "
        " :STARS,         :PARTNUMBER,    :PARTTOTAL, "
        " :SYNDICATENO, "
        " :AIRDATE,       :ORIGAIRDATE,   :LSOURCE, "
        " :SERIESID,      :PROGRAMID,     :PREVSHOWN, "
        " :SEASON,        :EPISODE,       :TOTALEPISODES, "
        " :INETREF ) ");

    QString cattype = myth_category_type_to_string(categoryType);
    QString empty("");
    query.bindValue(":CHANID",      chanid);
    query.bindValue(":TITLE",       denullify(title));
    query.bindValue(":SUBTITLE",    denullify(subtitle));
    query.bindValue(":DESCRIPTION", denullify(description));
    query.bindValue(":CATEGORY",    denullify(category));
    query.bindValue(":CATTYPE",     cattype);
    query.bindValue(":STARTTIME",   starttime);
    query.bindValue(":ENDTIME",     endtime);
    query.bindValue(":CC",          subtitleType & SUB_HARDHEAR ? true : false);
    query.bindValue(":STEREO",      audioProps   & AUD_STEREO   ? true : false);
    query.bindValue(":HDTV",        videoProps   & VID_HDTV     ? true : false);
    query.bindValue(":HASSUBTITLES",subtitleType & SUB_NORMAL   ? true : false);
    query.bindValue(":SUBTYPES",    subtitleType);
    query.bindValue(":AUDIOPROP",   audioProps);
    query.bindValue(":VIDEOPROP",   videoProps);
    query.bindValue(":STARS",       stars);
    query.bindValue(":PARTNUMBER",  partnumber);
    query.bindValue(":PARTTOTAL",   parttotal);
    query.bindValue(":SYNDICATENO", denullify(syndicatedepisodenumber));
    query.bindValue(":AIRDATE",     airdate ? QString::number(airdate):"0000");
    query.bindValue(":ORIGAIRDATE", originalairdate);
    query.bindValue(":LSOURCE",     listingsource);
    query.bindValue(":SERIESID",    denullify(seriesId));
    query.bindValue(":PROGRAMID",   denullify(programId));
    query.bindValue(":PREVSHOWN",   previouslyshown);
    query.bindValue(":SEASON",      season);
    query.bindValue(":EPISODE",     episode);
    query.bindValue(":TOTALEPISODES", totalepisodes);
    query.bindValue(":INETREF",     inetref);

    if (!query.exec())
    {
        MythDB::DBError("InsertDB", query);
        return 0;
    }

    if (credits)
    {
        for (uint i = 0; i < credits->size(); i++)
            (*credits)[i].InsertDB(query, chanid, starttime);
    }

    return 1;
}

ProgInfo::ProgInfo(const ProgInfo &other) :
    DBEvent(other.listingsource)
{
    *this = other;
}

ProgInfo &ProgInfo::operator=(const ProgInfo &other)
{
    if (this == &other)
        return *this;

    DBEvent::operator=(other);

    channel         = other.channel;
    startts         = other.startts;
    endts           = other.endts;
    stars           = other.stars;
    title_pronounce = other.title_pronounce;
    showtype        = other.showtype;
    colorcode       = other.colorcode;
    clumpidx        = other.clumpidx;
    clumpmax        = other.clumpmax;

    channel.squeeze();
    startts.squeeze();
    endts.squeeze();
    stars.squeeze();
    title_pronounce.squeeze();
    showtype.squeeze();
    colorcode.squeeze();
    clumpidx.squeeze();
    clumpmax.squeeze();

    return *this;
}

void ProgInfo::Squeeze(void)
{
    DBEvent::Squeeze();
    channel.squeeze();
    startts.squeeze();
    endts.squeeze();
    stars.squeeze();
    title_pronounce.squeeze();
    showtype.squeeze();
    colorcode.squeeze();
    clumpidx.squeeze();
    clumpmax.squeeze();
}

uint ProgInfo::InsertDB(MSqlQuery &query, uint chanid) const
{
    LOG(VB_XMLTV, LOG_INFO,
        QString("Inserting new program    : %1 - %2 %3 %4")
            .arg(starttime.toString(Qt::ISODate))
            .arg(endtime.toString(Qt::ISODate))
            .arg(channel)
            .arg(title));

    query.prepare(
        "REPLACE INTO program ("
        "  chanid,         title,          subtitle,        description, "
        "  category,       category_type,  "
        "  starttime,      endtime, "
        "  closecaptioned, stereo,         hdtv,            subtitled, "
        "  subtitletypes,  audioprop,      videoprop, "
        "  partnumber,     parttotal, "
        "  syndicatedepisodenumber, "
        "  airdate,        originalairdate,listingsource, "
        "  seriesid,       programid,      previouslyshown, "
        "  stars,          showtype,       title_pronounce, colorcode, "
        "  season,         episode,        totalepisodes, "
        "  inetref ) "

        "VALUES("
        " :CHANID,        :TITLE,         :SUBTITLE,       :DESCRIPTION, "
        " :CATEGORY,      :CATTYPE,       "
        " :STARTTIME,     :ENDTIME, "
        " :CC,            :STEREO,        :HDTV,           :HASSUBTITLES, "
        " :SUBTYPES,      :AUDIOPROP,     :VIDEOPROP, "
        " :PARTNUMBER,    :PARTTOTAL, "
        " :SYNDICATENO, "
        " :AIRDATE,       :ORIGAIRDATE,   :LSOURCE, "
        " :SERIESID,      :PROGRAMID,     :PREVSHOWN, "
        " :STARS,         :SHOWTYPE,      :TITLEPRON,      :COLORCODE, "
        " :SEASON,        :EPISODE,       :TOTALEPISODES, "
        " :INETREF )");

    QString cattype = myth_category_type_to_string(categoryType);

    query.bindValue(":CHANID",      chanid);
    query.bindValue(":TITLE",       denullify(title));
    query.bindValue(":SUBTITLE",    denullify(subtitle));
    query.bindValue(":DESCRIPTION", denullify(description));
    query.bindValue(":CATEGORY",    denullify(category));
    query.bindValue(":CATTYPE",     cattype);
    query.bindValue(":STARTTIME",   starttime);
    query.bindValue(":ENDTIME",     denullify(endtime));
    query.bindValue(":CC",
                    subtitleType & SUB_HARDHEAR ? true : false);
    query.bindValue(":STEREO",
                    audioProps   & AUD_STEREO   ? true : false);
    query.bindValue(":HDTV",
                    videoProps   & VID_HDTV     ? true : false);
    query.bindValue(":HASSUBTITLES",
                    subtitleType & SUB_NORMAL   ? true : false);
    query.bindValue(":SUBTYPES",    subtitleType);
    query.bindValue(":AUDIOPROP",   audioProps);
    query.bindValue(":VIDEOPROP",   videoProps);
    query.bindValue(":PARTNUMBER",  partnumber);
    query.bindValue(":PARTTOTAL",   parttotal);
    query.bindValue(":SYNDICATENO", denullify(syndicatedepisodenumber));
    query.bindValue(":AIRDATE",     airdate ? QString::number(airdate):"0000");
    query.bindValue(":ORIGAIRDATE", originalairdate);
    query.bindValue(":LSOURCE",     listingsource);
    query.bindValue(":SERIESID",    denullify(seriesId));
    query.bindValue(":PROGRAMID",   denullify(programId));
    query.bindValue(":PREVSHOWN",   previouslyshown);
    query.bindValue(":STARS",       stars);
    query.bindValue(":SHOWTYPE",    showtype);
    query.bindValue(":TITLEPRON",   title_pronounce);
    query.bindValue(":COLORCODE",   colorcode);
    query.bindValue(":SEASON",      season);
    query.bindValue(":EPISODE",     episode);
    query.bindValue(":TOTALEPISODES", totalepisodes);
    query.bindValue(":INETREF",     inetref);

    if (!query.exec())
    {
        MythDB::DBError("program insert", query);
        return 0;
    }

    QList<EventRating>::const_iterator j = ratings.begin();
    for (; j != ratings.end(); ++j)
    {
        query.prepare(
            "INSERT INTO programrating "
            "       ( chanid, starttime, system, rating) "
            "VALUES (:CHANID, :START,    :SYS,  :RATING)");
        query.bindValue(":CHANID", chanid);
        query.bindValue(":START",  starttime);
        query.bindValue(":SYS",    (*j).system);
        query.bindValue(":RATING", (*j).rating);

        if (!query.exec())
            MythDB::DBError("programrating insert", query);
    }

    if (credits)
    {
        for (uint i = 0; i < credits->size(); ++i)
            (*credits)[i].InsertDB(query, chanid, starttime);
    }

    return 1;
}

bool ProgramData::ClearDataByChannel(
    uint chanid, const QDateTime &from, const QDateTime &to,
    bool use_channel_time_offset)
{
    int secs = 0;
    if (use_channel_time_offset)
        secs = ChannelUtil::GetTimeOffset(chanid) * 60;

    QDateTime newFrom = from.addSecs(secs);
    QDateTime newTo   = to.addSecs(secs);

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("DELETE FROM program "
                  "WHERE starttime >= :FROM AND starttime < :TO "
                  "AND chanid = :CHANID ;");
    query.bindValue(":FROM", newFrom);
    query.bindValue(":TO", newTo);
    query.bindValue(":CHANID", chanid);
    bool ok = query.exec();

    query.prepare("DELETE FROM programrating "
                  "WHERE starttime >= :FROM AND starttime < :TO "
                  "AND chanid = :CHANID ;");
    query.bindValue(":FROM", newFrom);
    query.bindValue(":TO", newTo);
    query.bindValue(":CHANID", chanid);
    ok &= query.exec();

    query.prepare("DELETE FROM credits "
                  "WHERE starttime >= :FROM AND starttime < :TO "
                  "AND chanid = :CHANID ;");
    query.bindValue(":FROM", newFrom);
    query.bindValue(":TO", newTo);
    query.bindValue(":CHANID", chanid);
    ok &= query.exec();

    query.prepare("DELETE FROM programgenres "
                  "WHERE starttime >= :FROM AND starttime < :TO "
                  "AND chanid = :CHANID ;");
    query.bindValue(":FROM", newFrom);
    query.bindValue(":TO", newTo);
    query.bindValue(":CHANID", chanid);
    ok &= query.exec();

    return ok;
}

bool ProgramData::ClearDataBySource(
    uint sourceid, const QDateTime &from, const QDateTime &to,
    bool use_channel_time_offset)
{
    vector<uint> chanids = ChannelUtil::GetChanIDs(sourceid);

    bool ok = true;
    for (uint i = 0; i < chanids.size(); i++)
        ok &= ClearDataByChannel(chanids[i], from, to, use_channel_time_offset);

    return ok;
}

static bool start_time_less_than(const DBEvent *a, const DBEvent *b)
{
    return (a->starttime < b->starttime);
}

void ProgramData::FixProgramList(QList<ProgInfo*> &fixlist)
{
    qStableSort(fixlist.begin(), fixlist.end(), start_time_less_than);

    QList<ProgInfo*>::iterator it = fixlist.begin();
    while (1)
    {
        QList<ProgInfo*>::iterator cur = it;
        ++it;

        // fill in miss stop times
        if ((*cur)->endts.isEmpty() || (*cur)->startts > (*cur)->endts)
        {
            if (it != fixlist.end())
            {
                (*cur)->endts   = (*it)->startts;
                (*cur)->endtime = (*it)->starttime;
            }
            /* if its the last programme in the file then leave its
               endtime as 0000-00-00 00:00:00 so we can find it easily in
               fix_end_times() */
        }

        if (it == fixlist.end())
            break;

        // remove overlapping programs
        if ((*cur)->HasTimeConflict(**it))
        {
            QList<ProgInfo*>::iterator tokeep, todelete;

            if ((*cur)->endtime <= (*cur)->starttime)
                tokeep = it, todelete = cur;
            else if ((*it)->endtime <= (*it)->starttime)
                tokeep = cur, todelete = it;
            else if (!(*cur)->subtitle.isEmpty() &&
                     (*it)->subtitle.isEmpty())
                tokeep = cur, todelete = it;
            else if (!(*it)->subtitle.isEmpty()  &&
                     (*cur)->subtitle.isEmpty())
                tokeep = it, todelete = cur;
            else if (!(*cur)->description.isEmpty() &&
                     (*it)->description.isEmpty())
                tokeep = cur, todelete = it;
            else
                tokeep = it, todelete = cur;


            LOG(VB_XMLTV, LOG_INFO,
                QString("Removing conflicting program: %1 - %2 %3 %4")
                    .arg((*todelete)->starttime.toString(Qt::ISODate))
                    .arg((*todelete)->endtime.toString(Qt::ISODate))
                    .arg((*todelete)->channel)
                    .arg((*todelete)->title));

            LOG(VB_XMLTV, LOG_INFO,
                QString("Conflicted with            : %1 - %2 %3 %4")
                    .arg((*tokeep)->starttime.toString(Qt::ISODate))
                    .arg((*tokeep)->endtime.toString(Qt::ISODate))
                    .arg((*tokeep)->channel)
                    .arg((*tokeep)->title));

            bool step_back = todelete == it;
            it = fixlist.erase(todelete);
            if (step_back)
                --it;
        }
    }
}

void ProgramData::HandlePrograms(
    uint sourceid, QMap<QString, QList<ProgInfo> > &proglist)
{
    uint unchanged = 0, updated = 0;

    MSqlQuery query(MSqlQuery::InitCon());

    QMap<QString, QList<ProgInfo> >::const_iterator mapiter;
    for (mapiter = proglist.begin(); mapiter != proglist.end(); ++mapiter)
    {
        if (mapiter.key().isEmpty())
            continue;

        query.prepare(
            "SELECT chanid "
            "FROM channel "
            "WHERE sourceid = :ID AND "
            "      xmltvid  = :XMLTVID");
        query.bindValue(":ID",      sourceid);
        query.bindValue(":XMLTVID", mapiter.key());

        if (!query.exec())
        {
            MythDB::DBError("ProgramData::HandlePrograms", query);
            continue;
        }

        vector<uint> chanids;
        while (query.next())
            chanids.push_back(query.value(0).toUInt());

        if (chanids.empty())
        {
            LOG(VB_GENERAL, LOG_NOTICE,
                QString("Unknown xmltv channel identifier: %1"
                        " - Skipping channel.").arg(mapiter.key()));
            continue;
        }

        QList<ProgInfo> &list = proglist[mapiter.key()];
        QList<ProgInfo*> sortlist;
        QList<ProgInfo>::iterator it = list.begin();
        for (; it != list.end(); ++it)
            sortlist.push_back(&(*it));

        FixProgramList(sortlist);

        for (uint i = 0; i < chanids.size(); ++i)
        {
            HandlePrograms(query, chanids[i], sortlist, unchanged, updated);
        }
    }

    LOG(VB_GENERAL, LOG_INFO,
        QString("Updated programs: %1 Unchanged programs: %2")
                .arg(updated) .arg(unchanged));
}

void ProgramData::HandlePrograms(MSqlQuery             &query,
                                 uint                   chanid,
                                 const QList<ProgInfo*> &sortlist,
                                 uint &unchanged,
                                 uint &updated)
{
    QList<ProgInfo*>::const_iterator it = sortlist.begin();
    for (; it != sortlist.end(); ++it)
    {
        if (IsUnchanged(query, chanid, **it))
        {
            unchanged++;
            continue;
        }

        if (!DeleteOverlaps(query, chanid, **it))
            continue;

        updated += (*it)->InsertDB(query, chanid);
    }
}

int ProgramData::fix_end_times(void)
{
    int count = 0;
    QString chanid, starttime, endtime, querystr;
    MSqlQuery query1(MSqlQuery::InitCon()), query2(MSqlQuery::InitCon());

    querystr = "SELECT chanid, starttime, endtime FROM program "
               "WHERE endtime = '0000-00-00 00:00:00' "
               "ORDER BY chanid, starttime;";

    if (!query1.exec(querystr))
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString("fix_end_times query failed: %1").arg(querystr));
        return -1;
    }

    while (query1.next())
    {
        starttime = query1.value(1).toString();
        chanid = query1.value(0).toString();
        endtime = query1.value(2).toString();

        querystr = QString("SELECT chanid, starttime, endtime FROM program "
                           "WHERE starttime > '%1' "
                           "AND chanid = '%2' "
                           "ORDER BY starttime LIMIT 1;")
                           .arg(starttime)
                           .arg(chanid);

        if (!query2.exec(querystr))
        {
            LOG(VB_GENERAL, LOG_ERR,
                QString("fix_end_times query failed: %1").arg(querystr));
            return -1;
        }

        if (query2.next() && (endtime != query2.value(1).toString()))
        {
            count++;
            endtime = query2.value(1).toString();
            querystr = QString("UPDATE program SET "
                               "endtime = '%2' WHERE (chanid = '%3' AND "
                               "starttime = '%4');")
                               .arg(endtime)
                               .arg(chanid)
                               .arg(starttime);

            if (!query2.exec(querystr))
            {
                LOG(VB_GENERAL, LOG_ERR,
                    QString("fix_end_times query failed: %1").arg(querystr));
                return -1;
            }
        }
    }

    return count;
}

bool ProgramData::IsUnchanged(
    MSqlQuery &query, uint chanid, const ProgInfo &pi)
{
    query.prepare(
        "SELECT count(*) "
        "FROM program "
        "WHERE chanid          = :CHANID     AND "
        "      starttime       = :START      AND "
        "      endtime         = :END        AND "
        "      title           = :TITLE      AND "
        "      subtitle        = :SUBTITLE   AND "
        "      description     = :DESC       AND "
        "      category        = :CATEGORY   AND "
        "      category_type   = :CATEGORY_TYPE AND "
        "      airdate         = :AIRDATE    AND "
        "      stars >= (:STARS1 - 0.001)    AND "
        "      stars <= (:STARS2 + 0.001)    AND "
        "      previouslyshown = :PREVIOUSLYSHOWN AND "
        "      title_pronounce = :TITLE_PRONOUNCE AND "
        "      audioprop       = :AUDIOPROP  AND "
        "      videoprop       = :VIDEOPROP  AND "
        "      subtitletypes   = :SUBTYPES   AND "
        "      partnumber      = :PARTNUMBER AND "
        "      parttotal       = :PARTTOTAL  AND "
        "      seriesid        = :SERIESID   AND "
        "      showtype        = :SHOWTYPE   AND "
        "      colorcode       = :COLORCODE  AND "
        "      syndicatedepisodenumber = :SYNDICATEDEPISODENUMBER AND "
        "      programid       = :PROGRAMID  AND "
        "      inetref         = :INETREF");

    QString cattype = myth_category_type_to_string(pi.categoryType);

    query.bindValue(":CHANID",     chanid);
    query.bindValue(":START",      pi.starttime);
    query.bindValue(":END",        pi.endtime);
    query.bindValue(":TITLE",      denullify(pi.title));
    query.bindValue(":SUBTITLE",   denullify(pi.subtitle));
    query.bindValue(":DESC",       denullify(pi.description));
    query.bindValue(":CATEGORY",   denullify(pi.category));
    query.bindValue(":CATEGORY_TYPE", cattype);
    query.bindValue(":AIRDATE",    pi.airdate);
    query.bindValue(":STARS1",     pi.stars);
    query.bindValue(":STARS2",     pi.stars);
    query.bindValue(":PREVIOUSLYSHOWN", pi.previouslyshown);
    query.bindValue(":TITLE_PRONOUNCE", pi.title_pronounce);
    query.bindValue(":AUDIOPROP",  pi.audioProps);
    query.bindValue(":VIDEOPROP",  pi.videoProps);
    query.bindValue(":SUBTYPES",   pi.subtitleType);
    query.bindValue(":PARTNUMBER", pi.partnumber);
    query.bindValue(":PARTTOTAL",  pi.parttotal);
    query.bindValue(":SERIESID",   denullify(pi.seriesId));
    query.bindValue(":SHOWTYPE",   pi.showtype);
    query.bindValue(":COLORCODE",  pi.colorcode);
    query.bindValue(":SYNDICATEDEPISODENUMBER",
                    denullify(pi.syndicatedepisodenumber));
    query.bindValue(":PROGRAMID",  denullify(pi.programId));
    query.bindValue(":INETREF",    pi.inetref);

    if (query.exec() && query.next())
        return query.value(0).toUInt() > 0;

    return false;
}

bool ProgramData::DeleteOverlaps(
    MSqlQuery &query, uint chanid, const ProgInfo &pi)
{
    if (VERBOSE_LEVEL_CHECK(VB_XMLTV, LOG_INFO))
    {
        // Get overlaps..
        query.prepare(
            "SELECT title,starttime,endtime "
            "FROM program "
            "WHERE chanid     = :CHANID AND "
            "      starttime >= :START AND "
            "      starttime <  :END;");
        query.bindValue(":CHANID", chanid);
        query.bindValue(":START",  pi.starttime);
        query.bindValue(":END",    pi.endtime);

        if (!query.exec())
            return false;

        if (!query.next())
            return true;

        do
        {
            LOG(VB_XMLTV, LOG_INFO,
                QString("Removing existing program: %1 - %2 %3 %4")
                .arg(MythDate::as_utc(query.value(1).toDateTime()).toString(Qt::ISODate))
                .arg(MythDate::as_utc(query.value(2).toDateTime()).toString(Qt::ISODate))
                .arg(pi.channel)
                .arg(query.value(0).toString()));
        } while (query.next());
    }

    if (!ClearDataByChannel(chanid, pi.starttime, pi.endtime, false))
    {
        LOG(VB_XMLTV, LOG_ERR,
            QString("Program delete failed    : %1 - %2 %3 %4")
                .arg(pi.starttime.toString(Qt::ISODate))
                .arg(pi.endtime.toString(Qt::ISODate))
                .arg(pi.channel)
                .arg(pi.title));
        return false;
    }

    return true;
}
