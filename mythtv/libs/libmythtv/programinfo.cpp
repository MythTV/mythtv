#include <iostream>
#include <qsocket.h>
#include <qregexp.h>

#include "programinfo.h"
#include "scheduledrecording.h"
#include "util.h"
#include "mythcontext.h"
#include "commercial_skip.h"

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
    hostname = "";

    conflicting = false;
    recording = true;

    sourceid = -1;
    inputid = -1;
    cardid = -1;
    schedulerid = "";

    record = NULL;
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
    hostname = other.hostname;

    startts = other.startts;
    endts = other.endts;
    spread = other.spread;
    startCol = other.startCol;
 
    conflicting = other.conflicting;
    recording = other.recording;

    sourceid = other.sourceid;
    inputid = other.inputid;
    cardid = other.cardid;
    schedulerid = other.schedulerid;

    record = NULL;
}

ProgramInfo::~ProgramInfo() {
    if (record != NULL)
        delete record;
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
    if (hostname == "")
        hostname = " ";
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
    list << channame;
    list << pathname;
    encodeLongLong(list, filesize);
    list << startts.toString();
    list << endts.toString();
    list << QString::number(conflicting);
    list << QString::number(recording);
    list << hostname;
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
    channame = list[offset + 7];
    pathname = list[offset + 8];
    filesize = decodeLongLong(list, offset + 9);
    startts = QDateTime::fromString(list[offset + 11]);
    endts = QDateTime::fromString(list[offset + 12]);
    conflicting = list[offset + 13].toInt();
    recording = list[offset + 14].toInt();
    hostname = list[offset += 15];

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
    if (hostname == " ")
        hostname = "";
    if (chanid == " ")
        chanid = "";
    if (chanstr == " ")
        chanstr = "";
    if (pathname == " ")
        pathname = "";
    if (channame == " ")
        channame = "";
}

int ProgramInfo::CalculateLength(void)
{
    return startts.secsTo(endts);
}

void ProgramInfo::GetProgramRangeDateTime(QPtrList<ProgramInfo> *proglist, QString channel, 
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
    // allowed to use default connection
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

ProgramInfo *ProgramInfo::GetProgramAtDateTime(QString channel, const QString &ltime)
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

ProgramInfo *ProgramInfo::GetProgramAtDateTime(QString channel, QDateTime &dtime)
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

ScheduledRecording::RecordingType ProgramInfo::GetProgramRecordingStatus(QSqlDatabase *db)
{
    if (record == NULL) {
        record = new ScheduledRecording();
        record->loadByProgram(db, *this);
    }

    return record->getRecordingType();
}

// newstate uses same values as return of GetProgramRecordingState
void ProgramInfo::ApplyRecordStateChange(QSqlDatabase *db, 
                                         ScheduledRecording::RecordingType newstate)
{
    GetProgramRecordingStatus(db);
    record->setRecordingType(newstate);
    record->save(db);
}

void ProgramInfo::ApplyRecordTimeChange(QSqlDatabase *db,
                                        const QDateTime &newstartts, 
                                        const QDateTime &newendts)
{
    GetProgramRecordingStatus(db);
    if (record->getRecordingType() != ScheduledRecording::NotRecording) {
        record->setStart(newstartts);
        record->setEnd(newendts);
    }
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
                    "subtitle,description,hostname) "
                    "VALUES(%1,\"%2\",\"%3\",\"%4\",\"%5\",\"%6\",\"%7\");")
                    .arg(chanid).arg(starts).arg(ends).arg(sqltitle.utf8()) 
                    .arg(sqlsubtitle.utf8()).arg(sqldescription.utf8())
                    .arg(gContext->GetHostName());

    QSqlQuery qquery = db->exec(query);
    if (!qquery.isActive())
        MythContext::DBError("WriteRecordedToDB (recorded)", qquery);

    query = QString("INSERT INTO oldrecorded (chanid,starttime,endtime,title,"
                    "subtitle,description) "
                    "VALUES(%1,\"%2\",\"%3\",\"%4\",\"%5\",\"%6\");")
                    .arg(chanid).arg(starts).arg(ends).arg(sqltitle.utf8()) 
                    .arg(sqlsubtitle.utf8()).arg(sqldescription.utf8());

    qquery = db->exec(query);
    if (!qquery.isActive())
        MythContext::DBError("WriteRecordedToDB (oldrecorded)", qquery);

    GetProgramRecordingStatus(db);
    record->doneRecording(db, *this);
}

void ProgramInfo::SetBookmark(long long pos, QSqlDatabase *db)
{
    MythContext::KickDatabase(db);

    char position[128];
    sprintf(position, "%lld", pos);

    QString starts = startts.toString("yyyyMMddhhmm");
    starts += "00";

    QString querystr = QString("UPDATE recorded SET bookmark = '%1', "
                               "starttime = '%2' WHERE chanid = '%3' AND "
                               "starttime = '%4';").arg(position).arg(starts)
                                                   .arg(chanid).arg(starts);
    QSqlQuery query = db->exec(querystr);
    if (!query.isActive())
        MythContext::DBError("Save position update", querystr);
}

long long ProgramInfo::GetBookmark(QSqlDatabase *db)
{
    MythContext::KickDatabase(db);

    long long pos = 0;

    QString starts = startts.toString("yyyyMMddhhmm");
    starts += "00";

    QString querystr = QString("SELECT bookmark FROM recorded WHERE "
                               "chanid = '%1' AND starttime = '%2';")
                              .arg(chanid).arg(starts);

    QSqlQuery query = db->exec(querystr);
    if (query.isActive() && query.numRowsAffected() > 0)
    {
        query.next();

        QString result = query.value(0).toString();
        if (result != QString::null)
        {
            sscanf(result.ascii(), "%lld", &pos);
        }
    }

    return pos;
}

bool ProgramInfo::IsEditing(QSqlDatabase *db)
{
    MythContext::KickDatabase(db);

    bool result = false;

    QString starts = startts.toString("yyyyMMddhhmm");
    starts += "00";

    QString querystr = QString("SELECT editing FROM recorded WHERE "
                               "chanid = '%1' AND starttime = '%2';")
                              .arg(chanid).arg(starts);

    QSqlQuery query = db->exec(querystr);
    if (query.isActive() && query.numRowsAffected() > 0)
    {
        query.next();

        result = query.value(0).toInt();
    }

    return result;
}

void ProgramInfo::SetEditing(bool edit, QSqlDatabase *db)
{
    MythContext::KickDatabase(db);

    QString starts = startts.toString("yyyyMMddhhmm");
    starts += "00";

    QString querystr = QString("UPDATE recorded SET editing = '%1', "
                               "starttime = '%2' WHERE chanid = '%3' AND "
                               "starttime = '%4';").arg(edit).arg(starts)
                                                   .arg(chanid).arg(starts);
    QSqlQuery query = db->exec(querystr);
    if (!query.isActive())
        MythContext::DBError("Edit status update", querystr);
}

void ProgramInfo::GetCutList(QMap<long long, int> &delMap, QSqlDatabase *db)
{
    delMap.clear();

    MythContext::KickDatabase(db);

    QString starts = startts.toString("yyyyMMddhhmm");
    starts += "00";

    QString querystr = QString("SELECT cutlist FROM recorded WHERE "
                               "chanid = '%1' AND starttime = '%2';")
                              .arg(chanid).arg(starts);

    QSqlQuery query = db->exec(querystr);
    if (query.isActive() && query.numRowsAffected() > 0)
    {
        query.next();

        QString cutlist = query.value(0).toString();

        if (cutlist.length() > 1)
        {
            QStringList strlist = QStringList::split("\n", cutlist);
            QStringList::Iterator i;

            for (i = strlist.begin(); i != strlist.end(); ++i)
            {
                long long start = 0, end = 0;
                if (sscanf((*i).ascii(), "%lld - %lld", &start, &end) != 2)
                {
                    cerr << "Malformed cutlist list: " << *i << endl;
                }
                else
                {
                    delMap[start] = 1;
                    delMap[end] = 0;
                }
            }
        }
    }
}

void ProgramInfo::SetCutList(QMap<long long, int> &delMap, QSqlDatabase *db)
{
    QString cutdata;
    char tempc[256];

    QMap<long long, int>::Iterator i;

    for (i = delMap.begin(); i != delMap.end(); ++i)
    {
        long long frame = i.key();
        int direction = i.data();

        if (direction == 1)
        {
            sprintf(tempc, "%lld - ", frame);
            cutdata += tempc;
        }
        else if (direction == 0)
        {
            sprintf(tempc, "%lld\n", frame);
            cutdata += tempc;
        }
    }

    MythContext::KickDatabase(db);

    QString starts = startts.toString("yyyyMMddhhmm");
    starts += "00";

    QString querystr = QString("UPDATE recorded SET cutlist = \"%1\", "
                               "starttime = '%2' WHERE chanid = '%3' AND "
                               "starttime = '%4';").arg(cutdata).arg(starts)
                                                   .arg(chanid).arg(starts);
    QSqlQuery query = db->exec(querystr);
    if (!query.isActive())
        MythContext::DBError("cutlist update", querystr);
}

void ProgramInfo::SetBlankFrameList(QMap<long long, int> &frames,
        QSqlDatabase *db)
{
    QMap<long long, int>::Iterator i;

    QString starts = startts.toString("yyyyMMddhhmm");
    starts += "00";

    QString querystr = QString("DELETE FROM recordedmarkup "
                               "WHERE chanid = '%1' AND starttime = '%2' "
                               "AND type = %3;"
                               ).arg(chanid).arg(starts).arg(MARK_BLANK_FRAME);
    QSqlQuery query = db->exec(querystr);
    if (!query.isActive())
        MythContext::DBError("blank frame list clear", querystr);

    for (i = frames.begin(); i != frames.end(); ++i)
    {
        long long frame = i.key();
        QString frame_str;
        char tempc[128];
        sprintf(tempc, "%lld", frame );
        frame_str += tempc;
        QString querystr = QString("INSERT recordedmarkup (chanid, starttime, "
                                   "mark, type) values ( '%1', '%2', %3, "
                                   "%4);").arg(chanid).arg(starts)
                                   .arg(frame_str).arg(MARK_BLANK_FRAME);
        QSqlQuery query = db->exec(querystr);
        if (!query.isActive())
            MythContext::DBError("blank frame list insert", querystr);
    }
}

void ProgramInfo::GetBlankFrameList(QMap<long long, int> &frames,
        QSqlDatabase *db)
{
    frames.clear();

    MythContext::KickDatabase(db);

    QString starts = startts.toString("yyyyMMddhhmm");
    starts += "00";

    QString querystr = QString("SELECT mark, type FROM recordedmarkup WHERE "
                               "chanid = '%1' AND starttime = '%2' "
                               "AND type = %3 "
                               "ORDER BY mark;")
                               .arg(chanid).arg(starts).arg(MARK_BLANK_FRAME);

    QSqlQuery query = db->exec(querystr);
    if (query.isActive() && query.numRowsAffected() > 0)
    {
        while(query.next())
            frames[query.value(0).toInt()] = query.value(1).toInt();
    }
}

void ProgramInfo::SetCommBreakList(QMap<long long, int> &frames,
        QSqlDatabase *db)
{
    QMap<long long, int>::Iterator i;

    QString starts = startts.toString("yyyyMMddhhmm");
    starts += "00";

    QString querystr = QString("DELETE FROM recordedmarkup "
                               "WHERE chanid = '%1' AND starttime = '%2' "
                               "AND (type = %3 OR type = %4);"
                               ).arg(chanid).arg(starts)
                               .arg(MARK_COMM_START).arg(MARK_COMM_END);
    QSqlQuery query = db->exec(querystr);
    if (!query.isActive())
        MythContext::DBError("blank frame list clear", querystr);

    for (i = frames.begin(); i != frames.end(); ++i)
    {
        long long frame = i.key();
        int mark_type = i.data();
        QString querystr;
        QString frame_str;
        char tempc[128];
        sprintf(tempc, "%lld", frame );
        frame_str += tempc;
        
        querystr = QString("INSERT recordedmarkup (chanid, starttime, "
                               "mark, type) values ( '%1', '%2', %3, "
                               "%4);").arg(chanid).arg(starts)
                               .arg(frame_str).arg(mark_type);
        QSqlQuery query = db->exec(querystr);
        if (!query.isActive())
            MythContext::DBError("commercial break list insert", querystr);
    }
}

void ProgramInfo::GetCommBreakList(QMap<long long, int> &frames,
        QSqlDatabase *db)
{
    frames.clear();

    MythContext::KickDatabase(db);

    QString starts = startts.toString("yyyyMMddhhmm");
    starts += "00";

    QString querystr = QString("SELECT mark, type FROM recordedmarkup WHERE "
                               "chanid = '%1' AND starttime = '%2' "
                               "AND (type = %3 OR type = %4) "
                               "ORDER BY mark;")
                               .arg(chanid).arg(starts)
                               .arg(MARK_COMM_START).arg(MARK_COMM_END);

    QSqlQuery query = db->exec(querystr);
    if (query.isActive() && query.numRowsAffected() > 0)
    {
        while(query.next())
            frames[query.value(0).toInt()] = query.value(1).toInt();
    }
}
