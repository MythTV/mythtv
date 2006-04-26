#include <unistd.h>

#include <qfile.h>
#include <qstring.h>
#include <qregexp.h>
#include <qfileinfo.h>

#include "datadirect.h"
#include "sourceutil.h"
#include "channelutil.h"
#include "frequencytables.h"
#include "mythwidgets.h"
#include "mythcontext.h"
#include "mythdbcon.h"
#include "util.h"

#define SHOW_WGET_OUTPUT 0

#define LOC QString("DataDirect: ")
#define LOC_ERR QString("DataDirect, Error: ")

static QString get_setting(QString line, QString key);
static bool    has_setting(QString line, QString key);
static QString html_escape(QString str);
static void    get_atsc_stuff(QString channum, int sourceid, int freqid,
                              int &major, int &minor, long long &freq);
static uint    create_atscsrcid(QString chan_major, QString chan_minor);
static QString process_dd_station(uint sourceid,
                                  QString  chan_major, QString  chan_minor,
                                  QString &tvformat,   uint    &freqid,
                                  uint    &atscsrcid);
static void    update_channel_basic(uint    sourceid,   bool    insert,
                                    QString xmltvid,    QString callsign,
                                    QString name,       uint    freqid,
                                    QString chan_major, QString chan_minor);

DataDirectStation::DataDirectStation(void) :
    stationid(""),              callsign(""),
    stationname(""),            affiliate(""),
    fccchannelnumber("")
{
}

DataDirectLineup::DataDirectLineup() :
    lineupid(""), name(""), displayname(""), type(""), postal(""), device("")
{
}   

DataDirectLineupMap::DataDirectLineupMap() :
    lineupid(""), stationid(""), channel(""), channelMinor("")
{
}

DataDirectSchedule::DataDirectSchedule() :
    programid(""),              stationid(""),
    time(QDateTime()),          duration(QTime()),
    repeat(false),              stereo(false),
    subtitled(false),           hdtv(false),
    closecaptioned(false),      tvrating(""),
    partnumber(0),              parttotal(0)
{
}

DataDirectProgram::DataDirectProgram() :
    programid(""),  seriesid(""),      title(""),
    subtitle(""),   description(""),   mpaaRating(""),
    starRating(""), duration(QTime()), year(""),
    showtype(""),   colorcode(""),     originalAirDate(QDate()),
    syndicatedEpisodeNumber("")
{
}

DataDirectProductionCrew::DataDirectProductionCrew() :
    programid(""), role(""), givenname(""), surname(""), fullname("")
{
}

DataDirectGenre::DataDirectGenre() :
    programid(""), gclass(""), relevance("")
{
}

// XXX Program duration should be stored as seconds, not as a QTime.
//     limited to 24 hours this way.

bool DDStructureParser::startElement(const QString &pnamespaceuri, 
                                     const QString &plocalname, 
                                     const QString &pqname,
                                     const QXmlAttributes &pxmlatts) 
{
    (void)pnamespaceuri;
    (void)plocalname;

    currtagname = pqname;
    if (currtagname == "xtvd") 
    {
        QString   beg   = pxmlatts.value("from");
        QDateTime begts = QDateTime::fromString(beg, Qt::ISODate);
        parent.SetDDProgramsStartAt(begts);

        QString   end   = pxmlatts.value("to");
        QDateTime endts = QDateTime::fromString(end, Qt::ISODate);
        parent.SetDDProgramsEndAt(endts);
    }   
    else if (currtagname == "station") 
    {
        curr_station.Reset();
        curr_station.stationid = pxmlatts.value("id");
    }
    else if (currtagname == "lineup") 
    {
        curr_lineup.Reset();
        curr_lineup.name = pxmlatts.value("name");
        curr_lineup.type = pxmlatts.value("type");
        curr_lineup.device = pxmlatts.value("device");
        curr_lineup.postal = pxmlatts.value("postalCode");
        curr_lineup.lineupid = pxmlatts.value("id");
        curr_lineup.displayname = curr_lineup.name + "-" + curr_lineup.type + 
            "-" + curr_lineup.device + "-" + 
            curr_lineup.postal + "-" + 
            curr_lineup.lineupid;

        if (curr_lineup.lineupid.isEmpty()) 
        {
            curr_lineup.lineupid = curr_lineup.name + curr_lineup.postal + 
                curr_lineup.device + curr_lineup.type;
        }
    }
    else if (currtagname == "map") 
    {
        int tmpindex;
        curr_lineupmap.Reset();
        curr_lineupmap.lineupid = curr_lineup.lineupid;
        curr_lineupmap.stationid = pxmlatts.value("station");
        curr_lineupmap.channel = pxmlatts.value("channel");
        tmpindex = pxmlatts.index("channelMinor"); // for ATSC
        if (tmpindex != -1) 
            curr_lineupmap.channelMinor = pxmlatts.value(tmpindex);
    } 
    else if (currtagname == "schedule") 
    {
        curr_schedule.Reset();
        curr_schedule.programid = pxmlatts.value("program");
        curr_schedule.stationid = pxmlatts.value("station");

        QString timestr = pxmlatts.value("time");
        QDateTime UTCdt = QDateTime::fromString(timestr, Qt::ISODate);

        curr_schedule.time = MythUTCToLocal(UTCdt);
        QString durstr;

        durstr = pxmlatts.value("duration");
        curr_schedule.duration = QTime(durstr.mid(2, 2).toInt(), 
                                       durstr.mid(5, 2).toInt(), 0, 0);

        curr_schedule.repeat = (pxmlatts.value("repeat") == "true");
        curr_schedule.stereo = (pxmlatts.value("stereo") == "true");
        curr_schedule.subtitled = (pxmlatts.value("subtitled") == "true");
        curr_schedule.hdtv = (pxmlatts.value("hdtv") == "true");
        curr_schedule.closecaptioned = (pxmlatts.value("closeCaptioned") == 
                                        "true");
        curr_schedule.tvrating = pxmlatts.value("tvRating");
    }
    else if (currtagname == "part") 
    {
        curr_schedule.partnumber = pxmlatts.value("number").toInt();
        curr_schedule.parttotal = pxmlatts.value("total").toInt();
    }
    else if (currtagname == "program") 
    {
        curr_program.Reset();
        curr_program.programid = pxmlatts.value("id");
    }
    else if (currtagname == "crew") 
    {
        curr_program.Reset();
        lastprogramid = pxmlatts.value("program");
    }
    else if (currtagname == "programGenre") 
    {
        curr_genre.Reset();
        lastprogramid = pxmlatts.value("program");
    }        

    return true;
}                      

bool DDStructureParser::endElement(const QString &pnamespaceuri, 
                                   const QString &plocalname, 
                                   const QString &pqname) 
{
    (void)pnamespaceuri;
    (void)plocalname;

    MSqlQuery query(MSqlQuery::DDCon());

    if (pqname == "station") 
    {
        parent.stations[curr_station.stationid] = curr_station;

        query.prepare(
            "INSERT INTO dd_station "
            "     ( stationid,  callsign,  stationname, "
            "       affiliate,  fccchannelnumber)       "
            "VALUES "
            "     (:STATIONID, :CALLSIGN, :STATIONNAME, "
            "      :AFFILIATE, :FCCCHANNUM)");

        query.bindValue(":STATIONID",   curr_station.stationid);
        query.bindValue(":CALLSIGN",    curr_station.callsign);
        query.bindValue(":STATIONNAME", curr_station.stationname);
        query.bindValue(":AFFILIATE",   curr_station.affiliate);
        query.bindValue(":FCCCHANNUM",  curr_station.fccchannelnumber);

        if (!query.exec())
            MythContext::DBError("Inserting into dd_station", query);
    }
    else if (pqname == "lineup")
    {
        parent.lineups.push_back(curr_lineup);

        query.prepare(
            "INSERT INTO dd_lineup "
            "     ( lineupid,  name,  type,  device,  postal) "
            "VALUES "
            "     (:LINEUPID, :NAME, :TYPE, :DEVICE, :POSTAL)");

        query.bindValue(":LINEUPID",    curr_lineup.lineupid);
        query.bindValue(":NAME",        curr_lineup.name);
        query.bindValue(":TYPE",        curr_lineup.type);
        query.bindValue(":DEVICE",      curr_lineup.device);
        query.bindValue(":POSTAL",      curr_lineup.postal);

        if (!query.exec())
            MythContext::DBError("Inserting into dd_lineup", query);
    }
    else if (pqname == "map") 
    {
        parent.lineupmaps[curr_lineupmap.lineupid].push_back(curr_lineupmap);

        query.prepare(
            "INSERT INTO dd_lineupmap "
            "     ( lineupid,  stationid,  channel,  channelMinor) "
            "VALUES "
            "     (:LINEUPID, :STATIONID, :CHANNEL, :CHANNELMINOR)");

        query.bindValue(":LINEUPID",    curr_lineupmap.lineupid);
        query.bindValue(":STATIONID",   curr_lineupmap.stationid);
        query.bindValue(":CHANNEL",     curr_lineupmap.channel);
        query.bindValue(":CHANNELMINOR",curr_lineupmap.channelMinor);
        if (!query.exec())
            MythContext::DBError("Inserting into dd_lineupmap", query);
    }
    else if (pqname == "schedule") 
    {
        QDateTime endtime = curr_schedule.time.addSecs(
            QTime().secsTo(curr_schedule.duration));

        query.prepare(
            "INSERT INTO dd_schedule "
            "     ( programid,  stationid,   scheduletime,   "
            "       duration,   isrepeat,    stereo,         "
            "       subtitled,  hdtv,        closecaptioned, "
            "       tvrating,   partnumber,  parttotal,      "
            "       endtime) "
            "VALUES "
            "     (:PROGRAMID, :STATIONID,  :TIME,           "
            "      :DURATION,  :ISREPEAT,   :STEREO,         "
            "      :SUBTITLED, :HDTV,       :CAPTIONED,      "
            "      :TVRATING,  :PARTNUMBER, :PARTTOTAL,      "
            "      :ENDTIME)");

        query.bindValue(":PROGRAMID",   curr_schedule.programid);
        query.bindValue(":STATIONID",   curr_schedule.stationid);
        query.bindValue(":TIME",        curr_schedule.time);
        query.bindValue(":DURATION",    curr_schedule.duration);
        query.bindValue(":ISREPEAT",    curr_schedule.repeat);
        query.bindValue(":STEREO",      curr_schedule.stereo);
        query.bindValue(":SUBTITLED",   curr_schedule.subtitled);
        query.bindValue(":HDTV",        curr_schedule.hdtv);
        query.bindValue(":CAPTIONED",   curr_schedule.closecaptioned);
        query.bindValue(":TVRATING",    curr_schedule.tvrating);
        query.bindValue(":PARTNUMBER",  curr_schedule.partnumber);
        query.bindValue(":PARTTOTAL",   curr_schedule.parttotal);
        query.bindValue(":ENDTIME",     endtime);

        if (!query.exec())
            MythContext::DBError("Inserting into dd_schedule", query);
    }
    else if (pqname == "program") 
    {
        float staravg = 0.0;
        if (!curr_program.starRating.isEmpty()) 
        {
            int fullstarcount = curr_program.starRating.contains("*");
            int halfstarcount = curr_program.starRating.contains("+");
            staravg = (fullstarcount + (halfstarcount * .5)) / 4;
        }

        QString cat_type = "";
        QString prefix = curr_program.programid.left(2);

        if (prefix == "MV")
            cat_type = "movie";
        else if (prefix == "SP")
            cat_type = "sports";
        else if (prefix == "EP" ||
                 curr_program.showtype.contains("series", false))
            cat_type = "series";
        else
            cat_type = "tvshow";

        query.prepare(
            "INSERT INTO dd_program "
            "     ( programid,    title,       subtitle,       "
            "       description,  showtype,    category_type,  "
            "       mpaarating,   starrating,  stars,          "
            "       runtime,      year,        seriesid,       "
            "       colorcode,    syndicatedepisodenumber, originalairdate) "
            "VALUES "
            "     (:PROGRAMID,   :TITLE,      :SUBTITLE,       "
            "      :DESCRIPTION, :SHOWTYPE,   :CATTYPE,        "
            "      :MPAARATING,  :STARRATING, :STARS,          "
            "      :RUNTIME,     :YEAR,       :SERIESID,       "
            "      :COLORCODE,   :SYNDNUM,    :ORIGAIRDATE)    ");

        query.bindValue(":PROGRAMID",   curr_program.programid);
        query.bindValue(":TITLE",       curr_program.title.utf8());
        query.bindValue(":SUBTITLE",    curr_program.subtitle.utf8());
        query.bindValue(":DESCRIPTION", curr_program.description.utf8());
        query.bindValue(":SHOWTYPE",    curr_program.showtype.utf8()); 
        query.bindValue(":CATTYPE",     cat_type);
        query.bindValue(":MPAARATING",  curr_program.mpaaRating);
        query.bindValue(":STARRATING",  curr_program.starRating);
        query.bindValue(":STARS",       staravg);
        query.bindValue(":RUNTIME",     curr_program.duration);
        query.bindValue(":YEAR",        curr_program.year);
        query.bindValue(":SERIESID",    curr_program.seriesid);
        query.bindValue(":COLORCODE",   curr_program.colorcode);
        query.bindValue(":SYNDNUM",     curr_program.syndicatedEpisodeNumber);
        query.bindValue(":ORIGAIRDATE", curr_program.originalAirDate);

        if (!query.exec())
            MythContext::DBError("Inserting into dd_program", query);
    }    
    else if (pqname == "member") 
    {
        QString roleunderlines = curr_productioncrew.role.replace(" ", "_");

        QString fullname = curr_productioncrew.givenname;
        if (!fullname.isEmpty())
            fullname += " ";
        fullname += curr_productioncrew.surname;

        query.prepare(
            "INSERT INTO dd_productioncrew "
            "       ( programid,  role,  givenname,  surname,  fullname) "
            "VALUES (:PROGRAMID, :ROLE, :GIVENNAME, :SURNAME, :FULLNAME)");

        query.bindValue(":PROGRAMID",   lastprogramid);
        query.bindValue(":ROLE",        roleunderlines.utf8());
        query.bindValue(":GIVENNAME",   curr_productioncrew.givenname.utf8());
        query.bindValue(":SURNAME",     curr_productioncrew.surname.utf8());
        query.bindValue(":FULLNAME",    fullname.utf8());

        if (!query.exec())
            MythContext::DBError("Inserting into dd_productioncrew", query);
    }    
    else if (pqname == "genre") 
    {
        query.prepare(
            "INSERT INTO dd_genre "
            "       ( programid,  class,  relevance) "
            "VALUES (:PROGRAMID, :CLASS, :RELEVANCE)");

        query.bindValue(":PROGRAMID",   lastprogramid);
        query.bindValue(":CLASS",       curr_genre.gclass.utf8());
        query.bindValue(":RELEVANCE",   curr_genre.relevance);

        if (!query.exec())
            MythContext::DBError("Inserting into dd_genre", query);
    }

    return true;
} 
 
bool DDStructureParser::startDocument() 
{
    parent.CreateTempTables();
    return true;
}

bool DDStructureParser::endDocument() 
{
    return true;
}
 
bool DDStructureParser::characters(const QString& pchars) 
{
    // cerr << "Characters : " << pchars << "\n";
    if (pchars.stripWhiteSpace().isEmpty())
        return true;

    if (currtagname == "message")
    {
        if (pchars.contains("expire"))
        {
            QString ExtractDateFromMessage = pchars.right(20);
            QDateTime EDFM = QDateTime::fromString(ExtractDateFromMessage,
                                                   Qt::ISODate);
            QString ExpirationDate = EDFM.toString(Qt::LocalDate);
            QString ExpirationDateMessage = "Your subscription expires on " +
                ExpirationDate;

            QDateTime curTime = QDateTime::currentDateTime();
            if (curTime.daysTo(EDFM) <= 5)
            {
                VERBOSE(VB_IMPORTANT, LOC + QString("WARNING: ") +
                        ExpirationDateMessage);
            }
            else
            {
                VERBOSE(VB_IMPORTANT, LOC + ExpirationDateMessage);
            }

            MSqlQuery query(MSqlQuery::DDCon());

            QString querystr = QString(
                "UPDATE settings "
                "SET data ='%1' "
                "WHERE value='DataDirectMessage'")
                .arg(ExpirationDateMessage);

            query.prepare(querystr);

            if (!query.exec())
            {
                MythContext::DBError("Updating DataDirect Status Message",
                                     query);
            }
        }
    }
    if (currtagname == "callSign") 
        curr_station.callsign = pchars;
    else if (currtagname == "name")
        curr_station.stationname = pchars;
    else if (currtagname == "affiliate")
        curr_station.affiliate = pchars;
    else if (currtagname == "fccChannelNumber")
        curr_station.fccchannelnumber = pchars;
    else if (currtagname == "title")
        curr_program.title = pchars;
    else if (currtagname == "subtitle")
        curr_program.subtitle = pchars;
    else if (currtagname == "description")
        curr_program.description = pchars;
    else if (currtagname == "showType")
        curr_program.showtype = pchars;
    else if (currtagname == "series")
        curr_program.seriesid = pchars;
    else if (currtagname == "colorCode")
        curr_program.colorcode = pchars;
    else if (currtagname == "mpaaRating")
        curr_program.mpaaRating = pchars;
    else if (currtagname == "starRating")
        curr_program.starRating = pchars;
    else if (currtagname == "year")
        curr_program.year = pchars;
    else if (currtagname == "syndicatedEpisodeNumber")
        curr_program.syndicatedEpisodeNumber = pchars; 
    else if (currtagname == "runTime") 
    {
        QString runtimestr = pchars;
        QTime runtime = QTime(runtimestr.mid(2,2).toInt(),
                              runtimestr.mid(5,2).toInt(), 0, 0);
        curr_program.duration = runtime;
    }
    else if (currtagname == "originalAirDate") 
    {
        QDate airdate = QDate::fromString(pchars, Qt::ISODate);
        curr_program.originalAirDate = airdate;
    }
    else if (currtagname == "role")
        curr_productioncrew.role = pchars;
    else if (currtagname == "givenname")
        curr_productioncrew.givenname = pchars;
    else if (currtagname == "surname")
        curr_productioncrew.surname = pchars;  
    else if (currtagname == "class") 
        curr_genre.gclass = pchars;
    else if (currtagname == "relevance") 
        curr_genre.relevance = pchars;

    return true;
}

static QString makeTempFile(QString name_template)
{
    const char *tmp = name_template.ascii();
    char *ctemplate = strdup(tmp);
    int ret = mkstemp(ctemplate);
    QString tmpFileName(ctemplate);
    free(ctemplate);

    if (ret == -1)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Creating temp file from " +
                QString("template '%1'").arg(name_template) + ENO);
        return name_template;
    }
    close(ret);

    return tmpFileName;
}

DataDirectProcessor::DataDirectProcessor(uint lp, QString user, QString pass) :
    listings_provider(lp % DD_PROVIDER_COUNT),
    userid(user),               password(pass),
    inputfilename(""),          tmpPostFile(""),
    tmpResultFile(""),          cookieFile("")
{
    DataDirectURLs urls0(
        "Tribune Media Zap2It",
        "http://datadirect.webservices.zap2it.com/tvlistings/xtvdService",
        "http://labs.zap2it.com",
        "/ztvws/ztvws_login/1,1059,TMS01-1,00.html");
    providers.push_back(urls0);

    QString tmpDir = "/tmp";
    tmpPostFile   = makeTempFile(tmpDir + "/mythtv_post_XXXXXX");
    tmpResultFile = makeTempFile(tmpDir + "/mythtv_result_XXXXXX");
    cookieFile    = makeTempFile(tmpDir + "/mythtv_cookies_XXXXXX");
}

DataDirectProcessor::~DataDirectProcessor()
{
    unlink(tmpPostFile.ascii());
    unlink(tmpResultFile.ascii());
    unlink(cookieFile.ascii());
}

void DataDirectProcessor::UpdateStationViewTable(QString lineupid)
{
    MSqlQuery query(MSqlQuery::DDCon());
   
    if (!query.exec("TRUNCATE TABLE dd_v_station;")) 
        MythContext::DBError("Truncating temporary table dd_v_station", query);

    query.prepare(
        "INSERT INTO dd_v_station "
        "     ( stationid,            callsign,         stationname, "
        "       affiliate,            fccchannelnumber, channel,     "
        "       channelMinor) "
        "SELECT dd_station.stationid, callsign,         stationname, "
        "       affiliate,            fccchannelnumber, channel,     "
        "       channelMinor "
        "FROM dd_station, dd_lineupmap "
        "WHERE ((dd_station.stationid  = dd_lineupmap.stationid) AND "
        "       (dd_lineupmap.lineupid = :LINEUP))");

    query.bindValue(":LINEUP", lineupid);

    if (!query.exec())
        MythContext::DBError("Populating temporary table dd_v_station", query);
}

void DataDirectProcessor::UpdateProgramViewTable(uint sourceid)
{
    MSqlQuery query(MSqlQuery::DDCon());
   
    if (!query.exec("TRUNCATE TABLE dd_v_program;"))
        MythContext::DBError("Truncating temporary table dd_v_program", query);

    query.prepare(
        "INSERT INTO dd_v_program "
        "     ( chanid,         starttime,       endtime,         "
        "       title,          subtitle,        description,     "
        "       airdate,        stars,           previouslyshown, "
        "       stereo,         subtitled,       hdtv,            "
        "       closecaptioned, partnumber,      parttotal,       "
        "       seriesid,       originalairdate, showtype,        "
        "       category_type,  colorcode,       syndicatedepisodenumber, "
        "       tvrating,       mpaarating,      programid )      "
        "SELECT chanid,         scheduletime,    endtime,         "
        "       title,          subtitle,        description,     "
        "       year,           stars,           isrepeat,        "
        "       stereo,         subtitled,       hdtv,            "
        "       closecaptioned, partnumber,      parttotal,       "
        "       seriesid,       originalairdate, showtype,        "
        "       category_type,  colorcode,       syndicatedepisodenumber, "
        "       tvrating,       mpaarating,      dd_program.programid "
        "FROM channel, dd_schedule, dd_program "
        "WHERE ((dd_schedule.programid = dd_program.programid)  AND "
        "       (channel.xmltvid       = dd_schedule.stationid) AND "
        "       (channel.sourceid      = :SOURCEID))");

    query.bindValue(":SOURCEID", sourceid);

    if (!query.exec())
        MythContext::DBError("Populating temporary table dd_v_program", query);

    if (!query.exec("ANALYZE TABLE dd_v_program;"))
        MythContext::DBError("Analyzing table dd_v_program", query);

    if (!query.exec("ANALYZE TABLE dd_productioncrew;"))
        MythContext::DBError("Analyzing table dd_productioncrew", query);
}

bool DataDirectProcessor::UpdateChannelsSafe(uint sourceid,
                                             bool insert_channels)
{
    // Find all the channels in the dd_v_station temp table
    // where there is no channel with the same xmltvid in the
    // DB using the same source.
    MSqlQuery query(MSqlQuery::DDCon());
    query.prepare(
        "SELECT dd_v_station.stationid,   dd_v_station.callsign,         "
        "       dd_v_station.stationname, dd_v_station.fccchannelnumber, "
        "       dd_v_station.channel,     dd_v_station.channelMinor      "
        "FROM dd_v_station LEFT JOIN channel ON "
        "     dd_v_station.stationid = channel.xmltvid AND "
        "     channel.sourceid = :SOURCEID "
        "WHERE channel.chanid IS NULL");
    query.bindValue(":SOURCEID", sourceid);

    if (!query.exec())
    {
        MythContext::DBError("Selecting new channels", query);
        return false;
    }

    while (query.next())
    {
        QString xmltvid    = query.value(0).toString();
        QString callsign   = query.value(1).toString();
        QString name       = query.value(2).toString();
        uint    freqid     = query.value(3).toUInt();
        QString chan_major = query.value(4).toString();
        QString chan_minor = query.value(5).toString();

        update_channel_basic(sourceid, insert_channels,
                             xmltvid, callsign, name, freqid,
                             chan_major, chan_minor);
    }

    return true;
}

bool DataDirectProcessor::UpdateChannelsUnsafe(uint sourceid)
{
    MSqlQuery dd_station_info(MSqlQuery::DDCon());
    dd_station_info.prepare(
        "SELECT callsign,         stationname, stationid,"
        "       fccchannelnumber, channel,     channelMinor "
        "FROM dd_v_station");
    if (!dd_station_info.exec())
        return false;

    if (dd_station_info.size() == 0)
        return true;

    MSqlQuery chan_update_q(MSqlQuery::DDCon());
    chan_update_q.prepare(
        "UPDATE channel "
        "SET callsign  = :CALLSIGN,  name   = :NAME, "
        "    channum   = :CHANNUM,   freqid = :FREQID, "
        "    atscsrcid = :ATSCSRCID "
        "WHERE xmltvid = :STATIONID AND sourceid = :SOURCEID");

    while (dd_station_info.next())        
    {
        uint    atscsrcid  = 0;
        uint    freqid     = dd_station_info.value(3).toUInt();
        QString chan_major = dd_station_info.value(4).toString();
        QString chan_minor = dd_station_info.value(5).toString();
        QString tvformat   = QString::null;
        QString channum    = process_dd_station(
            sourceid, chan_major, chan_minor, tvformat, freqid, atscsrcid);

        chan_update_q.bindValue(":CALLSIGN",  dd_station_info.value(0));
        chan_update_q.bindValue(":NAME",      dd_station_info.value(1));
        chan_update_q.bindValue(":STATIONID", dd_station_info.value(2));
        chan_update_q.bindValue(":CHANNUM",   channum);
        chan_update_q.bindValue(":SOURCEID",  sourceid);
        chan_update_q.bindValue(":FREQID",    freqid);
        chan_update_q.bindValue(":ATSCSRCID", atscsrcid);

        if (!chan_update_q.exec())
        {
            MythContext::DBError("Updating channel table",
                                 chan_update_q.lastQuery());
        }
    }

    return true;
}

FILE *DataDirectProcessor::DDPost(
    QString    ddurl,
    QString    postFilename, QString    inputFile,
    QString    userid,       QString    password,
    QDateTime  pstartDate,   QDateTime  pendDate,
    QString   &err_txt)
{
    if (!inputFile.isEmpty())
    {
        err_txt = QString("Unable to open '%1'").arg(inputFile);
        return fopen(inputFile.ascii(), "r");
    }

    QFile postfile(postFilename);
    if (!postfile.open(IO_WriteOnly))
    {
        err_txt = "Unable to open post data output file.";
        return NULL;
    }

    QString startdatestr = pstartDate.toString(Qt::ISODate) + "Z";
    QString enddatestr = pendDate.toString(Qt::ISODate) + "Z";
    QTextStream poststream(&postfile);
    poststream << "<?xml version='1.0' encoding='utf-8'?>\n";
    poststream << "<SOAP-ENV:Envelope\n";
    poststream <<
        "xmlns:SOAP-ENV='http://schemas.xmlsoap.org/soap/envelope/'\n";
    poststream << "xmlns:xsd='http://www.w3.org/2001/XMLSchema'\n";
    poststream << "xmlns:xsi='http://www.w3.org/2001/XMLSchema-instance'\n";
    poststream <<
        "xmlns:SOAP-ENC='http://schemas.xmlsoap.org/soap/encoding/'>\n";
    poststream << "<SOAP-ENV:Body>\n";
    poststream << "<ns1:download  xmlns:ns1='urn:TMSWebServices'>\n";
    poststream << "<startTime xsi:type='xsd:dateTime'>";
    poststream << startdatestr << "</startTime>\n";
    poststream << "<endTime xsi:type='xsd:dateTime'>";
    poststream << enddatestr << "</endTime>\n";
    poststream << "</ns1:download>\n";
    poststream << "</SOAP-ENV:Body>\n";
    poststream << "</SOAP-ENV:Envelope>\n";
    postfile.close();

    QString command = QString(
        "wget --http-user='%1' --http-passwd='%2' --post-file='%3' "
        "--header='Accept-Encoding:gzip' %4 --output-document=- ")
        .arg(userid).arg(password).arg(postFilename).arg(ddurl);

    if (!SHOW_WGET_OUTPUT)
        command += " 2> /dev/null ";

    command += " | gzip -df";

    if (SHOW_WGET_OUTPUT)
        VERBOSE(VB_GENERAL, "command: "<<command<<endl);

    err_txt = command;

    return popen(command.ascii(), "r");
}

bool DataDirectProcessor::GrabNextSuggestedTime(void)
{
    VERBOSE(VB_GENERAL, "Grabbing next suggested grabbing time");

    QString ddurl = providers[listings_provider].webServiceURL;
         
    QFile postfile(tmpPostFile);
    if (!postfile.open(IO_WriteOnly))
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + QString("Opening '%1'")
                .arg(tmpPostFile) + ENO);
        return false;
    }

    QTextStream poststream(&postfile);
    poststream << "<?xml version='1.0' encoding='utf-8'?>\n";
    poststream << "<SOAP-ENV:Envelope\n";
    poststream
        << "xmlns:SOAP-ENV='http://schemas.xmlsoap.org/soap/envelope/'\n";
    poststream << "xmlns:xsd='http://www.w3.org/2001/XMLSchema'\n";
    poststream << "xmlns:xsi='http://www.w3.org/2001/XMLSchema-instance'\n";
    poststream
        << "xmlns:SOAP-ENC='http://schemas.xmlsoap.org/soap/encoding/'>\n";
    poststream << "<SOAP-ENV:Body>\n";
    poststream << "<tms:acknowledge xmlns:tms='urn:TMSWebServices'>\n";
    poststream << "</SOAP-ENV:Body>\n";
    poststream << "</SOAP-ENV:Envelope>\n";
    postfile.close();

    QString command = QString("wget --http-user='%1' --http-passwd='%2' "
                              "--post-file='%3' %4 --output-document='%5'")
        .arg(GetUserID()).arg(GetPassword()).arg(tmpPostFile)
        .arg(ddurl).arg(tmpResultFile);

    if (SHOW_WGET_OUTPUT)
        VERBOSE(VB_GENERAL, "command: "<<command<<endl);
    else
        command += " 2> /dev/null ";

    myth_system(command.ascii());

    QDateTime NextSuggestedTime;
    QDateTime BlockedTime;

    QFile file(tmpResultFile);

    bool GotNextSuggestedTime = false;
    bool GotBlockedTime = false;

    if (file.open(IO_ReadOnly)) 
    {
        QTextStream stream(&file);
        QString line;
        while (!stream.atEnd()) 
        {
            line = stream.readLine();
            if (line.contains("<suggestedTime>", false))
            {
                QString tmpStr = line;
                tmpStr.replace(
                    QRegExp(".*<suggestedTime>([^<]*)</suggestedTime>.*"),
                    "\\1");

                GotNextSuggestedTime = TRUE;
                QDateTime UTCdt = QDateTime::fromString(tmpStr, Qt::ISODate);
                NextSuggestedTime = MythUTCToLocal(UTCdt);
                VERBOSE(VB_GENERAL, LOC + QString("NextSuggestedTime is: ") 
                        + NextSuggestedTime.toString(Qt::ISODate));
            }

            if (line.contains("<blockedTime>", false))
            {
                QString tmpStr = line;
                tmpStr.replace(
                    QRegExp(".*<blockedTime>([^<]*)</blockedTime>.*"), "\\1");

                GotBlockedTime = TRUE;
                QDateTime UTCdt = QDateTime::fromString(tmpStr, Qt::ISODate);
                BlockedTime = MythUTCToLocal(UTCdt);
                VERBOSE(VB_GENERAL, LOC + QString("BlockedTime is: ") 
                        + BlockedTime.toString(Qt::ISODate));
            }
        }
        file.close();
    }

    if (GotNextSuggestedTime)
    {
        int daysToSuggested =
            QDateTime::currentDateTime().daysTo(NextSuggestedTime);
        int desiredPeriod = gContext->GetNumSetting("MythFillPeriod", 1);


        if (daysToSuggested > desiredPeriod)
        {
            QDateTime newTime =
                NextSuggestedTime.addDays(desiredPeriod - daysToSuggested);
            VERBOSE(VB_IMPORTANT, LOC + QString(
                        "Provider suggested running again at %1, "
                        "but MythFillPeriod is %2.  Next run time "
                        "will be adjusted to be %3.")
                    .arg(NextSuggestedTime.toString(Qt::ISODate))
                    .arg(desiredPeriod)
                    .arg(newTime.toString(Qt::ISODate)));
            NextSuggestedTime = newTime;
        }

        int minhr = NextSuggestedTime.toString("h").toInt();
        int maxhr = NextSuggestedTime.addSecs(7200).toString("h").toInt();

        if (maxhr < minhr)
        {
            minhr = 22;
            maxhr = 24;
        }

        MSqlQuery query(MSqlQuery::DDCon());
        QString querystr =
            QString("UPDATE settings SET data = '%1' WHERE value = '%2';");

        query.prepare(querystr.arg(minhr).arg("MythFillMinHour"));
        if (!query.exec())
            MythContext::DBError("Updating DataDirect MythFillMinHour", query);

        query.prepare(querystr.arg(maxhr).arg("MythFillMaxHour"));
        if (!query.exec())
            MythContext::DBError("Updating DataDirect MythFillMaxHour", query);

        query.prepare(querystr.arg(NextSuggestedTime.toString(Qt::ISODate))
                      .arg("MythFillSuggestedRunTime"));

        if (!query.exec())
            MythContext::DBError("Updating DataDirect Suggested RunTime",
                                 query);
    }
    return GotNextSuggestedTime;
}

bool DataDirectProcessor::GrabData(const QDateTime pstartDate,
                                   const QDateTime pendDate)
{
    QString msg = (pstartDate.addSecs(1) == pendDate) ? "channel" : "listing";
    VERBOSE(VB_GENERAL, "Grabbing " << msg << " data");

    QString err = "";
    QString ddurl = providers[listings_provider].webServiceURL;

    FILE *fp = DDPost(ddurl, tmpPostFile, inputfilename,
                      GetUserID(), GetPassword(),
                      pstartDate, pendDate, err);
    if (!fp)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Failed to get data " +
                QString("(%1) -- ").arg(err) + ENO);
        return false;
    }

    QFile f;
    if (f.open(IO_ReadOnly, fp)) 
    {
        DDStructureParser ddhandler(*this);
        QXmlInputSource  xmlsource(&f);
        QXmlSimpleReader xmlsimplereader;
        xmlsimplereader.setContentHandler(&ddhandler);
        xmlsimplereader.parse(xmlsource);
        f.close();
    }
    else
    {
        VERBOSE(VB_GENERAL, LOC_ERR + "Error opening DataDirect file");
        pclose(fp);
        fp = NULL;
    }

    return fp;
}

bool DataDirectProcessor::GrabLineupsOnly(void)
{
    const QDateTime start = QDateTime(QDate::currentDate().addDays(2),
                                      QTime::QTime(23, 59, 0));
    const QDateTime end   = start.addSecs(1);

    return GrabData(start, end);
}   

bool DataDirectProcessor::GrabAllData(void)
{
    return GrabData(QDateTime(QDate::currentDate()).addDays(-2),
                    QDateTime(QDate::currentDate()).addDays(15));
}

void DataDirectProcessor::CreateATempTable(const QString &ptablename, 
                                           const QString &ptablestruct) 
{
    MSqlQuery query(MSqlQuery::DDCon());
    QString querystr;
    querystr = "CREATE TEMPORARY TABLE IF NOT EXISTS " + ptablename + " " + 
        ptablestruct + ";";

    if (!query.exec(querystr))
        MythContext::DBError("Creating temporary table", query);

    querystr = "TRUNCATE TABLE " + ptablename + ";";

    if (!query.exec(querystr))
        MythContext::DBError("Truncating temporary table", query);
}      

void DataDirectProcessor::CreateTempTables() 
{
    QMap<QString,QString> dd_tables;

    dd_tables["dd_station"] =
        "( stationid char(12),           callsign char(10),     "
        "  stationname varchar(40),      affiliate varchar(25), "
        "  fccchannelnumber char(15) )";

    dd_tables["dd_lineup"] =
        "( lineupid char(100),           name char(42),  "
        "  type char(20),                postal char(6), "
        "  device char(30) )";

    dd_tables["dd_lineupmap"] =
        "( lineupid char(100),           stationid char(12),   "
        "  channel char(5),              channelMinor char(3) )";


    dd_tables["dd_v_station"] =
        "( stationid char(12),           callsign char(10),     "
        "  stationname varchar(40),      affiliate varchar(25), "
        "  fccchannelnumber char(15),    channel char(5),       "
        "  channelMinor char(3) )";

    dd_tables["dd_schedule"] =
        "( programid char(12),           stationid char(12), "
        "  scheduletime datetime,        duration time,      "
        "  isrepeat bool,                stereo bool,        "
        "  subtitled bool,               hdtv bool,          "
        "  closecaptioned bool,          tvrating char(5),   "
        "  partnumber int,               parttotal int,      "
        "  endtime datetime, "
        "INDEX progidx (programid) )";

    dd_tables["dd_program"] =
        "( programid char(12) NOT NULL,  seriesid char(12),     "
        "  title varchar(120),           subtitle varchar(150), "
        "  description text,             mpaarating char(5),    "
        "  starrating char(5),           runtime time,          "
        "  year char(4),                 showtype char(30),     "
        "  category_type char(64),       colorcode char(20),    "
        "  originalairdate date,         syndicatedepisodenumber char(20), "
        "  stars float unsigned, "
        "PRIMARY KEY (programid))";

    dd_tables["dd_v_program"] =
        "( chanid int unsigned NOT NULL, starttime datetime NOT NULL, "
        "  endtime datetime,             title varchar(128),          "
        "  subtitle varchar(128),        description text,            "
        "  category varchar(64),         category_type varchar(64),   "
        "  airdate year,                 stars float unsigned,        "
        "  previouslyshown tinyint,      isrepeat bool,               "
        "  stereo bool,                  subtitled bool,              "
        "  hdtv bool,                    closecaptioned bool,         "
        "  partnumber int,               parttotal int,               "
        "  seriesid char(12),            originalairdate date,        "
        "  showtype varchar(30),         colorcode varchar(20),       "
        "  syndicatedepisodenumber varchar(20), programid char(12),   "
        "  tvrating char(5),             mpaarating char(5),          "
        "INDEX progidx (programid))";

    dd_tables["dd_productioncrew"] =
        "( programid char(12),           role char(30),    "
        "  givenname char(20),           surname char(20), "
        "  fullname char(41), "
        "INDEX progidx (programid), "
        "INDEX nameidx (fullname))";

    dd_tables["dd_genre"] =
        "( programid char(12) NOT NULL,  class char(30), "
        "  relevance char(1), "
        "INDEX progidx (programid))";

    QMap<QString,QString>::const_iterator it;
    for (it = dd_tables.begin(); it != dd_tables.end(); ++it)
        CreateATempTable(it.key(), *it);
}

bool DataDirectProcessor::GrabLoginCookiesAndLineups(void)
{
    VERBOSE(VB_GENERAL, "Grabbing login cookies and lineups");

    PostList list;
    list.push_back(PostItem("username", GetUserID()));
    list.push_back(PostItem("password", GetPassword()));
    list.push_back(PostItem("action",   "Login"));

    QString labsURL   = providers[listings_provider].webURL;
    QString loginPage = providers[listings_provider].loginPage;

    bool ok = Post(labsURL + loginPage, list, tmpResultFile, "", cookieFile);

    bool got_cookie = QFileInfo(cookieFile).size() > 100;
    return ok && got_cookie && ParseLineups(tmpResultFile);
}

bool DataDirectProcessor::GrabLineupForModify(const QString &lineupid)
{
    VERBOSE(VB_GENERAL, QString("Grabbing lineup %1 for modification")
            .arg(lineupid));

    RawLineupMap::const_iterator it = rawlineups.find(lineupid);
    if (it == rawlineups.end())
        return false;

    PostList list;
    list.push_back(PostItem("udl_id",    GetRawUDLID(lineupid)));
    list.push_back(PostItem("zipcode",   GetRawZipCode(lineupid)));
    list.push_back(PostItem("lineup_id", lineupid));
    list.push_back(PostItem("submit",    "Modify"));

    QString labsURL = providers[listings_provider].webURL;
    bool ok = Post(labsURL + (*it).get_action, list, tmpResultFile,
                   cookieFile, "");

    return ok && ParseLineup(lineupid, tmpResultFile);
}

void DataDirectProcessor::SetAll(const QString &lineupid, bool val)
{
    VERBOSE(VB_GENERAL, QString("%1 all channels in lineup %2")
            .arg((val) ? "Selecting" : "Deselecting").arg(lineupid));

    RawLineupMap::iterator lit = rawlineups.find(lineupid);
    if (lit == rawlineups.end())
        return;

    RawLineupChannels &ch = (*lit).channels;
    for (RawLineupChannels::iterator it = ch.begin(); it != ch.end(); ++it)
        (*it).chk_checked = val;
}

bool DataDirectProcessor::GrabFullLineup(const QString &lineupid, bool restore)
{
    bool ok = GrabLoginCookiesAndLineups();
    if (!ok)
        return false;

    ok = GrabLineupForModify(lineupid);
    if (!ok)
        return false;

    RawLineupMap::iterator lit = rawlineups.find(lineupid);
    if (lit == rawlineups.end())
        return false;

    const RawLineupChannels orig_channels = (*lit).channels;
    SetAll(lineupid, true);
    if (!SaveLineupChanges(lineupid))
        return false;

    ok = GrabLineupsOnly();

    (*lit).channels = orig_channels;
    if (restore)
        ok &= SaveLineupChanges(lineupid);

    return ok;
}

bool DataDirectProcessor::SaveLineup(const QString &lineupid,
                                     const QMap<QString,bool> &xmltvids)
{
    QMap<QString,bool> callsigns;
    RawLineupMap::iterator lit = rawlineups.find(lineupid);
    if (lit == rawlineups.end())
        return false;

    // Get callsigns based on xmltv ids (aka stationid)
    DDLineupMap::const_iterator ddit = lineupmaps.find(lineupid);
    DDLineupChannels::const_iterator it;
    for (it = (*ddit).begin(); it != (*ddit).end(); ++it)
    {
        if (xmltvids.find((*it).stationid) != xmltvids.end())
            callsigns[GetDDStation((*it).stationid).callsign] = true;
    }

    // Set checked mark based on whether the channel is mapped
    RawLineupChannels &ch = (*lit).channels;
    RawLineupChannels::iterator cit;
    for (cit = ch.begin(); cit != ch.end(); ++cit)
    {
        bool chk = callsigns.find((*cit).lbl_callsign) != callsigns.end();
        (*cit).chk_checked = chk;
    }

    // Save these changes
    return SaveLineupChanges(lineupid);
}

bool DataDirectProcessor::SaveLineupChanges(const QString &lineupid)
{
    RawLineupMap::const_iterator lit = rawlineups.find(lineupid);
    if (lit == rawlineups.end())
        return false;

    const RawLineup &lineup = *lit;
    const RawLineupChannels &ch = lineup.channels;
    RawLineupChannels::const_iterator it;

    PostList list;
    for (it = ch.begin(); it != ch.end(); ++it)
    {
        if ((*it).chk_checked)
            list.push_back(PostItem((*it).chk_name, (*it).chk_value));
    }
    list.push_back(PostItem("action", "Update"));

    VERBOSE(VB_GENERAL, QString("Saving lineup %1 with %2 channels")
            .arg(lineupid).arg(list.size() - 1));

    QString labsURL = providers[listings_provider].webURL;
    return Post(labsURL + lineup.set_action, list, "", cookieFile, "");
}

bool DataDirectProcessor::UpdateListings(uint sourceid)
{
    MSqlQuery query(MSqlQuery::DDCon());
    query.prepare(
        "SELECT xmltvid "
        "FROM channel "
        "WHERE sourceid = :SOURCEID");
    query.bindValue(":SOURCEID", sourceid);

    if (!query.exec() || !query.isActive())
    {
        MythContext::DBError("Selecting existing channels", query);
        return false;
    }

    QString a, b, c, lineupid;
    if (!SourceUtil::GetListingsLoginData(sourceid, a, b, c, lineupid))
        return false;

    QMap<QString,bool> xmltvids;
    while (query.next())
    {
        if (!query.value(0).toString().isEmpty())
            xmltvids[query.value(0).toString()] = true;
    }

    VERBOSE(VB_GENERAL, "Saving updated DataDirect listing");
    bool ok = SaveLineup(lineupid, xmltvids);

    if (!ok)
        VERBOSE(VB_IMPORTANT, "Failed to update DataDirect listings.");

    return ok;
}

QDateTime  DataDirectProcessor::GetDDProgramsStartAt(bool localtime) const
{
    if (localtime)
        return MythUTCToLocal(actuallistingsfrom);
    return actuallistingsfrom;
}

QDateTime  DataDirectProcessor::GetDDProgramsEndAt(bool localtime) const
{
    if (localtime)
        return MythUTCToLocal(actuallistingsto);
    return actuallistingsto;
}

QString DataDirectProcessor::GetRawUDLID(const QString &lineupid) const
{
    RawLineupMap::const_iterator it = rawlineups.find(lineupid);
    if (it == rawlineups.end())
        return QString::null;
    return (*it).udl_id;
}

QString DataDirectProcessor::GetRawZipCode(const QString &lineupid) const
{
    RawLineupMap::const_iterator it = rawlineups.find(lineupid);
    if (it == rawlineups.end())
        return QString::null;
    return (*it).zipcode;
}

RawLineup DataDirectProcessor::GetRawLineup(const QString &lineupid) const
{
    RawLineup tmp;
    RawLineupMap::const_iterator it = rawlineups.find(lineupid);
    if (it == rawlineups.end())
        return tmp;
    return (*it);
}

bool DataDirectProcessor::Post(QString url, const PostList &list,
                               QString documentFile,
                               QString inCookieFile, QString outCookieFile)
{
    QString dfile = QString("'%1' ").arg(documentFile);
    QString command = "wget ";

    if (!inCookieFile.isEmpty())
        command += QString("--load-cookies=%1 ").arg(inCookieFile);

    if (!outCookieFile.isEmpty())
    {
        command += "--keep-session-cookies ";
        command += QString("--save-cookies=%1 ").arg(outCookieFile);
    }

    QString post_data = "";
    for (uint i = 0; i < list.size(); i++)
    {
        post_data += ((i) ? "&" : "") + list[i].key + "=";
        post_data += html_escape(list[i].value);
    }

    if (post_data.length())
        command += "--post-data='" + post_data + "' ";

    command += url;
    command += " ";

    command += "--output-document=";
    command += (documentFile.isEmpty()) ? "- " : dfile;

    if (SHOW_WGET_OUTPUT)
        VERBOSE(VB_GENERAL, "command: "<<command<<endl);
    else
    {
        command += (documentFile.isEmpty()) ? "&> " : "2> ";
        command += "/dev/null ";
    }

    myth_system(command.ascii());

    if (documentFile.isEmpty())
        return true;

    QFileInfo fi(documentFile);
    return fi.size();
}

bool DataDirectProcessor::ParseLineups(const QString &documentFile)
{
    QFile file(documentFile);
    if (!file.open(IO_ReadOnly))
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR +
                QString("Failed to open '%1'").arg(documentFile));
        return false;
    }

    QTextStream stream(&file);
    bool in_form = false;
    QString get_action = QString::null;
    QMap<QString,QString> name_value;

    rawlineups.clear();

    while (!stream.atEnd()) 
    {
        QString line = stream.readLine();
        QString llow = line.lower();
        int frm = llow.find("<form");
        if (frm >= 0)
        {
            in_form = true;
            get_action = get_setting(line.mid(frm + 5), "action");
            name_value.clear();
            //cerr<<QString("action: %1").arg(action)<<endl;
        }

        if (!in_form)
            continue;

        int inp = llow.find("<input");
        if (inp >= 0)
        {
            QString input_line = line.mid(inp + 6);
            //cerr<<QString("input: %1").arg(input_line)<<endl;
            QString name  = get_setting(input_line, "name");
            QString value = get_setting(input_line, "value");
            //cerr<<QString("name: %1").arg(name)<<endl;
            //cerr<<QString("value: %1").arg(value)<<endl;
            if (!name.isEmpty() && !value.isEmpty())
                name_value[name] = value;
        }

        if (llow.contains("</form>"))
        {
            in_form = false;
            if (!get_action.isEmpty() &&
                !name_value["udl_id"].isEmpty() &&
                !name_value["zipcode"].isEmpty() &&
                !name_value["lineup_id"].isEmpty())
            {
                RawLineup item(get_action, name_value["udl_id"],
                               name_value["zipcode"]);

                rawlineups[name_value["lineup_id"]] = item;
#if 0
                VERBOSE(VB_IMPORTANT, LOC +
                        QString("<%1>  \t--> <%2,%3,%4>")
                        .arg(name_value["lineup_id"])
                        .arg(item.udl_id).arg(item.zipcode)
                        .arg(item.get_action));
#endif
            }
        }
    }    
    return true;
}

bool DataDirectProcessor::ParseLineup(const QString &lineupid,
                                      const QString &documentFile)
{
    QFile file(documentFile);
    if (!file.open(IO_ReadOnly))
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR +
                QString("Failed to open '%1'").arg(documentFile));

        return false;
    }

    QTextStream stream(&file);
    bool in_form = false;
    int in_label = 0;
    QMap<QString,QString> settings;

    RawLineup &lineup = rawlineups[lineupid];
    RawLineupChannels &ch = lineup.channels;

    while (!stream.atEnd()) 
    {
        QString line = stream.readLine();
        QString llow = line.lower();
        int frm = llow.find("<form");
        if (frm >= 0)
        {
            in_form = true;
            lineup.set_action = get_setting(line.mid(frm + 5), "action");
            //cerr<<"set_action: "<<lineup.set_action<<endl;
        }

        if (!in_form)
            continue;

        int inp = llow.find("<input");
        if (inp >= 0)
        {
            QString in_line = line.mid(inp + 6);
            settings.clear();
            settings["chk_name"]    = get_setting(in_line, "name");
            settings["chk_id"]      = get_setting(in_line, "id");
            settings["chk_value"]   = get_setting(in_line, "value");
            settings["chk_checked"] = has_setting(in_line, "checked")?"1":"0";
        }

        int lbl = llow.find("<label");
        if (lbl >= 0)
        {
            QString lbl_line = line.mid(inp + 6);
            QString name = get_setting(lbl_line, "for");
            in_label = (name == settings["chk_name"]) ? 1 : 0;
        }
        
        if (in_label)
        {
            int start = (lbl >= 0) ? lbl + 6 : 0;
            int beg = llow.find("<td>", start), end = -1;
            if (beg)
                end = llow.find("</td>", beg + 4);

            if (end >= 0)
            {
                QString key = (in_label == 1) ? "lbl_ch" : "lbl_callsign";
                QString val = line.mid(beg + 4, end - beg - 4);
                settings[key] = val.replace("&nbsp;", "", false);
                in_label++;
            }
        }

        in_label = (llow.find("</label") >= 0) ? 0 : in_label;

        if (!in_label &&
            !settings["chk_name"].isEmpty() &&
            !settings["chk_id"].isEmpty() &&
            !settings["chk_value"].isEmpty() &&
            !settings["chk_checked"].isEmpty() &&
            !settings["lbl_ch"].isEmpty() &&
            !settings["lbl_callsign"].isEmpty())
        {
            RawLineupChannel chan(
                settings["chk_name"],  settings["chk_id"],
                settings["chk_value"], settings["chk_checked"] == "1",
                settings["lbl_ch"],    settings["lbl_callsign"]);

#if 0
            VERBOSE(VB_IMPORTANT, LOC +
                    QString("name: %1  id: %2  value: %3  "
                            "checked: %4  ch: %5  call: %6")
                    .arg(settings["chk_name"]).arg(settings["chk_id"])
                    .arg(settings["chk_value"]).arg(settings["chk_checked"])
                    .arg(settings["lbl_ch"],4).arg(settings["lbl_callsign"]));
#endif

            ch.push_back(chan);
            settings.clear();
        }

        if (llow.contains("</form>"))
        {
            in_form = false;
        }
    }
    return true;
}

static QString html_escape(QString str)
{
    QString new_str = "";
    for (uint i = 0; i < str.length(); i++)
    {
        if (str[i].isLetterOrNumber())
            new_str += str[i];
        else
            new_str += QString("\%%1").arg((int)str[i].latin1(),0,16);
    }

    return new_str;
}

static QString get_setting(QString line, QString key)
{
    QString llow = line.lower();
    QString kfind = key + "=\"";
    int beg = llow.find(kfind), end = -1;

    if (beg >= 0)
    {
        end = llow.find("\"", beg + kfind.length());
        return line.mid(beg + kfind.length(), end - beg - kfind.length());
    }

    kfind = key + "=";
    beg = llow.find(kfind);
    if (beg < 0)
        return QString::null;

    uint i = beg + kfind.length();
    while (i < line.length() && !line[i].isSpace() && line[i] != '>')
        i++;

    if (i < line.length() && (line[i].isSpace() || line[i] == '>'))
        return line.mid(beg + kfind.length(), i - beg - kfind.length());

    return QString::null;
}

static bool has_setting(QString line, QString key)
{
    return (line.lower().find(key) >= 0);
}

static void get_atsc_stuff(QString channum, int sourceid, int freqid,
                           int &major, int &minor, long long &freq)
{
    major = freqid;
    minor = 0;

    int chansep = channum.find(QRegExp("\\D"));
    if (chansep < 0)
        return;

    major = channum.left(chansep).toInt();
    minor = channum.right(channum.length() - (chansep + 1)).toInt();

    freq = get_center_frequency("atsc", "vsb8", "us", freqid);

    // Check if this is connected to an HDTV card.
    MSqlQuery query(MSqlQuery::DDCon());
    query.prepare(
        "SELECT cardtype "
        "FROM capturecard, cardinput "
        "WHERE cardinput.cardid = capturecard.cardid AND "
        "      sourceid         = :SOURCEID");
    query.bindValue(":SOURCEID", sourceid);

    if (query.exec() && query.isActive() && query.next() &&
        query.value(0).toString() == "HDTV")
    {
        freq -= 1750000; // convert to visual carrier freq.
    }
}

static uint create_atscsrcid(QString chan_major, QString chan_minor)
{
    bool ok;
    uint major = chan_major.toUInt(&ok), atscsrcid = 0;
    if (!ok)
        return atscsrcid;

    atscsrcid = major << 8;
    if (chan_minor.isEmpty())
        return atscsrcid;

    return atscsrcid | chan_minor.toUInt();
}

static QString process_dd_station(
    uint sourceid, QString chan_major, QString chan_minor,
    QString &tvformat, uint &freqid, uint &atscsrcid)
{
    QString channum = chan_major;
    atscsrcid = create_atscsrcid(chan_major, chan_minor);
    tvformat = "Default";

    if (atscsrcid & 0xff)
    {
        tvformat = "atsc";
        channum += SourceUtil::GetChannelSeparator(sourceid) + chan_minor;
    }

    if (!freqid && !(atscsrcid & 0xff))
        freqid = chan_major.toInt();

    return channum;
}

static void update_channel_basic(uint    sourceid,   bool    insert,
                                 QString xmltvid,    QString callsign,
                                 QString name,       uint    freqid,
                                 QString chan_major, QString chan_minor)
{
    callsign = (callsign.isEmpty()) ? name : callsign;

    uint atscsrcid;
    QString tvformat;
    QString channum = process_dd_station(
        sourceid, chan_major, chan_minor, tvformat, freqid, atscsrcid);

    // First check if channel already in DB, but without xmltvid
    MSqlQuery query(MSqlQuery::DDCon());
    query.prepare("SELECT chanid FROM channel "
                  "WHERE sourceid = :SOURCEID AND "
                  "      (channum=:CHANNUM OR atscsrcid=:ATSCSRCID)");
    query.bindValue(":SOURCEID",  sourceid);
    query.bindValue(":CHANNUM",   channum);
    query.bindValue(":ATSCSRCID", atscsrcid);

    if (!query.exec() || !query.isActive())
    {
        MythContext::DBError(
            "Getting chanid of existing channel", query);
        return; // go on to next channel without xmltv
    }

    if (query.size() > 0)
    {
        // The channel already exists in DB, at least once,
        // so set the xmltvid..
        MSqlQuery chan_update_q(MSqlQuery::DDCon());
        chan_update_q.prepare(
            "UPDATE channel "
            "SET xmltvid = :XMLTVID "
            "WHERE chanid = :CHANID AND sourceid = :SOURCEID");

        while (query.next())
        {
            uint chanid = query.value(0).toInt();
            chan_update_q.bindValue(":CHANID",    chanid);
            chan_update_q.bindValue(":XMLTVID",   xmltvid);
            chan_update_q.bindValue(":SOURCEID",  sourceid);
            if (!chan_update_q.exec() || !chan_update_q.isActive())
            {
                MythContext::DBError(
                    "Updating XMLTVID of existing channel", chan_update_q);
                continue; // go on to next instance of this channel
            }
        }
        return; // go on to next channel without xmltv
    }

    if (!insert)
        return; // go on to next channel without xmltv

    // The channel doesn't exist in the DB, insert it...
    int mplexid = -1, majorC, minorC, chanid = 0;
    long long freq = -1;
    get_atsc_stuff(channum, sourceid, freqid,
                   majorC, minorC, freq);

    if (minorC > 0 && freq >= 0)
        mplexid = ChannelUtil::CreateMultiplex(sourceid, "atsc", freq, "8vsb");

    if ((mplexid > 0) || (minorC == 0))
        chanid = ChannelUtil::CreateChanID(sourceid, channum);

    if (chanid > 0)
    {
        QString icon   = "";
        int  serviceid = 0;
        bool oag       = false; // use on air guide
        bool hidden    = false;
        bool hidden_in_guide = false;

        ChannelUtil::CreateChannel(
            mplexid,   sourceid,  chanid, 
            callsign,  name,      channum,
            serviceid, majorC,    minorC,
            oag,       hidden,    hidden_in_guide,
            freqid,    icon,      tvformat,
            xmltvid);
    }
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
