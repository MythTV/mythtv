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
#include "mythverbose.h"
#include "mythversion.h"
#include "util.h"
#include "langsettings.h"

#include "mythconfig.h"

// libmythtv headers
#include "scheduledrecording.h"
#include "remoteutil.h"
#include "videosource.h" // for is_grabber..
#include "dbcheck.h"
#include "mythsystemevent.h"

// filldata headers
#include "filldata.h"

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);
    FillData fill_data;
    int argpos = 1;
    int fromfile_id = 1;
    int fromfile_offset = 0;
    QString fromfile_name;
    bool from_xawfile = false;
    int fromxawfile_id = 1;
    QString fromxawfile_name;
    bool from_file = false;
    bool mark_repeats = true;

    bool usingDataDirect = false, usingDataDirectLabs = false;
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

    QFileInfo finfo(a.argv()[0]);
    QString binname = finfo.baseName();

    myth_nice(19);

    while (argpos < a.argc())
    {
        // The manual and update flags should be mutually exclusive.
        if (!strcmp(a.argv()[argpos], "--manual"))
        {
            cout << "###\n";
            cout << "### Running in manual channel configuration mode.\n";
            cout << "### This will ask you questions about every channel.\n";
            cout << "###\n";
            fill_data.chan_data.interactive = true;
        }
        else if (!strcmp(a.argv()[argpos], "--preset"))
        {
            // For using channel preset values instead of channel numbers.
            cout << "###\n";
            cout << "### Running in preset channel configuration mode.\n";
            cout << "### This will assign channel ";
            cout << "preset numbers to every channel.\n";
            cout << "###\n";
            fill_data.chan_data.channel_preset = true;
        }
        else if (!strcmp(a.argv()[argpos], "--update"))
        {
            // For running non-destructive updates on the database for
            // users in xmltv zones that do not provide channel data.
            fill_data.chan_data.non_us_updating = true;
        }
        else if (!strcmp(a.argv()[argpos], "--file"))
        {
            if (((argpos + 2) >= a.argc()) ||
                !strncmp(a.argv()[argpos + 1], "--", 2) ||
                !strncmp(a.argv()[argpos + 2], "--", 2))
            {
                printf("missing or invalid parameters for --file option\n");
                return FILLDB_EXIT_INVALID_CMDLINE;
            }

            if (!fromfile_name.isEmpty())
            {
                printf("only one --file option allowed\n");
                return FILLDB_EXIT_INVALID_CMDLINE;
            }

            fromfile_id = atoi(a.argv()[++argpos]);
            fromfile_name = a.argv()[++argpos];

            VERBOSE(VB_GENERAL, "Bypassing grabbers, reading directly from file");
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

            if (!fromfile_name.isEmpty())
            {
                printf("only one --dd-file option allowed\n");
                return FILLDB_EXIT_INVALID_CMDLINE;
            }

            fromfile_id = atoi(a.argv()[++argpos]);
            fromfile_offset = atoi(a.argv()[++argpos]);
            fromddfile_lineupid = a.argv()[++argpos];
            fromfile_name = a.argv()[++argpos];

            VERBOSE(VB_GENERAL, "Bypassing grabbers, reading directly from file");
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

            if (!fromxawfile_name.isEmpty())
            {
                printf("only one --xawchannels option allowed\n");
                return FILLDB_EXIT_INVALID_CMDLINE;
            }

            fromxawfile_id = atoi(a.argv()[++argpos]);
            fromxawfile_name = a.argv()[++argpos];

            VERBOSE(VB_GENERAL, "Reading channels from xawtv configfile");
            from_xawfile = true;
        }
        else if (!strcmp(a.argv()[argpos], "--do-channel-updates"))
        {
            fill_data.chan_data.channel_updates = true;
        }
        else if (!strcmp(a.argv()[argpos], "--remove-new-channels"))
        {
            fill_data.chan_data.remove_new_channels = true;
        }
        else if (!strcmp(a.argv()[argpos], "--do-not-filter-new-channels"))
        {
            fill_data.chan_data.filter_new_channels = false;
        }
        else if (!strcmp(a.argv()[argpos], "--graboptions"))
        {
            if (((argpos + 1) >= a.argc()))
            {
                printf("missing parameter for --graboptions option\n");
                return FILLDB_EXIT_INVALID_CMDLINE;
            }

            fill_data.graboptions = QString(" ") + QString(a.argv()[++argpos]);
        }
        else if (!strcmp(a.argv()[argpos], "--sourceid"))
        {
            if (((argpos + 1) >= a.argc()))
            {
                printf("missing parameter for --sourceid option\n");
                return FILLDB_EXIT_INVALID_CMDLINE;
            }

            sourceid = QString(a.argv()[++argpos]).toInt();
        }
        else if (!strcmp(a.argv()[argpos], "--cardtype"))
        {
            if (!sourceid)
            {
                printf("--cardtype option must follow a --sourceid option\n");
                return FILLDB_EXIT_INVALID_CMDLINE;
            }

            if (((argpos + 1) >= a.argc()))
            {
                printf("missing parameter for --cardtype option\n");
                return FILLDB_EXIT_INVALID_CMDLINE;
            }

            fill_data.chan_data.cardtype =
                QString(a.argv()[++argpos]).trimmed().toUpper();
        }
        else if (!strcmp(a.argv()[argpos], "--max-days"))
        {
            if (((argpos + 1) >= a.argc()))
            {
                printf("missing parameter for --max-days option\n");
                return FILLDB_EXIT_INVALID_CMDLINE;
            }

            fill_data.maxDays = QString(a.argv()[++argpos]).toUInt();

            if (fill_data.maxDays < 1)
            {
                printf("ignoring invalid parameter for --max-days\n");
                fill_data.maxDays = 0;
            }
            else if (fill_data.maxDays == 1)
            {
                fill_data.SetRefresh(0, true);
            }
        }
        else if (!strcmp(a.argv()[argpos], "--refresh-today"))
        {
            fill_data.SetRefresh(0, true);
        }
        else if (!strcmp(a.argv()[argpos], "--dont-refresh-tomorrow"))
        {
            fill_data.SetRefresh(1, false);
        }
        else if (!strcmp(a.argv()[argpos], "--refresh-second"))
        {
            fill_data.SetRefresh(2, true);
        }
        else if (!strcmp(a.argv()[argpos], "--refresh-all"))
        {
            fill_data.SetRefresh(FillData::kRefreshAll, true);
        }
        else if (!strcmp(a.argv()[argpos], "--refresh-day"))
        {
            if (((argpos + 1) >= a.argc()))
            {
                printf("missing parameter for --refresh-day option\n");
                return FILLDB_EXIT_INVALID_CMDLINE;
            }

            bool ok = true;
            uint day = QString(a.argv()[++argpos]).toUInt(&ok);

            if (!ok)
            {
                printf("ignoring invalid parameter for --refresh-day\n");
            }
            else
            {
                fill_data.SetRefresh(day, true);
            }
        }
        else if (!strcmp(a.argv()[argpos], "--dont-refresh-tba"))
        {
            fill_data.refresh_tba = false;
        }
        else if (!strcmp(a.argv()[argpos], "--only-update-channels"))
        {
            fill_data.only_update_channels = true;
        }
        else if (!strcmp(a.argv()[argpos],"-v") ||
                 !strcmp(a.argv()[argpos],"--verbose"))
        {
            if (a.argc()-1 > argpos)
            {
                if (parse_verbose_arg(a.argv()[argpos+1]) ==
                        GENERIC_EXIT_INVALID_CMDLINE)
                    return GENERIC_EXIT_INVALID_CMDLINE;

                ++argpos;
            }
            else
            {
                cerr << "Missing argument to -v/--verbose option\n";
                return GENERIC_EXIT_INVALID_CMDLINE;
            }
        }
        else if (!strcmp(a.argv()[argpos], "--dd-grab-all"))
        {
            fill_data.SetRefresh(FillData::kRefreshClear, false);
            fill_data.dd_grab_all = true;
        }
        else if (!strcmp(a.argv()[argpos], "--quiet"))
        {
            print_verbose_messages = VB_NONE;
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
                    export_icon_data_filename = '-';
                }
            }
            else
            {
                export_icon_data_filename = a.argv()[++argpos];
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
                    import_icon_data_filename = '-';
                }
            }
            else
            {
                import_icon_data_filename = a.argv()[++argpos];
            }
        }
        else if (!strcmp(a.argv()[argpos], "--update-icon-map"))
        {
            update_icon_data = true;
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
            cout << "   Stops the addition of new channels and the changing of channel icons.\n";
            cout << "\n";
            cout << "--preset\n";
            cout << "   Use it in case that you want to assign a preset number for\n";
            cout << "   each channel, useful for non US countries where people\n";
            cout << "   are used to assigning a sequenced number for each channel, i.e.:\n";
            cout << "   1->TVE1(S41), 2->La 2(SE18), 3->TV3(21), 4->Canal 33(60)...\n";
            cout << "\n";
            cout << "--file <sourceid> <xmlfile>\n";
            cout << "   Bypass the grabbers and read data directly from a file\n";
            cout << "   <sourceid> = number of the video source to use with this file\n";
            cout << "   <xmlfile>  = file to read\n";
            cout << "\n";
            cout << "--dd-file <sourceid> <offset> <lineupid> <xmlfile>\n";
            cout << "   <sourceid> = number of the video source to use with this file\n";
            cout << "   <offset>   = days from today that xmlfile defines\n";
            cout << "                (-1 means to replace all data, up to 10 days)\n";
            cout << "   <lineupid> = the lineup id\n";
            cout << "   <xmlfile>  = file to read\n";
            cout << "\n";
            cout << "--xawchannels <sourceid> <xawtvrcfile>\n";
            cout << "   (--manual flag works in combination with this)\n";
            cout << "   Read channels as defined in xawtvrc file given\n";
            cout << "   <sourceid>    = cardinput\n";
            cout << "   <xawtvrcfile> = file to read\n";
            cout << "\n";
            cout << "--do-channel-updates\n";
            cout << "   When using DataDirect, ask mythfilldatabase to\n";
            cout << "   overwrite channel names, frequencies, etc. with the\n";
            cout << "   values available from the data source. This will \n";
            cout << "   override custom channel names, which is why it is\n";
            cout << "   off by default.\n";
            cout << "\n";
            cout << "--remove-new-channels\n";
            cout << "   When using DataDirect, ask mythfilldatabase to\n";
            cout << "   remove new channels (those not in the database)\n";
            cout << "   from the DataDirect lineup.  These channels are\n";
            cout << "   removed from the lineup as if you had done so\n";
            cout << "   via the DataDirect website's Lineup Wizard, but\n";
            cout << "   may be re-added manually and incorporated into\n";
            cout << "   MythTV by running mythfilldatabase without this\n";
            cout << "   option.  New channels are automatically removed\n";
            cout << "   for DVB and HDTV sources that use DataDirect.\n";
            cout << "\n";
            cout << "--do-not-filter-new-channels\n";
            cout << "   Normally MythTV tries to avoid adding ATSC channels\n";
            cout << "   to NTSC channel lineups. This option restores the\n";
            cout << "   behaviour of adding every channel in the downloaded\n";
            cout << "   channel lineup to MythTV's lineup, in case MythTV's\n";
            cout << "   smarts fail you.\n";
            cout << "\n";
            cout << "--graboptions <\"options\">\n";
            cout << "   Pass options to grabber. Do NOT use unless you know\n";
            cout << "   what you are doing. Mythfilldatabase will\n";
            cout << "   automatically use the correct options for xmltv\n";
            cout << "   compliant grabbers.\n";
            cout << "\n";
            cout << "--sourceid <number>\n";
            cout << "   Only refresh data for sourceid given\n";
            cout << "\n";
            cout << "--max-days <number>\n";
            cout << "   Force the maximum number of days, counting today,\n";
            cout << "   for the grabber to check for future listings\n";
            cout << "--only-update-channels\n";
            cout << "   Get as little listings data as possible to update channels\n";
            cout << "--refresh-today\n";
            cout << "--refresh-second\n";
            cout << "--refresh-all\n";
            cout << "--refresh-day <number>";
            cout << "   (Only valid for selected grabbers: e.g. DataDirect)\n";
            cout << "   Force a refresh today, two days, every day, or a specific day from now,\n";
            cout << "   to catch the latest changes\n";
            cout << "--dont-refresh-tomorrow\n";
            cout << "   Tomorrow will always be refreshed unless this argument is used\n";
            cout << "--dont-refresh-tba\n";
            cout << "   \"To be announced\" programs will always be refreshed \n";
            cout << "   unless this argument is used\n";
            cout << "\n";
            cout << "--export-icon-map [<filename>]\n";
            cout << "   Exports your current icon map to <filename> (default: "
                 << export_icon_data_filename.toLocal8Bit().constData()
                 << ")\n";
            cout << "--import-icon-map [<filename>]\n";
            cout << "   Imports an icon map from <filename> (default: "
                 << import_icon_data_filename.toLocal8Bit().constData()
                 << ")\n";
            cout << "--update-icon-map\n";
            cout << "   Updates icon map icons only\n";
            cout << "--reset-icon-map [all]\n";
            cout << "   Resets your icon map (pass all to reset channel icons as well)\n";
            cout << "\n";
            cout << "--mark-repeats\n";
            cout << "   Marks any programs with a OriginalAirDate earlier\n";
            cout << "   than their start date as a repeat\n";
            cout << "\n";
            cout << "-v or --verbose debug-level\n";
            cout << "   Use '-v help' for level info\n";
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
        else if (!strcmp(a.argv()[argpos], "--no-delete"))
        {
            cerr << "Deprecated option '" << a.argv()[argpos] << "'" << endl;
        }
        else
        {
            fprintf(stderr, "illegal option: '%s' (use --help)\n",
                    a.argv()[argpos]);
            return FILLDB_EXIT_INVALID_CMDLINE;
        }

        ++argpos;
    }

    gContext = new MythContext(MYTH_BINARY_VERSION);
    if (!gContext->Init(false))
    {
        VERBOSE(VB_IMPORTANT, "Failed to init MythContext, exiting.");
        delete gContext;
        return FILLDB_EXIT_NO_MYTHCONTEXT;
    }

    gCoreContext->SetAppName(binname);

    LanguageSettings::load("mythfrontend");

    if (!UpgradeTVDatabaseSchema(false))
    {
        VERBOSE(VB_IMPORTANT, "Incorrect database schema");
        delete gContext;
        return GENERIC_EXIT_DB_OUTOFDATE;
    }

    gCoreContext->LogEntry("mythfilldatabase", LP_INFO,
                       "Listings Download Started", "");

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
                      "AND manualid = 0;");
        query.bindValue(":SRCID", fromfile_id);

        if (query.exec() && query.next())
        {
            if (!query.isNull(0))
                GuideDataBefore = QDateTime::fromString(query.value(0).toString(),
                                                    Qt::ISODate);
        }

        if (!fill_data.GrabDataFromFile(fromfile_id, fromfile_name))
        {
            delete gContext;
            return FILLDB_EXIT_GRAB_DATA_FAILED;
        }

        updateLastRunEnd(query);

        query.prepare("SELECT MAX(endtime) FROM program p LEFT JOIN channel c "
                      "ON p.chanid=c.chanid WHERE c.sourceid= :SRCID "
                      "AND manualid = 0;");
        query.bindValue(":SRCID", fromfile_id);

        if (query.exec() && query.next())
        {
            if (!query.isNull(0))
                GuideDataAfter = QDateTime::fromString(query.value(0).toString(),
                                                   Qt::ISODate);
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
            VERBOSE(VB_GENERAL,
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
                       usingDataDirectLabs |=
                           is_grabber_labs(newsource.xmltvgrabber);
                  }
             }
             else
             {
                  VERBOSE(VB_IMPORTANT,
                          "There are no channel sources defined, did you run "
                          "the setup program?");
                  gCoreContext->LogEntry("mythfilldatabase", LP_CRITICAL,
                                     "No channel sources defined",
                                     "Could not find any defined channel "
                                     "sources - did you run the setup "
                                     "program?");
                  delete gContext;
                  return FILLDB_EXIT_NO_CHAN_SRC;
             }
        }
        else
        {
             MythDB::DBError("loading channel sources", sourcequery);
             delete gContext;
             return FILLDB_EXIT_DB_ERROR;
        }

        if (!fill_data.Run(sourcelist))
        {
             VERBOSE(VB_IMPORTANT, "Failed to fetch some program info");
             gCoreContext->LogEntry("mythfilldatabase", LP_WARNING,
                                "Failed to fetch some program info", "");
        }
        else
            VERBOSE(VB_IMPORTANT, "Data fetching complete.");
    }

    if (fill_data.only_update_channels && !fill_data.need_post_grab_proc)
    {
        delete gContext;
        return FILLDB_EXIT_OK;
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
            return FILLDB_EXIT_DB_ERROR;
        }

        while (query.next())
            fill_data.icon_data.UpdateSourceIcons(query.value(0).toInt());
    }

    if (grab_data)
    {
        VERBOSE(VB_GENERAL, "Adjusting program database end times.");
        int update_count = ProgramData::fix_end_times();
        if (update_count == -1)
            VERBOSE(VB_IMPORTANT, "fix_end_times failed!");
        else
            VERBOSE(VB_GENERAL,
                    QString("    %1 replacements made").arg(update_count));

        gCoreContext->LogEntry("mythfilldatabase", LP_INFO,
                           "Listings Download Finished", "");
    }

    if (grab_data)
    {
        VERBOSE(VB_GENERAL, "Marking generic episodes.");

        MSqlQuery query(MSqlQuery::InitCon());
        query.prepare("UPDATE program SET generic = 1 WHERE "
            "((programid = '' AND subtitle = '' AND description = '') OR "
            " (programid <> '' AND category_type = 'series' AND "
            "  program.programid LIKE '%0000'));");

        if (!query.exec())
            MythDB::DBError("mark generic", query);
        else
            VERBOSE(VB_GENERAL,
                    QString("    Found %1").arg(query.numRowsAffected()));
    }

    if (grab_data)
    {
        VERBOSE(VB_GENERAL, "Fudging non-unique programids "
                "with multiple parts.");

        int found = 0;
        MSqlQuery sel(MSqlQuery::InitCon());
        sel.prepare("SELECT DISTINCT programid, partnumber, parttotal "
                    "FROM program WHERE partnumber > 0 AND parttotal > 0 AND "
                    "programid LIKE '%0000'");
        if (sel.exec())
        {
            MSqlQuery repl(MSqlQuery::InitCon());

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

                VERBOSE(VB_GENERAL,
                        QString("    %1 -> %2 (part %3 of %4)")
                        .arg(orig_programid).arg(new_programid)
                        .arg(partnum).arg(parttotal));

                repl.prepare("UPDATE program SET programid = :NEWID "
                             "WHERE programid = :OLDID AND "
                             "partnumber = :PARTNUM AND "
                             "parttotal = :PARTTOTAL");
                repl.bindValue(":NEWID", new_programid);
                repl.bindValue(":OLDID", orig_programid);
                repl.bindValue(":PARTNUM",   partnum);
                repl.bindValue(":PARTTOTAL", parttotal);
                if (!repl.exec())
                {
                    VERBOSE(VB_GENERAL,
                            QString("Fudging programid from '%1' to '%2'")
                            .arg(orig_programid)
                            .arg(new_programid));
                }
                else
                    found += repl.numRowsAffected();
            }
        }

        VERBOSE(VB_GENERAL, QString("    Found %1").arg(found));
    }

    if (mark_repeats)
    {
        VERBOSE(VB_GENERAL, "Marking repeats.");

        int newEpiWindow = gCoreContext->GetNumSetting( "NewEpisodeWindow", 14);

        MSqlQuery query(MSqlQuery::InitCon());
        query.prepare("UPDATE program SET previouslyshown = 1 "
                      "WHERE previouslyshown = 0 "
                      "AND originalairdate is not null "
                      "AND (to_days(starttime) - to_days(originalairdate)) "
                      "    > :NEWWINDOW;");
        query.bindValue(":NEWWINDOW", newEpiWindow);

        if (query.exec())
            VERBOSE(VB_GENERAL,
                    QString("    Found %1").arg(query.numRowsAffected()));

        VERBOSE(VB_GENERAL, "Unmarking new episode rebroadcast repeats.");
        query.prepare("UPDATE program SET previouslyshown = 0 "
                      "WHERE previouslyshown = 1 "
                      "AND originalairdate is not null "
                      "AND (to_days(starttime) - to_days(originalairdate)) "
                      "    <= :NEWWINDOW;");
        query.bindValue(":NEWWINDOW", newEpiWindow);

        if (query.exec())
            VERBOSE(VB_GENERAL,
                    QString("    Found %1").arg(query.numRowsAffected()));
    }

    // Mark first and last showings

    if (grab_data)
    {
        MSqlQuery updt(MSqlQuery::InitCon());
        updt.prepare("UPDATE program SET first = 0, last = 0;");
        if (!updt.exec())
            MythDB::DBError("Clearing first and last showings", updt);

        VERBOSE(VB_GENERAL, "Marking episode first showings.");

        MSqlQuery query(MSqlQuery::InitCon());
        query.prepare("SELECT MIN(starttime),programid FROM program "
                      "WHERE programid > '' GROUP BY programid;");
        if (query.exec())
        {
            while(query.next())
            {
                updt.prepare("UPDATE program set first = 1 "
                             "WHERE starttime = :STARTTIME "
                             "  AND programid = :PROGRAMID;");
                updt.bindValue(":STARTTIME", query.value(0).toDateTime());
                updt.bindValue(":PROGRAMID", query.value(1).toString());
                if (!updt.exec())
                    MythDB::DBError("Marking first showings by id", updt);
            }
        }
        int found = query.size();
        query.prepare("SELECT MIN(starttime),title,subtitle,description "
                      "FROM program WHERE programid = '' "
                      "GROUP BY title,subtitle,description;");
        if (query.exec())
        {
            while(query.next())
            {
                updt.prepare("UPDATE program set first = 1 "
                             "WHERE starttime = :STARTTIME "
                             "  AND title = :TITLE "
                             "  AND subtitle = :SUBTITLE "
                             "  AND description = :DESCRIPTION");
                updt.bindValue(":STARTTIME", query.value(0).toDateTime());
                updt.bindValue(":TITLE", query.value(1).toString());
                updt.bindValue(":SUBTITLE", query.value(2).toString());
                updt.bindValue(":DESCRIPTION", query.value(3).toString());
                if (!updt.exec())
                    MythDB::DBError("Marking first showings", updt);
            }
        }
        found += query.size();
        VERBOSE(VB_GENERAL, QString("    Found %1").arg(found));

        VERBOSE(VB_GENERAL, "Marking episode last showings.");
        query.prepare("SELECT MAX(starttime),programid FROM program "
                      "WHERE programid > '' GROUP BY programid;");
        if (query.exec())
        {
            while(query.next())
            {
                updt.prepare("UPDATE program set last = 1 "
                             "WHERE starttime = :STARTTIME "
                             "  AND programid = :PROGRAMID;");
                updt.bindValue(":STARTTIME", query.value(0).toDateTime());
                updt.bindValue(":PROGRAMID", query.value(1).toString());
                if (!updt.exec())
                    MythDB::DBError("Marking last showings by id", updt);
            }
        }
        found = query.size();
        query.prepare("SELECT MAX(starttime),title,subtitle,description "
                      "FROM program WHERE programid = '' "
                      "GROUP BY title,subtitle,description;");
        if (query.exec())
        {
            while(query.next())
            {
                updt.prepare("UPDATE program set last = 1 "
                             "WHERE starttime = :STARTTIME "
                             "  AND title = :TITLE "
                             "  AND subtitle = :SUBTITLE "
                             "  AND description = :DESCRIPTION");
                updt.bindValue(":STARTTIME", query.value(0).toDateTime());
                updt.bindValue(":TITLE", query.value(1).toString());
                updt.bindValue(":SUBTITLE", query.value(2).toString());
                updt.bindValue(":DESCRIPTION", query.value(3).toString());
                if (!updt.exec())
                    MythDB::DBError("Marking last showings", updt);
            }
        }
        found += query.size();
        VERBOSE(VB_GENERAL, QString("    Found %1").arg(found));
    }

    if (1) // limit MSqlQuery's lifetime
    {
        MSqlQuery query(MSqlQuery::InitCon());
        query.prepare("SELECT count(previouslyshown) "
                      "FROM program WHERE previouslyshown = 1;");
        if (query.exec() && query.next())
        {
            if (query.value(0).toInt() != 0)
            {
                query.prepare("UPDATE settings SET data = '1' "
                              "WHERE value = 'HaveRepeats';");
                if (!query.exec())
                    MythDB::DBError("Setting HaveRepeats", query);
            }
            else
            {
                query.prepare("UPDATE settings SET data = '0' "
                              "WHERE value = 'HaveRepeats';");
                if (!query.exec())
                    MythDB::DBError("Clearing HaveRepeats", query);
            }
        }
    }

    if ((usingDataDirect) &&
        (gCoreContext->GetNumSetting("MythFillGrabberSuggestsTime", 1)))
    {
        fill_data.ddprocessor.GrabNextSuggestedTime();
    }

    if (usingDataDirectLabs ||
        !gCoreContext->GetNumSetting("MythFillFixProgramIDsHasRunOnce", 0))
    {
        DataDirectProcessor::FixProgramIDs();
    }

    VERBOSE(VB_GENERAL, "\n"
            "===============================================================\n"
            "| Attempting to contact the master backend for rescheduling.  |\n"
            "| If the master is not running, rescheduling will happen when |\n"
            "| the master backend is restarted.                            |\n"
            "===============================================================");

    if (grab_data || mark_repeats)
        ScheduledRecording::signalChange(-1);

    RemoteSendMessage("CLEAR_SETTINGS_CACHE");

    SendMythSystemEvent("MYTHFILLDATABASE_RAN");

    delete gContext;

    VERBOSE(VB_IMPORTANT, "mythfilldatabase run complete.");

    return FILLDB_EXIT_OK;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
