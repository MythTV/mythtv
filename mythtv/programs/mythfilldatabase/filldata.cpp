// POSIX headers
#include <unistd.h>

// C++ headers
#include <algorithm>
#include <cstdlib>
#include <fstream>

// Qt headers
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QList>
#include <QMap>
#include <QTextStream>

// MythTV headers
#include "libmythbase/compat.h"
#include "libmythbase/exitcodes.h"
#include "libmythbase/mythcorecontext.h"
#include "libmythbase/mythdate.h"
#include "libmythbase/mythdb.h"
#include "libmythbase/mythdbcon.h"
#include "libmythbase/mythdirs.h"
#include "libmythbase/mythlogging.h"
#include "libmythbase/mythmiscutil.h"
#include "libmythbase/mythsystemlegacy.h"
#include "libmythtv/videosource.h" // for is_grabber..

// filldata headers
#include "filldata.h"

#define LOC QString("FillData: ")
#define LOC_WARN QString("FillData, Warning: ")
#define LOC_ERR QString("FillData, Error: ")

bool updateLastRunEnd(void)
{
    QDateTime qdtNow = MythDate::current();
    return gCoreContext->SaveSettingOnHost("mythfilldatabaseLastRunEnd",
                                           qdtNow.toString(Qt::ISODate),
                                           nullptr);
}

bool updateLastRunStart(void)
{
    updateNextScheduledRun();
    QDateTime qdtNow = MythDate::current();
    return gCoreContext->SaveSettingOnHost("mythfilldatabaseLastRunStart",
                                           qdtNow.toString(Qt::ISODate),
                                           nullptr);
}

bool updateLastRunStatus(QString &status)
{
    return gCoreContext->SaveSettingOnHost("mythfilldatabaseLastRunStatus",
                                           status,
                                           nullptr);
}

bool updateNextScheduledRun()
{
    QDateTime nextSuggestedTime = MythDate::current().addDays(1);
    return gCoreContext->SaveSettingOnHost("MythFillSuggestedRunTime",
                                        nextSuggestedTime.toString(Qt::ISODate),
                                        nullptr);
}

void FillData::SetRefresh(int day, bool set)
{
    if (kRefreshClear == day)
    {
        m_refreshAll = set;
        m_refreshDay.clear();
    }
    else if (kRefreshAll == day)
    {
        m_refreshAll = set;
    }
    else
    {
        m_refreshDay[(uint)day] = set;
    }
}

// XMLTV stuff
bool FillData::GrabDataFromFile(int id, const QString &filename)
{
    ChannelInfoList chanlist;
    QMap<QString, QList<ProgInfo> > proglist;

    if (!m_xmltvParser.parseFile(filename, &chanlist, &proglist))
        return false;

    m_chanData.handleChannels(id, &chanlist);
    if (m_onlyUpdateChannels)
    {
        if (proglist.count() != 0)
        {
            LOG(VB_GENERAL, LOG_INFO, "Skipping program guide updates");
        }
    }
    else
    {
        if (proglist.count() == 0)
        {
            LOG(VB_GENERAL, LOG_INFO, "No programs found in data.");
            m_endOfData = true;
        }
        else
        {
            ProgramData::HandlePrograms(id, proglist);
        }
    }
    return true;
}

bool FillData::GrabData(const DataSource& source, int offset)
{
    QString xmltv_grabber = source.xmltvgrabber;

    const QString templatename = "/tmp/mythXXXXXX";
    const QString tempfilename = createTempFile(templatename);
    if (templatename == tempfilename)
    {
        m_fatalErrors.push_back("Failed to create temporary file.");
        return false;
    }

    QString filename = QString(tempfilename);

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
        configfile = QString("%1/%2.xmltv").arg(GetConfDir(), source.name);

    LOG(VB_GENERAL, LOG_INFO,
        QString("XMLTV config file is: %1").arg(configfile));

    QString command = QString("nice %1 --config-file '%2' --output %3")
        .arg(xmltv_grabber, configfile, filename);

    if (m_onlyUpdateChannels && source.xmltvgrabber_apiconfig)
    {
        command += " --list-channels";
    }
    else if (source.xmltvgrabber_prefmethod != "allatonce"  || m_noAllAtOnce || m_onlyUpdateChannels)
    {
        // XMLTV Docs don't recommend grabbing one day at a
        // time but the current MythTV code is heavily geared
        // that way so until it is re-written behave as
        // we always have done.
        command += QString(" --days 1 --offset %1").arg(offset);
    }

    if (!VERBOSE_LEVEL_CHECK(VB_XMLTV, LOG_ANY))
        command += " --quiet";

    // Append additional arguments passed to mythfilldatabase
    // using --graboptions
    if (!m_grabOptions.isEmpty())
    {
        command += m_grabOptions;
        LOG(VB_XMLTV, LOG_INFO,
            QString("Using graboptions: %1").arg(m_grabOptions));
    }

    QString status = QObject::tr("currently running.");

    updateLastRunStart();
    updateLastRunStatus(status);

    LOG(VB_XMLTV, LOG_INFO, QString("Grabber Command: %1").arg(command));

    LOG(VB_XMLTV, LOG_INFO,
            "----------------- Start of XMLTV output -----------------");

    MythSystemLegacy run_grabber(command, kMSRunShell | kMSStdErr);

    run_grabber.Run();
    uint systemcall_status = run_grabber.Wait();
    bool succeeded = (systemcall_status == GENERIC_EXIT_OK);

    QByteArray result = run_grabber.ReadAllErr();
    QTextStream ostream(result);
    while (!ostream.atEnd())
        {
            QString line = ostream.readLine().simplified();
            LOG(VB_XMLTV, LOG_INFO, line);
        }

    LOG(VB_XMLTV, LOG_INFO,
            "------------------ End of XMLTV output ------------------");

    updateLastRunEnd();

    status = QObject::tr("Successful.");

    if (!succeeded)
    {
        if (systemcall_status == GENERIC_EXIT_KILLED)
        {
            m_interrupted = true;
            status = QObject::tr("FAILED: XMLTV grabber ran but was interrupted.");
            LOG(VB_GENERAL, LOG_ERR,
                QString("XMLTV grabber ran but was interrupted."));
        }
        else
        {
            status = QObject::tr("FAILED: XMLTV grabber returned error code %1.")
                            .arg(systemcall_status);
            LOG(VB_GENERAL, LOG_ERR,
                QString("XMLTV grabber returned error code %1")
                    .arg(systemcall_status));
        }
    }

    updateLastRunStatus(status);

    succeeded &= GrabDataFromFile(source.id, filename);

    QFile thefile(filename);
    thefile.remove();

    return succeeded;
}

/** \fn FillData::Run(DataSourceList &sourcelist)
 *  \brief Goes through the sourcelist and updates its channels with
 *         program info grabbed with the associated grabber.
 *  \return true if there were no failures
 */
bool FillData::Run(DataSourceList &sourcelist)
{
    DataSourceList::iterator it;

    QString status;
    QString querystr;
    MSqlQuery query(MSqlQuery::InitCon());
    QDateTime GuideDataBefore;
    QDateTime GuideDataAfter;
    int failures = 0;
    int externally_handled = 0;
    int total_sources = sourcelist.size();
    int source_channels = 0;

    QString sidStr = QString("Updating source #%1 (%2) with grabber %3");

    m_needPostGrabProc = false;
    int nonewdata = 0;

    for (it = sourcelist.begin(); it != sourcelist.end(); ++it)
    {
        if (!m_fatalErrors.empty())
            break;

        QString xmltv_grabber = (*it).xmltvgrabber;

        if (xmltv_grabber == "datadirect" ||
            xmltv_grabber == "schedulesdirect1")
        {
            LOG(VB_GENERAL, LOG_ERR,
                QString("Source %1 is configured to use the DataDirect guide"
                        "service from Schedules Direct.  That service is no "
                        "longer supported by MythTV.  Update to use one of "
                        "the XMLTV grabbers that use the JSON-based guide "
                        "service from Schedules Direct.")
                .arg((*it).id));
            continue;
        }

        query.prepare("SELECT MAX(endtime) "
                      "FROM program p "
                      "LEFT JOIN channel c ON p.chanid=c.chanid "
                      "WHERE c.deleted IS NULL AND c.sourceid= :SRCID "
                      "      AND manualid = 0 AND c.xmltvid != '';");
        query.bindValue(":SRCID", (*it).id);

        if (query.exec() && query.next())
        {
            if (!query.isNull(0))
                GuideDataBefore =
                    MythDate::fromString(query.value(0).toString());
        }

        m_channelUpdateRun = false;
        m_endOfData = false;

        if (xmltv_grabber == "eitonly")
        {
            LOG(VB_GENERAL, LOG_INFO,
                QString("Source %1 configured to use only the "
                        "broadcasted guide data. Skipping.") .arg((*it).id));

            externally_handled++;
            updateLastRunStart();
            updateLastRunEnd();
            continue;
        }
        if (xmltv_grabber.trimmed().isEmpty() ||
            xmltv_grabber == "/bin/true" ||
            xmltv_grabber == "none")
        {
            LOG(VB_GENERAL, LOG_INFO, 
                QString("Source %1 configured with no grabber. Nothing to do.")
                    .arg((*it).id));

            externally_handled++;
            updateLastRunStart();
            updateLastRunEnd();
            continue;
        }

        LOG(VB_GENERAL, LOG_INFO, sidStr.arg(QString::number((*it).id),
                                             (*it).name,
                                             xmltv_grabber));

        query.prepare(
            "SELECT COUNT(chanid) FROM channel "
            "WHERE deleted IS NULL AND "
            "      sourceid = :SRCID AND xmltvid != ''");
        query.bindValue(":SRCID", (*it).id);

        if (query.exec() && query.next())
        {
            source_channels = query.value(0).toInt();
            if (source_channels > 0)
            {
                LOG(VB_GENERAL, LOG_INFO,
                    QString("Found %1 channels for source %2 which use grabber")
                        .arg(source_channels).arg((*it).id));
            }
            else
            {
                LOG(VB_GENERAL, LOG_INFO,
                    QString("No channels are configured to use grabber (none have XMLTVIDs)."));
            }
        }
        else
        {
            source_channels = 0;
            LOG(VB_GENERAL, LOG_INFO,
                QString("Can't get a channel count for source id %1")
                    .arg((*it).id));
        }

        bool hasprefmethod = false;

        if (is_grabber_external(xmltv_grabber))
        {
            uint flags = kMSRunShell | kMSStdOut;
            MythSystemLegacy grabber_capabilities_proc(xmltv_grabber,
                                                 QStringList("--capabilities"),
                                                 flags);
            grabber_capabilities_proc.Run(25s);
            if (grabber_capabilities_proc.Wait() != GENERIC_EXIT_OK)
            {
                LOG(VB_GENERAL, LOG_ERR,
                    QString("%1  --capabilities failed or we timed out waiting."                            
                    " You may need to upgrade your xmltv grabber")
                        .arg(xmltv_grabber));
            }
            else
            {
                QByteArray result = grabber_capabilities_proc.ReadAll();
                QTextStream ostream(result);
                QString capabilities;
                while (!ostream.atEnd())
                {
                    QString capability
                        = ostream.readLine().simplified();

                    if (capability.isEmpty())
                        continue;

                    capabilities += capability + ' ';

                    if (capability == "baseline")
                        (*it).xmltvgrabber_baseline = true;

                    if (capability == "manualconfig")
                        (*it).xmltvgrabber_manualconfig = true;

                    if (capability == "cache")
                        (*it).xmltvgrabber_cache = true;

                    if (capability == "apiconfig")
                        (*it).xmltvgrabber_apiconfig = true;

                    if (capability == "lineups")
                        (*it).xmltvgrabber_lineups = true;

                    if (capability == "preferredmethod")
                        hasprefmethod = true;
                }
                LOG(VB_GENERAL, LOG_INFO,
                    QString("Grabber has capabilities: %1") .arg(capabilities));
            }
        }

        if (hasprefmethod)
        {
            uint flags = kMSRunShell | kMSStdOut;
            MythSystemLegacy grabber_method_proc(xmltv_grabber,
                                           QStringList("--preferredmethod"),
                                           flags);
            grabber_method_proc.Run(15s);
            if (grabber_method_proc.Wait() != GENERIC_EXIT_OK)
            {
                LOG(VB_GENERAL, LOG_ERR,
                    QString("%1 --preferredmethod failed or we timed out "
                            "waiting. You may need to upgrade your xmltv "
                            "grabber").arg(xmltv_grabber));
            }
            else
            {
                QTextStream ostream(grabber_method_proc.ReadAll());
                (*it).xmltvgrabber_prefmethod =
                                ostream.readLine().simplified();

                LOG(VB_GENERAL, LOG_INFO, QString("Grabber prefers method: %1")
                                    .arg((*it).xmltvgrabber_prefmethod));
            }
        }

        m_needPostGrabProc |= true;

        if ((*it).xmltvgrabber_prefmethod == "allatonce" && !m_noAllAtOnce && !m_onlyUpdateChannels)
        {
            if (!GrabData(*it, 0))
                ++failures;
        }
        else if ((*it).xmltvgrabber_baseline)
        {

            QDate qCurrentDate = MythDate::current().date();

            // We'll keep grabbing until it returns nothing
            // Max days currently supported is 21
            int grabdays = REFRESH_MAX;

            grabdays = (m_maxDays > 0)          ? m_maxDays : grabdays;
            grabdays = (m_onlyUpdateChannels)   ? 1         : grabdays;

            std::vector<bool> refresh_request;
            refresh_request.resize(grabdays, m_refreshAll);
            if (!m_refreshAll)
            {
                // Set up days to grab if all is not specified
                // If all was specified the vector was initialized
                // with true in all occurrences.
                for (int i = 0; i < grabdays; i++)
                    refresh_request[i] = m_refreshDay[i];
            }

            for (int i = 0; i < grabdays; i++)
            {
                if (!m_fatalErrors.empty())
                    break;

                // We need to check and see if the current date has changed
                // since we started in this loop.  If it has, we need to adjust
                // the value of 'i' to compensate for this.
                if (MythDate::current().date() != qCurrentDate)
                {
                    QDate newDate = MythDate::current().date();
                    i += (newDate.daysTo(qCurrentDate));
                    i = std::max(i, 0);
                    qCurrentDate = newDate;
                }

                QString currDate(qCurrentDate.addDays(i).toString());

                LOG(VB_GENERAL, LOG_INFO, ""); // add a space between days
                LOG(VB_GENERAL, LOG_INFO, "Checking day @ " +
                    QString("offset %1, date: %2").arg(i).arg(currDate));

                bool download_needed = false;

                if (refresh_request[i])
                {
                    if ( i == 1 )
                    {
                        LOG(VB_GENERAL, LOG_INFO,
                            "Data Refresh always needed for tomorrow");
                    }
                    else
                    {
                        LOG(VB_GENERAL, LOG_INFO,
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
                               "WHERE c.deleted IS NULL AND c.sourceid = %3 AND c.xmltvid != '' "
                               "GROUP BY c.chanid;";

                    if (query.exec(querystr.arg(i-1).arg(i).arg((*it).id)) &&
                        query.isActive())
                    {
                        int prevChanCount = 0;
                        int currentChanCount = 0;
                        int previousDayCount = 0;
                        int currentDayCount = 0;

                        LOG(VB_CHANNEL, LOG_INFO,
                            QString("Checking program counts for day %1")
                                .arg(i-1));

                        while (query.next())
                        {
                            if (query.value(1).toInt() > 0)
                                prevChanCount++;
                            previousDayCount += query.value(1).toInt();

                            LOG(VB_CHANNEL, LOG_INFO,
                                QString("    chanid %1 -> %2 programs")
                                    .arg(query.value(0).toString())
                                    .arg(query.value(1).toInt()));
                        }

                        if (query.exec(querystr.arg(i).arg(i+1).arg((*it).id))
                                && query.isActive())
                        {
                            LOG(VB_CHANNEL, LOG_INFO,
                                QString("Checking program counts for day %1")
                                    .arg(i));
                            while (query.next())
                            {
                                if (query.value(1).toInt() > 0)
                                    currentChanCount++;
                                currentDayCount += query.value(1).toInt();

                                LOG(VB_CHANNEL, LOG_INFO,
                                    QString("    chanid %1 -> %2 programs")
                                                .arg(query.value(0).toString())
                                                .arg(query.value(1).toInt()));
                            }
                        }
                        else
                        {
                            LOG(VB_GENERAL, LOG_INFO,
                                QString("Data Refresh because we are unable to "
                                        "query the data for day %1 to "
                                        "determine if we have enough").arg(i));
                            download_needed = true;
                        }

                        if (currentChanCount < (prevChanCount * 0.90))
                        {
                            LOG(VB_GENERAL, LOG_INFO,
                                QString("Data refresh needed because only %1 "
                                        "out of %2 channels have at least one "
                                        "program listed for day @ offset %3 "
                                        "from 8PM - midnight.  Previous day "
                                        "had %4 channels with data in that "
                                        "time period.")
                                    .arg(currentChanCount).arg(source_channels)
                                    .arg(i).arg(prevChanCount));
                            download_needed = true;
                        }
                        else if (currentDayCount == 0)
                        {
                            LOG(VB_GENERAL, LOG_INFO,
                                QString("Data refresh needed because no data "
                                        "exists for day @ offset %1 from 8PM - "
                                        "midnight.").arg(i));
                            download_needed = true;
                        }
                        else if (previousDayCount == 0)
                        {
                            LOG(VB_GENERAL, LOG_INFO,
                                QString("Data refresh needed because no data "
                                        "exists for day @ offset %1 from 8PM - "
                                        "midnight.  Unable to calculate how "
                                        "much we should have for the current "
                                        "day so a refresh is being forced.")
                                    .arg(i-1));
                            download_needed = true;
                        }
                        else if (currentDayCount < (currentChanCount * 3))
                        {
                            LOG(VB_GENERAL, LOG_INFO,
                                QString("Data Refresh needed because offset "
                                        "day %1 has less than 3 programs "
                                        "per channel for the 8PM - midnight "
                                        "time window for channels that "
                                        "normally have data. "
                                        "We want at least %2 programs, but "
                                        "only found %3")
                                    .arg(i).arg(currentChanCount * 3)
                                    .arg(currentDayCount));
                            download_needed = true;
                        }
                        else if (currentDayCount < (previousDayCount / 2))
                        {
                            LOG(VB_GENERAL, LOG_INFO,
                                QString("Data Refresh needed because offset "
                                        "day %1 has less than half the number "
                                        "of programs as the previous day for "
                                        "the 8PM - midnight time window. "
                                        "We want at least %2 programs, but "
                                        "only found %3").arg(i)
                                    .arg(previousDayCount / 2)
                                    .arg(currentDayCount));
                            download_needed = true;
                        }
                    }
                    else
                    {
                        LOG(VB_GENERAL, LOG_INFO,
                            QString("Data Refresh needed because we are unable "
                                    "to query the data for day @ offset %1 to "
                                    "determine how much we should have for "
                                    "offset day %2.").arg(i-1).arg(i));
                        download_needed = true;
                    }
                }

                if (download_needed)
                {
                    LOG(VB_GENERAL, LOG_NOTICE,
                        QString("Refreshing data for ") + currDate);
                    if (!GrabData(*it, i))
                    {
                        ++failures;
                        if (!m_fatalErrors.empty() || m_interrupted)
                        {
                            break;
                        }
                    }

                    if (m_endOfData)
                    {
                        LOG(VB_GENERAL, LOG_INFO,
                            "Grabber is no longer returning program data, "
                            "finishing");
                        break;
                    }
                }
                else
                {
                    LOG(VB_GENERAL, LOG_NOTICE,
                        QString("Data is already present for ") + currDate +
                        ", skipping");
                }
            }
            if (!m_fatalErrors.empty())
                break;
        }
        else
        {
            LOG(VB_GENERAL, LOG_ERR,
                QString("Grabbing XMLTV data using ") + xmltv_grabber +
                " is not supported. You may need to upgrade to"
                " the latest version of XMLTV.");
        }

        if (m_interrupted)
        {
            break;
        }

        query.prepare("SELECT MAX(endtime) FROM program p "
                      "LEFT JOIN channel c ON p.chanid=c.chanid "
                      "WHERE c.deleted IS NULL AND c.sourceid= :SRCID "
                      "AND manualid = 0 AND c.xmltvid != '';");
        query.bindValue(":SRCID", (*it).id);

        if (query.exec() && query.next())
        {
            if (!query.isNull(0))
                GuideDataAfter = MythDate::fromString(query.value(0).toString());
        }

        if (GuideDataAfter == GuideDataBefore)
        {
            nonewdata++;
        }
    }

    if (!m_fatalErrors.empty())
    {
        for (const QString& error : std::as_const(m_fatalErrors))
        {
            LOG(VB_GENERAL, LOG_CRIT, LOC + "Encountered Fatal Error: " + error);
        }
        return false;
    }

    if (m_onlyUpdateChannels && !m_needPostGrabProc)
        return true;

    if (failures == 0)
    {
        if (nonewdata > 0 &&
            (total_sources != externally_handled))
        {
            status = QObject::tr(
                     "mythfilldatabase ran, but did not insert "
                     "any new data into the Guide for %1 of %2 sources. "
                     "This can indicate a potential grabber failure.")
                     .arg(nonewdata)
                     .arg(total_sources);
        }
        else
        {
            status = QObject::tr("Successful.");
        }

        updateLastRunStatus(status);
    }

    return (failures == 0);
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
