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

#include <iostream>
#include <fstream>
#include <string>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

#include "libmyth/mythcontext.h"

using namespace std;

bool interactive = false;
bool channel_preset = false;
bool non_us_updating = false;
bool from_file = false;
bool quiet = false;
bool no_delete = false;
bool isNorthAmerica = false;

MythContext *gContext;

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
    QString subtitle;
    QString desc;
    QString category;
    QString catType;
    QString airdate;
    QString stars;
    QValueList<ProgRating> ratings;

    QDateTime start;
    QDateTime end;

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
};

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
    // we signal an error by setting it invalid (> 720min = 12hr)
    int result = 721;

    if (timezone.length() == 5)
    {
        bool ok;

        result = timezone.mid(1,2).toInt(&ok, 10);

        if (!ok)
            result = 721;
        else
        {
            result *= 60;

            int min = timezone.right(2).toInt(&ok, 10);

            if (!ok)
                result = 721;
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
    if (timestr.isEmpty() || abs(localTimezoneOffset) > 720)
        return;

    QStringList split = QStringList::split(" ", timestr);
    QString ts = split[0];
    int ts_offset = localTimezoneOffset;

    if (split.size() > 1)
    {
        QString tmp = split[1];
        tmp.stripWhiteSpace();

        ts_offset = TimezoneToInt(tmp);
        if (abs(ts_offset) > 720)
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
            if (info.tagName() == "title" && pginfo->title == "")
            {
                pginfo->title = getFirstText(info);
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
    // we disable this feature by setting it invalid (> 720min = 12hr)
    int localTimezoneOffset = 721;

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
        if (abs(localTimezoneOffset) > 720)
            cerr << "Ignoring invalid TimeOffset " << config_offset << endl;
    }

    QDomElement docElem = doc.documentElement();

    QUrl baseUrl(docElem.attribute("source-data-url", ""));

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
                (*proglist)[pginfo->channel].push_back(*pginfo);
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

            if((*cur).subtitle != "" && (*i).subtitle == "")
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
                 << (*todelete).title << " "
                 << (*todelete).startts << "-" << (*todelete).endts << endl;
            cerr << "conflicted with             : "
                 << (*tokeep).channel << " "
                 << (*tokeep).title << " "
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
        cout << " [" << def << "]  ";
    }
    
    char response[80];
    cin.getline(response, 80);

    QString qresponse = response;

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
                         "icon,freqid FROM channel WHERE xmltvid = \"%s\" AND "
                         "sourceid = %d;", (*i).xmltvid.ascii(), id); 

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

                cout << "### " << endl;
                cout << "### Existing channel found" << endl;
                cout << "### " << endl;
                cout << "### xmltvid  = " << (*i).xmltvid.ascii() << endl;
                cout << "### chanid   = " << chanid.ascii()       << endl;
                cout << "### name     = " << name.ascii()         << endl;
                cout << "### callsign = " << callsign.ascii()     << endl;
                cout << "### channum  = " << chanstr.ascii()      << endl;
                if (channel_preset)
                    cout << "### freqid   = " << freqid.ascii()   << endl;
                cout << "### finetune = " << finetune.ascii()     << endl;
                cout << "### icon     = " << icon.ascii()         << endl;
                cout << "### " << endl;

                (*i).name = name;
                (*i).callsign = callsign;
                (*i).chanstr  = chanstr;
                (*i).finetune = finetune;
                (*i).freqid = freqid;

                promptForChannelUpdates(i, atoi(chanid.ascii()));

                if (name     != (*i).name ||
                    callsign != (*i).callsign ||
                    chanstr  != (*i).chanstr ||
                    finetune != (*i).finetune ||
                    freqid   != (*i).freqid ||
                    icon     != localfile)
                {
                    querystr.sprintf("UPDATE channel SET chanid = %s, "
                                     "name = \"%s\", callsign = \"%s\", "
                                     "channum = \"%s\", finetune = %d, "
                                     "icon = \"%s\", freqid = \"%s\" "
                                     " WHERE xmltvid = \"%s\" "
                                     "AND sourceid = %d;",
                                     chanid.ascii(),
                                     (*i).name.ascii(),
                                     (*i).callsign.ascii(),
                                     (*i).chanstr.ascii(),
                                     atoi((*i).finetune.ascii()),
                                     localfile.ascii(),
                                     (*i).freqid.ascii(),
                                     (*i).xmltvid.ascii(),
                                     id);

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
                if (!non_us_updating)
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
                cout << "### name     = " << (*i).name.ascii()         << endl;
                cout << "### callsign = " << (*i).callsign.ascii()     << endl;
                cout << "### channum  = " << (*i).chanstr.ascii()      << endl;
                if (channel_preset)
                    cout << "### freqid   = " << (*i).freqid.ascii()   << endl;
                cout << "### finetune = " << (*i).finetune.ascii()     << endl;
                cout << "### icon     = " << localfile.ascii()         << endl;
                cout << "### " << endl;

                unsigned int chanid = promptForChannelUpdates(i,0);

                if (chanid > 0)
                {
                    querystr.sprintf("INSERT INTO channel (chanid,name,"
                                     "callsign,channum,finetune,icon,"
                                     "xmltvid,sourceid,freqid) "
                                     "VALUES(%d,\"%s\",\"%s\",\"%s\",%d,"
                                     "\"%s\",\"%s\",%d,\"%s\");", 
                                     chanid,
                                     (*i).name.ascii(),
                                     (*i).callsign.ascii(),
                                     (*i).chanstr.ascii(),
                                     atoi((*i).finetune.ascii()),
                                     localfile.ascii(),
                                     (*i).xmltvid.ascii(),
                                     id, (*i).freqid.ascii());

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

                querystr.sprintf("INSERT INTO channel (chanid,name,callsign,"
                                 "channum,finetune,icon,xmltvid,sourceid,freqid) "
                                 "VALUES(%d,\"%s\",\"%s\",\"%s\",%d,\"%s\","
                                 "\"%s\",%d,\"%s\");", 
                                 chanid,
                                 (*i).name.ascii(),
                                 (*i).callsign.ascii(),
                                 (*i).chanstr.ascii(),
                                 atoi((*i).finetune.ascii()),
                                 localfile.ascii(),
                                 (*i).xmltvid.ascii(),
                                 id, (*i).freqid.ascii());

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

    int nextoffset = offset + 1;

    if (offset == -1)
    {
        offset = 0;
        nextoffset = 10;
    }

    QSqlQuery query;
    QString querystr;

    querystr.sprintf("DELETE FROM program WHERE starttime >= "
                     "DATE_ADD('%s', INTERVAL %d DAY) "
                     "AND starttime < DATE_ADD('%s', INTERVAL "
                     "%d DAY) AND chanid = %d;", 
                     qCurrentDate->toString("yyyyMMdd").ascii(), offset, 
                     qCurrentDate->toString("yyyyMMdd").ascii(), nextoffset, 
                     chanid);
    query.exec(querystr);

    querystr.sprintf("DELETE FROM programrating WHERE starttime >= "
                     "DATE_ADD('%s', INTERVAL %d DAY) "
                     "AND starttime < DATE_ADD('%s', INTERVAL "
                     "%d DAY) AND chanid = %d;", 
                     qCurrentDate->toString("yyyyMMdd").ascii(), offset, 
                     qCurrentDate->toString("yyyyMMdd").ascii(), nextoffset, 
                     chanid);
    query.exec(querystr);

    querystr.sprintf("DELETE FROM credits WHERE starttime >= "
                     "DATE_ADD('%s', INTERVAL %d DAY) "
                     "AND starttime < DATE_ADD('%s', INTERVAL "
                     "%d DAY) AND chanid = %d;", 
                     qCurrentDate->toString("yyyyMMdd").ascii(), offset, 
                     qCurrentDate->toString("yyyyMMdd").ascii(), nextoffset, 
                     chanid);
    query.exec(querystr);
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
                         << (*i).title << " "
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
                             "title,subtitle,description,category,"
                             "category_type,airdate,stars,previouslyshown) "
                             "VALUES(%d,\"%s\",\"%s\",\"%s\",\"%s\","
                             "\"%s\",\"%s\",\"%s\",\"%s\",\"%s\",\"%d\");", 
                             chanid, 
                             (*i).start.toString("yyyyMMddhhmmss").ascii(), 
                             (*i).end.toString("yyyyMMddhhmmss").ascii(), 
                             (*i).title.utf8().data(), 
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
    QString xmltv_grabber = source.xmltvgrabber;

    if (xmltv_grabber == "tv_grab_uk")
        command.sprintf("nice %s --days 7 --config-file '%s' --output %s",
                        xmltv_grabber.ascii(), configfile.ascii(), 
                        filename.ascii());
    else if (xmltv_grabber == "tv_grab_uk_rt")
        command.sprintf("nice %s --days 1 --offset %d --config-file '%s' > %s",
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
        // Use fixed of 4 days for Spanish grabber
        command.sprintf("nice %s --days=4 --offset %d --config-file '%s' --output %s",
                        xmltv_grabber.ascii(), offset,
                        configfile.ascii(), filename.ascii());
    else if (xmltv_grabber == "tv_grab_sn")
        // Use fixed interval of 14 days for Swedish/Norwegian grabber
        command.sprintf("nice %s --days 14 --config-file '%s' --output %s",
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
         xmltv_grabber == "tv_grab_uk" ||
         xmltv_grabber == "tv_grab_uk_rt" ||
         xmltv_grabber == "tv_grab_nl" ||
         xmltv_grabber == "tv_grab_fi"))
         command += " --quiet";


    if (!quiet)
         cout << "----------------- Start of XMLTV output -----------------" << endl;

    int status = system(command.ascii());
 
    if (!quiet)
         cout << "------------------ End of XMLTV output ------------------" << endl;

    grabDataFromFile(source.id, offset, filename, qCurrentDate);

    QFile thefile(filename);
    thefile.remove();

    return (status == 0);
}

void clearOldDBEntries(void)
{
    QSqlQuery query;
    QString querystr;
    int offset = 1;

    if (no_delete)
        offset=7;

    querystr.sprintf("DELETE FROM program WHERE starttime <= "
                     "DATE_SUB(CURRENT_DATE, INTERVAL %d DAY);", offset);
    query.exec(querystr);

    querystr.sprintf("DELETE FROM programrating WHERE starttime <= "
                     "DATE_SUB(CURRENT_DATE, INTERVAL %d DAY);", offset);
    query.exec(querystr);

    querystr.sprintf("DELETE FROM credits WHERE starttime <= "
                     "DATE_SUB(CURRENT_DATE, INTERVAL %d DAY);", offset);
    query.exec(querystr);
}

bool fillData(QValueList<Source> &sourcelist)
{
    QValueList<Source>::Iterator it;

    int failures = 0;
    for (it = sourcelist.begin(); it != sourcelist.end(); ++it) {
        QString xmltv_grabber = (*it).xmltvgrabber;
        if (xmltv_grabber == "tv_grab_uk" || xmltv_grabber == "tv_grab_de" ||
            xmltv_grabber == "tv_grab_fi" || xmltv_grabber == "tv_grab_es" ||
            xmltv_grabber == "tv_grab_nl" || xmltv_grabber == "tv_grab_au")
        {
            // tv_grab_uk|de doesn't support the --offset option, so just grab a 
            // week.
            if (!grabData(*it, -1))
                ++failures;
        }
        else if (xmltv_grabber == "tv_grab_sn")
        {
            // tv_grab_sn has problems grabbing one day at a time as the site
            // (dagenstv.com) "wraps" the day-listings at 06:00am. This means
            // that programs starting at 02:00am on jul 19 gets "grabbed" in
            // the xmltv-file for jul 18, causing all sorts of trouble.
            // The simple solution is to grab the full two weeks every time.
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
        else if (xmltv_grabber == "tv_grab_na" || 
                     xmltv_grabber == "tv_grab_uk_rt")
        {
            QDate qCurrentDate = QDate::currentDate();

            if (!grabData(*it, 1, &qCurrentDate))
                ++failures;

            int maxday = 9;
            if (xmltv_grabber == "tv_grab_uk_rt")
                maxday = 14;

            for (int i = 0; i < maxday; i++)
            {
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

                QString date(qCurrentDate.addDays(i).toString());
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
                        if (!quiet)
                            cout << "Fetching data for " << date << endl;
                        if (!grabData(*it, i, &qCurrentDate))
                            ++failures;
                    }
                    else
                    {
                        if (!quiet)
                            cout << "Data is already present for " << date
                                 << ", skipping\n";
                    }
                } 
                else
                    MythContext::DBError("checking existing program data", 
                                         query);
            }
        }
        else
        {
            cerr << "Grabbing XMLTV data using " << xmltv_grabber.ascii() 
                 << " is not verified as working.\n";
        }
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
            cout << "--help\n";
            cout << "   This text\n";
            cout << "\n";
            cout << "\n";
            cout << "  --manual and --update can not be used together\n";
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

    if (from_xawfile)
    {
        readXawtvChannels(fromxawfile_id, fromxawfile_name);
    }
    else if (from_file)
    {
        grabDataFromFile(fromfile_id, fromfile_offset, fromfile_name);
    }
    else
    {
        QValueList<Source> sourcelist;

        QSqlQuery sourcequery;
        QString querystr = QString("SELECT sourceid,name,xmltvgrabber,userid "
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

                       sourcelist.append(newsource);
                  }
             }
             else
             {
                  cerr << "There are no channel sources defined, did you run the "
                       << "setup program?\n";
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

    delete gContext;

    return 0;
}
