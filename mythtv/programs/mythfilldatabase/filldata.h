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

#define REFRESH_MAX 21

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
    vector<int> dd_dups;
};

class FillData
{
  public:
    FillData() :
        logged_in(""),
        lastdduserid(QString::null),    graboptions(""),
        raw_lineup(0),                  maxDays(0),
        interrupted(false),             endofdata(false),
        refresh_tba(true),              dd_grab_all(false),
        dddataretrieved(false),
        need_post_grab_proc(true),      only_update_channels(false),
        channel_update_run(false) {
	    for( int i = 0; i < REFRESH_MAX; i++ ) refresh_request[i] = false;
	    refresh_request[1] = true;
	}

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
    bool    refresh_request[REFRESH_MAX];
    bool    refresh_tba;
    bool    dd_grab_all;
    bool    dddataretrieved;
    bool    need_post_grab_proc;
    bool    only_update_channels;
    bool    channel_update_run;
};

#endif // _FILLDATA_H_
