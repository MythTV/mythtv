#ifndef DATADIRECT_H_
#define DATADIRECT_H_

#include <qstringlist.h>
#include <qdatetime.h>
#include <qxml.h>
#include <qsqldatabase.h>
#include <qsqlquery.h>

enum DD_PROVIDERS
{
    DD_ZAP2IT = 0,
};

class DataDirectProcessor;

class DataDirectStation
{
  public:
    DataDirectStation();

    void Reset(void);

  public:
    QString stationid;        // 12
    QString callsign;         // 10
    QString stationname;      // 40
    QString affiliate;        // 25
    QString fccchannelnumber; // 8
};

class DataDirectLineup
{
  public:
    DataDirectLineup();

    void Reset(void);

  public:
    QString lineupid;
    QString name;
    QString displayname;
    QString type;
    QString postal;
    QString device;
};

class DataDirectLineupMap
{
  public:
    DataDirectLineupMap();

    void Reset(void);

  public:
    QString lineupid;     // 100
    QString stationid;    // 12
    QString channel;      // 5
    QString channelMinor; // 3
// QDate mapFrom;
// QDate mapTo;
// QDate onAirFrom;
// QDate onAirTo;
};

class DataDirectSchedule
{
  public:
    DataDirectSchedule();

    void Reset(void);

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

    void Reset(void);

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

    void Reset(void);

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

    void Reset(void);

  public:
    QString programid; // 12
    QString gclass;    // 30
    QString relevance; // 1
};

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

typedef QValueList<DataDirectStation>   DDStationList;
typedef QValueList<DataDirectLineup>    DDLineupList;
typedef QValueList<DataDirectLineupMap> DDLineupMap;

class DataDirectProcessor
{
    friend class DDStructureParser;
  public:
    DataDirectProcessor(int listings_provider = DD_ZAP2IT);

    // commands
    bool grabLineupsOnly(void);
    bool grabData(bool plineupsonly, QDateTime pstartdate, QDateTime penddate);
    bool grabAllData(void);
    void updateStationViewTable(void);
    void updateProgramViewTable(int sourceid);

    // gets
    DDStationList getStations(void)       const { return stations;           }
    DDLineupList  getLineups(void)        const { return lineups;            }
    DDLineupMap   getLineupMaps(void)     const { return lineupmaps;         }

    QString       getUserID(void)         const { return userid;             }
    QString       getPassword(void)       const { return password;           }
    QString       getLineup(void)         const { return selectedlineupid;   }
    int           getSource(void)         const { return source;             }
    QDateTime getActualListingsFrom(void) const { return actuallistingsfrom; }
    QDateTime getActualListingsTo(void)   const { return actuallistingsto;   }

    // non-const gets
    bool getNextSuggestedTime(void);

    // sets
    void setUserID(QString uid)                 { userid             = uid;  }
    void setPassword(QString pwd)               { password           = pwd;  }
    void setLineup(QString lid)                 { selectedlineupid   = lid;  }
    void setSource(int src)                     { source             = src;  }
    void setActualListingsFrom(QDateTime palf)  { actuallistingsfrom = palf; }
    void setActualListingsTo(QDateTime palt)    { actuallistingsto   = palt; }
    void setInputFile(const QString &filename);

  private:
    void createTempTables(void);
    void createATempTable(const QString &ptablename,
                          const QString &ptablestruct);

    FILE *getInputFile(bool plineupsOnly,
                       QDateTime  pstartDate, QDateTime  pendDate,
                       QString   &err_txt,    QString   &tmpfilename);

  private:
    int source;

    QString selectedlineupid;
    QString userid;
    QString password;

    QString lastrunuserid;
    QString lastrunpassword;
    QDateTime actuallistingsfrom;
    QDateTime actuallistingsto;

    QString inputfilename;

    DDStationList stations;
    DDLineupList  lineups;
    DDLineupMap   lineupmaps;

};

#endif
