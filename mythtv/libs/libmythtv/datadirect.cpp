#include <unistd.h>
#include <zlib.h>

// Qt headers
#include <QDir>
#include <QFileInfo>
#include <QByteArray>
#include <QNetworkReply>
#include <QAuthenticator>

// MythTV headers
#include "datadirect.h"
#include "sourceutil.h"
#include "channelutil.h"
#include "frequencytables.h"
#include "listingsources.h"
#include "mythmiscutil.h"
#include "mythcontext.h"
#include "mythdb.h"
#include "mythlogging.h"
#include "mythversion.h"
#include "mythdate.h"
#include "dbutil.h"
#include "mythsystemlegacy.h"
#include "exitcodes.h"
#include "mythdownloadmanager.h"
#include "mythtvexp.h"

#define LOC QString("DataDirect: ")

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
void authenticationCallback(QNetworkReply *reply, QAuthenticator *auth,
                            void *arg);
QByteArray gUncompress(const QByteArray &data);

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
    time(),                     duration(),
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
    starRating(""), duration(),        year(""),
    showtype(""),   colorcode(""),     originalAirDate(),
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
        QDateTime begts = MythDate::fromString(beg);
        parent.SetDDProgramsStartAt(begts);

        QString   end   = pxmlatts.value("to");
        QDateTime endts = MythDate::fromString(end);
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

        curr_schedule.time = MythDate::fromString(pxmlatts.value("time"));
        QString durstr = pxmlatts.value("duration");
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
        parent.m_stations[curr_station.stationid] = curr_station;

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

        parent.m_lineups.push_back(curr_lineup);

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
        parent.m_lineupmaps[curr_lineupmap.lineupid].push_back(curr_lineupmap);

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
#if 0
    LOG(VB_GENERAL, LOG_DEBUG, LOC + "Characters : " + pchars);
#endif
    if (pchars.trimmed().isEmpty())
        return true;

    if (currtagname == "message")
    {
        if (pchars.contains("expire"))
        {
            QString ExtractDateFromMessage = pchars.right(20);
            QDateTime EDFM = MythDate::fromString(ExtractDateFromMessage);
            QString SDDateFormat = GetMythDB()->GetSetting("DateFormat",
                                                           "ddd d MMMM");
            // Ensure we show the year when it's important, regardless of
            // specified DateFormat
            if ((!SDDateFormat.contains('y')) &&
                (EDFM.date().year() != MythDate::current().date().year()))
            {
                SDDateFormat.append(" (yyyy)");
            }
            QString dateFormat = QString("%1 %2")
                    .arg(SDDateFormat)
                    .arg(GetMythDB()->GetSetting("TimeFormat", "hh:mm"));
            QString ExpirationDate = EDFM.toString(dateFormat);

            QString ExpirationDateMessage = "Your subscription expires on " +
                ExpirationDate;

            QDateTime curTime = MythDate::current();
            if (curTime.daysTo(EDFM) <= 5)
            {
                LOG(VB_GENERAL, LOG_WARNING, LOC + QString("WARNING: ") +
                        ExpirationDateMessage);
            }
            else
            {
                LOG(VB_GENERAL, LOG_INFO, LOC + ExpirationDateMessage);
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

DataDirectProcessor::DataDirectProcessor(uint lp, QString user, QString pass) :
    m_listingsProvider(lp % DD_PROVIDER_COUNT),
    m_userid(user),                   m_password(pass),
    m_tmpDir("/tmp"),                 m_cacheData(false),
    m_inputFilename(""),              m_tmpPostFile(QString::null),
    m_tmpResultFile(QString::null),   m_cookieFile(QString::null),
    m_cookieFileDT()
{
    {
        QMutexLocker locker(&user_agent_lock);
        user_agent = QString("MythTV/%1.%2")
            .arg(MYTH_BINARY_VERSION).arg(MYTH_SOURCE_VERSION);
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
    m_providers.push_back(urls0);
    m_providers.push_back(urls1);
}

DataDirectProcessor::~DataDirectProcessor()
{
    LOG(VB_GENERAL, LOG_INFO, LOC + "Deleting temporary files");

    if (!m_tmpPostFile.isEmpty())
    {
        QByteArray tmp = m_tmpPostFile.toLatin1();
        unlink(tmp.constData());
    }

    if (!m_tmpResultFile.isEmpty())
    {
        QByteArray tmp = m_tmpResultFile.toLatin1();
        unlink(tmp.constData());
    }

    if (!m_cookieFile.isEmpty())
    {
        QByteArray tmp = m_cookieFile.toLatin1();
        unlink(tmp.constData());
    }

    QDir d(m_tmpDir, "mythtv_dd_cache_*", QDir::Name,
           QDir::Files | QDir::NoSymLinks);

    for (uint i = 0; i < d.count(); i++)
    {
        QString    tmps = QString(m_tmpDir + "/" + d[i]);
        QByteArray tmpa = tmps.toLatin1();
        unlink(tmpa.constData());
    }

    if (m_tmpDir != "/tmp")
    {
        QByteArray tmp = m_tmpDir.toLatin1();
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
        LOG(VB_GENERAL, LOG_WARNING, LOC +
            QString("Not inserting channels into disconnected source %1.")
                .arg(sourceid));
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

    bool is_encoder = (SourceUtil::IsCableCardPresent(sourceid) ||
                       SourceUtil::IsEncoder(sourceid, true) ||
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
            LOG(VB_GENERAL, LOG_INFO, LOC +
                QString("Not adding channel %1-%2 '%3' (%4),\n\t\t\t"
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
            LOG(VB_GENERAL, LOG_INFO, LOC +
                QString("Not adding channel '%1' (%2).")
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

    bool is_encoder = (SourceUtil::IsCableCardPresent(sourceid) ||
                       SourceUtil::IsEncoder(sourceid, true) ||
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
            LOG(VB_GENERAL, LOG_INFO, LOC +
                QString("Not adding channel %1-%2 '%3' (%4),\n\t\t\t"
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
            MythDB::DBError("Updating channel table", chan_update_q);
        }
    }

    return true;
}

void DataDirectProcessor::DataDirectProgramUpdate(void)
{
    MSqlQuery query(MSqlQuery::DDCon());

#if 0
    LOG(VB_GENERAL, LOG_DEBUG, LOC +
        "Adding rows to main program table from view table");
#endif
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

#if 0
    LOG(VB_GENERAL, LOG_DEBUG, LOC +
        "Finished adding rows to main program table");
    LOG(VB_GENERAL, LOG_DEBUG, LOC + "Adding program ratings");
#endif

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

#if 0
    LOG(VB_GENERAL, LOG_DEBUG, LOC + "Finished adding program ratings");
    LOG(VB_GENERAL, LOG_DEBUG, LOC +
        "Populating people table from production crew list");
#endif

    if (!query.exec("INSERT IGNORE INTO people (name) "
                    "SELECT fullname "
                    "FROM dd_productioncrew "
                    "LEFT OUTER JOIN people "
                    "ON people.name = dd_productioncrew.fullname "
                    "WHERE people.name IS NULL;"))
        MythDB::DBError("Inserting into people table", query);

#if 0
    LOG(VB_GENERAL, LOG_INFO, LOC + "Finished adding people");
    LOG(VB_GENERAL, LOG_INFO, LOC +
        "Adding credits entries from production crew list");
#endif

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

#if 0
    LOG(VB_GENERAL, LOG_DEBUG, LOC + "Finished inserting credits");
    LOG(VB_GENERAL, LOG_DEBUG, LOC + "Adding genres");
#endif

    if (!query.exec("INSERT IGNORE INTO programgenres (chanid, starttime, "
                    "relevance, genre) SELECT dd_v_program.chanid, "
                    "DATE_ADD(starttime, INTERVAL channel.tmoffset MINUTE), "
                    "relevance, class FROM dd_v_program, dd_genre, channel "
                    "WHERE (dd_v_program.programid = dd_genre.programid) "
                    "AND dd_v_program.chanid = channel.chanid"))
        MythDB::DBError("Inserting into programgenres table",query);

#if 0
    LOG(VB_GENERAL, LOG_DEBUG, LOC + "Done");
#endif
}

void authenticationCallback(QNetworkReply *reply, QAuthenticator *auth,
                            void *arg)
{
    if (!arg)
        return;

    DataDirectProcessor *dd = reinterpret_cast<DataDirectProcessor *>(arg);
    dd->authenticationCallback(reply, auth);
}

void DataDirectProcessor::authenticationCallback(QNetworkReply *reply,
                                                 QAuthenticator *auth)
{
    LOG(VB_FILE, LOG_DEBUG, "DataDirect auth callback");
    (void)reply;
    auth->setUser(GetUserID());
    auth->setPassword(GetPassword());
}

bool DataDirectProcessor::DDPost(QString    ddurl,        QString   &inputFile,
                                 QDateTime  pstartDate,   QDateTime  pendDate,
                                 QString   &err_txt)
{
    if (!inputFile.isEmpty() && QFile(inputFile).exists())
    {
        return true;
    }

    QString startdatestr = pstartDate.toString(Qt::ISODate) + "Z";
    QString enddatestr = pendDate.toString(Qt::ISODate) + "Z";
    QByteArray postdata;
    postdata  = "<?xml version='1.0' encoding='utf-8'?>\n";
    postdata += "<SOAP-ENV:Envelope\n";
    postdata += "xmlns:SOAP-ENV='http://schemas.xmlsoap.org/soap/envelope/'\n";
    postdata += "xmlns:xsd='http://www.w3.org/2001/XMLSchema'\n";
    postdata += "xmlns:xsi='http://www.w3.org/2001/XMLSchema-instance'\n";
    postdata += "xmlns:SOAP-ENC='http://schemas.xmlsoap.org/soap/encoding/'>\n";
    postdata += "<SOAP-ENV:Body>\n";
    postdata += "<ns1:download  xmlns:ns1='urn:TMSWebServices'>\n";
    postdata += "<startTime xsi:type='xsd:dateTime'>";
    postdata += startdatestr;
    postdata += "</startTime>\n";
    postdata += "<endTime xsi:type='xsd:dateTime'>";
    postdata += enddatestr;
    postdata += "</endTime>\n";
    postdata += "</ns1:download>\n";
    postdata += "</SOAP-ENV:Body>\n";
    postdata += "</SOAP-ENV:Envelope>\n";

    if (inputFile.isEmpty()) {
        inputFile = QString("/tmp/mythtv_ddp_data");
    }

    QHash<QByteArray, QByteArray> headers;
    headers.insert("Accept-Encoding", "gzip");
    headers.insert("Content-Type", "application/soap+xml; charset=utf-8");

    LOG(VB_GENERAL, LOG_INFO, "Downloading DataDirect feed");

    MythDownloadManager *manager = GetMythDownloadManager();

    if (!manager->postAuth(ddurl, &postdata, &::authenticationCallback, this,
                           &headers))
    {
        err_txt = QString("Download error");
        return false;
    }

    LOG(VB_GENERAL, LOG_INFO, QString("Downloaded %1 bytes")
        .arg(postdata.size()));

    LOG(VB_GENERAL, LOG_INFO, "Uncompressing DataDirect feed");

    QByteArray uncompressed = gUncompress(postdata);

    LOG(VB_GENERAL, LOG_INFO, QString("Uncompressed to %1 bytes")
        .arg(uncompressed.size()));

    if (uncompressed.size() == 0)
        uncompressed = postdata;

    QFile file(inputFile);
    file.open(QIODevice::WriteOnly);
    file.write(uncompressed);
    file.close();

    if (uncompressed.size() == 0)
    {
        err_txt = QString("Error uncompressing data");
        return false;
    }

    return true;
}

bool DataDirectProcessor::GrabNextSuggestedTime(void)
{
    LOG(VB_GENERAL, LOG_INFO, LOC + "Grabbing next suggested grabbing time");

    QString ddurl = m_providers[m_listingsProvider].webServiceURL;

    bool ok;
    QString resultFilename = GetResultFilename(ok);
    if (!ok)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            "GrabNextSuggestedTime: Creating temp result file");
        return false;
    }

    QByteArray postdata;
    postdata  = "<?xml version='1.0' encoding='utf-8'?>\n";
    postdata += "<SOAP-ENV:Envelope\n";
    postdata += "xmlns:SOAP-ENV='http://schemas.xmlsoap.org/soap/envelope/'\n";
    postdata += "xmlns:xsd='http://www.w3.org/2001/XMLSchema'\n";
    postdata += "xmlns:xsi='http://www.w3.org/2001/XMLSchema-instance'\n";
    postdata += "xmlns:SOAP-ENC='http://schemas.xmlsoap.org/soap/encoding/'>\n";
    postdata += "<SOAP-ENV:Body>\n";
    postdata += "<tms:acknowledge xmlns:tms='urn:TMSWebServices'>\n";
    postdata += "</SOAP-ENV:Body>\n";
    postdata += "</SOAP-ENV:Envelope>\n";

    QHash<QByteArray, QByteArray> headers;
    headers.insert("Content-Type", "application/soap+xml; charset=utf-8");

    MythDownloadManager *manager = GetMythDownloadManager();

    if (!manager->postAuth(ddurl, &postdata, &::authenticationCallback, this,
                           &headers))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            "GrabNextSuggestedTime: Could not download");
        return false;
    }

    QDateTime nextSuggestedTime;
    QDateTime blockedTime;

    LOG(VB_GENERAL, LOG_INFO, QString("Suggested Time data: %1 bytes")
        .arg(postdata.size()));

    QFile file(resultFilename);
    file.open(QIODevice::WriteOnly);
    file.write(postdata);
    file.close();

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

                nextSuggestedTime = MythDate::fromString(tmpStr);

                LOG(VB_GENERAL, LOG_INFO, LOC +
                    QString("nextSuggestedTime is: ") +
                    nextSuggestedTime.toString(Qt::ISODate));
            }

            if (line.contains("<blockedTime>", Qt::CaseInsensitive))
            {
                QString tmpStr = line;
                tmpStr.replace(
                    QRegExp(".*<blockedTime>([^<]*)</blockedTime>.*"), "\\1");

                blockedTime = MythDate::fromString(tmpStr);
                LOG(VB_GENERAL, LOG_INFO, LOC + QString("BlockedTime is: ") +
                    blockedTime.toString(Qt::ISODate));
            }
        }
        file.close();
    }

    if (nextSuggestedTime.isValid())
    {
        gCoreContext->SaveSettingOnHost(
            "MythFillSuggestedRunTime",
            nextSuggestedTime.toString(Qt::ISODate), NULL);
    }

    return nextSuggestedTime.isValid();
}

bool DataDirectProcessor::GrabData(const QDateTime &pstartDate,
                                   const QDateTime &pendDate)
{
    QString msg = (pstartDate.addSecs(1) == pendDate) ? "channel" : "listing";
    LOG(VB_GENERAL, LOG_INFO, LOC + "Grabbing " + msg + " data");

    QString err = "";
    QString ddurl = m_providers[m_listingsProvider].webServiceURL;
    QString inputfile = m_inputFilename;
    QString cache_dd_data = QString::null;

    if (m_cacheData)
    {
        QByteArray userid = GetUserID().toLatin1();
        cache_dd_data = m_tmpDir +
            QString("/mythtv_dd_cache_%1_%2_UTC_%3_to_%4")
            .arg(GetListingsProvider())
            .arg(userid.constData())
            .arg(MythDate::toString(pstartDate, MythDate::kFilename))
            .arg(MythDate::toString(pendDate, MythDate::kFilename));

        if (QFile(cache_dd_data).exists() && m_inputFilename.isEmpty())
        {
            LOG(VB_GENERAL, LOG_INFO, LOC + "Using DD cache");
        }

        if (m_inputFilename.isEmpty())
            inputfile = cache_dd_data;
    }

    if (!DDPost(ddurl, inputfile, pstartDate, pendDate, err))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + QString("Failed to get data: %1")
                .arg(err));
        return false;
    }

    QFile file(inputfile);
    file.open(QIODevice::ReadOnly);
    QByteArray data = file.readAll();
    file.close();

    if (data.isEmpty())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Data is empty");
        return false;
    }

    bool ok = true;

    DDStructureParser ddhandler(*this);
    QXmlInputSource  xmlsource;
    QXmlSimpleReader xmlsimplereader;

    xmlsource.setData(data);
    xmlsimplereader.setContentHandler(&ddhandler);
    if (!xmlsimplereader.parse(xmlsource))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            "DataDirect XML failed to properly parse, downloaded listings "
            "were probably corrupt.");
        ok = false;
    }

    return ok;
}

bool DataDirectProcessor::GrabLineupsOnly(void)
{
    const QDateTime start = QDateTime(MythDate::current().date().addDays(2),
                                      QTime(23, 59, 0), Qt::UTC);
    const QDateTime end   = start.addSecs(1);

    return GrabData(start, end);
}

bool DataDirectProcessor::GrabAllData(void)
{
    return GrabData(QDateTime(MythDate::current()).addDays(-2),
                    QDateTime(MythDate::current()).addDays(15));
}

void DataDirectProcessor::CreateATempTable(const QString &ptablename,
                                           const QString &ptablestruct)
{
    MSqlQuery query(MSqlQuery::DDCon());
    QString querystr;
    querystr = "CREATE TEMPORARY TABLE IF NOT EXISTS " + ptablename + " " +
        ptablestruct + " ENGINE=MyISAM;";

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
    LOG(VB_GENERAL, LOG_INFO, LOC + "Grabbing login cookies and lineups");

    PostList list;
    list.push_back(PostItem("username", GetUserID()));
    list.push_back(PostItem("password", GetPassword()));
    list.push_back(PostItem("action",   "Login"));

    QString labsURL   = m_providers[m_listingsProvider].webURL;
    QString loginPage = m_providers[m_listingsProvider].loginPage;

    bool ok;
    QString resultFilename = GetResultFilename(ok);
    if (!ok)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "GrabLoginCookiesAndLineups: "
                "Creating temp result file");
        return false;
    }
    QString cookieFilename = GetCookieFilename(ok);
    if (!ok)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "GrabLoginCookiesAndLineups: "
                "Creating temp cookie file");
        return false;
    }

    ok = Post(labsURL + loginPage, list, resultFilename, "",
              cookieFilename);

    bool got_cookie = QFileInfo(cookieFilename).size() > 100;

    ok &= got_cookie && (!parse_lineups || ParseLineups(resultFilename));
    if (ok)
        m_cookieFileDT = MythDate::current();

    return ok;
}

bool DataDirectProcessor::GrabLineupForModify(const QString &lineupid)
{
    LOG(VB_GENERAL, LOG_INFO, LOC +
        QString("Grabbing lineup %1 for modification").arg(lineupid));

    RawLineupMap::const_iterator it = m_rawLineups.find(lineupid);
    if (it == m_rawLineups.end())
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
        LOG(VB_GENERAL, LOG_ERR, LOC + "GrabLoginCookiesAndLineups: "
                "Creating temp result file");
        return false;
    }
    QString cookieFilename = GetCookieFilename(ok);
    if (!ok)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "GrabLoginCookiesAndLineups: "
                "Creating temp cookie file");
        return false;
    }

    QString labsURL = m_providers[m_listingsProvider].webURL;
    ok = Post(labsURL + (*it).get_action, list, resultFilename,
                   cookieFilename, "");

    return ok && ParseLineup(lineupid, resultFilename);
}

void DataDirectProcessor::SetAll(const QString &lineupid, bool val)
{
    LOG(VB_GENERAL, LOG_INFO, LOC + QString("%1 all channels in lineup %2")
            .arg((val) ? "Selecting" : "Deselecting").arg(lineupid));

    RawLineupMap::iterator lit = m_rawLineups.find(lineupid);
    if (lit == m_rawLineups.end())
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
    QDateTime cache_dt(QDate(1971, 1, 1), QTime(0,0,0), Qt::UTC);
    QFile lfile(get_cache_filename(lineupid));
    if (!lfile.exists())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "GrabLineupCacheAge("+lineupid+
            ") failed -- " +
            QString("file '%1' doesn't exist")
                .arg(get_cache_filename(lineupid)));
        return cache_dt;
    }
    if (lfile.size() < 8)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "GrabLineupCacheAge("+lineupid+
            ") failed -- " +
            QString("file '%1' size %2 too small")
                .arg(get_cache_filename(lineupid)).arg(lfile.size()));
        return cache_dt;
    }
    if (!lfile.open(QIODevice::ReadOnly))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "GrabLineupCacheAge("+lineupid+
            ") failed -- " +
            QString("cannot open file '%1'")
                .arg(get_cache_filename(lineupid)));
        return cache_dt;
    }

    QString tmp;
    QTextStream io(&lfile);
    io >> tmp;
    cache_dt = MythDate::fromString(tmp);

    LOG(VB_GENERAL, LOG_INFO, LOC + "GrabLineupCacheAge("+lineupid+") -> " +
            cache_dt.toString(Qt::ISODate));

    return cache_dt;
}

bool DataDirectProcessor::GrabLineupsFromCache(const QString &lineupid)
{
    QFile lfile(get_cache_filename(lineupid));
    if (!lfile.exists() || (lfile.size() < 8) ||
        !lfile.open(QIODevice::ReadOnly))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "GrabLineupFromCache("+lineupid+
            ") -- failed");
        return false;
    }

    QString tmp;
    uint size;
    QTextStream io(&lfile);
    io >> tmp; // read in date
    io >> size; // read in number of channels mapped

    for (uint i = 0; i < 14; i++)
        io.readLine(); // read extra lines

    DDLineupChannels &channels = m_lineupmaps[lineupid];
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

        m_stations[station.stationid] = station;
    }

    LOG(VB_GENERAL, LOG_INFO, LOC + "GrabLineupFromCache("+lineupid+
        ") -- success");

    return true;
}

bool DataDirectProcessor::SaveLineupToCache(const QString &lineupid) const
{
    QString fn = get_cache_filename(lineupid);
    QByteArray fna = fn.toLatin1();
    QFile lfile(fna.constData());
    if (!lfile.open(QIODevice::WriteOnly))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "SaveLineupToCache("+lineupid+
            ") -- failed");
        return false;
    }

    QTextStream io(&lfile);
    io << MythDate::current_iso_string() << endl;

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

    LOG(VB_GENERAL, LOG_INFO, LOC + "SaveLineupToCache("+lineupid+
        ") -- success");

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
        bool valid = exp_time > MythDate::current();
        if (valid && GrabLineupsFromCache(lineupid))
            return true;
    }

    bool ok = GrabLoginCookiesAndLineups();
    if (!ok)
        return false;

    ok = GrabLineupForModify(lineupid);
    if (!ok)
        return false;

    RawLineupMap::iterator lit = m_rawLineups.find(lineupid);
    if (lit == m_rawLineups.end())
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
    RawLineupMap::iterator lit = m_rawLineups.find(lineupid);
    if (lit == m_rawLineups.end())
        return false;

    // Grab login cookies if they are more than 5 minutes old
    if ((!m_cookieFileDT.isValid() ||
         m_cookieFileDT.addSecs(5*60) < MythDate::current()) &&
        !GrabLoginCookiesAndLineups(false))
    {
        return false;
    }

    // Get callsigns based on xmltv ids (aka stationid)
    DDLineupMap::const_iterator ddit = m_lineupmaps.find(lineupid);
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
    RawLineupMap::const_iterator lit = m_rawLineups.find(lineupid);
    if (lit == m_rawLineups.end())
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

    LOG(VB_GENERAL, LOG_INFO, LOC + QString("Saving lineup %1 with %2 channels")
            .arg(lineupid).arg(list.size() - 1));

    bool ok;
    QString cookieFilename = GetCookieFilename(ok);
    if (!ok)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "GrabLoginCookiesAndLineups: "
                "Creating temp cookie file");
        return false;
    }

    QString labsURL = m_providers[m_listingsProvider].webURL;
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

    LOG(VB_GENERAL, LOG_INFO, LOC + "Saving updated DataDirect listing");
    bool ok = SaveLineup(lineupid, xmltvids);

    if (!ok)
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to update DataDirect listings.");

    return ok;
}

QString DataDirectProcessor::GetRawUDLID(const QString &lineupid) const
{
    RawLineupMap::const_iterator it = m_rawLineups.find(lineupid);
    if (it == m_rawLineups.end())
        return QString::null;
    return (*it).udl_id;
}

QString DataDirectProcessor::GetRawZipCode(const QString &lineupid) const
{
    RawLineupMap::const_iterator it = m_rawLineups.find(lineupid);
    if (it == m_rawLineups.end())
        return QString::null;
    return (*it).zipcode;
}

RawLineup DataDirectProcessor::GetRawLineup(const QString &lineupid) const
{
    RawLineup tmp;
    RawLineupMap::const_iterator it = m_rawLineups.find(lineupid);
    if (it == m_rawLineups.end())
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
        m_fatalErrors.push_back(errmsg);
        ok = false;
    }
    else
    {
        filename = tmp;
        ok = true;
    }
}

QString DataDirectProcessor::CreateTempDirectory(bool *pok) const
{
    bool ok;
    pok = (pok) ? pok : &ok;
    if (m_tmpDir == "/tmp")
    {
        CreateTemp("/tmp/mythtv_ddp_XXXXXX",
                   "Failed to create temp directory",
                   true, m_tmpDir, *pok);
    }
    return m_tmpDir;
}


QString DataDirectProcessor::GetResultFilename(bool &ok) const
{
    ok = true;
    if (m_tmpResultFile.isEmpty())
    {
        CreateTemp(m_tmpDir + "/mythtv_result_XXXXXX",
                   "Failed to create temp result file",
                   false, m_tmpResultFile, ok);
    }
    return m_tmpResultFile;
}

QString DataDirectProcessor::GetCookieFilename(bool &ok) const
{
    ok = true;
    if (m_cookieFile.isEmpty())
    {
        CreateTemp(m_tmpDir + "/mythtv_cookies_XXXXXX",
                   "Failed to create temp cookie file",
                   false, m_cookieFile, ok);
    }
    return m_cookieFile;
}

void DataDirectProcessor::SetUserID(const QString &uid)
{
    m_userid = uid;
    m_userid.detach();
}

void DataDirectProcessor::SetPassword(const QString &pwd)
{
    m_password = pwd;
    m_password.detach();
}

void DataDirectProcessor::SetInputFile(const QString &file)
{
    m_inputFilename = file;
    m_inputFilename.detach();
}

bool DataDirectProcessor::Post(QString url, const PostList &list,
                               QString documentFile,
                               QString inCookieFile, QString outCookieFile)
{
    MythDownloadManager *manager = GetMythDownloadManager();

    if (!inCookieFile.isEmpty())
        manager->loadCookieJar(inCookieFile);

    QByteArray postdata;
    for (uint i = 0; i < list.size(); i++)
    {
        postdata += ((i) ? "&" : "") + list[i].key + "=";
        postdata += html_escape(list[i].value);
    }

    if (!manager->post(url, &postdata))
        return false;

    if (!outCookieFile.isEmpty())
        manager->saveCookieJar(outCookieFile);

    if (documentFile.isEmpty())
        return true;

    QFile file(documentFile);
    file.open(QIODevice::WriteOnly);
    file.write(postdata);
    file.close();

    QFileInfo fi(documentFile);
    return fi.size();
}

bool DataDirectProcessor::ParseLineups(const QString &documentFile)
{
    QFile file(documentFile);
    if (!file.open(QIODevice::ReadOnly))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + QString("Failed to open '%1'")
                .arg(documentFile));
        return false;
    }

    QTextStream stream(&file);
    bool in_form = false;
    QString get_action = QString::null;
    QMap<QString,QString> name_value;

    m_rawLineups.clear();

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
#if 0
            LOG(VB_GENERAL, LOG_DEBUG, LOC + QString("action: %1").arg(action));
#endif
        }

        if (!in_form)
            continue;

        int inp = llow.indexOf("<input");
        if (inp >= 0)
        {
            QString input_line = line.mid(inp + 6);
#if 0
            LOG(VB_GENERAL, LOG_DEBUG, LOC +
                QString("input: %1").arg(input_line));
#endif
            QString name  = get_setting(input_line, "name");
            QString value = get_setting(input_line, "value");
#if 0
            LOG(VB_GENERAL, LOG_DEBUG, LOC + QString("name: %1").arg(name));
            LOG(VB_GENERAL, LOG_DEBUG, LOC + QString("value: %1").arg(value));
#endif
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

                m_rawLineups[name_value["lineup_id"]] = item;
#if 0
                LOG(VB_GENERAL, LOG_DEBUG, LOC +
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
        LOG(VB_GENERAL, LOG_ERR, LOC + QString("Failed to open '%1'")
                .arg(documentFile));

        return false;
    }

    QTextStream stream(&file);
    bool in_form = false;
    int in_label = 0;
    QMap<QString,QString> settings;

    RawLineup &lineup = m_rawLineups[lineupid];
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
#if 0
            LOG(VB_GENERAL, LOG_DEBUG, LOC + "set_action: " +
                lineup.set_action);
#endif
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
            LOG(VB_GENERAL, LOG_DEBUG, LOC +
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
            new_str += QString("%%1").arg((int)str[1].toLatin1(), 0, 16);
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
            LOG(VB_GENERAL, LOG_INFO, LOC +
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

    LOG(VB_GENERAL, LOG_INFO, LOC + QString("Adding channel %1 '%2' (%3).")
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

        LOG(VB_GENERAL, LOG_INFO, LOC +
            QString("sourceid %1 has lineup type: %2").arg(srcid).arg(type));
    }
}

static QString get_lineup_type(uint sourceid)
{
    QMutexLocker locker(&lineup_type_lock);
    QString ret = srcid_to_type[sourceid];
    ret.detach();
    return ret;
}

/*
 * This function taken from: 
 * http://stackoverflow.com/questions/2690328/qt-quncompress-gzip-data
 *
 * Based on zlib example code.
 *
 * Copyright (c) 2011 Ralf Engels <ralf-engels@gmx.de>
 * Copyright (C) 1995-2012 Jean-loup Gailly and Mark Adler
 *
 * Licensed under the terms of the ZLib license which is found at
 * http://zlib.net/zlib_license.html and is as follows:
 *
 *  This software is provided 'as-is', without any express or implied
 *  warranty.  In no event will the authors be held liable for any damages
 *  arising from the use of this software.
 *
 *  Permission is granted to anyone to use this software for any purpose,
 *  including commercial applications, and to alter it and redistribute it
 *  freely, subject to the following restrictions:
 *
 *  1. The origin of this software must not be misrepresented; you must not
 *     claim that you wrote the original software. If you use this software
 *     in a product, an acknowledgment in the product documentation would be
 *     appreciated but is not required.
 *  2. Altered source versions must be plainly marked as such, and must not be
 *     misrepresented as being the original software.
 *  3. This notice may not be removed or altered from any source distribution.
 *
 *  NOTE: The Zlib license is listed as being GPL-compatible
 *    http://www.gnu.org/licenses/license-list.html#ZLib
 */

QByteArray gUncompress(const QByteArray &data)
{
    if (data.size() <= 4) {
        LOG(VB_GENERAL, LOG_WARNING, "gUncompress: Input data is truncated");
        return QByteArray();
    }

    QByteArray result;

    int ret;
    z_stream strm;
    static const int CHUNK_SIZE = 1024;
    char out[CHUNK_SIZE];

    /* allocate inflate state */
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    strm.avail_in = data.size();
    strm.next_in = (Bytef*)(data.data());

    ret = inflateInit2(&strm, 15 +  32); // gzip decoding
    if (ret != Z_OK)
        return QByteArray();

    // run inflate()
    do {
        strm.avail_out = CHUNK_SIZE;
        strm.next_out = (Bytef*)(out);

        ret = inflate(&strm, Z_NO_FLUSH);
        Q_ASSERT(ret != Z_STREAM_ERROR);  // state not clobbered

        switch (ret) {
        case Z_NEED_DICT:
            ret = Z_DATA_ERROR;     // and fall through
        case Z_DATA_ERROR:
        case Z_MEM_ERROR:
            (void)inflateEnd(&strm);
            return QByteArray();
        }

        result.append(out, CHUNK_SIZE - strm.avail_out);
    } while (strm.avail_out == 0);

    // clean up and return
    inflateEnd(&strm);
    return result;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
