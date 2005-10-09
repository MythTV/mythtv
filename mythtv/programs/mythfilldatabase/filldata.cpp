#include <qapplication.h>
#include <qdom.h>
#include <qfile.h>
#include <qstring.h>
#include <qregexp.h>
#include <qstringlist.h>
#include <qvaluelist.h>
#include <qmap.h>
#include <qdatetime.h>
#include <qdir.h>
#include <qfile.h>
#include <qsqldatabase.h>
#include <qsqlquery.h>
#include <qurl.h>

#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>

#include <iostream>
#include <fstream>
#include <string>
#include <cstdlib>
#include <cstdio>
#include <ctime>
#include <cerrno>

#include "libmyth/exitcodes.h"
#include "libmyth/mythcontext.h"
#include "libmythtv/scheduledrecording.h"
#include "libmythtv/datadirect.h"
#include "libmythtv/channelutil.h"
#include "libmyth/mythdbcon.h"
#include "libmythtv/programinfo.h"
#include "libmythtv/frequencytables.h"
#include "libmythtv/channelutil.h"

using namespace std;

static QString SetupIconCacheDirectory();

bool interactive = false;
bool channel_preset = false;
bool non_us_updating = false;
bool from_file = false;
bool quiet = false;
bool no_delete = false;
bool isNorthAmerica = false;
bool isJapan = false;
bool interrupted = false;
bool refresh_today = false;
bool refresh_tomorrow = true;
bool refresh_second = false;
bool refresh_all = false;
bool refresh_tba = true;
bool dd_grab_all = false;
bool dddataretrieved = false;
bool mark_repeats = true;
bool channel_updates = false;

int maxDays = 0;
int listing_wrap_offset = 0;

QString lastdduserid;
DataDirectProcessor ddprocessor;
QString graboptions = "";

class ChanInfo
{
  public:
    ChanInfo() { }
    ChanInfo(const ChanInfo &other) { callsign = other.callsign; 
                                      iconpath = other.iconpath;
                                      chanstr = other.chanstr;
                                      xmltvid = other.xmltvid;
                                      old_xmltvid = other.old_xmltvid;
                                      name = other.name;
                                      freqid = other.freqid;
                                      finetune = other.finetune;
                                      tvformat = other.tvformat;
                                    }

    QString callsign;
    QString iconpath;
    QString chanstr;
    QString xmltvid;
    QString old_xmltvid;
    QString name;
    QString freqid;
    QString finetune;
    QString tvformat;
};

struct ProgRating
{
    QString system;
    QString rating;
};

struct ProgCredit
{
    QString role;
    QString name;
};

class ProgInfo
{
  public:
    ProgInfo() { }
    ProgInfo(const ProgInfo &other) { channel = other.channel;
                                      startts = other.startts;
                                      endts = other.endts;
                                      start = other.start;
                                      end = other.end;
                                      title = other.title;
                                      subtitle = other.subtitle;
                                      desc = other.desc;
                                      category = other.category;
                                      catType = other.catType;
                                      airdate = other.airdate;
                                      stars = other.stars;
                                      previouslyshown = other.previouslyshown;
                                      title_pronounce = other.title_pronounce;
                                      stereo = other.stereo;
                                      subtitled = other.subtitled;
                                      hdtv = other.hdtv;
                                      closecaptioned = other.closecaptioned;
                                      partnumber = other.partnumber;
                                      parttotal = other.parttotal;
                                      seriesid = other.seriesid;
                                      originalairdate = other.originalairdate;
                                      showtype = other.showtype;
                                      colorcode = other.colorcode;
                                      syndicatedepisodenumber = other.syndicatedepisodenumber;
                                      programid = other.programid;
        
                                      clumpidx = other.clumpidx;
                                      clumpmax = other.clumpmax;
                                      ratings = other.ratings;
                                      credits = other.credits;
                                      content = other.content;
                                    }


    QString channel;
    QString startts;
    QString endts;
    QDateTime start;
    QDateTime end;
    QString title;
    QString subtitle;
    QString desc;
    QString category;
    QString catType;
    QString airdate;
    QString stars;
    bool previouslyshown;
    QString title_pronounce;
    bool stereo;
    bool subtitled;
    bool hdtv;
    bool closecaptioned;
    QString partnumber;
    QString parttotal;
    QString seriesid;                                
    QString originalairdate;
    QString showtype;
    QString colorcode;
    QString syndicatedepisodenumber;
    QString programid;

    QString clumpidx;
    QString clumpmax;
    QValueList<ProgRating> ratings;
    QValueList<ProgCredit> credits;
    QString content;
};

bool operator<(const ProgInfo &a, const ProgInfo &b)
{
    return (a.start < b.start);
}

bool operator>(const ProgInfo &a, const ProgInfo &b)
{
    return (a.start > b.start);
}

bool operator<=(const ProgInfo &a, const ProgInfo &b)
{
    return (a.start <= b.start);
}

struct Source
{
    int id;
    QString name;
    QString xmltvgrabber;
    QString userid;
    QString password;
    QString lineupid;
};


unsigned int ELFHash(const char *s)
{
    /* ELF hash uses unsigned chars and unsigned arithmetic for portability */
    const unsigned char *name = (const unsigned char *)s;
    unsigned long h = 0, g;

    while (*name)
    { /* do some fancy bitwanking on the string */
        h = (h << 4) + (unsigned long)(*name++);
        if ((g = (h & 0xF0000000UL))!=0)
            h ^= (g >> 24);
        h &= ~g;

    }

    return (int)h;
}

void clearDataByChannel(int chanid, QDateTime from, QDateTime to) 
{
    MSqlQuery query(MSqlQuery::InitCon());
    QString querystr;

    querystr.sprintf("DELETE FROM program "
                     "WHERE starttime >= '%s' AND starttime < '%s' "
                     "AND chanid = %d;",
                     from.toString("yyyyMMddhhmmss").ascii(),
                     to.toString("yyyyMMddhhmmss").ascii(),
                     chanid);
    query.exec(querystr);

    querystr.sprintf("DELETE FROM programrating WHERE starttime >= '%s' "
                     " AND starttime < '%s' AND chanid = %d;",
                     from.toString("yyyyMMddhhmmss").ascii(),
                     to.toString("yyyyMMddhhmmss").ascii(),
                     chanid);
    query.exec(querystr);

    querystr.sprintf("DELETE FROM credits WHERE starttime >= '%s' "
                     "AND starttime < '%s' AND chanid = %d;",
                     from.toString("yyyyMMddhhmmss").ascii(),
                     to.toString("yyyyMMddhhmmss").ascii(),
                     chanid);
    query.exec(querystr);

    querystr.sprintf("DELETE FROM programgenres WHERE starttime >= '%s' "
                     "AND starttime < '%s' AND chanid = %d;",
                     from.toString("yyyyMMddhhmmss").ascii(),
                     to.toString("yyyyMMddhhmmss").ascii(),
                     chanid);
    query.exec(querystr);
}

void clearDataBySource(int sourceid, QDateTime from, QDateTime to) 
{
    QString querystr= QString("SELECT chanid FROM channel WHERE "
                              "sourceid = \"%0\";").arg(sourceid);

    MSqlQuery query(MSqlQuery::InitCon());

    if (!query.exec(querystr))
        MythContext::DBError("Selecting channels per source", query);
        
    if (query.isActive() && query.numRowsAffected() > 0)
    {
        while (query.next())
        {
            int chanid = query.value(0).toInt();
            clearDataByChannel(chanid, from, to);
        }
    }
}

// icon mapping stuff
namespace {
const char * const IM_DOC_TAG = "iconmappings";

const char * const IM_CS_TO_NET_TAG = "callsigntonetwork";
const char * const IM_CS_TAG = "callsign";

const char * const IM_NET_TAG = "network";

const char * const IM_NET_TO_URL_TAG = "networktourl";
const char * const IM_NET_URL_TAG = "url";

const char * const BASEURLMAP_START = "mythfilldatabase.urlmap.";

const char * const IM_BASEURL_TAG = "baseurl";
const char * const IM_BASE_STUB_TAG = "stub";

QString expandURLString(const QString &url)
{
    QRegExp expandtarget("\\[([^\\]]+)\\]");
    QString retval = url;

    int found_at = 0;
    int start_index = 0;
    while (found_at != -1)
    {
        found_at = expandtarget.search(retval, start_index);
        if (found_at != -1)
        {
            QString no_mapping("no_URL_mapping");
            QString search_string = expandtarget.cap(1);
            QString expanded_text = gContext->GetSetting(
                    QString(BASEURLMAP_START) + search_string, no_mapping);
            if (expanded_text != no_mapping)
            {
                retval.replace(found_at, expandtarget.matchedLength(),
                        expanded_text);
            }
            else
            {
                start_index = found_at + expandtarget.matchedLength();
            }
        }
    }

    return retval;
}

void UpdateSourceIcons(int sourceid)
{
    if (!quiet)
        cout << QString("Updating icons for sourceid: %1").arg(sourceid) << endl;

    QString fileprefix = SetupIconCacheDirectory();

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT ch.chanid, nim.url "
            "FROM channel ch, callsignnetworkmap csm "
            "RIGHT JOIN networkiconmap nim ON csm.network = nim.network "
            "WHERE ch.callsign = csm.callsign AND "
            "(icon = :NOICON OR icon = '') AND ch.sourceid = :SOURCEID");
    query.bindValue(":SOURCEID", sourceid);
    query.bindValue(":NOICON", "none");

    if (!query.exec())
        MythContext::DBError("Looking for icons to fetch", query);

    if (query.isActive() && query.numRowsAffected() > 0)
    {
        while (query.next())
        {
            QString icon_url = expandURLString(query.value(1).toString());
            QFileInfo qfi(icon_url);
            QFile localfile(fileprefix + "/" + qfi.fileName());
            if (!localfile.exists())
            {
                QString icon_get_command = QString("wget --timestamping "
                        "--directory-prefix=") + fileprefix + " " + icon_url;
                if (!quiet)
                    cout << QString("Attempting to fetch icon with: %1").arg(icon_get_command) << endl;

                system(icon_get_command);
            }

            if (localfile.exists())
            {
                int chanid = query.value(0).toInt();
                if (!quiet)
                {
                    QString m = QString("Updating channel icon for chanid: %1")
                        .arg(chanid);
                    cout << m << endl;
                }
                MSqlQuery icon_update_query(MSqlQuery::InitCon());
                icon_update_query.prepare("UPDATE channel SET icon = :ICON "
                        "WHERE chanid = :CHANID AND sourceid = :SOURCEID");
                icon_update_query.bindValue(":ICON", localfile.name());
                icon_update_query.bindValue(":CHANID", query.value(0).toInt());
                icon_update_query.bindValue(":SOURCEID", sourceid);

                if (!icon_update_query.exec())
                    MythContext::DBError("Setting the icon file name",
                            icon_update_query);
            }
            else
            {
                cerr << QString(
                        "Error retrieving icon from '%1' to file '%2'")
                        .arg(icon_url)
                        .arg(localfile.name())
                     << endl;
            }
        }
    }
}

bool dash_open(QFile &file, const QString &filename, int m, FILE *handle = NULL)
{
    bool retval = false;
    if (filename == "-")
    {
        if (handle == NULL)
        {
            handle = stdout;
            if (m & IO_ReadOnly)
            {
                handle = stdin;
            }
        }
        retval = file.open(m, handle);
    }
    else
    {
        file.setName(filename);
        retval = file.open(m);
    }

    return retval;
}

class DOMException
{
  private:
    QString message;

  protected:
    void setMessage(const QString &mes)
    {
        message = mes;
    }

  public:
    DOMException() : message("Unknown DOMException") {}
    virtual ~DOMException() {}
    DOMException(const QString &mes) : message(mes) {}
    QString getMessage()
    {
        return message;
    }
};

class DOMBadElementConversion : public DOMException
{
  public:
    DOMBadElementConversion()
    {
        setMessage("Unknown DOMBadElementConversion");
    }
    DOMBadElementConversion(const QString &mes) : DOMException(mes) {}
    DOMBadElementConversion(const QDomNode &node)
    {
        setMessage(QString("Unable to convert node: '%1' to QDomElement.")
                .arg(node.nodeName()));
    }
};

class DOMUnknownChildElement : public DOMException
{
  public:
    DOMUnknownChildElement()
    {
        setMessage("Unknown DOMUnknownChildElement");
    }
    DOMUnknownChildElement(const QString &mes) : DOMException(mes) {}
    DOMUnknownChildElement(const QDomElement &e, QString child_name)
    {
        setMessage(QString("Unknown child element '%1' of: '%2'")
                .arg(child_name)
                .arg(e.tagName()));
    }
};

QDomElement nodeToElement(QDomNode &node)
{
    QDomElement retval = node.toElement();
    if (retval.isNull())
    {
        throw DOMBadElementConversion(node);
    }
    return retval;
}

QString getNamedElementText(const QDomElement &e,
        const QString &child_element_name)
{
    QDomNode child_node = e.namedItem(child_element_name);
    if (child_node.isNull())
    {
        throw DOMUnknownChildElement(e, child_element_name);
    }
    QDomElement element = nodeToElement(child_node);
    return element.text();
}

void ImportIconMap(const QString &filename)
{
    if (!quiet)
    {
        QString msg = QString("Importing icon mapping from %1...")
                .arg(filename);
        cout << msg << endl;
    }
    QFile xml_file;

    if (dash_open(xml_file, filename, IO_ReadOnly))
    {
        QDomDocument doc;
        QString de_msg;
        int de_ln = 0;
        int de_column = 0;
        if (doc.setContent(&xml_file, false, &de_msg, &de_ln, &de_column))
        {
            MSqlQuery nm_query(MSqlQuery::InitCon());
            nm_query.prepare("REPLACE INTO networkiconmap(network, url) "
                    "VALUES(:NETWORK, :URL)");
            MSqlQuery cm_query(MSqlQuery::InitCon());
            cm_query.prepare("REPLACE INTO callsignnetworkmap(callsign, "
                    "network) VALUES(:CALLSIGN, :NETWORK)");
            MSqlQuery su_query(MSqlQuery::InitCon());
            su_query.prepare("UPDATE settings SET data = :URL "
                    "WHERE value = :STUBNAME");
            MSqlQuery si_query(MSqlQuery::InitCon());
            si_query.prepare("INSERT INTO settings(value, data) "
                    "VALUES(:STUBNAME, :URL)");

            QDomElement element = doc.documentElement();

            QDomNode node = element.firstChild();
            while (!node.isNull())
            {
                try
                {
                    QDomElement e = nodeToElement(node);
                    if (e.tagName() == IM_NET_TO_URL_TAG)
                    {
                        QString net = getNamedElementText(e, IM_NET_TAG);
                        QString u = getNamedElementText(e, IM_NET_URL_TAG);

                        nm_query.bindValue(":NETWORK", net.stripWhiteSpace());
                        nm_query.bindValue(":URL", u.stripWhiteSpace());
                        if (!nm_query.exec())
                            MythContext::DBError(
                                    "Inserting network->url mapping", nm_query);
                    }
                    else if (e.tagName() == IM_CS_TO_NET_TAG)
                    {
                        QString cs = getNamedElementText(e, IM_CS_TAG);
                        QString net = getNamedElementText(e, IM_NET_TAG);

                        cm_query.bindValue(":CALLSIGN", cs.stripWhiteSpace());
                        cm_query.bindValue(":NETWORK", net.stripWhiteSpace());
                        if (!cm_query.exec())
                            MythContext::DBError("Inserting callsign->network "
                                    "mapping", cm_query);
                    }
                    else if (e.tagName() == IM_BASEURL_TAG)
                    {
                        MSqlQuery *qr = &si_query;

                        QString st(BASEURLMAP_START);
                        st += getNamedElementText(e, IM_BASE_STUB_TAG);
                        QString u = getNamedElementText(e, IM_NET_URL_TAG);

                        MSqlQuery qc(MSqlQuery::InitCon());
                        qc.prepare("SELECT COUNT(*) FROM settings "
                                "WHERE value = :STUBNAME");
                        qc.bindValue(":STUBNAME", st);
                        qc.exec();
                        if (qc.isActive() && qc.numRowsAffected() > 0)
                        {
                            qc.first();
                            if (qc.value(0).toInt() != 0)
                            {
                                qr = &su_query;
                            }
                        }

                        qr->bindValue(":STUBNAME", st);
                        qr->bindValue(":URL", u);

                        if (!qr->exec())
                            MythContext::DBError(
                                    "Inserting callsign->network mapping", *qr);
                    }
                }
                catch (DOMException &e)
                {
                    cerr << QString("Error while processing %1: %2")
                            .arg(node.nodeName())
                            .arg(e.getMessage())
                         << endl;
                }
                node = node.nextSibling();
            }
        }
        else
        {
            cerr << QString(
                    "Error unable to set document content: %1:%2c%3 %4")
                    .arg(filename)
                    .arg(de_ln)
                    .arg(de_column)
                    .arg(de_msg)
                 << endl;
        }
    }
    else
    {
        cerr << QString("Error unable to open '%1' for reading.")
                .arg(filename)
             << endl;
    }
}

void ExportIconMap(const QString &filename)
{
    if (!quiet)
    {
        cout << QString("Exporting icon mapping to %1...").arg(filename)
             << endl;
    }
    QFile xml_file(filename);
    if (dash_open(xml_file, filename, IO_WriteOnly))
    {
        QTextStream os(&xml_file);
        os << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
        os << "<!-- generated by mythfilldatabase -->\n";

        QDomDocument iconmap;
        QDomElement roote = iconmap.createElement(IM_DOC_TAG);

        MSqlQuery query(MSqlQuery::InitCon());
        query.exec("SELECT * FROM callsignnetworkmap ORDER BY callsign");

        if (query.isActive() && query.numRowsAffected() > 0)
        {
            while (query.next())
            {
                QDomElement cs2nettag = iconmap.createElement(IM_CS_TO_NET_TAG);
                QDomElement cstag = iconmap.createElement(IM_CS_TAG);
                QDomElement nettag = iconmap.createElement(IM_NET_TAG);
                QDomText cs_text = iconmap.createTextNode(
                        query.value(1).toString());
                QDomText net_text = iconmap.createTextNode(
                        query.value(2).toString());

                cstag.appendChild(cs_text);
                nettag.appendChild(net_text);

                cs2nettag.appendChild(cstag);
                cs2nettag.appendChild(nettag);

                roote.appendChild(cs2nettag);
            }
        }

        query.exec("SELECT * FROM networkiconmap ORDER BY network");
        if (query.isActive() && query.numRowsAffected() > 0)
        {
            while (query.next())
            {
                QDomElement net2urltag = iconmap.createElement(
                        IM_NET_TO_URL_TAG);
                QDomElement nettag = iconmap.createElement(IM_NET_TAG);
                QDomElement urltag = iconmap.createElement(IM_NET_URL_TAG);
                QDomText net_text = iconmap.createTextNode(
                        query.value(1).toString());
                QDomText url_text = iconmap.createTextNode(
                        query.value(2).toString());

                nettag.appendChild(net_text);
                urltag.appendChild(url_text);

                net2urltag.appendChild(nettag);
                net2urltag.appendChild(urltag);

                roote.appendChild(net2urltag);
            }
        }

        query.prepare("SELECT value,data FROM settings WHERE value "
                "LIKE :URLMAP");
        query.bindValue(":URLMAP", QString(BASEURLMAP_START) + "%");
        query.exec();
        if (query.isActive() && query.numRowsAffected() > 0)
        {
            QRegExp baseax("\\.([^\\.]+)$");
            while (query.next())
            {
                QString base_stub = query.value(0).toString();
                if (baseax.search(base_stub) != -1)
                {
                    base_stub = baseax.cap(1);
                }

                QDomElement baseurltag = iconmap.createElement(IM_BASEURL_TAG);
                QDomElement stubtag = iconmap.createElement(
                        IM_BASE_STUB_TAG);
                QDomElement urltag = iconmap.createElement(IM_NET_URL_TAG);
                QDomText base_text = iconmap.createTextNode(base_stub);
                QDomText url_text = iconmap.createTextNode(
                        query.value(1).toString());

                stubtag.appendChild(base_text);
                urltag.appendChild(url_text);

                baseurltag.appendChild(stubtag);
                baseurltag.appendChild(urltag);

                roote.appendChild(baseurltag);
            }
        }

        iconmap.appendChild(roote);
        iconmap.save(os, 4);
    }
    else
    {
        cerr << QString("Error unable to open '%1' for writing.") << endl;
    }
}

void RunSimpleQuery(const QString &query)
{
    MSqlQuery q(MSqlQuery::InitCon());
    if (!q.exec(query))
        MythContext::DBError("RunSimpleQuery ", q);
}

void ResetIconMap(bool reset_icons)
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("DELETE FROM settings WHERE value LIKE :URLMAPLIKE");
    query.bindValue(":URLMAPLIKE", QString(BASEURLMAP_START) + '%');
    if (!query.exec())
        MythContext::DBError("ResetIconMap", query);

    RunSimpleQuery("TRUNCATE TABLE callsignnetworkmap;");
    RunSimpleQuery("TRUNCATE TABLE networkiconmap");

    if (reset_icons)
    {
        RunSimpleQuery("UPDATE channel SET icon = 'none'");
    }
}

} // namespace

void get_atsc_stuff(QString channum, int sourceid, int freqid,
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
    cerr<<"centre_freq: "<<freq<<endl;

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

// DataDirect stuff
void DataDirectStationUpdate(Source source)
{
    MSqlQuery no_xmltvid_q(MSqlQuery::DDCon());
    MSqlQuery query(MSqlQuery::DDCon());
    MSqlQuery chan_update_q(MSqlQuery::DDCon());
    QString querystr;

    ddprocessor.updateStationViewTable();

    // Find all the channels in the dd_v_station temp table
    // where there is no channel with the same xmltvid in the
    // DB using the same source.
    no_xmltvid_q.prepare(
        "SELECT dd_v_station.stationid,        dd_v_station.callsign,"
        "       dd_v_station.stationname,      dd_v_station.channel,"
        "       dd_v_station.fccchannelnumber, dd_v_station.channelMinor "
        "FROM dd_v_station LEFT JOIN channel ON "
        "     dd_v_station.stationid = channel.xmltvid AND "
        "     channel.sourceid = :SOURCEID "
        "WHERE channel.chanid IS NULL;");
    no_xmltvid_q.bindValue(":SOURCEID", source.id);

    if (!no_xmltvid_q.exec())
        MythContext::DBError("Selecting new channels", no_xmltvid_q);

    if (no_xmltvid_q.isActive() && no_xmltvid_q.numRowsAffected() > 0)
    {
        while (no_xmltvid_q.next())
        {
            // Get the important info from our first query
            QString xmltvid  = no_xmltvid_q.value(0).toString();
            QString callsign = no_xmltvid_q.value(1).toString();
            QString name     = no_xmltvid_q.value(2).toString();
            QString channum  = no_xmltvid_q.value(3).toString();
            int     freqid   = no_xmltvid_q.value(4).toInt();
            QString minor    = no_xmltvid_q.value(5).toString();
            QString tvformat = "Default";
            callsign = (callsign == "") ? name : callsign;

            int major = 0, atscsrcid = -9999;
            if (minor != "") 
            {
                major = channum.toInt();
                atscsrcid = (major << 8) | minor.toInt();
                tvformat = "atsc";
                channum += "_" + minor; // default channum number
            }
            else
            {
                freqid = channum.toInt();
            }

            // First check if channel already in DB, but without xmltvid
            query.prepare("SELECT chanid FROM channel "
                          "WHERE sourceid = :SOURCEID AND "
                          "      (channum=:CHANNUM OR atscsrcid=:ATSCSRCID)");
            query.bindValue(":SOURCEID",  source.id);
            query.bindValue(":CHANNUM",   channum);
            query.bindValue(":ATSCSRCID", atscsrcid);

            if (!query.exec() || !query.isActive())
            {
                MythContext::DBError(
                    "Getting chanid of existing channel", query);
                continue; // go on to next channel without xmltv
            }

            if (query.size() > 0)
            {
                // The channel already exists in DB, at least once,
                // so set the xmltvid..
                chan_update_q.prepare(
                    "UPDATE channel "
                    "SET xmltvid = :XMLTVID "
                    "WHERE chanid = :CHANID AND sourceid = :SOURCEID");

                while (query.next())
                {
                    uint chanid = query.value(0).toInt();
                    chan_update_q.bindValue(":CHANID",    chanid);
                    chan_update_q.bindValue(":XMLTVID",   xmltvid);
                    chan_update_q.bindValue(":SOURCEID",  source.id);
                    if (!chan_update_q.exec() || !chan_update_q.isActive())
                    {
                        MythContext::DBError(
                            "Updating XMLTVID of existing channel", chan_update_q);
                        continue; // go on to next instance of this channel
                    }
                }
                continue; // go on to next channel without xmltv
            }

            // The channel doesn't exist in the DB, insert it...
            int mplexid = -1, majorC, minorC, chanid = 0;
            long long freq;
            get_atsc_stuff(channum, source.id, freqid,
                           majorC, minorC, freq);

            if (minorC > 0 && freq >= 0)
            {
                mplexid = ChannelUtil::CreateMultiplex(
                    source.id, "atsc", freq, "8vsb");
            }

            if ((mplexid > 0) || (minorC == 0))
                chanid = ChannelUtil::CreateChanID(source.id, channum);

            if (chanid > 0)
            {
                QString icon   = "";
                int  serviceid = 0;
                bool oag       = false; // use on air guide
                bool hidden    = false;
                bool hidden_in_guide = false;

                ChannelUtil::CreateChannel(
                    mplexid,   source.id, chanid, 
                    callsign,  name,      channum,
                    serviceid, majorC,    minorC,
                    oag,       hidden,    hidden_in_guide,
                    freqid,    icon,      tvformat,
                    xmltvid);
            }
        }
    }

    if (channel_updates)
    {
        //
        //  User must pass "--do_channel_update" for this block of code to
        //  execute
        //

        MSqlQuery dd_station_info(MSqlQuery::DDCon());
        dd_station_info.prepare(
            "SELECT callsign, stationname,      stationid,"
            "       channel,  fccchannelnumber, channelMinor "
            "FROM dd_v_station");
        dd_station_info.exec();

        if (chan_update_q.size() > 0)
        {
            chan_update_q.prepare(
                "UPDATE channel "
                "SET callsign  = :CALLSIGN,  name   = :NAME, "
                "    channum   = :CHANNEL,   freqid = :FREQID, "
                "    atscsrcid = :ATSCSRCID "
                "WHERE xmltvid = :STATIONID AND sourceid = :SOURCEID;");
        }
        while (dd_station_info.next())        
        {
            QString channel = dd_station_info.value(3).toString();
            QString freqid = dd_station_info.value(4).toString();
            QString minor = dd_station_info.value(5).toString();

            uint atscsrcid = 0;
            if (minor != "")
            {
                atscsrcid = (channel.toInt() << 8) | minor.toInt();
                channel += "_" + minor; // default channel number
            }
            else
            {
                freqid = channel;
            }

            chan_update_q.bindValue(":CALLSIGN",  dd_station_info.value(0));
            chan_update_q.bindValue(":NAME",      dd_station_info.value(1));
            chan_update_q.bindValue(":STATIONID", dd_station_info.value(2));
            chan_update_q.bindValue(":CHANNEL",   channel);
            chan_update_q.bindValue(":SOURCEID",  source.id);
            chan_update_q.bindValue(":FREQID",    freqid);
            chan_update_q.bindValue(":ATSCSRCID", atscsrcid);

            if (!chan_update_q.exec())
            {
                MythContext::DBError("Updating channel table",
                                     chan_update_q.lastQuery());
            }
        }
    }

    UpdateSourceIcons(source.id);

    // Now, delete any channels which no longer exist
    // (Not currently done in standard program -- need to verify if required)
}

void DataDirectProgramUpdate(Source source) 
{
    MSqlQuery query(MSqlQuery::DDCon());
   
    //cerr << "Creating program view table...\n";
    ddprocessor.updateProgramViewTable(source.id);
    //cerr <<  "Finished creating program view table...\n";

    //cerr << "Adding rows to main program table from view table..\n";
    if (!query.exec("INSERT IGNORE INTO program (chanid, starttime, endtime, "
                    "title, subtitle, description, "
                    "showtype, category, category_type, "
                    "airdate, stars, previouslyshown, stereo, subtitled, "
                    "hdtv, closecaptioned, partnumber, parttotal, seriesid, "
                    "originalairdate, colorcode, syndicatedepisodenumber, "
                    "programid) "
                    "SELECT chanid, starttime, endtime, "
                    "title, subtitle, description, "
                    "showtype, dd_genre.class, category_type, "
                    "airdate, stars, previouslyshown, stereo, subtitled, "
                    "hdtv, closecaptioned, partnumber, parttotal, seriesid, "
                    "originalairdate, colorcode, syndicatedepisodenumber, "
                    "dd_v_program.programid FROM dd_v_program "
                    "LEFT JOIN dd_genre ON ("
                    "dd_v_program.programid = dd_genre.programid AND "
                    "dd_genre.relevance = '0');"))
        MythContext::DBError("Inserting into program table", query);

    //cerr << "Finished adding rows to main program table...\n";
    //cerr << "Adding program ratings...\n";

    if (!query.exec("INSERT IGNORE INTO programrating (chanid, starttime, "
                    "system, rating) SELECT chanid, starttime, 'MPAA', "
                    "mpaarating FROM dd_v_program WHERE mpaarating != '';"))
        MythContext::DBError("Inserting into programrating table", query);

    if (!query.exec("INSERT IGNORE INTO programrating (chanid, starttime, "
                    "system, rating) SELECT chanid, starttime, 'VCHIP', "
                    "tvrating FROM dd_v_program WHERE tvrating != '';"))
        MythContext::DBError("Inserting into programrating table", query);

    //cerr << "Finished adding program ratings...\n";
    //cerr << "Populating people table from production crew list...\n";

    if (!query.exec("INSERT IGNORE INTO people (name) SELECT fullname "
                    "FROM dd_productioncrew;"))
        MythContext::DBError("Inserting into people table", query);

    //cerr << "Finished adding people...\n";
    //cerr << "Adding credits entries from production crew list...\n";

    if (!query.exec("INSERT IGNORE INTO credits (chanid, starttime, person, "
                    "role) SELECT chanid, starttime, person, role "
                    "FROM dd_productioncrew, dd_v_program, people "
                    "WHERE "
                    "((dd_productioncrew.programid = dd_v_program.programid) "
                    "AND (dd_productioncrew.fullname = people.name));"))
        MythContext::DBError("Inserting into credits table", query);

    //cerr << "Finished inserting credits...\n";
    //cerr << "Adding genres...\n";

    if (!query.exec("INSERT IGNORE INTO programgenres (chanid, starttime, "
                    "relevance, genre) SELECT chanid, starttime, "
                    "relevance, class FROM dd_v_program, dd_genre "
                    "WHERE (dd_v_program.programid = dd_genre.programid);"))
        MythContext::DBError("Inserting into programgenres table",query);

    //cerr << "Done...\n";
}

bool grabDDData(Source source, int poffset, QDate pdate, int ddSource) 
{
    ddprocessor.setLineup(source.lineupid);
    ddprocessor.setUserID(source.userid);
    ddprocessor.setPassword(source.password);
    ddprocessor.setSource(ddSource);
    
    bool needtoretrieve = true;

    if (source.userid != lastdduserid)
        dddataretrieved = false;

    if (dd_grab_all && dddataretrieved)
        needtoretrieve = false;

    QDateTime qdtNow = QDateTime::currentDateTime();
    MSqlQuery query(MSqlQuery::DDCon());
    QString status = "currently running.";

    query.exec(QString("UPDATE settings SET data ='%1' "
                       "WHERE value='mythfilldatabaseLastRunStart'")
                       .arg(qdtNow.toString("yyyy-MM-dd hh:mm")));

    if (needtoretrieve)
    {
        if (!quiet)
            cout << "Retrieving datadirect data... \n";
        if (dd_grab_all) 
        {
            if (!quiet)
                cout << "Grabbing ALL available data...\n";
            if (!ddprocessor.grabAllData())
            {
                cerr << "Encountered error in grabbing data...\n";
                return false;
            }
        }
        else
        {
            if (!quiet)
                cout << "Grabbing data for " << pdate.toString() 
                     << " offset " << poffset << "\n";
            QDateTime fromdatetime = QDateTime(pdate);
            QDateTime todatetime;
            fromdatetime.setTime_t(QDateTime(pdate).toTime_t(),Qt::UTC);
            fromdatetime = fromdatetime.addDays(poffset);
            todatetime = fromdatetime.addDays(1);
            if (!quiet)
                cout << "From : " << fromdatetime.toString() 
                     << " To : " << todatetime.toString() << " (UTC)\n";
            if (!ddprocessor.grabData(false, fromdatetime, todatetime))
            {
                cout << "Encountered error in grabbing data..\n";
                return false;
            }
        }

        dddataretrieved = true;
        lastdduserid = source.userid;
    }
    else
    {
        if (!quiet)
            cout << "Using existing grabbed data in temp tables..\n";
    }

    if (!quiet)
        cout << "Grab complete.  Actual data from " 
             << ddprocessor.getActualListingsFrom().toString() << " "
             << "to " << ddprocessor.getActualListingsTo().toString() 
             << " (UTC) \n";

    qdtNow = QDateTime::currentDateTime();
    query.exec(QString("UPDATE settings SET data ='%1' "
                       "WHERE value='mythfilldatabaseLastRunEnd'")
                       .arg(qdtNow.toString("yyyy-MM-dd hh:mm")));

    if (!quiet)
        cout << "Clearing data for source...\n";
    QDateTime fromlocaldt = MythUTCToLocal(ddprocessor.getActualListingsFrom());
    QDateTime tolocaldt = MythUTCToLocal(ddprocessor.getActualListingsTo());

    if (!quiet)
        cout << "Clearing from " << fromlocaldt.toString() 
             << " to " << tolocaldt.toString() << " (localtime)\n";

    clearDataBySource(source.id, fromlocaldt,tolocaldt);
    if (!quiet)
        cout << "Data for source cleared...\n";

    if (!quiet)
        cout << "Main temp tables populated.  Updating myth channels...\n";
    DataDirectStationUpdate(source);

    if (!quiet)
        cout << "Channels updated..  Updating programs...\n";
    DataDirectProgramUpdate(source);

    return true;
}

// XMLTV stuff

QString getFirstText(QDomElement element)
{
    for (QDomNode dname = element.firstChild(); !dname.isNull();
         dname = dname.nextSibling())
    {
        QDomText t = dname.toText();
        if (!t.isNull())
            return t.data();
    }
    return "";
}

ChanInfo *parseChannel(QDomElement &element, QUrl baseUrl) 
{
    ChanInfo *chaninfo = new ChanInfo;

    QString xmltvid = element.attribute("id", "");
    QStringList split = QStringList::split(" ", xmltvid);

    bool xmltvisjunk = false;

    if (isNorthAmerica)
    {
        if (xmltvid.contains("zap2it"))
        {
            xmltvisjunk = true;
            chaninfo->chanstr = "";
            chaninfo->xmltvid = xmltvid;
            chaninfo->callsign = "";
        }
        else
        {
            chaninfo->xmltvid = split[0];
            chaninfo->chanstr = split[0];
            if (split.size() > 1)
                chaninfo->callsign = split[1];
            else
                chaninfo->callsign = "";
        }
    }
    else
    {
        chaninfo->callsign = "";
        chaninfo->chanstr = "";
        chaninfo->xmltvid = xmltvid;
    }

    chaninfo->iconpath = "";
    chaninfo->name = "";
    chaninfo->finetune = "";
    chaninfo->tvformat = "Default";

    for (QDomNode child = element.firstChild(); !child.isNull();
         child = child.nextSibling())
    {
        QDomElement info = child.toElement();
        if (!info.isNull())
        {
            if (info.tagName() == "icon")
            {
                QUrl iconUrl(baseUrl, info.attribute("src", ""), true);
                chaninfo->iconpath = iconUrl.toString();
            }
            else if (info.tagName() == "display-name")
            {
                if (chaninfo->name.length() == 0)
                {
                    chaninfo->name = info.text();
                    if (xmltvisjunk)
                    {
                        QStringList split = QStringList::split(" ", 
                                                               chaninfo->name);
          
                        if (split[0] == "Channel")
                        { 
                            chaninfo->old_xmltvid = split[1];
                            chaninfo->chanstr = split[1];
                            if (split.size() > 2)
                                chaninfo->callsign = split[2];
                        }
                        else
                        {
                            chaninfo->old_xmltvid = split[0];
                            chaninfo->chanstr = split[0];
                            if (split.size() > 1)
                                chaninfo->callsign = split[1];
                        }
                    }
                }
                else if (isJapan && chaninfo->callsign.length() == 0)
                {
                    chaninfo->callsign = info.text();
                }
                else if (chaninfo->chanstr.length() == 0)
                {
                    chaninfo->chanstr = info.text();
                }
            }
        }
    }

    chaninfo->freqid = chaninfo->chanstr;
    return chaninfo;
}

int TimezoneToInt (QString timezone)
{
    // we signal an error by setting it invalid (> 840min = 14hr)
    int result = 841;

    if (timezone.length() == 5)
    {
        bool ok;

        result = timezone.mid(1,2).toInt(&ok, 10);

        if (!ok)
            result = 841;
        else
        {
            result *= 60;

            int min = timezone.right(2).toInt(&ok, 10);

            if (!ok)
                result = 841;
            else
            {
                result += min;
                if (timezone.left(1) == "-")
                    result *= -1;
            }
        }
    }
    return result;
}

// localTimezoneOffset: 841 == "None", -841 == "Auto", other == fixed offset
void fromXMLTVDate(QString &timestr, QDateTime &dt, int localTimezoneOffset = 841)
{
    if (timestr.isEmpty())
    {
        cerr << "Ignoring empty timestamp." << endl;
        return;
    }

    QStringList split = QStringList::split(" ", timestr);
    QString ts = split[0];
    bool ok;
    int year = 0, month = 0, day = 0, hour = 0, min = 0, sec = 0;

    if (ts.length() == 14)
    {
        year  = ts.left(4).toInt(&ok, 10);
        month = ts.mid(4,2).toInt(&ok, 10);
        day   = ts.mid(6,2).toInt(&ok, 10);
        hour  = ts.mid(8,2).toInt(&ok, 10);
        min   = ts.mid(10,2).toInt(&ok, 10);
        sec   = ts.mid(12,2).toInt(&ok, 10);
    }
    else if (ts.length() == 12)
    {
        year  = ts.left(4).toInt(&ok, 10);
        month = ts.mid(4,2).toInt(&ok, 10);
        day   = ts.mid(6,2).toInt(&ok, 10);
        hour  = ts.mid(8,2).toInt(&ok, 10);
        min   = ts.mid(10,2).toInt(&ok, 10);
        sec   = 0;
    }
    else
    {
        cerr << "Ignoring unknown timestamp format: " << ts << endl;
        return;
    }

    dt = QDateTime(QDate(year, month, day),QTime(hour, min, sec));

    if ((split.size() > 1) && (localTimezoneOffset <= 840))
    {
        QString tmp = split[1];
        tmp.stripWhiteSpace();

        int ts_offset = TimezoneToInt(tmp);
        if (abs(ts_offset) > 840)
        {
            ts_offset = 0;
            localTimezoneOffset = 841;
        }
        dt = dt.addSecs(-ts_offset * 60);
    }

    if (localTimezoneOffset < -840)
    {
        dt = MythUTCToLocal(dt);
    }
    else if (abs(localTimezoneOffset) <= 840)
    {
        dt = dt.addSecs(localTimezoneOffset * 60 );
    }

    timestr = dt.toString("yyyyMMddhhmmss");
}

void parseCredits(QDomElement &element, ProgInfo *pginfo)
{
    for (QDomNode child = element.firstChild(); !child.isNull();
         child = child.nextSibling())
    {
        QDomElement info = child.toElement();
        if (!info.isNull())
        {
            ProgCredit credit;
            credit.role = info.tagName();
            credit.name = getFirstText(info);
            pginfo->credits.append(credit);
        }
    }
}

ProgInfo *parseProgram(QDomElement &element, int localTimezoneOffset)
{
    QString uniqueid, seriesid, season, episode;
    ProgInfo *pginfo = new ProgInfo;
 
    pginfo->previouslyshown = pginfo->stereo = pginfo->subtitled =
    pginfo->hdtv = pginfo->closecaptioned = false;

    pginfo->subtitle = pginfo->title = pginfo->desc =
    pginfo->category = pginfo->content = pginfo->catType =
    pginfo->syndicatedepisodenumber =  pginfo->partnumber =
    pginfo->parttotal = pginfo->showtype = pginfo->colorcode =
    pginfo->stars = "";

    QString text = element.attribute("start", "");
    fromXMLTVDate(text, pginfo->start, localTimezoneOffset);
    pginfo->startts = text;

    text = element.attribute("stop", "");
    fromXMLTVDate(text, pginfo->end, localTimezoneOffset);
    pginfo->endts = text;

    text = element.attribute("channel", "");
    QStringList split = QStringList::split(" ", text);   
 
    pginfo->channel = split[0];

    text = element.attribute("clumpidx", "");
    if (!text.isEmpty()) 
    {
        split = QStringList::split("/", text);
        pginfo->clumpidx = split[0];
        pginfo->clumpmax = split[1];
    }

    for (QDomNode child = element.firstChild(); !child.isNull();
         child = child.nextSibling())
    {
        QDomElement info = child.toElement();
        if (!info.isNull())
        {
            if (info.tagName() == "title")
            {
                if (isJapan)
                {
                    if (info.attribute("lang") == "ja_JP")
                    {
                        pginfo->title = getFirstText(info);
                    }
                    else if (info.attribute("lang") == "ja_JP@kana")
                    {
                        pginfo->title_pronounce = getFirstText(info);
                    }
                }
                else if (pginfo->title == "")
                {
                    pginfo->title = getFirstText(info);
                }
            }
            else if (info.tagName() == "sub-title" && pginfo->subtitle == "")
            {
                pginfo->subtitle = getFirstText(info);
            }
            else if (info.tagName() == "content")
            {
                pginfo->content = getFirstText(info);
            }
            else if (info.tagName() == "desc" && pginfo->desc == "")
            {
                pginfo->desc = getFirstText(info);
            }
            else if (info.tagName() == "category")
            {
                QString cat = getFirstText(info);
                
                if (cat == "movie" || cat == "series" || 
                    cat == "sports" || cat == "tvshow")
                {
                    if (pginfo->catType.isEmpty())
                        pginfo->catType = cat;
                }
                else if (pginfo->category.isEmpty())
                {
                    pginfo->category = cat;
                }

                if ((cat == "Film" || cat == "film") && !isNorthAmerica)
                {
                    // Hack for tv_grab_uk_rt
                    pginfo->catType = "movie";
                }
            }
            else if (info.tagName() == "date" && pginfo->airdate == "")
            {
                // Movie production year
                QString date = getFirstText(info);
                pginfo->airdate = date.left(4);
            }
            else if (info.tagName() == "star-rating")
            {
                QDomNodeList values = info.elementsByTagName("value");
                QDomElement item;
                QString stars, num, den;
                float avg = 0.0;
                // not sure why the XML suggests multiple ratings,
                // but the following will average them anyway.
                for (unsigned int i = 0; i < values.length(); i++)
                {
                    item = values.item(i).toElement();
                    if (item.isNull())
                        continue;
                    stars = getFirstText(item);
                    num = stars.section('/', 0, 0);
                    den = stars.section('/', 1, 1);
                    if (0.0 >= den.toFloat())
                        continue;
                    avg *= i/(i+1);
                    avg += (num.toFloat()/den.toFloat()) / (i+1);
                }
                pginfo->stars.setNum(avg);
            }
            else if (info.tagName() == "rating")
            {
                // again, the structure of ratings seems poorly represented
                // in the XML.  no idea what we'd do with multiple values.
                QDomNodeList values = info.elementsByTagName("value");
                QDomElement item = values.item(0).toElement();
                if (item.isNull())
                    continue;
                ProgRating rating;
                rating.system = info.attribute("system", "");
                rating.rating = getFirstText(item);
                if ("" != rating.system)
                    pginfo->ratings.append(rating);
            }
            else if (info.tagName() == "previously-shown")
            {
                pginfo->previouslyshown = true;

                QString prevdate = getFirstText(info);
                pginfo->originalairdate = prevdate;
            } 
            else if (info.tagName() == "credits")
            {
                parseCredits(info, pginfo);
            }
            else if (info.tagName() == "episode-num" &&
                     info.attribute("system") == "xmltv_ns")
            {
                int tmp;
                QString episodenum(getFirstText(info));
                episode = episodenum.section('.',1,1);
                episode = episode.section('/',0,0).stripWhiteSpace();
                season = episodenum.section('.',0,0).stripWhiteSpace();
                QString part(episodenum.section('.',2,2));
                QString partnumber(part.section('/',0,0).stripWhiteSpace());
                QString parttotal(part.section('/',1,1).stripWhiteSpace());

                pginfo->catType = "series";

                if (!episode.isEmpty())
                {
                    tmp = episode.toInt() + 1;
                    episode = QString::number(tmp);
                    pginfo->syndicatedepisodenumber = QString("E" + episode);
                }

                if (!season.isEmpty())
                {
                    tmp = season.toInt() + 1;
                    season = QString::number(tmp);
                    pginfo->syndicatedepisodenumber.append(QString("S" + season));
                }

                if (!partnumber.isEmpty())
                {                
                    tmp = partnumber.toInt() + 1;
                    partnumber = QString::number(tmp);
                }
                
                if (partnumber != 0 && parttotal >= partnumber && !parttotal.isEmpty())
                {
                    pginfo->parttotal = parttotal;
                    pginfo->partnumber = partnumber;
                }
            }
            else if (info.tagName() == "episode-num" &&
                     info.attribute("system") == "onscreen" &&
                     pginfo->subtitle.isEmpty())
            {
                 pginfo->catType = "series";
                 pginfo->subtitle = getFirstText(info);
            }
        }
    }

    if (pginfo->category.isEmpty() && !pginfo->catType.isEmpty())
        pginfo->category = pginfo->catType;

    /* Do what MythWeb does and assume that programmes with
       star-rating in America are movies. This allows us to
       unify app code with grabbers which explicitly deliver that
       info. */
    if (isNorthAmerica && pginfo->catType == "" &&
        pginfo->stars != "" && pginfo->airdate != "")
        pginfo->catType = "movie";
    
    /* Hack for teveblad grabber to do something with the content tag*/
    if (pginfo->content != "")
    {
        if (pginfo->category == "film")
        {
            pginfo->subtitle = pginfo->desc;
            pginfo->desc = pginfo->content;
        }
        else if (pginfo->desc != "") 
        {
            pginfo->desc = pginfo->desc + " - " + pginfo->content;
        }
        else if (pginfo->desc == "")
        {
            pginfo->desc = pginfo->content;
        }
    }
    
    if (pginfo->airdate.isEmpty())
        pginfo->airdate = QDate::currentDate().toString("yyyy");

    /* Let's build ourself a programid */
    QString programid;
    
    if (pginfo->catType == "movie")
        programid = "MV";
    else if (pginfo->catType == "series")
        programid = "EP";
    else if (pginfo->catType == "sports")
        programid = "SP";
    else
        programid = "SH";
    
    if (!uniqueid.isEmpty()) // we already have a unique id ready for use
        programid.append(uniqueid);
    else
    {
        if (seriesid.isEmpty()) //need to hash ourself a seriesid from the title
        {
            seriesid = QString::number(ELFHash(pginfo->title));
        }
        pginfo->seriesid = seriesid;
        programid.append(seriesid);

        if (!episode.isEmpty() && !season.isEmpty())
        {
            programid.append(episode);
            programid.append(season);
            if (!pginfo->partnumber.isEmpty() && !pginfo->parttotal.isEmpty())
            {
                programid.append(pginfo->partnumber);
                programid.append(pginfo->parttotal);
            }
        }
        else
        {
            /* No ep/season info? Well then remove the programid and rely on
               normal dupchecking methods instead. */
            if (pginfo->catType != "movie")
                programid = "";
        }
    }
    
    pginfo->programid = programid;

    return pginfo;
}
                  
bool parseFile(QString filename, QValueList<ChanInfo> *chanlist,
               QMap<QString, QValueList<ProgInfo> > *proglist)
{
    QDomDocument doc;
    QFile f;

    if (!dash_open(f, filename, IO_ReadOnly))
    {
        cerr << QString("Error unable to open '%1' for reading.")
                .arg(filename)
             << endl;
        return false;
    }

    QString errorMsg = "unknown";
    int errorLine = 0;
    int errorColumn = 0;

    if (!doc.setContent(&f, &errorMsg, &errorLine, &errorColumn))
    {
        cerr << "Error in " << errorLine << ":" << errorColumn << ": "
             << errorMsg << endl;

        f.close();
        return true;
    }

    f.close();

    // now we calculate the localTimezoneOffset, so that we can fix
    // the programdata if needed
    QString config_offset = gContext->GetSetting("TimeOffset", "None");
    // we disable this feature by setting it invalid (> 840min = 14hr)
    int localTimezoneOffset = 841;

    if (config_offset == "Auto")
    {
        localTimezoneOffset = -841; // we mark auto with the -ve of the disable magic number
    }
    else if (config_offset != "None")
    {
        localTimezoneOffset = TimezoneToInt(config_offset);
        if (abs(localTimezoneOffset) > 840)
        {
            cerr << "Ignoring invalid TimeOffset " << config_offset << endl;
            localTimezoneOffset = 841;
        }
    }

    QDomElement docElem = doc.documentElement();

    QUrl baseUrl(docElem.attribute("source-data-url", ""));

    QUrl sourceUrl(docElem.attribute("source-info-url", ""));
    if (sourceUrl.toString() == "http://labs.zap2it.com/")
    {
        cerr << "Don't use tv_grab_na_dd, use the internal datadirect grabber."
             << endl;
        exit(FILLDB_BUGGY_EXIT_SRC_IS_DD);
    }

    QString aggregatedTitle;
    QString aggregatedDesc;
    QString groupingTitle;
    QString groupingDesc;

    QDomNode n = docElem.firstChild();
    while (!n.isNull())
    {
        QDomElement e = n.toElement();
        if (!e.isNull()) 
        {
            if (e.tagName() == "channel")
            {
                ChanInfo *chinfo = parseChannel(e, baseUrl);
                chanlist->push_back(*chinfo);
                delete chinfo;
            }
            else if (e.tagName() == "programme")
            {
                ProgInfo *pginfo = parseProgram(e, localTimezoneOffset);

                if (pginfo->startts == pginfo->endts)
                {
                    /* Not a real program : just a grouping marker */
                    if (!pginfo->title.isEmpty())
                        groupingTitle = pginfo->title + " : ";

                    if (!pginfo->desc.isEmpty())
                        groupingDesc = pginfo->desc + " : ";
                }
                else
                {
                    if (pginfo->clumpidx.isEmpty())
                    {
                        if (!groupingTitle.isEmpty())
                        {
                            pginfo->title.prepend(groupingTitle);
                            groupingTitle = "";
                        }

                        if (!groupingDesc.isEmpty())
                        {
                            pginfo->desc.prepend(groupingDesc);
                            groupingDesc = "";
                        }

                        (*proglist)[pginfo->channel].push_back(*pginfo);
                    }
                    else
                    {
                        /* append all titles/descriptions from one clump */
                        if (pginfo->clumpidx.toInt() == 0)
                        {
                            aggregatedTitle = "";
                            aggregatedDesc = "";
                        }

                        if (!pginfo->title.isEmpty())
                        {
                            if (!aggregatedTitle.isEmpty())
                                aggregatedTitle.append(" | ");
                            aggregatedTitle.append(pginfo->title);
                        }

                        if (!pginfo->desc.isEmpty())
                        {
                            if (!aggregatedDesc.isEmpty())
                                aggregatedDesc.append(" | ");
                            aggregatedDesc.append(pginfo->desc);
                        }    
                        if (pginfo->clumpidx.toInt() == 
                            pginfo->clumpmax.toInt() - 1)
                        {
                            pginfo->title = aggregatedTitle;
                            pginfo->desc = aggregatedDesc;
                            (*proglist)[pginfo->channel].push_back(*pginfo);
                        }
                    }
                }
                delete pginfo;
            }
        }
        n = n.nextSibling();
    }

    return true;
}

bool conflict(ProgInfo &a, ProgInfo &b)
{
    if ((a.start <= b.start && b.start < a.end) ||
        (b.end <= a.end && a.start < b.end))
        return true;
    return false;
}

void fixProgramList(QValueList<ProgInfo> *fixlist)
{
    qHeapSort(*fixlist);

    QValueList<ProgInfo>::iterator i = fixlist->begin();
    QValueList<ProgInfo>::iterator cur;
    while (1)    
    {
        cur = i;
        i++;
        // fill in miss stop times
        if ((*cur).endts == "" || (*cur).startts > (*cur).endts)
        {
            if (i != fixlist->end())
            {
                (*cur).endts = (*i).startts;
                (*cur).end = (*i).start;
            }
            else
            {
                (*cur).end = (*cur).start;
                if ((*cur).end < QDateTime((*cur).end.date(), QTime(6, 0)))
                {
                    (*cur).end.setTime(QTime(6, 0));
                }
                else
                {
                   (*cur).end.setTime(QTime(0, 0));
                   (*cur).end.setDate((*cur).end.date().addDays(1));
                }

                (*cur).endts = (*cur).end.toString("yyyyMMddhhmmss").ascii();
            }
        }
        if (i == fixlist->end())
            break;
        // remove overlapping programs
        if (conflict(*cur, *i))
        {
            QValueList<ProgInfo>::iterator tokeep, todelete;

            if ((*cur).end <= (*cur).start)
                tokeep = i, todelete = cur;
            else if ((*i).end <= (*i).start)
                tokeep = cur, todelete = i;
            else if ((*cur).subtitle != "" && (*i).subtitle == "")
                tokeep = cur, todelete = i;
            else if ((*i).subtitle != "" && (*cur).subtitle == "")
                tokeep = i, todelete = cur;
            else if ((*cur).desc != "" && (*i).desc == "")
                tokeep = cur, todelete = i;
            else if ((*i).desc != "" && (*cur).desc == "")
                tokeep = i, todelete = cur;
            else
                tokeep = i, todelete = cur;

            if (!quiet)
            {
                cerr << "removing conflicting program: "
                     << (*todelete).channel << " "
                     << (*todelete).title.local8Bit() << " "
                     << (*todelete).startts << "-" << (*todelete).endts << endl;
                cerr << "conflicted with             : "
                     << (*tokeep).channel << " "
                     << (*tokeep).title.local8Bit() << " "
                     << (*tokeep).startts << "-" <<   (*tokeep).endts << endl;
                cerr << endl;
            }

            if (todelete == i)
                i = cur;
            fixlist->erase(todelete);
        }
    }
}

QString getResponse(const QString &query, const QString &def)
{
    cout << query;

    if (def != "")
    {
        cout << " [" << (const char *)def.local8Bit() << "]  ";
    }
    
    char response[80];
    cin.getline(response, 80);

    QString qresponse = QString::fromLocal8Bit(response);

    if (qresponse == "")
        qresponse = def;

    return qresponse;
}

unsigned int promptForChannelUpdates(QValueList<ChanInfo>::iterator chaninfo, 
                                     unsigned int chanid)
{
    if (chanid == 0)
    {
        // Default is 0 to allow rapid skipping of many channels,
        // in some xmltv outputs there may be over 100 channel, but
        // only 10 or so that are available in each area.
        chanid = atoi(getResponse("Choose a channel ID (positive integer) ",
                                  "0"));

        // If we wish to skip this channel, use the default 0 and return.
        if (chanid == 0)
            return(0);
    }

    (*chaninfo).name = getResponse("Choose a channel name (any string, "
                                   "long version) ",(*chaninfo).name);
    (*chaninfo).callsign = getResponse("Choose a channel callsign (any string, "
                                       "short version) ",(*chaninfo).callsign);

    if (channel_preset)
    {
        (*chaninfo).chanstr = getResponse("Choose a channel preset (0..999) ",
                                         (*chaninfo).chanstr);
        (*chaninfo).freqid  = getResponse("Choose a frequency id (just like "
                                          "xawtv) ",(*chaninfo).freqid);
    }
    else
    {
        (*chaninfo).chanstr  = getResponse("Choose a channel number (just like "
                                           "xawtv) ",(*chaninfo).chanstr);
        (*chaninfo).freqid = (*chaninfo).chanstr;
    }

    (*chaninfo).finetune = getResponse("Choose a channel fine tune offset (just"
                                       " like xawtv) ",(*chaninfo).finetune);

    (*chaninfo).tvformat = getResponse("Choose a TV format "
                                       "(PAL/SECAM/NTSC/ATSC/Default) ",
                                       (*chaninfo).tvformat);

    (*chaninfo).iconpath = getResponse("Choose a channel icon image (any path "
                                       "name) ",(*chaninfo).iconpath);

    return(chanid);
}

static QString SetupIconCacheDirectory()
{
    QString fileprefix = MythContext::GetConfDir();

    QDir dir(fileprefix);
    if (!dir.exists())
        dir.mkdir(fileprefix);

    fileprefix += "/channels";

    dir = QDir(fileprefix);
    if (!dir.exists())
        dir.mkdir(fileprefix);

    return fileprefix;
}

void handleChannels(int id, QValueList<ChanInfo> *chanlist)
{
    QString fileprefix = SetupIconCacheDirectory();

    QDir::setCurrent(fileprefix);

    fileprefix += "/";

    QValueList<ChanInfo>::iterator i = chanlist->begin();
    for (; i != chanlist->end(); i++)
    {
        QString localfile = "";

        if ((*i).iconpath != "")
        {
            QDir remotefile = QDir((*i).iconpath);
            QString filename = remotefile.dirName();

            localfile = fileprefix + filename;
            QFile actualfile(localfile);
            if (!actualfile.exists())
            {
                QString command = QString("wget ") + (*i).iconpath;
                system(command);
            }
        }

        MSqlQuery query(MSqlQuery::InitCon());

        QString querystr;

        if ((*i).old_xmltvid != "")
        {
            querystr.sprintf("SELECT xmltvid FROM channel WHERE xmltvid = \"%s\"",
                             (*i).old_xmltvid.ascii());
            query.exec(querystr);

            if (query.isActive() && query.numRowsAffected() > 0)
            {
                if (!quiet)
                    cout << "Converting old xmltvid (" << (*i).old_xmltvid << ") to new ("
                         << (*i).xmltvid << ")\n";

                query.exec(QString("UPDATE channel SET xmltvid = '%1' WHERE xmltvid = '%2'")
                            .arg((*i).xmltvid)
                            .arg((*i).old_xmltvid));

                if (!query.numRowsAffected())
                    MythContext::DBError("xmltvid conversion",query);
            }
        }

        querystr.sprintf("SELECT chanid,name,callsign,channum,finetune,"
                         "icon,freqid,tvformat FROM channel WHERE "
                         "xmltvid = \"%s\" AND sourceid = %d;", 
                         (*i).xmltvid.ascii(), id); 

        query.exec(querystr);
        if (query.isActive() && query.numRowsAffected() > 0)
        {
            query.next();

            QString chanid = query.value(0).toString();
            if (interactive)
            {
                QString name     = QString::fromUtf8(query.value(1).toString());
                QString callsign = QString::fromUtf8(query.value(2).toString());
                QString chanstr  = QString::fromUtf8(query.value(3).toString());
                QString finetune = QString::fromUtf8(query.value(4).toString());
                QString icon     = QString::fromUtf8(query.value(5).toString());
                QString freqid   = QString::fromUtf8(query.value(6).toString());
                QString tvformat = QString::fromUtf8(query.value(7).toString());

                cout << "### " << endl;
                cout << "### Existing channel found" << endl;
                cout << "### " << endl;
                cout << "### xmltvid  = " << (*i).xmltvid.local8Bit() << endl;
                cout << "### chanid   = " << chanid.local8Bit()       << endl;
                cout << "### name     = " << name.local8Bit()         << endl;
                cout << "### callsign = " << callsign.local8Bit()     << endl;
                cout << "### channum  = " << chanstr.local8Bit()      << endl;
                if (channel_preset)
                    cout << "### freqid   = " << freqid.local8Bit()   << endl;
                cout << "### finetune = " << finetune.local8Bit()     << endl;
                cout << "### tvformat = " << tvformat.local8Bit()     << endl;
                cout << "### icon     = " << icon.local8Bit()         << endl;
                cout << "### " << endl;

                (*i).name = name;
                (*i).callsign = callsign;
                (*i).chanstr  = chanstr;
                (*i).finetune = finetune;
                (*i).freqid = freqid;
                (*i).tvformat = tvformat;

                promptForChannelUpdates(i, atoi(chanid.ascii()));

                if ((*i).callsign == "")
                    (*i).callsign = chanid;

                if (name     != (*i).name ||
                    callsign != (*i).callsign ||
                    chanstr  != (*i).chanstr ||
                    finetune != (*i).finetune ||
                    freqid   != (*i).freqid ||
                    icon     != localfile ||
                    tvformat != (*i).tvformat)
                {
                    MSqlQuery subquery(MSqlQuery::InitCon());
                    subquery.prepare("UPDATE channel SET chanid = :CHANID, "
                                     "name = :NAME, callsign = :CALLSIGN, "
                                     "channum = :CHANNUM, finetune = :FINE, "
                                     "icon = :ICON, freqid = :FREQID, "
                                     "tvformat = :TVFORMAT "
                                     " WHERE xmltvid = :XMLTVID "
                                     "AND sourceid = :SOURCEID;");
                    subquery.bindValue(":CHANID", chanid);
                    subquery.bindValue(":NAME", (*i).name.utf8());
                    subquery.bindValue(":CALLSIGN", (*i).callsign.utf8());
                    subquery.bindValue(":CHANNUM", (*i).chanstr);
                    subquery.bindValue(":FINE", (*i).finetune.toInt());
                    subquery.bindValue(":ICON", localfile);
                    subquery.bindValue(":FREQID", (*i).freqid);
                    subquery.bindValue(":TVFORMAT", (*i).tvformat);
                    subquery.bindValue(":XMLTVID", (*i).xmltvid);
                    subquery.bindValue(":SOURCEID", id);

                    if (!subquery.exec())
                    {
                        cerr << "DB Error: Channel update failed, SQL query "
                             << "was:" << endl;
                        cerr << querystr << endl;
                    }
                    else
                    {
                        cout << "### " << endl;
                        cout << "### Change performed" << endl;
                        cout << "### " << endl;
                    }
                }
                else
                {
                    cout << "### " << endl;
                    cout << "### Nothing changed" << endl;
                    cout << "### " << endl;
                }
            }
            else
            {
                if (!non_us_updating && localfile != "")
                {
                    MSqlQuery subquery(MSqlQuery::InitCon());
                    subquery.prepare("UPDATE channel SET icon = :ICON WHERE "
                                     "chanid = :CHANID;");
                    subquery.bindValue(":ICON", localfile);
                    subquery.bindValue(":CHANID", chanid);

                    if (!subquery.exec())
                        MythContext::DBError("Channel icon change", subquery);
                }
            }
        }
        else
        {
            int major, minor, freqid = (*i).freqid.toInt();
            long long freq;
            get_atsc_stuff((*i).chanstr, id, freqid, major, minor, freq);

            if (interactive && ((minor == 0) || (freq > 0)))
            {
                cout << "### " << endl;
                cout << "### New channel found" << endl;
                cout << "### " << endl;
                cout << "### name     = " << (*i).name.local8Bit()     << endl;
                cout << "### callsign = " << (*i).callsign.local8Bit() << endl;
                cout << "### channum  = " << (*i).chanstr.local8Bit()  << endl;
                if (channel_preset)
                    cout << "### freqid   = " << freqid                << endl;
                cout << "### finetune = " << (*i).finetune.local8Bit() << endl;
                cout << "### tvformat = " << (*i).tvformat.local8Bit() << endl;
                cout << "### icon     = " << localfile.local8Bit()     << endl;
                cout << "### " << endl;

                uint chanid = promptForChannelUpdates(i,0);

                if ((*i).callsign == "")
                    (*i).callsign = QString::number(chanid);

                int mplexid = 0;
                if ((chanid > 0) && (minor > 0))
                    mplexid = ChannelUtil::CreateMultiplex(id,   "atsc",
                                                           freq, "8vsb");

                if (((mplexid > 0) || ((minor == 0) && (chanid > 0))) &&
                    ChannelUtil::CreateChannel(
                        mplexid,          id,               chanid,
                        (*i).callsign,    (*i).name,        (*i).chanstr,
                        0 /*service id*/, major,            minor,
                        false /*use on air guide*/, false /*hidden*/,
                        false /*hidden in guide*/,
                        freqid,           localfile,        (*i).tvformat))
                {
                    cout << "### " << endl;
                    cout << "### Channel inserted" << endl;
                    cout << "### " << endl;
                }
                else
                {
                    cout << "### " << endl;
                    cout << "### Channel skipped" << endl;
                    cout << "### " << endl;
                }
            }
            else if (!non_us_updating && ((minor == 0) || (freq > 0)))
            {
                // We only do this if we are not asked to skip it with the
                // --updating flag.
                int mplexid = 0, chanid = 0;
                if (minor > 0)
                {
                    mplexid = ChannelUtil::CreateMultiplex(
                        id, "atsc", freq, "8vsb");
                }

                if ((mplexid > 0) || (minor == 0))
                    chanid = ChannelUtil::CreateChanID(id, (*i).chanstr);

                if ((*i).callsign == "")
                    (*i).callsign = QString::number(chanid);

                if (chanid > 0)
                {
                    ChannelUtil::CreateChannel(
                        mplexid,          id,        chanid, 
                        (*i).callsign,    (*i).name, (*i).chanstr,
                        0 /*service id*/, major,     minor,
                        false /*use on air guide*/,  false /*hidden*/,
                        false /*hidden in guide*/,
                        freqid,      localfile, (*i).tvformat);
                }
            }
        }
    }

    UpdateSourceIcons(id);
}

void clearDBAtOffset(int offset, int chanid, QDate *qCurrentDate)
{
    if (no_delete)
        return;

    QDate newDate; 
    if (qCurrentDate == 0)
    {
        newDate = QDate::currentDate();
        qCurrentDate = &newDate;
    }

    int nextoffset = 1;

    if (offset == -1)
    {
        offset = 0;
        nextoffset = 10;
    }

    QDateTime from, to;
    from.setDate(*qCurrentDate);
    from = from.addDays(offset);
    from = from.addSecs(listing_wrap_offset);
    to = from.addDays(nextoffset);

    clearDataByChannel(chanid, from, to);
}

void handlePrograms(int id, QMap<QString, QValueList<ProgInfo> > *proglist)
{
    int unchanged = 0, updated = 0;
    QMap<QString, QValueList<ProgInfo> >::Iterator mapiter;

    for (mapiter = proglist->begin(); mapiter != proglist->end(); ++mapiter)
    {
        MSqlQuery query(MSqlQuery::InitCon());

        if (mapiter.key() == "")
            continue;

        int chanid = 0;

        query.prepare("SELECT chanid FROM channel WHERE sourceid = :ID AND "
                      "xmltvid = :XMLTVID;"); 
        query.bindValue(":ID", id);
        query.bindValue(":XMLTVID", mapiter.key());

        query.exec();

        if (query.isActive() && query.numRowsAffected() > 0)
        {
            query.next();
            chanid = query.value(0).toInt();
        }

        if (chanid == 0)
        {
            cerr << "Unknown xmltv channel identifier: " << mapiter.key()
                 << endl << "Skipping channel.\n";
            continue;
        }

        QValueList<ProgInfo> *sortlist = &((*proglist)[mapiter.key()]);

        fixProgramList(sortlist);

        QValueList<ProgInfo>::iterator i = sortlist->begin();
        for (; i != sortlist->end(); i++)
        {
            QString startstr = (*i).start.toString(Qt::ISODate);
            QString endstr = (*i).end.toString(Qt::ISODate);

            query.prepare("SELECT * FROM program WHERE "
                          "chanid=:CHANID AND "
                          "starttime=:START AND "
                          "endtime=:END AND "
                          "title=:TITLE AND "
                          "subtitle=:SUBTITLE AND "
                          "description=:DESC AND "
                          "category=:CATEGORY AND "
                          "category_type=:CATEGORY_TYPE AND "
                          "airdate=:AIRDATE AND "
                          "stars=:STARS AND "
                          "previouslyshown=:PREVIOUSLYSHOWN AND "
                          "title_pronounce=:TITLE_PRONOUNCE AND "
                          "stereo=:STEREO AND "
                          "subtitled=:SUBTITLED AND "
                          "hdtv=:HDTV AND "
                          "closecaptioned=:CLOSECAPTIONED AND "
                          "partnumber=:PARTNUMBER AND "
                          "parttotal=:PARTTOTAL AND "
                          "seriesid=:SERIESID AND "
                          "showtype=:SHOWTYPE AND "
                          "colorcode=:COLORCODE AND "
                          "syndicatedepisodenumber=:SYNDICATEDEPISODENUMBER AND "
                          "programid=:PROGRAMID;");
            query.bindValue(":CHANID", chanid);
            query.bindValue(":START", startstr);
            query.bindValue(":END", endstr);
            query.bindValue(":TITLE", (*i).title.utf8());
            query.bindValue(":SUBTITLE", (*i).subtitle.utf8());
            query.bindValue(":DESC", (*i).desc.utf8());
            query.bindValue(":CATEGORY", (*i).category.utf8());
            query.bindValue(":CATEGORY_TYPE", (*i).catType.utf8());
            query.bindValue(":AIRDATE", (*i).airdate.utf8());
            query.bindValue(":STARS", (*i).stars.utf8());
            query.bindValue(":PREVIOUSLYSHOWN", (*i).previouslyshown);
            query.bindValue(":TITLE_PRONOUNCE", (*i).title_pronounce.utf8());
            query.bindValue(":STEREO", (*i).stereo);
            query.bindValue(":SUBTITLED", (*i).subtitled);
            query.bindValue(":HDTV", (*i).hdtv);
            query.bindValue(":CLOSECAPTIONED", (*i).closecaptioned);
            query.bindValue(":PARTNUMBER", (*i).partnumber);
            query.bindValue(":PARTTOTAL", (*i).parttotal);
            query.bindValue(":SERIESID", (*i).seriesid);
            query.bindValue(":SHOWTYPE", (*i).showtype);
            query.bindValue(":COLORCODE", (*i).colorcode);
            query.bindValue(":SYNDICATEDEPISODENUMBER", (*i).syndicatedepisodenumber);
            query.bindValue(":PROGRAMID", (*i).programid);
            query.exec();

            if (query.isActive() && query.numRowsAffected() > 0)
            {
                unchanged++;
                continue;
            }

            query.prepare("SELECT title,starttime,endtime FROM program WHERE "
                          "chanid=:CHANID AND starttime>=:START AND "
                          "starttime<:END;");
            query.bindValue(":CHANID", chanid);
            query.bindValue(":START", startstr);
            query.bindValue(":END", endstr);
            query.exec();

            if (query.isActive() && query.numRowsAffected() > 0)
            {
                if (!quiet)
                {
                    while(query.next())
                    {
                        cerr << "removing existing program: "
                             << (*i).channel.local8Bit() << " "
                             << QString::fromUtf8(query.value(0).toString()).local8Bit() << " "
                             << query.value(1).toDateTime().toString(Qt::ISODate) << " - "
                             << query.value(2).toDateTime().toString(Qt::ISODate) << endl;
                    }

                    cerr << "inserting new program    : "
                         << (*i).channel.local8Bit() << " "
                         << (*i).title.local8Bit() << " "
                         << startstr << " - " << endstr << endl << endl;
                }

                MSqlQuery subquery(MSqlQuery::InitCon());
                subquery.prepare("DELETE FROM program WHERE "
                                 "chanid=:CHANID AND starttime>=:START "
                                 "AND starttime<:END;");
                subquery.bindValue(":CHANID", chanid);
                subquery.bindValue(":START", startstr);
                subquery.bindValue(":END", endstr);

                subquery.exec();

                subquery.prepare("DELETE FROM programrating WHERE "
                                 "chanid=:CHANID AND starttime>=:START "
                                 "AND starttime<:END;");
                subquery.bindValue(":CHANID", chanid);
                subquery.bindValue(":START", startstr);
                subquery.bindValue(":END", endstr);

                subquery.exec();

                subquery.prepare("DELETE FROM credits WHERE "
                                 "chanid=:CHANID AND starttime>=:START "
                                 "AND starttime<:END;");
                subquery.bindValue(":CHANID", chanid);
                subquery.bindValue(":START", startstr);
                subquery.bindValue(":END", endstr);

                subquery.exec();
            }

            query.prepare("INSERT INTO program (chanid,starttime,endtime,"
                          "title,subtitle,description,category,category_type,"
                          "airdate,stars,previouslyshown,title_pronounce,stereo,"
                          "subtitled,hdtv,closecaptioned,partnumber,parttotal,"
                          "seriesid,originalairdate,showtype,colorcode,"
                          "syndicatedepisodenumber,programid) "
                          "VALUES(:CHANID,:STARTTIME,:ENDTIME,:TITLE,"
                          ":SUBTITLE,:DESCRIPTION,:CATEGORY,:CATEGORY_TYPE,:AIRDATE,:STARS,"
                          ":PREVIOUSLYSHOWN,:TITLE_PRONOUNCE,:STEREO,:SUBTITLED,"
                          ":HDTV,:CLOSECAPTIONED,:PARTNUMBER,:PARTTOTAL,:SERIESID,"
                          ":ORIGINALAIRDATE,:SHOWTYPE,:COLORCODE,:SYNDICATEDEPISODENUMBER,"
                          ":PROGRAMID);");
            query.bindValue(":CHANID", chanid);
            query.bindValue(":STARTTIME", startstr);
            query.bindValue(":ENDTIME", endstr);
            query.bindValue(":TITLE", (*i).title.utf8());
            query.bindValue(":SUBTITLE", (*i).subtitle.utf8());
            query.bindValue(":DESCRIPTION", (*i).desc.utf8());
            query.bindValue(":CATEGORY", (*i).category.utf8());
            query.bindValue(":CATEGORY_TYPE", (*i).catType.utf8());
            query.bindValue(":AIRDATE", (*i).airdate.utf8());
            query.bindValue(":STARS", (*i).stars.utf8());
            query.bindValue(":PREVIOUSLYSHOWN", (*i).previouslyshown);
            query.bindValue(":TITLE_PRONOUNCE", (*i).title_pronounce.utf8());
            query.bindValue(":STEREO", (*i).stereo);
            query.bindValue(":SUBTITLED", (*i).subtitled);
            query.bindValue(":HDTV", (*i).hdtv);
            query.bindValue(":CLOSECAPTIONED", (*i).closecaptioned);
            query.bindValue(":PARTNUMBER", (*i).partnumber);
            query.bindValue(":PARTTOTAL", (*i).parttotal);
            query.bindValue(":SERIESID", (*i).seriesid);
            query.bindValue(":ORIGINALAIRDATE", (*i).originalairdate);
            query.bindValue(":SHOWTYPE", (*i).showtype);
            query.bindValue(":COLORCODE", (*i).colorcode);
            query.bindValue(":SYNDICATEDEPISODENUMBER", (*i).syndicatedepisodenumber);
            query.bindValue(":PROGRAMID", (*i).programid);
            if (!query.exec())
            {
                MythContext::DBError("program insert", query);
            }

            updated++;

            QValueList<ProgRating>::iterator j = (*i).ratings.begin();
            for (; j != (*i).ratings.end(); j++)
            {
                query.prepare("INSERT INTO programrating (chanid,starttime,"
                              "system,rating) VALUES (:CHANID, :START, :SYS, "
                              ":RATING);");
                query.bindValue(":CHANID", chanid);
                query.bindValue(":START", startstr);
                query.bindValue(":SYS", (*j).system.utf8());
                query.bindValue(":RATING", (*j).rating.utf8());

                if (!query.exec())
                {
                    MythContext::DBError("programrating insert", query);
                }
            }

            QValueList<ProgCredit>::iterator k = (*i).credits.begin();
            for (; k != (*i).credits.end(); k++)
            {
                query.prepare("SELECT person FROM people WHERE "
                              "name = :NAME;");
                query.bindValue(":NAME", (*k).name.utf8());
                if (!query.exec())
                    MythContext::DBError("person lookup", query);

                int personid = -1;
                if (query.isActive() && query.numRowsAffected() > 0)
                {
                    query.next();
                    personid = query.value(0).toInt();
                }

                if (personid < 0)
                {
                    query.prepare("INSERT INTO people (name) VALUES "
                                  "(:NAME);");
                    query.bindValue(":NAME", (*k).name.utf8());
                    if (!query.exec())
                        MythContext::DBError("person insert", query);

                    query.prepare("SELECT person FROM people WHERE "
                                  "name = :NAME;");
                    query.bindValue(":NAME", (*k).name.utf8());
                    if (!query.exec())
                       MythContext::DBError("person lookup", query);

                    if (query.isActive() && query.numRowsAffected() > 0)
                    {
                        query.next();
                        personid = query.value(0).toInt();
                    }
                }

                if (personid < 0)
                {
                    cerr << "Error inserting person\n";
                    continue;
                }

                query.prepare("INSERT INTO credits (chanid,starttime,"
                              "role,person) VALUES "
                              "(:CHANID, :START, :ROLE, :PERSON);");
                query.bindValue(":CHANID", chanid);
                query.bindValue(":START", startstr);
                query.bindValue(":ROLE", (*k).role.utf8());
                query.bindValue(":PERSON", personid);
                if (!query.exec())
                {
                    // be careful of the startime/timestamp "feature"!
                    query.prepare("UPDATE credits SET "
                                  "role = concat(role,',:ROLE'), "
                                  "starttime = :START "
                                  "WHERE chanid = :CHANID AND "
                                  "starttime = :START2 and person = :PERSON");
                    query.bindValue(":ROLE", (*k).role.utf8());
                    query.bindValue(":START", startstr);
                    query.bindValue(":CHANID", chanid);
                    query.bindValue(":START2", startstr);
                    query.bindValue(":PERSON", personid);

                    if (!query.exec())
                        MythContext::DBError("credits update", query);
                }
            }
        }
    }
    if (!quiet)
    {
        cerr << "Updated programs: " << updated
             << "  Unchanged programs: " << unchanged << endl;
    }
}

bool grabDataFromFile(int id, QString &filename)
{
    QValueList<ChanInfo> chanlist;
    QMap<QString, QValueList<ProgInfo> > proglist;

    if (!parseFile(filename, &chanlist, &proglist))
        return false;

    handleChannels(id, &chanlist);
    handlePrograms(id, &proglist);
    return true;
}

time_t toTime_t(QDateTime &dt)
{
    tm brokenDown;
    brokenDown.tm_sec = dt.time().second();
    brokenDown.tm_min = dt.time().minute();
    brokenDown.tm_hour = dt.time().hour();
    brokenDown.tm_mday = dt.date().day();
    brokenDown.tm_mon = dt.date().month() - 1;
    brokenDown.tm_year = dt.date().year() - 1900;
    brokenDown.tm_isdst = -1;
    int secsSince1Jan1970UTC = (int) mktime( &brokenDown );
    if ( secsSince1Jan1970UTC < -1 )
        secsSince1Jan1970UTC = -1;
    return secsSince1Jan1970UTC;
}

bool grabData(Source source, int offset, QDate *qCurrentDate = 0)
{
    QString xmltv_grabber = source.xmltvgrabber;

    if (xmltv_grabber == "datadirect")
        return grabDDData(source, offset, *qCurrentDate, DD_ZAP2IT);
    else if (xmltv_grabber == "technovera")
        return grabDDData(source, offset, *qCurrentDate, DD_LXM);
        
    char tempfilename[] = "/tmp/mythXXXXXX";
    if (mkstemp(tempfilename) == -1)
    {
        VERBOSE(VB_IMPORTANT,
                QString("Error creating temporary file in /tmp, %1")
                .arg(strerror(errno)));
        exit(FILLDB_BUGGY_EXIT_ERR_OPEN_TMPFILE);
    }

    QString filename = QString(tempfilename);

    QString home = QDir::homeDirPath();
    QString configfile = QString("%1/%2.xmltv").arg(MythContext::GetConfDir())
                                                       .arg(source.name);
    QString command;

    if (xmltv_grabber == "tv_grab_uk")
        command.sprintf("nice %s --days 7 --config-file '%s' --output %s",
                        xmltv_grabber.ascii(), configfile.ascii(), 
                        filename.ascii());
    else if (xmltv_grabber == "tv_grab_uk_rt")
        command.sprintf("nice %s --days 14 --config-file '%s' --output %s",
                        xmltv_grabber.ascii(),
                        configfile.ascii(), filename.ascii());
    else if (xmltv_grabber == "tv_grab_au")
        command.sprintf("nice %s --days 7 --config-file '%s' --output %s",
                        xmltv_grabber.ascii(), configfile.ascii(),
                        filename.ascii());
    else if (xmltv_grabber == "tv_grab_de_tvtoday")
        command.sprintf("nice %s --slow --days 1 --config-file '%s' --offset %d --output %s",
                        xmltv_grabber.ascii(), configfile.ascii(),
                        offset, filename.ascii());
    else if (xmltv_grabber == "tv_grab_fr")
        command.sprintf("nice %s --days 7 '%s' --output %s",
                        xmltv_grabber.ascii(), configfile.ascii(),
                        filename.ascii());
    else if (xmltv_grabber == "tv_grab_nl")
        command.sprintf("nice %s --output %s",
                        xmltv_grabber.ascii(),
                        filename.ascii());
    else if (xmltv_grabber == "tv_grab_fi")
        // Use the default of 10 days for Finland's grabber
        command.sprintf("nice %s --offset %d --config-file '%s' --output %s",
                        xmltv_grabber.ascii(), offset,
                        configfile.ascii(), filename.ascii());
    else if (xmltv_grabber == "tv_grab_es")
        // Use fixed interval of 3 days for Spanish grabber
        command.sprintf("nice %s --days=4  --config-file '%s' --output %s",
                        xmltv_grabber.ascii(), 
                        configfile.ascii(), filename.ascii());
    else if (xmltv_grabber == "tv_grab_jp")
    {
         // Use fixed interval of 7 days for Japanese grabber
         command.sprintf("nice %s --days 7 --enable-readstr --config-file '%s' --output %s",
                         xmltv_grabber.ascii(), configfile.ascii(),
                         filename.ascii());
         isJapan = true;
    }
    else if (xmltv_grabber == "tv_grab_no")
        command.sprintf("nice %s --days 1 --offset %d --config-file '%s' --output %s",
                        xmltv_grabber.ascii(), offset, configfile.ascii(),
                        filename.ascii());
    else if (xmltv_grabber == "tv_grab_se_swedb")
         command.sprintf("nice %s --days 1 --offset %d --config-file '%s' --output %s",
                         xmltv_grabber.ascii(), offset, configfile.ascii(),
                         filename.ascii());
    else if (xmltv_grabber == "tv_grab_dk")
        // Use fixed interval of 7 days for Danish grabber
        command.sprintf("nice %s --days 7 --config-file '%s' --output %s",
                        xmltv_grabber.ascii(), configfile.ascii(),
                        filename.ascii());
    else if (xmltv_grabber == "tv_grab_pt")
        // Use fixed interval of 3 days for Portuguese grabber
        command.sprintf("nice %s --days=4  --config-file '%s' --output %s",
                        xmltv_grabber.ascii(), 
                        configfile.ascii(), filename.ascii());
    else if (xmltv_grabber == "tv_grab_be_tvb")
        command.sprintf("nice %s --days 1 --offset %d --config-file '%s' --output %s",
                        xmltv_grabber.ascii(), offset, configfile.ascii(),
                        filename.ascii());
    else if (xmltv_grabber == "tv_grab_be_tlm")
        command.sprintf("nice %s --days 1 --offset %d --config-file '%s' --output %s",
                        xmltv_grabber.ascii(), offset, configfile.ascii(),
                        filename.ascii());
    else
    {
        isNorthAmerica = true;
        command.sprintf("nice %s --days 1 --offset %d --config-file '%s' "
                        "--output %s", xmltv_grabber.ascii(),
                        offset, configfile.ascii(), filename.ascii());
    }

    if (quiet &&
        (xmltv_grabber == "tv_grab_na" ||
         xmltv_grabber == "tv_grab_de_tvtoday" ||
         xmltv_grabber == "tv_grab_fi" ||
         xmltv_grabber == "tv_grab_es" ||
         xmltv_grabber == "tv_grab_se_swedb" ||
         xmltv_grabber == "tv_grab_no" ||
         xmltv_grabber == "tv_grab_dk" ||
         xmltv_grabber == "tv_grab_uk" ||
         xmltv_grabber == "tv_grab_uk_rt" ||
         xmltv_grabber == "tv_grab_nl" ||
         xmltv_grabber == "tv_grab_fr" ||
         xmltv_grabber == "tv_grab_fi" ||
         xmltv_grabber == "tv_grab_jp" ||
         xmltv_grabber == "tv_grab_pt" ||
         xmltv_grabber == "tv_grab_be_tvb" ||
         xmltv_grabber == "tv_grab_be_tlm"))
         command += " --quiet";


    command += graboptions;

    if (!quiet)
         cout << "----------------- Start of XMLTV output -----------------" << endl;

    QDateTime qdtNow = QDateTime::currentDateTime();
    MSqlQuery query(MSqlQuery::InitCon());
    QString status = "currently running.";

    query.exec(QString("UPDATE settings SET data ='%1' "
                       "WHERE value='mythfilldatabaseLastRunStart'")
                       .arg(qdtNow.toString("yyyy-MM-dd hh:mm")));

    query.exec(QString("UPDATE settings SET data ='%1' "
                       "WHERE value='mythfilldatabaseLastRunStatus'")
                       .arg(status));

    int systemcall_status = system(command.ascii());
    bool succeeded = WIFEXITED(systemcall_status) &&
         WEXITSTATUS(systemcall_status) == 0;

    qdtNow = QDateTime::currentDateTime();
    query.exec(QString("UPDATE settings SET data ='%1' "
                       "WHERE value='mythfilldatabaseLastRunEnd'")
                       .arg(qdtNow.toString("yyyy-MM-dd hh:mm")));

    status = "Successful.";

    if (!succeeded)
    {
        status = QString("FAILED:  xmltv returned error code %1.")
                         .arg(systemcall_status);

        query.exec(QString("UPDATE settings SET data ='%1' "
                           "WHERE value='mythfilldatabaseLastRunStatus'")
                           .arg(status));
        if (WIFSIGNALED(systemcall_status) &&
            (WTERMSIG(systemcall_status) == SIGINT || WTERMSIG(systemcall_status) == SIGQUIT))
            interrupted = true;
    }
 
    if (!quiet)
         cout << "------------------ End of XMLTV output ------------------" << endl;

    grabDataFromFile(source.id, filename);

    QFile thefile(filename);
    thefile.remove();

    return succeeded;
}

void grabDataFromDDFile(int id, int offset, const QString &filename,
        const QString &lineupid, QDate *qCurrentDate = 0)
{
    QDate *currentd = qCurrentDate;
    QDate qcd = QDate::currentDate();
    if (!currentd)
        currentd = &qcd;

    ddprocessor.setInputFile(filename);
    Source s;
    s.id = id;
    s.name = "";
    s.xmltvgrabber = "datadirect";
    s.userid = "fromfile";
    s.password = "fromfile";
    s.lineupid = lineupid;

    grabData(s, offset, currentd);
}

void clearOldDBEntries(void)
{
    MSqlQuery query(MSqlQuery::InitCon());
    QString querystr;
    int offset = 1;

    if (no_delete)
    {
        offset = 7;
        if (!quiet)
            cout << "Keeping 7 days of data." << endl;
    }

    querystr.sprintf("DELETE FROM oldprogram WHERE airdate < "
                     "DATE_SUB(CURRENT_DATE, INTERVAL 320 DAY);");
    query.exec(querystr);

    querystr.sprintf("REPLACE INTO oldprogram (oldtitle,airdate) "
                     "SELECT title,starttime FROM program "
                     "WHERE starttime < NOW() AND manualid = 0 "
                     "GROUP BY title;");
    query.exec(querystr);

    querystr.sprintf("DELETE FROM program WHERE starttime <= "
                     "DATE_SUB(CURRENT_DATE, INTERVAL %d DAY);", offset);
    query.exec(querystr);

    querystr.sprintf("DELETE FROM programrating WHERE starttime <= "
                     "DATE_SUB(CURRENT_DATE, INTERVAL %d DAY);", offset);
    query.exec(querystr);

    querystr.sprintf("DELETE FROM programgenres WHERE starttime <= "
                     "DATE_SUB(CURRENT_DATE, INTERVAL %d DAY);", offset);
    query.exec(querystr);

    querystr.sprintf("DELETE FROM credits WHERE starttime <= "
                     "DATE_SUB(CURRENT_DATE, INTERVAL %d DAY);", offset);
    query.exec(querystr);

    querystr.sprintf("DELETE FROM record WHERE (type = %d "
                     "OR type = %d OR type = %d) AND enddate < NOW();",
                     kSingleRecord, kOverrideRecord, kDontRecord);
    query.exec(querystr);

    int cleanOldRecorded = gContext->GetNumSetting( "CleanOldRecorded", 60);

    querystr.sprintf("DELETE FROM oldrecorded WHERE "
                     "recstatus <> %d AND duplicate = 0 AND "
                     "endtime < DATE_SUB(CURRENT_DATE, INTERVAL %d DAY);",
                     rsRecorded, cleanOldRecorded);
    query.exec(querystr);
}

bool fillData(QValueList<Source> &sourcelist)
{
    QValueList<Source>::Iterator it;

    QString status;
    MSqlQuery query(MSqlQuery::InitCon());
    QDateTime GuideDataBefore, GuideDataAfter;

    query.exec(QString("SELECT MAX(endtime) FROM program;"));
    if (query.isActive() && query.numRowsAffected() > 0)
    {
        query.next();

        if (!query.isNull(0))
            GuideDataBefore = QDateTime::fromString(query.value(0).toString(),
                                                    Qt::ISODate);
    }

    int failures = 0;
    for (it = sourcelist.begin(); it != sourcelist.end(); ++it) {
        int chancnt = 0;
        QString xmltv_grabber = (*it).xmltvgrabber;
        if (xmltv_grabber == "tv_grab_uk" || xmltv_grabber == "tv_grab_uk_rt" ||
            xmltv_grabber == "tv_grab_fi" || xmltv_grabber == "tv_grab_es" ||
            xmltv_grabber == "tv_grab_nl" || xmltv_grabber == "tv_grab_au" ||
            xmltv_grabber == "tv_grab_fr" || xmltv_grabber == "tv_grab_jp" ||
            xmltv_grabber == "tv_grab_pt")
        {
            // These don't support the --offset option, so just grab the max.
            if (!grabData(*it, -1))
                ++failures;
        }
        else if (xmltv_grabber == "tv_grab_dk")
        {
            if (!grabData(*it, 0))
                ++failures;
        }
        else if ((xmltv_grabber == "datadirect" || xmltv_grabber == "technovera") && dd_grab_all)
        {
            QDate qCurrentDate = QDate::currentDate();

            grabData(*it, 0, &qCurrentDate);
        }
        else if (xmltv_grabber == "datadirect" ||
                 xmltv_grabber == "technovera" ||
                 xmltv_grabber == "tv_grab_se_swedb" ||
                 xmltv_grabber == "tv_grab_no" ||
                 xmltv_grabber == "tv_grab_ee" ||
                 xmltv_grabber == "tv_grab_de_tvtoday" ||
                 xmltv_grabber == "tv_grab_be_tvb" ||
                 xmltv_grabber == "tv_grab_be_tlm" ||
                 xmltv_grabber == "tv_grab_is" ||
                 xmltv_grabber == "tv_grab_br")
        {
            if (xmltv_grabber == "tv_grab_no")
                listing_wrap_offset = 6 * 3600;

            QDate qCurrentDate = QDate::currentDate();

            if (maxDays == 1)
            {
                refresh_today = true;
                refresh_tomorrow = false;
            }

            if (refresh_today || refresh_all)
            {
                if (!quiet)
                    cout << "Refreshing Today's data" << endl;
                if (!grabData(*it, 0, &qCurrentDate))
                    ++failures;
            }

            if (refresh_tomorrow || refresh_all)
            {
                if (!quiet)
                    cout << "Refreshing Tomorrow's data" << endl;
                if (!grabData(*it, 1, &qCurrentDate))
                    ++failures;
            }

            if (refresh_second || refresh_all)
            {
                if (!quiet)
                    cout << "Refreshing data for 2 days from today" << endl;
                if (!grabData(*it, 2, &qCurrentDate))
                    ++failures;
            }

            int maxday = 9;

            if (xmltv_grabber == "datadirect")
                maxday = 14;
            else if (xmltv_grabber == "technovera")
                maxday = 14;                
            else if (xmltv_grabber == "tv_grab_no")
                maxday = 7;
            else if (xmltv_grabber == "tv_grab_se_swedb")
                maxday = 10;
            else if (xmltv_grabber == "tv_grab_ee")
                maxday = 14;
            else if (xmltv_grabber == "tv_grab_de_tvtoday")
                maxday = 7;
            else if (xmltv_grabber == "tv_grab_be_tvb")
                maxday = 5;
            else if (xmltv_grabber == "tv_grab_be_tlm")
                maxday = 5;

            if (maxDays > 0)
                maxday = maxDays;

            for (int i = 0; i < maxday; i++)
            {
                if ((i == 0 && refresh_today) || (i == 1 && refresh_tomorrow) ||
                    (i == 2 && refresh_second))
                    continue;

                // we need to check and see if the current date has changed 
                // since we started in this loop.  If it has, we need to adjust
                // the value of 'i' to compensate for this.
                if (QDate::currentDate() != qCurrentDate)
                {
                    QDate newDate = QDate::currentDate();
                    i += (newDate.daysTo(qCurrentDate));
                    if (i < 0) 
                        i = 0;
                    qCurrentDate = newDate;
                }

                // Check to see if we already downloaded data for this date
                bool download_needed = false;

                if (refresh_all)
                    download_needed = true;

                QString date(qCurrentDate.addDays(i).toString());

                if (!chancnt)
                {
                    QString qstr = QString("SELECT COUNT(*) FROM channel "
                                           "WHERE sourceid = %1")
                                           .arg((*it).id);
                    MSqlQuery qchan(MSqlQuery::InitCon());
                    qchan.exec(qstr);
                    if (qchan.isActive() && qchan.numRowsAffected() > 0) 
                    {
                        qchan.next();
                        chancnt = qchan.value(0).toInt();
                    }
                    else
                        MythContext::DBError("counting channels per source", 
                                             qchan);
                }
                QString querystr;

                querystr.sprintf("SELECT COUNT(*) FROM program "
                                 "LEFT JOIN channel USING (chanid) "
                                 "WHERE sourceid = %d "
                                 "AND starttime >= DATE_ADD(CURRENT_DATE(), "
                                 "    INTERVAL '%d 18' DAY_HOUR) "
                                 "AND starttime < DATE_ADD(CURRENT_DATE(), "
                                 "    INTERVAL %d DAY)",
                                 (*it).id, i, i+1);
                MSqlQuery query(MSqlQuery::InitCon());
                query.exec(querystr);
               
                if (query.isActive()) 
                {
                    // We also need to get this day's data if there's only a 
                    // suspiciously small amount in the DB.
                    if (!query.numRowsAffected() ||
                        (query.next() && query.value(0).toInt() < chancnt * 4))
                    {
                        download_needed = true;
                    }
                } 
                else
                    MythContext::DBError("checking existing program data", 
                                         query);

                // Now look for programs marked as "To Be Announced" after noon
                if (xmltv_grabber == "tv_grab_uk_rt" && 
                    !download_needed && refresh_tba)
                {
                    querystr.sprintf("SELECT COUNT(*) FROM program "
                                 "LEFT JOIN channel USING (chanid) "
                                 "WHERE sourceid = %d "
                                 "AND starttime >= DATE_ADD(CURRENT_DATE(), "
                                 "    INTERVAL '%d 12' DAY_HOUR) "
                                 "AND starttime < DATE_ADD(CURRENT_DATE(), "
                                 "    INTERVAL %d DAY) "
                                 "AND category = 'TBA'",
                                 (*it).id, i, i+1);
                    MSqlQuery query(MSqlQuery::InitCon());
                    query.exec(querystr);

                    if (query.isActive()) 
                    {
                        if (query.numRowsAffected() || 
                            (query.next() && query.value(0).toInt() >= 1)) 
                        {
                            download_needed = true;
                        }
                    } 
                    else
                        MythContext::DBError("checking existing program data", 
                                             query);
                }

                if (download_needed)
                {
                    if (!quiet)
                        cout << "Fetching data for " << date << endl;
                    if (!grabData(*it, i, &qCurrentDate))
                    {
                        ++failures;
                        if (interrupted)
                        {
                            break;
                        }
                    }
                }
                else
                {
                    if (!quiet)
                        cout << "Data is already present for " << date
                             << ", skipping\n";
                }
            }
        }
        else
        {
            cerr << "Grabbing XMLTV data using " << xmltv_grabber.ascii() 
                 << " is not verified as working.\n";
        }
        if (interrupted)
        {
            break;
        }
    }

    query.exec(QString("SELECT MAX(endtime) FROM program;"));
    if (query.isActive() && query.numRowsAffected() > 0)
    {
        query.next();

        if (!query.isNull(0))
            GuideDataAfter = QDateTime::fromString(query.value(0).toString(),
                                                   Qt::ISODate);
    }

    if (failures == 0)
    {
        if (GuideDataAfter == GuideDataBefore)
            status = "mythfilldatabase ran, but did not insert "
                     "any new data into the Guide.  This can indicate a "
                     "potential grabber failure."; 
        else
            status = "Successful.";

        query.exec(QString("UPDATE settings SET data ='%1' "
                           "WHERE value='mythfilldatabaseLastRunStatus'")
                           .arg(status));
    }

    clearOldDBEntries();

    return (failures == 0);
}

ChanInfo *xawtvChannel(QString &id, QString &channel, QString &fine)
{
    ChanInfo *chaninfo = new ChanInfo;
    chaninfo->xmltvid = id;
    chaninfo->name = id;
    chaninfo->callsign = id;
    if (channel_preset)
        chaninfo->chanstr = id;
    else
        chaninfo->chanstr = channel;
    chaninfo->finetune = fine;
    chaninfo->freqid = channel;
    chaninfo->iconpath = "";
    chaninfo->tvformat = "Default";

    return chaninfo;
}

void readXawtvChannels(int id, QString xawrcfile)
{
    fstream fin(xawrcfile.ascii(), ios::in);
    if (!fin.is_open()) return;

    QValueList<ChanInfo> chanlist;

    QString xawid;
    QString channel;
    QString fine;

    string strLine;
    int nSplitPoint = 0;

    while(!fin.eof())
    {
        getline(fin,strLine);

        if ((strLine[0] != '#') && (!strLine.empty()))
        {
            if (strLine[0] == '[')
            {
                if ((nSplitPoint = strLine.find(']')) > 1)
                {
                    if ((xawid != "") && (channel != ""))
                    {
                        ChanInfo *chinfo = xawtvChannel(xawid, channel, fine);
                        chanlist.push_back(*chinfo);
                        delete chinfo;
                    }
                    xawid = strLine.substr(1, nSplitPoint - 1).c_str();
                    channel = "";
                    fine = "";
                }
            }
            else if ((nSplitPoint = strLine.find('=') + 1) > 0)
            {
                while (strLine.substr(nSplitPoint,1) == " ")
                { ++nSplitPoint; }

                if (!strncmp(strLine.c_str(), "channel", 7))
                {
                    channel = strLine.substr(nSplitPoint, 
                                             strLine.size()).c_str();
                }
                else if (!strncmp(strLine.c_str(), "fine", 4))
                {
                    fine = strLine.substr(nSplitPoint, strLine.size()).c_str();
                }
            }
        }
    }

    if ((xawid != "") && (channel != ""))
    {
        ChanInfo *chinfo = xawtvChannel(xawid, channel, fine);
        chanlist.push_back(*chinfo);
        delete chinfo;
    }

    handleChannels(id, &chanlist);
}

int fix_end_times(void)
{
    int count = 0;
    QString chanid, starttime, endtime, querystr;
    MSqlQuery query1(MSqlQuery::InitCon()), query2(MSqlQuery::InitCon());

    querystr = "SELECT chanid, starttime, endtime FROM program "
               "WHERE (DATE_FORMAT(endtime,\"%H%i\") = \"0000\") "
               "ORDER BY chanid, starttime;";

    if (!query1.exec(querystr))
    {
        cerr << "fix_end_times:  " << querystr << " failed!\n";
        return -1;
    }

    while (query1.next())
    {
        starttime = query1.value(1).toString();
        chanid = query1.value(0).toString();
        endtime = query1.value(2).toString();

        querystr = QString("SELECT chanid, starttime, endtime FROM program "
                           "WHERE (DATE_FORMAT(starttime, \"%%Y-%%m-%%d\") = "
                           "\"%1\") AND chanid = \"%2\" "
                           "ORDER BY starttime LIMIT 1;")
                           .arg(endtime.left(10))
                           .arg(chanid);

        if (!query2.exec(querystr))
        {
            cerr << "fix_end_times:  " << querystr << " failed!\n";
            return -1;
        }

        if (query2.next() && (endtime != query2.value(1).toString()))
        {
            count++;
            endtime = query2.value(1).toString();
            querystr = QString("UPDATE program SET starttime = \"%1\", "
                               "endtime = \"%2\" WHERE (chanid = \"%3\" AND "
                               "starttime = \"%4\");")
                               .arg(starttime)
                               .arg(endtime)
                               .arg(chanid)
                               .arg(starttime);

            if (!query2.exec(querystr)) 
            {
                cerr << "fix_end_times:  " << querystr << " failed!\n";
                return -1;
            }
        }
    }

    return count;
}

int main(int argc, char *argv[])
{
    QApplication a(argc, argv, false);
    int argpos = 1;
    int fromfile_id = 1;
    int fromfile_offset = 0;
    QString fromfile_name;
    bool from_xawfile = false;
    int fromxawfile_id = 1;
    QString fromxawfile_name;

    bool grab_data = true;

    bool export_iconmap = false;
    bool import_iconmap = false;
    bool reset_iconmap = false;
    bool reset_iconmap_icons = false;
    QString import_icon_map_filename("iconmap.xml");
    QString export_icon_map_filename("iconmap.xml");

    bool update_icon_map = false;

    bool from_dd_file = false;
    QString fromddfile_lineupid;

    while (argpos < a.argc())
    {
        // The manual and update flags should be mutually exclusive.
        if (!strcmp(a.argv()[argpos], "--manual"))
        {
            cout << "###\n";
            cout << "### Running in manual channel configuration mode.\n";
            cout << "### This will ask you questions about every channel.\n";
            cout << "###\n";
            interactive = true;
        }
        else if (!strcmp(a.argv()[argpos], "--preset"))
        {
            // For using channel preset values instead of channel numbers.
            cout << "###\n";
            cout << "### Running in preset channel configuration mode.\n";
            cout << "### This will assign channel ";
            cout << "preset numbers to every channel.\n";
            cout << "###\n";
            channel_preset = true;
        }
        else if (!strcmp(a.argv()[argpos], "--update"))
        {
            // For running non-destructive updates on the database for
            // users in xmltv zones that do not provide channel data.
            non_us_updating = true;
        }
        else if (!strcmp(a.argv()[argpos], "--no-delete"))
        {
            // Do not delete old programs from the database until 7 days old.
            no_delete = true;
        }
        else if (!strcmp(a.argv()[argpos], "--file"))
        {
            if (((argpos + 3) >= a.argc()) ||
                !strncmp(a.argv()[argpos + 1], "--", 2) ||
                !strncmp(a.argv()[argpos + 2], "--", 2) ||
                !strncmp(a.argv()[argpos + 3], "--", 2))
            {
                printf("missing or invalid parameters for --file option\n");
                return FILLDB_EXIT_INVALID_CMDLINE;
            }

            fromfile_id = atoi(a.argv()[++argpos]);
            fromfile_offset = atoi(a.argv()[++argpos]);
            fromfile_name = a.argv()[++argpos];

            if (!quiet)
                cout << "### bypassing grabbers, reading directly from file\n";
            from_file = true;
        }
        else if (!strcmp(a.argv()[argpos], "--dd-file"))
        {
            if (((argpos + 4) >= a.argc()) ||
                !strncmp(a.argv()[argpos + 1], "--", 2) ||
                !strncmp(a.argv()[argpos + 2], "--", 2) ||
                !strncmp(a.argv()[argpos + 3], "--", 2) ||
                !strncmp(a.argv()[argpos + 4], "--", 2))
            {
                printf("missing or invalid parameters for --dd-file option\n");
                return FILLDB_EXIT_INVALID_CMDLINE;
            }

            fromfile_id = atoi(a.argv()[++argpos]);
            fromfile_offset = atoi(a.argv()[++argpos]);
            fromddfile_lineupid = a.argv()[++argpos];
            fromfile_name = a.argv()[++argpos];

            if (!quiet)
                cout << "### bypassing grabbers, reading directly from file\n";
            from_dd_file = true;
        }
        else if (!strcmp(a.argv()[argpos], "--xawchannels"))
        {
            if (((argpos + 2) >= a.argc()) ||
                !strncmp(a.argv()[argpos + 1], "--", 2) ||
                !strncmp(a.argv()[argpos + 2], "--", 2))
            {
                printf("missing or invalid parameters for --xawchannels option\n");
                return FILLDB_EXIT_INVALID_CMDLINE;
            }

            fromxawfile_id = atoi(a.argv()[++argpos]);
            fromxawfile_name = a.argv()[++argpos];

            if (!quiet)
                 cout << "### reading channels from xawtv configfile\n";
            from_xawfile = true;
        }
        else if (!strcmp(a.argv()[argpos], "--do_channel_updates"))
        {
            channel_updates = true;
        }
        else if (!strcmp(a.argv()[argpos], "--graboptions"))
        {
            if (((argpos + 1) >= a.argc()))
            {
                printf("missing parameter for --graboptions option\n");
                return FILLDB_EXIT_INVALID_CMDLINE;
            }

            graboptions = QString(" ") + QString(a.argv()[++argpos]);
        }
        else if (!strcmp(a.argv()[argpos], "--max-days"))
        {
            if (((argpos + 1) >= a.argc()))
            {
                printf("missing parameter for --max-days option\n");
                return FILLDB_EXIT_INVALID_CMDLINE;
            }

            maxDays = QString(a.argv()[++argpos]).toInt();

            if (maxDays < 1 || maxDays > 21)
            {
                printf("ignoring invalid parameter for --max-days\n");
                maxDays = 0;
            }
        }
        else if (!strcmp(a.argv()[argpos], "--refresh-today"))
        {
            refresh_today = true;
        }
        else if (!strcmp(a.argv()[argpos], "--dont-refresh-tomorrow"))
        {
            refresh_tomorrow = false;
        }
        else if (!strcmp(a.argv()[argpos], "--refresh-second"))
        {
            refresh_second = true;
        }
        else if (!strcmp(a.argv()[argpos], "--refresh-all"))
        {
            refresh_all = true;
        }
        else if (!strcmp(a.argv()[argpos], "--dont-refresh-tba"))
        {
            refresh_tba = false;
        }
#if 0
        else if (!strcmp(a.argv()[argpos], "--dd-grab-all"))
        {
            dd_grab_all = true;
            refresh_today = false;
            refresh_tomorrow = false;
            refresh_second = false;
        }
#endif
        else if (!strcmp(a.argv()[argpos], "--quiet"))
        {
             quiet = true;
        }
        else if (!strcmp(a.argv()[argpos], "--mark-repeats"))
        {
             mark_repeats = true;
        }
        else if (!strcmp(a.argv()[argpos], "--nomark-repeats"))
        {
             mark_repeats = false;
        }
        else if (!strcmp(a.argv()[argpos], "--export-icon-map"))
        {
            export_iconmap = true;
            grab_data = false;

            if ((argpos + 1) >= a.argc() ||
                    !strncmp(a.argv()[argpos + 1], "--", 2))
            {
                if (!isatty(fileno(stdout)))
                {
                    quiet = true;
                    export_icon_map_filename = "-";
                }
            }
            else
            {
                export_icon_map_filename = a.argv()[++argpos];
            }
        }
        else if (!strcmp(a.argv()[argpos], "--import-icon-map"))
        {
            import_iconmap = true;
            grab_data = false;

            if ((argpos + 1) >= a.argc() ||
                    !strncmp(a.argv()[argpos + 1], "--", 2))
            {
                if (!isatty(fileno(stdin)))
                {
                    import_icon_map_filename = "-";
                }
            }
            else
            {
                import_icon_map_filename = a.argv()[++argpos];
            }
        }
        else if (!strcmp(a.argv()[argpos], "--update-icon-map"))
        {
            update_icon_map = true;
            grab_data = false;
        }
        else if (!strcmp(a.argv()[argpos], "--reset-icon-map"))
        {
            reset_iconmap = true;
            grab_data = false;

            if ((argpos + 1) < a.argc() &&
                    strncmp(a.argv()[argpos + 1], "--", 2))
            {
                ++argpos;
                if (QString(a.argv()[argpos]) == "all")
                {
                    reset_iconmap_icons = true;
                }
                else
                {
                    cerr << "Unknown icon group '" << a.argv()[argpos]
                            << "' for --reset-icon-map option" << endl;
                    return FILLDB_EXIT_UNKNOWN_ICON_GROUP;
                }
            }
        }
        else if (!strcmp(a.argv()[argpos], "-h") ||
                 !strcmp(a.argv()[argpos], "--help"))
        {
            cout << "usage:\n";
            cout << "--manual\n";
            cout << "   Run in manual channel configuration mode\n";
            cout << "   This will ask you questions about every channel\n";
            cout << "\n";
            cout << "--update\n";
            cout << "   For running non-destructive updates on the database for\n";
            cout << "   users in xmltv zones that do not provide channel data\n";
            cout << "\n";
            cout << "--preset\n";
            cout << "   Use it in case that you want to assign a preset number for\n";
            cout << "   each channel, useful for non US countries where people\n";
            cout << "   are used to assigning a sequenced number for each channel, i.e.:\n";
            cout << "   1->TVE1(S41), 2->La 2(SE18), 3->TV3(21), 4->Canal 33(60)...\n";
            cout << "\n";
            cout << "--no-delete\n";
            cout << "   Do not delete old programs from the database until 7 days old.\n";
            cout << "\n";
            cout << "--file <sourceid> <offset> <xmlfile>\n";
            cout << "   Bypass the grabbers and read data directly from a file\n";
            cout << "   <sourceid> = cardinput\n";
            cout << "   <offset>   = days from today that xmlfile defines\n";
            cout << "                (-1 means to replace all data, up to 10 days)\n";
            cout << "   <xmlfile>  = file to read\n";
            cout << "\n";
            cout << "--dd-file <sourceid> <offset> <lineupid> <xmlfile>\n";
            cout << "   <sourceid> = see --file\n";
            cout << "   <offset>   = see --file\n";
            cout << "   <lineupid> = the lineup id\n";
            cout << "   <xmlfile>  = see --file\n";
            cout << "\n";
            cout << "--xawchannels <sourceid> <xawtvrcfile>\n";
            cout << "   (--manual flag works in combination with this)\n";
            cout << "   Read channels as defined in xawtvrc file given\n";
            cout << "   <sourceid>    = cardinput\n";
            cout << "   <xawtvrcfile> = file to read\n";
            cout << "\n";
            cout << "--do_channel_updates\n";
            cout << "   When using data direct, ask mythfilldatabase to\n";
            cout << "   overwrite channel names, frequencies, etc. with the\n";
            cout << "   values available from the data source. This will \n";
            cout << "   override custom channel names, which is why it is\n";
            cout << "   off by default.\n";
            cout << "\n";
            cout << "--graboptions <\"options\">\n";
            cout << "   Pass options to grabber\n";
            cout << "\n";
            cout << "--max-days <number>\n";
            cout << "   Force the maximum number of days, counting today,\n";
            cout << "   for the grabber to check for future listings\n";
            cout << "--refresh-today\n";
            cout << "--refresh-second\n";
            cout << "--refresh-all\n";
            cout << "   (Only valid for grabbers: na, se_swedb, no, ee, de_tvtoday)\n";
            cout << "   Force a refresh today or two days (or every day) from now,\n";
            cout << "   to catch the latest changes\n";
            cout << "--dont-refresh-tomorrow\n";
            cout << "   Tomorrow will always be refreshed unless this argument is used\n";
            cout << "--dont-refresh-tba\n";
            cout << "   \"To be announced\" programs will always be refreshed \n";
            cout << "   unless this argument is used\n";
            cout << "\n";
            cout << "--export-icon-map [<filename>]\n";
            cout << "   Exports your current icon map to <filename> (default: "
                    << export_icon_map_filename << ")\n";
            cout << "--import-icon-map [<filename>]\n";
            cout << "   Imports an icon map from <filename> (default: " <<
                    import_icon_map_filename << ")\n";
            cout << "--update-icon-map\n";
            cout << "   Updates icon map icons only\n";
            cout << "--reset-icon-map [all]\n";
            cout << "   Resets your icon map (pass all to reset channel icons as well)\n";
            cout << "\n";
            cout << "--mark-repeats\n";
            cout << "   Marks any programs with a OriginalAirDate earlier\n";
            cout << "   than their start date as a repeat\n";
            cout << "\n";
#if 0
            cout << "--dd-grab-all\n";
            cout << "   The DataDirect grabber will grab all available data\n";
#endif
            cout << "--help\n";
            cout << "   This text\n";
            cout << "\n";
            cout << "\n";
            cout << "  --manual and --update can not be used together.\n";
            cout << "\n";
            return FILLDB_EXIT_INVALID_CMDLINE;
        }
        else
        {
            fprintf(stderr, "illegal option: '%s' (use --help)\n",
                    a.argv()[argpos]);
            return FILLDB_EXIT_INVALID_CMDLINE;
        }

        ++argpos;
    }

    gContext = NULL;
    gContext = new MythContext(MYTH_BINARY_VERSION);
    if(!gContext->Init(false))
    {
        VERBOSE(VB_IMPORTANT, "Failed to init MythContext, exiting.");
        return FILLDB_EXIT_NO_MYTHCONTEXT;
    }

    gContext->LogEntry("mythfilldatabase", LP_INFO,
                       "Listings Download Started", "");
    
    
    if (!grab_data)
    {
    }
    else if (from_xawfile)
    {
        readXawtvChannels(fromxawfile_id, fromxawfile_name);
    }
    else if (from_file)
    {
        QString status = "currently running.";
        QDateTime GuideDataBefore, GuideDataAfter;

        MSqlQuery query(MSqlQuery::InitCon());
        query.exec(QString("UPDATE settings SET data ='%1' "
                           "WHERE value='mythfilldatabaseLastRunStart'")
                           .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm")));

        query.exec(QString("UPDATE settings SET data ='%1' "
                           "WHERE value='mythfilldatabaseLastRunStatus'")
                           .arg(status));

        query.exec(QString("SELECT MAX(endtime) FROM program;"));
        if (query.isActive() && query.numRowsAffected() > 0)
        {
            query.next();

            if (!query.isNull(0))
                GuideDataBefore = QDateTime::fromString(query.value(0).toString(),
                                                    Qt::ISODate);
        }

        if (!grabDataFromFile(fromfile_id, fromfile_name))
            return FILLDB_EXIT_GRAB_DATA_FAILED;

        clearOldDBEntries();

        query.exec(QString("UPDATE settings SET data ='%1' "
                           "WHERE value='mythfilldatabaseLastRunEnd'")
                          .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm")));

        query.exec(QString("SELECT MAX(endtime) FROM program;"));
        if (query.isActive() && query.numRowsAffected() > 0)
        {
            query.next();

            if (!query.isNull(0))
                GuideDataAfter = QDateTime::fromString(query.value(0).toString(),
                                                   Qt::ISODate);
        }

        if (GuideDataAfter == GuideDataBefore)
            status = "mythfilldatabase ran, but did not insert "
                     "any new data into the Guide.  This can indicate a "
                     "potential problem with the XML file used for the update.";
        else
            status = "Successful.";

        query.exec(QString("UPDATE settings SET data ='%1' "
                           "WHERE value='mythfilldatabaseLastRunStatus'")
                           .arg(status));
    }
    else if (from_dd_file)
    {
        grabDataFromDDFile(fromfile_id, fromfile_offset, fromfile_name,
                fromddfile_lineupid);
        clearOldDBEntries();
    }
    else
    {
        QValueList<Source> sourcelist;

        MSqlQuery sourcequery(MSqlQuery::InitCon());
        QString querystr = QString("SELECT sourceid,name,xmltvgrabber,userid,"
                                   "password,lineupid "
                                   "FROM videosource ORDER BY sourceid;");
        sourcequery.exec(querystr);
        
        if (sourcequery.isActive())
        {
             if (sourcequery.numRowsAffected() > 0)
             {
                  while (sourcequery.next())
                  {
                       Source newsource;
            
                       newsource.id = sourcequery.value(0).toInt();
                       newsource.name = sourcequery.value(1).toString();
                       newsource.xmltvgrabber = sourcequery.value(2).toString();
                       newsource.userid = sourcequery.value(3).toString();
                       newsource.password = sourcequery.value(4).toString();
                       newsource.lineupid = sourcequery.value(5).toString();

                       sourcelist.append(newsource);
                  }
             }
             else
             {
                  cerr << "There are no channel sources defined, did you run "
                       << "the setup program?\n";
                  gContext->LogEntry("mythfilldatabase", LP_CRITICAL,
                                     "No channel sources defined",
                                     "Could not find any defined channel "
                                     "sources - did you run the setup "
                                     "program?");
                  return FILLDB_EXIT_NO_CHAN_SRC;
             }
        }
        else
        {
             MythContext::DBError("loading channel sources", sourcequery);
             return FILLDB_EXIT_DB_ERROR;
        }
    
        if (!fillData(sourcelist))
        {
             cerr << "Failed to fetch some program info\n";
             gContext->LogEntry("mythfilldatabase", LP_WARNING,
                                "Failed to fetch some program info", "");
             return FILLDB_EXIT_DB_ERROR;
        }
    }

    if (reset_iconmap)
    {
        ResetIconMap(reset_iconmap_icons);
    }

    if (import_iconmap)
    {
        ImportIconMap(import_icon_map_filename);
    }

    if (export_iconmap)
    {
        ExportIconMap(export_icon_map_filename);
    }

    if (update_icon_map)
    {
        MSqlQuery query(MSqlQuery::InitCon());
        query.exec("SELECT sourceid FROM videosource ORDER BY sourceid;");
        if (query.isActive() && query.numRowsAffected() > 0)
        {
            while (query.next())
            {
                UpdateSourceIcons(query.value(0).toInt());
            }
        }
    }

    if (grab_data)
    {
        if (!quiet)
             cout << "Adjusting program database end times...\n";
        int update_count = fix_end_times();
        if (update_count == -1)
             cerr << "fix_end_times failed!\a\n";
        else if (!quiet)
             cout << update_count << " replacements made.\n";

        gContext->LogEntry("mythfilldatabase", LP_INFO,
                           "Listings Download Finished", "");
    }
    
    if (mark_repeats)
    {
       if (!quiet)
             cout << "Marking repeats...";
       
        int newEpiWindow = gContext->GetNumSetting( "NewEpisodeWindow", 14);
        
        MSqlQuery query(MSqlQuery::InitCon());
        query.exec( QString( "UPDATE program SET previouslyshown = 1 "
                    "WHERE previouslyshown = 0 "
                    "AND originalairdate is not null "
                    "AND (to_days(starttime) - to_days(originalairdate)) > %1;")
                    .arg(newEpiWindow));
        
        if (!quiet)
            cout << "found " << query.numRowsAffected() << endl;
            
        if (!quiet)
             cout << "Unmarking repeats from grabber that fall within our new episode window...";            
             
        query.exec( QString( "UPDATE program SET previouslyshown = 0 "
                             "WHERE previouslyshown = 1 "
                             "AND originalairdate is not null "
                             "AND (to_days(starttime) - to_days(originalairdate)) <= %1;")
                             .arg(newEpiWindow));             
    
        if (!quiet)
            cout << "found " << query.numRowsAffected() << endl;
    }

    if (1) // limit MSqlQuery's lifetime
    {
        MSqlQuery query(MSqlQuery::InitCon());
        query.exec( "SELECT count(previouslyshown) FROM program WHERE previouslyshown = 1;");
        if (query.isActive() && query.numRowsAffected() > 0)
        {
            query.next();
            if (query.value(0).toInt() != 0)
                query.exec("UPDATE settings SET data = '1' WHERE value = 'HaveRepeats';");
            else
                query.exec("UPDATE settings SET data = '0' WHERE value = 'HaveRepeats';");
        }
    }

    if (grab_data || mark_repeats)
        ScheduledRecording::signalChange(-1);

    delete gContext;

    return FILLDB_EXIT_OK;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
