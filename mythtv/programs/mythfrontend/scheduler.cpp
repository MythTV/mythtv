#include <unistd.h>
#include <qsqldatabase.h>
#include <qsqlquery.h>
#include <qstring.h>
#include <qdatetime.h>

#include <algorithm>
using namespace std;

#include "scheduler.h"
#include "tv.h"
#include "infostructs.h"

Scheduler::Scheduler(TV *ltv)
{
    tv = ltv;
}

Scheduler::~Scheduler()
{
    while (recordingList.size() > 0)
    {
        ProgramInfo *pginfo = recordingList.back();
        delete pginfo;
        recordingList.pop_back();
    }
}

bool Scheduler::CheckForChanges(void)
{
    QSqlDatabase *db = QSqlDatabase::database("SUBDB");
    QSqlQuery query;
    char thequery[512];

    bool retval = false;

    sprintf(thequery, "SELECT * FROM settings WHERE value = "
                      "\"RecordChanged\";");
    query = db->exec(thequery);

    if (query.isActive() && query.numRowsAffected() > 0)
    {
        query.next();

        QString value = query.value(1).toString();
        if (value == "yes")
        {
            sprintf(thequery, "UPDATE settings SET data = \"no\" WHERE "
                              "value = \"RecordChanged\";");
            query = db->exec(thequery);

            retval = true;
        }
    }

    return retval;
}

class comp_proginfo 
{
  public:
    bool operator()(ProgramInfo *a, ProgramInfo *b)
    {
        return (a->starttime < b->starttime);
    }
};

bool Scheduler::FillRecordLists(void)
{
    while (recordingList.size() > 0)
    {
        ProgramInfo *pginfo = recordingList.back();
        delete pginfo;
        recordingList.pop_back();
    }

    char thequery[512];
    QSqlDatabase *db = QSqlDatabase::database("SUBDB");
    QSqlQuery query;
    QSqlQuery subquery;

    sprintf(thequery, "SELECT * FROM singlerecord;");

    query = db->exec(thequery);
    if (query.isActive() && query.numRowsAffected() > 0)
    {
        while (query.next())
        {
            ProgramInfo *proginfo = new ProgramInfo;
            proginfo->title = query.value(3).toString();
            proginfo->subtitle = query.value(4).toString();
            proginfo->description = query.value(5).toString();
            proginfo->starttime = query.value(1).toString();
            proginfo->endtime = query.value(2).toString();
            proginfo->channum = query.value(0).toString();
            proginfo->recordtype = 1;

            recordingList.push_back(proginfo);
        }
    }

    sprintf(thequery, "SELECT * FROM timeslotrecord;");

    query = db->exec(thequery);
    if (query.isActive() && query.numRowsAffected() > 0)
    {
        while (query.next())
        {
            QString channum = query.value(0).toString();
            QString starttime = query.value(1).toString();
            QString endtime = query.value(2).toString();
            QString title = query.value(3).toString();

            int hour, min;

            hour = starttime.mid(0, 2).toInt();
            min = starttime.mid(3, 2).toInt();

            QDate curdate = QDate::currentDate();
            char startquery[128], endquery[128];

            sprintf(startquery, "%4d%02d%02d%02d%02d00", curdate.year(),
                    curdate.month(), curdate.day(), hour, min);

            curdate = curdate.addDays(2);
            sprintf(endquery, "%4d%02d%02d%02d%02d00", curdate.year(),
                    curdate.month(), curdate.day(), hour, min);

            sprintf(thequery, "SELECT * FROM program WHERE channum = %s AND "
                              "starttime = %s AND endtime < %s AND "
                              "title = \"%s\";", channum.ascii(), startquery,
                              endquery, title.ascii());
            subquery = db->exec(thequery);

            if (subquery.isActive() && subquery.numRowsAffected() > 0)
            {
                while (subquery.next())
                {
                    ProgramInfo *proginfo = new ProgramInfo;
                    proginfo->title = subquery.value(3).toString();
                    proginfo->subtitle = subquery.value(4).toString();
                    proginfo->description = subquery.value(5).toString();
                    proginfo->starttime = subquery.value(1).toString();
                    proginfo->endtime = subquery.value(2).toString();
                    proginfo->channum = subquery.value(0).toString();
                    proginfo->recordtype = 2;

                    recordingList.push_back(proginfo);
                }
            }
        }
    }

    sprintf(thequery, "SELECT * FROM allrecord;");

    query = db->exec(thequery);
    if (query.isActive() && query.numRowsAffected() > 0)
    {
        while (query.next())
        {
            QString title = query.value(0).toString();

            QTime curtime = QTime::currentTime();
            QDate curdate = QDate::currentDate();
            char startquery[128], endquery[128];

            sprintf(startquery, "%4d%02d%02d%02d%02d00", curdate.year(),
                    curdate.month(), curdate.day(), curtime.hour(), 
                    curtime.minute());

            curdate = curdate.addDays(2);
            sprintf(endquery, "%4d%02d%02d%02d%02d00", curdate.year(),
                    curdate.month(), curdate.day(), curtime.hour(), 
                    curtime.minute());

            sprintf(thequery, "SELECT * FROM program WHERE "
                              "starttime >= %s AND endtime < %s AND "
                              "title = \"%s\";", startquery,
                              endquery, title.ascii());
            subquery = db->exec(thequery);

            if (subquery.isActive() && subquery.numRowsAffected() > 0)
            {
                while (subquery.next())
                {
                    ProgramInfo *proginfo = new ProgramInfo;
                    proginfo->title = subquery.value(3).toString();
                    proginfo->subtitle = subquery.value(4).toString();
                    proginfo->description = subquery.value(5).toString();
                    proginfo->starttime = subquery.value(1).toString();
                    proginfo->endtime = subquery.value(2).toString();
                    proginfo->channum = subquery.value(0).toString();
                    proginfo->recordtype = 3;

                    recordingList.push_back(proginfo);
                }
            }
        }
    }

    if (recordingList.size() > 0)
    {
        sort(recordingList.begin(), recordingList.end(), comp_proginfo());
    }

    return false;
}

ProgramInfo *Scheduler::GetNextRecording(void)
{
    if (recordingList.size() > 0)
        return recordingList[0];
    return NULL;
}

void Scheduler::RemoveFirstRecording(void)
{
    if (recordingList.size() == 0)
        return;

    ProgramInfo *rec = recordingList[0];

    if (rec->recordtype == 1)
    {
        char startt[128];
        char endt[128];

        QDateTime starttim = QDateTime::fromString(rec->starttime, Qt::ISODate);
        QDateTime endtime = QDateTime::fromString(rec->endtime, Qt::ISODate);
    
        sprintf(startt, "%4d%02d%02d%02d%02d00", starttim.date().year(),
                starttim.date().month(), starttim.date().day(),
                starttim.time().hour(), starttim.time().minute());
        sprintf(endt, "%4d%02d%02d%02d%02d00", endtime.date().year(),
                endtime.date().month(), endtime.date().day(), 
                endtime.time().hour(), endtime.time().minute());
    
        QSqlDatabase *db = QSqlDatabase::database("SUBDB");
        QSqlQuery query;                                          
        char thequery[512];
        sprintf(thequery, "DELETE FROM singlerecord WHERE "
                          "channum = %d AND starttime = %s AND "
                          "endtime = %s;", rec->channum.toInt(),
                          startt, endt);

        query = db->exec(thequery);
    }

    delete rec;
    recordingList.erase(recordingList.begin()); 
}
