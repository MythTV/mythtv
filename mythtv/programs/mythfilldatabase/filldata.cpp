// POSIX headers
#include <unistd.h>
#include <signal.h>

// Std C headers
#include <cstdlib>
#include <ctime>

// C++ headers
#include <fstream>
using namespace std;

// Qt headers
#include <QDateTime>
#include <QProcess>
#include <QFile>
#include <QList>
#include <QMap>
#include <QDir>

// libmyth headers
#include "exitcodes.h"
#include "mythverbose.h"
#include "mythdbcon.h"
#include "compat.h"
#include "util.h"
#include "mythdirs.h"
#include "mythdb.h"

// libmythtv headers
#include "videosource.h" // for is_grabber..

// filldata headers
#include "filldata.h"

#define LOC QString("FillData: ")
#define LOC_WARN QString("FillData, Warning: ")
#define LOC_ERR QString("FillData, Error: ")

bool updateLastRunEnd(MSqlQuery &query)
{
    QDateTime qdtNow = QDateTime::currentDateTime();
    query.prepare("UPDATE settings SET data = :ENDTIME "
                  "WHERE value='mythfilldatabaseLastRunEnd'");

    query.bindValue(":ENDTIME", qdtNow);

    if (!query.exec())
    {
        MythDB::DBError("updateLastRunEnd", query);
        return false;
    }
    return true;
}

bool updateLastRunStart(MSqlQuery &query)
{
    QDateTime qdtNow = QDateTime::currentDateTime();
    query.prepare("UPDATE settings SET data = :STARTTIME "
                  "WHERE value='mythfilldatabaseLastRunStart'");

    query.bindValue(":STARTTIME", qdtNow);

    if (!query.exec())
    {
        MythDB::DBError("updateLastRunStart", query);
        return false;
    }
    return true;
}

bool updateLastRunStatus(MSqlQuery &query, QString &status)
{
    query.prepare("UPDATE settings SET data = :STATUS "
                  "WHERE value='mythfilldatabaseLastRunStatus'");

    query.bindValue(":STATUS", status);

    if (!query.exec())
    {
        MythDB::DBError("updateLastRunStatus", query);
        return false;
    }
    return true;
}

void FillData::SetRefresh(int day, bool set)
{
    if (kRefreshClear == day)
    {
        refresh_all = set;
        refresh_day.clear();
    }
    else if (kRefreshAll == day)
    {
        refresh_all = set;
    }
    else
    {
        refresh_day[(uint)day] = set;
    }
}

// DataDirect stuff
void FillData::DataDirectStationUpdate(Source source, bool update_icons)
{
    DataDirectProcessor::UpdateStationViewTable(source.lineupid);

    bool insert_channels = chan_data.insert_chan(source.id);
    int new_channels = DataDirectProcessor::UpdateChannelsSafe(
        source.id, insert_channels, chan_data.filter_new_channels);

    //  User must pass "--do-channel-updates" for these updates
    if (chan_data.channel_updates)
    {
        DataDirectProcessor::UpdateChannelsUnsafe(
            source.id, chan_data.filter_new_channels);
    }
    // TODO delete any channels which no longer exist in listings source

    if (update_icons)
        icon_data.UpdateSourceIcons(source.id);

    // Unselect channels not in users lineup for DVB, HDTV
    if (!insert_channels && (new_channels > 0) &&
        is_grabber_labs(source.xmltvgrabber))
    {
        bool ok0 = (logged_in == source.userid);
        bool ok1 = (raw_lineup == source.id);
        if (!ok0)
        {
            VERBOSE(VB_GENERAL, "Grabbing login cookies for listing update");
            ok0 = ddprocessor.GrabLoginCookiesAndLineups();
        }
        if (ok0 && !ok1)
        {
            VERBOSE(VB_GENERAL, "Grabbing listing for listing update");
            ok1 = ddprocessor.GrabLineupForModify(source.lineupid);
        }
        if (ok1)
        {
            ddprocessor.UpdateListings(source.id);
            VERBOSE(VB_GENERAL, QString("Removed %1 channel(s) from lineup.")
                    .arg(new_channels));
        }
    }
}

bool FillData::DataDirectUpdateChannels(Source source)
{
    if (get_datadirect_provider(source.xmltvgrabber) >= 0)
    {
        ddprocessor.SetListingsProvider(
            get_datadirect_provider(source.xmltvgrabber));
    }
    else
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR +
                "We only support DataDirectUpdateChannels with "
                "TMS Labs and Schedules Direct.");
        return false;
    }

    ddprocessor.SetUserID(source.userid);
    ddprocessor.SetPassword(source.password);

    bool ok = true;
    if (!is_grabber_labs(source.xmltvgrabber))
    {
        ok = ddprocessor.GrabLineupsOnly();
    }
    else
    {
        ok = ddprocessor.GrabFullLineup(
            source.lineupid, true, chan_data.insert_chan(source.id)/*only sel*/);
        logged_in  = source.userid;
        raw_lineup = source.id;
    }

    if (ok)
        DataDirectStationUpdate(source, false);

    return ok;
}

bool FillData::GrabDDData(Source source, int poffset,
                          QDate pdate, int ddSource)
{
    if (source.dd_dups.empty())
        ddprocessor.SetCacheData(false);
    else
    {
        VERBOSE(VB_GENERAL, QString(
                    "This DataDirect listings source is "
                    "shared by %1 MythTV lineups")
                .arg(source.dd_dups.size()+1));
        if (source.id > source.dd_dups[0])
        {
            VERBOSE(VB_IMPORTANT, "We should use cached data for this one");
        }
        else if (source.id < source.dd_dups[0])
        {
            VERBOSE(VB_IMPORTANT, "We should keep data around after this one");
        }
        ddprocessor.SetCacheData(true);
    }

    ddprocessor.SetListingsProvider(ddSource);
    ddprocessor.SetUserID(source.userid);
    ddprocessor.SetPassword(source.password);

    bool needtoretrieve = true;

    if (source.userid != lastdduserid)
        dddataretrieved = false;

    if (dd_grab_all && dddataretrieved)
        needtoretrieve = false;

    MSqlQuery query(MSqlQuery::DDCon());
    QString status = QObject::tr("currently running.");

    updateLastRunStart(query);

    if (needtoretrieve)
    {
        VERBOSE(VB_GENERAL, "Retrieving datadirect data.");
        if (dd_grab_all)
        {
            VERBOSE(VB_GENERAL, "Grabbing ALL available data.");
            if (!ddprocessor.GrabAllData())
            {
                VERBOSE(VB_IMPORTANT, "Encountered error in grabbing data.");
                return false;
            }
        }
        else
        {
            QDateTime fromdatetime = QDateTime(pdate).toUTC().addDays(poffset);
            QDateTime todatetime = fromdatetime.addDays(1);

            VERBOSE(VB_GENERAL, QString("Grabbing data for %1 offset %2")
                                          .arg(pdate.toString())
                                          .arg(poffset));
            VERBOSE(VB_GENERAL, QString("From %1 to %2 (UTC)")
                                          .arg(fromdatetime.toString())
                                          .arg(todatetime.toString()));

            if (!ddprocessor.GrabData(fromdatetime, todatetime))
            {
                VERBOSE(VB_IMPORTANT, "Encountered error in grabbing data.");
                return false;
            }
        }

        dddataretrieved = true;
        lastdduserid = source.userid;
    }
    else
    {
        VERBOSE(VB_GENERAL, "Using existing grabbed data in temp tables.");
    }

    VERBOSE(VB_GENERAL,
            QString("Grab complete.  Actual data from %1 to %2 (UTC)")
            .arg(ddprocessor.GetDDProgramsStartAt().toString())
            .arg(ddprocessor.GetDDProgramsEndAt().toString()));

    updateLastRunEnd(query);

    VERBOSE(VB_GENERAL, "Main temp tables populated.");
    if (!channel_update_run)
    {
        VERBOSE(VB_GENERAL, "Updating MythTV channels.");
        DataDirectStationUpdate(source);
        VERBOSE(VB_GENERAL, "Channels updated.");
        channel_update_run = true;
    }

    //cerr << "Creating program view table...\n";
    DataDirectProcessor::UpdateProgramViewTable(source.id);
    //cerr <<  "Finished creating program view table...\n";

    query.prepare("SELECT count(*) from dd_v_program;");
    if (query.exec() && query.size() > 0)
    {
        query.next();
        if (query.value(0).toInt() < 1)
        {
            VERBOSE(VB_GENERAL, "Did not find any new program data.");
            return false;
        }
    }
    else
    {
        VERBOSE(VB_GENERAL, "Failed testing program view table.");
        return false;
    }

    VERBOSE(VB_GENERAL, "Clearing data for source.");
    QDateTime fromlocaldt = ddprocessor.GetDDProgramsStartAt(true);
    QDateTime tolocaldt = ddprocessor.GetDDProgramsEndAt(true);

    VERBOSE(VB_GENERAL, QString("Clearing from %1 to %2 (localtime)")
                                  .arg(fromlocaldt.toString())
                                  .arg(tolocaldt.toString()));
    ProgramData::ClearDataBySource(source.id, fromlocaldt, tolocaldt, true);
    VERBOSE(VB_GENERAL, "Data for source cleared.");

    VERBOSE(VB_GENERAL, "Updating programs.");
    DataDirectProcessor::DataDirectProgramUpdate();
    VERBOSE(VB_GENERAL, "Program table update complete.");

    return true;
}

// XMLTV stuff
bool FillData::GrabDataFromFile(int id, QString &filename)
{
    QList<ChanInfo> chanlist;
    QMap<QString, QList<ProgInfo> > proglist;

    if (!xmltv_parser.parseFile(filename, &chanlist, &proglist))
        return false;

    chan_data.handleChannels(id, &chanlist);
    icon_data.UpdateSourceIcons(id);
    if (proglist.count() == 0)
    {
        VERBOSE(VB_GENERAL,
                QString("No programs found in data."));
        endofdata = true;
    }
    else
    {
        prog_data.HandlePrograms(id, proglist);
    }
    return true;
}

bool FillData::GrabData(Source source, int offset, QDate *qCurrentDate)
{
    QString xmltv_grabber = source.xmltvgrabber;

    int dd_provider = get_datadirect_provider(xmltv_grabber);
    if (dd_provider >= 0)
    {
        if (!GrabDDData(source, offset, *qCurrentDate, dd_provider))
        {
            QStringList errors = ddprocessor.GetFatalErrors();
            for (int i = 0; i < errors.size(); i++)
                fatalErrors.push_back(errors[i]);
            return false;
        }
        return true;
    }

    const QString templatename = "/tmp/mythXXXXXX";
    const QString tempfilename = createTempFile(templatename);
    if (templatename == tempfilename)
    {
        fatalErrors.push_back("Failed to create temporary file.");
        return false;
    }

    QString filename = QString(tempfilename);

    QString home = QDir::homePath();

    QString configfile;

    MSqlQuery query1(MSqlQuery::InitCon());
    query1.prepare("SELECT configpath FROM videosource"
                   " WHERE sourceid = :ID AND configpath IS NOT NULL");
    query1.bindValue(":ID", source.id);
    if (!query1.exec())
    {
        MythDB::DBError("FillData::grabData", query1);
        return false;
    }

    if (query1.next())
        configfile = query1.value(0).toString();
    else
        configfile = QString("%1/%2.xmltv").arg(GetConfDir())
                                           .arg(source.name);

    VERBOSE(VB_GENERAL, QString("XMLTV config file is: %1").arg(configfile));

    QString command = QString("nice %1 --config-file '%2' --output %3")
        .arg(xmltv_grabber).arg(configfile).arg(filename);

    // The one concession to grabber specific behaviour.
    // Will be removed when the grabber allows.
    if (xmltv_grabber == "tv_grab_jp")
    {
        command += QString(" --enable-readstr");
        xmltv_parser.isJapan = true;
    }
    else if (source.xmltvgrabber_prefmethod != "allatonce")
    {
        // XMLTV Docs don't recommend grabbing one day at a
        // time but the current MythTV code is heavily geared
        // that way so until it is re-written behave as
        // we always have done.
        command += QString(" --days 1 --offset %1").arg(offset);
    }

    if (!VERBOSE_LEVEL_CHECK(VB_XMLTV))
        command += " --quiet";

    // Append additional arguments passed to mythfilldatabase
    // using --graboptions
    if (!graboptions.isEmpty())
    {
        command += graboptions;
        VERBOSE(VB_XMLTV, QString("Using graboptions: %1").arg(graboptions));
    }

    MSqlQuery query(MSqlQuery::InitCon());
    QString status = QObject::tr("currently running.");

    updateLastRunStart(query);
    updateLastRunStatus(query, status);

    VERBOSE(VB_XMLTV, QString("Grabber Command: %1").arg(command));

    VERBOSE(VB_XMLTV,
            "----------------- Start of XMLTV output -----------------");

    QByteArray tmp = command.toAscii();
    int systemcall_status = system(tmp.constData());
    bool succeeded = WIFEXITED(systemcall_status) &&
         WEXITSTATUS(systemcall_status) == 0;

    VERBOSE(VB_XMLTV,
            "------------------ End of XMLTV output ------------------");

    updateLastRunEnd(query);

    status = QObject::tr("Successful.");

    if (!succeeded)
    {
        if (WIFSIGNALED(systemcall_status) &&
            (WTERMSIG(systemcall_status) == SIGINT
            || WTERMSIG(systemcall_status) == SIGQUIT))
        {
            interrupted = true;
            status = QString(QObject::tr("FAILED: xmltv ran but was interrupted."));
        }
        else
        {
            status = QString(QObject::tr("FAILED: xmltv returned error code %1."))
                            .arg(systemcall_status);
            VERBOSE(VB_IMPORTANT, LOC_ERR +
                    QString("xmltv returned error code %1")
                    .arg(systemcall_status));
        }
    }

    updateLastRunStatus(query, status);

    succeeded &= GrabDataFromFile(source.id, filename);

    QFile thefile(filename);
    thefile.remove();

    return succeeded;
}

bool FillData::GrabDataFromDDFile(
    int id, int offset, const QString &filename,
    const QString &lineupid, QDate *qCurrentDate)
{
    QDate *currentd = qCurrentDate;
    QDate qcd = QDate::currentDate();
    if (!currentd)
        currentd = &qcd;

    ddprocessor.SetInputFile(filename);
    Source s;
    s.id = id;
    s.xmltvgrabber = "datadirect";
    s.userid = "fromfile";
    s.password = "fromfile";
    s.lineupid = lineupid;

    return GrabData(s, offset, currentd);
}


/** \fn FillData::Run(SourceList &sourcelist)
 *  \brief Goes through the sourcelist and updates its channels with
 *         program info grabbed with the associated grabber.
 *  \return true if there were no failures
 */
bool FillData::Run(SourceList &sourcelist)
{
    SourceList::iterator it;
    SourceList::iterator it2;

    QString status, querystr;
    MSqlQuery query(MSqlQuery::InitCon());
    QDateTime GuideDataBefore, GuideDataAfter;
    int failures = 0;
    int externally_handled = 0;
    int total_sources = sourcelist.size();
    int source_channels = 0;

    QString sidStr = QString("Updating source #%1 (%2) with grabber %3");

    need_post_grab_proc = false;
    int nonewdata = 0;
    bool has_dd_source = false;

    // find all DataDirect duplicates, so we only data download once.
    for (it = sourcelist.begin(); it != sourcelist.end(); ++it)
    {
        if (!is_grabber_datadirect((*it).xmltvgrabber))
            continue;

        has_dd_source = true;
        for (it2 = sourcelist.begin(); it2 != sourcelist.end(); ++it2)
        {
            if (((*it).id           != (*it2).id)           &&
                ((*it).xmltvgrabber == (*it2).xmltvgrabber) &&
                ((*it).userid       == (*it2).userid)       &&
                ((*it).password     == (*it2).password))
            {
                (*it).dd_dups.push_back((*it2).id);
            }
        }
    }
    if (has_dd_source)
        ddprocessor.CreateTempDirectory();

    for (it = sourcelist.begin(); it != sourcelist.end(); ++it)
    {
        if (!fatalErrors.empty())
            break;

        query.prepare("SELECT MAX(endtime) FROM program p LEFT JOIN channel c "
                      "ON p.chanid=c.chanid WHERE c.sourceid= :SRCID "
                      "AND manualid = 0;");
        query.bindValue(":SRCID", (*it).id);

        if (query.exec() && query.size() > 0)
        {
            query.next();

            if (!query.isNull(0))
                GuideDataBefore = QDateTime::fromString(query.value(0).toString(),
                                                        Qt::ISODate);
        }

        channel_update_run = false;
        endofdata = false;

        QString xmltv_grabber = (*it).xmltvgrabber;

        if (xmltv_grabber == "eitonly")
        {
            VERBOSE(VB_GENERAL,
                    QString("Source %1 configured to use only the "
                            "broadcasted guide data. Skipping.")
                    .arg((*it).id));

            externally_handled++;
            updateLastRunStart(query);
            updateLastRunEnd(query);
            continue;
        }
        else if (xmltv_grabber.trimmed().isEmpty() ||
                 xmltv_grabber == "/bin/true" ||
                 xmltv_grabber == "none")
        {
            VERBOSE(VB_GENERAL,
                    QString("Source %1 configured with no grabber. "
                            "Nothing to do.").arg((*it).id));

            externally_handled++;
            updateLastRunStart(query);
            updateLastRunEnd(query);
            continue;
        }

        VERBOSE(VB_GENERAL, sidStr.arg((*it).id)
                                  .arg((*it).name)
                                  .arg(xmltv_grabber));

        query.prepare(
            "SELECT COUNT(chanid) FROM channel WHERE sourceid = "
             ":SRCID AND xmltvid != ''");
        query.bindValue(":SRCID", (*it).id);

        if (query.exec() && query.size() > 0)
        {
            query.next();
            source_channels = query.value(0).toInt();
            if (source_channels > 0)
            {
                VERBOSE(VB_GENERAL, QString("Found %1 channels for "
                                "source %2 which use grabber")
                                .arg(source_channels).arg((*it).id));
            }
            else
            {
                VERBOSE(VB_GENERAL, QString("No channels are "
                    "configured to use grabber."));
            }
        }
        else
        {
            source_channels = 0;
            VERBOSE(VB_GENERAL,
                    QString("Can't get a channel count for source id %1")
                            .arg((*it).id));
        }

        bool hasprefmethod = false;

        if (is_grabber_external(xmltv_grabber))
        {
            QProcess grabber_capabilities_proc;
            grabber_capabilities_proc.setProcessChannelMode(
                QProcess::SeparateChannels);
            grabber_capabilities_proc.start(
                xmltv_grabber,
                QStringList("--capabilities"));

            if (grabber_capabilities_proc.waitForStarted(1000))
            {
                bool ok = grabber_capabilities_proc.waitForFinished(25*1000);

                if (ok &&
                    QProcess::NormalExit ==
                    grabber_capabilities_proc.exitStatus())
                {
                    QString capabilities;
                    grabber_capabilities_proc
                        .setReadChannel(QProcess::StandardOutput);
                    while (grabber_capabilities_proc.canReadLine())
                    {
                        QString capability
                            = grabber_capabilities_proc.readLine().simplified();
                        capabilities += capability + ' ';

                        if (capability == "baseline")
                            (*it).xmltvgrabber_baseline = true;

                        if (capability == "manualconfig")
                            (*it).xmltvgrabber_manualconfig = true;

                        if (capability == "cache")
                            (*it).xmltvgrabber_cache = true;

                        if (capability == "preferredmethod")
                            hasprefmethod = true;
                    }

                    VERBOSE(VB_GENERAL, QString("Grabber has capabilities: %1")
                        .arg(capabilities));
                }
                else
                {
                    VERBOSE(VB_IMPORTANT, QString("%1  --capabilities failed "
                        "or we timed out waiting. You may need to upgrade "
                        "your xmltv grabber").arg(xmltv_grabber));
                }
            }
            else
            {
                grabber_capabilities_proc
                    .setReadChannel(QProcess::StandardOutput);
                QString error = grabber_capabilities_proc.readLine();
                VERBOSE(VB_IMPORTANT, QString("Failed to run %1 "
                        "--capabilities").arg(xmltv_grabber));
            }
        }

        if (hasprefmethod)
        {

            QProcess grabber_method_proc;
            grabber_method_proc.setProcessChannelMode(
                QProcess::SeparateChannels);
            grabber_method_proc.start(
                xmltv_grabber,
                QStringList("--preferredmethod"));

            if (grabber_method_proc.waitForStarted(1000))
            {
                bool ok = grabber_method_proc.waitForFinished(15*1000);

                if (ok &&
                    QProcess::NormalExit == grabber_method_proc.exitStatus())
                {
                    grabber_method_proc
                        .setReadChannel(QProcess::StandardOutput);
                    (*it).xmltvgrabber_prefmethod =
                        grabber_method_proc.readLine().simplified();
                }
                else
                {
                    VERBOSE(VB_IMPORTANT, QString("%1 --preferredmethod failed"
                    " or we timed out waiting. You may need to upgrade your"
                    " xmltv grabber").arg(xmltv_grabber));
                }

                VERBOSE(VB_GENERAL, QString("Grabber prefers method: %1")
                                    .arg((*it).xmltvgrabber_prefmethod));
            }
            else
            {
                grabber_method_proc
                    .setReadChannel(QProcess::StandardOutput);
                QString error = grabber_method_proc.readLine();
                VERBOSE(VB_IMPORTANT,
                        QString("Failed to run %1 --preferredmethod")
                        .arg(xmltv_grabber));
            }
        }

        need_post_grab_proc |= !is_grabber_datadirect(xmltv_grabber);

        if (is_grabber_datadirect(xmltv_grabber) && dd_grab_all)
        {
            if (only_update_channels)
                DataDirectUpdateChannels(*it);
            else
            {
                QDate qCurrentDate = QDate::currentDate();
                if (!GrabData(*it, 0, &qCurrentDate))
                    ++failures;
            }
        }
        else if ((*it).xmltvgrabber_prefmethod == "allatonce")
        {
            if (!GrabData(*it, 0))
                ++failures;
        }
        else if ((*it).xmltvgrabber_baseline ||
                 is_grabber_datadirect(xmltv_grabber))
        {

            QDate qCurrentDate = QDate::currentDate();

            // We'll keep grabbing until it returns nothing
            // Max days currently supported is 21
            int grabdays = (is_grabber_datadirect(xmltv_grabber)) ?
                14 : REFRESH_MAX;

            grabdays = (maxDays > 0)          ? maxDays : grabdays;
            grabdays = (only_update_channels) ? 1       : grabdays;

            vector<bool> refresh_request;
            refresh_request.resize(grabdays, refresh_all);
            for (int i = 0; i < refresh_day.size(); i++)
                refresh_request[i] = refresh_day[i];

            if (is_grabber_datadirect(xmltv_grabber) && only_update_channels)
            {
                DataDirectUpdateChannels(*it);
                grabdays = 0;
            }

            for (int i = 0; i < grabdays; i++)
            {
                if (!fatalErrors.empty())
                    break;

                // We need to check and see if the current date has changed
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

                QString prevDate(qCurrentDate.addDays(i-1).toString());
                QString currDate(qCurrentDate.addDays(i).toString());

                VERBOSE(VB_GENERAL, ""); // add a space between days
                VERBOSE(VB_GENERAL, "Checking day @ " +
                        QString("offset %1, date: %2").arg(i).arg(currDate));

                bool download_needed = false;

                if (refresh_request[i])
                {
                    if ( i == 1 )
                    {
                        VERBOSE(VB_GENERAL,
                            "Data Refresh always needed for tomorrow");
                    }
                    else
                    {
                        VERBOSE(VB_GENERAL,
                            "Data Refresh needed because of user request");
                    }
                    download_needed = true;
                }
                else
                {
                    // Check to see if we already downloaded data for this date.

                    querystr = "SELECT c.chanid, COUNT(p.starttime) "
                               "FROM channel c "
                               "LEFT JOIN program p ON c.chanid = p.chanid "
                               "  AND starttime >= "
                                   "DATE_ADD(DATE_ADD(CURRENT_DATE(), "
                                   "INTERVAL '%1' DAY), INTERVAL '20' HOUR) "
                               "  AND starttime < DATE_ADD(CURRENT_DATE(), "
                                   "INTERVAL '%2' DAY) "
                               "WHERE c.sourceid = %3 AND c.xmltvid != '' "
                               "GROUP BY c.chanid;";

                    if (query.exec(querystr.arg(i-1).arg(i).arg((*it).id)) &&
                        query.isActive())
                    {
                        int prevChanCount = 0;
                        int currentChanCount = 0;
                        int previousDayCount = 0;
                        int currentDayCount = 0;

                        VERBOSE(VB_CHANNEL, QString(
                                "Checking program counts for day %1").arg(i-1));

                        while (query.next())
                        {
                            if (query.value(1).toInt() > 0)
                                prevChanCount++;
                            previousDayCount += query.value(1).toInt();

                            VERBOSE(VB_CHANNEL,
                                    QString("    chanid %1 -> %2 programs")
                                            .arg(query.value(0).toString())
                                            .arg(query.value(1).toInt()));
                        }

                        if (query.exec(querystr.arg(i).arg(i+1).arg((*it).id))
                                && query.isActive())
                        {
                            VERBOSE(VB_CHANNEL, QString("Checking program "
                                                "counts for day %1").arg(i));
                            while (query.next())
                            {
                                if (query.value(1).toInt() > 0)
                                    currentChanCount++;
                                currentDayCount += query.value(1).toInt();

                                VERBOSE(VB_CHANNEL,
                                        QString("    chanid %1 -> %2 programs")
                                                .arg(query.value(0).toString())
                                                .arg(query.value(1).toInt()));
                            }
                        }
                        else
                        {
                            VERBOSE(VB_GENERAL, QString(
                                    "Data Refresh because we are unable to "
                                    "query the data for day %1 to "
                                    "determine if we have enough").arg(i));
                            download_needed = true;
                        }

                        if (currentChanCount < (prevChanCount * 0.90))
                        {
                            VERBOSE(VB_GENERAL, QString(
                                    "Data refresh needed because only %1 out "
                                    "of %2 channels have at least one "
                                    "program listed for day @ offset %3 from "
                                    "8PM - midnight.  Previous day had %4 "
                                    "channels with data in that time period.")
                                    .arg(currentChanCount).arg(source_channels)
                                    .arg(i).arg(prevChanCount));
                            download_needed = true;
                        }
                        else if (currentDayCount == 0)
                        {
                            VERBOSE(VB_GENERAL, QString(
                                    "Data refresh needed because no data "
                                    "exists for day @ offset %1 from 8PM - "
                                    "midnight.").arg(i));
                            download_needed = true;
                        }
                        else if (previousDayCount == 0)
                        {
                            VERBOSE(VB_GENERAL, QString(
                                    "Data refresh needed because no data "
                                    "exists for day @ offset %1 from 8PM - "
                                    "midnight.  Unable to calculate how much "
                                    "we should have for the current day so "
                                    "a refresh is being forced.").arg(i-1));
                            download_needed = true;
                        }
                        else if (currentDayCount < (currentChanCount * 3))
                        {
                            VERBOSE(VB_GENERAL, QString(
                                    "Data Refresh needed because offset day %1 "
                                    "has less than 3 programs "
                                    "per channel for the 8PM - midnight "
                                    "time window for channels that "
                                    "normally have data. "
                                    "We want at least %2 programs, but only "
                                    "found %3").arg(i).arg(currentChanCount * 3)
                                    .arg(currentDayCount));
                            download_needed = true;
                        }
                        else if (currentDayCount < (previousDayCount / 2))
                        {
                            VERBOSE(VB_GENERAL, QString(
                                    "Data Refresh needed because offset day %1 "
                                    "has less than half the number of programs "
                                    "as the previous day for the 8PM - "
                                    "midnight time window. "
                                    "We want at least %2 programs, but only "
                                    "found %3").arg(i)
                                    .arg(previousDayCount / 2)
                                    .arg(currentDayCount));
                            download_needed = true;
                        }
                    }
                    else
                    {
                        VERBOSE(VB_GENERAL, QString(
                                "Data Refresh needed because we are unable to "
                                "query the data for day @ offset %1 to "
                                "determine how much we should have for "
                                "offset day %2.").arg(i-1).arg(i));
                        download_needed = true;
                    }
                }

                if (download_needed)
                {
                    VERBOSE(VB_IMPORTANT,
                            QString("Refreshing data for ") + currDate);
                    if (!GrabData(*it, i, &qCurrentDate))
                    {
                        ++failures;
                        if (!fatalErrors.empty() || interrupted)
                        {
                            break;
                        }
                    }

                    if (endofdata)
                    {
                        VERBOSE(VB_GENERAL, "Grabber is no longer returning "
                                            "program data, finishing");
                        break;
                    }
                }
                else
                {
                    VERBOSE(VB_IMPORTANT,
                            QString("Data is already present for ") + currDate +
                                    ", skipping");
                }
            }
            if (!fatalErrors.empty())
                break;
        }
        else
        {
            VERBOSE(VB_IMPORTANT,
                    QString("Grabbing XMLTV data using ") + xmltv_grabber +
                            " is not supported. You may need to upgrade to"
                            " the latest version of XMLTV.");
        }

        if (interrupted)
        {
            break;
        }

        query.prepare("SELECT MAX(endtime) FROM program p LEFT JOIN channel c "
                      "ON p.chanid=c.chanid WHERE c.sourceid= :SRCID "
                      "AND manualid = 0;");
        query.bindValue(":SRCID", (*it).id);

        if (query.exec() && query.size() > 0)
        {
            query.next();

            if (!query.isNull(0))
                GuideDataAfter = QDateTime::fromString(
                                     query.value(0).toString(), Qt::ISODate);
        }

        if (GuideDataAfter == GuideDataBefore)
        {
            nonewdata++;
        }
    }

    if (!fatalErrors.empty())
    {
        for (int i = 0; i < fatalErrors.size(); i++)
        {
            VERBOSE(VB_IMPORTANT, LOC + "Encountered Fatal Error: " +
                    fatalErrors[i]);
        }
        return false;
    }

    if (only_update_channels && !need_post_grab_proc)
        return true;

    if (failures == 0)
    {
        if (nonewdata > 0 &&
            (total_sources != externally_handled))
            status = QString(QObject::tr(
                     "mythfilldatabase ran, but did not insert "
                     "any new data into the Guide for %1 of %2 sources. "
                     "This can indicate a potential grabber failure."))
                     .arg(nonewdata)
                     .arg(total_sources);
        else
            status = QObject::tr("Successful.");

        updateLastRunStatus(query, status);
    }

    return (failures == 0);
}

ChanInfo *FillData::xawtvChannel(QString &id, QString &channel, QString &fine)
{
    ChanInfo *chaninfo = new ChanInfo;
    chaninfo->xmltvid = id;
    chaninfo->name = id;
    chaninfo->callsign = id;
    if (chan_data.channel_preset)
        chaninfo->chanstr = id;
    else
        chaninfo->chanstr = channel;
    chaninfo->finetune = fine;
    chaninfo->freqid = channel;
    chaninfo->tvformat = "Default";

    return chaninfo;
}

void FillData::readXawtvChannels(int id, QString xawrcfile)
{
    QByteArray tmp = xawrcfile.toAscii();
    fstream fin(tmp.constData(), ios::in);

    if (!fin.is_open())
        return;

    QList<ChanInfo> chanlist;

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
                    if (!xawid.isEmpty() && !channel.isEmpty())
                    {
                        ChanInfo *chinfo = xawtvChannel(xawid, channel, fine);
                        chanlist.push_back(*chinfo);
                        delete chinfo;
                    }
                    xawid = strLine.substr(1, nSplitPoint - 1).c_str();
                    channel.clear();
                    fine.clear();
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

    if (!xawid.isEmpty() && !channel.isEmpty())
    {
        ChanInfo *chinfo = xawtvChannel(xawid, channel, fine);
        chanlist.push_back(*chinfo);
        delete chinfo;
    }

    chan_data.handleChannels(id, &chanlist);
    icon_data.UpdateSourceIcons(id);
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
