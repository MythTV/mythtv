#include <qdatetime.h>
#include <qpixmap.h>
#include <qstring.h>
#include <qsqlquery.h>

#include "infostructs.h"

QDate *ProgramInfo::getStartDate(void)
{
    QString year, month, day;

    if (starttime[4] == '-')
    {
        year = starttime.mid(0, 4);
        month = starttime.mid(5, 2);
        day = starttime.mid(8, 2);
    }
    else
    {
        year = starttime.mid(0, 4);
        month = starttime.mid(4, 2);
        day = starttime.mid(6, 2);
    }
    
    QDate *ret = new QDate(year.toInt(), month.toInt(), day.toInt());

    return ret;
}

QDate *ProgramInfo::getEndDate(void)
{
    QString year, month, day;

    if (starttime[4] == '-')
    {
        year = endtime.mid(0, 4);
        month = endtime.mid(5, 2);
        day = endtime.mid(8, 2);
    }
    else
    {
        year = endtime.mid(0, 4);
        month = endtime.mid(4, 2);
        day = endtime.mid(6, 2);
    }
    
    QDate *ret = new QDate(year.toInt(), month.toInt(), day.toInt());

    return ret;
}

QTime *ProgramInfo::getStartTime(void)
{
    QString hour, min;

    if (starttime[4] == '-')
    {
        hour = starttime.mid(11, 2);
        min = starttime.mid(14, 2);
    }
    else
    {
        hour = starttime.mid(8, 2);
        min = starttime.mid(10, 2);
    }

    QTime *ret = new QTime(hour.toInt(), min.toInt());

    return ret;
}

QTime *ProgramInfo::getEndTime(void)
{
    QString hour, min;
   
    if (endtime[4] == '-')
    { 
        hour = endtime.mid(11, 2);
        min = endtime.mid(14, 2);
    }
    else
    {
        hour = endtime.mid(8, 2);
        min = endtime.mid(10, 2);
    }

    QTime *ret = new QTime(hour.toInt(), min.toInt());

    return ret;
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
        proginfo->starttime = query.value(1).toString();
        proginfo->endtime = query.value(2).toString();
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
    QTime *starttime = pginfo->getStartTime();
    QDate *startdate = pginfo->getStartDate();
    
    QDateTime dtime(*startdate, *starttime);
    
    int weekday = startdate->dayOfWeek();
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
    QDate *startdate, *enddate;
    startdate = pginfo->getStartDate();
    enddate = pginfo->getEndDate();
    QTime *starttime, *endtime;
    starttime = pginfo->getStartTime();
    endtime = pginfo->getEndTime();

    int year, month, day, hour, mins;
    year = startdate->year();
    month = startdate->month();
    day = startdate->day();
    hour = starttime->hour();
    mins = starttime->minute();

    char sqlstarttime[512];
    sprintf(sqlstarttime, "%d%02d%02d%02d%02d00", year, month, day, hour, mins);

    year = enddate->year();
    month = enddate->month();
    day = enddate->day();
    hour = endtime->hour();
    mins = endtime->minute();

    char sqlendtime[512];
    sprintf(sqlendtime, "%d%02d%02d%02d%02d00", year, month, day, hour, mins);

    delete startdate;
    delete enddate;
    delete starttime;
    delete endtime;

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
    QDate *startdate, *enddate;
    startdate = pginfo->getStartDate();
    enddate = pginfo->getEndDate();
    QTime *starttime, *endtime;
    starttime = pginfo->getStartTime();
    endtime = pginfo->getEndTime();

    int year, month, day, hour, mins;
    year = startdate->year();
    month = startdate->month();
    day = startdate->day();
    hour = starttime->hour();
    mins = starttime->minute();

    char sqlstarttime[512];
    sprintf(sqlstarttime, "%d%02d%02d%02d%02d00", year, month, day, hour, mins);

    year = enddate->year();
    month = enddate->month();
    day = enddate->day();
    hour = endtime->hour();
    mins = endtime->minute();

    char sqlendtime[512];
    sprintf(sqlendtime, "%d%02d%02d%02d%02d00", year, month, day, hour, mins);

    delete startdate;
    delete enddate;
    delete starttime;
    delete endtime;

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
