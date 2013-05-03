#include <QString>
#include <QList>

#include "mythcorecontext.h"
#include "commandlineparser.h"

MythMetadataLookupCommandLineParser::MythMetadataLookupCommandLineParser() :
    MythCommandLineParser(MYTH_APPNAME_MYTHMETADATALOOKUP)
{ LoadArguments(); }

void MythMetadataLookupCommandLineParser::LoadArguments(void)
{
    addHelp();
    addVersion();
    addRecording();
    addJob();
    addLogging();

    CommandLineArg::AllowOneOf( QList<CommandLineArg*>()
         << add("--refresh-all", "refresh-all", false,
                "Batch update recorded program metadata. If a recording's "
                "rule has an inetref but the recording does not, it will "
                "be inherited.", "")
         << add("--refresh-all-rules", "refresh-all-rules", false,
                "Batch update metadata for recording rules. This will "
                "set inetrefs for your recording rules based on an automated "
                "lookup. This is a best effort, and not guaranteed! If your "
                "recordings lack inetrefs but one is found for the rule, it "
                "will be inherited.", "")
         << add("--refresh-all-artwork", "refresh-all-artwork", false,
                "Batch update artwork for non-generic recording rules "
                "and recordings. This is a more conservative option "
                "which only operates on items for which it is likely "
                "to be able to find artwork.  This option will not "
                "overwrite any existing artwork.", "")
         << add("--refresh-all-artwork-dangerously",
                "refresh-all-artwork-dangerously", false,
                "Batch update artwork for ALL recording rules and recordings. "
                "This will attempt to download fanart, coverart, and banner "
                "for each season of each recording rule and all recordings. "
                "This option will not overwrite any existing artwork. If a "
                "rule or recording has not been looked up, this will attempt "
                "to look it up. This is a very aggressive option!  Use with "
                "care.", "") 
         << new CommandLineArg("chanid")
         << new CommandLineArg("jobid") );

    add("--refresh-rules", "refresh-rules", false,
        "Also update inetref for recording rules when metadata is "
        "found for a recording (Best effort only, imperfect)", "")
            ->SetBlocks("refresh-all")
            ->SetBlocks("refresh-all-rules")
            ->SetBlocks("refresh-all-artwork")
            ->SetBlocks("refresh-all-artwork-dangerously")
            ->SetChildOf("chanid")
            ->SetChildOf("jobid");
}

