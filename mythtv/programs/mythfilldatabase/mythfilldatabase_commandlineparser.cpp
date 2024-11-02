#include <QString>

#include "libmythbase/mythcorecontext.h"
#include "mythfilldatabase_commandlineparser.h"

MythFillDatabaseCommandLineParser::MythFillDatabaseCommandLineParser() :
    MythCommandLineParser(MYTH_APPNAME_MYTHFILLDATABASE)
{ MythFillDatabaseCommandLineParser::LoadArguments(); }

void MythFillDatabaseCommandLineParser::LoadArguments(void)
{
    addHelp();
    addVersion();
    addLogging();
    allowPassthrough();

    add("--manual", "manual", false, "Run interactive configuration",
            "Manual mode will interactively ask you questions about "
            "each channel as it is processed, to configure for "
            "future use.");

    add("--preset", "preset", false,
            "Use channel preset values instead of numbers",
            "For use with assigning preset numbers for each "
            "channel. Useful for non-US countries where people "
            "are used to assigning a sequenced number for each "
            "channel:\n1->TVE1(S41), 2->La 2(SE18), 3->TV(21)...");
    add("--file", "file", false,
            "Bypass grabbers and define sourceid and file",
            "Directly define the sourceid and XMLTV file to "
            "import.")
        ->SetRequires("sourceid");

    add("--sourceid", "sourceid", 0, "Operate on single source",
            "Limit mythfilldatabase to only operate on the "
            "specified channel source.");

    add("--offset", "offset", 0, "Day offset of input xml file",
            "Specify how many days offset from today is the "
            "information in the given XML file.");

    add("--xmlfile", "xmlfile", "", "XML file to import manually",
            "Specify an XML guide data file to import directly "
            "rather than pull data through the specified grabber.")
        ->SetRequiredChildOf("file");


    add("--update", "update", false, "", "")
        ->SetBlocks("manual")
        ->SetRemoved("Use --only-update-guide instead.", "34")
        ->SetGroup("Guide Data Handling");
    add("--only-update-guide", "onlyguide", false, "Only update guide data",
            "Only update the guide data, do not alter channels or icons.")
        ->SetBlocks("manual")
        ->SetGroup("Guide Data Handling");


    add("--do-channel-updates", "dochannelupdates", false,
            "update channels",
            "Ask mythfilldatabase to "
            "overwrite channel names, frequencies, etc. with "
            "values available from the data source. This will "
            "override custom channel names, which is why it "
            "is disabled by default.")
        ->SetGroup("Channel List Handling");
    add("--do-not-filter-new-channels", "nofilterchannels", false,
            "don't filter ATSC channels for addition",
            "Normally, MythTV tries to avoid adding ATSC "
            "channels to NTSC channel lineups. This option "
            "restores the behavior of adding every channel in "
            "the downloaded channel lineup to MythTV's lineup, "
            "in case MythTV's smarts fail you.")
        ->SetGroup("Channel List Handling");
    // need documentation for this one
    add("--cardtype", "cardtype", "", "", "No information.");

    add("--refresh", "refresh", QMetaType::QStringList,
            "Provide a day or range of days to refresh. Can be "
            "used repeatedly.",
            "Provide days to refresh during the grabber run.  Multiple \n"
            "days or ranges can be supplied by multiple instances of the \n"
            "option. Supported days are:\n"
            "    [not]today\n"
            "    [not]tomorrow\n"
            "    [not]second\n"
            "    #[-#]\n"
            "    all\n"
            "Note that if all is specified any others will be ingored.\n\n"
            "example:\n"
            "   --refresh today --refresh 4-8 --refresh nottomorrow")
        ->SetGroup("Filtering");

    add("--max-days", "maxdays", 0, "force number of days to update",
            "Force the maximum number of days, counting today, "
            "for the guide data grabber to check for future "
            "listings.")
        ->SetGroup("Filtering");
    add("--refresh-today", "refreshtoday", false, "", "")
        ->SetRemoved("use --refresh instead", "34")

        ->SetGroup("Filtering");
    add("--dont-refresh-tomorrow", "dontrefreshtomorrow", false, "", "")
        ->SetRemoved("use --refresh instead", "34")
        ->SetGroup("Filtering");
    add("--refresh-second", "refreshsecond", false, "", "")
        ->SetRemoved("use --refresh instead", "34")
        ->SetGroup("Filtering");
    add("--refresh-day", "refreshday", QMetaType::QStringList, "", "")
        ->SetRemoved("use --refresh instead", "34")
        ->SetGroup("Filtering");
    add("--dont-refresh-tba", "dontrefreshtba", false,
            "don't refresh \"To be announced\" programs",
            "This option is only valid for selected grabbers.\n"
            "Prevent mythfilldatabase from automatically refreshing any "
            "programs marked as \"To be announced\".")
        ->SetGroup("Filtering");

    add("--refresh-all", "refreshall", false, "", "")
        ->SetRemoved("use --refresh instead", "34")
        ->SetBlocks("dontrefreshtomorrow")
        ->SetBlocks("refreshsecond")
        ->SetBlocks("refreshday")
        ->SetBlocks("maxdays")
        ->SetGroup("Filtering");

    add("--no-allatonce", "noallatonce", false,
            "Do not use allatonce even if the grabber prefers it.",
            "This option prevents mythfilldatabase from utlizing "
            "the advertised grabber preference of 'allatonce'. "
            "This may be necessary for grabbers that return a large "
            "amount of data")
        ->SetGroup("Filtering");

    add("--only-update-channels", "onlychannels", false,
            "only update channel lineup",
            "Download as little listings data as possible to update the "
            "channel lineup.")
        ->SetGroup("Channel List Handling");
    add("--no-mark-repeats", "markrepeats", true, "do not mark repeats", "");

    add("--dd-grab-all", "ddgraball", false, "", "")
        ->SetRemoved("It's no longer valid with Schedules Direct XMLTV.\n"
          "          Remove in mythtv-setup General -> Program Schedule\n"
                     "          -> Downloading Options -> Guide Data Arguements", "35.0");
    add("--no-resched", "noresched", false,
            "Do not invoke the rescheduler in the backend.",
	    "This option prevents mythfilldatabase from asking the backend "
	    "to invoke the rescheduler after importing new metadata.\n"
	    "This is useful if you need to invoke mythfilldatabase multiple "
	    "times in a row, or if some postprocessing is required before "
	    "the scheduler should see the updated metadata.");
}
