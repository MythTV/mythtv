// C headers
#include <unistd.h>

// C++ headers
#include <iostream>
using namespace std;

// Qt headers
#include <qapplication.h>

// libmyth headers
#include "exitcodes.h"
#include "mythcontext.h"
#include "mythdbcon.h"

// libmythtv headers
#include "scheduledrecording.h"
#include "remoteutil.h"

// filldata headers
#include "filldata.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv, false);
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
        else if (!strcmp(a.argv()[argpos], "--no-delete"))
        {
            // Do not delete old programs from the database until 7 days old.
            fill_data.prog_data.no_delete = true;
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

            if (fill_data.IsVerbose())
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

            if (fill_data.IsVerbose())
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

            if (fill_data.IsVerbose())
                 cout << "### reading channels from xawtv configfile\n";
            from_xawfile = true;
        }
        else if ((!strcmp(a.argv()[argpos], "--do-channel-updates")) ||
                 (!strcmp(a.argv()[argpos], "--do_channel_updates")))
        {
            fill_data.chan_data.channel_updates = true;
        }
        else if (!strcmp(a.argv()[argpos], "--remove-new-channels"))
        {
            fill_data.chan_data.remove_new_channels = true;
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
                QString(a.argv()[++argpos]).stripWhiteSpace().upper();
        }
        else if (!strcmp(a.argv()[argpos], "--max-days"))
        {
            if (((argpos + 1) >= a.argc()))
            {
                printf("missing parameter for --max-days option\n");
                return FILLDB_EXIT_INVALID_CMDLINE;
            }

            fill_data.maxDays = QString(a.argv()[++argpos]).toInt();

            if (fill_data.maxDays < 1 || fill_data.maxDays > 21)
            {
                printf("ignoring invalid parameter for --max-days\n");
                fill_data.maxDays = 0;
            }
        }
        else if (!strcmp(a.argv()[argpos], "--refresh-today"))
        {
            fill_data.refresh_today = true;
        }
        else if (!strcmp(a.argv()[argpos], "--dont-refresh-tomorrow"))
        {
            fill_data.refresh_tomorrow = false;
        }
        else if (!strcmp(a.argv()[argpos], "--refresh-second"))
        {
            fill_data.refresh_second = true;
        }
        else if (!strcmp(a.argv()[argpos], "--refresh-all"))
        {
            fill_data.refresh_all = true;
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
#if 0
        else if (!strcmp(a.argv()[argpos], "--dd-grab-all"))
        {
            fill_data.dd_grab_all = true;
            fill_data.refresh_today = false;
            fill_data.refresh_tomorrow = false;
            fill_data.refresh_second = false;
        }
#endif
        else if (!strcmp(a.argv()[argpos], "--quiet"))
        {
            fill_data.SetQuiet(true);
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
                    fill_data.SetQuiet(true);
                    export_icon_data_filename = "-";
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
                    import_icon_data_filename = "-";
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
            cout << "   <sourceid> = number for the video source to use with this file\n";
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
            cout << "--graboptions <\"options\">\n";
            cout << "   Pass options to grabber\n";
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
            cout << "   (Only valid for selected grabbers: e.g. DataDirect)\n";
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
                    << export_icon_data_filename << ")\n";
            cout << "--import-icon-map [<filename>]\n";
            cout << "   Imports an icon map from <filename> (default: " <<
                    import_icon_data_filename << ")\n";
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
    if (!gContext->Init(false))
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
        fill_data.readXawtvChannels(fromxawfile_id, fromxawfile_name);
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

        query.exec("SELECT MAX(endtime) FROM program WHERE manualid = 0;");
        if (query.isActive() && query.size() > 0)
        {
            query.next();

            if (!query.isNull(0))
                GuideDataBefore = QDateTime::fromString(query.value(0).toString(),
                                                    Qt::ISODate);
        }

        if (!fill_data.grabDataFromFile(fromfile_id, fromfile_name))
            return FILLDB_EXIT_GRAB_DATA_FAILED;

        fill_data.prog_data.clearOldDBEntries();

        query.exec(QString("UPDATE settings SET data ='%1' "
                           "WHERE value='mythfilldatabaseLastRunEnd'")
                          .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm")));

        query.exec("SELECT MAX(endtime) FROM program WHERE manualid = 0;");
        if (query.isActive() && query.size() > 0)
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
        fill_data.grabDataFromDDFile(
            fromfile_id, fromfile_offset, fromfile_name, fromddfile_lineupid);
        fill_data.prog_data.clearOldDBEntries();
    }
    else
    {
        QValueList<Source> sourcelist;

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
        sourcequery.exec(querystr);

        if (sourcequery.isActive())
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

                       sourcelist.append(newsource);
                       if (newsource.xmltvgrabber == "datadirect")
                           usingDataDirect = true;
                  }
             }
             else
             {
                  VERBOSE(VB_IMPORTANT,
                          "There are no channel sources defined, did you run "
                          "the setup program?");
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
    
        if (!fill_data.fillData(sourcelist))
        {
             VERBOSE(VB_IMPORTANT, "Failed to fetch some program info");
             gContext->LogEntry("mythfilldatabase", LP_WARNING,
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
        query.exec("SELECT sourceid FROM videosource ORDER BY sourceid;");
        if (query.isActive() && query.size() > 0)
        {
            while (query.next())
            {
                fill_data.icon_data.UpdateSourceIcons(query.value(0).toInt());
            }
        }
    }

    if (grab_data)
    {
        VERBOSE(VB_GENERAL, "Adjusting program database end times.");
        int update_count = ProgramData::fix_end_times();
        if (update_count == -1)
            VERBOSE(VB_IMPORTANT, "fix_end_times failed!");
        else if (fill_data.IsVerbose())
            VERBOSE(VB_GENERAL,
                    QString("    %1 replacements made").arg(update_count));

        gContext->LogEntry("mythfilldatabase", LP_INFO,
                           "Listings Download Finished", "");
    }

    if (grab_data)
    {
        VERBOSE(VB_GENERAL, "Marking generic episodes.");

        MSqlQuery query(MSqlQuery::InitCon());
        query.exec("UPDATE program SET generic = 1 WHERE "
            "((programid = '' AND subtitle = '' AND description = '') OR "
            " (programid <> '' AND category_type = 'series' AND "
            "  program.programid LIKE '%0000'));");

        VERBOSE(VB_GENERAL,
                QString("    Found %1").arg(query.numRowsAffected()));
    }

    if (mark_repeats)
    {
        VERBOSE(VB_GENERAL, "Marking repeats.");
       
        int newEpiWindow = gContext->GetNumSetting( "NewEpisodeWindow", 14);
        
        MSqlQuery query(MSqlQuery::InitCon());
        query.exec( QString( "UPDATE program SET previouslyshown = 1 "
                    "WHERE previouslyshown = 0 "
                    "AND originalairdate is not null "
                    "AND (to_days(starttime) - to_days(originalairdate)) > %1;")
                    .arg(newEpiWindow));
        
        VERBOSE(VB_GENERAL,
                QString("    Found %1").arg(query.numRowsAffected()));
            
        VERBOSE(VB_GENERAL, "Unmarking new episode rebroadcast repeats.");
        query.exec( QString( "UPDATE program SET previouslyshown = 0 "
                             "WHERE previouslyshown = 1 "
                             "AND originalairdate is not null "
                             "AND (to_days(starttime) - to_days(originalairdate)) <= %1;")
                             .arg(newEpiWindow));             
    
        VERBOSE(VB_GENERAL,
                QString("    Found %1").arg(query.numRowsAffected()));
    }

    // Mark first and last showings

    if (grab_data)
    {
        MSqlQuery updt(MSqlQuery::InitCon());
        updt.exec("UPDATE program SET first = 0, last = 0;");

        VERBOSE(VB_GENERAL, "Marking episode first showings.");

        MSqlQuery query(MSqlQuery::InitCon());
        query.exec("SELECT MIN(starttime),programid FROM program "
                   "WHERE programid > '' GROUP BY programid;");

        if (query.isActive() && query.size() > 0)
        {
            while(query.next())
            {
                updt.prepare("UPDATE program set first = 1 "
                             "WHERE starttime = :STARTTIME "
                             "  AND programid = :PROGRAMID;");
                updt.bindValue(":STARTTIME", query.value(0).toDateTime());
                updt.bindValue(":PROGRAMID", query.value(1).toString());
                updt.exec();
            }
        }
        int found = query.numRowsAffected();

        query.exec("SELECT MIN(starttime),title,subtitle,description "
                   "FROM program WHERE programid = '' "
                   "GROUP BY title,subtitle,description;");

        if (query.isActive() && query.size() > 0)
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
                updt.exec();
            }
        }
        found += query.numRowsAffected();
        VERBOSE(VB_GENERAL, QString("    Found %1").arg(found));

        VERBOSE(VB_GENERAL, "Marking episode last showings.");

        query.exec("SELECT MAX(starttime),programid FROM program "
                   "WHERE programid > '' GROUP BY programid;");

        if (query.isActive() && query.size() > 0)
        {
            while(query.next())
            {
                updt.prepare("UPDATE program set last = 1 "
                             "WHERE starttime = :STARTTIME "
                             "  AND programid = :PROGRAMID;");
                updt.bindValue(":STARTTIME", query.value(0).toDateTime());
                updt.bindValue(":PROGRAMID", query.value(1).toString());
                updt.exec();
            }
        }
        found = query.numRowsAffected();

        query.exec("SELECT MAX(starttime),title,subtitle,description "
                   "FROM program WHERE programid = '' "
                   "GROUP BY title,subtitle,description;");

        if (query.isActive() && query.size() > 0)
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
                updt.exec();
            }
        }
        found += query.numRowsAffected();
        VERBOSE(VB_GENERAL, QString("    Found %1").arg(found));
    }

    if (1) // limit MSqlQuery's lifetime
    {
        MSqlQuery query(MSqlQuery::InitCon());
        query.exec( "SELECT count(previouslyshown) FROM program WHERE previouslyshown = 1;");
        if (query.isActive() && query.size() > 0)
        {
            query.next();
            if (query.value(0).toInt() != 0)
                query.exec("UPDATE settings SET data = '1' WHERE value = 'HaveRepeats';");
            else
                query.exec("UPDATE settings SET data = '0' WHERE value = 'HaveRepeats';");
        }
    }

    if ((usingDataDirect) &&
        (gContext->GetNumSetting("MythFillGrabberSuggestsTime", 1)))
    {
        fill_data.ddprocessor.GrabNextSuggestedTime();
    }

    VERBOSE(VB_IMPORTANT, "\n"
            "===============================================================\n"
            "| Attempting to contact the master backend for rescheduling.  |\n"
            "| If the master is not running, rescheduling will happen when |\n"
            "| the master backend is restarted.                            |\n"
            "===============================================================");

    if (grab_data || mark_repeats)
        ScheduledRecording::signalChange(-1);

    RemoteSendMessage("CLEAR_SETTINGS_CACHE");

    delete gContext;

    VERBOSE(VB_IMPORTANT, "mythfilldatabase run complete.");

    return FILLDB_EXIT_OK;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
