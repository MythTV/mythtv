#include <stdio.h>
#include "recordinginfo.h"

RecordingInfo::RecordingInfo(const string &chan, const string &start, 
                             const string &end, const string &stitle,
                             const string &ssubtitle, 
                             const string &sdescription)
{
    channel = chan;
    starttime = starttime;
    endtime = end;
    title = stitle;
    subtitle = ssubtitle;
    description = sdescription;
}

RecordingInfo::~RecordingInfo(void)
{
}

void RecordingInfo::GetRecordFilename(string &filename)
{
    char tempstr[1024];

    sprintf(tempstr, "%s_%s_%s.nuv", channel.c_str(), starttime.c_str(), 
                                     endtime.c_str());
    filename = tempstr;
}

time_t RecordingInfo::GetStartTime(void)
{
    struct tm *convtime = GetStructTM(starttime);

    time_t ret = mktime(convtime);
    free(convtime);
    return ret;
}

time_t RecordingInfo::GetEndTime(void)
{
    struct tm *convtime = GetStructTM(endtime);

    time_t ret = mktime(convtime);
    free(convtime);
    return ret;
}

struct tm *RecordingInfo::GetStructTM(string &timestamp)
{
    string hour, min, sec, year, month, day;

cout << "timestamp is " << timestamp << endl;
    year = timestamp.substr(0, 4);
    month = timestamp.substr(4, 2);
    day = timestamp.substr(6, 2);
    hour = timestamp.substr(8, 2);
    min = timestamp.substr(10, 2);
    sec = timestamp.substr(12, 2);

    struct tm *rettm = new struct tm;
    rettm->tm_sec = atoi(sec.c_str());
    rettm->tm_min = atoi(min.c_str());
    rettm->tm_hour = atoi(hour.c_str());
    rettm->tm_mday = atoi(day.c_str());
    rettm->tm_mon = atoi(month.c_str());
    rettm->tm_year = atoi(year.c_str());

    return rettm;
}

void RecordingInfo::WriteToDB(MYSQL *conn)
{
    if (!conn)
        return;

    char query[4096];
    sprintf(query, "INSERT INTO recorded (channum,starttime,endtime,title,"
                   "subtitle,description) "
                   "VALUES(%s,\"%s\",\"%s\",\"%s\",\"%s\",\"%s\")",
                   channel.c_str(), starttime.c_str(), endtime.c_str(), 
                   title.c_str(), subtitle.c_str(), description.c_str());

    if (mysql_query(conn, query) != 0)
    {
        printf("couldn't insert recording into db\n");
    }
}
