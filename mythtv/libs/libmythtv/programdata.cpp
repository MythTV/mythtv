// MythTV headers
#include "mythcontext.h"
#include "mythdbcon.h"
#include "programinfo.h"
#include "programdata.h"

bool operator<(const ProgInfo &a, const ProgInfo &b)
{
    return (a.start < b.start);
}

bool operator>(const ProgInfo &a, const ProgInfo &b)
{
    return (a.start > b.start);
}

bool operator<=(const ProgInfo &a, const ProgInfo &b)
{
    return (a.start <= b.start);
}

static bool conflict(ProgInfo &a, ProgInfo &b)
{
    if ((a.start <= b.start && b.start < a.end) ||
        (b.end <= a.end && a.start < b.end))
        return true;
    return false;
}

void ProgramData::clearDataByChannel(int chanid, QDateTime from, QDateTime to) 
{
    int secs;
    QDateTime newFrom, newTo;

    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("SELECT tmoffset FROM channel where chanid = :CHANID ;");
    query.bindValue(":CHANID", chanid);
    query.exec();
    if (!query.isActive() || query.size() != 1)
    {
        MythContext::DBError("clearDataByChannel", query);
        return;
    }
    query.next();
    secs = query.value(0).toInt();

    secs *= 60;
    newFrom = from.addSecs(secs);
    newTo = to.addSecs(secs);

    query.prepare("DELETE FROM program "
                  "WHERE starttime >= :FROM AND starttime < :TO "
                  "AND chanid = :CHANID ;");
    query.bindValue(":FROM", newFrom);
    query.bindValue(":TO", newTo);
    query.bindValue(":CHANID", chanid);
    query.exec();

    query.prepare("DELETE FROM programrating "
                  "WHERE starttime >= :FROM AND starttime < :TO "
                  "AND chanid = :CHANID ;");
    query.bindValue(":FROM", newFrom);
    query.bindValue(":TO", newTo);
    query.bindValue(":CHANID", chanid);
    query.exec();

    query.prepare("DELETE FROM credits "
                  "WHERE starttime >= :FROM AND starttime < :TO "
                  "AND chanid = :CHANID ;");
    query.bindValue(":FROM", newFrom);
    query.bindValue(":TO", newTo);
    query.bindValue(":CHANID", chanid);
    query.exec();

    query.prepare("DELETE FROM programgenres "
                  "WHERE starttime >= :FROM AND starttime < :TO "
                  "AND chanid = :CHANID ;");
    query.bindValue(":FROM", newFrom);
    query.bindValue(":TO", newTo);
    query.bindValue(":CHANID", chanid);
    query.exec();
}

void ProgramData::clearDataBySource(int sourceid, QDateTime from, QDateTime to) 
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT chanid FROM channel WHERE "
                  "sourceid = :SOURCE ;");
    query.bindValue(":SOURCE", sourceid);

    if (!query.exec())
        MythContext::DBError("Selecting channels per source", query);
        
    if (query.isActive() && query.size() > 0)
    {
        while (query.next())
        {
            int chanid = query.value(0).toInt();
            clearDataByChannel(chanid, from, to);
        }
    }
}

void ProgramData::clearOldDBEntries(void)
{
    MSqlQuery query(MSqlQuery::InitCon());
    QString querystr;
    int offset = 1;

    if (no_delete)
    {
        offset = 7;
        VERBOSE(VB_GENERAL, "Listings Cleanup: Keeping 7 days of data.");
    }

    query.prepare("DELETE FROM oldprogram WHERE airdate < "
                  "DATE_SUB(CURRENT_DATE, INTERVAL 320 DAY);");
    query.exec();

    query.prepare("REPLACE INTO oldprogram (oldtitle,airdate) "
                  "SELECT title,starttime FROM program "
                  "WHERE starttime < NOW() AND manualid = 0 "
                  "GROUP BY title;");
    query.exec();

    query.prepare("DELETE FROM program WHERE starttime <= "
                  "DATE_SUB(CURRENT_DATE, INTERVAL :OFFSET DAY);");
    query.bindValue(":OFFSET", offset);
    query.exec();

    query.prepare("DELETE FROM programrating WHERE starttime <= "
                  "DATE_SUB(CURRENT_DATE, INTERVAL :OFFSET DAY);");
    query.bindValue(":OFFSET", offset);
    query.exec();

    query.prepare("DELETE FROM programgenres WHERE starttime <= "
                  "DATE_SUB(CURRENT_DATE, INTERVAL :OFFSET DAY);");
    query.bindValue(":OFFSET", offset);
    query.exec();

    query.prepare("DELETE FROM credits WHERE starttime <= "
                  "DATE_SUB(CURRENT_DATE, INTERVAL :OFFSET DAY);");
    query.bindValue(":OFFSET", offset);
    query.exec();

    query.prepare("DELETE FROM record WHERE (type = :SINGLE "
                  "OR type = :OVERRIDE OR type = :DONTRECORD) "
                  "AND enddate < NOW();");
    query.bindValue(":SINGLE", kSingleRecord);
    query.bindValue(":OVERRIDE", kOverrideRecord);
    query.bindValue(":DONTRECORD", kDontRecord);
    query.exec();

    MSqlQuery findq(MSqlQuery::InitCon());
    findq.prepare("SELECT record.recordid FROM record "
                  "LEFT JOIN oldfind ON oldfind.recordid = record.recordid "
                  "WHERE type = :FINDONE AND oldfind.findid IS NOT NULL;");
    findq.bindValue(":FINDONE", kFindOneRecord);
    findq.exec();
        
    if (findq.isActive() && findq.size() > 0)
    {
        while (findq.next())
        {
            query.prepare("DELETE FROM record WHERE recordid = :RECORDID;");
            query.bindValue(":RECORDID", findq.value(0).toInt());
            query.exec();
        }
    }
    query.prepare("DELETE FROM oldfind WHERE findid < TO_DAYS(NOW()) - 14;");
    query.exec();

    int cleanOldRecorded = gContext->GetNumSetting( "CleanOldRecorded", 10);

    query.prepare("DELETE FROM oldrecorded WHERE "
                  "recstatus <> :RECORDED AND duplicate = 0 AND "
                  "endtime < DATE_SUB(CURRENT_DATE, INTERVAL :CLEAN DAY);");
    query.bindValue(":RECORDED", rsRecorded);
    query.bindValue(":CLEAN", cleanOldRecorded);
    query.exec();
}

void ProgramData::fixProgramList(QValueList<ProgInfo> *fixlist)
{
    qHeapSort(*fixlist);

    QValueList<ProgInfo>::iterator i = fixlist->begin();
    QValueList<ProgInfo>::iterator cur;
    while (1)    
    {
        cur = i;
        i++;
        // fill in miss stop times
        if ((*cur).endts == "" || (*cur).startts > (*cur).endts)
        {
            if (i != fixlist->end())
            {
                (*cur).endts = (*i).startts;
                (*cur).end = (*i).start;
            }
            else
            {
                (*cur).end = (*cur).start;
                if ((*cur).end < QDateTime((*cur).end.date(), QTime(6, 0)))
                {
                    (*cur).end.setTime(QTime(6, 0));
                }
                else
                {
                   (*cur).end.setTime(QTime(0, 0));
                   (*cur).end.setDate((*cur).end.date().addDays(1));
                }

                (*cur).endts = (*cur).end.toString("yyyyMMddhhmmss").ascii();
            }
        }
        if (i == fixlist->end())
            break;
        // remove overlapping programs
        if (conflict(*cur, *i))
        {
            QValueList<ProgInfo>::iterator tokeep, todelete;

            if ((*cur).end <= (*cur).start)
                tokeep = i, todelete = cur;
            else if ((*i).end <= (*i).start)
                tokeep = cur, todelete = i;
            else if ((*cur).subtitle != "" && (*i).subtitle == "")
                tokeep = cur, todelete = i;
            else if ((*i).subtitle != "" && (*cur).subtitle == "")
                tokeep = i, todelete = cur;
            else if ((*cur).desc != "" && (*i).desc == "")
                tokeep = cur, todelete = i;
            else if ((*i).desc != "" && (*cur).desc == "")
                tokeep = i, todelete = cur;
            else
                tokeep = i, todelete = cur;

            if (!quiet)
            {
                cerr << "removing conflicting program: "
                     << (*todelete).start.toString(Qt::ISODate) << "-"
                     << (*todelete).end.toString(Qt::ISODate) << " "
                     << (*todelete).channel << " "
                     << (*todelete).title.local8Bit() << endl;
                cerr << "conflicted with             : "
                     << (*tokeep).start.toString(Qt::ISODate) << "-"
                     << (*tokeep).end.toString(Qt::ISODate) << " "
                     << (*tokeep).channel << " "
                     << (*tokeep).title.local8Bit() << endl;
                cerr << endl;
            }

            if (todelete == i)
                i = cur;
            fixlist->erase(todelete);
        }
    }
}

void ProgramData::handlePrograms(
    int id, QMap<QString, QValueList<ProgInfo> > *proglist)
{
    int unchanged = 0, updated = 0;
    QMap<QString, QValueList<ProgInfo> >::Iterator mapiter;

    for (mapiter = proglist->begin(); mapiter != proglist->end(); ++mapiter)
    {
        MSqlQuery query(MSqlQuery::InitCon()), chanQuery(MSqlQuery::InitCon());

        if (mapiter.key() == "")
            continue;

        int chanid = 0;

        chanQuery.prepare("SELECT chanid FROM channel WHERE sourceid = :ID AND "
                          "xmltvid = :XMLTVID;"); 
        chanQuery.bindValue(":ID", id);
        chanQuery.bindValue(":XMLTVID", mapiter.key());

        chanQuery.exec();

        if (!chanQuery.isActive() || chanQuery.size() <= 0)
        {
            cerr << "Unknown xmltv channel identifier: " << mapiter.key()
                 << endl << "Skipping channel.\n";
            continue;
        }

        while (chanQuery.next())
        {
            chanid = chanQuery.value(0).toInt();

            if (chanid == 0)
            {
                cerr << "Unknown xmltv channel identifier: " << mapiter.key()
                     << endl << "Skipping channel.\n";
                continue;
            }

            QValueList<ProgInfo> *sortlist = &((*proglist)[mapiter.key()]);

            fixProgramList(sortlist);

            QValueList<ProgInfo>::iterator i = sortlist->begin();
            for (; i != sortlist->end(); i++)
            {
                query.prepare("SELECT * FROM program WHERE "
                              "chanid=:CHANID AND starttime=:START AND "
                              "endtime=:END AND title=:TITLE AND "
                              "subtitle=:SUBTITLE AND description=:DESC AND "
                              "category=:CATEGORY AND "
                              "category_type=:CATEGORY_TYPE AND "
                              "airdate=:AIRDATE AND "
                              "stars >= (:STARS - 0.001) AND stars <= (:STARS + 0.001) AND "
                              "previouslyshown=:PREVIOUSLYSHOWN AND "
                              "title_pronounce=:TITLE_PRONOUNCE AND "
                              "stereo=:STEREO AND subtitled=:SUBTITLED AND "
                              "hdtv=:HDTV AND "
                              "closecaptioned=:CLOSECAPTIONED AND "
                              "partnumber=:PARTNUMBER AND "
                              "parttotal=:PARTTOTAL AND "
                              "seriesid=:SERIESID AND "
                              "showtype=:SHOWTYPE AND "
                              "colorcode=:COLORCODE AND "
                              "syndicatedepisodenumber=:SYNDICATEDEPISODENUMBER AND "
                              "programid=:PROGRAMID;");
                query.bindValue(":CHANID", chanid);
                query.bindValue(":START", (*i).start);
                query.bindValue(":END", (*i).end);
                query.bindValue(":TITLE", (*i).title.utf8());
                query.bindValue(":SUBTITLE", (*i).subtitle.utf8());
                query.bindValue(":DESC", (*i).desc.utf8());
                query.bindValue(":CATEGORY", (*i).category.utf8());
                query.bindValue(":CATEGORY_TYPE", (*i).catType.utf8());
                query.bindValue(":AIRDATE", (*i).airdate.utf8());
                query.bindValue(":STARS", (*i).stars.utf8());
                query.bindValue(":PREVIOUSLYSHOWN", (*i).previouslyshown);
                query.bindValue(":TITLE_PRONOUNCE", (*i).title_pronounce.utf8());
                query.bindValue(":STEREO", (*i).stereo);
                query.bindValue(":SUBTITLED", (*i).subtitled);
                query.bindValue(":HDTV", (*i).hdtv);
                query.bindValue(":CLOSECAPTIONED", (*i).closecaptioned);
                query.bindValue(":PARTNUMBER", (*i).partnumber);
                query.bindValue(":PARTTOTAL", (*i).parttotal);
                query.bindValue(":SERIESID", (*i).seriesid);
                query.bindValue(":SHOWTYPE", (*i).showtype);
                query.bindValue(":COLORCODE", (*i).colorcode);
                query.bindValue(":SYNDICATEDEPISODENUMBER", (*i).syndicatedepisodenumber);
                query.bindValue(":PROGRAMID", (*i).programid);
                query.exec();

                if (query.isActive() && query.size() > 0)
                {
                    unchanged++;
                    continue;
                }

                query.prepare("SELECT title,starttime,endtime FROM program "
                              "WHERE chanid=:CHANID AND starttime>=:START AND "
                              "starttime<:END;");
                query.bindValue(":CHANID", chanid);
                query.bindValue(":START", (*i).start);
                query.bindValue(":END", (*i).end);
                query.exec();

                if (query.isActive() && query.size() > 0)
                {
                    if (!quiet)
                    {
                        while (query.next())
                        {
                            cerr << "removing existing program: "
                                 << query.value(1).toDateTime().toString(Qt::ISODate) << " - "
                                 << query.value(2).toDateTime().toString(Qt::ISODate) << " "
                                 << (*i).channel.local8Bit() << " "
                                 << QString::fromUtf8(query.value(0).toString()).local8Bit()  << endl;
                        }

                        cerr << "inserting new program    : "
                             << (*i).start.toString(Qt::ISODate) << " - " 
                             << (*i).end.toString(Qt::ISODate) << " "
                             << (*i).channel.local8Bit() << " "
                             << (*i).title.local8Bit() << endl << endl;
                    }

                    MSqlQuery subquery(MSqlQuery::InitCon());
                    subquery.prepare("DELETE FROM program WHERE "
                                     "chanid=:CHANID AND starttime>=:START "
                                     "AND starttime<:END;");
                    subquery.bindValue(":CHANID", chanid);
                    subquery.bindValue(":START", (*i).start);
                    subquery.bindValue(":END", (*i).end);

                    subquery.exec();

                    subquery.prepare("DELETE FROM programrating WHERE "
                                     "chanid=:CHANID AND starttime>=:START "
                                     "AND starttime<:END;");
                    subquery.bindValue(":CHANID", chanid);
                    subquery.bindValue(":START", (*i).start);
                    subquery.bindValue(":END", (*i).end);
 
                    subquery.exec();

                    subquery.prepare("DELETE FROM credits WHERE "
                                     "chanid=:CHANID AND starttime>=:START "
                                     "AND starttime<:END;");
                    subquery.bindValue(":CHANID", chanid);
                    subquery.bindValue(":START", (*i).start);
                    subquery.bindValue(":END", (*i).end);

                    subquery.exec();
                }

                query.prepare("INSERT INTO program (chanid,starttime,endtime,"
                              "title,subtitle,description,category,"
                              "category_type,airdate,stars,previouslyshown,"
                              "title_pronounce,stereo,subtitled,hdtv,"
                              "closecaptioned,partnumber,parttotal,"
                              "seriesid,originalairdate,showtype,colorcode,"
                              "syndicatedepisodenumber,programid) "
                              "VALUES(:CHANID,:STARTTIME,:ENDTIME,:TITLE,"
                              ":SUBTITLE,:DESCRIPTION,:CATEGORY,:CATEGORY_TYPE,"
                              ":AIRDATE,:STARS,:PREVIOUSLYSHOWN,"
                              ":TITLE_PRONOUNCE,:STEREO,:SUBTITLED,:HDTV,"
                              ":CLOSECAPTIONED,:PARTNUMBER,:PARTTOTAL,"
                              ":SERIESID,:ORIGINALAIRDATE,:SHOWTYPE,:COLORCODE,"
                              ":SYNDICATEDEPISODENUMBER,:PROGRAMID);");
                query.bindValue(":CHANID", chanid);
                query.bindValue(":STARTTIME", (*i).start);
                query.bindValue(":ENDTIME", (*i).end);
                query.bindValue(":TITLE", (*i).title.utf8());
                query.bindValue(":SUBTITLE", (*i).subtitle.utf8());
                query.bindValue(":DESCRIPTION", (*i).desc.utf8());
                query.bindValue(":CATEGORY", (*i).category.utf8());
                query.bindValue(":CATEGORY_TYPE", (*i).catType.utf8());
                query.bindValue(":AIRDATE", (*i).airdate.utf8());
                query.bindValue(":STARS", (*i).stars.utf8());
                query.bindValue(":PREVIOUSLYSHOWN", (*i).previouslyshown);
                query.bindValue(":TITLE_PRONOUNCE", (*i).title_pronounce.utf8());
                query.bindValue(":STEREO", (*i).stereo);
                query.bindValue(":SUBTITLED", (*i).subtitled);
                query.bindValue(":HDTV", (*i).hdtv);
                query.bindValue(":CLOSECAPTIONED", (*i).closecaptioned);
                query.bindValue(":PARTNUMBER", (*i).partnumber);
                query.bindValue(":PARTTOTAL", (*i).parttotal);
                query.bindValue(":SERIESID", (*i).seriesid);
                query.bindValue(":ORIGINALAIRDATE", (*i).originalairdate);
                query.bindValue(":SHOWTYPE", (*i).showtype);
                query.bindValue(":COLORCODE", (*i).colorcode);
                query.bindValue(":SYNDICATEDEPISODENUMBER", (*i).syndicatedepisodenumber);
                query.bindValue(":PROGRAMID", (*i).programid);
                if (!query.exec())
                    MythContext::DBError("program insert", query);

                updated++;

                QValueList<ProgRating>::iterator j = (*i).ratings.begin();
                for (; j != (*i).ratings.end(); j++)
                {
                    query.prepare("INSERT INTO programrating (chanid,starttime,"
                                  "system,rating) VALUES (:CHANID,:START,:SYS,"
                                  ":RATING);");
                    query.bindValue(":CHANID", chanid);
                    query.bindValue(":START", (*i).start);
                    query.bindValue(":SYS", (*j).system.utf8());
                    query.bindValue(":RATING", (*j).rating.utf8());

                    if (!query.exec())
                        MythContext::DBError("programrating insert", query);
                }

                QValueList<ProgCredit>::iterator k = (*i).credits.begin();
                for (; k != (*i).credits.end(); k++)
                {
                    query.prepare("SELECT person FROM people WHERE "
                                  "name = :NAME;");
                    query.bindValue(":NAME", (*k).name.utf8());
                    if (!query.exec())
                        MythContext::DBError("person lookup", query);

                    int personid = -1;
                    if (query.isActive() && query.size() > 0)
                    {
                        query.next();
                        personid = query.value(0).toInt();
                    }

                    if (personid < 0)
                    {
                        query.prepare("INSERT INTO people (name) VALUES "
                                      "(:NAME);");
                        query.bindValue(":NAME", (*k).name.utf8());
                        if (!query.exec())
                            MythContext::DBError("person insert", query);

                        query.prepare("SELECT person FROM people WHERE "
                                      "name = :NAME;");
                        query.bindValue(":NAME", (*k).name.utf8());
                        if (!query.exec())
                            MythContext::DBError("person lookup", query);

                        if (query.isActive() && query.size() > 0)
                        {
                            query.next();
                            personid = query.value(0).toInt();
                        }
                    }

                    if (personid < 0)
                    {
                        cerr << "Error inserting person\n";
                        continue;
                    }

                    query.prepare("INSERT INTO credits (chanid,starttime,"
                                  "role,person) VALUES "
                                  "(:CHANID, :START, :ROLE, :PERSON);");
                    query.bindValue(":CHANID", chanid);
                    query.bindValue(":START", (*i).start);
                    query.bindValue(":ROLE", (*k).role.utf8());
                    query.bindValue(":PERSON", personid);
                    if (!query.exec())
                    {
                        // be careful of the startime/timestamp "feature"!
                        query.prepare("UPDATE credits SET "
                                      "role = concat(role,',:ROLE'), "
                                      "starttime = :START "
                                      "WHERE chanid = :CHANID AND "
                                      "starttime = :START2 and person = :PERSON");
                        query.bindValue(":ROLE", (*k).role.utf8());
                        query.bindValue(":START", (*i).start);
                        query.bindValue(":CHANID", chanid);
                        query.bindValue(":START2", (*i).start);
                        query.bindValue(":PERSON", personid);

                        if (!query.exec())
                            MythContext::DBError("credits update", query);
                    }
                }
            }
        }
    }
    if (!quiet)
    {
        cerr << "Updated programs: " << updated
             << "  Unchanged programs: " << unchanged << endl;
    }
}

int ProgramData::fix_end_times(void)
{
    int count = 0;
    QString chanid, starttime, endtime, querystr;
    MSqlQuery query1(MSqlQuery::InitCon()), query2(MSqlQuery::InitCon());

    querystr = "SELECT chanid, starttime, endtime FROM program "
               "WHERE (DATE_FORMAT(endtime,'%H%i') = '0000') "
               "ORDER BY chanid, starttime;";

    if (!query1.exec(querystr))
    {
        VERBOSE(VB_IMPORTANT,
                QString("fix_end_times query failed: %1").arg(querystr));
        return -1;
    }

    while (query1.next())
    {
        starttime = query1.value(1).toString();
        chanid = query1.value(0).toString();
        endtime = query1.value(2).toString();

        querystr = QString("SELECT chanid, starttime, endtime FROM program "
                           "WHERE (DATE_FORMAT(starttime, '%%Y-%%m-%%d') = "
                           "'%1') AND chanid = '%2' "
                           "ORDER BY starttime LIMIT 1;")
                           .arg(endtime.left(10))
                           .arg(chanid);

        if (!query2.exec(querystr))
        {
            VERBOSE(VB_IMPORTANT,
                    QString("fix_end_times query failed: %1").arg(querystr));
            return -1;
        }

        if (query2.next() && (endtime != query2.value(1).toString()))
        {
            count++;
            endtime = query2.value(1).toString();
            querystr = QString("UPDATE program SET starttime = '%1', "
                               "endtime = '%2' WHERE (chanid = '%3' AND "
                               "starttime = '%4');")
                               .arg(starttime)
                               .arg(endtime)
                               .arg(chanid)
                               .arg(starttime);

            if (!query2.exec(querystr)) 
            {
                VERBOSE(VB_IMPORTANT,
                       QString("fix_end_times query failed: %1").arg(querystr));
                return -1;
            }
        }
    }

    return count;
}
