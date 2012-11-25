#ifndef _FILLDATA_H_
#define _FILLDATA_H_

// C++ headers
#include <vector>
using namespace std;

// Qt headers
#include <QString>

// libmythtv headers
#include "datadirect.h"
#include "programdata.h"

// filldata headers
#include "channeldata.h"
#include "xmltvparser.h"
#include "icondata.h"

#define REFRESH_MAX 21

// helper functions to update mfdb status fields in settings
bool updateLastRunEnd(MSqlQuery &query);
bool updateLastRunStart(MSqlQuery &query);
bool updateLastRunStatus(MSqlQuery &query, QString &status);

struct Source
{
    Source() : id(0), name(), xmltvgrabber(), userid(), password(), lineupid(),
        xmltvgrabber_baseline(false), xmltvgrabber_manualconfig(false),
        xmltvgrabber_cache(false), xmltvgrabber_prefmethod() {}
    int id;
    QString name;
    QString xmltvgrabber;
    QString userid;
    QString password;
    QString lineupid;
    bool    xmltvgrabber_baseline;
    bool    xmltvgrabber_manualconfig;
    bool    xmltvgrabber_cache;
    QString xmltvgrabber_prefmethod;
    vector<int> dd_dups;
};
typedef vector<Source> SourceList;

class FillData
{
  public:
    FillData() :
        raw_lineup(0),                  maxDays(0),
        interrupted(false),             endofdata(false),
        refresh_tba(true),              dd_grab_all(false),
        dddataretrieved(false),
        need_post_grab_proc(true),      only_update_channels(false),
        channel_update_run(false),      refresh_all(false)
    {
        SetRefresh(1, true);
    }

    void SetRefresh(int day, bool set);

    void DataDirectStationUpdate(Source source, bool update_icons = true);
    bool DataDirectUpdateChannels(Source source);
    bool GrabDDData(Source source, int poffset,
                    QDate pdate, int ddSource);
    bool GrabDataFromFile(int id, QString &filename);
    bool GrabData(Source source, int offset, QDate *qCurrentDate = 0);
    bool GrabDataFromDDFile(int id, int offset, const QString &filename,
                            const QString &lineupid, QDate *qCurrentDate = 0);
    bool Run(SourceList &sourcelist);

    enum
    {
        kRefreshClear = 0xFFFF0,
        kRefreshAll   = 0xFFFF1,
    };

  public:
    ProgramData         prog_data;
    ChannelData         chan_data;
    XMLTVParser         xmltv_parser;
    IconData            icon_data;
    DataDirectProcessor ddprocessor;

    QString logged_in;
    QString lastdduserid;
    QString graboptions;
    int     raw_lineup;
    uint    maxDays;

    bool    interrupted;
    bool    endofdata;
    bool    refresh_tba;
    bool    dd_grab_all;
    bool    dddataretrieved;
    bool    need_post_grab_proc;
    bool    only_update_channels;
    bool    channel_update_run;

  private:
    QMap<uint,bool>     refresh_day;
    bool                refresh_all;
    mutable QStringList fatalErrors;
};

#endif // _FILLDATA_H_
