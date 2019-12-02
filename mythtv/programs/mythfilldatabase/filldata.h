#ifndef _FILLDATA_H_
#define _FILLDATA_H_

// C++ headers
#include <vector>
using namespace std;

// Qt headers
#include <QString>

// libmythtv headers
#include "programdata.h"

// filldata headers
#include "channeldata.h"
#include "xmltvparser.h"

#define REFRESH_MAX 21

// helper functions to update mfdb status fields in settings
bool updateLastRunEnd();
bool updateLastRunStart();
bool updateLastRunStatus(QString &status);
bool updateNextScheduledRun();

struct Source
{
    int     id                        {0};
    QString name;
    QString xmltvgrabber;
    QString userid;
    QString password;
    QString lineupid;
    bool    xmltvgrabber_baseline     {false};
    bool    xmltvgrabber_manualconfig {false};
    bool    xmltvgrabber_cache        {false};
    QString xmltvgrabber_prefmethod;
    vector<int> dd_dups;
};
using SourceList = vector<Source>;

class FillData
{
  public:
    FillData()
    {
        SetRefresh(1, true);
    }

    void SetRefresh(int day, bool set);

    bool GrabDataFromFile(int id, QString &filename);
    bool GrabData(const Source& source, int offset);
    bool Run(SourceList &sourcelist);

    enum
    {
        kRefreshClear = 0xFFFF0,
        kRefreshAll   = 0xFFFF1,
    };

  public:
    ProgramData         m_prog_data;
    ChannelData         m_chan_data;
    XMLTVParser         m_xmltv_parser;

    QString m_logged_in;
    QString m_lastdduserid;
    QString m_graboptions;
    int     m_raw_lineup              {0};
    uint    m_maxDays                 {0};

    bool    m_interrupted             {false};
    bool    m_endofdata               {false};
    bool    m_refresh_tba             {true};
    bool    m_dd_grab_all             {false};
    bool    m_dddataretrieved         {false};
    bool    m_need_post_grab_proc     {true};
    bool    m_only_update_channels    {false};
    bool    m_channel_update_run      {false};
    bool    m_no_allatonce            {false};

  private:
    QMap<uint,bool>     m_refresh_day;
    bool                m_refresh_all {false};
    mutable QStringList m_fatalErrors;
};

#endif // _FILLDATA_H_
