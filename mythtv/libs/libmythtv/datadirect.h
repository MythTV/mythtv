#ifndef DATADIRECT_H_
#define DATADIRECT_H_

#include <qstringlist.h>
#include <qdatetime.h>
#include <qxml.h>
#include <qmap.h>

#include <vector>
using namespace std;

enum DD_PROVIDERS
{
    DD_ZAP2IT = 0,
    DD_PROVIDER_COUNT,
};

class DataDirectURLs
{
  public:
    DataDirectURLs(QString a, QString b, QString c, QString d) :
        name(a), webServiceURL(b), webURL(c), loginPage(d) {}

    QString name;
    QString webServiceURL;
    QString webURL;
    QString loginPage;
};
typedef vector<DataDirectURLs> DDProviders;

class DataDirectProcessor;

class DataDirectStation
{
  public:
    DataDirectStation();

    void Reset(void) { DataDirectStation tmp; *this = tmp; }

  public:
    //                      field length   req/opt
    QString stationid;      //   12        required
    QString callsign;       //   10        required
    QString stationname;    //   40        required
    QString affiliate;      //   25        optional
    QString fccchannelnumber;//   8        optional
};
typedef DataDirectStation DDStation;

class DataDirectLineup
{
  public:
    DataDirectLineup();

    void Reset(void) { DataDirectLineup tmp; *this = tmp; }

  public:
    //                      field length   req/opt
    QString lineupid;       //   12        required
    QString name;           //  100        required
    QString displayname;    //   50        ????????
    QString type;           //   20        required
    QString postal;         //    6        optional
    QString device;         //   30        optional
    QString location;       //   28        required unused
};

class DataDirectLineupMap
{
  public:
    DataDirectLineupMap();

    void Reset(void) { DataDirectLineupMap tmp; *this = tmp; }

  public:
    //                      field length   req/opt
    QString lineupid;       //   12        required
    QString stationid;      //   12        required
    QString channel;        //    5        optional (major channel num)
    QString channelMinor;   //    3        optional
    QDate   mapFrom;        //    8  optional unused (date broadcasting begins)
    QDate   mapTo;          //    8  optional unused (date broadcasting ends)
};

class DataDirectSchedule
{
  public:
    DataDirectSchedule();

    void Reset(void) { DataDirectSchedule tmp; *this = tmp; }

  public:
    QString   programid; // 12
    QString   stationid; // 12
    QDateTime time;
    QTime     duration;
    bool      repeat;
    bool      stereo;
    bool      subtitled;
    bool      hdtv;
    bool      closecaptioned;
    QString   tvrating;
    int       partnumber;
    int       parttotal;
};

class DataDirectProgram
{
  public:
    DataDirectProgram();

    void Reset(void) { DataDirectProgram tmp; *this = tmp; }

  public:
    QString programid;   // 12
    QString seriesid;    // 12
    QString title;       // 120
    QString subtitle;    // 150
    QString description; // 255
    QString mpaaRating;  // 5
    QString starRating;  // 5
    QTime   duration;
    QString year;        // 4
    QString showtype;    // 30
    QString colorcode;   // 20
    QDate   originalAirDate; // 20
    QString syndicatedEpisodeNumber; // 20
    // advisories ?
};

class DataDirectProductionCrew
{
  public:
    DataDirectProductionCrew();

    void Reset(void) { DataDirectProductionCrew tmp; *this = tmp; }

  public:
    QString programid; // 12
    QString role;      // 30
    QString givenname; // 20
    QString surname;   // 20
    QString fullname;  // 41
};

class DataDirectGenre
{
  public:
    DataDirectGenre();

    void Reset(void) { DataDirectGenre tmp; *this = tmp; }

  public:
    QString programid; // 12
    QString gclass;    // 30
    QString relevance; // 1
};

class RawLineupChannel
{
  public:
    RawLineupChannel() :
        chk_name(QString::null),  chk_id(QString::null),
        chk_value(QString::null), chk_checked(false),
        lbl_ch(QString::null),    lbl_callsign(QString::null) {}

    RawLineupChannel(QString name,  QString id,
                     QString value, bool    checked,
                     QString ch,    QString callsign) :
        chk_name(name),       chk_id(id), chk_value(value),
        chk_checked(checked), lbl_ch(ch), lbl_callsign(callsign) {}

  public:
    QString chk_name;
    QString chk_id;
    QString chk_value;
    bool    chk_checked;
    QString lbl_ch;
    QString lbl_callsign;
};
typedef vector<RawLineupChannel> RawLineupChannels;

class RawLineup
{
  public:
    RawLineup() :
        get_action(QString::null), set_action(QString::null),
        udl_id(QString::null),     zipcode(QString::null) {}
    RawLineup(QString a, QString b, QString c) :
        get_action(a), set_action(QString::null), udl_id(b), zipcode(c) {}

  public:
    QString get_action;
    QString set_action;
    QString udl_id;
    QString zipcode;

    RawLineupChannels channels;
};
typedef QMap<QString,RawLineup> RawLineupMap;

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
    DDStructureParser(DataDirectProcessor& _ddparent): parent(_ddparent) {}

    bool startElement(const QString &pnamespaceuri, const QString &plocalname,
                      const QString &pqname, const QXmlAttributes &pxmlatts);

    bool endElement(const QString &pnamespaceuri, const QString &plocalname,
                    const QString &pqname);

    bool characters(const QString &pchars);

    bool startDocument(void);
    bool endDocument(void);

  private:
    DataDirectProcessor     &parent;

    QString                  currtagname;
    DataDirectStation        curr_station;
    DataDirectLineup         curr_lineup;
    DataDirectLineupMap      curr_lineupmap;
    DataDirectSchedule       curr_schedule;
    DataDirectProgram        curr_program;
    DataDirectProductionCrew curr_productioncrew;
    DataDirectGenre          curr_genre;
    QString                  lastprogramid;
};

typedef QMap<QString,DataDirectStation>    DDStationList; // stationid ->
typedef vector<DataDirectLineup>           DDLineupList;
typedef vector<DataDirectLineupMap>        DDLineupChannels;
typedef QMap<QString,DDLineupChannels>     DDLineupMap;   // lineupid ->

class DataDirectProcessor
{
    friend class DDStructureParser;
  public:
    DataDirectProcessor(uint listings_provider = DD_ZAP2IT,
                        QString userid = "", QString password = "");
   ~DataDirectProcessor();

    // web service commands
    bool GrabData(const QDateTime pstartdate, const QDateTime penddate);
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
                        uint cache_age_allowed_in_seconds = 0);
    bool SaveLineup(const QString &lineupid,
                    const QMap<QString,bool> &xmltvids);
    bool UpdateListings(uint sourceid);

    // cache commands
    bool GrabLineupsFromCache(const QString &lineupid);
    bool SaveLineupToCache(const QString &lineupid) const;

    // gets
    DDStationList GetStations(void)       const { return stations;           }
    DDLineupList  GetLineups(void)        const { return lineups;            }
    DDLineupMap   GetLineupMap(void)      const { return lineupmaps;         }
    QDateTime     GetLineupCacheAge(const QString &lineupid) const;

    QString   GetUserID(void)             const { return userid;             }
    QString   GetPassword(void)           const { return password;           }
    uint      GetListingsProvider(void)   const { return listings_provider;  }
    QString   GetListingsProviderName(void) const
        { return providers[listings_provider % DD_PROVIDER_COUNT].name; }

    QDateTime GetDDProgramsStartAt(bool localtime = false) const;
    QDateTime GetDDProgramsEndAt(bool localtime = false) const;
    DDLineupChannels GetDDLineup(const QString &lineupid) const
        { return lineupmaps[lineupid]; }
    DDStation GetDDStation( const QString &xmltvid ) const
        { return stations[xmltvid]; }

    QString   GetRawUDLID(  const QString &lineupid) const;
    QString   GetRawZipCode(const QString &lineupid) const;
    RawLineup GetRawLineup( const QString &lineupid) const;

    // sets
    void SetUserID(QString uid)                 { userid             = uid;  }
    void SetPassword(QString pwd)               { password           = pwd;  }
    void SetListingsProvider(uint i)
        { listings_provider = i % DD_PROVIDER_COUNT; }
    void SetInputFile(const QString &file)      { inputfilename      = file; }

    // static commands (these update temp DB tables)
    static void UpdateStationViewTable(QString lineupid);
    static void UpdateProgramViewTable(uint sourceid);

    // static commands (these update regular DB tables from temp DB tables)
    static int  UpdateChannelsSafe(uint sourceid, bool insert_channels);
    static bool UpdateChannelsUnsafe(uint sourceid);

  private:
    void CreateTempTables(void);
    void CreateATempTable(const QString &ptablename,
                          const QString &ptablestruct);

    bool ParseLineups(const QString &documentFile);
    bool ParseLineup(const QString &lineupid, const QString &documentFile);

    void SetAll(const QString &lineupid, bool val);
    void SetDDProgramsStartAt(QDateTime begts)  { actuallistingsfrom = begts; }
    void SetDDProgramsEndAt(QDateTime endts)    { actuallistingsto   = endts; }

    static bool Post(QString url, const PostList &list, QString documentFile,
                     QString inCookieFile, QString outCookieFile);

    static FILE *DDPost(QString    url,
                        QString    postFilename, QString    inputFilename,
                        QString    userid,       QString    password,
                        QDateTime  pstartDate,   QDateTime  pendDate,
                        QString   &err_txt);


  private:
    uint          listings_provider;
    DDProviders   providers;

    QString       userid;
    QString       password;

    QDateTime     actuallistingsfrom;
    QDateTime     actuallistingsto;

    QString       inputfilename;

    DDStationList stations;
    DDLineupList  lineups;
    DDLineupMap   lineupmaps;

    RawLineupMap  rawlineups;
    QString       tmpPostFile;
    QString       tmpResultFile;
    QString       cookieFile;
    QDateTime     cookieFileDT;
};

#endif
