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

#include "libmyth/mythcontext.h"

using namespace std;

bool interactive = false;
bool non_us_updating = false;
bool from_file = false;
MythContext *context;

class ChanInfo
{
  public:
    ChanInfo() { }
    ChanInfo(const ChanInfo &other) { callsign = other.callsign; 
                                      iconpath = other.iconpath;
                                      chanstr = other.chanstr;
                                      xmltvid = other.xmltvid;
                                      name = other.name;
                                      finetune = other.finetune;
                                    }

    QString callsign;
    QString iconpath;
    QString chanstr;
    QString xmltvid;
    QString name;
    QString finetune;
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
                                      start = other.start;
                                      end = other.end;
                                    }

    QString startts;
    QString endts;
    QString channel;
    QString title;
    QString subtitle;
    QString desc;
    QString category;

    QDateTime start;
    QDateTime end;
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

    chaninfo->xmltvid = split[0];
    chaninfo->chanstr = split[0];
    if (split.size() > 1)
        chaninfo->callsign = split[1];
    else
        chaninfo->callsign = "";

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
            else if (info.tagName() == "display-name" && 
                     chaninfo->name.length() == 0)
            {
                chaninfo->name = info.text();
            }
        }
    }

    return chaninfo;
}

void addTimeOffset(QString &timestr, int config_off, QString offset )
{
    bool ok;
    int off = offset.stripWhiteSpace().left(3).toInt(&ok, 10);
               
    if (ok && (off != config_off))
    {
        int diff = config_off - off;
        int year = 0, month = 0, day = 0, hour = 0, min = 0, sec = 0;
                
        if (timestr.length() == 14)
        {
            year  = timestr.left(4).toInt(&ok, 10);
            month = timestr.mid(4,2).toInt(&ok, 10);
            day   = timestr.mid(6,2).toInt(&ok, 10);
            hour  = timestr.mid(8,2).toInt(&ok, 10);
            min   = timestr.mid(10,2).toInt(&ok, 10);
            sec   = timestr.mid(12,2).toInt(&ok, 10);
        }
        else if (timestr.length() == 12)
        {
            year  = timestr.left(2).toInt(&ok, 10);
            month = timestr.mid(2,2).toInt(&ok, 10);
            day   = timestr.mid(4,2).toInt(&ok, 10);
            hour  = timestr.mid(6,2).toInt(&ok, 10);
            min   = timestr.mid(8,2).toInt(&ok, 10);
            sec   = timestr.mid(10,2).toInt(&ok, 10);
        }
        else
        {
            cerr << "Unknown timestamp format: " << timestr << endl;
        }
                
        QDateTime dt = QDateTime(QDate(year, month, day),QTime(hour, min, sec));
        dt = dt.addSecs(diff * 60 * 60);
        timestr = dt.toString("yyyyMMddhhmmss");
    }
}

ProgInfo *parseProgram(QDomElement &element)
{
    QString config_offset = context->GetSetting("TimeOffset");
    
    bool ok;
    int config_off = 0;

    if (config_offset != "")
        config_off = config_offset.left(3).toInt(&ok, 10);

    ProgInfo *pginfo = new ProgInfo;
 
    QString text = element.attribute("start", "");
    QStringList split = QStringList::split(" ", text);

    QString st = split[0];
    QString offset;
    if (split.size() > 1)
        offset = split[1];
    else
        offset = config_offset;

    if (config_off >= 0 && config_off <= 24 && offset != "")
        addTimeOffset(st, config_off, offset);

    pginfo->startts = st;

    text = element.attribute("stop", "");
    split = QStringList::split(" ", text);

    QString et;

    if (split.size() > 0)
    {
        et = split[0];
        if (split.size() > 1)
            offset = split[1];
        else
            offset = config_offset;
    }
    else
        et = "";

    if (config_off >= 0 && config_off <= 24 && offset != "")
        addTimeOffset(et, config_off, offset);

    pginfo->endts = et;

    text = element.attribute("channel", "");
    split = QStringList::split(" ", text);
    
    pginfo->channel = split[0];

    pginfo->start = fromXMLTVDate(pginfo->startts);
    pginfo->end = fromXMLTVDate(pginfo->endts);

    pginfo->subtitle = pginfo->title = pginfo->desc = pginfo->category = "";
    
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
            else if (info.tagName() == "category" && pginfo->category == "")
            {
                pginfo->category = getFirstText(info);
            }
        }
    }

    return pginfo;
}
                  
void parseFile(QString filename, QValueList<ChanInfo> *chanlist,
               QMap<QString, QValueList<ProgInfo> > *proglist)
{
    QDomDocument doc;
    QFile f(filename);

    if (!f.open(IO_ReadOnly))
        return;
    if (!doc.setContent(&f))
    {
        f.close();
        return;
    }

    f.close();

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
                ProgInfo *pginfo = parseProgram(e);
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
        if (i == fixlist->end())
            break;
        // fill in miss stop times
        if ((*cur).endts == "")
        {
            (*cur).endts = (*i).startts;
            (*cur).end = (*i).start;
        }
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

    (*chaninfo).chanstr  = getResponse("Choose a channel number (just like "
                                       "xawtv) ",(*chaninfo).chanstr);
    (*chaninfo).finetune = getResponse("Choose a channel fine tune offset (just"
                                       " like xawtv) ",(*chaninfo).finetune);

    (*chaninfo).iconpath = getResponse("Choose a channel icon image (any path "
                                       "name) ",(*chaninfo).iconpath);

    return(chanid);
}

void handleChannels(int id, QValueList<ChanInfo> *chanlist)
{
    char *home = getenv("HOME");
    QString fileprefix = QString(home) + "/.mythtv";

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
        querystr.sprintf("SELECT chanid,name,callsign,channum,finetune,icon "
                         "FROM channel WHERE xmltvid = \"%s\" AND "
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

                cout << "### " << endl;
                cout << "### Existing channel found" << endl;
                cout << "### " << endl;
                cout << "### xmltvid  = " << (*i).xmltvid.ascii() << endl;
                cout << "### chanid   = " << chanid.ascii()       << endl;
                cout << "### name     = " << name.ascii()         << endl;
                cout << "### callsign = " << callsign.ascii()     << endl;
                cout << "### channum  = " << chanstr.ascii()      << endl;
                cout << "### finetune = " << finetune.ascii()     << endl;
                cout << "### icon     = " << icon.ascii()         << endl;
                cout << "### " << endl;

                (*i).name = name;
                (*i).callsign = callsign;
                (*i).chanstr  = chanstr;
                (*i).finetune = finetune;

                promptForChannelUpdates(i, atoi(chanid.ascii()));

                if (name     != (*i).name ||
                    callsign != (*i).callsign ||
                    chanstr  != (*i).chanstr ||
                    finetune != (*i).finetune ||
                    icon     != localfile)
                {
                    querystr.sprintf("UPDATE channel SET chanid = %s, "
                                     "name = \"%s\", callsign = \"%s\", "
                                     "channum = \"%s\", finetune = %d, "
                                     "icon = \"%s\" WHERE xmltvid = \"%s\" "
                                     "AND sourceid = %d;",
                                     chanid.ascii(),
                                     (*i).name.ascii(),
                                     (*i).callsign.ascii(),
                                     (*i).chanstr.ascii(),
                                     atoi((*i).finetune.ascii()),
                                     localfile.ascii(),
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
                cout << "### finetune = " << (*i).finetune.ascii()     << endl;
                cout << "### icon     = " << localfile.ascii()         << endl;
                cout << "### " << endl;

                unsigned int chanid = promptForChannelUpdates(i,0);

                if (chanid > 0)
                {
                    querystr.sprintf("INSERT INTO channel (chanid,name,"
                                     "callsign,channum,finetune,icon,"
                                     "xmltvid,sourceid) VALUES(%d,\"%s\","
                                     "\"%s\",\"%s\",%d,\"%s\",\"%s\",%d);", 
                                     chanid,
                                     (*i).name.ascii(),
                                     (*i).callsign.ascii(),
                                     (*i).chanstr.ascii(),
                                     atoi((*i).finetune.ascii()),
                                     localfile.ascii(),
                                     (*i).xmltvid.ascii(),
                                     id);

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
                                 "channum,finetune,icon,xmltvid,sourceid) "
                                 "VALUES(%d,\"%s\",\"%s\",\"%s\",%d,\"%s\","
                                 "\"%s\",%d);", 
                                 chanid,
                                 (*i).name.ascii(),
                                 (*i).callsign.ascii(),
                                 (*i).chanstr.ascii(),
                                 atoi((*i).finetune.ascii()),
                                 localfile.ascii(),
                                 (*i).xmltvid.ascii(),
                                 id);

                if (!query.exec(querystr))
                {
                    cerr << "DB Error: Channel insert failed, SQL query "
                         << "was:" << endl;
                    cerr << querystr << endl;
                }

            }
        }
    }
}

void clearDBAtOffset(int offset, int chanid)
{
    int nextoffset = offset + 1;

    if (offset == -1)
    {
        offset = 0;
        nextoffset = 10;
    }

    QString querystr;
    querystr.sprintf("DELETE FROM program WHERE starttime >= "
                     "DATE_ADD(CURRENT_DATE, INTERVAL %d DAY) "
                     "AND starttime < DATE_ADD(CURRENT_DATE, INTERVAL "
                     "%d DAY) AND chanid = %d;", offset, nextoffset, chanid);

    QSqlQuery query;

    query.exec(querystr);
}

void handlePrograms(int id, int offset, QMap<QString, 
                    QValueList<ProgInfo> > *proglist)
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

        clearDBAtOffset(offset, chanid);

        QValueList<ProgInfo> *sortlist = &((*proglist)[mapiter.key()]);

        fixProgramList(sortlist);

        QValueList<ProgInfo>::iterator i = sortlist->begin();
        for (; i != sortlist->end(); i++)
        {
            (*i).title.replace(QRegExp("\""), QString("\\\""));
            (*i).subtitle.replace(QRegExp("\""), QString("\\\""));
            (*i).desc.replace(QRegExp("\""), QString("\\\""));

            querystr.sprintf("INSERT INTO program (chanid,starttime,endtime,"
                             "title,subtitle,description,category) VALUES(%d,"
                             " %s, %s, \"%s\", \"%s\", \"%s\", \"%s\");", 
                             chanid, (*i).startts.ascii(), 
                             (*i).endts.ascii(), (*i).title.utf8().data(), 
                             (*i).subtitle.utf8().data(), 
                             (*i).desc.utf8().data(), 
                             (*i).category.utf8().data());

            if (!query.exec(querystr.utf8().data()))
            {
                cerr << "DB Error: Program insertion failed, SQL query "
                     << "was: " << endl;
                cerr << querystr.utf8().data() << endl;
            }
        }
    }
}


void grabDataFromFile(int id, int offset, QString &filename)
{
    QValueList<ChanInfo> chanlist;
    QMap<QString, QValueList<ProgInfo> > proglist;

    parseFile(filename, &chanlist, &proglist);

    handleChannels(id, &chanlist);
    handlePrograms(id, offset, &proglist);
}

void grabData(Source source, int offset)
{
    char tempfilename[128];
    strcpy(tempfilename, "/tmp/mythXXXXXX");
    mkstemp(tempfilename);

    QString filename = QString(tempfilename);

    char *home = getenv("HOME");
    QString configfile = QString("%1/.mythtv/%2.xmltv").arg(home)
                                                       .arg(source.name);
    QString command;
    QString xmltv_grabber = source.xmltvgrabber;

    if (xmltv_grabber == "tv_grab_uk")
        command.sprintf("nice -19 %s --days 7 --config-file %s --output %s",
                        xmltv_grabber.ascii(), configfile.ascii(), 
                        filename.ascii());
    else if (xmltv_grabber == "tv_grab_de")
        command.sprintf("nice -19 %s --days 7 --output %s",
                        xmltv_grabber.ascii(),
                        filename.ascii());
    else
        command.sprintf("nice -19 %s --days 1 --offset %d --config-file %s "
                        "--output %s", xmltv_grabber.ascii(),
                        offset, configfile.ascii(), filename.ascii());


    cout << "Fetching data for " <<
         QDate::currentDate().addDays(offset).toString() << "...\n";
    cout << "----------------- Start of XMLTV output -----------------" << endl;
 
     system(command.ascii());
 
    cout << "------------------ End of XMLTV output ------------------" << endl;

    grabDataFromFile(source.id, offset, filename);

    QFile thefile(filename);
    thefile.remove();
}

void clearOldDBEntries(void)
{
    QString querystr = "DELETE FROM program WHERE starttime <= "
                       "DATE_SUB(CURRENT_DATE, INTERVAL 1 DAY);";
    QSqlQuery query;

    query.exec(querystr);
}

void fillData(QValueList<Source> &sourcelist)
{
    QValueList<Source>::Iterator it;

    for (it = sourcelist.begin(); it != sourcelist.end(); ++it) {
        QString xmltv_grabber = (*it).xmltvgrabber;
        if (xmltv_grabber == "tv_grab_uk" || xmltv_grabber == "tv_grab_de")
        {
            // tv_grab_uk|de doesn't support the --offset option, so just grab a 
            // week.
            grabData(*it, -1);
        }
        else if (xmltv_grabber == "tv_grab_na" || xmltv_grabber == "tv_grab_aus" ||
             xmltv_grabber == "tv_grab_sn")
        {
            grabData(*it, 1);

            for (int i = 0; i < 9; i++)
            {
                int nextoffset = i + 1;
                QString querystr;
                querystr.sprintf("SELECT COUNT(*) FROM program, channel "
                                 "WHERE program.chanid = channel.chanid "
                                 "AND channel.sourceid = %d "
                                 "AND starttime >= "
                                 "DATE_ADD(CURRENT_DATE, INTERVAL %d DAY) AND "
                                 "starttime < DATE_ADD(CURRENT_DATE, INTERVAL %d "
                                 "DAY);", (*it).id, i, nextoffset);
                
                QSqlQuery query;
                query.exec(querystr);
                
                if (query.isActive() && query.numRowsAffected() > 0) {

                    query.next();
                    if (query.value(0).toInt() < 20)
                        grabData(*it, i);

                } else {
                    cout << "DB error when checking for existing program data: "
                         << query.lastError().databaseText() << endl;
                }
            }
        }
        else
        {
            cout << "Grabbing XMLTV data using " << xmltv_grabber.ascii() 
                 << " is not verified as working.\n";
        }
    }

    clearOldDBEntries();
}

ChanInfo *xawtvChannel(QString &id, QString &channel, QString &fine)
{
    ChanInfo *chaninfo = new ChanInfo;
    chaninfo->xmltvid = id;
    chaninfo->name = id;
    chaninfo->callsign = id;
    chaninfo->chanstr = channel;
    chaninfo->finetune = fine;

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
        else if (!strcmp(a.argv()[argpos], "--update"))
        {
            // For running non-destructive updates on the database for
            // users in xmltv zones that do not provide channel data.
            non_us_updating = true;
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

            cout << "### reading channels from xawtv configfile\n";
            from_xawfile = true;
        }
        else if (!strcmp(a.argv()[argpos], "--help"))
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
            cout << "--file <sourceid> <offset> <xmlfile>\n";
            cout << "   Bypass the grabbers and read data directly from a file\n";
            cout << "   <sourceid> = cardinput\n";
            cout << "   <offset>   = days from today that xmlfile defines\n";
            cout << "                (-1 meaning from today on all future entries)\n";
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
            printf("illegal option: '%s' (use --help)\n", a.argv()[argpos]);
            return -1;
        }

        ++argpos;
    }

    context = new MythContext(false);

    QSqlDatabase *db = QSqlDatabase::addDatabase("QMYSQL3");
    if (!context->OpenDatabase(db))
    {
        printf("couldn't open db\n");
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
        QString querystr = QString("SELECT sourceid,name,xmltvgrabber FROM videosource "
                                   "ORDER BY sourceid;");
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
    
        fillData(sourcelist);
    }

    delete context;

    return 0;
}
