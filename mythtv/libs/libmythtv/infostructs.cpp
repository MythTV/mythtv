#include <qdatetime.h>
#include <qpixmap.h>
#include <qstring.h>
#include <qsqlquery.h>

#include "infostructs.h"

ProgramInfo::ProgramInfo(void)
{
    spread = -1;
    startCol = -1;

    recordtype = -1;
    conflicting = false;
    recording = true;
}

ProgramInfo::ProgramInfo(const ProgramInfo &other)
{
    title = other.title;
    subtitle = other.subtitle;
    description = other.description;
    category = other.category;
    channum = other.channum;

    startts = other.startts;
    endts = other.endts;
    spread = other.spread;
    startCol = other.startCol;
 
    recordtype = other.recordtype;
    conflicting = other.conflicting;
    recording = other.recording;
}

ProgramInfo *GetProgramAtDateTime(int channel, const char *time)
{
    char thequery[512];
    QSqlQuery query;

    sprintf(thequery, "SELECT * FROM program WHERE channum = %d AND "
                      "starttime < %s AND endtime > %s;",
                      channel, time, time);

    query.exec(thequery);
  
    if (query.isActive() && query.numRowsAffected() > 0)
    {
        query.next();

        ProgramInfo *proginfo = new ProgramInfo;
        proginfo->title = query.value(3).toString();
        proginfo->subtitle = query.value(4).toString();
        proginfo->description = query.value(5).toString();
        proginfo->category = query.value(6).toString();
        proginfo->startts = QDateTime::fromString(query.value(1).toString(), 
                                                  Qt::ISODate);
        proginfo->endts = QDateTime::fromString(query.value(2).toString(),
                                                Qt::ISODate);
        proginfo->channum = query.value(0).toString();
        proginfo->spread = -1;
        proginfo->recordtype = -1;

        return proginfo;
    }

    return NULL;
}

ProgramInfo *GetProgramAtDateTime(int channel, QDateTime &dtime)
{
    int year, month, day, hour, mins;
    year = dtime.date().year();
    month = dtime.date().month();
    day = dtime.date().day();
    hour = dtime.time().hour();
    mins = dtime.time().minute();

    char sqltime[512];
    sprintf(sqltime, "%d%02d%02d%02d%02d50", year, month, day, hour, mins);

    return GetProgramAtDateTime(channel, sqltime);
}

// 0 for no, 1 for weekdaily, 2 for weekly.
int IsProgramRecurring(ProgramInfo *pginfo)
{
    QDateTime dtime = pginfo->startts;
    
    int weekday = dtime.date().dayOfWeek();
    if (weekday < 6) 
    {                  
        // week day    
        int daysadd = 1;
        if (weekday == 5)
            daysadd = 3;
    
        QDateTime checktime = dtime.addDays(daysadd);

        ProgramInfo *nextday = GetProgramAtDateTime(pginfo->channum.toInt(),
                                                    checktime);

        if (nextday && nextday->title == pginfo->title)
        {
            delete nextday;
            return 1;
        }
        if (nextday)
            delete nextday;
    }

    QDateTime checktime = dtime.addDays(7);
    ProgramInfo *nextweek = GetProgramAtDateTime(pginfo->channum.toInt(),
                                                 checktime);
    if (nextweek && nextweek->title == pginfo->title)
    {
        delete nextweek;
        return 2;
    }

    if (nextweek)
        delete nextweek;
    return 0;
}

// returns 0 for no recording, 1 for onetime, 2 for timeslot, 3 for every
int GetProgramRecordingStatus(ProgramInfo *pginfo)
{
    QString starts = pginfo->startts.toString("yyyyMMddhhmm");
    QString endts = pginfo->endts.toString("yyyyMMddhhmm");

    char sqlstarttime[128];
    sprintf(sqlstarttime, "%s00", starts.ascii());

    char sqlendtime[128];
    sprintf(sqlendtime, "%s00", endts.ascii());

    char thequery[512];
    QSqlQuery query;

    sprintf(thequery, "SELECT * FROM singlerecord WHERE channum = %d AND "
                      "starttime = %s AND endtime = %s;",
                      pginfo->channum.toInt(), sqlstarttime, sqlendtime);

    query.exec(thequery);

    if (query.isActive() && query.numRowsAffected() > 0)
    {
        query.next();
        pginfo->recordtype = 1;
        return 1;
    } 

    for (int i = 0; i < 8; i++)
    {
        sqlstarttime[i] = '0';
        sqlendtime[i] = '0';
    }

    sprintf(thequery, "SELECT * FROM timeslotrecord WHERE channum = %d AND "
                      "starttime = %s AND endtime = %s;",
                      pginfo->channum.toInt(), sqlstarttime, sqlendtime);

    query.exec(thequery);

    if (query.isActive() && query.numRowsAffected() > 0)
    {
        query.next();
        pginfo->recordtype = 2;
        return 2;
    }

    sprintf(thequery, "SELECT * FROM allrecord WHERE title = \"%s\";", 
                      pginfo->title.ascii());

    query.exec(thequery);
   
    if (query.isActive() && query.numRowsAffected() > 0)
    {
        query.next();
        pginfo->recordtype = 3;
        return 3;
    }

    pginfo->recordtype = 0;
    return 0;
}

// newstate uses same values as return of GetProgramRecordingState
void ApplyRecordStateChange(ProgramInfo *pginfo, int newstate)
{
    QString starts = pginfo->startts.toString("yyyyMMddhhmm");
    QString endts = pginfo->endts.toString("yyyyMMddhhmm");

    char sqlstarttime[128];
    sprintf(sqlstarttime, "%s00", starts.ascii());

    char sqlendtime[128];
    sprintf(sqlendtime, "%s00", endts.ascii());

    char thequery[512];
    QSqlQuery query;

    if (pginfo->recordtype == 1)
    {
        sprintf(thequery, "DELETE FROM singlerecord WHERE "
                          "channum = %d AND starttime = %s AND "
                          "endtime = %s;", pginfo->channum.toInt(), 
                          sqlstarttime, sqlendtime);
        query.exec(thequery);
    }
    else if (pginfo->recordtype == 2)
    {
        char tempstarttime[512], tempendtime[512];
        strcpy(tempstarttime, sqlstarttime);
        strcpy(tempendtime, sqlendtime); 

        for (int i = 0; i < 8; i++)
        {
            tempstarttime[i] = '0';
            tempendtime[i] = '0';
        }

        sprintf(thequery, "DELETE FROM timeslotrecord WHERE channum = %d "
                          "AND starttime = %s AND endtime = %s;",
                          pginfo->channum.toInt(), tempstarttime, 
                          tempendtime);

        query.exec(thequery);
    }
    else if (pginfo->recordtype == 3)
    {
        sprintf(thequery, "DELETE FROM allrecord WHERE title = \"%s\";",
                          pginfo->title.ascii());

        query.exec(thequery);
    }
    
    if (newstate == 1)
    {
        sprintf(thequery, "INSERT INTO singlerecord (channum,starttime,endtime,"
                          "title,subtitle,description) VALUES(%s, %s, %s, "
                          "\"%s\", \"%s\", \"%s\");", 
                          pginfo->channum.ascii(), sqlstarttime, sqlendtime, 
                          pginfo->title.ascii(), pginfo->subtitle.ascii(),
                          pginfo->description.ascii());

        query.exec(thequery);
    }
    else if (newstate == 2)
    {
        for (int i = 0; i < 8; i++)
        {
            sqlstarttime[i] = '0';
            sqlendtime[i] = '0';
        }

        sprintf(thequery, "INSERT INTO timeslotrecord (channum,starttime,"
                          "endtime,title) VALUES(%s,%s,%s,\"%s\");",
                          pginfo->channum.ascii(), sqlstarttime, sqlendtime,
                          pginfo->title.ascii());
        query.exec(thequery);
    }
    else if (newstate == 3)
    {
        sprintf(thequery, "INSERT INTO allrecord (title) VALUES(\"%s\");",
                          pginfo->title.ascii());
        query.exec(thequery);
    }
 
    if (newstate != pginfo->recordtype)
    {
        sprintf(thequery, "SELECT * FROM settings WHERE value = "
                          "\"RecordChanged\";");
        query.exec(thequery);

        if (query.isActive() && query.numRowsAffected() > 0)
        {
            sprintf(thequery, "UPDATE settings SET data = \"yes\" WHERE "
                              "value = \"RecordChanged\";");
        }
        else
        {
            sprintf(thequery, "INSERT settings (value,data) "
                              "VALUES(\"RecordChanged\", \"yes\");");
        }
        query.exec(thequery);
    }
}
