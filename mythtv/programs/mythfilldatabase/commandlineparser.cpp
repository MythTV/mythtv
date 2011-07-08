
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
            "future use."
            "mutually exclusive with --update");
    add("--update", "update", false, "Run non-destructive updates",
            "Run non-destructive updates on the database for "
            "users in xmltv zones that do not provide channel "
            "data. Stops the addition of new channels and the "
            "changing of channel icons."
            "mutually exclusive with --manual");
    add("--preset", "preset", false, "Use channel preset values instead of numbers",
            "For use with assigning preset numbers for each "
            "channel. Useful for non-US countries where people "
            "are used to assigning a sequenced number for each "
            "channel:\n1->TVE1(S41), 2->La 2(SE18), 3->TV(21)...");
    add("--file", "file", false, "Bypass grabbers and define sourceid and file",
            "Directly define the sourceid and XMLTV file to "
            "import. Must be used in combination with:"
            " --sourceid  --xmlfile");
    add("--dd-file", "file", false, "Bypass grabber, and read SD data from file",
            "Directly define the data needed to import a local "
            "DataDirect download. Must be used in combination "
            "with: \n"
            " --sourceid  --lineupid  --offset  --xmlfile");
    add("--sourceid", "sourceid", 0, "Operate on single source",
            "Limit mythfilldatabase to only operate on the "
            "specified channel source. This option is required "
            "when using --file, --dd-file, or --xawchannels.");
    add("--offset", "offset", 0, "Day offset of input xml file",
            "Specify how many days offset from today is the "
            "information in the given XML file. This option is "
            "required when using --dd-file.");
    add("--lineupid", "lineupid", 0, "DataDirect lineup of input xml file",
            "Specify the DataDirect lineup that corresponds to "
            "the information in the given XML file. This option "
            "is required when using --dd-file.");
    add("--xmlfile", "xmlfile", "", "XML file to import manually",
            "Specify an XML guide data file to import directly "
            "rather than pull data through the specified grabber.\n"
            "This option is required when using --file or --dd-file.");
    add("--xawchannels", "xawchannels", false, "Read channels from xawtvrc file",
            "Import channels from an xawtvrc file.\nThis option "
            "requires --sourceid and --xawtvrcfile.");
    add("--xawtvrcfile", "xawtvrcfile", "", "xawtvrc file to import channels from",
            "Xawtvrc file containing channels to be imported.\n"
            "This option is required when using --xawchannels.");
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
            "imported and thus are disabled automatically.");
    add("--do-not-filter-new-channels", "nofilterchannels", false,
            "don't filter ATSC channels for addition",
            "Normally, MythTV tries to avoid adding ATSC "
            "channels to NTSC channel lineups. This option "
            "restores the behavior of adding every channel in "
            "the downloaded channel lineup to MythTV's lineup, "
            "in case MythTV's smarts fail you.");
//    add("--graboptions", "graboptions", "", "",
//            "Manually define options to pass to the data grabber. "
//            "Do NOT use this option unless you know what you are "
//            "doing. Mythfilldatabase will automatically use the "
//            "correct options for xmltv compliant grabbers.");
    add("--cardtype", "cardtype", "", "", "No information.");           // need documentation for this one
    add("--max-days", "maxdays", 0, "force number of days to update",
            "Force the maximum number of days, counting today, "
            "for the guide data grabber to check for future "
            "listings.");
    add("--refresh-today", "refreshtoday", false, "refresh today's listings",
            "This option is only valid for selected grabbers.\n"
            "Force a refresh for today's guide data.\nThis can be used "
            "in combination with other --refresh-<n> options.\n"
            "If being used with datadirect, this option should not be "
            "used, rather use --dd-grab-all to pull all listings each time.");
    add("--dont-refresh-tomorrow", "dontrefreshtomorrow", false,
            "don't refresh tomorrow's listings",
            "This option is only valid for selected grabbers.\n"
            "Prevent mythfilldatabase from pulling information for "
            "tomorrow's listings. Data for tomorrow is always pulled "
            "unless specifically specified otherwise.\n"
            "If being used with datadirect, this option should not be "
            "used, rather use --dd-grab-all to pull all listings each time.");
    add("--refresh-second", "refreshsecond", false, "refresh listings two days from now",
            "This option is only valid for selected grabbers.\n"
            "Force a refresh for guide data two days from now. This can "
            "be used in combination with other --refresh-<n> options.\n"
            "If being used with datadirect, this option should not be "
            "used, rather use --dd-grab-all to pull all listings each time.");
    add("--refresh-all", "refreshall", false, "refresh listings on all days",
            "This option is only valid for selected grabbers.\n"
            "This option forces a refresh of all guide data, but does so "
            "with fourteen downloads of one day each.\n"
            "If being used with datadirect, this option should not be "
            "used, rather use --dd-grab-all to pull all listings each time.");
// TODO: I should be converted to a qstringlist and used in place of
//       the other refresh options
    add("--refresh-day", "refreshday", 0U, "refresh specific day's listings",
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
    add("--dd-grab-all", "ddgraball", false, "refresh full data using DataDirect",
            "This option is only valid for selected grabbers (DataDirect).\n"
            "This option is the preferred way of updating guide data from "
            "DataDirect, and pulls all fourteen days of guide data at once.");
    add("--only-update-channels", "onlychannels", false, "only update channel lineup",
            "Download as little listings data as possible to update the "
            "channel lineup.");
    add("--no-mark-repeats", "markrepeats", true, "do not mark repeats", "");
    add("--export-icon-map", "exporticonmap", "iconmap.xml",
            "export icon map to file", "");
    add("--import-icon-map", "importiconmap", "iconmap.xml",
            "import icon map to file", "");
    add("--update-icon-map", "updateiconmap", false, "updates icon map icons", "");
    add("--reset-icon-map", "reseticonmap", "", "resets icon maps",
            "Reset all icon maps. If given 'all' as an optional value, reset "
            "channel icons as well.");
}
