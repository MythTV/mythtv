#ifndef RECORDINGINFO_H_
#define RECORDINGINFO_H_

#include <string>
using namespace std;

#include <mysql.h>

class RecordingInfo
{
  public:
    RecordingInfo(const string &chan, const string &start, const string &end, 
                  const string &stitle, const string &ssubtitle, 
                  const string &sdescription);
   ~RecordingInfo();

    void GetRecordFilename(string &filename);
    time_t GetStartTime(void);
    time_t GetEndTime(void);

    void WriteToDB(MYSQL *conn);

    void GetChannel(string &schan) { schan = channel; }
    void GetStartTimeStamp(string &start) { start = starttime; }
    void GetEndTimeStamp(string &end) { end = endtime; }
    void GetTitle(string &stitle) { stitle = title; }
    void GetSubTitle(string &ssubtitle) { ssubtitle = subtitle; }
    void GetDescription(string &sdescription) { sdescription = description; }

  private:
    struct tm *GetStructTM(string &timestamp);

    string channel;
    string starttime;
    string endtime;
    string title;
    string subtitle;
    string description;
};

#endif
