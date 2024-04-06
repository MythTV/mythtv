<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE TS>
<TS version="2.1" language="sv_SE" sourcelanguage="en_US">
<context>
    <name>(ArchiveUtils)</name>
    <message>
        <location filename="../mytharchive/archiveutil.cpp" line="52"/>
        <source>Cannot find the MythArchive work directory.
Have you set the correct path in the settings?</source>
        <translation>Kan inte hitta arbetskatalog för MythArchive.
Har du ställt in rätt sökväg i inställningarna?</translation>
    </message>
</context>
<context>
    <name>(MythArchiveMain)</name>
    <message>
        <location filename="../mytharchive/mytharchive.cpp" line="100"/>
        <source>Found a lock file but the owning process isn&apos;t running!
Removing stale lock file.</source>
        <translation>Hittade en låsfil, men processen som äger den körs inte!
Tar bort föråldrad låsfil.</translation>
    </message>
    <message>
        <location filename="../mytharchive/mytharchive.cpp" line="211"/>
        <source>Last run did not create a playable DVD.</source>
        <translation>Senaste körningen skapade inte en spelbar DVD.</translation>
    </message>
    <message>
        <location filename="../mytharchive/mytharchive.cpp" line="218"/>
        <source>Last run failed to create a DVD.</source>
        <translation>Senaste körningen misslyckades att skapa en DVD.</translation>
    </message>
</context>
<context>
    <name>ArchiveFileSelector</name>
    <message>
        <location filename="../mytharchive/importnative.cpp" line="205"/>
        <source>Find File To Import</source>
        <translation>Välj fil att importera</translation>
    </message>
    <message>
        <location filename="../mytharchive/importnative.cpp" line="265"/>
        <source>The selected item is not a valid archive file!</source>
        <translation>Vald post är inte en giltig arkivfil!</translation>
    </message>
</context>
<context>
    <name>ArchiveSettings</name>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="20"/>
        <source>MythArchive Temp Directory</source>
        <translation>Tillfällig katalog för MythArchive</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="23"/>
        <source>Location where MythArchive should create its temporary work files. LOTS of free space required here.</source>
        <translation>Plats där MythArchive ska skapa tillfälliga arbetsfiler. MYCKET ledigt utrymme krävs här.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="35"/>
        <source>MythArchive Share Directory</source>
        <translation>Delad katalog för MythArchive</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="38"/>
        <source>Location where MythArchive stores its scripts, intro movies and theme files</source>
        <translation>Platsen där MythArchive lagrar sina skript, intro-filmer och temafiler</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="50"/>
        <source>Video format</source>
        <translation>Videoformat</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="55"/>
        <source>Video format for DVD recordings, PAL or NTSC.</source>
        <translation>Videoformat för DVD-inspelningar, PAL eller NTSC.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="64"/>
        <source>File Selector Filter</source>
        <translation>Filvalsfilter</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="67"/>
        <source>The file name filter to use in the file selector.</source>
        <translation>Filnamnsfilter att använda i filväljaren.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="76"/>
        <source>Location of DVD</source>
        <translation>Plats för DVD</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="79"/>
        <source>Which DVD drive to use when burning discs.</source>
        <translation>Vilken DVD-enhet att använda för att bränna skivor.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="89"/>
        <source>DVD Drive Write Speed</source>
        <translation>Skrivhastighet för DVD</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="92"/>
        <source>This is the write speed to use when burning a DVD. Set to 0 to allow growisofs to choose the fastest available speed.</source>
        <translation>Detta är skrivhastigheten som används vid bränning av DVD. Sätt till 0 för att tillåta growisofs för att välja snabbast tillgängliga hastighet.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="103"/>
        <source>Command to play DVD</source>
        <translation>Kommando för att spela DVD</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="106"/>
        <source>Command to run when test playing a created DVD. &apos;Internal&apos; will use the internal MythTV player. %f will be replaced with the path to the created DVD structure eg. &apos;xine -pfhq --no-splash dvd:/%f&apos;.</source>
        <translation>Kommando att köra för att prova uppspelning av en skapad DVD. &apos;Internal&apos; använder den inbyggda spelaren i MythTV. %f ersätts med sökvägen till den skapade DVD-strukturen, t.ex. &apos;xine -pfhq --no-splash dvd:/%f&apos;.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="119"/>
        <source>Copy remote files</source>
        <translation>Kopiera fjärrfiler</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="122"/>
        <source>If set files on remote filesystems will be copied over to the local filesystem before processing. Speeds processing and reduces bandwidth on the network</source>
        <translation>Om aktiverat, kopieras fjärrfiler till lokalt filsystem innan bearbetning. Snabbar upp bearbetning och minskar bandbredd på nätverket</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="134"/>
        <source>Always Use Mythtranscode</source>
        <translation>Använd alltid Mythtranscode</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="137"/>
        <source>If set mpeg2 files will always be passed though mythtranscode to clean up any errors. May help to fix some audio problems. Ignored if &apos;Use ProjectX&apos; is set.</source>
        <translation>Om aktiverat, kommer mpeg2-filer alltid att skickas genom mythtranscode för att rensa bort eventuella fel. Kan hjälpa till att rätta eventuella ljudproblem. Ignoreras om &apos;Använd ProjectX&apos; är valt.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="149"/>
        <source>Use ProjectX</source>
        <translation>Använd ProjectX</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="152"/>
        <source>If set ProjectX will be used to cut commercials and split mpeg2 files instead of mythtranscode and mythreplex.</source>
        <translation>Om aktiverat, kommer ProjectX att användas för att klippa bort reklam och dela mpeg2-filer, istället för mythtranscode och mythreplex.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="163"/>
        <source>Use FIFOs</source>
        <translation>Använd FIFO:n</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="166"/>
        <source>The script will use FIFOs to pass the output of mplex into dvdauthor rather than creating intermediate files. Saves time and disk space during multiplex operations but not supported on Windows platform</source>
        <translation>Skriptet kommer att använda FIFO:n för att skicka utdata från mplex till dvdauthor, istället för att skapa tillfälliga filer. Sparar både tid och diskutrymme vid multiplexåtgärder, men stöds inte på Windows-plattformen</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="179"/>
        <source>Add Subtitles</source>
        <translation>Lägg till textning</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="182"/>
        <source>If available this option will add subtitles to the final DVD. Requires &apos;Use ProjectX&apos; to be on.</source>
        <translation>Om tillgängligt, kommer valet att lägga till textning på den slutliga DVD:n. Kräver att &apos;Använd ProjectX&apos; är valt.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="192"/>
        <source>Main Menu Aspect Ratio</source>
        <translation>Proportion för huvudmeny</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="194"/>
        <location filename="../mytharchive/archivesettings.cpp" line="210"/>
        <source>4:3</source>
        <comment>Aspect ratio</comment>
        <translation>4:3</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="195"/>
        <location filename="../mytharchive/archivesettings.cpp" line="211"/>
        <source>16:9</source>
        <comment>Aspect ratio</comment>
        <translation>16:9</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="199"/>
        <source>Aspect ratio to use when creating the main menu.</source>
        <translation>Proportion att använda för huvudmenyn.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="208"/>
        <source>Chapter Menu Aspect Ratio</source>
        <translation>Proportion för kapitelmeny</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="212"/>
        <location filename="../mytharchive/archivesettings.cpp" line="221"/>
        <source>Video</source>
        <translation>Video</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="217"/>
        <source>Aspect ratio to use when creating the chapter menu. &apos;%1&apos; means use the same aspect ratio as the associated video.</source>
        <extracomment>%1 is the translation of the &quot;Video&quot; combo box choice</extracomment>
        <translation>Proportion att använda för kapitelmenyn.  &apos;%1&apos; betyder använd samma proportion som video.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="228"/>
        <source>Date format</source>
        <translation>Datumformat</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="231"/>
        <source>Samples are shown using today&apos;s date.</source>
        <translation>Exempel visas med dagens datum.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="237"/>
        <source>Samples are shown using tomorrow&apos;s date.</source>
        <translation>Exempel visas med morgondagens datum.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="255"/>
        <source>Your preferred date format to use on DVD menus. %1</source>
        <extracomment>%1 gives additional info on the date used</extracomment>
        <translation>Det datumformat du föredrar att använda i DVD-menyer. %1</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="264"/>
        <source>Time format</source>
        <translation>Tidsformat</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="271"/>
        <source>Your preferred time format to display on DVD menus. You must choose a format with &quot;AM&quot; or &quot;PM&quot; in it, otherwise your time display will be 24-hour or &quot;military&quot; time.</source>
        <translation>Det tidsformat du föredrar att visa för DVD-menyer. Tidsvisningen är 24-timmars, om du inte väljer ett format med &quot;AM&quot; eller &quot;PM&quot;.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="282"/>
        <source>Default Encoder Profile</source>
        <translation>Omkodarens standardprofil</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="284"/>
        <source>HQ</source>
        <comment>Encoder profile</comment>
        <translation>HQ</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="285"/>
        <source>SP</source>
        <comment>Encoder profile</comment>
        <translation>SP</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="286"/>
        <source>LP</source>
        <comment>Encoder profile</comment>
        <translation>LP</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="287"/>
        <source>EP</source>
        <comment>Encoder profile</comment>
        <translation>EP</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="291"/>
        <source>Default encoding profile to use if a file needs re-encoding.</source>
        <translation>Förvald kodningsprofil att använda om en fil behöver omkodas.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="300"/>
        <source>mplex Command</source>
        <translation>mplex-kommando</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="304"/>
        <source>Command to run mplex</source>
        <translation>Kommando för att köra mplex</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="313"/>
        <source>dvdauthor command</source>
        <translation>dvdauthor-kommando</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="317"/>
        <source>Command to run dvdauthor.</source>
        <translation>Kommando för att köra dvdauthor.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="326"/>
        <source>mkisofs command</source>
        <translation>mkisofs-kommando</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="330"/>
        <source>Command to run mkisofs. (Used to create ISO images)</source>
        <translation>Kommando för att köra mkisofs. (Används för att skapa ISO-avbildningar)</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="339"/>
        <source>growisofs command</source>
        <translation>growisofs-kommando</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="343"/>
        <source>Command to run growisofs. (Used to burn DVDs)</source>
        <translation>Kommando för att köra growisofs. (Används för att bränna DVD:er)</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="352"/>
        <source>M2VRequantiser command</source>
        <translation>M2VRequantiser-kommando</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="356"/>
        <source>Command to run M2VRequantiser. Optional - leave blank if you don&apos;t have M2VRequantiser installed.</source>
        <translation>Kommando för att köra M2VRequantiser. Valfritt - lämna tomt om du inte har M2VRequantiser installerat.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="366"/>
        <source>jpeg2yuv command</source>
        <translation>jpeg2yuv-kommando</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="370"/>
        <source>Command to run jpeg2yuv. Part of mjpegtools package</source>
        <translation>Kommando för att köra jpeg2yuv. En del av mjpegtools-paketet</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="379"/>
        <source>spumux command</source>
        <translation>spumux-kommando</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="383"/>
        <source>Command to run spumux. Part of dvdauthor package</source>
        <translation>Kommando för att köra spumux. En del av dvdauthor-paketet</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="392"/>
        <source>mpeg2enc command</source>
        <translation>mpeg2enc-kommando</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="396"/>
        <source>Command to run mpeg2enc. Part of mjpegtools package</source>
        <translation>Kommando för att köra mpeg2enc. En del av mjpegtools-paketet</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="405"/>
        <source>projectx command</source>
        <translation>projectx kommando</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="409"/>
        <source>Command to run ProjectX. Will be used to cut commercials and split mpegs files instead of mythtranscode and mythreplex.</source>
        <translation>Kommando för att köra ProjectX. Kommer att användas för att klippa bort reklam och dela mpeg-filer, istället för mythtranscode och mythreplex.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="418"/>
        <source>MythArchive Settings</source>
        <translation>MythArchive-inställningar</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="442"/>
        <source>MythArchive External Commands</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <source>MythArchive Settings (2)</source>
        <translation type="vanished">MythArchive-inställningar (2)</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="434"/>
        <source>DVD Menu Settings</source>
        <translation>DVD-menyinställningar</translation>
    </message>
    <message>
        <source>MythArchive External Commands (1)</source>
        <translation type="vanished">MythArchive externa kommandon (1)</translation>
    </message>
    <message>
        <source>MythArchive External Commands (2)</source>
        <translation type="vanished">MythArchive externa kommandon (2)</translation>
    </message>
</context>
<context>
    <name>BurnMenu</name>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="1091"/>
        <source>Cannot burn a DVD.
The last run failed to create a DVD.</source>
        <translation>Kan inte bränna en DVD.
Den senaste körningen misslyckades skapa en DVD.</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="1097"/>
        <location filename="../mytharchive/mythburn.cpp" line="1109"/>
        <source>Burn DVD</source>
        <translation>Bränn DVD</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="1098"/>
        <source>
Place a blank DVD in the drive and select an option below.</source>
        <translation>Lägg en tom DVD i brännaren och välj ett alternativ nedan.</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="1110"/>
        <source>Burn DVD Rewritable</source>
        <translation>Bränn återskrivbar DVD</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="1111"/>
        <source>Burn DVD Rewritable (Force Erase)</source>
        <translation>Bränn återskrivbar DVD (tvinga radering)</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="1165"/>
        <source>It was not possible to run mytharchivehelper to burn the DVD.</source>
        <translation>Det var inte möjligt att köra &apos;mytharchivehelper&apos; för att bränna DVD:n.</translation>
    </message>
</context>
<context>
    <name>BurnThemeUI</name>
    <message>
        <location filename="../mythburn/themes/burnthemestrings.h" line="5"/>
        <source>Has an intro and contains a main menu with 4 recordings per page. Does not have a chapter selection submenu.</source>
        <translation>Har ett intro och innehåller en huvudmeny med fyra inspelningar per sida. Har ingen undermeny för val av kapitel.</translation>
    </message>
    <message>
        <location filename="../mythburn/themes/burnthemestrings.h" line="6"/>
        <source>Has an intro and contains a summary main menu with 10 recordings per page. Does not have a chapter selection submenu, recording titles, dates or category.</source>
        <translation>Har ett intro och innehåller en huvudmeny med tio inspelningr per sida. Har ingen undermeny för val av kapitel, inspelningstitel, datum eller kategori.</translation>
    </message>
    <message>
        <location filename="../mythburn/themes/burnthemestrings.h" line="7"/>
        <source>Has an intro and contains a main menu with 6 recordings per page. Does not have a scene selection submenu.</source>
        <translation>Har ett intro och innehåller en huvdmeny med sex inspelningar per sida. Har inte någon undermeny för val scener.</translation>
    </message>
    <message>
        <location filename="../mythburn/themes/burnthemestrings.h" line="8"/>
        <source>Has an intro and contains a main menu with 3 recordings per page and a scene selection submenu with 8 chapters points. Shows a program details page before each recording.</source>
        <translation>Har ett intro och innehåller en huvudmeny med tre inspelningar per sida samt en undermeny för val av scener med åtta kapitelval. Visar en sida med programdetaljer innan varje inspelning.</translation>
    </message>
    <message>
        <location filename="../mythburn/themes/burnthemestrings.h" line="9"/>
        <source>Has an intro and contains a main menu with 3 recordings per page and a scene selection submenu with 8 chapters points. Shows a program details page before each recording. Uses animated thumb images.</source>
        <translation>Har ett intro och innehåller en huvudmeny med tre inspelningar per sida samt en undermeny för val av scener med åtta kapitelval. Visar en sida med programdetaljer innan varje inspelning. Använder animerade miniatyrbilder.</translation>
    </message>
    <message>
        <location filename="../mythburn/themes/burnthemestrings.h" line="10"/>
        <source>Has an intro and contains a main menu with 3 recordings per page and a scene selection submenu with 8 chapters points.</source>
        <translation>Har ett intro och innehåller en huvudmeny med tre inspelningar per sida samt en undermeny för val av scener med åtta kapitelval.</translation>
    </message>
    <message>
        <location filename="../mythburn/themes/burnthemestrings.h" line="11"/>
        <source>Has an intro and contains a main menu with 3 recordings per page and a scene selection submenu with 8 chapters points. All the thumb images are animated.</source>
        <translation>Har ett intro och innehåller en huvudmeny med tre inspelningar per sida samt en undermeny för val av scener med åtta kapitelval. Alla miniatyrbilder är animerade.</translation>
    </message>
    <message>
        <location filename="../mythburn/themes/burnthemestrings.h" line="12"/>
        <source>Creates an auto play DVD with no menus. Shows an intro movie then for each title shows a details page followed by the video in sequence.</source>
        <translation>Skapar en DVD utan menyer som startar automatiskt. Visar en introfilm och därefter visas för varje titel en sida med detaljer följt av filmen.</translation>
    </message>
    <message>
        <location filename="../mythburn/themes/burnthemestrings.h" line="13"/>
        <source>Creates an auto play DVD with no menus and no intro.</source>
        <translation>Skapar en DVD som startar automatiskt och är utan menyer och introfilm.</translation>
    </message>
</context>
<context>
    <name>DVDThemeSelector</name>
    <message>
        <location filename="../mytharchive/themeselector.cpp" line="201"/>
        <location filename="../mytharchive/themeselector.cpp" line="212"/>
        <source>No theme description file found!</source>
        <translation>Ingen temabeskrivningsfil hittades!</translation>
    </message>
    <message>
        <location filename="../mytharchive/themeselector.cpp" line="225"/>
        <source>Empty theme description!</source>
        <translation>Tom temabeskrivningsfil!</translation>
    </message>
    <message>
        <location filename="../mytharchive/themeselector.cpp" line="230"/>
        <source>Unable to open theme description file!</source>
        <translation>Kan inte öppna temabeskrivningsfilen!</translation>
    </message>
</context>
<context>
    <name>ExportNative</name>
    <message>
        <location filename="../mytharchive/exportnative.cpp" line="198"/>
        <source>You need to add at least one item to archive!</source>
        <translation>Du måste lägga till minst en post att arkivera!</translation>
    </message>
    <message>
        <location filename="../mytharchive/exportnative.cpp" line="350"/>
        <source>Remove Item</source>
        <translation>Ta bort post</translation>
    </message>
    <message>
        <location filename="../mytharchive/exportnative.cpp" line="442"/>
        <source>It was not possible to create the DVD. An error occured when running the scripts</source>
        <translation>Det var inte möjligt att skapa DVD:n. Ett fel uppstod när skripten kördes</translation>
    </message>
    <message>
        <location filename="../mytharchive/exportnative.cpp" line="478"/>
        <source>You don&apos;t have any videos!</source>
        <translation>Du har inga videor!</translation>
    </message>
    <message>
        <location filename="../mytharchive/exportnative.cpp" line="343"/>
        <source>Menu</source>
        <translation>Meny</translation>
    </message>
</context>
<context>
    <name>FileSelector</name>
    <message>
        <location filename="../mytharchive/fileselector.cpp" line="280"/>
        <source>The selected item is not a directory!</source>
        <translation>Vald post är inte en katalog!</translation>
    </message>
    <message>
        <location filename="../mytharchive/fileselector.cpp" line="61"/>
        <source>Find File</source>
        <translation>Sök efter fil</translation>
    </message>
    <message>
        <location filename="../mytharchive/fileselector.cpp" line="64"/>
        <source>Find Directory</source>
        <translation>Sök efter katalog</translation>
    </message>
    <message>
        <location filename="../mytharchive/fileselector.cpp" line="67"/>
        <source>Find Files</source>
        <translation>Sök efter filer</translation>
    </message>
</context>
<context>
    <name>ImportNative</name>
    <message>
        <location filename="../mytharchive/importnative.cpp" line="389"/>
        <source>You need to select a valid channel id!</source>
        <translation>Du måste välja ett giltigt kanal-ID!</translation>
    </message>
    <message>
        <location filename="../mytharchive/importnative.cpp" line="420"/>
        <source>It was not possible to import the Archive.  An error occured when running &apos;mytharchivehelper&apos;</source>
        <translation>Det var inte möjligt att importera arkivet. Ett fel uppstod när &apos;mytharchivehelper&apos; kördes</translation>
    </message>
    <message>
        <location filename="../mytharchive/importnative.cpp" line="550"/>
        <source>Select a channel id</source>
        <translation>Välj ett kanal-ID</translation>
    </message>
    <message>
        <location filename="../mytharchive/importnative.cpp" line="576"/>
        <source>Select a channel number</source>
        <translation>Välj ett kanalnummer</translation>
    </message>
    <message>
        <location filename="../mytharchive/importnative.cpp" line="602"/>
        <source>Select a channel name</source>
        <translation>Välj ett kanalnamn</translation>
    </message>
    <message>
        <location filename="../mytharchive/importnative.cpp" line="628"/>
        <source>Select a Callsign</source>
        <translation>Välj ett stationsnamn</translation>
    </message>
</context>
<context>
    <name>LogViewer</name>
    <message>
        <location filename="../mytharchive/logviewer.cpp" line="340"/>
        <source>Show Progress Log</source>
        <translation>Visa förloppslogg</translation>
    </message>
    <message>
        <location filename="../mytharchive/logviewer.cpp" line="341"/>
        <source>Show Full Log</source>
        <translation>Visa hela loggen</translation>
    </message>
    <message>
        <location filename="../mytharchive/logviewer.cpp" line="336"/>
        <source>Don&apos;t Auto Update</source>
        <translation>Ingen automatisk uppdatering</translation>
    </message>
    <message>
        <location filename="../mytharchive/logviewer.cpp" line="75"/>
        <source>Cannot find any logs to show!</source>
        <translation>Kan inte hitta några loggar att visa!</translation>
    </message>
    <message>
        <location filename="../mytharchive/logviewer.cpp" line="189"/>
        <source>Background creation has been asked to stop.
This may take a few minutes.</source>
        <translation>Bakgrundgenerering har skickats en begäran om att stoppa.
Detta kan ta ett par minuter.</translation>
    </message>
    <message>
        <location filename="../mytharchive/logviewer.cpp" line="338"/>
        <source>Auto Update</source>
        <translation>Automatisk uppdatering</translation>
    </message>
    <message>
        <location filename="../mytharchive/logviewer.cpp" line="328"/>
        <source>Menu</source>
        <translation>Meny</translation>
    </message>
</context>
<context>
    <name>MythBurn</name>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="328"/>
        <location filename="../mytharchive/mythburn.cpp" line="448"/>
        <source>No Cut List</source>
        <translation>Ingen klipplista</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="339"/>
        <source>You need to add at least one item to archive!</source>
        <translation>Du måste lägga till minst en post att arkivera!</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="385"/>
        <source>Retrieving File Information. Please Wait...</source>
        <translation>Hämtar information om filen. Vänta...</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="451"/>
        <source>Encoder: </source>
        <translation>Kodare: </translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="317"/>
        <location filename="../mytharchive/mythburn.cpp" line="437"/>
        <source>Using Cut List</source>
        <translation>Använder klipplista</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="322"/>
        <location filename="../mytharchive/mythburn.cpp" line="442"/>
        <source>Not Using Cut List</source>
        <translation>Använder inte klipplista</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="774"/>
        <source>Don&apos;t Use Cut List</source>
        <translation>Använd inte klipplista</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="779"/>
        <source>Use Cut List</source>
        <translation>Använd klipplista</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="784"/>
        <source>Remove Item</source>
        <translation>Ta bort post</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="785"/>
        <source>Edit Details</source>
        <translation>Redigera information</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="786"/>
        <source>Change Encoding Profile</source>
        <translation>Ändra kodningsprofil</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="787"/>
        <source>Edit Thumbnails</source>
        <translation>Redigera miniatyrbilder</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="922"/>
        <source>It was not possible to create the DVD.  An error occured when running the scripts</source>
        <translation>Det var inte möjligt att skapa DVD. Ett fel uppstod när skripten kördes</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="964"/>
        <source>You don&apos;t have any videos!</source>
        <translation>Du har inga videor!</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="763"/>
        <source>Menu</source>
        <translation>Meny</translation>
    </message>
</context>
<context>
    <name>MythControls</name>
    <message>
        <location filename="../mytharchive/mytharchive.cpp" line="329"/>
        <source>Toggle use cut list state for selected program</source>
        <translation>Växla använd klipplista för valt program</translation>
    </message>
    <message>
        <location filename="../mytharchive/mytharchive.cpp" line="332"/>
        <source>Create DVD</source>
        <translation>Skapa DVD</translation>
    </message>
    <message>
        <location filename="../mytharchive/mytharchive.cpp" line="334"/>
        <source>Create Archive</source>
        <translation>Skapa arkiv</translation>
    </message>
    <message>
        <location filename="../mytharchive/mytharchive.cpp" line="336"/>
        <source>Import Archive</source>
        <translation>Importera arkiv</translation>
    </message>
    <message>
        <location filename="../mytharchive/mytharchive.cpp" line="338"/>
        <source>View Archive Log</source>
        <translation>Visa arkiveringslogg</translation>
    </message>
    <message>
        <location filename="../mytharchive/mytharchive.cpp" line="340"/>
        <source>Play Created DVD</source>
        <translation>Spela skapad DVD</translation>
    </message>
    <message>
        <location filename="../mytharchive/mytharchive.cpp" line="342"/>
        <source>Burn DVD</source>
        <translation>Bränn DVD</translation>
    </message>
</context>
<context>
    <name>RecordingSelector</name>
    <message>
        <location filename="../mytharchive/recordingselector.cpp" line="110"/>
        <source>Retrieving Recording List.
Please Wait...</source>
        <translation>Hämtar inspelningslista.
Vänta...</translation>
    </message>
    <message>
        <location filename="../mytharchive/recordingselector.cpp" line="189"/>
        <source>Clear All</source>
        <translation>Rensa alla</translation>
    </message>
    <message>
        <location filename="../mytharchive/recordingselector.cpp" line="190"/>
        <source>Select All</source>
        <translation>Välj alla</translation>
    </message>
    <message>
        <location filename="../mytharchive/recordingselector.cpp" line="89"/>
        <location filename="../mytharchive/recordingselector.cpp" line="374"/>
        <location filename="../mytharchive/recordingselector.cpp" line="479"/>
        <source>All Recordings</source>
        <translation>Alla inspelningar</translation>
    </message>
    <message>
        <location filename="../mytharchive/recordingselector.cpp" line="134"/>
        <source>Either you don&apos;t have any recordings or no recordings are available locally!</source>
        <translation>Antingen har du inga inspelningar, eller så är inga inspelningar lokalt tillgängliga!</translation>
    </message>
    <message>
        <location filename="../mytharchive/recordingselector.cpp" line="182"/>
        <source>Menu</source>
        <translation>Meny</translation>
    </message>
</context>
<context>
    <name>SelectDestination</name>
    <message>
        <location filename="../mytharchive/archiveutil.cpp" line="29"/>
        <source>Single Layer DVD</source>
        <translation>DVD med ett lager</translation>
    </message>
    <message>
        <location filename="../mytharchive/archiveutil.cpp" line="33"/>
        <source>Dual Layer DVD</source>
        <translation>DVD med två lager</translation>
    </message>
    <message>
        <location filename="../mytharchive/archiveutil.cpp" line="30"/>
        <source>Single Layer DVD (4,482 MB)</source>
        <translation>DVD med ett lager (4482 MiB)</translation>
    </message>
    <message>
        <location filename="../mytharchive/archiveutil.cpp" line="34"/>
        <source>Dual Layer DVD (8,964 MB)</source>
        <translation>DVD med två lager (8964 MiB)</translation>
    </message>
    <message>
        <location filename="../mytharchive/archiveutil.cpp" line="37"/>
        <source>DVD +/- RW</source>
        <translation>DVD +/- RW</translation>
    </message>
    <message>
        <location filename="../mytharchive/archiveutil.cpp" line="38"/>
        <source>Rewritable DVD</source>
        <translation>Återskrivbar DVD</translation>
    </message>
    <message>
        <location filename="../mytharchive/archiveutil.cpp" line="41"/>
        <source>File</source>
        <translation>Fil</translation>
    </message>
    <message>
        <location filename="../mytharchive/archiveutil.cpp" line="42"/>
        <source>Any file accessable from your filesystem.</source>
        <translation>Vilken fil som helst, tillgänglig i filsystemet.</translation>
    </message>
    <message>
        <location filename="../mytharchive/selectdestination.cpp" line="262"/>
        <location filename="../mytharchive/selectdestination.cpp" line="318"/>
        <source>Unknown</source>
        <translation>Okänt</translation>
    </message>
</context>
<context>
    <name>ThemeUI</name>
    <message>
        <location filename="themestrings.h" line="218"/>
        <source>Select Archive Items</source>
        <translation>Välj arkivposter</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="175"/>
        <source>No files are selected for archive</source>
        <translation>Inga filer är valda för arkivet</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="287"/>
        <source>title goes here</source>
        <translation>titel här</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="286"/>
        <source>sep 13, 2004 11:00 pm (1h 15m)</source>
        <translation>13 sep, 2004 23:00 (1tim 15m)</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="280"/>
        <source>description goes here.</source>
        <translation>beskrivning här.</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="290"/>
        <source>x.xx Gb</source>
        <translation>x.xx GiB</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="124"/>
        <source>File Finder To Import</source>
        <translation>Hitta filer att importera</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="11"/>
        <source>0 mb</source>
        <translation>0 MiB</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="292"/>
        <source>xxxxx mb</source>
        <translation>xxxxx MiB</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="263"/>
        <source>Title:</source>
        <translation>Titel:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="252"/>
        <source>Subtitle:</source>
        <translation>Undertitel:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="248"/>
        <source>Start Time:</source>
        <translation>Starttid:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="219"/>
        <source>Select Associated Channel</source>
        <translation>Välj tillhörande kanal</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="45"/>
        <source>Archived Channel</source>
        <translation>Arkiverad kanal</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="60"/>
        <source>Chan. ID:</source>
        <translation>Kanal-ID:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="62"/>
        <source>Chan. No:</source>
        <translation>Kanal-nr:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="56"/>
        <source>Callsign:</source>
        <translation>Stationsnamn:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="167"/>
        <source>Name:</source>
        <translation>Namn:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="151"/>
        <source>Local Channel</source>
        <translation>Lokal kanal</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="221"/>
        <source>Select Destination</source>
        <translation>Välj destination</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="138"/>
        <source>Free Space:</source>
        <translation>Ledigt:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="12"/>
        <source>0.00 GB</source>
        <oldsource>0.00Gb</oldsource>
        <translation>0,00 GiB</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="160"/>
        <source>Make ISO Image</source>
        <translation>Skapa ISO-avbild</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="52"/>
        <source>Burn to DVD</source>
        <translation>Bränn DVD</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="6"/>
        <source>%SIZE% ~ %PROFILE%</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="8"/>
        <source>%date% / %profile%</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="9"/>
        <source>%size% (%profile%)</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="13"/>
        <source>0.00Gb</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="17"/>
        <source>12.34GB</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="18"/>
        <source>&lt;-- Details</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="19"/>
        <source>&lt;-- Main Menu</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="26"/>
        <source>Add Recordings</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="28"/>
        <source>Add Videos</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="29"/>
        <source>Add a recording or video to archive.</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="30"/>
        <source>Add a recording, video or file to archive.</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="32"/>
        <source>Archive</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="33"/>
        <source>Archive Callsign:</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="34"/>
        <source>Archive Chan ID:</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="35"/>
        <source>Archive Chan No:</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="36"/>
        <source>Archive Files</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="38"/>
        <source>Archive Item:</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="40"/>
        <source>Archive Items to DVD</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="41"/>
        <source>Archive Log Viewer</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="42"/>
        <source>Archive Media</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="43"/>
        <source>Archive Name:</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="46"/>
        <source>Associate Channel</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="47"/>
        <source>Associated Channel</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="49"/>
        <source>Burn</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="50"/>
        <source>Burn Created DVD</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="51"/>
        <source>Burn the last created archive to DVD</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="53"/>
        <source>Burn to DVD:</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="54"/>
        <source>CUTLIST</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="55"/>
        <source>Callsign</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="57"/>
        <source>Callsign:  %1</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="59"/>
        <source>Cancel Job</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="61"/>
        <source>Chan. Id:</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="63"/>
        <source>Change</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="65"/>
        <source>Channel ID</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="67"/>
        <source>Channel ID:  %1</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="68"/>
        <source>Channel Name:</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="69"/>
        <source>Channel No</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="71"/>
        <source>Channel Number:  %1</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="74"/>
        <source>Chapter Menu --&gt;</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="75"/>
        <source>Chapter Menu:</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="76"/>
        <source>Chapters</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="82"/>
        <source>Create ISO Image:</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="83"/>
        <source>Create a</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="84"/>
        <source>Create a DVD of your videos</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="85"/>
        <source>Current Destination:</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="87"/>
        <source>Current Position:</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="88"/>
        <source>Current Size:</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="89"/>
        <source>Current selected item size: %1</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="90"/>
        <source>Current size: %1</source>
        <translation type="unfinished"></translation>
    </message>
    <message numerus="yes">
        <location filename="themestrings.h" line="91"/>
        <source>Current: %n</source>
        <translation type="unfinished">
            <numerusform></numerusform>
            <numerusform></numerusform>
        </translation>
    </message>
    <message>
        <location filename="themestrings.h" line="92"/>
        <source>DVD</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="93"/>
        <source>DVD Media</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="94"/>
        <source>DVD Menu</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="96"/>
        <source>Date (time)</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="98"/>
        <source>Destination</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="99"/>
        <source>Destination Free Space:</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="103"/>
        <source>Details:</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="104"/>
        <source>Done</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="105"/>
        <source>Edit Archive item Information</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="106"/>
        <source>Edit Details</source>
        <translation type="unfinished">Redigera information</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="107"/>
        <source>Edit Thumbnails</source>
        <translation type="unfinished">Redigera miniatyrbilder</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="108"/>
        <source>Eject your</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="110"/>
        <source>Encode your video</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="111"/>
        <source>Encode your video to a file</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="112"/>
        <source>Encoded Size:</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="113"/>
        <source>Encoder Profile:</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="114"/>
        <source>Encoding Profile</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="116"/>
        <source>Error: %1</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="118"/>
        <source>Export</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="119"/>
        <source>Export your</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="120"/>
        <source>Export your videos</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="121"/>
        <source>File</source>
        <translation type="unfinished">Fil</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="122"/>
        <source>File Browser</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="125"/>
        <source>File browser</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="126"/>
        <source>File...</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="127"/>
        <source>Filename:</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="128"/>
        <source>Filesize</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="132"/>
        <source>Find Location</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="134"/>
        <source>First, select the thumb image you want to change from the overview. Second, press the &apos;tab&apos; key to move to the select thumb frame button to change the image.</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="135"/>
        <source>Force Overwrite of DVD-RW Media</source>
        <translation>Tvinga överskrivning av DVD-RW media</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="136"/>
        <source>Force Overwrite of DVD-RW Media:</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="140"/>
        <source>Import</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="142"/>
        <source>Import an</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="143"/>
        <source>Import recordings from a native archive</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="144"/>
        <source>Import videos from an Archive</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="146"/>
        <source>Intro --&gt;</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="147"/>
        <source>Intro:</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="148"/>
        <source>Local Callsign:</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="149"/>
        <source>Local Chan ID:</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="150"/>
        <source>Local Chan No:</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="152"/>
        <source>Local Name:</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="153"/>
        <source>Log</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="155"/>
        <source>Log viewer</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="156"/>
        <source>MENU changes focus. Numbers 0-9 jump to that thumb image.
When the preview image has focus, UP/DOWN changes the seek amount, LEFT/RIGHT jumps forward/backward by the seek amount, and SELECT chooses the current preview image for the selected thumb image.</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="158"/>
        <source>Main Menu:</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="159"/>
        <source>Main menu</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="161"/>
        <source>Make ISO image</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="162"/>
        <source>Make ISO image:</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="163"/>
        <source>Media</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="165"/>
        <source>Metadata</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="166"/>
        <source>Name</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="168"/>
        <source>Name:  %1</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="169"/>
        <source>Navigation</source>
        <translation type="unfinished"></translation>
    </message>
    <message numerus="yes">
        <location filename="themestrings.h" line="172"/>
        <source>New: %n</source>
        <translation type="unfinished">
            <numerusform></numerusform>
            <numerusform></numerusform>
        </translation>
    </message>
    <message>
        <location filename="themestrings.h" line="174"/>
        <source>No files are selected for DVD</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="177"/>
        <source>Not Applicable</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="178"/>
        <source>Not Available in this Theme</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="179"/>
        <source>Not available in this theme</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="184"/>
        <source>Original Size:</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="186"/>
        <source>Overwrite DVD-RW Media:</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="188"/>
        <source>Parental Level</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="189"/>
        <source>Parental Level:</source>
        <translation type="unfinished"></translation>
    </message>
    <message numerus="yes">
        <location filename="themestrings.h" line="191"/>
        <source>Parental Level: %n</source>
        <translation type="unfinished">
            <numerusform></numerusform>
            <numerusform></numerusform>
        </translation>
    </message>
    <message>
        <location filename="themestrings.h" line="194"/>
        <source>Play the last created archive DVD</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="195"/>
        <source>Position View:</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="196"/>
        <source>Press &apos;left&apos; and &apos;right&apos; arrows to move through the frames. Press &apos;up&apos; and &apos;down&apos; arrows to change seek amount. Press &apos;select&apos; or &apos;enter&apos; key to lock the selected thumb image.</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="198"/>
        <source>Preview</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="201"/>
        <source>Read video from data dvd or file</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="203"/>
        <source>Recordings</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="204"/>
        <source>Recordings group:</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="206"/>
        <source>Save recordings and videos to a native archive</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="207"/>
        <source>Save recordings and videos to video DVD</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="209"/>
        <source>Search Chan ID</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="210"/>
        <source>Search Chan NO</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="212"/>
        <source>Search Local</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="216"/>
        <source>Seek Amount:</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="217"/>
        <source>Seek amount:</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="220"/>
        <source>Select Channel</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="224"/>
        <source>Select Files</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="225"/>
        <source>Select Recordings</source>
        <translation>Välj inspelningar</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="226"/>
        <source>Select Recordings for your archive or image</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="228"/>
        <source>Select a destination for your archive or image</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="233"/>
        <source>Select thumb image:</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="234"/>
        <source>Select your DVD menu theme</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="235"/>
        <source>Selected Items to be archived</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="236"/>
        <source>Selected Items to be burned</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="237"/>
        <source>Selected recording item size: %1</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="238"/>
        <source>Selected video item size: %1</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="239"/>
        <source>Sep 13, 2004 11:00 pm (1h 15m)</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="240"/>
        <source>Show</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="241"/>
        <source>Show Recordings</source>
        <translation>Visa inspelningar</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="293"/>
        <source>~</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="123"/>
        <source>File Finder</source>
        <translation>Filbläddrare</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="227"/>
        <source>Select Videos</source>
        <translation>Välj video</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="272"/>
        <source>Video Category</source>
        <translation>Videokategori</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="187"/>
        <source>PL:</source>
        <translation>SL:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="15"/>
        <source>1</source>
        <translation>1</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="176"/>
        <source>No videos available</source>
        <translation>Ingen video tillgänglig</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="154"/>
        <source>Log Viewer</source>
        <translation>Loggvisare</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="64"/>
        <source>Change Encoding Profile</source>
        <translation>Ändra kodningsprofil</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="16"/>
        <source>12.34 GB</source>
        <oldsource>12.34GB</oldsource>
        <translation>12,34 GiB</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="95"/>
        <source>DVD Menu Theme</source>
        <translation>DVD menytema</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="229"/>
        <source>Select a theme</source>
        <translation>Välj ett tema</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="145"/>
        <source>Intro</source>
        <translation>Inledning</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="78"/>
        <source>Choose where you would like your files archived.</source>
        <translation>Välj var du vill lagra dina arkiv.</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="185"/>
        <source>Output Type:</source>
        <translation>Utdatatyp:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="100"/>
        <source>Destination:</source>
        <translation>Destination:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="79"/>
        <source>Click here to find an output location...</source>
        <translation>Klicka här för att hitta en utdataplats...</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="115"/>
        <source>Erase DVD-RW before burning</source>
        <translation>Töm DVD-RW innan bränning</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="58"/>
        <source>Cancel</source>
        <translation>Avbryt</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="199"/>
        <source>Previous</source>
        <translation>Föregående</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="173"/>
        <source>Next</source>
        <translation>Nästa</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="130"/>
        <source>Filter:</source>
        <translation>Filter:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="180"/>
        <source>OK</source>
        <translation>OK</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="231"/>
        <source>Select the file you wish to use.</source>
        <translation>Välj den fil som du vill använda.</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="48"/>
        <source>Back</source>
        <translation>Tillbaka</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="139"/>
        <source>Home</source>
        <translation>Hem</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="214"/>
        <source>See logs from your archive runs.</source>
        <translation>Se loggar från dina arkivkörningar.</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="242"/>
        <source>Show Videos</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="243"/>
        <source>Show log with archive activities</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="244"/>
        <source>Show the Archive Log Viewer</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="245"/>
        <source>Size:</source>
        <translation type="unfinished"></translation>
    </message>
    <message numerus="yes">
        <location filename="themestrings.h" line="246"/>
        <source>Size: %n</source>
        <translation type="unfinished">
            <numerusform></numerusform>
            <numerusform></numerusform>
        </translation>
    </message>
    <message>
        <location filename="themestrings.h" line="249"/>
        <source>Start Time: %1</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="250"/>
        <source>Start to</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="251"/>
        <source>Start to Burn your archive to DVD</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="253"/>
        <source>Subtitle: %1</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="254"/>
        <source>Test</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="255"/>
        <source>Test created</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="256"/>
        <source>Test created DVD</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="257"/>
        <source>Theme description</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="258"/>
        <source>Theme for the DVD menu:</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="259"/>
        <source>Theme preview:</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="264"/>
        <source>Title: %1</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="265"/>
        <source>Tools and Utilities for archives</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="267"/>
        <source>Update</source>
        <translation>Uppdatera</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="117"/>
        <source>Exit</source>
        <translation>Avsluta</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="266"/>
        <source>Up Level</source>
        <translation>Upp nivå</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="289"/>
        <source>x.xx GB</source>
        <translation>x,xx GiB</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="5"/>
        <source>%DATE%, %TIME%</source>
        <translation>%DATE%, %TIME%</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="273"/>
        <source>Video Category:</source>
        <translation>Videokategori:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="77"/>
        <source>Choose the appearance of your DVD.</source>
        <translation>Välj utseende på din DVD.</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="260"/>
        <source>Theme:</source>
        <translation>Tema:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="157"/>
        <source>Main Menu</source>
        <translation>Huvudmeny</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="73"/>
        <source>Chapter Menu</source>
        <translation>Kapitelmeny</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="102"/>
        <source>Details</source>
        <translation>Information</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="232"/>
        <source>Select the recordings and videos you wish to save.</source>
        <translation>Välj de inspelningar och videor du vill spara.</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="25"/>
        <source>Add Recording</source>
        <translation>Lägg till inspelning</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="27"/>
        <source>Add Video</source>
        <translation>Lägg till video</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="24"/>
        <source>Add File</source>
        <translation>Lägg till fil</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="37"/>
        <source>Archive Item Details</source>
        <translation>Information om arkivposten</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="247"/>
        <source>Start Date:</source>
        <translation>Startdatum:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="262"/>
        <source>Time:</source>
        <translation>Tid:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="97"/>
        <source>Description:</source>
        <translation>Beskrivning:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="261"/>
        <source>Thumb Image Selector</source>
        <translation>Miniatyrbildsväljare</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="86"/>
        <source>Current Position</source>
        <translation>Aktuell position</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="14"/>
        <source>0:00:00.00</source>
        <translation>0:00:00.00</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="215"/>
        <source>Seek Amount</source>
        <translation>Söklängd</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="137"/>
        <source>Frame</source>
        <translation>Bild</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="21"/>
        <source>A high quality profile giving approx. 1 hour of video on a single layer DVD</source>
        <translation>En högkvalitativ profil ger ungefär 1 timme video på en enkellagers DVD</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="23"/>
        <source>A standard play profile giving approx. 2 hour of video on a single layer DVD</source>
        <translation>En standard profil ger ungefär 2 timmar video på en enkellagers DVD</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="22"/>
        <source>A long play profile giving approx. 4 hour of video on a single layer DVD</source>
        <translation>En long-play profil ger ungefär 4 timmar video på en enkellagers DVD</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="20"/>
        <source>A extended play profile giving approx. 6 hour of video on a single layer DVD</source>
        <translation>En utökad profil ger ungefär 6 timmar video på en enkellagers DVD</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="205"/>
        <source>Save</source>
        <translation>Spara</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="268"/>
        <source>Use archive</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="269"/>
        <source>Used: %1</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="270"/>
        <source>Utilities</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="271"/>
        <source>Utilities for MythArchive</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="274"/>
        <source>Video category:</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="275"/>
        <source>Videos</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="276"/>
        <source>View progress of your archive or image</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="277"/>
        <source>Write video to data dvd or file</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="278"/>
        <source>XML File to Import</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="279"/>
        <source>decrease seek amount</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="281"/>
        <source>frame</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="282"/>
        <source>increase seek amount</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="283"/>
        <source>move left</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="284"/>
        <source>move right</source>
        <translation type="unfinished"></translation>
    </message>
    <message numerus="yes">
        <location filename="themestrings.h" line="285"/>
        <source>profile: %n</source>
        <translation type="unfinished">
            <numerusform></numerusform>
            <numerusform></numerusform>
        </translation>
    </message>
    <message>
        <location filename="themestrings.h" line="288"/>
        <source>to</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="291"/>
        <source>xxxxx MB</source>
        <translation>xxxxx MiB</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="131"/>
        <source>Find</source>
        <translation>Sök</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="10"/>
        <source>0 MB</source>
        <translation>0 MiB</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="197"/>
        <source>Prev</source>
        <translation>Föregående</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="211"/>
        <source>Search Channel</source>
        <translation>Sök kanal</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="208"/>
        <source>Search Callsign</source>
        <translation>Sök stationsnamn</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="213"/>
        <source>Search Name</source>
        <translation>Sök namn</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="133"/>
        <source>Finish</source>
        <translation>Slutför</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="222"/>
        <source>Select Destination:</source>
        <translation>Välj destination:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="192"/>
        <source>Parental level: %1</source>
        <translation>Föräldranivå: %1</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="183"/>
        <source>Old size:</source>
        <translation>Tidigare storlek:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="171"/>
        <source>New size:</source>
        <translation>Ny storlek:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="230"/>
        <source>Select a theme:</source>
        <translation>Välj ett tema:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="164"/>
        <source>Menu</source>
        <translation>Meny</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="72"/>
        <source>Chapter</source>
        <translation>Kapitel</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="101"/>
        <source>Detail</source>
        <translation>Detaljer</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="223"/>
        <source>Select File to Import</source>
        <translation>Välj fil att importera</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="31"/>
        <source>Add video</source>
        <translation>Lägg till video</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="129"/>
        <source>Filesize: %1</source>
        <translation>Filstorlek: %1</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="202"/>
        <source>Recorded Time: %1</source>
        <translation>Inspelad tid: %1</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="7"/>
        <source>%&quot;|SUBTITLE|&quot; %%(|ORIGINALAIRDATE|) %%(|STARS|) %%DESCRIPTION%</source>
        <translation>%&quot;|SUBTITLE|&quot; %%(|ORIGINALAIRDATE|) %%(|STARS|) %%DESCRIPTION%</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="181"/>
        <source>Ok</source>
        <translation>Ok</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="190"/>
        <source>Parental Level: %1</source>
        <translation>Föräldranivå: %1</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="39"/>
        <source>Archive Items</source>
        <translation>Arkivposter</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="200"/>
        <source>Profile:</source>
        <translation>Profil:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="182"/>
        <source>Old Size:</source>
        <translation>Tidigare storlek:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="170"/>
        <source>New Size:</source>
        <translation>Ny storlek:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="66"/>
        <source>Channel ID:</source>
        <translation>Kanal-ID:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="70"/>
        <source>Channel Number:</source>
        <translation>Kanalnummer:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="81"/>
        <source>Create DVD</source>
        <translation>Skapa DVD</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="80"/>
        <source>Create Archive</source>
        <translation>Skapa arkiv</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="109"/>
        <source>Encode Video File</source>
        <translation>Omkoda videofil</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="141"/>
        <source>Import Archive</source>
        <translation>Importera arkiv</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="44"/>
        <source>Archive Utilities</source>
        <translation>Arkivverktyg</translation>
    </message>
    <message>
        <source>Show Log Viewer</source>
        <translation type="vanished">Loggvisaren</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="193"/>
        <source>Play Created DVD</source>
        <translation>Spela skapad DVD</translation>
    </message>
    <message>
        <source>Burn DVD</source>
        <translation type="vanished">Bränn DVD</translation>
    </message>
</context>
<context>
    <name>ThumbFinder</name>
    <message>
        <location filename="../mytharchive/thumbfinder.cpp" line="858"/>
        <source>Exit, Save Thumbnails</source>
        <translation>Avbryt, spara miniatyrbilder</translation>
    </message>
    <message>
        <location filename="../mytharchive/thumbfinder.cpp" line="859"/>
        <source>Exit, Don&apos;t Save Thumbnails</source>
        <translation>Avbryt, spara inte miniatyrbilder</translation>
    </message>
    <message>
        <location filename="../mytharchive/thumbfinder.cpp" line="851"/>
        <source>Menu</source>
        <translation>Meny</translation>
    </message>
</context>
<context>
    <name>VideoSelector</name>
    <message>
        <location filename="../mytharchive/videoselector.cpp" line="148"/>
        <source>Clear All</source>
        <translation>Rensa alla</translation>
    </message>
    <message>
        <location filename="../mytharchive/videoselector.cpp" line="149"/>
        <source>Select All</source>
        <translation>Välj alla</translation>
    </message>
    <message>
        <location filename="../mytharchive/videoselector.cpp" line="549"/>
        <source>You need to enter a valid password for this parental level</source>
        <translation>Du måste ange ett giltigt lösenord för den här barnlåsnivån</translation>
    </message>
    <message>
        <location filename="../mytharchive/videoselector.cpp" line="331"/>
        <location filename="../mytharchive/videoselector.cpp" line="497"/>
        <source>All Videos</source>
        <translation>Alla videor</translation>
    </message>
    <message>
        <location filename="../mytharchive/videoselector.cpp" line="141"/>
        <source>Menu</source>
        <translation>Meny</translation>
    </message>
</context>
</TS>
