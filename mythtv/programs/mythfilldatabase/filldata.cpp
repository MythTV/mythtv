#include <qapplication.h>
#include <qdom.h>
#include <qfile.h>
#include <qstring.h>
#include <qregexp.h>
#include <qstringlist.h>
#include <qvaluelist.h>
#include <qmap.h>
#include <qdatetime.h>
#include <qdir.h>
#include <qfile.h>
#include <qsqldatabase.h>
#include <qsqlquery.h>
#include <qurl.h>

#include <unistd.h>
#include <signal.h>

#include <iostream>
#include <fstream>
#include <string>
#include <cstdlib>
#include <cstdio>
#include <ctime>

#include "libmyth/mythcontext.h"
#include "libmythtv/scheduledrecording.h"
#include "libmythtv/datadirect.h"

using namespace std;

bool interactive = false;
bool channel_preset = false;
bool non_us_updating = false;
bool from_file = false;
bool quiet = false;
bool no_delete = false;
bool isNorthAmerica = false;
bool isJapan = false;
bool interrupted = false;
bool refresh_today = false;
bool refresh_tomorrow = true;
bool refresh_second = false;
bool refresh_tba = true;
int listing_wrap_offset = 0;
bool dd_grab_all = false;
bool dddataretrieved = false;
QString lastdduserid;
DataDirectProcessor ddprocessor;
QString graboptions = "";

class ChanInfo
{
  public:
    ChanInfo() { }
    ChanInfo(const ChanInfo &other) { callsign = other.callsign; 
                                      iconpath = other.iconpath;
                                      chanstr = other.chanstr;
                                      xmltvid = other.xmltvid;
                                      old_xmltvid = other.old_xmltvid;
                                      name = other.name;
                                      freqid = other.freqid;
                                      finetune = other.finetune;
                                    }

    QString callsign;
    QString iconpath;
    QString chanstr;
    QString xmltvid;
    QString old_xmltvid;
    QString name;
    QString freqid;
    QString finetune;
    QString tvformat;
};

struct ProgRating
{
    QString system;
    QString rating;
};

struct ProgCredit
{
    QString role;
    QString name;
};

class ProgInfo
{
  public:
    ProgInfo() { }
    ProgInfo(const ProgInfo &other) { startts = other.startts;
                                      endts = other.endts;
                                      channel = other.channel;
                                      title = other.title;
                                      pronounce = other.pronounce;
                                      clumpidx = other.clumpidx;
                                      clumpmax = other.clumpmax;
                                      subtitle = other.subtitle;
                                      desc = other.desc;
                                      category = other.category;
                                      catType = other.catType;
                                      start = other.start;
                                      end = other.end;
                                      airdate = other.airdate;
                                      stars = other.stars;
                                      ratings = other.ratings;
                                      repeat = other.repeat;
                                      credits = other.credits;
                                    }

    QString startts;
    QString endts;
    QString channel;
    QString title;
    QString pronounce;
    QString subtitle;
    QString desc;
    QString category;
    QString catType;
    QString airdate;
    QString stars;
    QValueList<ProgRating> ratings;

    QDateTime start;
    QDateTime end;

    QString clumpidx;
    QString clumpmax;

    bool repeat;
    QValueList<ProgCredit> credits;
};

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

struct Source
{
    int id;
    QString name;
    QString xmltvgrabber;
    QString userid;
    QString password;
    QString lineupid;
};

void clearDataByChannel(int chanid, QDateTime from, QDateTime to) 
{
    QSqlQuery query;
    QString querystr;

    querystr.sprintf("DELETE FROM program "
                     "WHERE starttime >= '%s' AND starttime < '%s' "
                     "AND chanid = %d;",
                     from.toString("yyyyMMddhhmmss").ascii(),
                     to.toString("yyyyMMddhhmmss").ascii(),
                     chanid);
    query.exec(querystr);

    querystr.sprintf("DELETE FROM programrating WHERE starttime >= '%s' "
                     " AND starttime < '%s' AND chanid = %d;",
                     from.toString("yyyyMMddhhmmss").ascii(),
                     to.toString("yyyyMMddhhmmss").ascii(),
                     chanid);
    query.exec(querystr);

    querystr.sprintf("DELETE FROM credits WHERE starttime >= '%s' "
                     "AND starttime < '%s' AND chanid = %d;",
                     from.toString("yyyyMMddhhmmss").ascii(),
                     to.toString("yyyyMMddhhmmss").ascii(),
                     chanid);
    query.exec(querystr);

    querystr.sprintf("DELETE FROM programgenres WHERE starttime >= '%s' "
                     "AND starttime < '%s' AND chanid = %d;",
                     from.toString("yyyyMMddhhmmss").ascii(),
                     to.toString("yyyyMMddhhmmss").ascii(),
                     chanid);
    query.exec(querystr);
}

void clearDataBySource(int sourceid, QDateTime from, QDateTime to) 
{
    QString querystr= QString("SELECT chanid FROM channel WHERE "
                              "sourceid = \"%0\";").arg(sourceid);

    QSqlQuery query;

    if (!query.exec(querystr))
        MythContext::DBError("Selecting channels per source", query);
        
    if (query.isActive() && query.numRowsAffected() > 0)
    {
        while (query.next())
        {
            int chanid = query.value(0).toInt();
            clearDataByChannel(chanid, from, to);
        }
    }
}

// DataDirect stuff

void DataDirectStationUpdate(Source source)
{
    QSqlQuery query;
    QString querystr;
    int chanid;

    ddprocessor.updateStationViewTable();

    querystr = "SELECT dd_v_station.stationid,dd_v_station.callsign,"
               "dd_v_station.stationname,dd_v_station.channel "
               "FROM dd_v_station LEFT JOIN channel ON "
               "dd_v_station.stationid = channel.xmltvid "
               "WHERE channel.chanid IS NULL;";

    QSqlQuery query1;

    if (!query1.exec(querystr))
        MythContext::DBError("Selecting new channels", query1);

    if (query1.isActive() && query1.numRowsAffected() > 0)
    {
        while (query1.next())
        {
            chanid = source.id * 1000 + query1.value(3).toString().toInt();

            while(1)
            {
                querystr.sprintf("SELECT channum FROM channel WHERE "
                                 "chanid = %d;", chanid);
                if (!query.exec(querystr))
                    break;

                if (query.isActive() && query.numRowsAffected() > 0)
                    chanid++;
                else
                    break;
            }

            QString xmltvid = query1.value(0).toString();
            QString callsign = query1.value(1).toString();
            if (callsign == QString::null || callsign == "")
                callsign = QString::number(chanid);
            QString name = query1.value(2).toString();
            QString channel = query1.value(3).toString();

            query.prepare("INSERT INTO channel (chanid,channum,sourceid,"
                          "callsign, name, xmltvid, freqid, tvformat) "
                          "VALUES (:CHANID,:CHANNUM,:SOURCEID,:CALLSIGN,"
                          ":NAME,:XMLTVID,:FREQID,:TVFORMAT);");

            query.bindValue(":CHANID", chanid);
            query.bindValue(":CHANNUM", channel);
            query.bindValue(":SOURCEID", source.id);
            query.bindValue(":CALLSIGN", callsign);
            query.bindValue(":NAME", name);
            query.bindValue(":XMLTVID", xmltvid);
            query.bindValue(":FREQID", channel);
            query.bindValue(":TVFORMAT", "Default");
        
            if (!query.exec())
                MythContext::DBError("Inserting new channel", query);
        }
    }

    // Now, update the data for channels which do exist

    QSqlQuery dd_station_info("SELECT callsign, stationname, stationid"
            " FROM dd_v_station;");

    if (dd_station_info.first())
    {
        QSqlQuery dd_update;
        dd_update.prepare("UPDATE channel SET callsign = :CALLSIGN,"
                " name = :NAME WHERE xmltvid = :STATIONID"
                " AND sourceid = :SOURCEID;");
        do
        {
            dd_update.bindValue(":CALLSIGN", dd_station_info.value(0));
            dd_update.bindValue(":NAME", dd_station_info.value(1));
            dd_update.bindValue(":STATIONID", dd_station_info.value(2));
            dd_update.bindValue(":SOURCEID", source.id);
            if (!dd_update.exec())
            {
                MythContext::DBError("Updating channel table",
                        dd_update.lastQuery());
            }
        } while (dd_station_info.next());
    }

    // Now, delete any channels which no longer exist
    // (Not currently done in standard program -- need to verify if required)
}

void DataDirectProgramUpdate(Source source) 
{
    QSqlQuery query;
   
    //cerr << "Creating program view table...\n";
    ddprocessor.updateProgramViewTable(source.id);
    //cerr <<  "Finished creating program view table...\n";

    //cerr << "Adding rows to main program table from view table..\n";
    if (!query.exec("INSERT IGNORE INTO program (chanid, starttime, endtime, "
                    "title, subtitle, description, category, category_type, "
                    "airdate, stars, previouslyshown, stereo, subtitled, "
                    "hdtv, closecaptioned, partnumber, parttotal, seriesid, "
                    "originalairdate, colorcode, syndicatedepisodenumber, "
                    "programid) SELECT chanid, starttime, endtime, title, "
                    "subtitle, description, dd_genre.class, category_type, "
                    "airdate, stars, previouslyshown, stereo, subtitled, hdtv, "
                    "closecaptioned, partnumber, parttotal, seriesid, "
                    "originalairdate, colorcode, syndicatedepisodenumber, "
                    "dd_v_program.programid FROM dd_v_program "
                    "LEFT JOIN dd_genre ON ("
                    "dd_v_program.programid = dd_genre.programid AND "
                    "dd_genre.relevance = '0');"))
        MythContext::DBError("Inserting into program table", query);

    //cerr << "Finished adding rows to main program table...\n";
    //cerr << "Adding program ratings...\n";

    if (!query.exec("INSERT IGNORE INTO programrating (chanid, starttime, "
                    "system, rating) SELECT chanid, starttime, 'MPAA', "
                    "mpaarating FROM dd_v_program WHERE mpaarating != '';"))
        MythContext::DBError("Inserting into programrating table", query);

    if (!query.exec("INSERT IGNORE INTO programrating (chanid, starttime, "
                    "system, rating) SELECT chanid, starttime, 'VCHIP', "
                    "tvrating FROM dd_v_program WHERE tvrating != '';"))
        MythContext::DBError("Inserting into programrating table", query);

    //cerr << "Finished adding program ratings...\n";
    //cerr << "Populating people table from production crew list...\n";

    if (!query.exec("INSERT IGNORE INTO people (name) SELECT fullname "
                    "FROM dd_productioncrew;"))
        MythContext::DBError("Inserting into people table", query);

    //cerr << "Finished adding people...\n";
    //cerr << "Adding credits entries from production crew list...\n";

    if (!query.exec("INSERT IGNORE INTO credits (chanid, starttime, person, "
                    "role) SELECT chanid, starttime, person, role "
                    "FROM dd_productioncrew, dd_v_program, people "
                    "WHERE "
                    "((dd_productioncrew.programid = dd_v_program.programid) "
                    "AND (dd_productioncrew.fullname = people.name));"))
        MythContext::DBError("Inserting into credits table", query);

    //cerr << "Finished inserting credits...\n";
    //cerr << "Adding genres...\n";

    if (!query.exec("INSERT IGNORE INTO programgenres (chanid, starttime, "
                    "relevance, genre) SELECT chanid, starttime, "
                    "relevance, class FROM dd_v_program, dd_genre "
                    "WHERE (dd_v_program.programid = dd_genre.programid);"))
        MythContext::DBError("Inserting into programgenres table",query);

    //cerr << "Done...\n";
}

bool grabDDData(Source source, int poffset, QDate pdate) 
{
    ddprocessor.setLineup(source.lineupid);
    ddprocessor.setUserID(source.userid);
    ddprocessor.setPassword(source.password);

    bool needtoretrieve = true;

    if (source.userid != lastdduserid)
        dddataretrieved = false;

    if (dd_grab_all && dddataretrieved)
        needtoretrieve = false;

    QDateTime qdtNow = QDateTime::currentDateTime();
    QSqlQuery query;
    QString status = "currently running.";

    query.exec(QString("UPDATE settings SET data ='%1' "
                       "WHERE value='mythfilldatabaseLastRunStart'")
                       .arg(qdtNow.toString("yyyy-MM-dd hh:mm")));

    if (needtoretrieve)
    {
        cerr << "Retrieving datadirect data... \n";
        if (dd_grab_all) 
        {
            cerr << "Grabbing ALL available data...\n";
            ddprocessor.grabAllData();
        }
        else
        {
            cerr << "Grabbing data for " << pdate.toString() 
                 << " offset " << poffset << "\n";
            QDateTime fromdatetime = QDateTime(pdate);
            QDateTime todatetime;
            fromdatetime.setTime_t(QDateTime(pdate).toTime_t(),Qt::UTC);
            fromdatetime = fromdatetime.addDays(poffset);
            todatetime = fromdatetime.addDays(1);
            cerr << "From : " << fromdatetime.toString() 
                 << " To : " << todatetime.toString() << " (UTC)\n";
            ddprocessor.grabData(false, fromdatetime, todatetime);
        }

        dddataretrieved = true;
        lastdduserid = source.userid;
    }
    else
    {
        cerr << "Using existing grabbed data in temp tables..\n";
    }

    cerr << "Grab complete.  Actual data from " 
         << ddprocessor.getActualListingsFrom().toString() << " "
         << "to " << ddprocessor.getActualListingsTo().toString() 
         << " (UTC) \n";

    qdtNow = QDateTime::currentDateTime();
    query.exec(QString("UPDATE settings SET data ='%1' "
                       "WHERE value='mythfilldatabaseLastRunEnd'")
                       .arg(qdtNow.toString("yyyy-MM-dd hh:mm")));

    cerr << "Clearing data for source...\n";
    QDateTime basedt = QDateTime(QDate(1970, 1, 1));
    QDateTime fromlocaldt;
    int timesecs = basedt.secsTo(ddprocessor.getActualListingsFrom());
    fromlocaldt.setTime_t(timesecs);
    QDateTime tolocaldt;
    timesecs = basedt.secsTo(ddprocessor.getActualListingsTo());
    tolocaldt.setTime_t(timesecs);

    cerr << "Clearing from " << fromlocaldt.toString() 
         << " to " << tolocaldt.toString() << " (localtime)\n";

    clearDataBySource(source.id, fromlocaldt,tolocaldt);
    cerr << "Data for source cleared...\n";

    cerr << "Main temp tables populated.  Updating myth channels...\n";
    DataDirectStationUpdate(source);
    cerr << "Channels updated..  Updating programs...\n";
    DataDirectProgramUpdate(source);

    return true;
}

// XMLTV stuff

QDateTime fromXMLTVDate(QString &text)
{
    int year, month, day, hour, min, sec;
    QDate ldate;
    QTime ltime;
    QDateTime dt;

    if (text == QString::null)
        return dt;

    if (text.find(QRegExp("^\\d{12}")) != 0)
        return dt;

    year = atoi(text.mid(0, 4).ascii());
    month = atoi(text.mid(4, 2).ascii());
    day = atoi(text.mid(6, 2).ascii());
    hour = atoi(text.mid(8, 2).ascii());
    min = atoi(text.mid(10, 2).ascii());
    if (text.find(QRegExp("^\\d\\d"), 12) == 0)
        sec = atoi(text.mid(12, 2).ascii());
    else
        sec = 0;

    ldate = QDate(year, month, day);
    ltime = QTime(hour, min, sec);

    dt = QDateTime(ldate, ltime);

    return dt; 
}

QString getFirstText(QDomElement element)
{
    for (QDomNode dname = element.firstChild(); !dname.isNull();
         dname = dname.nextSibling())
    {
        QDomText t = dname.toText();
        if (!t.isNull())
            return t.data();
    }
    return "";
}

ChanInfo *parseChannel(QDomElement &element, QUrl baseUrl) 
{
    ChanInfo *chaninfo = new ChanInfo;

    QString xmltvid = element.attribute("id", "");
    QStringList split = QStringList::split(" ", xmltvid);

    bool xmltvisjunk = false;

    if (isNorthAmerica)
    {
        if (xmltvid.contains("zap2it"))
        {
            xmltvisjunk = true;
            chaninfo->chanstr = "";
            chaninfo->xmltvid = xmltvid;
            chaninfo->callsign = "";
        }
        else
        {
            chaninfo->xmltvid = split[0];
            chaninfo->chanstr = split[0];
            if (split.size() > 1)
                chaninfo->callsign = split[1];
            else
                chaninfo->callsign = "";
        }
    }
    else
    {
        chaninfo->callsign = "";
        chaninfo->chanstr = "";
        chaninfo->xmltvid = xmltvid;
    }

    chaninfo->iconpath = "";
    chaninfo->name = "";
    chaninfo->finetune = "";
    chaninfo->tvformat = "Default";

    for (QDomNode child = element.firstChild(); !child.isNull();
         child = child.nextSibling())
    {
        QDomElement info = child.toElement();
        if (!info.isNull())
        {
            if (info.tagName() == "icon")
            {
                QUrl iconUrl(baseUrl, info.attribute("src", ""), true);
                chaninfo->iconpath = iconUrl.toString();
            }
            else if (info.tagName() == "display-name")
            {
                if (chaninfo->name.length() == 0)
                {
                    chaninfo->name = info.text();
                    if (xmltvisjunk)
                    {
                        QStringList split = QStringList::split(" ", 
                                                               chaninfo->name);
          
                        if (split[0] == "Channel")
                        { 
                            chaninfo->old_xmltvid = split[1];
                            chaninfo->chanstr = split[1];
                            if (split.size() > 2)
                                chaninfo->callsign = split[2];
                        }
                        else
                        {
                            chaninfo->old_xmltvid = split[0];
                            chaninfo->chanstr = split[0];
                            if (split.size() > 1)
                                chaninfo->callsign = split[1];
                        }
                    }
                }
                else if (isJapan && chaninfo->callsign.length() == 0)
                {
                    chaninfo->callsign = info.text();
                }
                else if (chaninfo->chanstr.length() == 0)
                {
                    chaninfo->chanstr = info.text();
                }
            }
        }
    }

    chaninfo->freqid = chaninfo->chanstr;
    return chaninfo;
}

int TimezoneToInt (QString timezone)
{
    // we signal an error by setting it invalid (> 840min = 14hr)
    int result = 841;

    if (timezone.length() == 5)
    {
        bool ok;

        result = timezone.mid(1,2).toInt(&ok, 10);

        if (!ok)
            result = 841;
        else
        {
            result *= 60;

            int min = timezone.right(2).toInt(&ok, 10);

            if (!ok)
                result = 841;
            else
            {
                result += min;
                if (timezone.left(1) == "-")
                    result *= -1;
            }
        }
    }
    return result;
}

void addTimeOffset(QString &timestr, int localTimezoneOffset)
{
    if (timestr.isEmpty() || abs(localTimezoneOffset) > 840)
        return;

    QStringList split = QStringList::split(" ", timestr);
    QString ts = split[0];
    int ts_offset = localTimezoneOffset;

    if (split.size() > 1)
    {
        QString tmp = split[1];
        tmp.stripWhiteSpace();

        ts_offset = TimezoneToInt(tmp);
        if (abs(ts_offset) > 840)
            ts_offset = localTimezoneOffset;
    }

    int diff = localTimezoneOffset - ts_offset;
    int year = 0, month = 0, day = 0, hour = 0, min = 0, sec = 0;

    if (diff != 0)
    {
        bool ok;
                    
            if (ts.length() == 14)
            {
                year  = ts.left(4).toInt(&ok, 10);
                month = ts.mid(4,2).toInt(&ok, 10);
                day   = ts.mid(6,2).toInt(&ok, 10);
                hour  = ts.mid(8,2).toInt(&ok, 10);
                min   = ts.mid(10,2).toInt(&ok, 10);
                sec   = ts.mid(12,2).toInt(&ok, 10);
            }
            else if (ts.length() == 12)
            {
                year  = ts.left(4).toInt(&ok, 10);
                month = ts.mid(4,2).toInt(&ok, 10);
                day   = ts.mid(6,2).toInt(&ok, 10);
                hour  = ts.mid(8,2).toInt(&ok, 10);
                min   = ts.mid(10,2).toInt(&ok, 10);
                sec   = 0;
            }
            else
            {
                diff = 0;
                cerr << "Ignoring unknown timestamp format: " << ts << endl;
            }
    }

    if (diff != 0)
    {
        QDateTime dt = QDateTime(QDate(year, month, day),QTime(hour, min, sec));
        dt = dt.addSecs(diff * 60 );
        timestr = dt.toString("yyyyMMddhhmmss");
    }
}

void parseCredits(QDomElement &element, ProgInfo *pginfo)
{
    for (QDomNode child = element.firstChild(); !child.isNull();
         child = child.nextSibling())
    {
        QDomElement info = child.toElement();
        if (!info.isNull())
        {
            ProgCredit credit;
            credit.role = info.tagName();
            credit.name = getFirstText(info);
            pginfo->credits.append(credit);
        }
    }
}

ProgInfo *parseProgram(QDomElement &element, int localTimezoneOffset)
{
    ProgInfo *pginfo = new ProgInfo;
 
    QString text = element.attribute("start", "");
    addTimeOffset(text, localTimezoneOffset);
    pginfo->startts = text;

    text = element.attribute("stop", "");
    addTimeOffset(text, localTimezoneOffset);
    pginfo->endts = text;

    text = element.attribute("channel", "");
    QStringList split = QStringList::split(" ", text);   
 
    pginfo->channel = split[0];

    text = element.attribute("clumpidx", "");
    if (!text.isEmpty()) 
    {
        split = QStringList::split("/", text);
        pginfo->clumpidx = split[0];
        pginfo->clumpmax = split[1];
    }

    pginfo->start = fromXMLTVDate(pginfo->startts);
    pginfo->end = fromXMLTVDate(pginfo->endts);

    pginfo->subtitle = pginfo->title = pginfo->desc = pginfo->category = "";
    pginfo->catType = "";
    pginfo->repeat = false;   
 
    for (QDomNode child = element.firstChild(); !child.isNull();
         child = child.nextSibling())
    {
        QDomElement info = child.toElement();
        if (!info.isNull())
        {
            if (info.tagName() == "title")
            {
                if (isJapan)
                {
                    if (info.attribute("lang") == "ja_JP")
                    {
                        pginfo->title = getFirstText(info);
                    }
                    else if (info.attribute("lang") == "ja_JP@kana")
                    {
                        pginfo->pronounce = getFirstText(info);
                    }
                }
                else if (pginfo->title == "")
                {
                    pginfo->title = getFirstText(info);
                }
            }
            else if (info.tagName() == "sub-title" && pginfo->subtitle == "")
            {
                pginfo->subtitle = getFirstText(info);
            }
            else if (info.tagName() == "desc" && pginfo->desc == "")
            {
                pginfo->desc = getFirstText(info);
            }
            else if (info.tagName() == "category")
            {
                QString cat = getFirstText(info);
                if (pginfo->category == "")
                {
                    pginfo->category = cat;
                }
                else if (cat == "movie" || cat == "series" || cat == "sports" ||
                         cat == "tvshow")
                    /* Hack until we have the new XMLTV DTD with category
                       "system"s.
                       I can't use a new tag, because I'd then
                       be incompliant with the (current) XMLTV (I think),
                       unless I use XML namespaces etc., and it's category
                       info after all, just formalized and narrow. */
                {
                    if (pginfo->catType == "")
                        pginfo->catType = cat;
                }

                if (cat == "Film" && !isNorthAmerica)
                {
                    // Hack for tv_grab_uk_rt
                    pginfo->catType = cat;
                }
            }
            else if (info.tagName() == "date" && pginfo->airdate == "")
            {
                pginfo->airdate = getFirstText(info);

                if (4 != pginfo->airdate.length())
                    pginfo->airdate = "";
            }
            else if (info.tagName() == "star-rating")
            {
                QDomNodeList values = info.elementsByTagName("value");
                QDomElement item;
                QString stars, num, den;
                float avg = 0.0;
                // not sure why the XML suggests multiple ratings,
                // but the following will average them anyway.
                for (unsigned int i = 0; i < values.length(); i++)
                {
                    item = values.item(i).toElement();
                    if (item.isNull())
                        continue;
                    stars = getFirstText(item);
                    num = stars.section('/', 0, 0);
                    den = stars.section('/', 1, 1);
                    if (0.0 >= den.toFloat())
                        continue;
                    avg *= i/(i+1);
                    avg += (num.toFloat()/den.toFloat()) / (i+1);
                }
                pginfo->stars.setNum(avg);
            }
            else if (info.tagName() == "rating")
            {
                // again, the structure of ratings seems poorly represented
                // in the XML.  no idea what we'd do with multiple values.
                QDomNodeList values = info.elementsByTagName("value");
                QDomElement item = values.item(0).toElement();
                if (item.isNull())
                    continue;
                ProgRating rating;
                rating.system = info.attribute("system", "");
                rating.rating = getFirstText(item);
                if ("" != rating.system)
                    pginfo->ratings.append(rating);
            }
            else if (info.tagName() == "previously-shown")
            {
                pginfo->repeat = true;
            } 
            else if (info.tagName() == "credits")
            {
                parseCredits(info, pginfo);
            }  
        }
    }

    if (pginfo->category == "" && pginfo->catType != "")
        pginfo->category = pginfo->catType;

    /* Do what MythWeb does and assume that programmes with
       star-rating in America are movies. This allows us to
       unify app code with grabbers which explicitly deliver that
       info. */
    if (isNorthAmerica && pginfo->catType == "" &&
        pginfo->stars != "" && pginfo->airdate != "")
        pginfo->catType = "movie";

    if (pginfo->airdate == "")
        pginfo->airdate = QDate::currentDate().toString("yyyy");

    return pginfo;
}
                  
void parseFile(QString filename, QValueList<ChanInfo> *chanlist,
               QMap<QString, QValueList<ProgInfo> > *proglist)
{
    QDomDocument doc;
    QFile f;

    if (filename.compare("-"))
    {
        f.setName(filename);
        if (!f.open(IO_ReadOnly))
            return;
    }
    else
    {
        if (!f.open(IO_ReadOnly, stdin))
            return;
    }

    QString errorMsg = "unknown";
    int errorLine = 0;
    int errorColumn = 0;

    if (!doc.setContent(&f, &errorMsg, &errorLine, &errorColumn))
    {
        cout << "Error in " << errorLine << ":" << errorColumn << ": "
             << errorMsg << endl;

        f.close();
        return;
    }

    f.close();

    // now we calculate the localTimezoneOffset, so that we can fix
    // the programdata if needed
    QString config_offset = gContext->GetSetting("TimeOffset", "None");
    // we disable this feature by setting it invalid (> 840min = 14hr)
    int localTimezoneOffset = 841;

    if (config_offset == "Auto")
    {
        time_t now = time(NULL);
        struct tm local_tm;
        localtime_r(&now, &local_tm);
        localTimezoneOffset = local_tm.tm_gmtoff / 60;
    }
    else if (config_offset != "None")
    {
        localTimezoneOffset = TimezoneToInt(config_offset);
        if (abs(localTimezoneOffset) > 840)
            cerr << "Ignoring invalid TimeOffset " << config_offset << endl;
    }

    QDomElement docElem = doc.documentElement();

    QUrl baseUrl(docElem.attribute("source-data-url", ""));

    QString aggregatedTitle;
    QString aggregatedDesc;
    QString groupingTitle;
    QString groupingDesc;

    QDomNode n = docElem.firstChild();
    while (!n.isNull())
    {
        QDomElement e = n.toElement();
        if (!e.isNull()) 
        {
            if (e.tagName() == "channel")
            {
                ChanInfo *chinfo = parseChannel(e, baseUrl);
                chanlist->push_back(*chinfo);
                delete chinfo;
            }
            else if (e.tagName() == "programme")
            {
                ProgInfo *pginfo = parseProgram(e, localTimezoneOffset);

                if (pginfo->startts == pginfo->endts)
                {
                    /* Not a real program : just a grouping marker */
                    if (!pginfo->title.isEmpty())
                        groupingTitle = pginfo->title + " : ";

                    if (!pginfo->desc.isEmpty())
                        groupingDesc = pginfo->desc + " : ";
                }
                else
                {
                    if (pginfo->clumpidx.isEmpty())
                    {
                        if (!groupingTitle.isEmpty())
                        {
                            pginfo->title.prepend(groupingTitle);
                            groupingTitle = "";
                        }

                        if (!groupingDesc.isEmpty())
                        {
                            pginfo->desc.prepend(groupingDesc);
                            groupingDesc = "";
                        }

                        (*proglist)[pginfo->channel].push_back(*pginfo);
                    }
                    else
                    {
                        /* append all titles/descriptions from one clump */
                        if (pginfo->clumpidx.toInt() == 0)
                        {
                            aggregatedTitle = "";
                            aggregatedDesc = "";
                        }

                        if (!pginfo->title.isEmpty())
                        {
                            if (!aggregatedTitle.isEmpty())
                                aggregatedTitle.append(" | ");
                            aggregatedTitle.append(pginfo->title);
                        }

                        if (!pginfo->desc.isEmpty())
                        {
                            if (!aggregatedDesc.isEmpty())
                                aggregatedDesc.append(" | ");
                            aggregatedDesc.append(pginfo->desc);
                        }    
                        if (pginfo->clumpidx.toInt() == 
                            pginfo->clumpmax.toInt() - 1)
                        {
                            pginfo->title = aggregatedTitle;
                            pginfo->desc = aggregatedDesc;
                            (*proglist)[pginfo->channel].push_back(*pginfo);
                        }
                    }
                }
                delete pginfo;
            }
        }
        n = n.nextSibling();
    }

    return;
}

bool conflict(ProgInfo &a, ProgInfo &b)
{
    if ((a.start <= b.start && b.start < a.end) ||
        (b.end <= a.end && a.start < b.end))
        return true;
    return false;
}

void fixProgramList(QValueList<ProgInfo> *fixlist)
{
    qHeapSort(*fixlist);

    QValueList<ProgInfo>::iterator i = fixlist->begin();
    QValueList<ProgInfo>::iterator cur;
    while (1)    
    {
        cur = i;
        i++;
        // fill in miss stop times
        if ((*cur).endts == "")
        {
            if (i != fixlist->end())
            {
                (*cur).endts = (*i).startts;
                (*cur).end = (*i).start;
            }
            else
            {
                (*cur).end = (*cur).start;
                (*cur).end.setTime(QTime(0, 0));
                (*cur).end.setDate((*cur).end.date().addDays(1));

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
            else if((*i).subtitle != "" && (*cur).subtitle == "")
                tokeep = i, todelete = cur;
            else if((*cur).desc != "" && (*i).desc == "")
                tokeep = cur, todelete = i;
            else if((*i).desc != "" && (*cur).desc == "")
                tokeep = i, todelete = cur;
            else
                tokeep = i, todelete = cur;

            cerr << "removing conflicting program: "
                 << (*todelete).channel << " "
                 << (*todelete).title.local8Bit() << " "
                 << (*todelete).startts << "-" << (*todelete).endts << endl;
            cerr << "conflicted with             : "
                 << (*tokeep).channel << " "
                 << (*tokeep).title.local8Bit() << " "
                 << (*tokeep).startts << "-" <<   (*tokeep).endts << endl;
            cerr << endl;

            if (todelete == i)
                i = cur;
            fixlist->erase(todelete);
        }
    }
}

QString getResponse(const QString &query, const QString &def)
{
    cout << query;

    if (def != "")
    {
        cout << " [" << (const char *)def.local8Bit() << "]  ";
    }
    
    char response[80];
    cin.getline(response, 80);

    QString qresponse = QString::fromLocal8Bit(response);

    if (qresponse == "")
        qresponse = def;

    return qresponse;
}

unsigned int promptForChannelUpdates(QValueList<ChanInfo>::iterator chaninfo, 
                                     unsigned int chanid)
{
    if (chanid == 0)
    {
        // Default is 0 to allow rapid skipping of many channels,
        // in some xmltv outputs there may be over 100 channel, but
        // only 10 or so that are available in each area.
        chanid = atoi(getResponse("Choose a channel ID (positive integer) ",
                                  "0"));

        // If we wish to skip this channel, use the default 0 and return.
        if (chanid == 0)
            return(0);
    }

    (*chaninfo).name = getResponse("Choose a channel name (any string, "
                                   "long version) ",(*chaninfo).name);
    (*chaninfo).callsign = getResponse("Choose a channel callsign (any string, "
                                       "short version) ",(*chaninfo).callsign);

    if (channel_preset)
    {
        (*chaninfo).chanstr = getResponse("Choose a channel preset (0..999) ",
                                         (*chaninfo).chanstr);
        (*chaninfo).freqid  = getResponse("Choose a frequency id (just like "
                                          "xawtv) ",(*chaninfo).freqid);
    }
    else
    {
        (*chaninfo).chanstr  = getResponse("Choose a channel number (just like "
                                           "xawtv) ",(*chaninfo).chanstr);
        (*chaninfo).freqid = (*chaninfo).chanstr;
    }

    (*chaninfo).finetune = getResponse("Choose a channel fine tune offset (just"
                                       " like xawtv) ",(*chaninfo).finetune);

    (*chaninfo).tvformat = getResponse("Choose a TV format "
                                       "(PAL/SECAM/NTSC/ATSC/Default) ",
                                       (*chaninfo).tvformat);

    (*chaninfo).iconpath = getResponse("Choose a channel icon image (any path "
                                       "name) ",(*chaninfo).iconpath);

    return(chanid);
}

void handleChannels(int id, QValueList<ChanInfo> *chanlist)
{
    QString fileprefix = QDir::homeDirPath() + "/.mythtv";

    QDir dir(fileprefix);
    if (!dir.exists())
        dir.mkdir(fileprefix);

    fileprefix += "/channels";

    dir = QDir(fileprefix);
    if (!dir.exists())
        dir.mkdir(fileprefix);

    QDir::setCurrent(fileprefix);

    fileprefix += "/";

    QValueList<ChanInfo>::iterator i = chanlist->begin();
    for (; i != chanlist->end(); i++)
    {
        QString localfile = "";

        if ((*i).iconpath != "")
        {
            QDir remotefile = QDir((*i).iconpath);
            QString filename = remotefile.dirName();

            localfile = fileprefix + filename;
            QFile actualfile(localfile);
            if (!actualfile.exists())
            {
                QString command = QString("wget ") + (*i).iconpath;
                system(command);
            }
        }

        QSqlQuery query;

        QString querystr;

        if ((*i).old_xmltvid != QString::null) {
            querystr.sprintf("SELECT xmltvid FROM channel WHERE xmltvid = \"%s\"",
                             (*i).old_xmltvid.ascii());
            query.exec(querystr);

            if (query.isActive() && query.numRowsAffected() > 0) {
                if (!quiet)
                    cout << "Converting old xmltvid (" << (*i).old_xmltvid << ") to new ("
                         << (*i).xmltvid << ")\n";

                query.exec(QString("UPDATE channel SET xmltvid = '%1' WHERE xmltvid = '%2'")
                            .arg((*i).xmltvid)
                            .arg((*i).old_xmltvid));

                if (!query.numRowsAffected())
                    MythContext::DBError("xmltvid conversion",query);
            }
        }

        querystr.sprintf("SELECT chanid,name,callsign,channum,finetune,"
                         "icon,freqid,tvformat FROM channel WHERE "
                         "xmltvid = \"%s\" AND sourceid = %d;", 
                         (*i).xmltvid.ascii(), id); 

        query.exec(querystr);
        if (query.isActive() && query.numRowsAffected() > 0)
        {
            query.next();

            QString chanid = query.value(0).toString();
            if (interactive)
            {
                QString name     = QString::fromUtf8(query.value(1).toString());
                QString callsign = QString::fromUtf8(query.value(2).toString());
                QString chanstr  = QString::fromUtf8(query.value(3).toString());
                QString finetune = QString::fromUtf8(query.value(4).toString());
                QString icon     = QString::fromUtf8(query.value(5).toString());
                QString freqid   = QString::fromUtf8(query.value(6).toString());
                QString tvformat = QString::fromUtf8(query.value(7).toString());

                cout << "### " << endl;
                cout << "### Existing channel found" << endl;
                cout << "### " << endl;
                cout << "### xmltvid  = " << (*i).xmltvid.local8Bit() << endl;
                cout << "### chanid   = " << chanid.local8Bit()       << endl;
                cout << "### name     = " << name.local8Bit()         << endl;
                cout << "### callsign = " << callsign.local8Bit()     << endl;
                cout << "### channum  = " << chanstr.local8Bit()      << endl;
                if (channel_preset)
                    cout << "### freqid   = " << freqid.local8Bit()   << endl;
                cout << "### finetune = " << finetune.local8Bit()     << endl;
                cout << "### tvformat = " << tvformat.local8Bit()     << endl;
                cout << "### icon     = " << icon.local8Bit()         << endl;
                cout << "### " << endl;

                (*i).name = name;
                (*i).callsign = callsign;
                (*i).chanstr  = chanstr;
                (*i).finetune = finetune;
                (*i).freqid = freqid;
                (*i).tvformat = tvformat;

                promptForChannelUpdates(i, atoi(chanid.ascii()));

                if ((*i).callsign == QString::null || (*i).callsign == "")
                    (*i).callsign = chanid;

                if (name     != (*i).name ||
                    callsign != (*i).callsign ||
                    chanstr  != (*i).chanstr ||
                    finetune != (*i).finetune ||
                    freqid   != (*i).freqid ||
                    icon     != localfile ||
                    tvformat != (*i).tvformat)
                {
                     querystr = QString("UPDATE channel SET chanid = %0, "
                                        "name = \"%1\", callsign = \"%2\", "
                                        "channum = \"%3\", finetune = %4, "
                                        "icon = \"%5\", freqid = \"%6\", "
                                        "tvformat = \"%7\" "
                                        " WHERE xmltvid = \"%8\" "
                                        "AND sourceid = %9;")
                                        .arg(chanid.utf8())
                                        .arg((*i).name.utf8())
                                        .arg((*i).callsign.utf8())
                                        .arg((*i).chanstr.utf8())
                                        .arg(atoi((*i).finetune.utf8()))
                                        .arg(localfile.utf8())
                                        .arg((*i).freqid.utf8())
                                        .arg((*i).tvformat.utf8())
                                        .arg((*i).xmltvid.utf8())
                                        .arg(id);

                    if (!query.exec(querystr))
                    {
                        cerr << "DB Error: Channel update failed, SQL query "
                             << "was:" << endl;
                        cerr << querystr << endl;
                    }
                    else
                    {
                        cout << "### " << endl;
                        cout << "### Change performed" << endl;
                        cout << "### " << endl;
                    }
                }
                else
                {
                    cout << "### " << endl;
                    cout << "### Nothing changed" << endl;
                    cout << "### " << endl;
                }
            }
            else
            {
                if (!non_us_updating && localfile != "")
                {
                    querystr.sprintf("UPDATE channel SET icon = \"%s\" WHERE "
                                     "chanid = \"%s\"",
                                     localfile.ascii(), chanid.ascii());

                    if (!query.exec(querystr))
                    {
                        cerr << "DB Error: Channel icon change failed, SQL query "
                             << "was:" << endl;
                        cerr << querystr << endl;
                    }
                }
            }
        }
        else
        {
            if (interactive)
            {
                cout << "### " << endl;
                cout << "### New channel found" << endl;
                cout << "### " << endl;
                cout << "### name     = " << (*i).name.local8Bit()     << endl;
                cout << "### callsign = " << (*i).callsign.local8Bit() << endl;
                cout << "### channum  = " << (*i).chanstr.local8Bit()  << endl;
                if (channel_preset)
                    cout << "### freqid   = " << (*i).freqid.local8Bit() << endl;
                cout << "### finetune = " << (*i).finetune.local8Bit() << endl;
                cout << "### tvformat = " << (*i).tvformat.local8Bit() << endl;
                cout << "### icon     = " << localfile.local8Bit()     << endl;
                cout << "### " << endl;

                unsigned int chanid = promptForChannelUpdates(i,0);

                if ((*i).callsign == QString::null || (*i).callsign == "")
                    (*i).callsign = QString::number(chanid);

                if (chanid > 0)
                {
                    querystr = QString("INSERT INTO channel (chanid,name"
                                       ",callsign,channum,finetune,icon"
                                       ",xmltvid,sourceid,freqid,tvformat) "
                                       "VALUES(%0,\"%1\",\"%2\",\"%3\",%4,"
                                       "\"%5\",\"%6\",%7,\"%8\",\"%9\");")
                                       .arg(chanid)
                                       .arg((*i).name.utf8())
                                       .arg((*i).callsign.utf8())
                                       .arg((*i).chanstr.utf8())
                                       .arg(atoi((*i).finetune))
                                       .arg(localfile.utf8())
                                       .arg((*i).xmltvid.utf8())
                                       .arg(id)
                                       .arg((*i).freqid.utf8())
                                       .arg((*i).tvformat.utf8());

                    if (!query.exec(querystr))
                    {
                        cerr << "DB Error: Channel insert failed, SQL query "
                             << "was:" << endl;
                        cerr << querystr << endl;
                    }
                    else
                    {
                        cout << "### " << endl;
                        cout << "### Channel inserted" << endl;
                        cout << "### " << endl;
                    }
                }
                else
                {
                    cout << "### " << endl;
                    cout << "### Channel skipped" << endl;
                    cout << "### " << endl;
                }
            }
            else if (!non_us_updating)
            {
                // Make up a chanid if automatically adding one.
                // Must be unique, nothing else matters, nobody else knows.
                // We only do this if we are not asked to skip it with the
                // --updating flag.

                int chanid = id * 1000 + atoi((*i).chanstr.ascii());

                while(1)
                {
                    querystr.sprintf("SELECT channum FROM channel WHERE "
                                     "chanid = %d;", chanid);
                    if (!query.exec(querystr))
                        break;

                    if (query.isActive() && query.numRowsAffected() > 0)
                        chanid++;
                    else
                        break;
                }

                if ((*i).callsign == QString::null || (*i).callsign == "")
                    (*i).callsign = QString::number(chanid);

                querystr = QString("INSERT INTO channel (chanid,name,callsign,"
                                   "channum,finetune,icon,xmltvid,sourceid,"
                                   "freqid,tvformat) "
                                   "VALUES(%0,\"%1\",\"%2\",\"%3\",%4,\"%5\","
                                   "\"%6\",%7,\"%8\",\"%9\");")
                                   .arg(chanid)
                                   .arg((*i).name.utf8())
                                   .arg((*i).callsign)
                                   .arg((*i).chanstr)
                                   .arg(atoi((*i).finetune))
                                   .arg(localfile)
                                   .arg((*i).xmltvid)
                                   .arg(id)
                                   .arg((*i).freqid)
                                   .arg((*i).tvformat);

                if (!query.exec(querystr))
                    MythContext::DBError("channel insert", query);

            }
        }
    }
}

void clearDBAtOffset(int offset, int chanid, QDate *qCurrentDate)
{
    if (no_delete)
        return;

    QDate newDate; 
    if (qCurrentDate == 0)
    {
        newDate = QDate::currentDate();
        qCurrentDate = &newDate;
    }

    int nextoffset = 1;

    if (offset == -1)
    {
        offset = 0;
        nextoffset = 10;
    }

    QDateTime from, to;
    from.setDate(*qCurrentDate);
    from = from.addDays(offset);
    from = from.addSecs(listing_wrap_offset);
    to = from.addDays(nextoffset);

    clearDataByChannel(chanid, from, to);
}

void handlePrograms(int id, int offset, QMap<QString, 
                    QValueList<ProgInfo> > *proglist, QDate *qCurrentDate)
{
    QMap<QString, QValueList<ProgInfo> >::Iterator mapiter;
    for (mapiter = proglist->begin(); mapiter != proglist->end(); ++mapiter)
    {
        QSqlQuery query;
        QString querystr;

        if (mapiter.key() == "")
            continue;

        int chanid = 0;

        querystr.sprintf("SELECT chanid FROM channel WHERE sourceid = %d AND "
                         "xmltvid = \"%s\";", id, mapiter.key().ascii());
        query.exec(querystr.ascii());

        if (query.isActive() && query.numRowsAffected() > 0)
        {
            query.next();
            
            chanid = query.value(0).toInt();
        }

        if (chanid == 0)
        {
            cerr << "Unknown xmltv channel identifier: " << mapiter.key() 
                 << endl;
            cerr << "Skipping channel.\n";
            continue;
        }

        clearDBAtOffset(offset, chanid, qCurrentDate);

        QValueList<ProgInfo> *sortlist = &((*proglist)[mapiter.key()]);

        fixProgramList(sortlist);

        QValueList<ProgInfo>::iterator i = sortlist->begin();
        for (; i != sortlist->end(); i++)
        {
            (*i).title.replace(QRegExp("\""), QString("\\\""));
            (*i).subtitle.replace(QRegExp("\""), QString("\\\""));
            (*i).desc.replace(QRegExp("\""), QString("\\\""));
            if ("" == (*i).airdate)
                (*i).airdate = "0";
            if ("" == (*i).stars)
                (*i).stars = "0";

            if (no_delete)
            {
                querystr.sprintf("SELECT * FROM program WHERE chanid=%d AND "
                                 "starttime=\"%s\" AND endtime=\"%s\" AND "
                                 "title=\"%s\" AND subtitle=\"%s\" AND "
                                 "description=\"%s\" AND category=\"%s\";",
                                 chanid, 
                                 (*i).start.toString("yyyyMMddhhmmss").ascii(), 
                                 (*i).end.toString("yyyyMMddhhmmss").ascii(), 
                                 (*i).title.utf8().data(), 
                                 (*i).subtitle.utf8().data(), 
                                 (*i).desc.utf8().data(), 
                                 (*i).category.utf8().data());

                query.exec(querystr.utf8().data());

                if (query.isActive() && query.numRowsAffected() > 0)
                    continue;

                querystr.sprintf("SELECT title,subtitle,starttime,endtime "
                                 "FROM program WHERE chanid=%d AND "
                                 "starttime>=\"%s\" AND starttime<\"%s\";",
                                 chanid, 
                                 (*i).start.toString("yyyyMMddhhmmss").ascii(), 
                                 (*i).end.toString("yyyyMMddhhmmss").ascii());
                query.exec(querystr.utf8().data());

                if (query.isActive() && query.numRowsAffected() > 0)
                {
                    while(query.next())
                    {
                        cerr << "removing existing program: "
                             << (*i).channel << " "
                             << query.value(0).toString() << " "
                             << query.value(2).toDateTime().toString("yyyyMMddhhmmss") << "-"
                             << query.value(3).toDateTime().toString("yyyyMMddhhmmss") << endl;
                    }

                    cerr << "inserting new program    : "
                         << (*i).channel << " "
                         << (*i).title.local8Bit() << " "
                         << (*i).startts << "-" << (*i).endts << endl;
                    cerr << endl;

                    querystr.sprintf("DELETE FROM program WHERE chanid=%d AND "
                                     "starttime>=\"%s\" AND starttime<\"%s\";",
                                     chanid, 
                                     (*i).start.toString("yyyyMMddhhmmss").ascii(), 
                                     (*i).end.toString("yyyyMMddhhmmss").ascii());
                    query.exec(querystr.utf8().data());

                    querystr.sprintf("DELETE FROM programrating WHERE chanid=%d AND "
                                     "starttime>=\"%s\" AND starttime<\"%s\";",
                                     chanid, 
                                     (*i).start.toString("yyyyMMddhhmmss").ascii(), 
                                     (*i).end.toString("yyyyMMddhhmmss").ascii());
                    query.exec(querystr.utf8().data());

                    querystr.sprintf("DELETE FROM credits WHERE chanid=%d AND "
                                     "starttime>=\"%s\" AND starttime<\"%s\";",
                                     chanid,
                                     (*i).start.toString("yyyyMMddhhmmss").ascii(),
                                     (*i).end.toString("yyyyMMddhhmmss").ascii());
                    query.exec(querystr.utf8().data());
                }
            }

            querystr.sprintf("INSERT INTO program (chanid,starttime,endtime,"
                             "title,title_pronounce,subtitle,description,category,"
                             "category_type,airdate,stars,previouslyshown) "
                             "VALUES(%d,\"%s\",\"%s\",\"%s\",\"%s\",\"%s\","
                             "\"%s\",\"%s\",\"%s\",\"%s\",\"%s\",\"%d\");", 
                             chanid, 
                             (*i).start.toString("yyyyMMddhhmmss").ascii(), 
                             (*i).end.toString("yyyyMMddhhmmss").ascii(), 
                             (*i).title.utf8().data(), 
                             (*i).pronounce.utf8().data(), 
                             (*i).subtitle.utf8().data(), 
                             (*i).desc.utf8().data(), 
                             (*i).category.utf8().data(),
                             (*i).catType.utf8().data(),
                             (*i).airdate.utf8().data(),
                             (*i).stars.utf8().data(),
                             (*i).repeat);

            if (!query.exec(querystr.utf8().data()))
            {
                MythContext::DBError("program insert", query);
            }

            QValueList<ProgRating>::iterator j = (*i).ratings.begin();
            for (; j != (*i).ratings.end(); j++)
            {
                querystr.sprintf("INSERT INTO programrating (chanid,starttime,"
                "system,rating) VALUES (%d, \"%s\", \"%s\", \"%s\");",
                chanid,
                (*i).start.toString("yyyyMMddhhmmss").ascii(),
                (*j).system.utf8().data(),
                (*j).rating.utf8().data());

                if (!query.exec(querystr.utf8().data()))
                {
                    MythContext::DBError("programrating insert", query);
                }
            }

            QValueList<ProgCredit>::iterator k = (*i).credits.begin();
            for (; k != (*i).credits.end(); k++)
            {
                (*k).name.replace(QRegExp("\""), QString("\\\""));

                querystr.sprintf("SELECT person FROM people WHERE "
                                 "name = \"%s\";", (*k).name.utf8().data());
                if (!query.exec(querystr.utf8().data()))
                    MythContext::DBError("person lookup", query);

                int personid = -1;
                if (query.isActive() && query.numRowsAffected() > 0)
                {
                    query.next();
                    personid = query.value(0).toInt();
                }

                if (personid < 0)
                {
                    querystr.sprintf("INSERT INTO people (name) VALUES "
                                     "(\"%s\");", (*k).name.utf8().data());
                    if (!query.exec(querystr.utf8().data()))
                        MythContext::DBError("person insert", query);

                    querystr.sprintf("SELECT person FROM people WHERE "
                                     "name = \"%s\";", (*k).name.utf8().data());
                    if (!query.exec(querystr.utf8().data()))
                        MythContext::DBError("person lookup", query);

                    if (query.isActive() && query.numRowsAffected() > 0)
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

                querystr.sprintf("INSERT INTO credits (chanid,starttime,"
                                 "role,person) VALUES "
                                 "(%d, \"%s\", \"%s\", %d);",
                                 chanid,
                                 (*i).start.toString("yyyyMMddhhmmss").ascii(),
                                 (*k).role.ascii(),
                                 personid);

                if (!query.exec(querystr.utf8().data()))
                {
                    // be careful of the startime/timestamp "feature"!
                    querystr.sprintf("UPDATE credits SET "
                                     "role = concat(role,',%s'), "
                                     "starttime = %s "
                                     "WHERE chanid = %d AND "
                                        "starttime = %s and person = %d",
                                     (*k).role.ascii(),
                                     (*i).start.toString("yyyyMMddhhmmss").ascii(),
                                     chanid,
                                     (*i).start.toString("yyyyMMddhhmmss").ascii(),
                                     personid);
                    if (!query.exec(querystr.utf8().data()))
                        MythContext::DBError("credits update", query);
                }
            }
        }
    }
}


void grabDataFromFile(int id, int offset, QString &filename, 
                      QDate *qCurrentDate = 0)
{
    QValueList<ChanInfo> chanlist;
    QMap<QString, QValueList<ProgInfo> > proglist;

    parseFile(filename, &chanlist, &proglist);

    handleChannels(id, &chanlist);
    handlePrograms(id, offset, &proglist, qCurrentDate);
}

time_t toTime_t(QDateTime &dt)
{
    tm brokenDown;
    brokenDown.tm_sec = dt.time().second();
    brokenDown.tm_min = dt.time().minute();
    brokenDown.tm_hour = dt.time().hour();
    brokenDown.tm_mday = dt.date().day();
    brokenDown.tm_mon = dt.date().month() - 1;
    brokenDown.tm_year = dt.date().year() - 1900;
    brokenDown.tm_isdst = -1;
    int secsSince1Jan1970UTC = (int) mktime( &brokenDown );
    if ( secsSince1Jan1970UTC < -1 )
        secsSince1Jan1970UTC = -1;
    return secsSince1Jan1970UTC;
}

bool grabData(Source source, int offset, QDate *qCurrentDate = 0)
{
    QString xmltv_grabber = source.xmltvgrabber + graboptions;

    if (xmltv_grabber == "datadirect")
        return grabDDData(source, offset, *qCurrentDate);

    char tempfilename[] = "/tmp/mythXXXXXX";
    if (mkstemp(tempfilename) == -1) {
         perror("mkstemp");
         exit(1);
    }

    QString filename = QString(tempfilename);

    QString home = QDir::homeDirPath();
    QString configfile = QString("%1/.mythtv/%2.xmltv").arg(home)
                                                       .arg(source.name);
    QString command;

    if (xmltv_grabber == "tv_grab_uk")
        command.sprintf("nice %s --days 7 --config-file '%s' --output %s",
                        xmltv_grabber.ascii(), configfile.ascii(), 
                        filename.ascii());
    else if (xmltv_grabber == "tv_grab_uk_rt")
        command.sprintf("nice %s --days 1 --offset %d --config-file '%s' --output %s",
	                 xmltv_grabber.ascii(), offset, 
			 configfile.ascii(), filename.ascii());
    else if (xmltv_grabber == "tv_grab_au")
        command.sprintf("nice %s --days 7 --config-file '%s' --output %s",
                        xmltv_grabber.ascii(), configfile.ascii(),
                        filename.ascii());
    else if (xmltv_grabber == "tv_grab_nz")
        command.sprintf("nice %s -n 1 -f %d -o '%s'",
                        xmltv_grabber.ascii(), offset,
                        filename.ascii());
    else if (xmltv_grabber == "tv_grab_de")
        command.sprintf("nice %s --days 7 --output %s",
                        xmltv_grabber.ascii(),
                        filename.ascii());
    else if (xmltv_grabber == "tv_grab_fr")
        command.sprintf("nice %s --days 7 '%s' --output %s",
                        xmltv_grabber.ascii(), configfile.ascii(),
                        filename.ascii());
    else if (xmltv_grabber == "tv_grab_nl")
        command.sprintf("nice %s --output %s",
                        xmltv_grabber.ascii(),
                        filename.ascii());
    else if (xmltv_grabber == "tv_grab_fi")
        // Use the default of 10 days for Finland's grabber
        command.sprintf("nice %s --offset %d --config-file '%s' --output %s",
                        xmltv_grabber.ascii(), offset,
                        configfile.ascii(), filename.ascii());
    else if (xmltv_grabber == "tv_grab_es")
        // Use fixed interval of 3 days for Spanish grabber
        command.sprintf("nice %s --days=4  --config-file '%s' --output %s",
                        xmltv_grabber.ascii(), 
                        configfile.ascii(), filename.ascii());
    else if (xmltv_grabber == "tv_grab_jp")
    {
         // Use fixed interval of 7 days for Japanese grabber
         command.sprintf("nice %s --days 7 --enable-readstr --config-file '%s' --output %s",
                         xmltv_grabber.ascii(), configfile.ascii(),
                         filename.ascii());
         isJapan = true;
    }
    else if (xmltv_grabber == "tv_grab_sn")
        command.sprintf("nice %s --days 1 --offset %d --config-file '%s' --output %s",
                        xmltv_grabber.ascii(), offset, configfile.ascii(),
                        filename.ascii());
    else if (xmltv_grabber == "tv_grab_dk")
        // Use fixed interval of 7 days for Danish grabber
        command.sprintf("nice %s --days 7 --config-file '%s' --output %s",
                        xmltv_grabber.ascii(), configfile.ascii(),
                        filename.ascii());
    else
    {
        isNorthAmerica = true;
        command.sprintf("nice %s --days 1 --offset %d --config-file '%s' "
                        "--output %s", xmltv_grabber.ascii(),
                        offset, configfile.ascii(), filename.ascii());
    }

    if (quiet &&
        (xmltv_grabber == "tv_grab_na" ||
         xmltv_grabber == "tv_grab_de" ||
         xmltv_grabber == "tv_grab_fi" ||
         xmltv_grabber == "tv_grab_es" ||
         xmltv_grabber == "tv_grab_nz" ||
         xmltv_grabber == "tv_grab_sn" ||
         xmltv_grabber == "tv_grab_dk" ||
         xmltv_grabber == "tv_grab_uk" ||
         xmltv_grabber == "tv_grab_uk_rt" ||
         xmltv_grabber == "tv_grab_nl" ||
         xmltv_grabber == "tv_grab_fr" ||
         xmltv_grabber == "tv_grab_fi" ||
         xmltv_grabber == "tv_grab_jp"))
         command += " --quiet";


    if (!quiet)
         cout << "----------------- Start of XMLTV output -----------------" << endl;

    QDateTime qdtNow = QDateTime::currentDateTime();
    QSqlQuery query;
    QString status = "currently running.";

    query.exec(QString("UPDATE settings SET data ='%1' "
                       "WHERE value='mythfilldatabaseLastRunStart'")
                       .arg(qdtNow.toString("yyyy-MM-dd hh:mm")));

    query.exec(QString("UPDATE settings SET data ='%1' "
                       "WHERE value='mythfilldatabaseLastRunStatus'")
                       .arg(status));

    int systemcall_status = system(command.ascii());
    bool succeeded = WIFEXITED(systemcall_status) &&
         WEXITSTATUS(systemcall_status) == 0;

    qdtNow = QDateTime::currentDateTime();
    query.exec(QString("UPDATE settings SET data ='%1' "
                       "WHERE value='mythfilldatabaseLastRunEnd'")
                       .arg(qdtNow.toString("yyyy-MM-dd hh:mm")));

    status = "Successful.";

    if (!succeeded)
    {
        status = QString("FAILED:  xmltv returned error code %1.")
                         .arg(systemcall_status);

        query.exec(QString("UPDATE settings SET data ='%1' "
                           "WHERE value='mythfilldatabaseLastRunStatus'")
                           .arg(status));
        if (WIFSIGNALED(systemcall_status) &&
            (WTERMSIG(systemcall_status) == SIGINT || WTERMSIG(systemcall_status) == SIGQUIT))
            interrupted = true;
    }
 
    if (!quiet)
         cout << "------------------ End of XMLTV output ------------------" << endl;

    grabDataFromFile(source.id, offset, filename, qCurrentDate);

    QFile thefile(filename);
    thefile.remove();

    return succeeded;
}

void clearOldDBEntries(void)
{
    QSqlQuery query;
    QString querystr;
    int offset = 1;

    if (no_delete)
        offset=7;

    querystr.sprintf("DELETE FROM oldprogram WHERE airdate < "
                     "DATE_SUB(CURRENT_DATE, INTERVAL 320 DAY);");
    query.exec(querystr);

    querystr.sprintf("REPLACE INTO oldprogram (oldtitle,airdate) "
                     "SELECT title,starttime FROM program "
                     "WHERE starttime < NOW() group by title;");
    query.exec(querystr);

    querystr.sprintf("DELETE FROM program WHERE starttime <= "
                     "DATE_SUB(CURRENT_DATE, INTERVAL %d DAY);", offset);
    query.exec(querystr);

    querystr.sprintf("DELETE FROM programrating WHERE starttime <= "
                     "DATE_SUB(CURRENT_DATE, INTERVAL %d DAY);", offset);
    query.exec(querystr);

    querystr.sprintf("DELETE FROM credits WHERE starttime <= "
                     "DATE_SUB(CURRENT_DATE, INTERVAL %d DAY);", offset);
    query.exec(querystr);

    querystr.sprintf("DELETE FROM record WHERE (type = %d "
                     "OR type = %d OR type = %d) AND enddate < "
                     "DATE_SUB(CURRENT_DATE, INTERVAL 1 DAY);",
                     kSingleRecord, kOverrideRecord, kDontRecord);
    query.exec(querystr);

    querystr.sprintf("DELETE FROM recordoverride WHERE endtime < "
                     "DATE_SUB(CURRENT_DATE, INTERVAL 1 DAY);");
    query.exec(querystr);
}

bool fillData(QValueList<Source> &sourcelist)
{
    QValueList<Source>::Iterator it;

    QString status;
    QSqlQuery query;
    QDateTime GuideDataBefore, GuideDataAfter;

    query.exec(QString("SELECT MAX(endtime) FROM program;"));
    if (query.isActive() && query.numRowsAffected())
    {
        query.next();

        if (!query.isNull(0))
            GuideDataBefore = QDateTime::fromString(query.value(0).toString(),
                                                    Qt::ISODate);
    }

    int failures = 0;
    for (it = sourcelist.begin(); it != sourcelist.end(); ++it) {
        QString xmltv_grabber = (*it).xmltvgrabber;
        if (xmltv_grabber == "tv_grab_uk" || xmltv_grabber == "tv_grab_de" ||
            xmltv_grabber == "tv_grab_fi" || xmltv_grabber == "tv_grab_es" ||
            xmltv_grabber == "tv_grab_nl" || xmltv_grabber == "tv_grab_au" ||
            xmltv_grabber == "tv_grab_fr" || xmltv_grabber == "tv_grab_jp")
        {
            // tv_grab_uk|de doesn't support the --offset option, so just grab a 
            // week.
            if (!grabData(*it, -1))
                ++failures;
        }
        else if (xmltv_grabber == "tv_grab_dk")
        {
            if (!grabData(*it, 0))
                ++failures;
        }
        else if (xmltv_grabber == "tv_grab_nz")
        {
            // tv_grab_nz only supports a 7-day "grab".
            grabData(*it, 1);

            for (int i = 0; i < 7; i++)
            {
                QString querystr;
                querystr.sprintf("SELECT COUNT(*) as 'hits' "
                                 "FROM channel LEFT JOIN program USING (chanid) "
                                 "WHERE sourceid = %d AND starttime >= "
                                 "DATE_ADD(CURRENT_DATE(), INTERVAL %d DAY) "
                                 "AND starttime < DATE_ADD(CURRENT_DATE(), "
                                 "INTERVAL 1+%d DAY) "
                                 "GROUP BY channel.chanid "
                                 "ORDER BY hits DESC LIMIT 1",
                                 (*it).id, i, i);               
 
                QSqlQuery query;
                query.exec(querystr);
                
                if (query.isActive()) 
                {
                    if (!query.numRowsAffected() ||
                        (query.next() && query.value(0).toInt() <= 1))
                    {
                        if (!grabData(*it, i))
                            ++failures;
                    }
                } 
                else
                    MythContext::DBError("checking existing program data", 
                                         query);
            }
        }
        else if (xmltv_grabber == "datadirect" && dd_grab_all)
        {
            QDate qCurrentDate = QDate::currentDate();

            grabData(*it, 0, &qCurrentDate);
        }
        else if (xmltv_grabber == "datadirect" ||
                 xmltv_grabber == "tv_grab_na" || 
                 xmltv_grabber == "tv_grab_uk_rt" ||
                 xmltv_grabber == "tv_grab_sn")
        {
            if (xmltv_grabber == "tv_grab_sn")
                listing_wrap_offset = 6 * 3600;

            QDate qCurrentDate = QDate::currentDate();

            if (refresh_today)
            {
                if (!quiet)
                    cout << "Refreshing Today's data" << endl;
                if (!grabData(*it, 0, &qCurrentDate))
                    ++failures;
            }

            if (refresh_tomorrow)
            {
                if (!quiet)
                    cout << "Refreshing Tomorrow's data" << endl;
                if (!grabData(*it, 1, &qCurrentDate))
                    ++failures;
            }

            if (refresh_second)
            {
                if (!quiet)
                    cout << "Refreshing data for 2 days from today" << endl;
                if (!grabData(*it, 2, &qCurrentDate))
                    ++failures;
            }

            int maxday = 9;
            if (xmltv_grabber == "tv_grab_uk_rt" ||
                xmltv_grabber == "tv_grab_sn")
                maxday = 14;
            if (xmltv_grabber == "datadirect")
                maxday = 13;

            for (int i = 0; i < maxday; i++)
            {
                if ((i == 0 && refresh_today) || (i == 1 && refresh_tomorrow) ||
                    (i == 2 && refresh_second))
                    continue;

                // we need to check and see if the current date has changed 
                // since we started in this loop.  If it has, we need to adjust
                // the value of 'i' to compensate for this.
                if (QDate::currentDate() != qCurrentDate)
                {
                    QDate newDate = QDate::currentDate();
                    i += (newDate.daysTo(qCurrentDate));
                    if (i < 0) 
                        i = 0;
                    qCurrentDate = newDate;
                }

                // Check to see if we already downloaded data for this date
                bool download_needed = false;
                QString date(qCurrentDate.addDays(i).toString());
                QString querystr;

                querystr.sprintf("SELECT COUNT(*) as 'hits' "
                                 "FROM channel LEFT JOIN program USING (chanid) "
                                 "WHERE sourceid = %d AND starttime >= "
                                 "DATE_ADD(CURRENT_DATE(), INTERVAL '%d 12' "
                                 "DAY_HOUR) AND "
                                 "starttime < DATE_ADD(CURRENT_DATE(), "
                                 "INTERVAL 1+%d DAY) "
                                 "GROUP BY channel.chanid "
                                 "ORDER BY hits DESC LIMIT 1",
                                 (*it).id, i, i); 
                QSqlQuery query;
                query.exec(querystr);
               
                if (query.isActive()) 
                {
                    // We also need to get this day's data if there's only a 
                    // suspiciously small amount in the DB.
                    if (!query.numRowsAffected() ||
                        (query.next() && query.value(0).toInt() <= 20)) 
                    {
                        download_needed = true;
                    }
                } 
                else
                    MythContext::DBError("checking existing program data", 
                                         query);

                // Now look for programs marked as "To Be Announced"
                if (refresh_tba && !download_needed && 
                    xmltv_grabber == "tv_grab_uk_rt") 
                {
                    querystr.sprintf("SELECT COUNT(*) as 'hits' "
                                     "FROM channel LEFT JOIN program USING (chanid) "
                                     "WHERE sourceid = %d AND starttime >= "
                                     "DATE_ADD(CURRENT_DATE(), INTERVAL '%d 12' DAY_HOUR) "
                                     "AND starttime < DATE_ADD(CURRENT_DATE(), "
                                     "INTERVAL 1+%d DAY) "
                                     "AND category = 'TBA' "
                                     "GROUP BY channel.chanid "
                                     "ORDER BY hits DESC LIMIT 1",
                                      (*it).id, i, i);
                    QSqlQuery query;
                    query.exec(querystr);

                    if (query.isActive()) 
                    {
                        if (query.numRowsAffected() || 
                            (query.next() && query.value(0).toInt() >= 1)) 
                        {
                            download_needed = true;
                        }
                    } 
                    else
                        MythContext::DBError("checking existing program data", 
                                             query);
                }

                if (download_needed)
                {
                    if (!quiet)
                        cout << "Fetching data for " << date << endl;
                    if (!grabData(*it, i, &qCurrentDate))
                    {
                        ++failures;
                        if (interrupted)
                        {
                            break;
                        }
                    }
                }
                else
                {
                    if (!quiet)
                        cout << "Data is already present for " << date
                             << ", skipping\n";
                }
            }
        }
        else
        {
            cerr << "Grabbing XMLTV data using " << xmltv_grabber.ascii() 
                 << " is not verified as working.\n";
        }
        if (interrupted)
        {
            break;
        }
    }

    query.exec(QString("SELECT MAX(endtime) FROM program;"));
    if (query.isActive() && query.numRowsAffected())
    {
        query.next();

        if (!query.isNull(0))
            GuideDataAfter = QDateTime::fromString(query.value(0).toString(),
                                                   Qt::ISODate);
    }

    if (failures == 0)
    {
        if (GuideDataAfter == GuideDataBefore)
            status = "mythfilldatabase ran, but did not insert "
                     "any new data into the Guide.  This can indicate a "
                     "potential xmltv failure."; 
        else
            status = "Successful.";

        query.exec(QString("UPDATE settings SET data ='%1' "
                           "WHERE value='mythfilldatabaseLastRunStatus'")
                           .arg(status));
    }

    clearOldDBEntries();

    return (failures == 0);
}

ChanInfo *xawtvChannel(QString &id, QString &channel, QString &fine)
{
    ChanInfo *chaninfo = new ChanInfo;
    chaninfo->xmltvid = id;
    chaninfo->name = id;
    chaninfo->callsign = id;
    if (channel_preset)
        chaninfo->chanstr = id;
    else
        chaninfo->chanstr = channel;
    chaninfo->finetune = fine;
    chaninfo->freqid = channel;
    chaninfo->iconpath = "";

    return chaninfo;
}

void readXawtvChannels(int id, QString xawrcfile)
{
    fstream fin(xawrcfile.ascii(), ios::in);
    if (!fin.is_open()) return;

    QValueList<ChanInfo> chanlist;

    QString xawid;
    QString channel;
    QString fine;

    string strLine;
    int nSplitPoint = 0;

    while(!fin.eof())
    {
        getline(fin,strLine);

        if ((strLine[0] != '#') && (!strLine.empty()))
        {
            if (strLine[0] == '[')
            {
                if ((nSplitPoint = strLine.find(']')) > 1)
                {
                    if ((xawid != "") && (channel != ""))
                    {
                        ChanInfo *chinfo = xawtvChannel(xawid, channel, fine);
                        chanlist.push_back(*chinfo);
                        delete chinfo;
                    }
                    xawid = strLine.substr(1, nSplitPoint - 1).c_str();
                    channel = "";
                    fine = "";
                }
            }
            else if ((nSplitPoint = strLine.find('=') + 1) > 0)
            {
                while (strLine.substr(nSplitPoint,1) == " ")
                { ++nSplitPoint; }

                if (!strncmp(strLine.c_str(), "channel", 7))
                {
                    channel = strLine.substr(nSplitPoint, 
                                             strLine.size()).c_str();
                }
                else if (!strncmp(strLine.c_str(), "fine", 4))
                {
                    fine = strLine.substr(nSplitPoint, strLine.size()).c_str();
                }
            }
        }
    }

    if ((xawid != "") && (channel != ""))
    {
        ChanInfo *chinfo = xawtvChannel(xawid, channel, fine);
        chanlist.push_back(*chinfo);
        delete chinfo;
    }

    handleChannels(id, &chanlist);
}

int fix_end_times(void)
{
    int count = 0;
    QString chanid, starttime, endtime, querystr;
    QSqlQuery query1, query2;

    querystr = "SELECT chanid, starttime, endtime FROM program "
               "WHERE (DATE_FORMAT(endtime,\"%H%i\") = \"0000\") "
               "ORDER BY chanid, starttime;";

    if (!query1.exec(querystr))
    {
        cerr << "fix_end_times:  " << querystr << " failed!\n";
        return -1;
    }

    while (query1.next())
    {
        starttime = query1.value(1).toString();
        chanid = query1.value(0).toString();
        endtime = query1.value(2).toString();

        querystr.sprintf("SELECT chanid, starttime, endtime FROM program "
                         "WHERE (DATE_FORMAT(starttime, \"%%Y-%%m-%%d\") = "
                         "\"%s\") AND chanid = \"%s\" "
                         "ORDER BY starttime LIMIT 1;", 
                         endtime.left(10).ascii(), chanid.ascii());

        if (!query2.exec(querystr))
        {
            cerr << "fix_end_times:  " << querystr << " failed!\n";
            return -1;
        }

        if (query2.next() && (endtime != query2.value(1).toString()))
        {
            count++;
            endtime = query2.value(1).toString();
            querystr.sprintf("UPDATE program SET starttime = \"%s\", "
                             "endtime = \"%s\" WHERE (chanid = \"%s\" AND "
                             "starttime = \"%s\");", starttime.ascii(), 
                             endtime.ascii(), chanid.ascii(), 
                             starttime.ascii());

            if (!query2.exec(querystr)) 
            {
                cerr << "fix_end_times:  " << querystr << " failed!\n";
                return -1;
            }
        }
    }

    return count;
}

int main(int argc, char *argv[])
{
    QApplication a(argc, argv, false);
    int argpos = 1;
    int fromfile_id = 1;
    int fromfile_offset = 0;
    QString fromfile_name;
    bool from_xawfile = false;
    int fromxawfile_id = 1;
    QString fromxawfile_name;

    while (argpos < a.argc())
    {
        // The manual and update flags should be mutually exclusive.
        if (!strcmp(a.argv()[argpos], "--manual"))
        {
            cout << "###\n";
            cout << "### Running in manual channel configuration mode.\n";
            cout << "### This will ask you questions about every channel.\n";
            cout << "###\n";
            interactive = true;
        }
        else if (!strcmp(a.argv()[argpos], "--preset"))
        {
            // For using channel preset values instead of channel numbers.
            cout << "###\n";
            cout << "### Running in preset channel configuration mode.\n";
            cout << "### This will assign channel ";
            cout << "preset numbers to every channel.\n";
            cout << "###\n";
            channel_preset = true;
        }
        else if (!strcmp(a.argv()[argpos], "--update"))
        {
            // For running non-destructive updates on the database for
            // users in xmltv zones that do not provide channel data.
            non_us_updating = true;
        }
        else if (!strcmp(a.argv()[argpos], "--no-delete"))
        {
            // Do not delete old programs from the database until 7 days old.
            // Do not delete existing programs from the database when updating.
            no_delete = true;
        }
        else if (!strcmp(a.argv()[argpos], "--file"))
        {
            if (((argpos + 3) >= a.argc()) ||
                !strncmp(a.argv()[argpos + 1], "--", 2) ||
                !strncmp(a.argv()[argpos + 2], "--", 2) ||
                !strncmp(a.argv()[argpos + 3], "--", 2))
            {
                printf("missing or invalid parameters for --file option\n");
                return -1;
            }

            fromfile_id = atoi(a.argv()[++argpos]);
            fromfile_offset = atoi(a.argv()[++argpos]);
            fromfile_name = a.argv()[++argpos];

            if (!quiet)
                cout << "### bypassing grabbers, reading directly from file\n";
            from_file = true;
        }
        else if (!strcmp(a.argv()[argpos], "--xawchannels"))
        {
            if (((argpos + 2) >= a.argc()) ||
                !strncmp(a.argv()[argpos + 1], "--", 2) ||
                !strncmp(a.argv()[argpos + 2], "--", 2))
            {
                printf("missing or invalid parameters for --xawchannels option\n");
                return -1;
            }

            fromxawfile_id = atoi(a.argv()[++argpos]);
            fromxawfile_name = a.argv()[++argpos];

            if (!quiet)
                 cout << "### reading channels from xawtv configfile\n";
            from_xawfile = true;
        }
        else if (!strcmp(a.argv()[argpos], "--graboptions"))
        {
            if (((argpos + 1) >= a.argc()))
            {
                printf("missing parameter for --graboptions option\n");
                return -1;
            }

            graboptions = QString(" ") + QString(a.argv()[++argpos]);
        }
        else if (!strcmp(a.argv()[argpos], "--refresh-today"))
        {
            refresh_today = true;
        }
        else if (!strcmp(a.argv()[argpos], "--dont-refresh-tomorrow"))
        {
            refresh_tomorrow = false;
        }
        else if (!strcmp(a.argv()[argpos], "--refresh-second"))
        {
            refresh_second = true;
        }
        else if (!strcmp(a.argv()[argpos], "--dont-refresh-tba"))
        {
            refresh_tba = false;
        }
#if 0
        else if (!strcmp(a.argv()[argpos], "--dd-grab-all"))
        {
            dd_grab_all = true;
            refresh_today = false;
            refresh_tomorrow = false;
            refresh_second = false;
        }
#endif
        else if (!strcmp(a.argv()[argpos], "--quiet"))
        {
             quiet = true;
             ++argpos;
        }
        else if (!strcmp(a.argv()[argpos], "-h") ||
                 !strcmp(a.argv()[argpos], "--help"))
        {
            cout << "usage:\n";
            cout << "--manual\n";
            cout << "   Run in manual channel configuration mode\n";
            cout << "   This will ask you questions about every channel\n";
            cout << "\n";
            cout << "--update\n";
            cout << "   For running non-destructive updates on the database for\n";
            cout << "   users in xmltv zones that do not provide channel data\n";
            cout << "\n";
            cout << "--preset\n";
            cout << "   Use it in case that you want to assign a preset number for\n";
            cout << "   each channel, useful for non US countries where people\n";
            cout << "   are used to assigning a sequenced number for each channel, i.e.:\n";
            cout << "   1->TVE1(S41), 2->La 2(SE18), 3->TV3(21), 4->Canal 33(60)...\n";
            cout << "\n";
            cout << "--no-delete\n";
            cout << "   Do not delete old programs from the database until 7 days old.\n";
            cout << "   Do not delete existing programs from the database when updating.\n";
            cout << "\n";
            cout << "--file <sourceid> <offset> <xmlfile>\n";
            cout << "   Bypass the grabbers and read data directly from a file\n";
            cout << "   <sourceid> = cardinput\n";
            cout << "   <offset>   = days from today that xmlfile defines\n";
            cout << "                (-1 means to replace all data, up to 10 days)\n";
            cout << "   <xmlfile>  = file to read\n";
            cout << "\n";
            cout << "--xawchannels <sourceid> <xawtvrcfile>\n";
            cout << "   (--manual flag works in combination with this)\n";
            cout << "   Read channels as defined in xawtvrc file given\n";
            cout << "   <sourceid>    = cardinput\n";
            cout << "   <xawtvrcfile> = file to read\n";
            cout << "\n";
            cout << "--graboptions <\"options\">\n";
            cout << "   Pass options to grabber\n";
            cout << "\n";
            cout << "--refresh-today\n";
            cout << "--refresh-second\n";
            cout << "   (Only valid for grabbers: na/uk_rt/sn)\n";
            cout << "   Force a refresh today or two days from now, to catch the latest changes\n";
            cout << "--dont-refresh-tomorrow\n";
            cout << "   Tomorrow will be refreshed always unless this argument is used\n";
            cout << "--dont-refresh-tba\n";
            cout << "   \"To be announced\" progs will be refreshed always unless this argument is used\n";
#if 0
            cout << "--dd-grab-all\n";
            cout << "   The DataDirect grabber will grab all available data\n";
#endif
            cout << "--help\n";
            cout << "   This text\n";
            cout << "\n";
            cout << "\n";
            cout << "  --manual and --update can not be used together.\n";
            cout << "\n";
            return -1;
        }
        else
        {
            fprintf(stderr, "illegal option: '%s' (use --help)\n",
                    a.argv()[argpos]);
            return -1;
        }

        ++argpos;
    }

    gContext = new MythContext(MYTH_BINARY_VERSION, false);

    QSqlDatabase *db = QSqlDatabase::addDatabase("QMYSQL3");
    if (!gContext->OpenDatabase(db))
    {
        cerr << "couldn't open db\n";
        return -1;
    }

    gContext->LogEntry("mythfilldatabase", LP_INFO,
                       "Listings Download Started", "");

    if (from_xawfile)
    {
        readXawtvChannels(fromxawfile_id, fromxawfile_name);
    }
    else if (from_file)
    {
        grabDataFromFile(fromfile_id, fromfile_offset, fromfile_name);
        clearOldDBEntries();
    }
    else
    {
        QValueList<Source> sourcelist;

        QSqlQuery sourcequery;
        QString querystr = QString("SELECT sourceid,name,xmltvgrabber,userid,"
                                   "password,lineupid "
                                   "FROM videosource ORDER BY sourceid;");
        sourcequery.exec(querystr);
        
        if (sourcequery.isActive())
        {
             if (sourcequery.numRowsAffected() > 0)
             {
                  while (sourcequery.next())
                  {
                       Source newsource;
            
                       newsource.id = sourcequery.value(0).toInt();
                       newsource.name = sourcequery.value(1).toString();
                       newsource.xmltvgrabber = sourcequery.value(2).toString();
                       newsource.userid = sourcequery.value(3).toString();
                       newsource.password = sourcequery.value(4).toString();
                       newsource.lineupid = sourcequery.value(5).toString();

                       sourcelist.append(newsource);
                  }
             }
             else
             {
                  cerr << "There are no channel sources defined, did you run "
                       << "the setup program?\n";
                  gContext->LogEntry("mythfilldatabase", LP_CRITICAL,
                                     "No channel sources defined",
                                     "Could not find any defined channel "
                                     "sources - did you run the setup "
                                     "program?");
                  exit(-1);
             }
        }
        else
        {
             MythContext::DBError("loading channel sources", sourcequery);
             exit(-1);
        }
    
        bool ret = fillData(sourcelist);

        if (!ret)
        {
             cerr << "Failed to fetch some program info\n";
             gContext->LogEntry("mythfilldatabase", LP_WARNING,
                                "Failed to fetch some program info", "");
             exit(1);
        }
    }

    if (!quiet)
         cout << "Adjusting program database end times...\n";
    int update_count = fix_end_times();
    if (update_count == -1)
         cerr << "fix_end_times failed!\a\n";
    else if (!quiet)
         cout << update_count << " replacements made.\n";

    ScheduledRecording::signalChange(db);

    gContext->LogEntry("mythfilldatabase", LP_INFO,
                       "Listings Download Finished", "");
    delete gContext;

    return 0;
}
