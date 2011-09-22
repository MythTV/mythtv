
using namespace std;

#include <QString>

#include "mythcorecontext.h"
#include "commandlineparser.h"

MythFillDatabaseCommandLineParser::MythFillDatabaseCommandLineParser() :
    MythCommandLineParser(MYTH_APPNAME_MYTHFILLDATABASE)
{ LoadArguments(); }

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
    add("--update", "update", false, "Run non-destructive updates",
            "Run non-destructive updates on the database for "
            "users in xmltv zones that do not provide channel "
            "data. Stops the addition of new channels and the "
            "changing of channel icons.")
        ->SetBlocks("manual");

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
        ->SetBlocks("ddfile")
        ->SetBlocks("xawchannels");
    add("--dd-file", "ddfile", false,
            "Bypass grabber, and read SD data from file",
            "Directly define the data needed to import a local "
            "DataDirect download.")
        ->SetBlocks("xawchannels");
    add("--xawchannels", "xawchannels", false,
            "Read channels from xawtvrc file",
            "Import channels from an xawtvrc file.");

    add("--sourceid", "sourceid", 0, "Operate on single source",
            "Limit mythfilldatabase to only operate on the "
            "specified channel source.")
        ->SetRequiredChildOf("file")
        ->SetRequiredChildOf("ddfile")
        ->SetRequiredChildOf("xawchannels");

    add("--offset", "offset", 0, "Day offset of input xml file",
            "Specify how many days offset from today is the "
            "information in the given XML file.")
        ->SetRequiredChildOf("ddfile");

    add("--lineupid", "lineupid", 0, "DataDirect lineup of input xml file",
            "Specify the DataDirect lineup that corresponds to "
            "the information in the given XML file.")
        ->SetRequiredChildOf("ddfile");

    add("--xmlfile", "xmlfile", "", "XML file to import manually",
            "Specify an XML guide data file to import directly "
            "rather than pull data through the specified grabber.")
        ->SetRequiredChildOf("ddfile")
        ->SetRequiredChildOf("file");

    add("--xawtvrcfile", "xawtvrcfile", "",
            "xawtvrc file to import channels from",
            "Xawtvrc file containing channels to be imported.")
        ->SetRequiredChildOf("xawchannels");

    add("--do-channel-updates", "dochannelupdates", false,
            "update channels using datadirect",
            "When using DataDirect, ask mythfilldatabase to "
            "overwrite channel names, frequencies, etc. with "
            "values available from the data source. This will "
            "override custom channel names, which is why it "
            "is disabled by default.");
    add("--remove-new-channels", "removechannels", false,
            "disable new channels on datadirect web interface",
            "When using DataDirect, ask mythfilldatabase to "
            "mark any new channels as disabled on the remote "
            "lineup. Channels can be manually enabled on the "
            "website at a later time, and incorporated into "
            "MythTV by running mythfilldatabase without this "
            "option. New digital channels cannot be directly "
            "imported and thus are disabled automatically.")
        ->SetBlocks("file");
    add("--do-not-filter-new-channels", "nofilterchannels", false,
            "don't filter ATSC channels for addition",
            "Normally, MythTV tries to avoid adding ATSC "
            "channels to NTSC channel lineups. This option "
            "restores the behavior of adding every channel in "
            "the downloaded channel lineup to MythTV's lineup, "
            "in case MythTV's smarts fail you.");
    // need documentation for this one
    add("--cardtype", "cardtype", "", "", "No information.");

    add("--refresh", "refresh", QVariant::StringList,
            "Provide a day or range of days to refresh. Can be "
            "used repeatedly.",
            "Provide days to refresh during the grabber run.  Multiple \n"
            "days or ranges can be supplied by multiple instances of the \n"
            "option. Supported days are:\n"
            "    [not]today\n"
            "    [not]tomorrow\n"
            "    [not]second\n"
            "    #[-#]\n"
            "    all\n\n"
            "example:\n"
            "   --refresh today --refresh 4-8 --refresh nottomorrow");

    add("--max-days", "maxdays", 0, "force number of days to update",
            "Force the maximum number of days, counting today, "
            "for the guide data grabber to check for future "
            "listings.");
    add("--refresh-today", "refreshtoday", false, "",
            "This option is only valid for selected grabbers.\n"
            "Force a refresh for today's guide data.\nThis can be used "
            "in combination with other --refresh-<n> options.\n"
            "If being used with datadirect, this option should not be "
            "used, rather use --dd-grab-all to pull all listings each time.");
    add("--dont-refresh-tomorrow", "dontrefreshtomorrow", false, "",
            "This option is only valid for selected grabbers.\n"
            "Prevent mythfilldatabase from pulling information for "
            "tomorrow's listings. Data for tomorrow is always pulled "
            "unless specifically specified otherwise.\n"
            "If being used with datadirect, this option should not be "
            "used, rather use --dd-grab-all to pull all listings each time.");
    add("--refresh-second", "refreshsecond", false, "",
            "This option is only valid for selected grabbers.\n"
            "Force a refresh for guide data two days from now. This can "
            "be used in combination with other --refresh-<n> options.\n"
            "If being used with datadirect, this option should not be "
            "used, rather use --dd-grab-all to pull all listings each time.");
    add("--refresh-day", "refreshday", 0U, "",
            "This option is only valid for selected grabbers.\n"
            "Force a refresh for guide data on a specific day. This can "
            "be used in combination with other --refresh-<n> options.\n"
            "If being used with datadirect, this option should not be "
            "used, rather use --dd-grab-all to pull all listings each time.");
    add("--dont-refresh-tba", "dontrefreshtba", false,
            "don't refresh \"To be announced\" programs",
            "This option is only valid for selected grabbers.\n"
            "Prevent mythfilldatabase from automatically refreshing any "
            "programs marked as \"To be announced\".\n"
            "If being used with datadirect, this option should not be "
            "used, rather use --dd-grab-all to pull all listings each time.");

    add("--refresh-all", "refreshall", false, "",
            "This option is only valid for selected grabbers.\n"
            "This option forces a refresh of all guide data, but does so "
            "with fourteen downloads of one day each.\n"
            "If being used with datadirect, this option should not be "
            "used, rather use --dd-grab-all to pull all listings each time.")
        ->SetBlocks("dontrefreshtomorrow")
        ->SetBlocks("refreshsecond")
        ->SetBlocks("refreshday")
        ->SetBlocks("maxdays");

    add("--dd-grab-all", "ddgraball", false,
            "refresh full data using DataDirect",
            "This option is only valid for selected grabbers (DataDirect).\n"
            "This option is the preferred way of updating guide data from "
            "DataDirect, and pulls all fourteen days of guide data at once.")
        ->SetBlocks("refreshtoday")
        ->SetBlocks("dontrefreshtomorrow")
        ->SetBlocks("refreshsecond")
        ->SetBlocks("refreshall")
        ->SetBlocks("refreshday")
        ->SetBlocks("dontrefreshtba")
        ->SetBlocks("maxdays");

    add("--only-update-channels", "onlychannels", false,
            "only update channel lineup",
            "Download as little listings data as possible to update the "
            "channel lineup.");
    add("--no-mark-repeats", "markrepeats", true, "do not mark repeats", "");
    add("--export-icon-map", "exporticonmap", "iconmap.xml",
            "export icon map to file", "");
    add("--import-icon-map", "importiconmap", "iconmap.xml",
            "import icon map to file", "");
    add("--update-icon-map", "updateiconmap", false,
            "updates icon map icons", "");
    add("--reset-icon-map", "reseticonmap", "", "resets icon maps",
            "Reset all icon maps. If given 'all' as an optional value, reset "
            "channel icons as well.");
}
