#ifndef DATADIRECT_H_
#define DATADIRECT_H_

#include <QStringList>
#include <QDateTime>
#include <QXmlDefaultHandler>
#include <QMap>
#include <QNetworkReply>
#include <QAuthenticator>

#include "mythtvexp.h"

#include <vector>

using namespace std;

enum DD_PROVIDERS
{
    DD_ZAP2IT           = 0,
    DD_SCHEDULES_DIRECT = 1,
    DD_PROVIDER_COUNT   = 2,
};

class DataDirectURLs
{

public:
    DataDirectURLs(QString a, QString b, QString c, QString d) :
            m_name(a), m_webServiceURL(b), m_webURL(c), m_loginPage(d) {}

    QString m_name;
    QString m_webServiceURL;
    QString m_webURL;
    QString m_loginPage;
};

typedef vector<DataDirectURLs> DDProviders;

class DataDirectProcessor;

class DataDirectStation
{

public:
    DataDirectStation() = default;

    void Reset(void)
    {
        DataDirectStation tmp;
        *this = tmp;
    }

public:
    //                      field length   req/opt
    QString m_stationid;       //  12        required
    QString m_callsign;        //  10        required
    QString m_stationname;     //  40        required
    QString m_affiliate;       //  25        optional
    QString m_fccchannelnumber;//   8        optional
};

typedef DataDirectStation DDStation;

class DataDirectLineup
{

public:
    DataDirectLineup() = default;

    void Reset(void)
    {
        DataDirectLineup tmp;
        *this = tmp;
    }

public:
    //                      field length   req/opt
    QString m_lineupid;       //   12        required
    QString m_name;           //  100        required
    QString m_displayname;    //   50        ????????
    QString m_type;           //   20        required
    QString m_postal;         //    6        optional
    QString m_device;         //   30        optional
    QString m_location;       //   28        required unused
};

class DataDirectLineupMap
{

public:
    DataDirectLineupMap() = default;

    void Reset(void)
    {
        DataDirectLineupMap tmp;
        *this = tmp;
    }

public:
    //                      field length   req/opt
    QString m_lineupid;       //   12        required
    QString m_stationid;      //   12        required
    QString m_channel;        //    5        optional (major channel num)
    QString m_channelMinor;   //    3        optional
    QDate   m_mapFrom;        //    8  optional unused (date broadcasting begins)
    QDate   m_mapTo;          //    8  optional unused (date broadcasting ends)
};

class DataDirectSchedule
{

public:
    DataDirectSchedule() = default;

    void Reset(void)
    {
        DataDirectSchedule tmp;
        *this = tmp;
    }

public:
    QString   m_programid; // 12
    QString   m_stationid; // 12
    QDateTime m_time;
    QTime     m_duration;
    bool      m_repeat         {false};
    bool      m_isnew          {false};
    bool      m_stereo         {false};
    bool      m_dolby          {false};
    bool      m_subtitled      {false};
    bool      m_hdtv           {false};
    bool      m_closecaptioned {false};
    QString   m_tvrating;
    int       m_partnumber     {0};
    int       m_parttotal      {0};
};

class DataDirectProgram
{

public:
    DataDirectProgram() = default;

    void Reset(void)
    {
        DataDirectProgram tmp;
        *this = tmp;
    }

public:
    QString m_programid;   // 12
    QString m_seriesid;    // 12
    QString m_title;       // 120
    QString m_subtitle;    // 150
    QString m_description; // 255
    QString m_mpaaRating;  // 5
    QString m_starRating;  // 5
    QTime   m_duration;
    QString m_year;        // 4
    QString m_showtype;    // 30
    QString m_colorcode;   // 20
    QDate   m_originalAirDate; // 20
    QString m_syndicatedEpisodeNumber; // 20
    // advisories ?
};

class DataDirectProductionCrew
{

public:
    DataDirectProductionCrew() = default;

    void Reset(void)
    {
        DataDirectProductionCrew tmp;
        *this = tmp;
    }

public:
    QString m_programid; // 12
    QString m_role;      // 30
    QString m_givenname; // 20
    QString m_surname;   // 20
    QString m_fullname;  // 41
};

class DataDirectGenre
{

public:
    DataDirectGenre() = default;

    void Reset(void)
    {
        DataDirectGenre tmp;
        *this = tmp;
    }

public:
    QString m_programid; // 12
    QString m_gclass;    // 30
    QString m_relevance; // 1
};

class RawLineupChannel
{

public:
    RawLineupChannel() = default;
    RawLineupChannel(QString name,  QString id,
                     QString value, bool    checked,
                     QString ch,    QString callsign) :
            m_chk_name(name),       m_chk_id(id), m_chk_value(value),
            m_chk_checked(checked), m_lbl_ch(ch), m_lbl_callsign(callsign) {}

public:
    QString m_chk_name;
    QString m_chk_id;
    QString m_chk_value;
    bool    m_chk_checked {false};
    QString m_lbl_ch;
    QString m_lbl_callsign;
};

typedef vector<RawLineupChannel> RawLineupChannels;

class RawLineup
{

public:
    RawLineup() = default;

    RawLineup(QString a, QString b, QString c) :
            m_get_action(a), m_udl_id(b), m_zipcode(c) {}

public:
    QString           m_get_action;
    QString           m_set_action;
    QString           m_udl_id;
    QString           m_zipcode;

    RawLineupChannels m_channels;
};

typedef QMap<QString, RawLineup> RawLineupMap;

class PostItem
{

public:
    PostItem(QString k, QString v) : key(k), value(v) {}

    QString key;
    QString value;
};

typedef vector<PostItem> PostList;

class DDStructureParser: public QXmlDefaultHandler
{

public:
    explicit DDStructureParser(DataDirectProcessor& _ddparent) :
            m_parent(_ddparent) {}

    bool startElement(const QString &pnamespaceuri, const QString &plocalname,
                      const QString &pqname, const QXmlAttributes &pxmlatts) override; // QXmlDefaultHandler

    bool endElement(const QString &pnamespaceuri, const QString &plocalname,
                    const QString &pqname) override; // QXmlDefaultHandler

    bool characters(const QString &pchars) override; // QXmlDefaultHandler

    bool startDocument(void) override; // QXmlDefaultHandler
    bool endDocument(void) override; // QXmlDefaultHandler

private:
    DataDirectProcessor     &m_parent;

    QString                  m_currtagname;
    DataDirectStation        m_curr_station;
    DataDirectLineup         m_curr_lineup;
    DataDirectLineupMap      m_curr_lineupmap;
    DataDirectSchedule       m_curr_schedule;
    DataDirectProgram        m_curr_program;
    DataDirectProductionCrew m_curr_productioncrew;
    DataDirectGenre          m_curr_genre;
    QString                  m_lastprogramid;
};


typedef QMap<QString, DataDirectStation>    DDStationList; // stationid ->
typedef vector<DataDirectLineup>           DDLineupList;
typedef vector<DataDirectLineupMap>        DDLineupChannels;
typedef QMap<QString, DDLineupChannels>     DDLineupMap;  // lineupid ->

class MTV_PUBLIC DataDirectProcessor
{

    friend class DDStructureParser;
    friend void authenticationCallback(QNetworkReply*, QAuthenticator*, void*);

  public:
    DataDirectProcessor(uint listings_provider = DD_ZAP2IT,
                        QString userid = "", QString password = "");
    ~DataDirectProcessor();

    QString CreateTempDirectory(bool *ok = nullptr) const;

    // web service commands
    bool GrabData(const QDateTime &startdate, const QDateTime &enddate);
    bool GrabNextSuggestedTime(void);

    // utility wrappers
    bool GrabLineupsOnly(void);
    bool GrabAllData(void);

    // screen scraper commands
    bool GrabLoginCookiesAndLineups(bool parse_lineups = true);
    bool GrabLineupForModify(const QString &lineupid);
    bool SaveLineupChanges(const QString &lineupid);

    // combined commands
    bool GrabFullLineup(const QString &lineupid, bool restore = true,
                        bool onlyGrabSelected = false,
                        uint cache_age_allowed_in_seconds = 0);
    bool SaveLineup(const QString &lineupid,
                    const QMap<QString, bool> &xmltvids);
    bool UpdateListings(uint sourceid);

    // cache commands
    bool GrabLineupsFromCache(const QString &lineupid);
    bool SaveLineupToCache(const QString &lineupid) const;

    // gets
    DDStationList GetStations(void) const { return m_stations; }
    DDLineupList GetLineups(void) const { return m_lineups; }
    DDLineupMap GetLineupMap(void) const { return m_lineupmaps; }
    QDateTime GetLineupCacheAge(const QString &lineupid) const;

    QString GetUserID(void) const { return m_userid.toLower(); }
    QString GetPassword(void) const { return m_password; }
    uint GetListingsProvider(void) const { return m_listingsProvider; }

    QString   GetListingsProviderName(void) const
    {
        return m_providers[m_listingsProvider % DD_PROVIDER_COUNT].m_name;
    }

    QDateTime GetDDProgramsStartAt(void) const { return m_actualListingsFrom; }
    QDateTime GetDDProgramsEndAt(void) const { return m_actualListingsTo; }
    DDLineupChannels GetDDLineup(const QString &lineupid) const
    {
        return m_lineupmaps[lineupid];
    }

    DDStation GetDDStation(const QString &xmltvid) const
    {
        return m_stations[xmltvid];
    }

    QString   GetRawUDLID(const QString &lineupid) const;
    QString   GetRawZipCode(const QString &lineupid) const;
    RawLineup GetRawLineup(const QString &lineupid) const;

    // sets
    void SetUserID(const QString &uid) { m_userid = uid; }
    void SetPassword(const QString &pwd) { m_password = pwd; }
    void SetListingsProvider(uint i)
    {
        m_listingsProvider = i % DD_PROVIDER_COUNT;
    }

    void SetInputFile(const QString &file) { m_inputFilename = file; }
    void SetCacheData(bool cd) { m_cacheData = cd; }

    // static commands (these update temp DB tables)
    static void UpdateStationViewTable(QString lineupid);
    static void UpdateProgramViewTable(uint sourceid);

    // static commands (these update regular DB tables from temp DB tables)
    static int  UpdateChannelsSafe(
        uint sourceid, bool insert_channels, bool filter_new_channels);
    static bool UpdateChannelsUnsafe(
        uint sourceid, bool filter_new_channels);
    static void DataDirectProgramUpdate(void);

    QStringList GetFatalErrors(void) const { return m_fatalErrors; }

  private:
    void CreateTempTables(void);
    void CreateATempTable(const QString &ptablename,
                          const QString &ptablestruct);

    bool ParseLineups(const QString &documentFile);
    bool ParseLineup(const QString &lineupid, const QString &documentFile);

    void CreateTemp(const QString &templatefilename, const QString &errmsg,
                    bool directory, QString &filename, bool &ok) const;

    QString GetResultFilename(bool &ok) const;
    QString GetDDPFilename(bool &ok) const;
    QString GetCookieFilename(bool &ok) const;

    void SetAll(const QString &lineupid, bool val);
    void SetDDProgramsStartAt(QDateTime begts) { m_actualListingsFrom = begts; }
    void SetDDProgramsEndAt(QDateTime endts) { m_actualListingsTo = endts; }

    static bool Post(QString url, const PostList &list, QString documentFile,
                     QString inCookieFile, QString outCookieFile);

    bool DDPost(QString    url,          QString   &inputFilename,
                       QDateTime  pstartDate,   QDateTime  pendDate,
                       QString   &err_txt);


  private:
    void authenticationCallback(QNetworkReply *reply, QAuthenticator *auth);

    uint                m_listingsProvider;
    DDProviders         m_providers;

    QString             m_userid;
    QString             m_password;
    mutable QString     m_tmpDir    {"/tmp"};
    bool                m_cacheData {false};

    QDateTime           m_actualListingsFrom;
    QDateTime           m_actualListingsTo;

    QString             m_inputFilename;

    DDStationList       m_stations;
    DDLineupList        m_lineups;
    DDLineupMap         m_lineupmaps;

    RawLineupMap        m_rawLineups;
    mutable QString     m_tmpPostFile;
    mutable QString     m_tmpResultFile;
    mutable QString     m_tmpDDPFile;
    mutable QString     m_cookieFile;
    QDateTime           m_cookieFileDT;

    mutable QStringList m_fatalErrors;
};

#endif
