// POSIX headers
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>

// Std C headers
#include <cstdlib>
#include <ctime>

// C++ headers
#include <fstream>
using namespace std;

// Qt headers
#include <qmap.h>
#include <qdatetime.h>
#include <qdir.h>
#include <qfile.h>

// libmyth headers
#include "exitcodes.h"
#include "mythcontext.h"
#include "mythdbcon.h"

// filldata headers
#include "filldata.h"

// DataDirect stuff
void FillData::DataDirectStationUpdate(Source source, bool update_icons)
{
    DataDirectProcessor::UpdateStationViewTable(source.lineupid);

    bool insert_channels = chan_data.insert_chan(source.id);
    int new_channels = DataDirectProcessor::UpdateChannelsSafe(
        source.id, insert_channels);

    //  User must pass "--do-channel-updates" for these updates
    if (chan_data.channel_updates)
        DataDirectProcessor::UpdateChannelsUnsafe(source.id);
    // TODO delete any channels which no longer exist in listings source

    if (update_icons)
        icon_data.UpdateSourceIcons(source.id);

    // Unselect channels not in users lineup for DVB, HDTV
    if (!insert_channels && (new_channels > 0))
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
    ddprocessor.SetListingsProvider(DD_ZAP2IT);
    ddprocessor.SetUserID(source.userid);
    ddprocessor.SetPassword(source.password);

    bool ok    = ddprocessor.GrabFullLineup(
        source.lineupid, true, chan_data.insert_chan(source.id)/*only sel*/);
    logged_in  = source.userid;
    raw_lineup = source.id;

    if (ok)
        DataDirectStationUpdate(source, false);

    return ok;
}

bool FillData::grabDDData(Source source, int poffset,
                          QDate pdate, int ddSource) 
{
    ddprocessor.SetListingsProvider(ddSource);
    ddprocessor.SetUserID(source.userid);
    ddprocessor.SetPassword(source.password);
    
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
            QDateTime fromdatetime = QDateTime(pdate);
            QDateTime todatetime;
            fromdatetime.setTime_t(QDateTime(pdate).toTime_t(),Qt::UTC);
            fromdatetime = fromdatetime.addDays(poffset);
            todatetime = fromdatetime.addDays(1);

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

    qdtNow = QDateTime::currentDateTime();
    query.exec(QString("UPDATE settings SET data ='%1' "
                       "WHERE value='mythfilldatabaseLastRunEnd'")
                       .arg(qdtNow.toString("yyyy-MM-dd hh:mm")));

    VERBOSE(VB_GENERAL, "Main temp tables populated.");
    if (!channel_update_run)
    {
        VERBOSE(VB_GENERAL, "Updating myth channels.");
        DataDirectStationUpdate(source);
        VERBOSE(VB_GENERAL, "Channels updated.");
        channel_update_run = true;
    }

    //cerr << "Creating program view table...\n";
    DataDirectProcessor::UpdateProgramViewTable(source.id);
    //cerr <<  "Finished creating program view table...\n";

    query.exec("SELECT count(*) from dd_v_program;");
    if (query.isActive() && query.size() > 0)
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
    ProgramData::clearDataBySource(source.id, fromlocaldt,tolocaldt);
    VERBOSE(VB_GENERAL, "Data for source cleared.");
    
    VERBOSE(VB_GENERAL, "Updating programs.");
    DataDirectProcessor::DataDirectProgramUpdate();
    VERBOSE(VB_GENERAL, "Program table update complete.");

    return true;
}

// XMLTV stuff
bool FillData::grabDataFromFile(int id, QString &filename)
{
    QValueList<ChanInfo> chanlist;
    QMap<QString, QValueList<ProgInfo> > proglist;

    if (!xmltv_parser.parseFile(filename, &chanlist, &proglist))
        return false;

    chan_data.handleChannels(id, &chanlist);
    icon_data.UpdateSourceIcons(id);
    prog_data.handlePrograms(id, &proglist);
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

bool FillData::grabData(Source source, int offset, QDate *qCurrentDate)
{
    QString xmltv_grabber = source.xmltvgrabber;

    if (xmltv_grabber == "datadirect")
        return grabDDData(source, offset, *qCurrentDate, DD_ZAP2IT);
    else if (xmltv_grabber == "technovera")
    {
        VERBOSE(VB_ALL, "This grabber is no longer supported");
        exit(FILLDB_EXIT_INVALID_CMDLINE);
    }
        
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
        command.sprintf("nice %s --days 7 --config-file '%s' --output %s",
                        xmltv_grabber.ascii(), configfile.ascii(),
                        filename.ascii());
    else if (xmltv_grabber == "tv_grab_nl")
        command.sprintf("nice %s --output %s",
                        xmltv_grabber.ascii(),
                        filename.ascii());
    else if (xmltv_grabber == "tv_grab_fi")
        // Use the default of 10 days for Finland's grabber
        command.sprintf("nice %s --config-file '%s' --output %s",
                        xmltv_grabber.ascii(), configfile.ascii(),
                        filename.ascii());
    else if (xmltv_grabber == "tv_grab_es" ||
             xmltv_grabber == "tv_grab_es_laguiatv")
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
         xmltv_parser.isJapan = true;
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
    else if (xmltv_grabber == "tv_grab_ee")
        // Estonian grabber returns all known data by default
        command.sprintf("nice %s --output %s",
                        xmltv_grabber.ascii(),
                        filename.ascii());
    else if (xmltv_grabber == "tv_grab_il")
        // Israeli grabber returns all known data by default
        command.sprintf("nice %s --config-file '%s' --output %s",
                        xmltv_grabber.ascii(), configfile.ascii(),
                        filename.ascii());
    else if (xmltv_grabber == "tv_grab_ru")
        // Russian grabber returns all known data by default
        command.sprintf("nice %s --config-file '%s' --output %s",
                        xmltv_grabber.ascii(), configfile.ascii(),
                        filename.ascii());
    else
    {
        xmltv_parser.isNorthAmerica = true;
        command.sprintf("nice %s --days 1 --offset %d --config-file '%s' "
                        "--output %s", xmltv_grabber.ascii(),
                        offset, configfile.ascii(), filename.ascii());
    }

    if (((print_verbose_messages & VB_GENERAL) == 0) &&
        (xmltv_grabber == "tv_grab_na" ||
         xmltv_grabber == "tv_grab_de_tvtoday" ||
         xmltv_grabber == "tv_grab_fi" ||
         xmltv_grabber == "tv_grab_es" ||
         xmltv_grabber == "tv_grab_es_laguiatv" ||
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
         xmltv_grabber == "tv_grab_be_tlm" ||
         xmltv_grabber == "tv_grab_ee" ||
         xmltv_grabber == "tv_grab_ru"))
         command += " --quiet";


    command += graboptions;

    VERBOSE(VB_GENERAL,
            "----------------- Start of XMLTV output -----------------");

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
 
    VERBOSE(VB_GENERAL,
            "------------------ End of XMLTV output ------------------");

    grabDataFromFile(source.id, filename);

    QFile thefile(filename);
    thefile.remove();

    return succeeded;
}

void FillData::grabDataFromDDFile(
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
    s.name = "";
    s.xmltvgrabber = "datadirect";
    s.userid = "fromfile";
    s.password = "fromfile";
    s.lineupid = lineupid;

    grabData(s, offset, currentd);
}


/** \fn FillData::fillData(QValueList<Source> &sourcelist)
 *  \brief Goes through the sourcelist and updates its channels with
 *         program info grabbed with the associated grabber.
 *  \return true if there was no failures
 */
bool FillData::fillData(QValueList<Source> &sourcelist)
{
    QValueList<Source>::Iterator it;

    QString status, querystr;
    MSqlQuery query(MSqlQuery::InitCon());
    QDateTime GuideDataBefore, GuideDataAfter;
    int failures = 0;
    int externally_handled = 0;
    int total_sources = sourcelist.size();
    int source_channels = 0;

    query.exec(QString("SELECT MAX(endtime) FROM program WHERE manualid=0;"));
    if (query.isActive() && query.size() > 0)
    {
        query.next();

        if (!query.isNull(0))
            GuideDataBefore = QDateTime::fromString(query.value(0).toString(),
                                                    Qt::ISODate);
    }

    QString sidStr = QString("Updating source #%1 (%2) with grabber %3");

    need_post_grab_proc = false;

    for (it = sourcelist.begin(); it != sourcelist.end(); ++it)
    {
        channel_update_run = false;
        VERBOSE(VB_GENERAL, sidStr.arg((*it).id)
                                  .arg((*it).name)
                                  .arg((*it).xmltvgrabber));

        query.prepare(
            "SELECT COUNT(chanid) FROM channel WHERE sourceid = :SRCID");
        query.bindValue(":SRCID", (*it).id);
        query.exec();

        if (query.isActive() && query.numRowsAffected() > 0) {
            query.next();
            source_channels = query.value(0).toInt();
            VERBOSE(VB_GENERAL, QString("Found %1 channels for source %2")
                                        .arg(source_channels).arg((*it).id));
        } else {
            source_channels = 0;
            VERBOSE(VB_GENERAL,
                    QString("Can't get a channel count for source id %1")
                            .arg((*it).id));
        }

        QString xmltv_grabber = (*it).xmltvgrabber;
        need_post_grab_proc |= (xmltv_grabber != "datadirect");

        if (xmltv_grabber == "tv_grab_uk" || xmltv_grabber == "tv_grab_uk_rt" ||
            xmltv_grabber == "tv_grab_fi" || xmltv_grabber == "tv_grab_es" ||
            xmltv_grabber == "tv_grab_es_laguiatv" ||
            xmltv_grabber == "tv_grab_nl" || xmltv_grabber == "tv_grab_au" ||
            xmltv_grabber == "tv_grab_fr" || xmltv_grabber == "tv_grab_jp" ||
            xmltv_grabber == "tv_grab_pt" || xmltv_grabber == "tv_grab_ee" ||
            xmltv_grabber == "tv_grab_dk")
        {
            // These don't support the --offset option, so just grab the max.
            // TODO: tv_grab_fi/dk/is seems to support --offset, maybe more. Needs verification.
            if (!grabData(*it, 0))
                ++failures;
        }
        else if ((xmltv_grabber == "datadirect") && dd_grab_all)
        {
            if (only_update_channels)
                DataDirectUpdateChannels(*it);
            else
            {
                QDate qCurrentDate = QDate::currentDate();
                grabData(*it, 0, &qCurrentDate);
            }
        }
        else if (xmltv_grabber == "datadirect" ||
                 xmltv_grabber == "tv_grab_se_swedb" ||
                 xmltv_grabber == "tv_grab_no" ||
                 xmltv_grabber == "tv_grab_de_tvtoday" ||
                 xmltv_grabber == "tv_grab_be_tvb" ||
                 xmltv_grabber == "tv_grab_be_tlm" ||
                 xmltv_grabber == "tv_grab_is" ||
                 xmltv_grabber == "tv_grab_br" ||
                 xmltv_grabber == "tv_grab_cz" ||
                 xmltv_grabber == "tv_grab_ru")
        {
            // Grabbers supporting the --offset option

            if (xmltv_grabber == "tv_grab_no")
                prog_data.listing_wrap_offset = 6 * 3600;

            QDate qCurrentDate = QDate::currentDate();

            int grabdays = 9;

            // Grab different amount of days for the different grabbers,
            // often decided by the person maintaining the grabbers.
            if (maxDays > 0) // passed with --max-days
                grabdays = maxDays;
            else if (xmltv_grabber == "datadirect")
                grabdays = 14;
            else if (xmltv_grabber == "tv_grab_se_swedb")
                grabdays = 10;
            else if (xmltv_grabber == "tv_grab_no" ||
                     xmltv_grabber == "tv_grab_de_tvtoday")
                grabdays = 7;
            else if (xmltv_grabber == "tv_grab_be_tvb" ||
                     xmltv_grabber == "tv_grab_be_tlm")
                grabdays = 5;

            grabdays = (only_update_channels) ? 1 : grabdays;

            if (grabdays == 1)
                refresh_today = true;

            if ((xmltv_grabber == "datadirect") && only_update_channels)
            {
                DataDirectUpdateChannels(*it);
                grabdays = 0;
            }

            for (int i = 0; i < grabdays; i++)
            {
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
                VERBOSE(VB_GENERAL, "Checking day @ " <<
                        QString("offset %1, date: %2").arg(i).arg(currDate));

                bool download_needed = false;

                if (refresh_all)
                {
                    VERBOSE(VB_GENERAL,
                            "Data Refresh needed because of --refresh-all");
                    download_needed = true;
                }
                else if ((i == 0 && refresh_today) || (i == 1 && refresh_tomorrow) ||
                         (i == 2 && refresh_second))
                {
                    // Always refresh if the user specified today/tomorrow/second.
                    download_needed = true;
                }
                else if (xmltv_grabber == "tv_grab_se_swedb")
                {
                    // Since tv_grab_se_swedb handles caching internally,
                    // let it do its job and always grab new data.
                    VERBOSE(VB_GENERAL,
                            "Data Refresh needed because the grabber relies on "
                            "internal caching.");
                    download_needed = true;
                }
                else
                {
                    // Check to see if we already downloaded data for this date.
                    int prevChanCount = 0;
                    int currentChanCount = 0;
                    int previousDayCount = 0;
                    int currentDayCount = 0;

                    querystr = "SELECT c.chanid, COUNT(p.starttime) "
                               "FROM channel c "
                               "LEFT JOIN program p ON c.chanid = p.chanid "
                               "  AND starttime >= "
                                   "DATE_ADD(DATE_ADD(CURRENT_DATE(), "
                                   "INTERVAL '%1' DAY), INTERVAL '20' HOUR) "
                               "  AND starttime < DATE_ADD(CURRENT_DATE(), "
                                   "INTERVAL '%2' DAY) "
                               "WHERE c.sourceid = %3 "
                               "GROUP BY c.chanid;";
  
                    if (query.exec(querystr.arg(i-1).arg(i).arg((*it).id)) &&
                        query.isActive()) 
                    {
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

                        if (currentChanCount < (prevChanCount * 0.90)) {
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
                                "Data Refresh because we are unable to "
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
                    VERBOSE(VB_IMPORTANT,
                            QString("Data is already present for ") + currDate +
                                    ", skipping");
                }
            }
        }
        else if (xmltv_grabber == "eitonly")
        {
            VERBOSE(VB_IMPORTANT, "Source configured to use only the "
                    "broadcasted guide data. Skipping.");
            externally_handled++;
            query.exec(QString("UPDATE settings SET data ='%1' "
                               "WHERE value='mythfilldatabaseLastRunStart' OR "
                               "value = 'mythfilldatabaseLastRunEnd'")
                       .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm")));
        }
        else if (xmltv_grabber == "/bin/true" ||
                 xmltv_grabber == "none" ||
                 xmltv_grabber == "")
        {
            VERBOSE(VB_IMPORTANT,
                    "Source configured with no grabber. Nothing to do.");
            externally_handled++;
            query.exec(QString("UPDATE settings SET data ='%1' "
                               "WHERE value='mythfilldatabaseLastRunStart' OR "
                               "value = 'mythfilldatabaseLastRunEnd'")
                       .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm")));
        }
        else
        {
            VERBOSE(VB_IMPORTANT,
                    QString("Grabbing XMLTV data using ") + xmltv_grabber +
                            " is not verified as working.");
        }

        if (interrupted)
        {
            break;
        }
    }

    if (only_update_channels && !need_post_grab_proc)
        return true;

    query.exec(QString("SELECT MAX(endtime) FROM program WHERE manualid=0;"));
    if (query.isActive() && query.size() > 0)
    {
        query.next();

        if (!query.isNull(0))
            GuideDataAfter = QDateTime::fromString(query.value(0).toString(),
                                                   Qt::ISODate);
    }

    if (failures == 0)
    {
        if ((GuideDataAfter == GuideDataBefore) &&
            (total_sources != externally_handled))
            status = "mythfilldatabase ran, but did not insert "
                     "any new data into the Guide.  This can indicate a "
                     "potential grabber failure."; 
        else
            status = "Successful.";

        query.exec(QString("UPDATE settings SET data ='%1' "
                           "WHERE value='mythfilldatabaseLastRunStatus'")
                           .arg(status));
    }

    prog_data.clearOldDBEntries();

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
    chaninfo->iconpath = "";
    chaninfo->tvformat = "Default";

    return chaninfo;
}

void FillData::readXawtvChannels(int id, QString xawrcfile)
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

    chan_data.handleChannels(id, &chanlist);
    icon_data.UpdateSourceIcons(id);
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
