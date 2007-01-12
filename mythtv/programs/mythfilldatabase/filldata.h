#ifndef _FILLDATA_H_
#define _FILLDATA_H_

// Qt headers
#include <qstring.h>
#include <qvaluelist.h>

// libmythtv headers
#include "datadirect.h"
#include "programdata.h"

// filldata headers
#include "channeldata.h"
#include "xmltvparser.h"
#include "icondata.h"

struct Source
{
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
};

class FillData
{
  public:
    FillData() :
        logged_in(""),
        lastdduserid(QString::null),    graboptions(""),
        raw_lineup(0),                  maxDays(0),
        interrupted(false),             endofdata(false),
        refresh_today(false),           refresh_tomorrow(true),
        refresh_second(false),          refresh_all(false),
        refresh_tba(true),              dd_grab_all(false),
        dddataretrieved(false),
        need_post_grab_proc(true),      only_update_channels(false),
        channel_update_run(false) {}

    void DataDirectStationUpdate(Source source, bool update_icons = true);
    bool DataDirectUpdateChannels(Source source);
    bool grabDDData(Source source, int poffset,
                    QDate pdate, int ddSource);
    bool grabDataFromFile(int id, QString &filename);
    bool grabData(Source source, int offset, QDate *qCurrentDate = 0);
    void grabDataFromDDFile(int id, int offset, const QString &filename,
                            const QString &lineupid, QDate *qCurrentDate = 0);
    bool fillData(QValueList<Source> &sourcelist);
    ChanInfo *xawtvChannel(QString &id, QString &channel, QString &fine);
    void readXawtvChannels(int id, QString xawrcfile);

    void SetQuiet(bool quiet)
    {
        prog_data.quiet = icon_data.quiet = chan_data.quiet = quiet;
    }

    bool IsQuiet(void)   const { return prog_data.quiet; }
    bool IsVerbose(void) const { return !IsQuiet(); }

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
    int     maxDays;

    bool    interrupted;
    bool    endofdata;
    bool    refresh_today;
    bool    refresh_tomorrow;
    bool    refresh_second;
    bool    refresh_all;
    bool    refresh_tba;
    bool    dd_grab_all;
    bool    dddataretrieved;
    bool    need_post_grab_proc;
    bool    only_update_channels;
    bool    channel_update_run;
};

#endif // _FILLDATA_H_
