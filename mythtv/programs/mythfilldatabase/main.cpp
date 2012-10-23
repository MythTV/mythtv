// C headers
#include <unistd.h>

// C++ headers
#include <iostream>
using namespace std;

// Qt headers
#include <QCoreApplication>
#include <QFileInfo>

// libmyth headers
#include "exitcodes.h"
#include "mythcontext.h"
#include "mythdb.h"
#include "mythversion.h"
#include "mythdate.h"
#include "mythtranslation.h"

#include "mythconfig.h"

// libmythtv headers
#include "commandlineparser.h"
#include "scheduledrecording.h"
#include "mythmiscutil.h"
#include "remoteutil.h"
#include "videosource.h" // for is_grabber..
#include "dbcheck.h"
#include "mythsystemevent.h"
#include "mythlogging.h"
#include "signalhandling.h"

// filldata headers
#include "filldata.h"
namespace
{
    void cleanup()
    {
        delete gContext;
        gContext = NULL;
        SignalHandler::Done();
    }

    class CleanupGuard
    {
      public:
        typedef void (*CleanupFunc)();

      public:
        CleanupGuard(CleanupFunc cleanFunction) :
            m_cleanFunction(cleanFunction) {}

        ~CleanupGuard()
        {
            m_cleanFunction();
        }

      private:
        CleanupFunc m_cleanFunction;
    };
}

int main(int argc, char *argv[])
{
    FillData fill_data;
    int fromfile_id = 1;
    int fromfile_offset = 0;
    QString fromfile_name;
    bool from_xawfile = false;
    int fromxawfile_id = 1;
    QString fromxawfile_name;
    bool from_file = false;
    bool mark_repeats = true;

    bool usingDataDirect = false;
    bool grab_data = true;

    bool export_iconmap = false;
    bool import_iconmap = false;
    bool reset_iconmap = false;
    bool reset_iconmap_icons = false;
    QString import_icon_data_filename("iconmap.xml");
    QString export_icon_data_filename("iconmap.xml");

    bool update_icon_data = false;

    bool from_dd_file = false;
    int sourceid = -1;
    QString fromddfile_lineupid;

    MythFillDatabaseCommandLineParser cmdline;
    if (!cmdline.Parse(argc, argv))
    {
        cmdline.PrintHelp();
        return GENERIC_EXIT_INVALID_CMDLINE;
    }

    if (cmdline.toBool("showhelp"))
    {
        cmdline.PrintHelp();
        return GENERIC_EXIT_OK;
    }

    if (cmdline.toBool("showversion"))
    {
        cmdline.PrintVersion();
        return GENERIC_EXIT_OK;
    }

    QCoreApplication a(argc, argv);
    QCoreApplication::setApplicationName(MYTH_APPNAME_MYTHFILLDATABASE);

    myth_nice(19);

    int retval;
    if ((retval = cmdline.ConfigureLogging()) != GENERIC_EXIT_OK)
        return retval;

    if (cmdline.toBool("manual"))
    {
        cout << "###\n";
        cout << "### Running in manual channel configuration mode.\n";
        cout << "### This will ask you questions about every channel.\n";
        cout << "###\n";
        fill_data.chan_data.interactive = true;
    }

    if (cmdline.toBool("update"))
    {
        if (cmdline.toBool("manual"))
        {
            cerr << "--update and --manual cannot be used simultaneously"
                 << endl;
            return GENERIC_EXIT_INVALID_CMDLINE;
        }
        fill_data.chan_data.non_us_updating = true;
    }

    if (cmdline.toBool("preset"))
    {
        cout << "###\n";
        cout << "### Running in preset channel configuration mode.\n";
        cout << "### This will assign channel ";
        cout << "preset numbers to every channel.\n";
        cout << "###\n";
        fill_data.chan_data.channel_preset = true;
    }

    if (cmdline.toBool("file"))
    {
        // manual file mode
        if (!cmdline.toBool("sourceid") ||
            !cmdline.toBool("xmlfile"))
        {
            cerr << "The --file option must be used in combination" << endl
                 << "with both --sourceid and --xmlfile." << endl;
            return GENERIC_EXIT_INVALID_CMDLINE;
        }

        fromfile_id = cmdline.toInt("sourceid");
        fromfile_name = cmdline.toString("xmlfile");

        LOG(VB_GENERAL, LOG_INFO,
            "Bypassing grabbers, reading directly from file");
        from_file = true;
    }

    if (cmdline.toBool("ddfile"))
    {
        // datadirect file mode
        if (!cmdline.toBool("sourceid") || 
            !cmdline.toBool("offset") ||
            !cmdline.toBool("lineupid") ||
            !cmdline.toBool("xmlfile"))
        {
            cerr << "The --dd-file option must be used in combination" << endl
                 << "with each of --sourceid, --offset, --lineupid," << endl
                 << "and --xmlfile." << endl;
            return GENERIC_EXIT_INVALID_CMDLINE;
        }

        fromfile_id         = cmdline.toInt("sourceid");
        fromfile_offset     = cmdline.toInt("offset");
        fromddfile_lineupid = cmdline.toInt("lineupid");
        fromfile_name       = cmdline.toString("xmlfile");

        LOG(VB_GENERAL, LOG_INFO,
            "Bypassing grabbers, reading directly from file");
        from_dd_file = true;
    }

    if (cmdline.toBool("xawchannels"))
    {
        // xaw channel import mode
        if (!cmdline.toBool("sourceid") ||
            !cmdline.toBool("xawtvrcfile"))
        {
            cerr << "The --xawchannels option must be used in combination"
                 <<  endl
                 << "with both --sourceid and --xawtvrcfile" << endl;
        }

        fromxawfile_id = cmdline.toInt("sourceid");
        fromxawfile_name = cmdline.toString("xawtvrcfile");

        LOG(VB_GENERAL, LOG_INFO, "Reading channels from xawtv configfile");
        from_xawfile = true;
    }

    if (cmdline.toBool("dochannelupdates"))
        fill_data.chan_data.channel_updates = true;
    if (cmdline.toBool("removechannels"))
        fill_data.chan_data.remove_new_channels = true;
    if (cmdline.toBool("nofilterchannels"))
        fill_data.chan_data.filter_new_channels = false;
    if (!cmdline.GetPassthrough().isEmpty())
        fill_data.graboptions = " " + cmdline.GetPassthrough();
    if (cmdline.toBool("sourceid"))
        sourceid = cmdline.toInt("sourceid");
    if (cmdline.toBool("cardtype"))
    {
        if (!cmdline.toBool("sourceid"))
        {
            cerr << "The --cardtype option must be used in combination" << endl
                 << "with a --sourceid option." << endl;
            return GENERIC_EXIT_INVALID_CMDLINE;
        }

        fill_data.chan_data.cardtype = cmdline.toString("cardtype")
                                                .trimmed().toUpper();
    }
    if (cmdline.toBool("maxdays") && cmdline.toInt("maxdays") > 0)
    {
        fill_data.maxDays = cmdline.toInt("maxdays");
        if (fill_data.maxDays == 1)
            fill_data.SetRefresh(0, true);
    }

    if (cmdline.toBool("refreshtoday"))
        cmdline.SetValue("refresh",
                cmdline.toStringList("refresh") << "today");
    if (cmdline.toBool("dontrefreshtomorrow"))
        cmdline.SetValue("refresh", 
                cmdline.toStringList("refresh") << "nottomorrow");
    if (cmdline.toBool("refreshsecond"))
        cmdline.SetValue("refresh", 
                cmdline.toStringList("refresh") << "second");
    if (cmdline.toBool("refreshall"))
        cmdline.SetValue("refresh", 
                cmdline.toStringList("refresh") << "all");
    if (cmdline.toBool("refreshday"))
        cmdline.SetValue("refresh",
                cmdline.toStringList("refresh") << 
                                        cmdline.toStringList("refreshday"));

    QStringList sl = cmdline.toStringList("refresh");
    if (!sl.isEmpty())
    {
        QStringList::const_iterator i = sl.constBegin();
        for (; i != sl.constEnd(); ++i)
        {
            QString warn = QString("Invalid entry in --refresh list: %1")
                                .arg(*i);

            bool enable = (*i).contains("not") ? false : true;

            if ((*i).contains("today"))
                fill_data.SetRefresh(0, enable);
            else if ((*i).contains("tomorrow"))
                fill_data.SetRefresh(1, enable);
            else if ((*i).contains("second"))
                fill_data.SetRefresh(2, enable);
            else if ((*i).contains("all"))
                fill_data.SetRefresh(FillData::kRefreshAll, enable);
            else if ((*i).contains("-"))
            {
                bool ok;
                QStringList r = (*i).split("-");

                uint lower = r[0].toUInt(&ok);
                if (!ok)
                {
                    cerr << warn.toLocal8Bit().constData() << endl;
                    return false;
                }

                uint upper = r[1].toUInt(&ok);
                if (!ok)
                {
                    cerr << warn.toLocal8Bit().constData() << endl;
                    return false;
                }

                if (lower > upper)
                {
                    cerr << warn.toLocal8Bit().constData() << endl;
                    return false;
                }

                for (uint j = lower; j <= upper; ++j)
                    fill_data.SetRefresh(j, true);
            }
            else
            {
                bool ok;
                uint day = (*i).toUInt(&ok);
                if (!ok)
                {
                    cerr << warn.toLocal8Bit().constData() << endl;
                    return false;
                }

                fill_data.SetRefresh(day, true);
            }
        }
    }

    if (cmdline.toBool("dontrefreshtba"))
        fill_data.refresh_tba = false;
    if (cmdline.toBool("ddgraball"))
    {
        fill_data.SetRefresh(FillData::kRefreshClear, false);
        fill_data.dd_grab_all = true;
    }
    if (cmdline.toBool("onlychannels"))
        fill_data.only_update_channels = true;

    mark_repeats = cmdline.toBool("markrepeats");
    if (cmdline.toBool("exporticonmap"))
        export_icon_data_filename = cmdline.toString("exporticonmap");
    if (cmdline.toBool("importiconmap"))
        import_icon_data_filename = cmdline.toString("importiconmap");
    if (cmdline.toBool("updateiconmap"))
    {
        update_icon_data = true;
        grab_data = false;
    }
    if (cmdline.toBool("reseticonmap"))
    {
        reset_iconmap = true;
        grab_data = false;
        if (cmdline.toString("reseticonmap") == "all")
            reset_iconmap_icons = true;
    }

    CleanupGuard callCleanup(cleanup);

#ifndef _WIN32
    QList<int> signallist;
    signallist << SIGINT << SIGTERM << SIGSEGV << SIGABRT << SIGBUS << SIGFPE
               << SIGILL;
#if ! CONFIG_DARWIN
    signallist << SIGRTMIN;
#endif
    SignalHandler::Init(signallist);
    signal(SIGHUP, SIG_IGN);
#endif

    gContext = new MythContext(MYTH_BINARY_VERSION);
    if (!gContext->Init(false))
    {
        LOG(VB_GENERAL, LOG_ERR, "Failed to init MythContext, exiting.");
        return GENERIC_EXIT_NO_MYTHCONTEXT;
    }

    setHttpProxy();

    MythTranslation::load("mythfrontend");

    if (!UpgradeTVDatabaseSchema(false))
    {
        LOG(VB_GENERAL, LOG_ERR, "Incorrect database schema");
        return GENERIC_EXIT_DB_OUTOFDATE;
    }

    if (!grab_data)
    {
    }
    else if (from_xawfile)
    {
        fill_data.readXawtvChannels(fromxawfile_id, fromxawfile_name);
    }
    else if (from_file)
    {
        QString status = QObject::tr("currently running.");
        QDateTime GuideDataBefore, GuideDataAfter;

        MSqlQuery query(MSqlQuery::InitCon());
        updateLastRunStart(query);
        updateLastRunStatus(query, status);

        query.prepare("SELECT MAX(endtime) FROM program p LEFT JOIN channel c "
                      "ON p.chanid=c.chanid WHERE c.sourceid= :SRCID "
                      "AND manualid = 0 AND c.xmltvid != '';");
        query.bindValue(":SRCID", fromfile_id);

        if (query.exec() && query.next())
        {
            if (!query.isNull(0))
                GuideDataBefore =
                    MythDate::fromString(query.value(0).toString());
        }

        if (!fill_data.GrabDataFromFile(fromfile_id, fromfile_name))
        {
            return GENERIC_EXIT_NOT_OK;
        }

        updateLastRunEnd(query);

        query.prepare("SELECT MAX(endtime) FROM program p LEFT JOIN channel c "
                      "ON p.chanid=c.chanid WHERE c.sourceid= :SRCID "
                      "AND manualid = 0 AND c.xmltvid != '';");
        query.bindValue(":SRCID", fromfile_id);

        if (query.exec() && query.next())
        {
            if (!query.isNull(0))
                GuideDataAfter =
                    MythDate::fromString(query.value(0).toString());
        }

        if (GuideDataAfter == GuideDataBefore)
            status = QObject::tr("mythfilldatabase ran, but did not insert "
                    "any new data into the Guide.  This can indicate a "
                    "potential problem with the XML file used for the update.");
        else
            status = QObject::tr("Successful.");

        updateLastRunStatus(query, status);
    }
    else if (from_dd_file)
    {
        fill_data.GrabDataFromDDFile(
            fromfile_id, fromfile_offset, fromfile_name, fromddfile_lineupid);
    }
    else
    {
        SourceList sourcelist;

        MSqlQuery sourcequery(MSqlQuery::InitCon());
        QString where = "";

        if (sourceid != -1)
        {
            LOG(VB_GENERAL, LOG_INFO,
                QString("Running for sourceid %1 ONLY because --sourceid "
                        "was given on command-line").arg(sourceid));
            where = QString("WHERE sourceid = %1").arg(sourceid);
        }

        QString querystr = QString("SELECT sourceid,name,xmltvgrabber,userid,"
                                   "password,lineupid "
                                   "FROM videosource ") + where +
                                   QString(" ORDER BY sourceid;");

        if (sourcequery.exec(querystr))
        {
             if (sourcequery.size() > 0)
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

                       newsource.xmltvgrabber_baseline = false;
                       newsource.xmltvgrabber_manualconfig = false;
                       newsource.xmltvgrabber_cache = false;
                       newsource.xmltvgrabber_prefmethod = "";

                       sourcelist.push_back(newsource);
                       usingDataDirect |=
                           is_grabber_datadirect(newsource.xmltvgrabber);
                  }
             }
             else
             {
                  LOG(VB_GENERAL, LOG_ERR,
                      "There are no channel sources defined, did you run "
                      "the setup program?");
                  return GENERIC_EXIT_SETUP_ERROR;
             }
        }
        else
        {
             MythDB::DBError("loading channel sources", sourcequery);
             return GENERIC_EXIT_DB_ERROR;
        }

        if (!fill_data.Run(sourcelist))
            LOG(VB_GENERAL, LOG_ERR, "Failed to fetch some program info");
        else
            LOG(VB_GENERAL, LOG_NOTICE, "Data fetching complete.");
    }

    if (fill_data.only_update_channels && !fill_data.need_post_grab_proc)
    {
        return GENERIC_EXIT_OK;
    }

    if (reset_iconmap)
    {
        fill_data.icon_data.ResetIconMap(reset_iconmap_icons);
    }

    if (import_iconmap)
    {
        fill_data.icon_data.ImportIconMap(import_icon_data_filename);
    }

    if (export_iconmap)
    {
        fill_data.icon_data.ExportIconMap(export_icon_data_filename);
    }

    if (update_icon_data)
    {
        MSqlQuery query(MSqlQuery::InitCon());
        query.prepare("SELECT sourceid FROM videosource ORDER BY sourceid");
        if (!query.exec())
        {
            MythDB::DBError("Querying sources", query);
            return GENERIC_EXIT_DB_ERROR;
        }

        while (query.next())
            fill_data.icon_data.UpdateSourceIcons(query.value(0).toInt());
    }

    if (grab_data)
    {
        LOG(VB_GENERAL, LOG_INFO, "Adjusting program database end times.");
        int update_count = ProgramData::fix_end_times();
        if (update_count == -1)
            LOG(VB_GENERAL, LOG_ERR, "fix_end_times failed!");
        else
            LOG(VB_GENERAL, LOG_INFO,
                QString("    %1 replacements made").arg(update_count));
    }

    if (grab_data)
    {
        LOG(VB_GENERAL, LOG_INFO, "Marking generic episodes.");

        MSqlQuery query(MSqlQuery::InitCon());
        query.prepare("UPDATE program SET generic = 1 WHERE "
            "((programid = '' AND subtitle = '' AND description = '') OR "
            " (programid <> '' AND category_type = 'series' AND "
            "  program.programid LIKE '%0000'));");

        if (!query.exec())
            MythDB::DBError("mark generic", query);
        else
            LOG(VB_GENERAL, LOG_INFO,
                QString("    Found %1").arg(query.numRowsAffected()));
    }

    if (grab_data)
    {
        LOG(VB_GENERAL, LOG_INFO, "Extending non-unique programids "
                                  "with multiple parts.");

        int found = 0;
        MSqlQuery sel(MSqlQuery::InitCon());
        sel.prepare("SELECT DISTINCT programid, partnumber, parttotal "
                    "FROM program WHERE partnumber > 0 AND parttotal > 0 AND "
                    "programid LIKE '%0000'");
        if (sel.exec())
        {
            MSqlQuery repl(MSqlQuery::InitCon());
            repl.prepare("UPDATE program SET programid = :NEWID "
                         "WHERE programid = :OLDID AND "
                         "partnumber = :PARTNUM AND "
                         "parttotal = :PARTTOTAL");

            while (sel.next())
            {
                QString orig_programid = sel.value(0).toString();
                QString new_programid = orig_programid.left(10);
                int     partnum, parttotal;
                QString part;

                partnum   = sel.value(1).toInt();
                parttotal = sel.value(2).toInt();

                part.setNum(parttotal);
                new_programid.append(part.rightJustified(2, '0'));
                part.setNum(partnum);
                new_programid.append(part.rightJustified(2, '0'));

                LOG(VB_GENERAL, LOG_INFO,
                    QString("    %1 -> %2 (part %3 of %4)")
                        .arg(orig_programid).arg(new_programid)
                        .arg(partnum).arg(parttotal));

                repl.bindValue(":NEWID", new_programid);
                repl.bindValue(":OLDID", orig_programid);
                repl.bindValue(":PARTNUM",   partnum);
                repl.bindValue(":PARTTOTAL", parttotal);
                if (!repl.exec())
                {
                    LOG(VB_GENERAL, LOG_INFO,
                        QString("Fudging programid from '%1' to '%2'")
                            .arg(orig_programid)
                            .arg(new_programid));
                }
                else
                    found += repl.numRowsAffected();
            }
        }

        LOG(VB_GENERAL, LOG_INFO, QString("    Found %1").arg(found));
    }

    if (grab_data)
    {
        LOG(VB_GENERAL, LOG_INFO, "Fixing missing original airdates.");
        MSqlQuery query(MSqlQuery::InitCon());

        query.prepare("UPDATE program p "
                      "JOIN ( "
                      "  SELECT programid, MAX(originalairdate) maxoad "
                      "  FROM program "
                      "  WHERE programid <> '' AND "
                      "        originalairdate IS NOT NULL "
                      "  GROUP BY programid ) oad "
                      "  ON p.programid = oad.programid "
                      "SET p.originalairdate = oad.maxoad "
                      "WHERE p.originalairdate IS NULL");

        if (query.exec())
            LOG(VB_GENERAL, LOG_INFO,
                QString("    Found %1 with programids")
                .arg(query.numRowsAffected()));

        query.prepare("UPDATE program p "
                      "JOIN ( "
                      "  SELECT title, subtitle, description, "
                      "         MAX(originalairdate) maxoad "
                      "  FROM program "
                      "  WHERE programid = '' AND "
                      "        originalairdate IS NOT NULL "
                      "  GROUP BY title, subtitle, description ) oad "
                      "  ON p.programid = '' AND "
                      "     p.title = oad.title AND "
                      "     p.subtitle = oad.subtitle AND "
                      "     p.description = oad.description "
                      "SET p.originalairdate = oad.maxoad "
                      "WHERE p.originalairdate IS NULL");

        if (query.exec())
            LOG(VB_GENERAL, LOG_INFO,
                QString("    Found %1 without programids")
                .arg(query.numRowsAffected()));
    }

    if (mark_repeats)
    {
        LOG(VB_GENERAL, LOG_INFO, "Marking repeats.");

        int newEpiWindow = gCoreContext->GetNumSetting( "NewEpisodeWindow", 14);

        MSqlQuery query(MSqlQuery::InitCon());
        query.prepare("UPDATE program SET previouslyshown = 1 "
                      "WHERE previouslyshown = 0 "
                      "AND originalairdate is not null "
                      "AND (to_days(starttime) - to_days(originalairdate)) "
                      "    > :NEWWINDOW;");
        query.bindValue(":NEWWINDOW", newEpiWindow);

        if (query.exec())
            LOG(VB_GENERAL, LOG_INFO,
                QString("    Found %1").arg(query.numRowsAffected()));

        LOG(VB_GENERAL, LOG_INFO, "Unmarking new episode rebroadcast repeats.");
        query.prepare("UPDATE program SET previouslyshown = 0 "
                      "WHERE previouslyshown = 1 "
                      "AND originalairdate is not null "
                      "AND (to_days(starttime) - to_days(originalairdate)) "
                      "    <= :NEWWINDOW;");
        query.bindValue(":NEWWINDOW", newEpiWindow);

        if (query.exec())
            LOG(VB_GENERAL, LOG_INFO,
                QString("    Found %1").arg(query.numRowsAffected()));
    }

    // Mark first and last showings

    if (grab_data)
    {
        MSqlQuery updt(MSqlQuery::InitCon());
        updt.prepare("UPDATE program SET first = 0, last = 0;");
        if (!updt.exec())
            MythDB::DBError("Clearing first and last showings", updt);

        LOG(VB_GENERAL, LOG_INFO, "Marking episode first showings.");
        updt.prepare("UPDATE program "
                     "JOIN (SELECT MIN(starttime) AS starttime, programid "
                     "      FROM program "
                     "      WHERE programid <> '' "
                     "      GROUP BY programid "
                     "     ) AS firsts "
                     "ON program.programid = firsts.programid "
                     "  AND program.starttime = firsts.starttime "
                     "SET program.first=1;");
        if (!updt.exec())
            MythDB::DBError("Marking first showings by id", updt);
        int found = updt.numRowsAffected();

        updt.prepare("UPDATE program "
                      "JOIN (SELECT MIN(starttime) AS starttime, title, subtitle,"
                      "           LEFT(description, 1024) AS partdesc "
                      "      FROM program "
                      "      WHERE programid = '' "
                      "      GROUP BY title, subtitle, partdesc "
                      "     ) AS firsts "
                      "ON program.starttime = firsts.starttime "
                      "  AND program.title = firsts.title "
                      "  AND program.subtitle = firsts.subtitle "
                      "  AND LEFT(program.description, 1024) = firsts.partdesc "
                      "SET program.first = 1 "
                      "WHERE program.programid = '';");
        if (!updt.exec())
            MythDB::DBError("Marking first showings", updt);
        found += updt.numRowsAffected();
        LOG(VB_GENERAL, LOG_INFO, QString("    Found %1").arg(found));

        LOG(VB_GENERAL, LOG_INFO, "Marking episode last showings.");
        updt.prepare("UPDATE program "
                     "JOIN (SELECT MAX(starttime) AS starttime, programid "
                     "      FROM program "
                     "      WHERE programid <> '' "
                     "      GROUP BY programid "
                     "     ) AS lasts "
                     "ON program.programid = lasts.programid "
                     "  AND program.starttime = lasts.starttime "
                     "SET program.last=1;");
        if (!updt.exec())
            MythDB::DBError("Marking last showings by id", updt);
        found = updt.numRowsAffected();

        updt.prepare("UPDATE program "
                      "JOIN (SELECT MAX(starttime) AS starttime, title, subtitle,"
                      "           LEFT(description, 1024) AS partdesc "
                      "      FROM program "
                      "      WHERE programid = '' "
                      "      GROUP BY title, subtitle, partdesc "
                      "     ) AS lasts "
                      "ON program.starttime = lasts.starttime "
                      "  AND program.title = lasts.title "
                      "  AND program.subtitle = lasts.subtitle "
                      "  AND LEFT(program.description, 1024) = lasts.partdesc "
                      "SET program.last = 1 "
                      "WHERE program.programid = '';");
        if (!updt.exec())
            MythDB::DBError("Marking last showings", updt);
        found += updt.numRowsAffected();
        LOG(VB_GENERAL, LOG_INFO, QString("    Found %1").arg(found));
    }

    if (1) // limit MSqlQuery's lifetime
    {
        MSqlQuery query(MSqlQuery::InitCon());
        query.prepare("SELECT count(previouslyshown) "
                      "FROM program WHERE previouslyshown = 1;");
        if (query.exec() && query.next())
        {
            if (query.value(0).toInt() != 0)
                gCoreContext->SaveSettingOnHost("HaveRepeats", "1", NULL);
            else
                gCoreContext->SaveSettingOnHost("HaveRepeats", "0", NULL);
        }
    }

    if ((usingDataDirect) &&
        (gCoreContext->GetNumSetting("MythFillGrabberSuggestsTime", 1)))
    {
        fill_data.ddprocessor.GrabNextSuggestedTime();
    }

    LOG(VB_GENERAL, LOG_INFO, "\n"
            "===============================================================\n"
            "| Attempting to contact the master backend for rescheduling.  |\n"
            "| If the master is not running, rescheduling will happen when |\n"
            "| the master backend is restarted.                            |\n"
            "===============================================================");

    if (grab_data || mark_repeats)
        ScheduledRecording::RescheduleMatch(0, 0, 0, QDateTime(),
                                            "MythFillDatabase");

    gCoreContext->SendMessage("CLEAR_SETTINGS_CACHE");

    gCoreContext->SendSystemEvent("MYTHFILLDATABASE_RAN");

    LOG(VB_GENERAL, LOG_NOTICE, "mythfilldatabase run complete.");

    return GENERIC_EXIT_OK;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
