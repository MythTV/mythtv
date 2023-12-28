#ifndef FILLDATA_H
#define FILLDATA_H

// C++ headers
#include <vector>

// Qt headers
#include <QString>

// MythTV headers
#include "libmythtv/programdata.h"

// filldata headers
#include "channeldata.h"
#include "xmltvparser.h"

static constexpr int8_t REFRESH_MAX { 21 };

// helper functions to update mfdb status fields in settings
bool updateLastRunEnd();
bool updateLastRunStart();
bool updateLastRunStatus(QString &status);
bool updateNextScheduledRun();

struct DataSource
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
    bool    xmltvgrabber_apiconfig    {false};
    bool    xmltvgrabber_lineups      {false};
    QString xmltvgrabber_prefmethod;
};
using DataSourceList = std::vector<DataSource>;

class FillData
{
  public:
    FillData()
    {
        SetRefresh(1, true);
    }

    void SetRefresh(int day, bool set);

    bool GrabDataFromFile(int id, const QString &filename);
    bool GrabData(const DataSource& source, int offset);
    bool Run(DataSourceList &sourcelist);

    enum
    {
        kRefreshClear = 0xFFFF0,
        kRefreshAll   = 0xFFFF1,
    };

  public:
    ChannelData m_chanData;
    XMLTVParser m_xmltvParser;

    QString m_grabOptions;
    uint    m_maxDays                 {0};

    bool    m_interrupted             {false};
    bool    m_endOfData               {false};
    bool    m_refreshTba              {true};
    bool    m_needPostGrabProc        {true};
    bool    m_onlyUpdateChannels      {false};
    bool    m_channelUpdateRun        {false};
    bool    m_noAllAtOnce             {false};

  private:
    QMap<uint,bool>     m_refreshDay;
    bool                m_refreshAll  {false};
    mutable QStringList m_fatalErrors;
};

#endif // FILLDATA_H
