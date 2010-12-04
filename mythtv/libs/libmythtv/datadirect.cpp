#include <unistd.h>

// Qt headers
#include <QDir>
#include <QFileInfo>

// MythTV headers
#include "datadirect.h"
#include "sourceutil.h"
#include "channelutil.h"
#include "frequencytables.h"
#include "listingsources.h"
#include "mythcontext.h"
#include "mythdb.h"
#include "mythverbose.h"
#include "mythversion.h"
#include "util.h"
#include "dbutil.h"

#define SHOW_WGET_OUTPUT 0

#define LOC QString("DataDirect: ")
#define LOC_WARN QString("DataDirect, Warning: ")
#define LOC_ERR QString("DataDirect, Error: ")

static QMutex  user_agent_lock;
static QString user_agent;

static QMutex lineup_type_lock;
static QMap<QString,uint> lineupid_to_srcid;
static QMap<uint,QString> srcid_to_type;

static void    set_lineup_type(const QString &lineupid, const QString &type);
static QString get_lineup_type(uint sourceid);
static QString get_setting(QString line, QString key);
static bool    has_setting(QString line, QString key);
static QString html_escape(QString str);
static void    get_atsc_stuff(QString channum, int sourceid, int freqid,
                              int &major, int &minor, long long &freq);
static QString process_dd_station(uint sourceid,
                                  QString  chan_major, QString  chan_minor,
                                  QString &tvformat,   uint    &freqid);
static uint    update_channel_basic(uint    sourceid,   bool    insert,
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
    repeat(false),              isnew(false),
    stereo(false),              dolby(false),
    subtitled(false),
    hdtv(false),                closecaptioned(false),
    tvrating(""),
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
        curr_schedule.isnew = (pxmlatts.value("new") == "true");
        curr_schedule.stereo = (pxmlatts.value("stereo") == "true");
        curr_schedule.dolby = (pxmlatts.value("dolby") == "Dolby" ||
                               pxmlatts.value("dolby") == "Dolby Digital");
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
            MythDB::DBError("Inserting into dd_station", query);
    }
    else if (pqname == "lineup")
    {
        set_lineup_type(curr_lineup.lineupid, curr_lineup.type);

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
            MythDB::DBError("Inserting into dd_lineup", query);
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
            MythDB::DBError("Inserting into dd_lineupmap", query);
    }
    else if (pqname == "schedule")
    {
        QDateTime endtime = curr_schedule.time.addSecs(
            QTime().secsTo(curr_schedule.duration));

        query.prepare(
            "INSERT INTO dd_schedule "
            "     ( programid,      stationid,   scheduletime,   "
            "       duration,       isrepeat,    stereo,         "
            "       dolby,          subtitled,   hdtv,           "
            "       closecaptioned, tvrating,    partnumber,      "
            "       parttotal,      endtime,     isnew) "
            "VALUES "
            "     (:PROGRAMID, :STATIONID,  :TIME,           "
            "      :DURATION,  :ISREPEAT,   :STEREO,         "
            "      :DOLBY,     :SUBTITLED,  :HDTV,           "
            "      :CAPTIONED, :TVRATING,   :PARTNUMBER,     "
            "      :PARTTOTAL, :ENDTIME,    :ISNEW)");

        query.bindValue(":PROGRAMID",   curr_schedule.programid);
        query.bindValue(":STATIONID",   curr_schedule.stationid);
        query.bindValue(":TIME",        curr_schedule.time);
        query.bindValue(":DURATION",    curr_schedule.duration);
        query.bindValue(":ISREPEAT",    curr_schedule.repeat);
        query.bindValue(":STEREO",      curr_schedule.stereo);
        query.bindValue(":DOLBY",       curr_schedule.dolby);
        query.bindValue(":SUBTITLED",   curr_schedule.subtitled);
        query.bindValue(":HDTV",        curr_schedule.hdtv);
        query.bindValue(":CAPTIONED",   curr_schedule.closecaptioned);
        query.bindValue(":TVRATING",    curr_schedule.tvrating);
        query.bindValue(":PARTNUMBER",  curr_schedule.partnumber);
        query.bindValue(":PARTTOTAL",   curr_schedule.parttotal);
        query.bindValue(":ENDTIME",     endtime);
        query.bindValue(":ISNEW",       curr_schedule.isnew);

        if (!query.exec())
            MythDB::DBError("Inserting into dd_schedule", query);
    }
    else if (pqname == "program")
    {
        float staravg = 0.0;
        if (!curr_program.starRating.isEmpty())
        {
            int fullstarcount = curr_program.starRating.count("*");
            int halfstarcount = curr_program.starRating.count("+");
            staravg = (fullstarcount + (halfstarcount * .5)) / 4;
        }

        QString cat_type = "";
        QString prefix = curr_program.programid.left(2);

        if (prefix == "MV")
            cat_type = "movie";
        else if (prefix == "SP")
            cat_type = "sports";
        else if (prefix == "EP" ||
                 curr_program.showtype.contains("series", Qt::CaseInsensitive))
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
        query.bindValue(":TITLE",       curr_program.title);
        query.bindValue(":SUBTITLE",    curr_program.subtitle);
        query.bindValue(":DESCRIPTION", curr_program.description);
        query.bindValue(":SHOWTYPE",    curr_program.showtype);
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
            MythDB::DBError("Inserting into dd_program", query);
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
        query.bindValue(":ROLE",        roleunderlines);
        query.bindValue(":GIVENNAME",   curr_productioncrew.givenname);
        query.bindValue(":SURNAME",     curr_productioncrew.surname);
        query.bindValue(":FULLNAME",    fullname);

        if (!query.exec())
            MythDB::DBError("Inserting into dd_productioncrew", query);

        curr_productioncrew.givenname = "";
        curr_productioncrew.surname = "";
    }
    else if (pqname == "genre")
    {
        query.prepare(
            "INSERT INTO dd_genre "
            "       ( programid,  class,  relevance) "
            "VALUES (:PROGRAMID, :CLASS, :RELEVANCE)");

        query.bindValue(":PROGRAMID",   lastprogramid);
        query.bindValue(":CLASS",       curr_genre.gclass);
        query.bindValue(":RELEVANCE",   curr_genre.relevance);

        if (!query.exec())
            MythDB::DBError("Inserting into dd_genre", query);
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
    if (pchars.trimmed().isEmpty())
        return true;

    if (currtagname == "message")
    {
        if (pchars.contains("expire"))
        {
            QString ExtractDateFromMessage = pchars.right(20);
            QDateTime EDFM = QDateTime::fromString(ExtractDateFromMessage,
                                                   Qt::ISODate);
            QString SDDateFormat = GetMythDB()->GetSetting("DateFormat",
                                                           "ddd d MMMM");
            // Ensure we show the year when it's important, regardless of
            // specified DateFormat
            if ((!SDDateFormat.contains('y')) &&
                (EDFM.date().year() != QDate::currentDate().year()))
            {
                SDDateFormat.append(" (yyyy)");
            }
            QString dateFormat = QString("%1 %2")
                    .arg(SDDateFormat)
                    .arg(GetMythDB()->GetSetting("TimeFormat", "hh:mm"));
            QString ExpirationDate = EDFM.toString(dateFormat);

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
                MythDB::DBError("Updating DataDirect Status Message",
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

extern const char *myth_source_version;

DataDirectProcessor::DataDirectProcessor(uint lp, QString user, QString pass) :
    listings_provider(lp % DD_PROVIDER_COUNT),
    userid(user),                   password(pass),
    tmpDir("/tmp"),                 cachedata(false),
    inputfilename(""),              tmpPostFile(QString::null),
    tmpResultFile(QString::null),   cookieFile(QString::null),
    cookieFileDT()
{
    {
        QMutexLocker locker(&user_agent_lock);
        user_agent = QString("MythTV/%1.%2")
            .arg(MYTH_BINARY_VERSION).arg(myth_source_version);
    }

    DataDirectURLs urls0(
        "Tribune Media Zap2It",
        "http://datadirect.webservices.zap2it.com/tvlistings/xtvdService",
        "http://labs.zap2it.com",
        "/ztvws/ztvws_login/1,1059,TMS01-1,00.html");
    DataDirectURLs urls1(
        "Schedules Direct",
        "http://webservices.schedulesdirect.tmsdatadirect.com"
        "/schedulesdirect/tvlistings/xtvdService",
        "http://schedulesdirect.org",
        "/login/index.php");
    providers.push_back(urls0);
    providers.push_back(urls1);
}

DataDirectProcessor::~DataDirectProcessor()
{
    VERBOSE(VB_GENERAL, LOC + "Deleting temporary files");

    if (!tmpPostFile.isEmpty())
    {
        QByteArray tmp = tmpPostFile.toAscii();
        unlink(tmp.constData());
    }

    if (!tmpResultFile.isEmpty())
    {
        QByteArray tmp = tmpResultFile.toAscii();
        unlink(tmp.constData());
    }

    if (!cookieFile.isEmpty())
    {
        QByteArray tmp = cookieFile.toAscii();
        unlink(tmp.constData());
    }

    QDir d(tmpDir, "mythtv_dd_cache_*", QDir::Name,
           QDir::Files | QDir::NoSymLinks);

    for (uint i = 0; i < d.count(); i++)
    {
        //cout<<"deleting '"<<tmpDir<<"/"<<d[i]<<"'"<<endl;
        QString    tmps = QString(tmpDir + "/" + d[i]);
        QByteArray tmpa = tmps.toAscii();
        unlink(tmpa.constData());
    }

    if (tmpDir != "/tmp")
    {
        QByteArray tmp = tmpDir.toAscii();
        rmdir(tmp.constData());
    }
}

void DataDirectProcessor::UpdateStationViewTable(QString lineupid)
{
    MSqlQuery query(MSqlQuery::DDCon());

    if (!query.exec("TRUNCATE TABLE dd_v_station;"))
        MythDB::DBError("Truncating temporary table dd_v_station", query);

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
        MythDB::DBError("Populating temporary table dd_v_station", query);
}

void DataDirectProcessor::UpdateProgramViewTable(uint sourceid)
{
    MSqlQuery query(MSqlQuery::DDCon());

    if (!query.exec("TRUNCATE TABLE dd_v_program;"))
        MythDB::DBError("Truncating temporary table dd_v_program", query);

    QString qstr =
        "INSERT INTO dd_v_program "
        "     ( chanid,                  starttime,       endtime,         "
        "       title,                   subtitle,        description,     "
        "       airdate,                 stars,           previouslyshown, "
        "       stereo,                  dolby,           subtitled,       "
        "       hdtv,                    closecaptioned,  partnumber,      "
        "       parttotal,               seriesid,        originalairdate, "
        "       showtype,                category_type,   colorcode,       "
        "       syndicatedepisodenumber, tvrating,        mpaarating,      "
        "       programid )      "
        "SELECT chanid,                  scheduletime,    endtime,         "
        "       title,                   subtitle,        description,     "
        "       year,                    stars,           isrepeat,        "
        "       stereo,                  dolby,           subtitled,       "
        "       hdtv,                    closecaptioned,  partnumber,      "
        "       parttotal,               seriesid,        originalairdate, "
        "       showtype,                category_type,   colorcode,       "
        "       syndicatedepisodenumber, tvrating,        mpaarating,      "
        "       dd_program.programid "
        "FROM channel, dd_schedule, dd_program "
        "WHERE ((dd_schedule.programid = dd_program.programid)  AND "
        "       (channel.xmltvid       = dd_schedule.stationid) AND "
        "       (channel.sourceid      = :SOURCEID))";

    query.prepare(qstr);

    query.bindValue(":SOURCEID", sourceid);

    if (!query.exec())
        MythDB::DBError("Populating temporary table dd_v_program", query);

    if (!query.exec("ANALYZE TABLE dd_v_program;"))
        MythDB::DBError("Analyzing table dd_v_program", query);

    if (!query.exec("ANALYZE TABLE dd_productioncrew;"))
        MythDB::DBError("Analyzing table dd_productioncrew", query);
}

int DataDirectProcessor::UpdateChannelsSafe(
    uint sourceid,
    bool insert_channels,
    bool filter_new_channels)
{
    int new_channels = 0;

    if (!SourceUtil::GetConnectionCount(sourceid))
    {
        VERBOSE(VB_IMPORTANT, LOC +
                "Not inserting channels into disconnected source "
                <<sourceid<<".");
        return -1;
    }

    if (!SourceUtil::IsProperlyConnected(sourceid, true))
        return -1;

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
        MythDB::DBError("Selecting new channels", query);
        return -1;
    }

    bool is_encoder = (SourceUtil::IsEncoder(sourceid, true) ||
                       SourceUtil::IsUnscanable(sourceid));

    while (query.next())
    {
        QString xmltvid    = query.value(0).toString();
        QString callsign   = query.value(1).toString();
        QString name       = query.value(2).toString();
        uint    freqid     = query.value(3).toUInt();
        QString chan_major = query.value(4).toString();
        QString chan_minor = query.value(5).toString();

        if (filter_new_channels && is_encoder &&
            (query.value(5).toUInt() > 0))
        {
#if 0
            VERBOSE(VB_GENERAL, LOC + QString(
                        "Not adding channel %1-%2 '%3' (%4),\n\t\t\t"
                        "looks like a digital channel on an analog source.")
                    .arg(chan_major).arg(chan_minor).arg(name).arg(callsign));
#endif
            continue;
        }

        uint mods =
            update_channel_basic(sourceid, insert_channels && is_encoder,
                                 xmltvid, callsign, name, freqid,
                                 chan_major, chan_minor);

        (void) mods;
#if 0
        if (!insert_channels && !mods)
        {
            VERBOSE(VB_GENERAL, LOC + QString("Not adding channel '%1' (%2).")
                    .arg(name).arg(callsign));
        }
#endif
        new_channels++;
    }

    teardown_frequency_tables();

    return new_channels;
}

bool DataDirectProcessor::UpdateChannelsUnsafe(
    uint sourceid, bool filter_new_channels)
{
    if (filter_new_channels &&
        !SourceUtil::IsProperlyConnected(sourceid, false))
    {
        return false;
    }

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
        "    atsc_major_chan = :MAJORCHAN, "
        "    atsc_minor_chan = :MINORCHAN "
        "WHERE xmltvid = :STATIONID AND sourceid = :SOURCEID");

    bool is_encoder = (SourceUtil::IsEncoder(sourceid, true) ||
                       SourceUtil::IsUnscanable(sourceid));

    while (dd_station_info.next())
    {
        uint    freqid     = dd_station_info.value(3).toUInt();
        QString chan_major = dd_station_info.value(4).toString();
        QString chan_minor = dd_station_info.value(5).toString();
        QString tvformat   = QString::null;
        QString channum    = process_dd_station(
            sourceid, chan_major, chan_minor, tvformat, freqid);

        if (filter_new_channels && is_encoder &&
            (dd_station_info.value(5).toUInt() > 0))
        {
#if 0
            VERBOSE(VB_GENERAL, LOC + QString(
                        "Not adding channel %1-%2 '%3' (%4),\n\t\t\t"
                        "looks like a digital channel on an analog source.")
                    .arg(chan_major).arg(chan_minor)
                    .arg(dd_station_info.value(1).toString())
                    .arg(dd_station_info.value(0).toString()));
#endif
            continue;
        }

        chan_update_q.bindValue(":CALLSIGN",  dd_station_info.value(0));
        chan_update_q.bindValue(":NAME",      dd_station_info.value(1));
        chan_update_q.bindValue(":STATIONID", dd_station_info.value(2));
        chan_update_q.bindValue(":CHANNUM",   channum);
        chan_update_q.bindValue(":SOURCEID",  sourceid);
        chan_update_q.bindValue(":FREQID",    freqid);
        chan_update_q.bindValue(":MAJORCHAN", chan_major.toUInt());
        chan_update_q.bindValue(":MINORCHAN", chan_minor.toUInt());

        if (!chan_update_q.exec())
        {
            MythDB::DBError("Updating channel table",
                            chan_update_q.lastQuery());
        }
    }

    return true;
}

void DataDirectProcessor::DataDirectProgramUpdate(void)
{
    MSqlQuery query(MSqlQuery::DDCon());

    //cerr << "Adding rows to main program table from view table..\n";
    query.prepare(
        "INSERT IGNORE INTO program "
        "  ( chanid,        starttime,   endtime,         title,           "
        "    subtitle,      description, showtype,        category,        "
        "    category_type, airdate,     stars,           previouslyshown, "
        "    stereo,        subtitled,   subtitletypes,   videoprop,       "
        "    audioprop,     hdtv,        closecaptioned,  partnumber,      "
        "    parttotal,     seriesid,    originalairdate, colorcode,       "
        "    syndicatedepisodenumber,                                      "
        "                   programid,   listingsource)                    "
        "  SELECT                                                          "
        "    dd_v_program.chanid,                                          "
        "    DATE_ADD(starttime, INTERVAL channel.tmoffset MINUTE),        "
        "    DATE_ADD(endtime, INTERVAL channel.tmoffset MINUTE),          "
        "                                                 title,           "
        "    subtitle,      description, showtype,        dd_genre.class,  "
        "    category_type, airdate,     stars,           previouslyshown, "
        "    stereo,        subtitled,                                     "
        "    (subtitled << 1 ) | closecaptioned, hdtv,                     "
        "    (dolby << 3) | stereo,                                        "
        "                   hdtv,        closecaptioned,  partnumber,      "
        "    parttotal,     seriesid,    originalairdate, colorcode,       "
        "    syndicatedepisodenumber,                                      "
        "                   dd_v_program.programid,                        "
        "                               :LSOURCE                           "
        "FROM (dd_v_program, channel) "
        "LEFT JOIN dd_genre ON "
        "  ( dd_v_program.programid = dd_genre.programid AND  "
        "    dd_genre.relevance     = '0' ) "
        "WHERE dd_v_program.chanid = channel.chanid");

    query.bindValue(":LSOURCE", kListingSourceDDSchedulesDirect);

    if (!query.exec())
        MythDB::DBError("Inserting into program table", query);

    //cerr << "Finished adding rows to main program table...\n";
    //cerr << "Adding program ratings...\n";

    if (!query.exec("INSERT IGNORE INTO programrating (chanid, starttime, "
                    "system, rating) SELECT dd_v_program.chanid, "
                    "DATE_ADD(starttime, INTERVAL channel.tmoffset MINUTE), "
                    " 'MPAA', "
                    "mpaarating FROM dd_v_program, channel WHERE "
                    "mpaarating != '' AND dd_v_program.chanid = "
                    "channel.chanid"))
        MythDB::DBError("Inserting into programrating table", query);

    if (!query.exec("INSERT IGNORE INTO programrating (chanid, starttime, "
                    "system, rating) SELECT dd_v_program.chanid, "
                    "DATE_ADD(starttime, INTERVAL channel.tmoffset MINUTE), "
                    "'VCHIP', "
                    "tvrating FROM dd_v_program, channel WHERE tvrating != ''"
                    " AND dd_v_program.chanid = channel.chanid"))
        MythDB::DBError("Inserting into programrating table", query);

    //cerr << "Finished adding program ratings...\n";
    //cerr << "Populating people table from production crew list...\n";

    if (!query.exec("INSERT IGNORE INTO people (name) "
                    "SELECT fullname "
                    "FROM dd_productioncrew "
                    "LEFT OUTER JOIN people "
                    "ON people.name = dd_productioncrew.fullname "
                    "WHERE people.name IS NULL;"))
        MythDB::DBError("Inserting into people table", query);

    //cerr << "Finished adding people...\n";
    //cerr << "Adding credits entries from production crew list...\n";

    if (!query.exec("INSERT IGNORE INTO credits (chanid, starttime, person, role)"
                    "SELECT dd_v_program.chanid, "
                    "DATE_ADD(dd_v_program.starttime, INTERVAL channel.tmoffset MINUTE), "
                    "people.person, "
                    "dd_productioncrew.role "
                    "FROM dd_v_program "
                    "JOIN channel "
                    "ON dd_v_program.chanid = channel.chanid "
                    "JOIN dd_productioncrew "
                    "ON dd_productioncrew.programid = dd_v_program.programid "
                    "JOIN people "
                    "ON people.name = dd_productioncrew.fullname "
                    "LEFT OUTER JOIN credits "
                    "ON credits.chanid = dd_v_program.chanid "
                    "AND credits.starttime = DATE_ADD(dd_v_program.starttime, INTERVAL channel.tmoffset MINUTE) "
                    "AND credits.person = people.person "
                    "AND credits.role = dd_productioncrew.role "
                    "WHERE credits.role IS NULL;"))
        MythDB::DBError("Inserting into credits table", query);

    //cerr << "Finished inserting credits...\n";
    //cerr << "Adding genres...\n";

    if (!query.exec("INSERT IGNORE INTO programgenres (chanid, starttime, "
                    "relevance, genre) SELECT dd_v_program.chanid, "
                    "DATE_ADD(starttime, INTERVAL channel.tmoffset MINUTE), "
                    "relevance, class FROM dd_v_program, dd_genre, channel "
                    "WHERE (dd_v_program.programid = dd_genre.programid) "
                    "AND dd_v_program.chanid = channel.chanid"))
        MythDB::DBError("Inserting into programgenres table",query);

    //cerr << "Done...\n";
}

FILE *DataDirectProcessor::DDPost(
    QString    ddurl,
    QString    postFilename, QString    inputFile,
    QString    userid,       QString    password,
    QDateTime  pstartDate,   QDateTime  pendDate,
    QString   &err_txt,      bool      &is_pipe)
{
    if (!inputFile.isEmpty())
    {
        err_txt = QString("Unable to open '%1'").arg(inputFile);
        is_pipe = false;
        QByteArray tmp = inputFile.toAscii();
        return fopen(tmp.constData(), "r");
    }

    QFile postfile(postFilename);
    if (!postfile.open(QIODevice::WriteOnly))
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
    poststream << flush;
    postfile.close();

    // Allow for single quotes in userid and password (shell escape)
    password.replace('\'', "'\\''");
    userid.replace('\'', "'\\''");

    QString command;
    {
        QMutexLocker locker(&user_agent_lock);
        command = QString(
            "wget --http-user='%1' --http-passwd='%2' --post-file='%3' "
            " %4 --user-agent='%5' --output-document=- ")
            .arg(userid).arg(password).arg(postFilename).arg(ddurl)
            .arg(user_agent);
    }

#ifdef USING_MINGW
    // Allow for double quotes in userid and password (shell escape)
    command.replace("\"","^\"");
    // Replace single quotes with double-quotes
    command.replace("'", "\"");
    // Unescape unix-escaped single-quotes (which just became "\"")
    command.replace("\"\\\"\"","'");
    // gzip on win32 complains about broken pipe, so don't use it
#else
    // if (!SHOW_WGET_OUTPUT)
    //    command += " 2> /dev/null ";

    command += " --header='Accept-Encoding:gzip' | gzip -df";
#endif

    if (SHOW_WGET_OUTPUT)
        VERBOSE(VB_GENERAL, "command: "<<command<<endl);

    err_txt = command;

    is_pipe = true;
    QByteArray tmp = command.toAscii();
    return popen(tmp.constData(), "r");
}

bool DataDirectProcessor::GrabNextSuggestedTime(void)
{
    VERBOSE(VB_GENERAL, "Grabbing next suggested grabbing time");

    QString ddurl = providers[listings_provider].webServiceURL;

    bool ok;
    QString postFilename = GetPostFilename(ok);
    if (!ok)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "GrabNextSuggestedTime: "
                "Creating temp post file");
        return false;
    }
    QString resultFilename = GetResultFilename(ok);
    if (!ok)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "GrabNextSuggestedTime: "
                "Creating temp result file");
        return false;
    }

    QFile postfile(postFilename);
    if (!postfile.open(QIODevice::WriteOnly))
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + QString("Opening '%1'")
                .arg(postFilename) + ENO);
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
    poststream << flush;
    postfile.close();

    QString command;
    {
        QMutexLocker locker(&user_agent_lock);
        command = QString(
            "wget --http-user='%1' --http-passwd='%2' --post-file='%3' %4 "
            "--user-agent='%5' --output-document='%6'")
            .arg(GetUserID().replace('\'', "'\\''"))
            .arg(GetPassword().replace('\'', "'\\''")).arg(postFilename)
            .arg(ddurl).arg(user_agent).arg(resultFilename);
#ifdef USING_MINGW
        // Allow for double quotes in userid and password (shell escape)
        command.replace("\"","^\"");
        // Replace single quotes with double-quotes
        command.replace("'", "\"");
        // Unescape unix-escaped single-quotes (which just became "\"")
        command.replace("\"\\\"\"","'");
#endif
    }

    if (SHOW_WGET_OUTPUT)
        VERBOSE(VB_GENERAL, "command: "<<command<<endl);
#ifndef USING_MINGW
    else
        command += " 2> /dev/null ";
#endif

    myth_system(command);

    QDateTime NextSuggestedTime;
    QDateTime BlockedTime;

    QFile file(resultFilename);

    bool GotNextSuggestedTime = false;
    bool GotBlockedTime = false;

    if (file.open(QIODevice::ReadOnly))
    {
        QTextStream stream(&file);
        QString line;
        while (!stream.atEnd())
        {
            line = stream.readLine();
            if (line.contains("<suggestedTime>", Qt::CaseInsensitive))
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

            if (line.contains("<blockedTime>", Qt::CaseInsensitive))
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
        int desiredPeriod = gCoreContext->GetNumSetting("MythFillPeriod", 1);


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

	gCoreContext->SaveSettingOnHost("MythFillSuggestedRunTime",
            NextSuggestedTime.toString(Qt::ISODate), NULL);
    }
    return GotNextSuggestedTime;
}

static inline bool close_fp(FILE *&fp, bool fp_is_pipe)
{
    int err;

    if (fp_is_pipe)
        err = pclose(fp);
    else
        err = fclose(fp);

    if (err<0)
        VERBOSE(VB_IMPORTANT, "Failed to close file." + ENO);

    fp = NULL;

    return err>=0;
}

bool DataDirectProcessor::GrabData(const QDateTime pstartDate,
                                   const QDateTime pendDate)
{
    QString msg = (pstartDate.addSecs(1) == pendDate) ? "channel" : "listing";
    VERBOSE(VB_GENERAL, "Grabbing " << msg << " data");

    QString err = "";
    QString ddurl = providers[listings_provider].webServiceURL;
    QString inputfile = inputfilename;
    QString cache_dd_data = QString::null;

    if (cachedata)
    {
        QByteArray userid = GetUserID().toAscii();
        cache_dd_data = tmpDir + QString("/mythtv_dd_cache_%1_%2_UTC_%3_to_%4")
            .arg(GetListingsProvider())
            .arg(userid.constData())
            .arg(pstartDate.toString("yyyyMMddhhmmss"))
            .arg(pendDate.toString("yyyyMMddhhmmss"));

        if (QFile(cache_dd_data).exists() && inputfilename.isEmpty())
        {
            VERBOSE(VB_GENERAL, LOC + "Copying from DD cache");
            inputfile = cache_dd_data;
        }
    }

    bool ok;
    QString postFilename = GetPostFilename(ok);
    if (!ok)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "GrabData: Creating temp post file");
        return false;
    }

    bool fp_is_pipe;
    FILE *fp = DDPost(ddurl, postFilename, inputfile,
                      GetUserID(), GetPassword(),
                      pstartDate, pendDate, err, fp_is_pipe);
    if (!fp)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Failed to get data " +
                QString("(%1) -- ").arg(err) + ENO);
        return false;
    }

    if (cachedata && (inputfile != cache_dd_data))
    {
        QFile in, out(cache_dd_data);
        bool ok = out.open(QIODevice::WriteOnly);
        if (!ok)
        {
            VERBOSE(VB_IMPORTANT, LOC_WARN +
                    "Cannot open DD cache file in '" +
                    tmpDir + "' for writing!");
        }
        else
        {
            VERBOSE(VB_GENERAL, LOC + "Saving listings to DD cache");
            ok = in.open(fp, QIODevice::ReadOnly);
            out.close(); // let copy routine handle dst file
        }

        if (ok)
        {
            ok = copy(out, in);
            in.close();

            close_fp(fp, fp_is_pipe);

            if (ok)
            {
                QByteArray tmp = cache_dd_data.toAscii();
                fp = fopen(tmp.constData(), "r");
                fp_is_pipe = false;
            }
            else
            {
                VERBOSE(VB_IMPORTANT,
                        LOC_ERR + "Failed to save DD cache! "
                        "redownloading data...");
                cachedata = false;
                fp = DDPost(ddurl, postFilename, inputfile,
                            GetUserID(), GetPassword(),
                            pstartDate, pendDate, err, fp_is_pipe);
            }
        }
    }

    if (!fp)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Failed to get data 2 " +
                QString("(%1) -- ").arg(err) + ENO);
        return false;
    }

    ok = true;
    QFile f;
    if (f.open(fp, QIODevice::ReadOnly))
    {
        DDStructureParser ddhandler(*this);
        QXmlInputSource  xmlsource(&f);
        QXmlSimpleReader xmlsimplereader;
        xmlsimplereader.setContentHandler(&ddhandler);
        if (!xmlsimplereader.parse(xmlsource))
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR + "DataDirect XML failed to "
                    "properly parse, downloaded listings were probably "
                    "corrupt.");
            ok = false;
        }
        f.close();
    }
    else
    {
        VERBOSE(VB_GENERAL, LOC_ERR + "Error opening DataDirect file");
        ok = false;
    }

    // f.close() only flushes pipe/file, we need to actually close it.
    close_fp(fp, fp_is_pipe);

    return ok;
}

bool DataDirectProcessor::GrabLineupsOnly(void)
{
    const QDateTime start = QDateTime(QDate::currentDate().addDays(2),
                                      QTime(23, 59, 0));
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
        MythDB::DBError("Creating temporary table", query);

    querystr = "TRUNCATE TABLE " + ptablename + ";";

    if (!query.exec(querystr))
        MythDB::DBError("Truncating temporary table", query);
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
        "( programid char(40),           stationid char(12), "
        "  scheduletime datetime,        duration time,      "
        "  isrepeat bool,                stereo bool,        "
        "  dolby bool, "
        "  subtitled bool,               hdtv bool,          "
        "  closecaptioned bool,          tvrating char(5),   "
        "  partnumber int,               parttotal int,      "
        "  endtime datetime,             isnew bool,         "
        "INDEX progidx (programid) )";

    dd_tables["dd_program"] =
        "( programid char(40) NOT NULL,  seriesid char(12),     "
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
        "  stereo bool,                  dolby bool,                  "
        "  subtitled bool,              "
        "  hdtv bool,                    closecaptioned bool,         "
        "  partnumber int,               parttotal int,               "
        "  seriesid char(12),            originalairdate date,        "
        "  showtype varchar(30),         colorcode varchar(20),       "
        "  syndicatedepisodenumber varchar(20), programid char(40),   "
        "  tvrating char(5),             mpaarating char(5),          "
        "INDEX progidx (programid))";

    dd_tables["dd_productioncrew"] =
        "( programid char(40),           role char(30),    "
        "  givenname char(20),           surname char(20), "
        "  fullname char(41), "
        "INDEX progidx (programid), "
        "INDEX nameidx (fullname))";

    dd_tables["dd_genre"] =
        "( programid char(40) NOT NULL,  class char(30), "
        "  relevance char(1), "
        "INDEX progidx (programid))";

    QMap<QString,QString>::const_iterator it;
    for (it = dd_tables.begin(); it != dd_tables.end(); ++it)
        CreateATempTable(it.key(), *it);
}

bool DataDirectProcessor::GrabLoginCookiesAndLineups(bool parse_lineups)
{
    VERBOSE(VB_GENERAL, "Grabbing login cookies and lineups");

    PostList list;
    list.push_back(PostItem("username", GetUserID()));
    list.push_back(PostItem("password", GetPassword()));
    list.push_back(PostItem("action",   "Login"));

    QString labsURL   = providers[listings_provider].webURL;
    QString loginPage = providers[listings_provider].loginPage;

    bool ok;
    QString resultFilename = GetResultFilename(ok);
    if (!ok)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "GrabLoginCookiesAndLineups: "
                "Creating temp result file");
        return false;
    }
    QString cookieFilename = GetCookieFilename(ok);
    if (!ok)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "GrabLoginCookiesAndLineups: "
                "Creating temp cookie file");
        return false;
    }

    ok = Post(labsURL + loginPage, list, resultFilename, "",
              cookieFilename);

    bool got_cookie = QFileInfo(cookieFilename).size() > 100;

    ok &= got_cookie && (!parse_lineups || ParseLineups(resultFilename));
    if (ok)
        cookieFileDT = QDateTime::currentDateTime();

    return ok;
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

    bool ok;
    QString resultFilename = GetResultFilename(ok);
    if (!ok)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "GrabLoginCookiesAndLineups: "
                "Creating temp result file");
        return false;
    }
    QString cookieFilename = GetCookieFilename(ok);
    if (!ok)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "GrabLoginCookiesAndLineups: "
                "Creating temp cookie file");
        return false;
    }

    QString labsURL = providers[listings_provider].webURL;
    ok = Post(labsURL + (*it).get_action, list, resultFilename,
                   cookieFilename, "");

    return ok && ParseLineup(lineupid, resultFilename);
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

static QString get_cache_filename(const QString &lineupid)
{
    return QString("/tmp/.mythtv_cached_lineup_") + lineupid;
}

QDateTime DataDirectProcessor::GetLineupCacheAge(const QString &lineupid) const
{
    QDateTime cache_dt(QDate(1971, 1, 1));
    QFile lfile(get_cache_filename(lineupid));
    if (!lfile.exists())
    {
        VERBOSE(VB_GENERAL, "GrabLineupCacheAge("<<lineupid<<") failed -- "
                <<QString("file '%1' doesn't exist")
                .arg(get_cache_filename(lineupid)));
        return cache_dt;
    }
    if (lfile.size() < 8)
    {
        VERBOSE(VB_IMPORTANT, "GrabLineupCacheAge("<<lineupid<<") failed -- "
                <<QString("file '%1' size %2 too small")
                .arg(get_cache_filename(lineupid)).arg(lfile.size()));
        return cache_dt;
    }
    if (!lfile.open(QIODevice::ReadOnly))
    {
        VERBOSE(VB_IMPORTANT, "GrabLineupCacheAge("<<lineupid<<") failed -- "
                <<QString("cannot open file '%1'")
                .arg(get_cache_filename(lineupid)));
        return cache_dt;
    }

    QString tmp;
    QTextStream io(&lfile);
    io >> tmp;
    cache_dt = QDateTime::fromString(tmp, Qt::ISODate);

    VERBOSE(VB_GENERAL, "GrabLineupCacheAge("<<lineupid<<") -> "
            <<cache_dt.toString(Qt::ISODate));

    return cache_dt;
}

bool DataDirectProcessor::GrabLineupsFromCache(const QString &lineupid)
{
    QFile lfile(get_cache_filename(lineupid));
    if (!lfile.exists() || (lfile.size() < 8) || !lfile.open(QIODevice::ReadOnly))
    {
        VERBOSE(VB_IMPORTANT, "GrabLineupFromCache("<<lineupid<<") -- failed");
        return false;
    }

    QString tmp;
    uint size;
    QTextStream io(&lfile);
    io >> tmp; // read in date
    io >> size; // read in number of channels mapped

    for (uint i = 0; i < 14; i++)
        io.readLine(); // read extra lines

    DDLineupChannels &channels = lineupmaps[lineupid];
    channels.clear();

    for (uint i = 0; i < size; i++)
    {
        io.readLine(); // read "start record" string

        DataDirectLineupMap chan;
        chan.lineupid     = lineupid;
        chan.stationid    = io.readLine();
        chan.channel      = io.readLine();
        chan.channelMinor = io.readLine();

        chan.mapFrom = QDate();
        tmp = io.readLine();
        if (!tmp.isEmpty())
            chan.mapFrom.fromString(tmp, Qt::ISODate);

        chan.mapTo = QDate();
        tmp = io.readLine();
        if (!tmp.isEmpty())
            chan.mapTo.fromString(tmp, Qt::ISODate);

        channels.push_back(chan);

        DDStation station;
        station.stationid   = chan.stationid;
        station.callsign    = io.readLine();
        station.stationname = io.readLine();
        station.affiliate   = io.readLine();
        station.fccchannelnumber = io.readLine();
        tmp = io.readLine(); // read "end record" string

        stations[station.stationid] = station;
    }

    VERBOSE(VB_GENERAL, "GrabLineupFromCache("<<lineupid<<") -- success");

    return true;
}

bool DataDirectProcessor::SaveLineupToCache(const QString &lineupid) const
{
    QString fn = get_cache_filename(lineupid);
    QByteArray fna = fn.toAscii();
    QFile lfile(fna.constData());
    if (!lfile.open(QIODevice::WriteOnly))
    {
        VERBOSE(VB_IMPORTANT, "SaveLineupToCache("<<lineupid<<") -- failed");
        return false;
    }

    QTextStream io(&lfile);
    io << QDateTime::currentDateTime().toString(Qt::ISODate) << endl;

    const DDLineupChannels channels = GetDDLineup(lineupid);
    io << channels.size() << endl;

    io << endl;
    io << "# start record"       << endl;
    io << "#   stationid"        << endl;
    io << "#   channel"          << endl;
    io << "#   channelMinor"     << endl;
    io << "#   mapped from date" << endl;
    io << "#   mapped to date"   << endl;
    io << "#   callsign"         << endl;
    io << "#   stationname"      << endl;
    io << "#   affiliate"        << endl;
    io << "#   fccchannelnumber" << endl;
    io << "# end record"         << endl;
    io << endl;

    DDLineupChannels::const_iterator it;
    for (it = channels.begin(); it != channels.end(); ++it)
    {
        io << "# start record"    << endl;
        io << (*it).stationid     << endl;
        io << (*it).channel       << endl;
        io << (*it).channelMinor  << endl;
        io << (*it).mapFrom.toString(Qt::ISODate) << endl;
        io << (*it).mapTo.toString(Qt::ISODate)   << endl;

        DDStation station = GetDDStation((*it).stationid);
        io << station.callsign    << endl;
        io << station.stationname << endl;
        io << station.affiliate   << endl;
        io << station.fccchannelnumber << endl;
        io << "# end record"      << endl;
    }
    io << flush;

    VERBOSE(VB_GENERAL, "SaveLineupToCache("<<lineupid<<") -- success");

    makeFileAccessible(fna.constData()); // Let anybody update it

    return true;
}

bool DataDirectProcessor::GrabFullLineup(const QString &lineupid,
                                         bool restore, bool onlyGrabSelected,
                                         uint cache_age_allowed_in_seconds)
{
    if (cache_age_allowed_in_seconds)
    {
        QDateTime exp_time = GetLineupCacheAge(lineupid)
            .addSecs(cache_age_allowed_in_seconds);
        bool valid = exp_time > QDateTime::currentDateTime();
        if (valid && GrabLineupsFromCache(lineupid))
            return true;
    }

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

    if (!onlyGrabSelected)
    {
        SetAll(lineupid, true);
        if (!SaveLineupChanges(lineupid))
            return false;
    }

    ok = GrabLineupsOnly();

    if (ok)
        SaveLineupToCache(lineupid);

    (*lit).channels = orig_channels;
    if (restore && !onlyGrabSelected)
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

    // Grab login cookies if they are more than 5 minutes old
    if ((!cookieFileDT.isValid() ||
         cookieFileDT.addSecs(5*60) < QDateTime::currentDateTime()) &&
        !GrabLoginCookiesAndLineups(false))
    {
        return false;
    }

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

    bool ok;
    QString cookieFilename = GetCookieFilename(ok);
    if (!ok)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "GrabLoginCookiesAndLineups: "
                "Creating temp cookie file");
        return false;
    }

    QString labsURL = providers[listings_provider].webURL;
    return Post(labsURL + lineup.set_action, list, "",
                cookieFilename, "");
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
        MythDB::DBError("Selecting existing channels", query);
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
        VERBOSE(VB_GENERAL, "Failed to update DataDirect listings.");

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

void DataDirectProcessor::CreateTemp(
    const QString &templatefilename,
    const QString &errmsg,
    bool           directory,
    QString       &filename,
    bool          &ok) const
{
    QString tmp = createTempFile(templatefilename, directory);
    if (templatefilename == tmp)
    {
        fatalErrors.push_back(errmsg);
        ok = false;
    }
    else
    {
        filename = tmp;
        ok = true;
    }
}

QString DataDirectProcessor::CreateTempDirectory(bool *pok)
{
    bool ok;
    pok = (pok) ? pok : &ok;
    if (tmpDir == "/tmp")
    {
        CreateTemp("/tmp/mythtv_ddp_XXXXXX",
                   "Failed to create temp directory",
                   true, tmpDir, *pok);
    }
    return tmpDir;
}

QString DataDirectProcessor::GetPostFilename(bool &ok) const
{
    ok = true;
    if (tmpPostFile.isEmpty())
    {
        CreateTemp(tmpDir + "/mythtv_post_XXXXXX",
                   "Failed to create temp post file",
                   false, tmpPostFile, ok);
    }
    return tmpPostFile;
}

QString DataDirectProcessor::GetResultFilename(bool &ok) const
{
    ok = true;
    if (tmpResultFile.isEmpty())
    {
        CreateTemp(tmpDir + "/mythtv_result_XXXXXX",
                   "Failed to create temp result file",
                   false, tmpResultFile, ok);
    }
    return tmpResultFile;
}

QString DataDirectProcessor::GetCookieFilename(bool &ok) const
{
    ok = true;
    if (cookieFile.isEmpty())
    {
        CreateTemp(tmpDir + "/mythtv_cookies_XXXXXX",
                   "Failed to create temp cookie file",
                   false, cookieFile, ok);
    }
    return cookieFile;
}

void DataDirectProcessor::SetUserID(const QString &uid)
{
    userid = uid;
    userid.detach();
}

void DataDirectProcessor::SetPassword(const QString &pwd)
{
    password = pwd;
    password.detach();
}

void DataDirectProcessor::SetInputFile(const QString &file)
{
    inputfilename = file;
    inputfilename.detach();
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

    {
        QMutexLocker locker(&user_agent_lock);
        command += QString("--user-agent='%1' ").arg(user_agent);
    }

    command += "--output-document=";
    command += (documentFile.isEmpty()) ? "- " : dfile;

#ifdef USING_MINGW
    command.replace("'", "\"");
#endif

    if (SHOW_WGET_OUTPUT)
        VERBOSE(VB_GENERAL, "command: "<<command<<endl);
    else
    {
        command += (documentFile.isEmpty()) ? "&> " : "2> ";
        command += "/dev/null ";
    }

    myth_system(command);

    if (documentFile.isEmpty())
        return true;

    QFileInfo fi(documentFile);
    return fi.size();
}

bool DataDirectProcessor::ParseLineups(const QString &documentFile)
{
    QFile file(documentFile);
    if (!file.open(QIODevice::ReadOnly))
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
        QString llow = line.toLower();
        int frm = llow.indexOf("<form");
        if (frm >= 0)
        {
            in_form = true;
            get_action = get_setting(line.mid(frm + 5), "action");
            name_value.clear();
            //cerr<<QString("action: %1").arg(action)<<endl;
        }

        if (!in_form)
            continue;

        int inp = llow.indexOf("<input");
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
    if (!file.open(QIODevice::ReadOnly))
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
        QString llow = line.toLower();
        int frm = llow.indexOf("<form");
        if (frm >= 0)
        {
            in_form = true;
            lineup.set_action = get_setting(line.mid(frm + 5), "action");
            //cerr<<"set_action: "<<lineup.set_action<<endl;
        }

        if (!in_form)
            continue;

        int inp = llow.indexOf("<input");
        if (inp >= 0)
        {
            QString in_line = line.mid(inp + 6);
            settings.clear();
            settings["chk_name"]    = get_setting(in_line, "name");
            settings["chk_id"]      = get_setting(in_line, "id");
            settings["chk_value"]   = get_setting(in_line, "value");
            settings["chk_checked"] = has_setting(in_line, "checked")?"1":"0";
        }

        int lbl = llow.indexOf("<label");
        if (lbl >= 0)
        {
            QString lbl_line = line.mid(inp + 6);
            QString name = get_setting(lbl_line, "for");
            in_label = (name == settings["chk_name"]) ? 1 : 0;
        }

        if (in_label)
        {
            int start = (lbl >= 0) ? lbl + 6 : 0;
            int beg = llow.indexOf("<td>", start), end = -1;
            if (beg)
                end = llow.indexOf("</td>", beg + 4);

            if (end >= 0)
            {
                QString key = (in_label == 1) ? "lbl_ch" : "lbl_callsign";
                QString val = line.mid(beg + 4, end - beg - 4);
                settings[key] = val.replace("&nbsp;", "", Qt::CaseInsensitive);
                in_label++;
            }
        }

        in_label = (llow.indexOf("</label") >= 0) ? 0 : in_label;

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
    for (int i = 0; i < str.length(); i++)
    {
        if (str[i].isLetterOrNumber())
            new_str += str[i];
        else
        {
            new_str += QString("\%%1").arg((int)str[1].toLatin1(), 0, 16);
        }
    }

    return new_str;
}

static QString get_setting(QString line, QString key)
{
    QString llow = line.toLower();
    QString kfind = key + "=\"";
    int beg = llow.indexOf(kfind), end = -1;

    if (beg >= 0)
    {
        end = llow.indexOf("\"", beg + kfind.length());
        return line.mid(beg + kfind.length(), end - beg - kfind.length());
    }

    kfind = key + "=";
    beg = llow.indexOf(kfind);
    if (beg < 0)
        return QString::null;

    int i = beg + kfind.length();
    while (i < line.length() && !line[i].isSpace() && line[i] != '>')
        i++;

    if (i < line.length() && (line[i].isSpace() || line[i] == '>'))
        return line.mid(beg + kfind.length(), i - beg - kfind.length());

    return QString::null;
}

static bool has_setting(QString line, QString key)
{
    return (line.toLower().indexOf(key) >= 0);
}

static void get_atsc_stuff(QString channum, int sourceid, int freqid,
                           int &major, int &minor, long long &freq)
{
    major = freqid;
    minor = 0;

    int chansep = channum.indexOf(QRegExp("\\D"));
    if (chansep < 0)
        return;

    major = channum.left(chansep).toInt();
    minor = channum.right(channum.length() - (chansep + 1)).toInt();

    freq = get_center_frequency("atsc", "vsb8", "us", freqid);
}

static QString process_dd_station(
    uint sourceid, QString chan_major, QString chan_minor,
    QString &tvformat, uint &freqid)
{
    QString channum = chan_major;
    bool ok;
    uint minor = chan_minor.toUInt(&ok);

    tvformat = "Default";

    if (minor && ok)
    {
        tvformat = "atsc";
        channum += SourceUtil::GetChannelSeparator(sourceid) + chan_minor;
    }
    else if (!freqid && (get_lineup_type(sourceid) == "LocalBroadcast"))
        freqid = chan_major.toInt();
    else
        freqid = channum.toInt();

    return channum;
}

static uint update_channel_basic(uint    sourceid,   bool    insert,
                                 QString xmltvid,    QString callsign,
                                 QString name,       uint    freqid,
                                 QString chan_major, QString chan_minor)
{
    callsign = (callsign.isEmpty()) ? name : callsign;

    QString tvformat;
    QString channum = process_dd_station(
        sourceid, chan_major, chan_minor, tvformat, freqid);

    // First check if channel already in DB, but without xmltvid
    MSqlQuery query(MSqlQuery::DDCon());
    query.prepare("SELECT chanid, callsign, name "
                  "FROM channel "
                  "WHERE sourceid = :SOURCEID AND "
                  "      ( xmltvid = '0' OR xmltvid = '') AND "
                  "      ( channum = :CHANNUM OR "
                  "        ( freqid  = :FREQID AND "
                  "          freqid != '0'     AND "
                  "          freqid != ''      AND "
                  "          atsc_minor_chan = '0') OR "
                  "        ( atsc_major_chan = :MAJORCHAN AND "
                  "          atsc_minor_chan = :MINORCHAN ) )");
    query.bindValue(":SOURCEID",  sourceid);
    query.bindValue(":CHANNUM",   channum);
    query.bindValue(":FREQID",    freqid);
    query.bindValue(":MAJORCHAN", chan_major.toUInt());
    query.bindValue(":MINORCHAN", chan_minor.toUInt());

    if (!query.exec() || !query.isActive())
    {
        MythDB::DBError("Getting chanid of existing channel", query);
        return 0; // go on to next channel without xmltv
    }

    if (query.next())
    {
        // The channel already exists in DB, at least once,
        // so set the xmltvid..
        MSqlQuery chan_update_q(MSqlQuery::DDCon());
        chan_update_q.prepare(
            "UPDATE channel "
            "SET xmltvid = :XMLTVID, name = :NAME, callsign = :CALLSIGN "
            "WHERE chanid = :CHANID AND sourceid = :SOURCEID");

        uint i = 0;
        do
        {
            uint chanid = query.value(0).toInt();

            QString new_callsign = query.value(1).toString();
            new_callsign =
                (new_callsign.indexOf(ChannelUtil::GetUnknownCallsign()) == 0) ?
                callsign : new_callsign;

            QString new_name = query.value(2).toString();
            new_name = (new_name.isEmpty()) ? name         : new_name;
            new_name = (new_name.isEmpty()) ? new_callsign : new_name;

            chan_update_q.bindValue(":CHANID",   chanid);
            chan_update_q.bindValue(":NAME",     new_name);
            chan_update_q.bindValue(":CALLSIGN", new_callsign);
            chan_update_q.bindValue(":XMLTVID",  xmltvid);
            chan_update_q.bindValue(":SOURCEID", sourceid);

#if 0
            VERBOSE(VB_GENERAL, LOC +
                    QString("Updating channel %1: '%2' (%3).")
                    .arg(chanid).arg(name).arg(callsign));
#endif

            if (!chan_update_q.exec() || !chan_update_q.isActive())
            {
                MythDB::DBError(
                    "Updating XMLTVID of existing channel", chan_update_q);
                continue; // go on to next instance of this channel
            }
            i++;
        }
        while (query.next());

        return i; // go on to next channel without xmltv
    }

    if (!insert)
        return 0; // go on to next channel without xmltv

    // The channel doesn't exist in the DB, insert it...
    int mplexid = -1, majorC, minorC, chanid = 0;
    long long freq = -1;
    get_atsc_stuff(channum, sourceid, freqid,
                   majorC, minorC, freq);

    if (minorC > 0 && freq >= 0)
        mplexid = ChannelUtil::CreateMultiplex(sourceid, "atsc", freq, "8vsb");

    if ((mplexid > 0) || (minorC == 0))
        chanid = ChannelUtil::CreateChanID(sourceid, channum);

    VERBOSE(VB_GENERAL, LOC + QString("Adding channel %1 '%2' (%3).")
            .arg(channum).arg(name).arg(callsign));

    if (chanid > 0)
    {
        QString icon   = "";
        int  serviceid = 0;
        bool oag       = false; // use on air guide
        bool hidden    = false;
        bool hidden_in_guide = false;
        QString freq_id= QString::number(freqid);

        ChannelUtil::CreateChannel(
            mplexid,   sourceid,  chanid,
            callsign,  name,      channum,
            serviceid, majorC,    minorC,
            oag,       hidden,    hidden_in_guide,
            freq_id,   icon,      tvformat,
            xmltvid);
    }

    return 1;
}

static void set_lineup_type(const QString &lineupid, const QString &type)
{
    QMutexLocker locker(&lineup_type_lock);
    if (lineupid_to_srcid[lineupid])
        return;

    // get lineup to source mapping
    uint srcid = 0;
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "SELECT sourceid "
        "FROM videosource "
        "WHERE lineupid = :LINEUPID");
    query.bindValue(":LINEUPID", lineupid);

    if (!query.exec() || !query.isActive())
        MythDB::DBError("end_element", query);
    else if (query.next())
        srcid = query.value(0).toUInt();

    if (srcid)
    {
        QString tmpid = lineupid;
        tmpid.detach();
        lineupid_to_srcid[tmpid] = srcid;

        // set type for source
        QString tmptype = type;
        tmptype.detach();
        srcid_to_type[srcid] = tmptype;

        VERBOSE(VB_GENERAL, "sourceid "<<srcid<<" has lineup type: "<<type);
    }
}

static QString get_lineup_type(uint sourceid)
{
    QMutexLocker locker(&lineup_type_lock);
    QString ret = srcid_to_type[sourceid];
    ret.detach();
    return ret;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
