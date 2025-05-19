// -*- Mode: c++ -*-

// C++ includes
#include <algorithm>
#include <climits>
#include <utility>

// Qt includes
#include <QtGlobal> // for qAbs

// MythTV headers
#include "libmythbase/mythdb.h"
#include "libmythbase/mythlogging.h"

#include "channelutil.h"
#include "mpeg/dvbdescriptors.h"
#include "programdata.h"

#define LOC      QString("ProgramData: ")

static const std::array<const std::string,DBPerson::kGuest+1> roles
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

static void add_genres(MSqlQuery &query, const QStringList &genres,
                uint chanid, const QDateTime &starttime)
{
    QString relevance = QString("0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ");
    for (auto it = genres.constBegin(); (it != genres.constEnd()) &&
             ((it - genres.constBegin()) < relevance.size()); ++it)
    {
        query.prepare(
           "INSERT INTO programgenres "
           "       ( chanid,  starttime, genre,  relevance) "
           "VALUES (:CHANID, :START,    :genre, :relevance)");
        query.bindValue(":CHANID",    chanid);
        query.bindValue(":START",     starttime);
        query.bindValue(":genre",     *it);
        query.bindValue(":relevance", relevance.at(it - genres.constBegin()));

        if (!query.exec())
            MythDB::DBError("programgenres insert", query);
    }
}

DBPerson::DBPerson(const DBPerson &other)
    : m_role(other.m_role)
    , m_name(other.m_name)
    , m_priority(other.m_priority)
    , m_character(other.m_character)
{
    m_name.squeeze();
    m_character.squeeze();
}

DBPerson& DBPerson::operator=(const DBPerson &rhs)
{
    if (this == &rhs)
        return *this;
    m_role = rhs.m_role;
    m_name = rhs.m_name;
    m_name.squeeze();
    m_priority = rhs.m_priority;
    m_character = rhs.m_character;
    m_character.squeeze();
    return *this;
}

DBPerson::DBPerson(Role role, QString name, int priority,
                   QString character)
    : m_role(role)
    , m_name(std::move(name))
    , m_priority(priority)
    , m_character(std::move(character))
{
    m_name.squeeze();
    m_character.squeeze();
}

DBPerson::DBPerson(const QString &role, QString name,
                   int priority, QString character)
    : m_role(kUnknown)
    , m_name(std::move(name))
    , m_priority(priority)
    , m_character(std::move(character))
{
    if (!role.isEmpty())
    {
        std::string rolestr = role.toLower().toStdString();
        for (size_t i = 0; i < roles.size(); ++i)
        {
            if (rolestr == roles[i])
                m_role = (Role) i;
        }
    }
    m_name.squeeze();
    m_character.squeeze();
}

QString DBPerson::GetRole(void) const
{
    if ((m_role < kActor) || (m_role > kGuest))
        return "guest";
    return QString::fromStdString(roles[m_role]);
}

QString DBPerson::toString(void) const
{
    return QString("%1 %2 as %3").arg(m_role).arg(m_name, m_character);
}

uint DBPerson::InsertDB(MSqlQuery &query, uint chanid,
                        const QDateTime &starttime,
                        bool recording) const
{
    uint personid = GetPersonDB(query);
    if (!personid && InsertPersonDB(query))
        personid = GetPersonDB(query);

    uint roleid = 0;
    if (!m_character.isEmpty())
    {
        roleid = GetRoleDB(query);
        if (!roleid && InsertRoleDB(query))
            roleid = GetRoleDB(query);
    }

    return InsertCreditsDB(query, personid, roleid, chanid,
                           starttime, recording);
}

uint DBPerson::GetPersonDB(MSqlQuery &query) const
{
    query.prepare(
        "SELECT person "
        "FROM people "
        "WHERE name = :NAME");
    query.bindValue(":NAME", m_name);

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
    query.bindValue(":NAME", m_name);

    if (query.exec())
        return 1;

    MythDB::DBError("insert_person", query);
    return 0;
}

uint DBPerson::GetRoleDB(MSqlQuery &query) const
{
    query.prepare(
        "SELECT roleid "
        "FROM roles "
        "WHERE name = :NAME");
    query.bindValue(":NAME", m_character);

    if (query.exec() && query.next())
        return query.value(0).toUInt();

    return 0;
}

bool DBPerson::InsertRoleDB(MSqlQuery &query) const
{
    query.prepare(
        "INSERT IGNORE INTO roles (name) "
        "VALUES (:NAME);");
    query.bindValue(":NAME", m_character);

    if (query.exec())
        return true;

    MythDB::DBError("insert_role", query);
    return false;
}

uint DBPerson::InsertCreditsDB(MSqlQuery &query, uint personid, uint roleid,
                               uint chanid, const QDateTime &starttime,
                               bool recording) const
{
    if (!personid)
        return 0;

    QString table = recording ? "recordedcredits" : "credits";

    query.prepare(QString("REPLACE INTO %1 "
        "       ( person,  roleid,  chanid,  starttime,  role, priority) "
        "VALUES (:PERSON, :ROLEID, :CHANID, :STARTTIME, :ROLE, :PRIORITY);")
                  .arg(table));
    query.bindValue(":PERSON",    personid);
    query.bindValue(":ROLEID",    roleid);
    query.bindValue(":CHANID",    chanid);
    query.bindValue(":STARTTIME", starttime);
    query.bindValue(":ROLE",      GetRole());
    query.bindValue(":PRIORITY",  m_priority);

    if (query.exec())
        return 1;

    MythDB::DBError("insert_credits", query);
    return 0;
}

DBEvent &DBEvent::operator=(const DBEvent &other)
{
    if (this == &other)
        return *this;

    m_title           = other.m_title;
    m_subtitle        = other.m_subtitle;
    m_description     = other.m_description;
    m_category        = other.m_category;
    m_starttime       = other.m_starttime;
    m_endtime         = other.m_endtime;
    m_airdate         = other.m_airdate;
    m_originalairdate = other.m_originalairdate;

    if (m_credits != other.m_credits)
    {
        if (m_credits)
        {
            delete m_credits;
            m_credits = nullptr;
        }

        if (other.m_credits)
        {
            m_credits = new DBCredits;
            m_credits->insert(m_credits->end(),
                            other.m_credits->begin(),
                            other.m_credits->end());
        }
    }

    m_partnumber              = other.m_partnumber;
    m_parttotal               = other.m_parttotal;
    m_syndicatedepisodenumber = other.m_syndicatedepisodenumber;
    m_subtitleType            = other.m_subtitleType;
    m_audioProps              = other.m_audioProps;
    m_videoProps              = other.m_videoProps;
    m_stars                   = other.m_stars;
    m_categoryType            = other.m_categoryType;
    m_seriesId                = other.m_seriesId;
    m_programId               = other.m_programId;
    m_inetref                 = other.m_inetref;
    m_previouslyshown         = other.m_previouslyshown;
    m_ratings                 = other.m_ratings;
    m_listingsource           = other.m_listingsource;
    m_season                  = other.m_season;
    m_episode                 = other.m_episode;
    m_totalepisodes           = other.m_totalepisodes;
    m_genres                  = other.m_genres;

    Squeeze();

    return *this;
}

void DBEvent::Squeeze(void)
{
    m_title.squeeze();
    m_subtitle.squeeze();
    m_description.squeeze();
    m_category.squeeze();
    m_syndicatedepisodenumber.squeeze();
    m_seriesId.squeeze();
    m_programId.squeeze();
    m_inetref.squeeze();
}

void DBEvent::AddPerson(DBPerson::Role role, const QString &name,
                        int priority, const QString &character)
{
    if (!m_credits)
        m_credits = new DBCredits;

    m_credits->emplace_back(role, name.simplified(),
                            priority, character.simplified());
}

void DBEvent::AddPerson(const QString &role, const QString &name,
                        int priority, const QString &character)
{
    if (!m_credits)
        m_credits = new DBCredits;

    m_credits->emplace_back(role, name.simplified(),
                            priority, character.simplified());
}

bool DBEvent::HasTimeConflict(const DBEvent &o) const
{
    return ((m_starttime <= o.m_starttime && o.m_starttime < m_endtime) ||
            (o.m_endtime <= m_endtime     && m_starttime   < o.m_endtime));
}

// Processing new EIT entry starts here
uint DBEvent::UpdateDB(
    MSqlQuery &query, uint chanid, int match_threshold) const
{
    // List the program that we are going to add
    LOG(VB_EIT, LOG_DEBUG,
        QString("EIT: new program: %1 %2 '%3' chanid %4")
                .arg(m_starttime.toString(Qt::ISODate),
                     m_endtime.toString(Qt::ISODate),
                     m_title.left(35),
                     QString::number(chanid)));

    // Do not insert or update when the program is in the past
    QDateTime now = QDateTime::currentDateTimeUtc();
    if (m_endtime < now)
    {
        LOG(VB_EIT, LOG_DEBUG,
            QString("EIT: skip '%1' endtime is in the past")
                    .arg(m_title.left(35)));
        return 0;
    }

    // Get all programs already in the database that overlap
    // with our new program.
    std::vector<DBEvent> programs;
    uint count = GetOverlappingPrograms(query, chanid, programs);
    int  match = INT_MIN;
    int  i     = -1;

    // If there are no programs already in the database that overlap
    // with our new program then we can simply insert it in the database.
    if (!count)
        return InsertDB(query, chanid);

    // List all overlapping programs with start- and endtime.
    for (uint j=0; j<count; ++j)
    {
        LOG(VB_EIT, LOG_DEBUG,
            QString("EIT: overlap[%1] : %2 %3 '%4'")
                .arg(QString::number(j),
                     programs[j].m_starttime.toString(Qt::ISODate),
                     programs[j].m_endtime.toString(Qt::ISODate),
                     programs[j].m_title.left(35)));
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
                .arg(i).arg(match)
                .arg(m_title.left(35),
                     programs[i].m_title.left(35)));
        return UpdateDB(query, chanid, programs, i);
    }

    // If we are here then either we have a match but the match is
    // not good enough (the "i >= 0" case) or we did not find
    // a match at all.
    if (i >= 0)
    {
        LOG(VB_EIT, LOG_DEBUG,
            QString("EIT: reject match[%1]: %2 '%3' vs. '%4'")
                .arg(i).arg(match)
                .arg(m_title.left(35),
                     programs[i].m_title.left(35)));
    }

    // Move the overlapping programs out of the way and
    // insert the new program.
    return UpdateDB(query, chanid, programs, -1);
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
    MSqlQuery &query, uint chanid, std::vector<DBEvent> &programs) const
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
    query.bindValue(":STIME1", m_starttime);
    query.bindValue(":ETIME1", m_endtime);
    query.bindValue(":STIME2", m_starttime);
    query.bindValue(":ETIME2", m_endtime);
    query.bindValue(":STIME3", m_starttime);
    query.bindValue(":ETIME3", m_endtime);

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
            query.value(19).toFloat(),
            query.value(10).toString(),
            query.value(11).toString(),
            query.value(18).toUInt(),
            query.value(20).toUInt(),  // Season
            query.value(21).toUInt(),  // Episode
            query.value(22).toUInt()); // Total Episodes

        prog.m_inetref    = query.value(23).toString();
        prog.m_partnumber = query.value(12).toUInt();
        prog.m_parttotal  = query.value(13).toUInt();
        prog.m_syndicatedepisodenumber = query.value(14).toString();
        prog.m_airdate    = query.value(15).toUInt();
        prog.m_originalairdate  = query.value(16).toDate();
        prog.m_previouslyshown  = query.value(17).toBool();

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
                bscore = std::max(1000, 2000 - (dist * 500));
                // lower score for short words
                if (ait->length() < 5)
                    bscore /= 5 - ait->length();
                break;
            }
            dist++;
        }
        if (bscore && dist < 3)
        {
            for (int i = 0; (i < dist) && bit != bl.end(); ++i)
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
    if (a == b)
        return 1000;

    QString A = a.simplified().toUpper();
    QString B = b.simplified().toUpper();
    if (A == B)
        return 1000;

    QStringList al = A.split(" ", Qt::SkipEmptyParts);
    if (al.isEmpty())
        return 0;

    QStringList bl = B.split(" ", Qt::SkipEmptyParts);
    if (bl.isEmpty())
        return 0;

    // score words symmetrically
    int score = (score_words(al, bl) + score_words(bl, al)) / 2;

    return std::min(900, score);
}

int DBEvent::GetMatch(const std::vector<DBEvent> &programs, int &bestmatch) const
{
    bestmatch = -1;
    int match_val = INT_MIN;
    int duration = m_starttime.secsTo(m_endtime);

    for (size_t i = 0; i < programs.size(); ++i)
    {
        int mv = 0;
        int duration_loop = programs[i].m_starttime.secsTo(programs[i].m_endtime);

        mv -= qAbs(m_starttime.secsTo(programs[i].m_starttime));
        mv -= qAbs(m_endtime.secsTo(programs[i].m_endtime));
        mv -= qAbs(duration - duration_loop);
        mv += score_match(m_title, programs[i].m_title) * 10;
        mv += score_match(m_subtitle, programs[i].m_subtitle);
        mv += score_match(m_description, programs[i].m_description);

        /* determine overlap of both programs
         * we don't know which one starts first */
        int overlap = 0;
        if (m_starttime < programs[i].m_starttime)
            overlap = programs[i].m_starttime.secsTo(m_endtime);
        else if (m_starttime > programs[i].m_starttime)
            overlap = m_starttime.secsTo(programs[i].m_endtime);
        else
        {
            if (m_endtime <= programs[i].m_endtime)
                overlap = m_starttime.secsTo(m_endtime);
            else
                overlap = m_starttime.secsTo(programs[i].m_endtime);
        }

        /* scale the score depending on the overlap length
         * full score is preserved if the overlap is at least 1/2 of the length
         * of the shorter program */
        if (overlap > 0)
        {
            /* crappy providers apparently have events without duration
             * ensure that the minimal duration is 2 second to avoid
             * multiplying and more importantly dividing by zero */
            int min_dur = std::max(2, std::min(duration, duration_loop));
            overlap = std::min(overlap, min_dur/2);
            mv *= overlap * 2;
            mv /= min_dur;
        }
        else
        {
            LOG(VB_GENERAL, LOG_ERR,
                QString("Unexpected result: shows don't "
                        "overlap\n\t%1: %2 - %3\n\t%4: %5 - %6")
                    .arg(m_title.left(35), 35)
                    .arg(m_starttime.toString(Qt::ISODate),
                         m_endtime.toString(Qt::ISODate))
                    .arg(programs[i].m_title.left(35), 35)
                    .arg(programs[i].m_starttime.toString(Qt::ISODate),
                         programs[i].m_endtime.toString(Qt::ISODate))
                );
        }

        if (mv > match_val)
        {
            LOG(VB_EIT, LOG_DEBUG,
                QString("GM : '%1' new best match '%2' with score %3")
                    .arg(m_title.left(35),
                         programs[i].m_title.left(35),
                         QString::number(mv)));
            bestmatch = i;
            match_val = mv;
        }
    }

    return match_val;
}

uint DBEvent::UpdateDB(
    MSqlQuery &q, uint chanid, const std::vector<DBEvent> &p, int match) const
{
    // Adjust/delete overlaps;
    bool ok = true;
    for (size_t i = 0; i < p.size(); ++i)
    {
        if (i != (uint)match)
            ok &= MoveOutOfTheWayDB(q, chanid, p[i]);
    }

    // If we failed to move programs out of the way, don't insert new ones..
    if (!ok)
    {
        LOG(VB_EIT, LOG_DEBUG,
            QString("EIT: cannot insert '%1' MoveOutOfTheWayDB failed")
                    .arg(m_title.left(35)));
        return 0;
    }

    // No match, insert current item
    if ((match < 0) || ((uint)match >= p.size()))
    {
        LOG(VB_EIT, LOG_DEBUG,
            QString("EIT: insert '%1'")
                    .arg(m_title.left(35)));
        return InsertDB(q, chanid);
    }

    // Changing a starttime of a program that is being recorded can
    // start another recording of the same program.
    // Therefore we skip updates that change a starttime in the past
    // unless the endtime is later.
    if (m_starttime != p[match].m_starttime)
    {
        QDateTime now = QDateTime::currentDateTimeUtc();
        if (m_starttime < now && m_endtime <= p[match].m_endtime)
        {
            LOG(VB_EIT, LOG_DEBUG,
                QString("EIT:  skip '%1' starttime is in the past")
                        .arg(m_title.left(35)));
            return 0;
        }
    }

    // Update matched item with current data
    LOG(VB_EIT, LOG_DEBUG,
         QString("EIT: update '%1' with '%2'")
                 .arg(p[match].m_title.left(35),
                      m_title.left(35)));
    return UpdateDB(q, chanid, p[match]);
}

// Update starttime in table record for single recordings
// when the starttime of a program is changed.
//
// Return the number of rows affected:
// 0    if program is not found in table record
// 1    if program is found and updated
//
static int change_record(MSqlQuery &query, uint chanid,
                          const QDateTime &old_starttime,
                          const QDateTime &new_starttime)
{
    query.prepare("UPDATE record "
                   "SET starttime = :NEWSTARTTIME, "
                   "    startdate = :NEWSTARTDATE "
                   "WHERE chanid  = :CHANID "
                   "AND type      = :TYPE "
                   "AND search    = :SEARCH "
                   "AND starttime = :OLDSTARTTIME "
                   "AND startdate = :OLDSTARTDATE ");
    query.bindValue(":CHANID",       chanid);
    query.bindValue(":TYPE",         kSingleRecord);
    query.bindValue(":SEARCH",       kNoSearch);
    query.bindValue(":OLDSTARTTIME", old_starttime.time());
    query.bindValue(":OLDSTARTDATE", old_starttime.date());
    query.bindValue(":NEWSTARTTIME", new_starttime.time());
    query.bindValue(":NEWSTARTDATE", new_starttime.date());

    int rows = 0;
    if (!query.exec() || !query.isActive())
    {
        MythDB::DBError("Updating record", query);
    }
    else
    {
        rows = query.numRowsAffected();
    }
    if (rows > 0)
    {
        LOG(VB_EIT, LOG_DEBUG,
            QString("EIT: Updated record: chanid:%1 old:%3 new:%4 rows:%5")
            .arg(QString::number(chanid),
                 old_starttime.toString(Qt::ISODate),
                 new_starttime.toString(Qt::ISODate),
                 QString::number(rows)));
    }
    return rows;
}

// Update matched item with current data.
//
uint DBEvent::UpdateDB(
    MSqlQuery &query, uint chanid, const DBEvent &match)  const
{
    QString  ltitle           = m_title;
    QString  lsubtitle        = m_subtitle;
    QString  ldesc            = m_description;
    QString  lcategory        = m_category;
    uint16_t lairdate         = m_airdate;
    QString  lprogramId       = m_programId;
    QString  lseriesId        = m_seriesId;
    QString  linetref         = m_inetref;
    QDate    loriginalairdate = m_originalairdate;

    // Update starttime also in database table record so that
    // tables program and record remain consistent.
    if (m_starttime != match.m_starttime)
    {
        QDateTime const &old_starttime = match.m_starttime;
        QDateTime const &new_starttime = m_starttime;
        change_record(query, chanid, old_starttime, new_starttime);

        LOG(VB_EIT, LOG_DEBUG,
            QString("EIT: (U) change starttime from %1 to %2 for chanid:%3 program '%4' ")
                    .arg(old_starttime.toString(Qt::ISODate),
                         new_starttime.toString(Qt::ISODate),
                         QString::number(chanid),
                         m_title.left(35)));
    }

    if (ltitle.isEmpty() && !match.m_title.isEmpty())
        ltitle = match.m_title;

    if (lsubtitle.isEmpty() && !match.m_subtitle.isEmpty())
        lsubtitle = match.m_subtitle;

    if (ldesc.isEmpty() && !match.m_description.isEmpty())
        ldesc = match.m_description;

    if (lcategory.isEmpty() && !match.m_category.isEmpty())
        lcategory = match.m_category;

    if (!lairdate && match.m_airdate)
        lairdate = match.m_airdate;

    if (!loriginalairdate.isValid() && match.m_originalairdate.isValid())
        loriginalairdate = match.m_originalairdate;

    if (lprogramId.isEmpty() && !match.m_programId.isEmpty())
        lprogramId = match.m_programId;

    if (lseriesId.isEmpty() && !match.m_seriesId.isEmpty())
        lseriesId = match.m_seriesId;

    if (linetref.isEmpty() && !match.m_inetref.isEmpty())
        linetref= match.m_inetref;

    ProgramInfo::CategoryType tmp = m_categoryType;
    if (!m_categoryType && match.m_categoryType)
        tmp = match.m_categoryType;

    QString lcattype = myth_category_type_to_string(tmp);

    unsigned char lsubtype = m_subtitleType | match.m_subtitleType;
    unsigned char laudio   = m_audioProps   | match.m_audioProps;
    unsigned char lvideo   = m_videoProps   | match.m_videoProps;

    uint lseason = match.m_season;
    uint lepisode = match.m_episode;
    uint lepisodeTotal = match.m_totalepisodes;

    if (m_season || m_episode || m_totalepisodes)
    {
        lseason = m_season;
        lepisode = m_episode;
        lepisodeTotal = m_totalepisodes;
    }

    uint lpartnumber = match.m_partnumber;
    uint lparttotal = match.m_parttotal;

    if (m_partnumber || m_parttotal)
    {
        lpartnumber = m_partnumber;
        lparttotal  = m_parttotal;
    }

    bool lpreviouslyshown = m_previouslyshown || match.m_previouslyshown;

    uint32_t llistingsource = m_listingsource | match.m_listingsource;

    QString lsyndicatedepisodenumber = m_syndicatedepisodenumber;
    if (lsyndicatedepisodenumber.isEmpty() &&
        !match.m_syndicatedepisodenumber.isEmpty())
        lsyndicatedepisodenumber = match.m_syndicatedepisodenumber;

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
        "    season         = :SEASON,  "
        "    episode        = :EPISODE,   totalepisodes = :TOTALEPS, "
        "    partnumber     = :PARTNO,    parttotal     = :PARTTOTAL, "
        "    syndicatedepisodenumber = :SYNDICATENO, "
        "    airdate        = :AIRDATE,   originalairdate=:ORIGAIRDATE, "
        "    listingsource  = :LSOURCE, "
        "    seriesid       = :SERIESID,  programid     = :PROGRAMID, "
        "    previouslyshown = :PREVSHOWN, inetref      = :INETREF "
        "WHERE chanid    = :CHANID AND "
        "      starttime = :OLDSTART ");

    query.bindValue(":CHANID",      chanid);
    query.bindValue(":OLDSTART",    match.m_starttime);
    query.bindValue(":TITLE",       denullify(ltitle));
    query.bindValue(":SUBTITLE",    denullify(lsubtitle));
    query.bindValue(":DESC",        denullify(ldesc));
    query.bindValue(":CATEGORY",    denullify(lcategory));
    query.bindValue(":CATTYPE",     lcattype);
    query.bindValue(":STARTTIME",   m_starttime);
    query.bindValue(":ENDTIME",     m_endtime);
    query.bindValue(":CC",          (lsubtype & SUB_HARDHEAR) != 0);
    query.bindValue(":HASSUBTITLES",(lsubtype & SUB_NORMAL) != 0);
    query.bindValue(":STEREO",      (laudio   & AUD_STEREO) != 0);
    query.bindValue(":HDTV",        (lvideo   & VID_HDTV) != 0);
    query.bindValue(":SUBTYPE",     lsubtype);
    query.bindValue(":AUDIOPROP",   laudio);
    query.bindValue(":VIDEOPROP",   lvideo);
    query.bindValue(":SEASON",      lseason);
    query.bindValue(":EPISODE",     lepisode);
    query.bindValue(":TOTALEPS",    lepisodeTotal);
    query.bindValue(":PARTNO",      lpartnumber);
    query.bindValue(":PARTTOTAL",   lparttotal);
    query.bindValue(":SYNDICATENO", denullify(lsyndicatedepisodenumber));
    query.bindValue(":AIRDATE",     lairdate ? QString::number(lairdate) : "0000");
    query.bindValue(":ORIGAIRDATE", loriginalairdate);
    query.bindValue(":LSOURCE",     llistingsource);
    query.bindValue(":SERIESID",    denullify(lseriesId));
    query.bindValue(":PROGRAMID",   denullify(lprogramId));
    query.bindValue(":PREVSHOWN",   lpreviouslyshown);
    query.bindValue(":INETREF",     denullify(linetref));

    if (!query.exec())
    {
        MythDB::DBError("UpdateDB", query);
        return 0;
    }

    if (m_credits)
    {
        for (auto & credit : *m_credits)
            credit.InsertDB(query, chanid, m_starttime);
    }

    for (const auto & rating : std::as_const(m_ratings))
    {
        query.prepare(
            "INSERT IGNORE INTO programrating "
            "       ( chanid, starttime, `system`, rating) "
            "VALUES (:CHANID, :START,    :SYS,  :RATING)");
        query.bindValue(":CHANID", chanid);
        query.bindValue(":START",  m_starttime);
        query.bindValue(":SYS",    rating.m_system);
        query.bindValue(":RATING", rating.m_rating);

        if (!query.exec())
            MythDB::DBError("programrating insert", query);
    }

    add_genres(query, m_genres, chanid, m_starttime);

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

    query.prepare(
        "DELETE from programrating "
        "WHERE chanid    = :CHANID AND "
        "      starttime = :STARTTIME");

    query.bindValue(":CHANID",    chanid);
    query.bindValue(":STARTTIME", st);

    if (!query.exec())
    {
        MythDB::DBError("delete_rating", query);
        return false;
    }

    query.prepare(
        "DELETE from programgenres "
        "WHERE chanid    = :CHANID AND "
        "      starttime = :STARTTIME");

    query.bindValue(":CHANID",    chanid);
    query.bindValue(":STARTTIME", st);

    if (!query.exec())
    {
        MythDB::DBError("delete_genres", query);
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
    return query.next();
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

    query.prepare(
        "UPDATE programrating "
        "SET starttime = :NEWSTART "
        "WHERE chanid    = :CHANID AND "
        "      starttime = :OLDSTART");

    query.bindValue(":CHANID",   chanid);
    query.bindValue(":OLDSTART", st);
    query.bindValue(":NEWSTART", new_st);

    if (!query.exec())
    {
        MythDB::DBError("change_rating", query);
        return false;
    }

    query.prepare(
        "UPDATE programgenres "
        "SET starttime = :NEWSTART "
        "WHERE chanid    = :CHANID AND "
        "      starttime = :OLDSTART");

    query.bindValue(":CHANID",   chanid);
    query.bindValue(":OLDSTART", st);
    query.bindValue(":NEWSTART", new_st);

    if (!query.exec())
    {
        MythDB::DBError("change_genres", query);
        return false;
    }

    return true;
}

// Move the program "prog" (3rd parameter) out of the way
// because it overlaps with our new program.
bool DBEvent::MoveOutOfTheWayDB(
    MSqlQuery &query, uint chanid, const DBEvent &prog) const
{
    if (prog.m_starttime >= m_starttime && prog.m_endtime <= m_endtime)
    {
        // Old program completely inside our new program.
        // Delete the old program completely.
        LOG(VB_EIT, LOG_DEBUG,
            QString("EIT: delete '%1' %2 - %3")
                    .arg(prog.m_title.left(35),
                         prog.m_starttime.toString(Qt::ISODate),
                         prog.m_endtime.toString(Qt::ISODate)));
        return delete_program(query, chanid, prog.m_starttime);
    }
    if (prog.m_starttime < m_starttime && prog.m_endtime > m_starttime)
    {
        // Old program starts before, but ends during or after our new program.
        // Adjust the end time of the old program to the start time
        // of our new program.
        // This will leave a hole after our new program when the end time of
        // the old program was after the end time of the new program!!
        LOG(VB_EIT, LOG_DEBUG,
            QString("EIT: change '%1' endtime to %2")
                    .arg(prog.m_title.left(35),
                         m_starttime.toString(Qt::ISODate)));
        return change_program(query, chanid, prog.m_starttime,
                              prog.m_starttime, // Keep the start time
                              m_starttime);     // New end time is our start time
    }
    if (prog.m_starttime < m_endtime && prog.m_endtime > m_endtime)
    {
        // Old program starts during, but ends after our new program.
        // Adjust the starttime of the old program to the end time
        // of our new program.
        // If there is already a program starting just when our
        // new program ends we cannot move the old program
        // so then we have to delete the old program.
        if (program_exists(query, chanid, m_endtime))
        {
            LOG(VB_EIT, LOG_DEBUG,
                QString("EIT: delete '%1' %2 - %3")
                        .arg(prog.m_title.left(35),
                             prog.m_starttime.toString(Qt::ISODate),
                             prog.m_endtime.toString(Qt::ISODate)));
            return delete_program(query, chanid, prog.m_starttime);
        }
        LOG(VB_EIT, LOG_DEBUG,
            QString("EIT: (M) change starttime from %1 to %2 for chanid:%3 program '%4' ")
                    .arg(prog.m_starttime.toString(Qt::ISODate),
                         m_endtime.toString(Qt::ISODate),
                         QString::number(chanid),
                         prog.m_title.left(35)));

        // Update starttime in tables record and program so they stay consistent.
        change_record(query, chanid, prog.m_starttime, m_endtime);
        return change_program(query, chanid, prog.m_starttime,
                              m_endtime,        // New start time is our endtime
                              prog.m_endtime);  // Keep the end time
    }
    // must be non-conflicting...
    return true;
}

/**
 *  \brief Insert Callback function when Allow Re-record is pressed in Watch Recordings
 */
uint DBEvent::InsertDB(MSqlQuery &query, uint chanid,
                       bool recording) const
{
    QString table = recording ? "recordedprogram" : "program";

    query.prepare(QString(
        "REPLACE INTO %1 ("
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
        " :INETREF ) ").arg(table));

    QString cattype = myth_category_type_to_string(m_categoryType);
    query.bindValue(":CHANID",      chanid);
    query.bindValue(":TITLE",       denullify(m_title));
    query.bindValue(":SUBTITLE",    denullify(m_subtitle));
    query.bindValue(":DESCRIPTION", denullify(m_description));
    query.bindValue(":CATEGORY",    denullify(m_category));
    query.bindValue(":CATTYPE",     cattype);
    query.bindValue(":STARTTIME",   m_starttime);
    query.bindValue(":ENDTIME",     m_endtime);
    query.bindValue(":CC",          (m_subtitleType & SUB_HARDHEAR) != 0);
    query.bindValue(":STEREO",      (m_audioProps   & AUD_STEREO) != 0);
    query.bindValue(":HDTV",        (m_videoProps   & VID_HDTV) != 0);
    query.bindValue(":HASSUBTITLES",(m_subtitleType & SUB_NORMAL) != 0);
    query.bindValue(":SUBTYPES",    m_subtitleType);
    query.bindValue(":AUDIOPROP",   m_audioProps);
    query.bindValue(":VIDEOPROP",   m_videoProps);
    query.bindValue(":STARS",       m_stars);
    query.bindValue(":PARTNUMBER",  m_partnumber);
    query.bindValue(":PARTTOTAL",   m_parttotal);
    query.bindValue(":SYNDICATENO", denullify(m_syndicatedepisodenumber));
    query.bindValue(":AIRDATE",     m_airdate ? QString::number(m_airdate) : "0000");
    query.bindValue(":ORIGAIRDATE", m_originalairdate);
    query.bindValue(":LSOURCE",     m_listingsource);
    query.bindValue(":SERIESID",    denullify(m_seriesId));
    query.bindValue(":PROGRAMID",   denullify(m_programId));
    query.bindValue(":PREVSHOWN",   m_previouslyshown);
    query.bindValue(":SEASON",      m_season);
    query.bindValue(":EPISODE",     m_episode);
    query.bindValue(":TOTALEPISODES", m_totalepisodes);
    query.bindValue(":INETREF",     denullify(m_inetref));

    if (!query.exec())
    {
        MythDB::DBError("InsertDB", query);
        return 0;
    }

    table = recording ? "recordedrating" : "programrating";
    for (const auto & rating : std::as_const(m_ratings))
    {
        query.prepare(QString(
            "INSERT IGNORE INTO %1 "
            "       ( chanid, starttime, `system`, rating) "
            "VALUES (:CHANID, :START,    :SYS,  :RATING)").arg(table));
        query.bindValue(":CHANID", chanid);
        query.bindValue(":START",  m_starttime);
        query.bindValue(":SYS",    rating.m_system);
        query.bindValue(":RATING", rating.m_rating);

        if (!query.exec())
            MythDB::DBError("programrating insert", query);
    }

    if (m_credits)
    {
        for (auto & credit : *m_credits)
            credit.InsertDB(query, chanid, m_starttime, recording);
    }

    add_genres(query, m_genres, chanid, m_starttime);

    return 1;
}

ProgInfo::ProgInfo(const ProgInfo &other) :
    DBEvent(other.m_listingsource)
{
    *this = other;
}

ProgInfo &ProgInfo::operator=(const ProgInfo &other)
{
    if (this == &other)
        return *this;

    DBEvent::operator=(other);

    m_channel         = other.m_channel;
    m_startts         = other.m_startts;
    m_endts           = other.m_endts;
    m_title_pronounce = other.m_title_pronounce;
    m_showtype        = other.m_showtype;
    m_colorcode       = other.m_colorcode;
    m_clumpidx        = other.m_clumpidx;
    m_clumpmax        = other.m_clumpmax;

    m_channel.squeeze();
    m_startts.squeeze();
    m_endts.squeeze();
    m_title_pronounce.squeeze();
    m_showtype.squeeze();
    m_colorcode.squeeze();
    m_clumpidx.squeeze();
    m_clumpmax.squeeze();

    return *this;
}

void ProgInfo::Squeeze(void)
{
    DBEvent::Squeeze();
    m_channel.squeeze();
    m_startts.squeeze();
    m_endts.squeeze();
    m_title_pronounce.squeeze();
    m_showtype.squeeze();
    m_colorcode.squeeze();
    m_clumpidx.squeeze();
    m_clumpmax.squeeze();
}

/**
 *  \brief Insert a single entry into the "program" database.
 *
 *  This function is called from HandlePrograms, which is called from
 *  mythfilldatabase. It inserts a single entry into the "program"
 *  database. The data for this structure is taken from the 'this'
 *  ProgInfo object.
 *
 *  \param query  Any mysql query structure. The contents is ignored
 *                and the structure is repurposed for local queries.
 *  \param chanid The channel number for this program.
 *  \param recorded defaults to false, i.e. program, programrating
 */
uint ProgInfo::InsertDB(MSqlQuery &query, uint chanid,
                        bool recording) const
{
    QString table = recording ? "recordedprogram" : "program";

    LOG(VB_XMLTV, LOG_DEBUG,
        QString("Inserting new %1    : %2 - %3 %4 %5")
        .arg(table,
             m_starttime.toString(Qt::ISODate),
             m_endtime.toString(Qt::ISODate),
             m_channel));

    query.prepare(QString(
        "REPLACE INTO %1 ("
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
        " :INETREF )").arg(table));

    QString cattype = myth_category_type_to_string(m_categoryType);

    query.bindValue(":CHANID",      chanid);
    query.bindValue(":TITLE",       denullify(m_title));
    query.bindValue(":SUBTITLE",    denullify(m_subtitle));
    query.bindValue(":DESCRIPTION", denullify(m_description));
    query.bindValue(":CATEGORY",    denullify(m_category));
    query.bindValue(":CATTYPE",     cattype);
    query.bindValue(":STARTTIME",   m_starttime);
    query.bindValue(":ENDTIME",     denullify(m_endtime));
    query.bindValue(":CC",
                    (m_subtitleType & SUB_HARDHEAR) != 0);
    query.bindValue(":STEREO",
                    (m_audioProps   & AUD_STEREO) != 0);
    query.bindValue(":HDTV",
                    (m_videoProps   & VID_HDTV) != 0);
    query.bindValue(":HASSUBTITLES",
                    (m_subtitleType & SUB_NORMAL) != 0);
    query.bindValue(":SUBTYPES",    m_subtitleType);
    query.bindValue(":AUDIOPROP",   m_audioProps);
    query.bindValue(":VIDEOPROP",   m_videoProps);
    query.bindValue(":PARTNUMBER",  m_partnumber);
    query.bindValue(":PARTTOTAL",   m_parttotal);
    query.bindValue(":SYNDICATENO", denullify(m_syndicatedepisodenumber));
    query.bindValue(":AIRDATE",     m_airdate ? QString::number(m_airdate):"0000");
    query.bindValue(":ORIGAIRDATE", m_originalairdate);
    query.bindValue(":LSOURCE",     m_listingsource);
    query.bindValue(":SERIESID",    denullify(m_seriesId));
    query.bindValue(":PROGRAMID",   denullify(m_programId));
    query.bindValue(":PREVSHOWN",   m_previouslyshown);
    query.bindValue(":STARS",       m_stars);
    query.bindValue(":SHOWTYPE",    denullify(m_showtype));
    query.bindValue(":TITLEPRON",   denullify(m_title_pronounce));
    query.bindValue(":COLORCODE",   denullify(m_colorcode));
    query.bindValue(":SEASON",      m_season);
    query.bindValue(":EPISODE",     m_episode);
    query.bindValue(":TOTALEPISODES", m_totalepisodes);
    query.bindValue(":INETREF",     denullify(m_inetref));

    if (!query.exec())
    {
        MythDB::DBError(table + " insert", query);
        return 0;
    }

    table = recording ? "recordedrating" : "programrating";
    for (const auto & rating : m_ratings)
    {
        query.prepare(QString("INSERT IGNORE INTO %1 "
                              "       ( chanid, starttime, `system`, rating) "
                              "VALUES (:CHANID, :START,    :SYS,  :RATING)")
                      .arg(table));
        query.bindValue(":CHANID", chanid);
        query.bindValue(":START",  m_starttime);
        query.bindValue(":SYS",    rating.m_system);
        query.bindValue(":RATING", rating.m_rating);

        if (!query.exec())
            MythDB::DBError(QString("%1 insert").arg(table), query);
    }

    if (m_credits)
    {
        for (auto & credit : *m_credits)
            credit.InsertDB(query, chanid, m_starttime, recording);
    }

    add_genres(query, m_genres, chanid, m_starttime);

    return 1;
}

bool ProgramData::ClearDataByChannel(
    uint chanid, const QDateTime &from, const QDateTime &to,
    bool use_channel_time_offset)
{
    std::chrono::seconds secs = 0s;
    if (use_channel_time_offset)
        secs = ChannelUtil::GetTimeOffset(chanid);

    QDateTime newFrom = from.addSecs(secs.count());
    QDateTime newTo   = to.addSecs(secs.count());

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
    std::vector<uint> chanids = ChannelUtil::GetChanIDs(sourceid);

    bool ok = true;
    auto cleardata = [&](uint chanid)
        { ok &= ClearDataByChannel(chanid, from, to, use_channel_time_offset); };
    std::for_each(chanids.cbegin(), chanids.cend(), cleardata);
    return ok;
}

static bool start_time_less_than(const DBEvent *a, const DBEvent *b)
{
    return (a->m_starttime < b->m_starttime);
}

void ProgramData::FixProgramList(QList<ProgInfo*> &fixlist)
{
    std::stable_sort(fixlist.begin(), fixlist.end(), start_time_less_than);

    QList<ProgInfo*>::iterator it = fixlist.begin();
    while (true)
    {
        QList<ProgInfo*>::iterator cur = it;
        ++it;

        // fill in miss stop times
        if ((*cur)->m_endts.isEmpty() || (*cur)->m_startts > (*cur)->m_endts)
        {
            if (it != fixlist.end())
            {
                (*cur)->m_endts   = (*it)->m_startts;
                (*cur)->m_endtime = (*it)->m_starttime;
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
            QList<ProgInfo*>::iterator tokeep;
            QList<ProgInfo*>::iterator todelete;

            if ((*cur)->m_endtime <= (*cur)->m_starttime)
                tokeep = it, todelete = cur;    // NOLINT(bugprone-branch-clone)
            else if ((*it)->m_endtime <= (*it)->m_starttime)
                tokeep = cur, todelete = it;    // NOLINT(bugprone-branch-clone)
            else if (!(*cur)->m_subtitle.isEmpty() &&
                     (*it)->m_subtitle.isEmpty())
                tokeep = cur, todelete = it;
            else if (!(*it)->m_subtitle.isEmpty()  &&
                     (*cur)->m_subtitle.isEmpty())
                tokeep = it, todelete = cur;
            else if (!(*cur)->m_description.isEmpty() &&
                     (*it)->m_description.isEmpty())
                tokeep = cur, todelete = it;
            else
                tokeep = it, todelete = cur;


            LOG(VB_XMLTV, LOG_DEBUG,
                QString("Removing conflicting program: %1 - %2 %3 %4")
                    .arg((*todelete)->m_starttime.toString(Qt::ISODate),
                         (*todelete)->m_endtime.toString(Qt::ISODate),
                         (*todelete)->m_channel,
                         (*todelete)->m_title));

            LOG(VB_XMLTV, LOG_DEBUG,
                QString("Conflicted with            : %1 - %2 %3 %4")
                    .arg((*tokeep)->m_starttime.toString(Qt::ISODate),
                         (*tokeep)->m_endtime.toString(Qt::ISODate),
                         (*tokeep)->m_channel,
                         (*tokeep)->m_title));

            bool step_back = todelete == it;
            it = fixlist.erase(todelete);
            if (step_back)
                --it;
        }
    }
}

/**
 *  \brief Called from mythfilldatabase to bulk insert data into the
 *  program database.
 *
 *  \param sourceid The data source identifier
 *  \param proglist A map of all program information keyed by channel
 *                  identifier
 */
void ProgramData::HandlePrograms(
    uint sourceid, QMap<QString, QList<ProgInfo> > &proglist)
{
    uint unchanged = 0;
    uint updated = 0;

    MSqlQuery query(MSqlQuery::InitCon());

    QMap<QString, QList<ProgInfo> >::const_iterator mapiter;
    for (mapiter = proglist.cbegin(); mapiter != proglist.cend(); ++mapiter)
    {
        if (mapiter.key().isEmpty())
            continue;

        query.prepare(
            "SELECT chanid "
            "FROM channel "
            "WHERE deleted  IS NULL AND "
            "      sourceid = :ID AND "
            "      xmltvid  = :XMLTVID");
        query.bindValue(":ID",      sourceid);
        query.bindValue(":XMLTVID", mapiter.key());

        if (!query.exec())
        {
            MythDB::DBError("ProgramData::HandlePrograms", query);
            continue;
        }

        std::vector<uint> chanids;
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
        // NOLINTNEXTLINE(modernize-loop-convert)
        for (auto it = list.begin(); it != list.end(); ++it)
            sortlist.push_back(&(*it));

        FixProgramList(sortlist);

        for (uint chanid : chanids)
            HandlePrograms(query, chanid, sortlist, unchanged, updated);
    }

    LOG(VB_GENERAL, LOG_INFO,
        QString("Updated programs: %1 Unchanged programs: %2")
                .arg(updated) .arg(unchanged));
}

/**
 *  \brief Called from HandlePrograms to bulk insert data into the
 *  program database.
 *
 *  \param query A mysql query related to all channel ids for
 *               a given source
 *  \param chanid The specific channel id to process
 *  \param sortlist A time sorted list of ProgInfo structures
 *  \param unchanged Set to the number of unchanged programs
 *  \param updated Set to the number of updated programs
 */
void ProgramData::HandlePrograms(MSqlQuery             &query,
                                 uint                   chanid,
                                 const QList<ProgInfo*> &sortlist,
                                 uint &unchanged,
                                 uint &updated)
{
    for (auto *pinfo : std::as_const(sortlist))
    {
        if (IsUnchanged(query, chanid, *pinfo))
        {
            unchanged++;
            continue;
        }

        if (!DeleteOverlaps(query, chanid, *pinfo))
            continue;

        updated += pinfo->InsertDB(query, chanid);
    }
}

int ProgramData::fix_end_times(void)
{
    int count = 0;
    QString chanid;
    QString starttime;
    QString endtime;
    QString querystr;
    MSqlQuery query1(MSqlQuery::InitCon());
    MSqlQuery query2(MSqlQuery::InitCon());

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
                           .arg(starttime, chanid);

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
                               .arg(endtime, chanid, starttime);

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
        "      season          = :SEASON     AND "
        "      episode         = :EPISODE    AND "
        "      totalepisodes   = :TOTALEPISODES AND "
        "      inetref         = :INETREF");

    QString cattype = myth_category_type_to_string(pi.m_categoryType);

    query.bindValue(":CHANID",     chanid);
    query.bindValue(":START",      pi.m_starttime);
    query.bindValue(":END",        pi.m_endtime);
    query.bindValue(":TITLE",      denullify(pi.m_title));
    query.bindValue(":SUBTITLE",   denullify(pi.m_subtitle));
    query.bindValue(":DESC",       denullify(pi.m_description));
    query.bindValue(":CATEGORY",   denullify(pi.m_category));
    query.bindValue(":CATEGORY_TYPE", cattype);
    query.bindValue(":AIRDATE",    pi.m_airdate);
    query.bindValue(":STARS1",     pi.m_stars);
    query.bindValue(":STARS2",     pi.m_stars);
    query.bindValue(":PREVIOUSLYSHOWN", pi.m_previouslyshown);
    query.bindValue(":TITLE_PRONOUNCE", denullify(pi.m_title_pronounce));
    query.bindValue(":AUDIOPROP",  pi.m_audioProps);
    query.bindValue(":VIDEOPROP",  pi.m_videoProps);
    query.bindValue(":SUBTYPES",   pi.m_subtitleType);
    query.bindValue(":PARTNUMBER", pi.m_partnumber);
    query.bindValue(":PARTTOTAL",  pi.m_parttotal);
    query.bindValue(":SERIESID",   denullify(pi.m_seriesId));
    query.bindValue(":SHOWTYPE",   denullify(pi.m_showtype));
    query.bindValue(":COLORCODE",  denullify(pi.m_colorcode));
    query.bindValue(":SYNDICATEDEPISODENUMBER",
                    denullify(pi.m_syndicatedepisodenumber));
    query.bindValue(":PROGRAMID",  denullify(pi.m_programId));
    query.bindValue(":SEASON",     pi.m_season);
    query.bindValue(":EPISODE",    pi.m_episode);
    query.bindValue(":TOTALEPISODES", pi.m_totalepisodes);
    query.bindValue(":INETREF",    denullify(pi.m_inetref));

    if (query.exec() && query.next())
        return query.value(0).toUInt() > 0;

    return false;
}

bool ProgramData::DeleteOverlaps(
    MSqlQuery &query, uint chanid, const ProgInfo &pi)
{
    if (VERBOSE_LEVEL_CHECK(VB_XMLTV, LOG_DEBUG))
    {
        // Get overlaps..
        query.prepare(
            "SELECT title,starttime,endtime "
            "FROM program "
            "WHERE chanid     = :CHANID AND "
            "      starttime >= :START AND "
            "      starttime <  :END;");
        query.bindValue(":CHANID", chanid);
        query.bindValue(":START",  pi.m_starttime);
        query.bindValue(":END",    pi.m_endtime);

        if (!query.exec())
            return false;

        while (query.next())
        {
            LOG(VB_XMLTV, LOG_DEBUG,
                QString("Removing existing program: %1 - %2 %3 %4")
                .arg(MythDate::as_utc(query.value(1).toDateTime()).toString(Qt::ISODate),
                     MythDate::as_utc(query.value(2).toDateTime()).toString(Qt::ISODate),
                     pi.m_channel,
                     query.value(0).toString()));
        }

        if (query.at() == QSql::BeforeFirstRow)
        {
            // Successful query, no results
            return true;
        }
    }

    if (!ClearDataByChannel(chanid, pi.m_starttime, pi.m_endtime, false))
    {
        LOG(VB_XMLTV, LOG_ERR,
            QString("Program delete failed    : %1 - %2 %3 %4")
                .arg(pi.m_starttime.toString(Qt::ISODate),
                     pi.m_endtime.toString(Qt::ISODate),
                     pi.m_channel,
                     pi.m_title));
        return false;
    }

    return true;
}
