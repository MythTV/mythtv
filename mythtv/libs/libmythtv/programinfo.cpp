#include "programinfo.h"
#include <iostream>
#include <qsocket.h>

using namespace std;

ProgramInfo::ProgramInfo(void)
{
    spread = -1;
    startCol = -1;

    chanstr = "";
    chansign = "";
    channame = "";

    pathname = "";
    filesize = 0;

    recordtype = kUnknown;
    conflicting = false;
    recording = true;

    sourceid = -1;
    inputid = -1;
    cardid = -1;
    recordingprofileid = -1;
}   
        
ProgramInfo::ProgramInfo(const ProgramInfo &other)
{           
    title = other.title;
    subtitle = other.subtitle;
    description = other.description;
    category = other.category;
    chanid = other.chanid;
    chanstr = other.chanstr;
    chansign = other.chansign;
    channame = other.channame;
    pathname = other.pathname;
    filesize = other.filesize;

    startts = other.startts;
    endts = other.endts;
    spread = other.spread;
    startCol = other.startCol;
 
    recordtype = other.recordtype;
    conflicting = other.conflicting;
    recording = other.recording;

    sourceid = other.sourceid;
    inputid = other.inputid;
    cardid = other.cardid;
    recordingprofileid = other.recordingprofileid;
}   

void ProgramInfo::ToStringList(QStringList &list)
{
    if (title == "")
        title = " ";
    if (subtitle == "")
        subtitle = " ";
    if (description == "")
        description = " ";
    if (category == "")
        category = " ";
    if (pathname == "")
        pathname = " ";
    if (chanid == "")
        chanid = " ";
    if (chanstr == "")
        chanstr = " ";
    if (pathname == "")
        pathname = " ";

    list << title;
    list << subtitle;
    list << description;
    list << category;
    list << chanid;
    list << chanstr;
    list << chansign;
    list << pathname;
    list << QString::number((int)(filesize & 0xffffffff));
    list << QString::number((int)((filesize >> 32) & 0xffffffff));
    list << startts.toString();
    list << endts.toString();
    list << QString::number((int)recordtype);
    list << QString::number(conflicting);
    list << QString::number(recording);
}

void ProgramInfo::FromStringList(QStringList &list, int offset)
{
    if (offset + NUMPROGRAMLINES > (int)list.size())
    {
        cerr << "offset is: " << offset << " but size is " << list.size() 
             << endl;
        return;
    }

    title = list[offset];
    subtitle = list[offset + 1];
    description = list[offset + 2];
    category = list[offset + 3];
    chanid = list[offset + 4];
    chanstr = list[offset + 5];
    chansign = list[offset + 6];
    pathname = list[offset + 7];
    filesize = list[offset + 8].toInt();
    filesize |= ((long long)list[offset + 9].toInt()) << 32;
    startts = QDateTime::fromString(list[offset + 10]);
    endts = QDateTime::fromString(list[offset + 11]);
    recordtype = (RecordingType)(list[offset + 12].toInt());
    conflicting = list[offset + 13].toInt();
    recording = list[offset + 14].toInt();

    if (title == " ")
        title = "";
    if (subtitle == " ")
        subtitle = "";
    if (description == " ")
        description = "";
    if (category == " ")
        category = "";
    if (pathname == " ")
        pathname = "";
    if (chanid == " ")
        chanid = "";
    if (chanstr == " ")
        chanstr = "";
    if (pathname == " ")
        pathname = "";
}

int ProgramInfo::CalculateLength(void)
{
    return startts.secsTo(endts);
}

void GetProgramRangeDateTime(QPtrList<ProgramInfo> *proglist, QString channel, 
                             const QString &ltime, const QString &rtime)
{
    QSqlQuery query;
    QString thequery;

    thequery = QString("SELECT channel.chanid,starttime,endtime,title,subtitle,"
                       "description,category,channel.channum,channel.callsign, "
                       "channel.name FROM program,channel "
                       "WHERE program.chanid = %1 AND endtime >= %1 AND "
                       "starttime <= %3 AND program.chanid = channel.chanid "
                       "ORDER BY starttime;")
                       .arg(channel).arg(ltime).arg(rtime);
    query.exec(thequery);

    if (query.isActive() && query.numRowsAffected() > 0)
    {
        while (query.next())
        {
            ProgramInfo *proginfo = new ProgramInfo;
            proginfo->chanid = query.value(0).toString();
            proginfo->startts = QDateTime::fromString(query.value(1).toString(),
                                                      Qt::ISODate);
            proginfo->endts = QDateTime::fromString(query.value(2).toString(),
                                                    Qt::ISODate);
            proginfo->title = QString::fromUtf8(query.value(3).toString());
            proginfo->subtitle = QString::fromUtf8(query.value(4).toString());
            proginfo->description = 
                                   QString::fromUtf8(query.value(5).toString());
            proginfo->category = QString::fromUtf8(query.value(6).toString());
            proginfo->chanstr = query.value(7).toString();
            proginfo->chansign = query.value(8).toString();
            proginfo->channame = query.value(9).toString();
            proginfo->spread = -1;
            proginfo->recordtype = kUnknown;

            if (proginfo->title == QString::null)
                proginfo->title = "";
            if (proginfo->subtitle == QString::null)
                proginfo->subtitle = "";
            if (proginfo->description == QString::null)
                proginfo->description = "";
            if (proginfo->category == QString::null)
                proginfo->category = "";

            proglist->append(proginfo);
        }
    }
}    

ProgramInfo *GetProgramAtDateTime(QString channel, const QString &ltime)
{
    QSqlQuery query;
    QString thequery;
   
    thequery = QString("SELECT channel.chanid,starttime,endtime,title,subtitle,"
                       "description,category,channel.channum,channel.callsign, "
                       "channel.name FROM program,channel WHERE "
                       "program.chanid = %1 AND starttime < %2 AND "
                       "endtime > %3 AND program.chanid = channel.chanid;")
                       .arg(channel).arg(ltime).arg(ltime);

    query.exec(thequery);

    if (query.isActive() && query.numRowsAffected() > 0)
    {
        query.next();

        ProgramInfo *proginfo = new ProgramInfo;
        proginfo->chanid = query.value(0).toString();
        proginfo->startts = QDateTime::fromString(query.value(1).toString(),
                                                  Qt::ISODate);
        proginfo->endts = QDateTime::fromString(query.value(2).toString(),
                                                Qt::ISODate);
        proginfo->title = QString::fromUtf8(query.value(3).toString());
        proginfo->subtitle = QString::fromUtf8(query.value(4).toString());
        proginfo->description = QString::fromUtf8(query.value(5).toString());
        proginfo->category = QString::fromUtf8(query.value(6).toString());
        proginfo->chanstr = query.value(7).toString();
        proginfo->chansign = query.value(8).toString();
        proginfo->channame = query.value(9).toString();
        proginfo->spread = -1;
        proginfo->recordtype = kUnknown;

        if (proginfo->title == QString::null)
            proginfo->title = "";
        if (proginfo->subtitle == QString::null)
            proginfo->subtitle = "";
        if (proginfo->description == QString::null)
            proginfo->description = "";
        if (proginfo->category == QString::null)
            proginfo->category = "";

        return proginfo;
    }

    return NULL;
}

ProgramInfo *GetProgramAtDateTime(QString channel, QDateTime &dtime)
{
    QString sqltime = dtime.toString("yyyyMMddhhmm");
    sqltime += "50"; 

    return GetProgramAtDateTime(channel, sqltime);
}

// -1 for no data, 0 for no, 1 for weekdaily, 2 for weekly.
int ProgramInfo::IsProgramRecurring(void)
{
    QDateTime dtime = startts;

    int weekday = dtime.date().dayOfWeek();
    if (weekday < 6)
    {
        // week day    
        int daysadd = 1;
        if (weekday == 5)
            daysadd = 3;

        QDateTime checktime = dtime.addDays(daysadd);

        ProgramInfo *nextday = GetProgramAtDateTime(chanid, checktime);

        if (NULL == nextday)
            return -1;

        if (nextday && nextday->title == title)
        {
            delete nextday;
            return 1;
        }
        if (nextday)
            delete nextday;
    }

    QDateTime checktime = dtime.addDays(7);
    ProgramInfo *nextweek = GetProgramAtDateTime(chanid, checktime);

    if (NULL == nextweek)
        return -1;

    if (nextweek && nextweek->title == title)
    {
        delete nextweek;
        return 2;
    }

    if (nextweek)
        delete nextweek;
    return 0;
}

RecordingType ProgramInfo::GetProgramRecordingStatus(void)
{
    QString starts = startts.toString("yyyyMMddhhmm");
    QString ends = endts.toString("yyyyMMddhhmm");

    char sqlstarttime[128];
    sprintf(sqlstarttime, "%s00", starts.ascii());

    char sqlendtime[128];
    sprintf(sqlendtime, "%s00", ends.ascii());

    QString thequery;
    QSqlQuery query;

    thequery = QString("SELECT NULL FROM singlerecord WHERE chanid = %1 AND "
                       "starttime = %2 AND endtime = %3 AND title = \"%4\";")
                       .arg(chanid).arg(sqlstarttime).arg(sqlendtime)
                       .arg(title);

    query.exec(thequery);

    if (query.isActive() && query.numRowsAffected() > 0)
    {
        query.next();
        recordtype = kSingleRecord;
        return recordtype;
    }

    for (int i = 0; i < 8; i++)
    {
        sqlstarttime[i] = '0';
        sqlendtime[i] = '0';
    }

    thequery = QString("SELECT NULL FROM timeslotrecord WHERE chanid = %1 AND "
                       "starttime = %2 AND endtime = %3 AND title = \"%4\";")
                       .arg(chanid).arg(sqlstarttime).arg(sqlendtime)
                       .arg(title);

    query.exec(thequery);

    if (query.isActive() && query.numRowsAffected() > 0)
    {
        query.next();
        recordtype = kTimeslotRecord;
        return recordtype;
    }

    thequery = QString("SELECT chanid FROM allrecord WHERE title = \"%1\";")
                       .arg(title);

    query.exec(thequery);

    if (query.isActive() && query.numRowsAffected() > 0)
    {
        query.next();

        if (query.value(0).toInt() > 0)
        {
            if (query.value(0).toString() == chanid)
            {
                recordtype = kChannelRecord;
                return recordtype;
            }
        }
        else
        {    
            recordtype = kAllRecord;
            return recordtype;
        }
    }

    recordtype = kNotRecording;
    return recordtype;
}

// newstate uses same values as return of GetProgramRecordingState
void ProgramInfo::ApplyRecordStateChange(RecordingType newstate)
{
    QString starts = startts.toString("yyyyMMddhhmm");
    QString ends = endts.toString("yyyyMMddhhmm");

    char sqlstarttime[128];
    sprintf(sqlstarttime, "%s00", starts.ascii());

    char sqlendtime[128];
    sprintf(sqlendtime, "%s00", ends.ascii());

    QString thequery;
    QSqlQuery query;

    if (recordtype == kSingleRecord)
    {
        thequery = QString("DELETE FROM singlerecord WHERE "
                           "chanid = %1 AND starttime = %2 AND "
                           "endtime = %3;").arg(chanid).arg(sqlstarttime)
                           .arg(sqlendtime);
        if (!query.exec(thequery))
        {
            cerr << "DB Error: record deletion failed, SQL query was:" << endl;
            cerr << thequery << endl;
        }
    }
    else if (recordtype == kTimeslotRecord)
    {
        char tempstarttime[512], tempendtime[512];
        strcpy(tempstarttime, sqlstarttime);
        strcpy(tempendtime, sqlendtime);

        for (int i = 0; i < 8; i++)
        {
            tempstarttime[i] = '0';
            tempendtime[i] = '0';
        }

        thequery = QString("DELETE FROM timeslotrecord WHERE chanid = %1 "
                           "AND starttime = %2 AND endtime = %3;")
                           .arg(chanid).arg(tempstarttime).arg(tempendtime);

        if (!query.exec(thequery))
        {
            cerr << "DB Error: record deletion failed, SQL query was:" << endl;
            cerr << thequery << endl;
        }
    }
    else if (recordtype == kChannelRecord)
    {
        thequery = QString("DELETE FROM allrecord WHERE title = \"%1\" AND "
                           "chanid = %2;").arg(title).arg(chanid);

        if (!query.exec(thequery))
        {
            cerr << "DB Error: record deletion failed, SQL query was:" << endl;
            cerr << thequery << endl;
        }
    }
    else if (recordtype == kAllRecord)
    {
        thequery = QString("DELETE FROM allrecord WHERE title = \"%1\";")
                          .arg(title);

        if (!query.exec(thequery))
        {
            cerr << "DB Error: record deletion failed, SQL query was:" << endl;
            cerr << thequery << endl;
        }
    }

    QString sqltitle = title;
    QString sqlsubtitle = subtitle;
    QString sqldescription = description;

    sqltitle.replace(QRegExp("\""), QString("\\\""));
    sqlsubtitle.replace(QRegExp("\""), QString("\\\""));
    sqldescription.replace(QRegExp("\""), QString("\\\""));

    if (newstate == kSingleRecord)
    {
        thequery = QString("INSERT INTO singlerecord (chanid,starttime,"
                           "endtime,title,subtitle,description) "
                           "VALUES(%1, %2, %3, \"%4\", \"%5\", \"%6\");")
                           .arg(chanid).arg(sqlstarttime).arg(sqlendtime)
                           .arg(sqltitle).arg(sqlsubtitle).arg(sqldescription);
        if (!query.exec(thequery))
        {
            cerr << "DB Error: record insertion failed, SQL query was:" << endl;
            cerr << thequery << endl;
        }
    }
    else if (newstate == kTimeslotRecord)
    {
        for (int i = 0; i < 8; i++)
        {
            sqlstarttime[i] = '0';
            sqlendtime[i] = '0';
        }

        thequery = QString("INSERT INTO timeslotrecord (chanid,starttime,"
                           "endtime,title) VALUES(%1,%2,%3,\"%4\");")
                           .arg(chanid).arg(sqlstarttime).arg(sqlendtime)
                           .arg(sqltitle);
        if (!query.exec(thequery))
        {
            cerr << "DB Error: record insertion failed, SQL query was:" << endl;
            cerr << thequery << endl;
        }
    }
    else if (newstate == kChannelRecord)
    {
        thequery = QString("INSERT INTO allrecord (title,chanid) VALUES("
                           "\"%1\",%2);").arg(sqltitle).arg(chanid);
        if (!query.exec(thequery))
        {
            cerr << "DB Error: record insertion failed, SQL query was:" << endl;
            cerr << thequery << endl;
        }
    }
    else if (newstate == kAllRecord)
    {
        thequery = QString("INSERT INTO allrecord (title) VALUES(\"%1\");")
                           .arg(sqltitle);
        if (!query.exec(thequery))
        {
            cerr << "DB Error: record insertion failed, SQL query was:" << endl;
            cerr << thequery << endl;
        }
    }

    if (newstate != recordtype)
    {
        thequery = "SELECT NULL FROM settings WHERE value = \"RecordChanged\";";
        query.exec(thequery);

        if (query.isActive() && query.numRowsAffected() > 0)
        {
            thequery = "UPDATE settings SET data = \"yes\" WHERE "
                       "value = \"RecordChanged\";";
        }
        else
        {
            thequery = "INSERT settings (value,data) "
                       "VALUES(\"RecordChanged\", \"yes\");";
        }
        query.exec(thequery);
    }

    recordtype = newstate;
}

bool ProgramInfo::IsSameProgram(const ProgramInfo& other) const
{
    if (title == other.title &&
        subtitle.length() > 2 &&
        description.length() > 2 &&
        subtitle == other.subtitle &&
        description == other.description)
        return true;
    else
        return false;
}
 
bool ProgramInfo::IsSameTimeslot(const ProgramInfo& other) const
{
    if (chanid == other.chanid &&
        startts == other.startts &&
        endts == other.endts &&
        sourceid == other.sourceid &&
        cardid == other.cardid &&
	inputid == other.inputid)
        return true;
    else
        return false;
}

QString ProgramInfo::GetRecordFilename(const QString &prefix)
{
    QString starts = startts.toString("yyyyMMddhhmm");
    QString ends = endts.toString("yyyyMMddhhmm");

    starts += "00";
    ends += "00";

    QString retval = QString("%1/%2_%3_%4.nuv").arg(prefix).arg(chanid)
                             .arg(starts).arg(ends);
    
    return retval;
}               

void ProgramInfo::WriteRecordedToDB(QSqlDatabase *db)
{
    if (!db)
        return;

    QString starts = startts.toString("yyyyMMddhhmm");
    QString ends = endts.toString("yyyyMMddhhmm");

    starts += "00";
    ends += "00";

    QString sqltitle = title;
    QString sqlsubtitle = subtitle;
    QString sqldescription = description;

    sqltitle.replace(QRegExp("\""), QString("\\\""));
    sqlsubtitle.replace(QRegExp("\""), QString("\\\""));
    sqldescription.replace(QRegExp("\""), QString("\\\""));

    QString query;
    query = QString("INSERT INTO recorded (chanid,starttime,endtime,title,"
                    "subtitle,description) "
                    "VALUES(%1,\"%2\",\"%3\",\"%4\",\"%5\",\"%6\");")
                    .arg(chanid).arg(starts).arg(ends).arg(sqltitle) 
                    .arg(sqlsubtitle).arg(sqldescription);

    QSqlQuery qquery = db->exec(query);
    if (!qquery.isActive())
    {
        cerr << "DB Error: recorded program insertion failed, SQL query "
             << "was:" << endl;
        cerr << query << endl;
    }

    query = QString("INSERT INTO oldrecorded (chanid,starttime,endtime,title,"
                    "subtitle,description) "
                    "VALUES(%1,\"%2\",\"%3\",\"%4\",\"%5\",\"%6\");")
                    .arg(chanid).arg(starts).arg(ends).arg(sqltitle) 
                    .arg(sqlsubtitle).arg(sqldescription);

    qquery = db->exec(query);
    if (!qquery.isActive())
    {
        cerr << "DB Error: recorded program insertion failed, SQL query "
             << "was:" << endl;
        cerr << query << endl;
    }

    if (recordtype == kSingleRecord)
    {
        query = QString("DELETE FROM singlerecord WHERE chanid = %1 AND "
                        "starttime = %2 AND endtime = %3;").arg(chanid)
                        .arg(starts).arg(ends);

        qquery = db->exec(query);
        if (!qquery.isActive())
        {
            cerr << "DB Error: recorded program deletion from singlerecord "
                 << "failed, SQL query was:" << endl;
            cerr << query << endl;
        }
    }

}
