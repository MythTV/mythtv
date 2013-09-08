<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE TS>
<TS version="2.0" language="en_GB">
<context>
    <name>(ArchiveUtils)</name>
    <message>
        <location filename="../mytharchive/archiveutil.cpp" line="80"/>
        <source>Cannot find the MythArchive work directory.
Have you set the correct path in the settings?</source>
        <translation>Cannot find the MythArchive work directory.
Have you set the correct path in the settings?</translation>
    </message>
</context>
<context>
    <name>(MythArchiveMain)</name>
    <message>
        <location filename="../mytharchive/main.cpp" line="93"/>
        <source>Found a lock file but the owning process isn&apos;t running!
Removing stale lock file.</source>
        <translation>Found a lock file but the owning process isn&apos;t running!
Removing stale lock file.</translation>
    </message>
    <message>
        <location filename="../mytharchive/main.cpp" line="212"/>
        <source>Last run did not create a playable DVD.</source>
        <translation>Last run did not create a playable DVD.</translation>
    </message>
    <message>
        <location filename="../mytharchive/main.cpp" line="219"/>
        <source>Last run failed to create a DVD.</source>
        <translation>Last run failed to create a DVD.</translation>
    </message>
</context>
<context>
    <name>ArchiveFileSelector</name>
    <message>
        <location filename="../mytharchive/importnative.cpp" line="212"/>
        <source>Find File To Import</source>
        <translation>Find File To Import</translation>
    </message>
    <message>
        <location filename="../mytharchive/importnative.cpp" line="272"/>
        <source>The selected item is not a valid archive file!</source>
        <translation>The selected item is not a valid archive file!</translation>
    </message>
</context>
<context>
    <name>ArchiveSettings</name>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="20"/>
        <source>MythArchive Temp Directory</source>
        <translation>MythArchive Temp Directory</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="23"/>
        <source>Location where MythArchive should create its temporary work files. LOTS of free space required here.</source>
        <translation>Location where MythArchive should create its temporary work files. LOTS of free space required here.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="33"/>
        <source>MythArchive Share Directory</source>
        <translation>MythArchive Share Directory</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="36"/>
        <source>Location where MythArchive stores its scripts, intro movies and theme files</source>
        <translation>Location where MythArchive stores its scripts, intro movies and theme files</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="46"/>
        <source>Video format</source>
        <translation>Video format</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="51"/>
        <source>Video format for DVD recordings, PAL or NTSC.</source>
        <translation>Video format for DVD recordings, PAL or NTSC.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="60"/>
        <source>File Selector Filter</source>
        <translation>File Selector Filter</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="63"/>
        <source>The file name filter to use in the file selector.</source>
        <translation>The file name filter to use in the file selector.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="72"/>
        <source>Location of DVD</source>
        <translation>Location of DVD</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="75"/>
        <source>Which DVD drive to use when burning discs.</source>
        <translation>Which DVD drive to use when burning discs.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="84"/>
        <source>DVD Drive Write Speed</source>
        <translation>DVD Drive Write Speed</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="87"/>
        <source>This is the write speed to use when burning a DVD. Set to 0 to allow growisofs to choose the fastest available speed.</source>
        <translation>This is the write speed to use when burning a DVD. Set to 0 to allow growisofs to choose the fastest available speed.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="98"/>
        <source>Command to play DVD</source>
        <translation>Command to play DVD</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="101"/>
        <source>Command to run when test playing a created DVD. &apos;Internal&apos; will use the internal MythTV player. %f will be replaced with the path to the created DVD structure eg. &apos;xine -pfhq --no-splash dvd:/%f&apos;.</source>
        <translation>Command to run when test playing a created DVD. &apos;Internal&apos; will use the internal MythTV player. %f will be replaced with the path to the created DVD structure eg. &apos;xine -pfhq --no-splash dvd:/%f&apos;.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="114"/>
        <source>Copy remote files</source>
        <translation>Copy remote files</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="117"/>
        <source>If set files on remote filesystems will be copied over to the local filesystem before processing. Speeds processing and reduces bandwidth on the network</source>
        <translation>If set files on remote filesystems will be copied over to the local filesystem before processing. Speeds processing and reduces bandwidth on the network</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="129"/>
        <source>Always Use Mythtranscode</source>
        <translation>Always Use Mythtranscode</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="132"/>
        <source>If set mpeg2 files will always be passed though mythtranscode to clean up any errors. May help to fix some audio problems. Ignored if &apos;Use ProjectX&apos; is set.</source>
        <translation>If set, MPEG-2 files will always be passed through mythtranscode to clean up any errors. May help to fix some audio problems. Ignored if &apos;Use ProjectX&apos; is set.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="144"/>
        <source>Use ProjectX</source>
        <translation>Use ProjectX</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="147"/>
        <source>If set ProjectX will be used to cut commercials and split mpeg2 files instead of mythtranscode and mythreplex.</source>
        <translation>If set ProjectX will be used to cut adverts and split MPEG-2 files instead of mythtranscode and mythreplex.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="158"/>
        <source>Use FIFOs</source>
        <translation>Use FIFOs</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="161"/>
        <source>The script will use FIFOs to pass the output of mplex into dvdauthor rather than creating intermediate files. Saves time and disk space during multiplex operations but not supported on Windows platform</source>
        <translation>The script will use FIFOs to pass the output of mplex into dvdauthor rather than creating intermediate files. Saves time and disk space during multiplex operations but not supported on Windows platform</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="174"/>
        <source>Add Subtitles</source>
        <translation>Add Subtitles</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="177"/>
        <source>If available this option will add subtitles to the final DVD. Requires &apos;Use ProjectX&apos; to be on.</source>
        <translation>If available this option will add subtitles to the final DVD. Requires &apos;Use ProjectX&apos; to be enabled.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="187"/>
        <source>Main Menu Aspect Ratio</source>
        <translation>Main Menu Aspect Ratio</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="189"/>
        <location filename="../mytharchive/archivesettings.cpp" line="205"/>
        <source>4:3</source>
        <comment>Aspect ratio</comment>
        <translation>4:3</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="190"/>
        <location filename="../mytharchive/archivesettings.cpp" line="206"/>
        <source>16:9</source>
        <comment>Aspect ratio</comment>
        <translation>16:9</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="194"/>
        <source>Aspect ratio to use when creating the main menu.</source>
        <translation>Aspect ratio to use when creating the main menu.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="203"/>
        <source>Chapter Menu Aspect Ratio</source>
        <translation>Chapter Menu Aspect Ratio</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="207"/>
        <location filename="../mytharchive/archivesettings.cpp" line="216"/>
        <source>Video</source>
        <translation>Video</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="212"/>
        <source>Aspect ratio to use when creating the chapter menu. &apos;%1&apos; means use the same aspect ratio as the associated video.</source>
        <extracomment>%1 is the translation of the &quot;Video&quot; combo box choice</extracomment>
        <translation>Aspect ratio to use when creating the chapter menu. &apos;%1&apos; means use the same aspect ratio as the associated video.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="223"/>
        <source>Date format</source>
        <translation>Date format</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="226"/>
        <source>Samples are shown using today&apos;s date.</source>
        <translation>Samples are shown using today&apos;s date.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="232"/>
        <source>Samples are shown using tomorrow&apos;s date.</source>
        <translation>Samples are shown using tomorrow&apos;s date.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="250"/>
        <source>Your preferred date format to use on DVD menus. %1</source>
        <extracomment>%1 gives additional info on the date used</extracomment>
        <translation>Your preferred date format to use on DVD menus. %1</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="259"/>
        <source>Time format</source>
        <translation>Time format</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="266"/>
        <source>Your preferred time format to display on DVD menus. You must choose a format with &quot;AM&quot; or &quot;PM&quot; in it, otherwise your time display will be 24-hour or &quot;military&quot; time.</source>
        <translation>Your preferred time format to display on DVD menus. You must choose a format with &quot;AM&quot; or &quot;PM&quot; in it, otherwise your time display will be 24-hour or &quot;military&quot; time.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="277"/>
        <source>Default Encoder Profile</source>
        <translation>Default Encoder Profile</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="279"/>
        <source>HQ</source>
        <comment>Encoder profile</comment>
        <translation>HQ</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="280"/>
        <source>SP</source>
        <comment>Encoder profile</comment>
        <translation>SP</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="281"/>
        <source>LP</source>
        <comment>Encoder profile</comment>
        <translation>LP</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="282"/>
        <source>EP</source>
        <comment>Encoder profile</comment>
        <translation>EP</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="286"/>
        <source>Default encoding profile to use if a file needs re-encoding.</source>
        <translation>Default encoding profile to use if a file needs re-encoding.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="295"/>
        <source>mplex Command</source>
        <translation>mplex command</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="299"/>
        <source>Command to run mplex</source>
        <translation>Command to run mplex</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="308"/>
        <source>dvdauthor command</source>
        <translation>dvdauthor command</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="312"/>
        <source>Command to run dvdauthor.</source>
        <translation>Command to run dvdauthor.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="321"/>
        <source>mkisofs command</source>
        <translation>mkisofs command</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="325"/>
        <source>Command to run mkisofs. (Used to create ISO images)</source>
        <translation>Command to run mkisofs. (Used to create ISO images)</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="334"/>
        <source>growisofs command</source>
        <translation>growisofs command</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="338"/>
        <source>Command to run growisofs. (Used to burn DVDs)</source>
        <translation>Command to run growisofs. (Used to burn DVDs)</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="347"/>
        <source>M2VRequantiser command</source>
        <translation>M2VRequantiser command</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="351"/>
        <source>Command to run M2VRequantiser. Optional - leave blank if you don&apos;t have M2VRequantiser installed.</source>
        <translation>Command to run M2VRequantiser. Optional - leave blank if you don&apos;t have M2VRequantiser installed.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="361"/>
        <source>jpeg2yuv command</source>
        <translation>jpeg2yuv command</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="365"/>
        <source>Command to run jpeg2yuv. Part of mjpegtools package</source>
        <translation>Command to run jpeg2yuv. Part of mjpegtools package</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="374"/>
        <source>spumux command</source>
        <translation>spumux command</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="378"/>
        <source>Command to run spumux. Part of dvdauthor package</source>
        <translation>Command to run spumux. Part of dvdauthor package</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="387"/>
        <source>mpeg2enc command</source>
        <translation>mpeg2enc command</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="391"/>
        <source>Command to run mpeg2enc. Part of mjpegtools package</source>
        <translation>Command to run mpeg2enc. Part of mjpegtools package</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="400"/>
        <source>projectx command</source>
        <translation>projectx command</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="404"/>
        <source>Command to run ProjectX. Will be used to cut commercials and split mpegs files instead of mythtranscode and mythreplex.</source>
        <translation>Command to run ProjectX. Will be used to cut adverts and split MPEG-2 files instead of mythtranscode and mythreplex.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="414"/>
        <source>MythArchive Settings</source>
        <translation>MythArchive Settings</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="425"/>
        <source>MythArchive Settings (2)</source>
        <translation>MythArchive Settings (2)</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="435"/>
        <source>DVD Menu Settings</source>
        <translation>DVD Menu Settings</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="443"/>
        <source>MythArchive External Commands (1)</source>
        <translation>MythArchive External Commands (1)</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="451"/>
        <source>MythArchive External Commands (2)</source>
        <translation>MythArchive External Commands (2)</translation>
    </message>
</context>
<context>
    <name>BurnMenu</name>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="1155"/>
        <source>Cannot burn a DVD.
The last run failed to create a DVD.</source>
        <translation>Cannot burn a DVD.
The last run failed to create a DVD.</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="1161"/>
        <location filename="../mytharchive/mythburn.cpp" line="1173"/>
        <source>Burn DVD</source>
        <translation>Burn DVD</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="1162"/>
        <source>
Place a blank DVD in the drive and select an option below.</source>
        <translation>
Place a blank DVD in the drive and select an option below.</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="1174"/>
        <source>Burn DVD Rewritable</source>
        <translation>Burn DVD Rewritable</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="1175"/>
        <source>Burn DVD Rewritable (Force Erase)</source>
        <translation>Burn DVD Rewritable (Force Erase)</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="1233"/>
        <source>It was not possible to run mytharchivehelper to burn the DVD.</source>
        <translation>It was not possible to run mytharchivehelper to burn the DVD.</translation>
    </message>
</context>
<context>
    <name>BurnThemeUI</name>
    <message>
        <location filename="../mythburn/themes/burnthemestrings.h" line="5"/>
        <source>Has an intro and contains a main menu with 4 recordings per page. Does not have a chapter selection submenu.</source>
        <translation>Has an intro and contains a main menu with 4 recordings per page. Does not have a chapter selection submenu.</translation>
    </message>
    <message>
        <location filename="../mythburn/themes/burnthemestrings.h" line="6"/>
        <source>Has an intro and contains a summary main menu with 10 recordings per page. Does not have a chapter selection submenu, recording titles, dates or category.</source>
        <translation>Has an intro and contains a summary main menu with 10 recordings per page. Does not have a chapter selection submenu, recording titles, dates or category.</translation>
    </message>
    <message>
        <location filename="../mythburn/themes/burnthemestrings.h" line="7"/>
        <source>Has an intro and contains a main menu with 6 recordings per page. Does not have a scene selection submenu.</source>
        <translation>Has an intro and contains a main menu with 6 recordings per page. Does not have a scene selection submenu.</translation>
    </message>
    <message>
        <location filename="../mythburn/themes/burnthemestrings.h" line="8"/>
        <source>Has an intro and contains a main menu with 3 recordings per page and a scene selection submenu with 8 chapters points. Shows a program details page before each recording.</source>
        <translation>Has an intro and contains a main menu with 3 recordings per page and a scene selection submenu with 8 chapters points. Shows a programme details page before each recording.</translation>
    </message>
    <message>
        <location filename="../mythburn/themes/burnthemestrings.h" line="9"/>
        <source>Has an intro and contains a main menu with 3 recordings per page and a scene selection submenu with 8 chapters points. Shows a program details page before each recording. Uses animated thumb images.</source>
        <translation>Has an intro and contains a main menu with 3 recordings per page and a scene selection submenu with 8 chapters points. Shows a programme details page before each recording. Uses animated thumb images.</translation>
    </message>
    <message>
        <location filename="../mythburn/themes/burnthemestrings.h" line="10"/>
        <source>Has an intro and contains a main menu with 3 recordings per page and a scene selection submenu with 8 chapters points.</source>
        <translation>Has an intro and contains a main menu with 3 recordings per page and a scene selection submenu with 8 chapters points.</translation>
    </message>
    <message>
        <location filename="../mythburn/themes/burnthemestrings.h" line="11"/>
        <source>Has an intro and contains a main menu with 3 recordings per page and a scene selection submenu with 8 chapters points. All the thumb images are animated.</source>
        <translation>Has an intro and contains a main menu with 3 recordings per page and a scene selection submenu with 8 chapters points. All the thumb images are animated.</translation>
    </message>
    <message>
        <location filename="../mythburn/themes/burnthemestrings.h" line="12"/>
        <source>Creates an auto play DVD with no menus. Shows an intro movie then for each title shows a details page followed by the video in sequence.</source>
        <translation>Creates an auto play DVD with no menus. Shows an intro movie then for each title shows a details page followed by the video in sequence.</translation>
    </message>
    <message>
        <location filename="../mythburn/themes/burnthemestrings.h" line="13"/>
        <source>Creates an auto play DVD with no menus and no intro.</source>
        <translation>Creates an auto play DVD with no menus and no intro.</translation>
    </message>
</context>
<context>
    <name>DVDThemeSelector</name>
    <message>
        <location filename="../mytharchive/themeselector.cpp" line="219"/>
        <location filename="../mytharchive/themeselector.cpp" line="230"/>
        <source>No theme description file found!</source>
        <translation>No theme description file found!</translation>
    </message>
    <message>
        <location filename="../mytharchive/themeselector.cpp" line="243"/>
        <source>Empty theme description!</source>
        <translation>Empty theme description!</translation>
    </message>
    <message>
        <location filename="../mytharchive/themeselector.cpp" line="248"/>
        <source>Unable to open theme description file!</source>
        <translation>Unable to open theme description file!</translation>
    </message>
</context>
<context>
    <name>ExportNative</name>
    <message>
        <location filename="../mytharchive/exportnative.cpp" line="242"/>
        <source>You need to add at least one item to archive!</source>
        <translation>You need to add at least one item to archive!</translation>
    </message>
    <message>
        <location filename="../mytharchive/exportnative.cpp" line="393"/>
        <source>Menu</source>
        <translation>Menu</translation>
    </message>
    <message>
        <location filename="../mytharchive/exportnative.cpp" line="400"/>
        <source>Remove Item</source>
        <translation>Remove Item</translation>
    </message>
    <message>
        <location filename="../mytharchive/exportnative.cpp" line="495"/>
        <source>It was not possible to create the DVD. An error occured when running the scripts</source>
        <translation>It was not possible to create the DVD. An error occured when running the scripts</translation>
    </message>
    <message>
        <location filename="../mytharchive/exportnative.cpp" line="531"/>
        <source>You don&apos;t have any videos!</source>
        <translation>You don&apos;t have any videos!</translation>
    </message>
</context>
<context>
    <name>FileSelector</name>
    <message>
        <location filename="../mytharchive/fileselector.cpp" line="81"/>
        <source>Find File</source>
        <translation>Find File</translation>
    </message>
    <message>
        <location filename="../mytharchive/fileselector.cpp" line="84"/>
        <source>Find Directory</source>
        <translation>Find Directory</translation>
    </message>
    <message>
        <location filename="../mytharchive/fileselector.cpp" line="87"/>
        <source>Find Files</source>
        <translation>Find Files</translation>
    </message>
    <message>
        <location filename="../mytharchive/fileselector.cpp" line="321"/>
        <source>The selected item is not a directory!</source>
        <translation>The selected item is not a directory!</translation>
    </message>
</context>
<context>
    <name>ImportNative</name>
    <message>
        <location filename="../mytharchive/importnative.cpp" line="433"/>
        <source>You need to select a valid channel id!</source>
        <translation>You need to select a valid channel ID!</translation>
    </message>
    <message>
        <location filename="../mytharchive/importnative.cpp" line="464"/>
        <source>It was not possible to import the Archive.  An error occured when running &apos;mytharchivehelper&apos;</source>
        <translation>It was not possible to import the Archive.  An error occured when running &apos;mytharchivehelper&apos;</translation>
    </message>
    <message>
        <location filename="../mytharchive/importnative.cpp" line="594"/>
        <source>Select a channel id</source>
        <translation>Select a channel ID</translation>
    </message>
    <message>
        <location filename="../mytharchive/importnative.cpp" line="620"/>
        <source>Select a channel number</source>
        <translation>Select a channel number</translation>
    </message>
    <message>
        <location filename="../mytharchive/importnative.cpp" line="646"/>
        <source>Select a channel name</source>
        <translation>Select a channel name</translation>
    </message>
    <message>
        <location filename="../mytharchive/importnative.cpp" line="672"/>
        <source>Select a Callsign</source>
        <translation>Select a Callsign</translation>
    </message>
</context>
<context>
    <name>LogViewer</name>
    <message>
        <location filename="../mytharchive/logviewer.cpp" line="77"/>
        <source>Cannot find any logs to show!</source>
        <translation>Cannot find any logs to show!</translation>
    </message>
    <message>
        <location filename="../mytharchive/logviewer.cpp" line="216"/>
        <source>Background creation has been asked to stop.
This may take a few minutes.</source>
        <translation>Background creation has been asked to stop.
This may take a few minutes.</translation>
    </message>
    <message>
        <location filename="../mytharchive/logviewer.cpp" line="355"/>
        <source>Menu</source>
        <translation>Menu</translation>
    </message>
    <message>
        <location filename="../mytharchive/logviewer.cpp" line="363"/>
        <source>Don&apos;t Auto Update</source>
        <translation>Don&apos;t Auto Update</translation>
    </message>
    <message>
        <location filename="../mytharchive/logviewer.cpp" line="365"/>
        <source>Auto Update</source>
        <translation>Auto Update</translation>
    </message>
    <message>
        <location filename="../mytharchive/logviewer.cpp" line="367"/>
        <source>Show Progress Log</source>
        <translation>Show Progress Log</translation>
    </message>
    <message>
        <location filename="../mytharchive/logviewer.cpp" line="368"/>
        <source>Show Full Log</source>
        <translation>Show Full Log</translation>
    </message>
</context>
<context>
    <name>MythBurn</name>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="356"/>
        <location filename="../mytharchive/mythburn.cpp" line="480"/>
        <source>No Cut List</source>
        <translation>No Cut List</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="367"/>
        <source>You need to add at least one item to archive!</source>
        <translation>You need to add at least one item to archive!</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="413"/>
        <source>Retrieving File Information. Please Wait...</source>
        <translation>Retrieving File Information. Please Wait...</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="483"/>
        <source>Encoder: </source>
        <translation>Encoder: </translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="809"/>
        <source>Menu</source>
        <translation>Menu</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="345"/>
        <location filename="../mytharchive/mythburn.cpp" line="469"/>
        <source>Using Cut List</source>
        <translation>Using Cut List</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="350"/>
        <location filename="../mytharchive/mythburn.cpp" line="474"/>
        <source>Not Using Cut List</source>
        <translation>Not Using Cut List</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="820"/>
        <source>Don&apos;t Use Cut List</source>
        <translation>Don&apos;t Use Cut List</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="823"/>
        <source>Use Cut List</source>
        <translation>Use Cut List</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="827"/>
        <source>Remove Item</source>
        <translation>Remove Item</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="828"/>
        <source>Edit Details</source>
        <translation>Edit Details</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="829"/>
        <source>Change Encoding Profile</source>
        <translation>Change Encoding Profile</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="830"/>
        <source>Edit Thumbnails</source>
        <translation>Edit Thumbnails</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="965"/>
        <source>It was not possible to create the DVD.  An error occured when running the scripts</source>
        <translation>It was not possible to create the DVD.  An error occured when running the scripts</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="1008"/>
        <source>You don&apos;t have any videos!</source>
        <translation>You don&apos;t have any videos!</translation>
    </message>
</context>
<context>
    <name>MythControls</name>
    <message>
        <location filename="../mytharchive/main.cpp" line="302"/>
        <source>Toggle use cut list state for selected program</source>
        <translation>Toggle use cut list state for selected programme</translation>
    </message>
    <message>
        <location filename="../mytharchive/main.cpp" line="305"/>
        <source>Create DVD</source>
        <translation>Create DVD</translation>
    </message>
    <message>
        <location filename="../mytharchive/main.cpp" line="307"/>
        <source>Create Archive</source>
        <translation>Create Archive</translation>
    </message>
    <message>
        <location filename="../mytharchive/main.cpp" line="309"/>
        <source>Import Archive</source>
        <translation>Import Archive</translation>
    </message>
    <message>
        <location filename="../mytharchive/main.cpp" line="311"/>
        <source>View Archive Log</source>
        <translation>View Archive Log</translation>
    </message>
    <message>
        <location filename="../mytharchive/main.cpp" line="313"/>
        <source>Play Created DVD</source>
        <translation>Play Created DVD</translation>
    </message>
    <message>
        <location filename="../mytharchive/main.cpp" line="315"/>
        <source>Burn DVD</source>
        <translation>Burn DVD</translation>
    </message>
</context>
<context>
    <name>RecordingSelector</name>
    <message>
        <location filename="../mytharchive/recordingselector.cpp" line="112"/>
        <location filename="../mytharchive/recordingselector.cpp" line="420"/>
        <location filename="../mytharchive/recordingselector.cpp" line="525"/>
        <source>All Recordings</source>
        <translation>All Recordings</translation>
    </message>
    <message>
        <location filename="../mytharchive/recordingselector.cpp" line="133"/>
        <source>Retrieving Recording List.
Please Wait...</source>
        <translation>Retrieving Recording List.
Please Wait...</translation>
    </message>
    <message>
        <location filename="../mytharchive/recordingselector.cpp" line="157"/>
        <source>Either you don&apos;t have any recordings or no recordings are available locally!</source>
        <translation>Either you don&apos;t have any recordings or no recordings are available locally!</translation>
    </message>
    <message>
        <location filename="../mytharchive/recordingselector.cpp" line="206"/>
        <source>Menu</source>
        <translation>Menu</translation>
    </message>
    <message>
        <location filename="../mytharchive/recordingselector.cpp" line="213"/>
        <source>Clear All</source>
        <translation>Clear All</translation>
    </message>
    <message>
        <location filename="../mytharchive/recordingselector.cpp" line="214"/>
        <source>Select All</source>
        <translation>Select All</translation>
    </message>
</context>
<context>
    <name>SelectDestination</name>
    <message>
        <location filename="../mytharchive/archiveutil.cpp" line="35"/>
        <source>Single Layer DVD</source>
        <translation>Single Layer DVD</translation>
    </message>
    <message>
        <location filename="../mytharchive/archiveutil.cpp" line="39"/>
        <source>Dual Layer DVD</source>
        <translation>Dual Layer DVD</translation>
    </message>
    <message>
        <location filename="../mytharchive/archiveutil.cpp" line="36"/>
        <source>Single Layer DVD (4,482 MB)</source>
        <translation>Single Layer DVD (4,482 MB)</translation>
    </message>
    <message>
        <location filename="../mytharchive/archiveutil.cpp" line="40"/>
        <source>Dual Layer DVD (8,964 MB)</source>
        <translation>Dual Layer DVD (8,964 MB)</translation>
    </message>
    <message>
        <location filename="../mytharchive/archiveutil.cpp" line="43"/>
        <source>DVD +/- RW</source>
        <translation>DVD +/- RW</translation>
    </message>
    <message>
        <location filename="../mytharchive/archiveutil.cpp" line="44"/>
        <source>Rewritable DVD</source>
        <translation>Rewritable DVD</translation>
    </message>
    <message>
        <location filename="../mytharchive/archiveutil.cpp" line="47"/>
        <source>File</source>
        <translation>File</translation>
    </message>
    <message>
        <location filename="../mytharchive/archiveutil.cpp" line="48"/>
        <source>Any file accessable from your filesystem.</source>
        <translation>Any file accessable from your filesystem.</translation>
    </message>
    <message>
        <location filename="../mytharchive/selectdestination.cpp" line="294"/>
        <location filename="../mytharchive/selectdestination.cpp" line="350"/>
        <source>Unknown</source>
        <translation>Unknown</translation>
    </message>
</context>
<context>
    <name>ThemeUI</name>
    <message>
        <location filename="themestrings.h" line="5"/>
        <source>A high quality profile giving approx. 1 hour of video on a single layer DVD</source>
        <translation>A high quality profile giving approx. 1 hour of video on a single layer DVD</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="6"/>
        <source>A standard play profile giving approx. 2 hour of video on a single layer DVD</source>
        <translation>A standard play profile giving approx. 2 hour of video on a single layer DVD</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="7"/>
        <source>A long play profile giving approx. 4 hour of video on a single layer DVD</source>
        <translation>A long play profile giving approx. 4 hour of video on a single layer DVD</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="8"/>
        <source>A extended play profile giving approx. 6 hour of video on a single layer DVD</source>
        <translation>A extended play profile giving approx. 6 hour of video on a single layer DVD</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="70"/>
        <source>%DATE%, %TIME%</source>
        <translation>%DATE%, %TIME%</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="18"/>
        <source>Select Destination</source>
        <translation>Select Destination</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="71"/>
        <source>Choose where you would like your files archived.</source>
        <translation>Choose where you would like your files archived.</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="72"/>
        <source>Output Type:</source>
        <translation>Output Type:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="73"/>
        <source>Destination:</source>
        <translation>Destination:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="20"/>
        <source>Free Space:</source>
        <translation>Free Space:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="74"/>
        <source>Click here to find an output location...</source>
        <translation>Click here to find an output location...</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="21"/>
        <source>Make ISO Image</source>
        <translation>Make ISO Image</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="22"/>
        <source>Burn to DVD</source>
        <translation>Burn to DVD</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="75"/>
        <source>Erase DVD-RW before burning</source>
        <translation>Erase DVD-RW before burning</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="15"/>
        <source>Cancel</source>
        <translation>Cancel</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="76"/>
        <source>Previous</source>
        <translation>Previous</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="25"/>
        <source>Next</source>
        <translation>Next</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="77"/>
        <source>Filter:</source>
        <translation>Filter:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="28"/>
        <source>OK</source>
        <translation>OK</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="29"/>
        <source>File Finder</source>
        <translation>File Finder</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="78"/>
        <source>Select the file you wish to use.</source>
        <translation>Select the file you wish to use.</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="16"/>
        <source>Back</source>
        <translation>Back</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="30"/>
        <source>Home</source>
        <translation>Home</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="34"/>
        <source>No videos available</source>
        <translation>No videos available</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="35"/>
        <source>Log Viewer</source>
        <translation>Log Viewer</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="79"/>
        <source>See logs from your archive runs.</source>
        <translation>See logs from your archive runs.</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="36"/>
        <source>Update</source>
        <translation>Update</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="37"/>
        <source>Exit</source>
        <translation>Exit</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="85"/>
        <source>Up Level</source>
        <translation>Up Level</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="96"/>
        <source>description goes here.</source>
        <translation>description goes here.</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="86"/>
        <source>0.00 GB</source>
        <translation>0.00 GB</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="23"/>
        <source>Force Overwrite of DVD-RW Media</source>
        <translation>Force Overwrite of DVD-RW Media</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="26"/>
        <source>Select Recordings</source>
        <translation>Select Recordings</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="27"/>
        <source>Show Recordings</source>
        <translation>Show Recordings</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="31"/>
        <source>Select Videos</source>
        <translation>Select Videos</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="97"/>
        <source>Video Category:</source>
        <translation>Video Category:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="102"/>
        <source>sep 13, 2004 11:00 pm (1h 15m)</source>
        <translation>sep 13, 2004 11:00 pm (1h 15m)</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="103"/>
        <source>x.xx GB</source>
        <translation>x.xx GB</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="90"/>
        <source>Ok</source>
        <translation>OK</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="32"/>
        <source>Video Category</source>
        <translation>Video Category</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="98"/>
        <source>title goes here</source>
        <translation>title goes here</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="33"/>
        <source>PL:</source>
        <translation>PL:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="99"/>
        <source>1</source>
        <translation>1</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="104"/>
        <source>x.xx Gb</source>
        <translation>x.xx GB</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="38"/>
        <source>Change Encoding Profile</source>
        <translation>Change Encoding Profile</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="80"/>
        <source>12.34 GB</source>
        <translation>12.34GB</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="39"/>
        <source>DVD Menu Theme</source>
        <translation>DVD Menu Theme</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="81"/>
        <source>Choose the appearance of your DVD.</source>
        <translation>Choose the appearance of your DVD.</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="82"/>
        <source>Theme:</source>
        <translation>Theme:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="41"/>
        <source>Intro</source>
        <translation>Intro</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="42"/>
        <source>Main Menu</source>
        <translation>Main Menu</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="43"/>
        <source>Chapter Menu</source>
        <translation>Chapter Menu</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="44"/>
        <source>Details</source>
        <translation>Details</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="45"/>
        <source>Select Archive Items</source>
        <translation>Select Archive Items</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="83"/>
        <source>Select the recordings and videos you wish to save.</source>
        <translation>Select the recordings and videos you wish to save.</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="9"/>
        <source>No files are selected for archive</source>
        <translation>No files are selected for archive</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="10"/>
        <source>Add Recording</source>
        <translation>Add Recording</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="11"/>
        <source>Add Video</source>
        <translation>Add Video</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="12"/>
        <source>Add File</source>
        <translation>Add File</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="13"/>
        <source>0 mb</source>
        <translation>0 MB</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="14"/>
        <source>xxxxx mb</source>
        <translation>xxxxx MB</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="46"/>
        <source>Archive Item Details</source>
        <translation>Archive Item Details</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="47"/>
        <source>Title:</source>
        <translation>Title:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="48"/>
        <source>Subtitle:</source>
        <translation>Subtitle:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="49"/>
        <source>Start Date:</source>
        <translation>Start Date:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="50"/>
        <source>Time:</source>
        <translation>Time:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="51"/>
        <source>Description:</source>
        <translation>Description:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="52"/>
        <source>Thumb Image Selector</source>
        <translation>Thumb Image Selector</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="53"/>
        <source>Current Position</source>
        <translation>Current Position</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="84"/>
        <source>0:00:00.00</source>
        <translation>0:00:00.00</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="54"/>
        <source>Seek Amount</source>
        <translation>Seek Amount</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="55"/>
        <source>Frame</source>
        <translation>Frame</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="56"/>
        <source>Save</source>
        <translation>Save</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="40"/>
        <source>Select a theme</source>
        <translation>Select a theme</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="100"/>
        <source>xxxxx MB</source>
        <translation>xxxxx MB</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="19"/>
        <source>Find</source>
        <translation>Find</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="101"/>
        <source>0 MB</source>
        <translation>0 MB</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="24"/>
        <source>Prev</source>
        <translation>Prev</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="58"/>
        <source>File Finder To Import</source>
        <translation>File Finder To Import</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="59"/>
        <source>Start Time:</source>
        <translation>Start Time:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="60"/>
        <source>Select Associated Channel</source>
        <translation>Select Associated Channel</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="61"/>
        <source>Archived Channel</source>
        <translation>Archived Channel</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="62"/>
        <source>Chan. ID:</source>
        <translation>Chan. ID:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="63"/>
        <source>Chan. No:</source>
        <translation>Chan. No:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="64"/>
        <source>Callsign:</source>
        <translation>Callsign:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="65"/>
        <source>Name:</source>
        <translation>Name:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="66"/>
        <source>Local Channel</source>
        <translation>Local Channel</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="67"/>
        <source>Search Channel</source>
        <translation>Search Channel</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="68"/>
        <source>Search Callsign</source>
        <translation>Search Callsign</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="69"/>
        <source>Search Name</source>
        <translation>Search Name</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="17"/>
        <source>Finish</source>
        <translation>Finish</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="105"/>
        <source>Select Destination:</source>
        <translation>Select Destination:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="106"/>
        <source>Parental level: %1</source>
        <translation>Parental level: %1</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="107"/>
        <source>Old size:</source>
        <translation>Old size:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="108"/>
        <source>New size:</source>
        <translation>New size:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="109"/>
        <source>Select a theme:</source>
        <translation>Select a theme:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="110"/>
        <source>Menu</source>
        <translation>Menu</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="111"/>
        <source>Chapter</source>
        <translation>Chapter</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="112"/>
        <source>Detail</source>
        <translation>Detail</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="113"/>
        <source>Select File to Import</source>
        <translation>Select File to Import</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="57"/>
        <source>Add video</source>
        <translation>Add video</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="87"/>
        <source>Filesize: %1</source>
        <translation>Filesize: %1</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="88"/>
        <source>Recorded Time: %1</source>
        <translation>Recorded Time: %1</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="89"/>
        <source>%&quot;|SUBTITLE|&quot; %%(|ORIGINALAIRDATE|) %%(|STARS|) %%DESCRIPTION%</source>
        <translation>%&quot;|SUBTITLE|&quot; %%(|ORIGINALAIRDATE|) %%(|STARS|) %%DESCRIPTION%</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="91"/>
        <source>Parental Level: %1</source>
        <translation>Parental Level: %1</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="92"/>
        <source>Archive Items</source>
        <translation>Archive Items</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="93"/>
        <source>Profile:</source>
        <translation>Profile:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="94"/>
        <source>Old Size:</source>
        <translation>Old Size:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="95"/>
        <source>New Size:</source>
        <translation>New Size:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="114"/>
        <source>Channel ID:</source>
        <translation>Channel ID:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="115"/>
        <source>Channel Number:</source>
        <translation>Channel Number:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="116"/>
        <source>Create DVD</source>
        <translation>Create DVD</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="117"/>
        <source>Create Archive</source>
        <translation>Create Archive</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="118"/>
        <source>Encode Video File</source>
        <translation>Encode Video File</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="119"/>
        <source>Import Archive</source>
        <translation>Import Archive</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="120"/>
        <source>Archive Utilities</source>
        <translation>Archive Utilities</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="121"/>
        <source>Show Log Viewer</source>
        <translation>Show Log Viewer</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="122"/>
        <source>Play Created DVD</source>
        <translation>Play Created DVD</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="123"/>
        <source>Burn DVD</source>
        <translation>Burn DVD</translation>
    </message>
</context>
<context>
    <name>ThumbFinder</name>
    <message>
        <location filename="../mytharchive/thumbfinder.cpp" line="921"/>
        <source>Menu</source>
        <translation>Menu</translation>
    </message>
    <message>
        <location filename="../mytharchive/thumbfinder.cpp" line="928"/>
        <source>Exit, Save Thumbnails</source>
        <translation>Exit, Save Thumbnails</translation>
    </message>
    <message>
        <location filename="../mytharchive/thumbfinder.cpp" line="929"/>
        <source>Exit, Don&apos;t Save Thumbnails</source>
        <translation>Exit, Don&apos;t Save Thumbnails</translation>
    </message>
</context>
<context>
    <name>VideoSelector</name>
    <message>
        <location filename="../mytharchive/videoselector.cpp" line="155"/>
        <source>Menu</source>
        <translation>Menu</translation>
    </message>
    <message>
        <location filename="../mytharchive/videoselector.cpp" line="162"/>
        <source>Clear All</source>
        <translation>Clear All</translation>
    </message>
    <message>
        <location filename="../mytharchive/videoselector.cpp" line="163"/>
        <source>Select All</source>
        <translation>Select All</translation>
    </message>
    <message>
        <location filename="../mytharchive/videoselector.cpp" line="368"/>
        <location filename="../mytharchive/videoselector.cpp" line="505"/>
        <source>All Videos</source>
        <translation>All Videos</translation>
    </message>
    <message>
        <location filename="../mytharchive/videoselector.cpp" line="562"/>
        <source>You need to enter a valid password for this parental level</source>
        <translation>You need to enter a valid password for this parental level</translation>
    </message>
</context>
</TS>
