#include <qapplication.h>
#include <qdom.h>
#include <qfile.h>
#include <qstring.h>
#include <qstringlist.h>
#include <qvaluelist.h>
#include <qmap.h>
#include <qdatetime.h>
#include <qdir.h>
#include <qfile.h>
#include <qsqldatabase.h>
#include <qsqlquery.h>

#include <iostream>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

#include "libmyth/settings.h"

Settings *globalsettings;
char installprefix[] = "/usr/local";

using namespace std;

class ChanInfo
{
  public:
    ChanInfo() { }
    ChanInfo(const ChanInfo &other) { callsign = other.callsign; 
                                      iconpath = other.iconpath;
                                      chanstr = other.chanstr;
                                    }

    QString callsign;
    QString iconpath;
    QString chanstr;
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

QDateTime fromXMLTVDate(QString &text)
{
    int year, month, day, hour, min, sec;
    QDate ldate;
    QTime ltime;
    QDateTime dt;

    if (text == QString::null)
        return dt;

    year = atoi(text.mid(0, 4).ascii());
    month = atoi(text.mid(4, 2).ascii());
    day = atoi(text.mid(6, 2).ascii());
    hour = atoi(text.mid(8, 2).ascii());
    min = atoi(text.mid(10, 2).ascii());
    sec = atoi(text.mid(12, 2).ascii());

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

ChanInfo *parseChannel(QDomElement &element) 
{
    ChanInfo *chaninfo = new ChanInfo;

    QString chanid = element.attribute("id", "");
    QStringList split = QStringList::split(" ", chanid);

    chaninfo->chanstr = split[0];
    if (split.size() > 1)
        chaninfo->callsign = split[1];
    else
        chaninfo->callsign = "";

    chaninfo->iconpath = "";

    for (QDomNode child = element.firstChild(); !child.isNull();
         child = child.nextSibling())
    {
        QDomElement info = child.toElement();
        if (!info.isNull())
        {
            if (info.tagName() == "icon")
            {
                chaninfo->iconpath = info.attribute("src", "");
            }
        }
    }

    return chaninfo;
}

ProgInfo *parseProgram(QDomElement &element)
{
    ProgInfo *pginfo = new ProgInfo;
 
    QString text = element.attribute("start", "");
    QStringList split = QStringList::split(" ", text);

    pginfo->startts = split[0];

    text = element.attribute("stop", "");
    split = QStringList::split(" ", text);

    if (split.size() > 0)
        pginfo->endts = split[0]; 
    else
        pginfo->endts = "";

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

    QDomNode n = docElem.firstChild();
    while (!n.isNull())
    {
        QDomElement e = n.toElement();
        if (!e.isNull()) 
        {
            if (e.tagName() == "channel")
            {
                ChanInfo *chinfo = parseChannel(e);
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
            fixlist->erase(cur);
        }
    }
}

void handleChannels(QValueList<ChanInfo> *chanlist)
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
        querystr.sprintf("SELECT * FROM channel WHERE channum = %s;", 
                         (*i).chanstr.ascii()); 

        query.exec(querystr);
        if (query.isActive() && query.numRowsAffected() > 0)
        {
            querystr.sprintf("UPDATE channel SET icon = \"%s\" WHERE channum = "
                             "%s AND callsign = \"%s\";", localfile.ascii(), 
                             (*i).chanstr.ascii(),
                             (*i).callsign.ascii());
        }
        else
        {
            querystr.sprintf("INSERT INTO channel (channum,callsign,icon) "
                             "VALUES(%s,\"%s\",\"%s\");", 
                             (*i).chanstr.ascii(), 
                             (*i).callsign.ascii(),
                             localfile.ascii());                  
        }

        query.exec(querystr);
    } 
}

void clearDBAtOffset(int offset)
{
    int nextoffset = offset + 1;
    QString querystr;
    querystr.sprintf("DELETE FROM program WHERE starttime >= "
                     "DATE_ADD(CURRENT_DATE, INTERVAL %d DAY) "
                     "AND starttime < DATE_ADD(CURRENT_DATE, INTERVAL "
                     "%d DAY);", offset, nextoffset);

    QSqlQuery query;

    query.exec(querystr);
}

void handlePrograms(int offset, QMap<QString, QValueList<ProgInfo> > *proglist)
{
    clearDBAtOffset(offset); 
   
    QMap<QString, QValueList<ProgInfo> >::Iterator mapiter;
    for (mapiter = proglist->begin(); mapiter != proglist->end(); ++mapiter)
    {
        QValueList<ProgInfo> *sortlist = &((*proglist)[mapiter.key()]);

        fixProgramList(sortlist);

        QValueList<ProgInfo>::iterator i = sortlist->begin();
        for (; i != sortlist->end(); i++)
        {
            QSqlQuery query;
            QString querystr;
 
            querystr.sprintf("INSERT INTO program (channum,starttime,endtime,"
                             "title,subtitle,description,category) VALUES(%s,"
                             " %s, %s, \"%s\", \"%s\", \"%s\", \"%s\")", 
                             (*i).channel.ascii(), (*i).startts.ascii(), 
                             (*i).endts.ascii(), (*i).title.utf8().data(), 
                             (*i).subtitle.utf8().data(), 
                             (*i).desc.utf8().data(), 
                             (*i).category.utf8().data());
            query.exec(querystr.utf8().data());
        }
    }
}

void grabData(int offset)
{
    QValueList<ChanInfo> chanlist;
    QMap<QString, QValueList<ProgInfo> > proglist;

    char tempfilename[128];
    strcpy(tempfilename, "/tmp/mythXXXXXX");
    mkstemp(tempfilename);

    QString filename = QString(tempfilename);

    QString command;
    command.sprintf("nice -19 tv_grab_na --days 1 --offset %d --output %s", 
                    offset, filename.ascii());

    system(command.ascii());

    parseFile(filename, &chanlist, &proglist);

    QFile thefile(filename);
    thefile.remove();

    handleChannels(&chanlist);
    handlePrograms(offset, &proglist);
}

void clearOldDBEntries(void)
{
    QString querystr = "DELETE FROM program WHERE starttime <= "
                       "DATE_SUB(CURRENT_DATE, INTERVAL 1 DAY)";
    QSqlQuery query;

    query.exec(querystr);
}

void fillData(void)
{
    grabData(1);

    for (int i = 0; i < 9; i++)
    {
        int nextoffset = i + 1;
        QString querystr;
        querystr.sprintf("SELECT * FROM program WHERE starttime >= "
                         "DATE_ADD(CURRENT_DATE, INTERVAL %d DAY) AND "
                         "starttime < DATE_ADD(CURRENT_DATE, INTERVAL %d DAY);",
                         i, nextoffset);

        QSqlQuery query;
        query.exec(querystr);
 
        if (query.isActive() && query.numRowsAffected() > 20)
        {
        }
        else
            grabData(i);
    }

    clearOldDBEntries();
}

int main(int argc, char *argv[])
{
    QApplication a(argc, argv, false);

    globalsettings = new Settings;
    globalsettings->LoadSettingsFiles("mysql.txt", installprefix);

    QSqlDatabase *db = QSqlDatabase::addDatabase("QMYSQL3");
    db->setDatabaseName(globalsettings->GetSetting("DBName"));
    db->setUserName(globalsettings->GetSetting("DBUserName"));
    db->setPassword(globalsettings->GetSetting("DBPassword"));
    db->setHostName(globalsettings->GetSetting("DBHostName"));

    if (!db->open())
    {
        printf("couldn't open db\n");
        return -1;
    }

    fillData();

    return 0;
}
