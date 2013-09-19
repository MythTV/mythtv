<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE TS>
<TS version="2.0" language="sv_SE">
<context>
    <name>(ArchiveUtils)</name>
    <message>
        <location filename="../mytharchive/archiveutil.cpp" line="80"/>
        <source>Cannot find the MythArchive work directory.
Have you set the correct path in the settings?</source>
        <translation>Kan inte hitta arbetskatalog för MythArchive.
Har du ställt in rätt sökväg i inställningarna?</translation>
    </message>
</context>
<context>
    <name>(MythArchiveMain)</name>
    <message>
        <location filename="../mytharchive/main.cpp" line="93"/>
        <source>Found a lock file but the owning process isn&apos;t running!
Removing stale lock file.</source>
        <translation>Hittade en låsfil, men processen som äger den körs inte!
Tar bort föråldrad låsfil.</translation>
    </message>
    <message>
        <location filename="../mytharchive/main.cpp" line="212"/>
        <source>Last run did not create a playable DVD.</source>
        <translation>Senaste körningen skapade inte en spelbar DVD.</translation>
    </message>
    <message>
        <location filename="../mytharchive/main.cpp" line="219"/>
        <source>Last run failed to create a DVD.</source>
        <translation>Senaste körningen misslyckades att skapa en DVD.</translation>
    </message>
</context>
<context>
    <name>ArchiveFileSelector</name>
    <message>
        <location filename="../mytharchive/importnative.cpp" line="212"/>
        <source>Find File To Import</source>
        <translation>Välj fil att importera</translation>
    </message>
    <message>
        <location filename="../mytharchive/importnative.cpp" line="272"/>
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
        <location filename="../mytharchive/archivesettings.cpp" line="33"/>
        <source>MythArchive Share Directory</source>
        <translation>Delad katalog för MythArchive</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="36"/>
        <source>Location where MythArchive stores its scripts, intro movies and theme files</source>
        <translation>Platsen där MythArchive lagrar sina skript, intro-filmer och temafiler</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="46"/>
        <source>Video format</source>
        <translation>Videoformat</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="51"/>
        <source>Video format for DVD recordings, PAL or NTSC.</source>
        <translation>Videoformat för DVD-inspelningar, PAL eller NTSC.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="60"/>
        <source>File Selector Filter</source>
        <translation>Filvalsfilter</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="63"/>
        <source>The file name filter to use in the file selector.</source>
        <translation>Filnamnsfilter att använda i filväljaren.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="72"/>
        <source>Location of DVD</source>
        <translation>Plats för DVD</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="75"/>
        <source>Which DVD drive to use when burning discs.</source>
        <translation>Vilken DVD-enhet att använda för att bränna skivor.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="84"/>
        <source>DVD Drive Write Speed</source>
        <translation>Skrivhastighet för DVD</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="87"/>
        <source>This is the write speed to use when burning a DVD. Set to 0 to allow growisofs to choose the fastest available speed.</source>
        <translation>Detta är skrivhastigheten som används vid bränning av DVD. Sätt till 0 för att tillåta growisofs för att välja snabbast tillgängliga hastighet.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="98"/>
        <source>Command to play DVD</source>
        <translation>Kommando för att spela DVD</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="101"/>
        <source>Command to run when test playing a created DVD. &apos;Internal&apos; will use the internal MythTV player. %f will be replaced with the path to the created DVD structure eg. &apos;xine -pfhq --no-splash dvd:/%f&apos;.</source>
        <translation>Kommando att köra för att prova uppspelning av en skapad DVD. &apos;Internal&apos; använder den inbyggda spelaren i MythTV. %f ersätts med sökvägen till den skapade DVD-strukturen, t.ex. &apos;xine -pfhq --no-splash dvd:/%f&apos;.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="114"/>
        <source>Copy remote files</source>
        <translation>Kopiera fjärrfiler</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="117"/>
        <source>If set files on remote filesystems will be copied over to the local filesystem before processing. Speeds processing and reduces bandwidth on the network</source>
        <translation>Om aktiverat, kopieras fjärrfiler till lokalt filsystem innan bearbetning. Snabbar upp bearbetning och minskar bandbredd på nätverket</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="129"/>
        <source>Always Use Mythtranscode</source>
        <translation>Använd alltid Mythtranscode</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="132"/>
        <source>If set mpeg2 files will always be passed though mythtranscode to clean up any errors. May help to fix some audio problems. Ignored if &apos;Use ProjectX&apos; is set.</source>
        <translation>Om aktiverat, kommer mpeg2-filer alltid att skickas genom mythtranscode för att rensa bort eventuella fel. Kan hjälpa till att rätta eventuella ljudproblem. Ignoreras om &apos;Använd ProjectX&apos; är valt.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="144"/>
        <source>Use ProjectX</source>
        <translation>Använd ProjectX</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="147"/>
        <source>If set ProjectX will be used to cut commercials and split mpeg2 files instead of mythtranscode and mythreplex.</source>
        <translation>Om aktiverat, kommer ProjectX att användas för att klippa bort reklam och dela mpeg2-filer, istället för mythtranscode och mythreplex.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="158"/>
        <source>Use FIFOs</source>
        <translation>Använd FIFO:n</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="161"/>
        <source>The script will use FIFOs to pass the output of mplex into dvdauthor rather than creating intermediate files. Saves time and disk space during multiplex operations but not supported on Windows platform</source>
        <translation>Skriptet kommer att använda FIFO:n för att skicka utdata från mplex till dvdauthor, istället för att skapa tillfälliga filer. Sparar både tid och diskutrymme vid multiplexåtgärder, men stöds inte på Windows-plattformen</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="174"/>
        <source>Add Subtitles</source>
        <translation>Lägg till textning</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="177"/>
        <source>If available this option will add subtitles to the final DVD. Requires &apos;Use ProjectX&apos; to be on.</source>
        <translation>Om tillgängligt, kommer valet att lägga till textning på den slutliga DVD:n. Kräver att &apos;Använd ProjectX&apos; är valt.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="187"/>
        <source>Main Menu Aspect Ratio</source>
        <translation>Proportion för huvudmeny</translation>
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
        <translation>Proportion att använda för huvudmenyn.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="203"/>
        <source>Chapter Menu Aspect Ratio</source>
        <translation>Proportion för kapitelmeny</translation>
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
        <translation>Proportion att använda för kapitelmenyn.  &apos;%1&apos; betyder använd samma proportion som video.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="223"/>
        <source>Date format</source>
        <translation>Datumformat</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="226"/>
        <source>Samples are shown using today&apos;s date.</source>
        <translation>Exempel visas med dagens datum.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="232"/>
        <source>Samples are shown using tomorrow&apos;s date.</source>
        <translation>Exempel visas med morgondagens datum.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="250"/>
        <source>Your preferred date format to use on DVD menus. %1</source>
        <extracomment>%1 gives additional info on the date used</extracomment>
        <translation>Det datumformat du föredrar att använda i DVD-menyer. %1</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="259"/>
        <source>Time format</source>
        <translation>Tidsformat</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="266"/>
        <source>Your preferred time format to display on DVD menus. You must choose a format with &quot;AM&quot; or &quot;PM&quot; in it, otherwise your time display will be 24-hour or &quot;military&quot; time.</source>
        <translation>Det tidsformat du föredrar att visa för DVD-menyer. Tidsvisningen är 24-timmars, om du inte väljer ett format med &quot;AM&quot; eller &quot;PM&quot;.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="277"/>
        <source>Default Encoder Profile</source>
        <translation>Omkodarens standardprofil</translation>
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
        <translation>Förvald kodningsprofil att använda om en fil behöver omkodas.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="295"/>
        <source>mplex Command</source>
        <translation>mplex-kommando</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="299"/>
        <source>Command to run mplex</source>
        <translation>Kommando för att köra mplex</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="308"/>
        <source>dvdauthor command</source>
        <translation>dvdauthor-kommando</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="312"/>
        <source>Command to run dvdauthor.</source>
        <translation>Kommando för att köra dvdauthor.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="321"/>
        <source>mkisofs command</source>
        <translation>mkisofs-kommando</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="325"/>
        <source>Command to run mkisofs. (Used to create ISO images)</source>
        <translation>Kommando för att köra mkisofs. (Används för att skapa ISO-avbildningar)</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="334"/>
        <source>growisofs command</source>
        <translation>growisofs-kommando</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="338"/>
        <source>Command to run growisofs. (Used to burn DVDs)</source>
        <translation>Kommando för att köra growisofs. (Används för att bränna DVD:er)</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="347"/>
        <source>M2VRequantiser command</source>
        <translation>M2VRequantiser-kommando</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="351"/>
        <source>Command to run M2VRequantiser. Optional - leave blank if you don&apos;t have M2VRequantiser installed.</source>
        <translation>Kommando för att köra M2VRequantiser. Valfritt - lämna tomt om du inte har M2VRequantiser installerat.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="361"/>
        <source>jpeg2yuv command</source>
        <translation>jpeg2yuv-kommando</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="365"/>
        <source>Command to run jpeg2yuv. Part of mjpegtools package</source>
        <translation>Kommando för att köra jpeg2yuv. En del av mjpegtools-paketet</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="374"/>
        <source>spumux command</source>
        <translation>spumux-kommando</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="378"/>
        <source>Command to run spumux. Part of dvdauthor package</source>
        <translation>Kommando för att köra spumux. En del av dvdauthor-paketet</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="387"/>
        <source>mpeg2enc command</source>
        <translation>mpeg2enc-kommando</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="391"/>
        <source>Command to run mpeg2enc. Part of mjpegtools package</source>
        <translation>Kommando för att köra mpeg2enc. En del av mjpegtools-paketet</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="400"/>
        <source>projectx command</source>
        <translation>projectx kommando</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="404"/>
        <source>Command to run ProjectX. Will be used to cut commercials and split mpegs files instead of mythtranscode and mythreplex.</source>
        <translation>Kommando för att köra ProjectX. Kommer att användas för att klippa bort reklam och dela mpeg-filer, istället för mythtranscode och mythreplex.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="414"/>
        <source>MythArchive Settings</source>
        <translation>MythArchive-inställningar</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="425"/>
        <source>MythArchive Settings (2)</source>
        <translation>MythArchive-inställningar (2)</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="435"/>
        <source>DVD Menu Settings</source>
        <translation>DVD-menyinställningar</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="443"/>
        <source>MythArchive External Commands (1)</source>
        <translation>MythArchive externa kommandon (1)</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="451"/>
        <source>MythArchive External Commands (2)</source>
        <translation>MythArchive externa kommandon (2)</translation>
    </message>
</context>
<context>
    <name>BurnMenu</name>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="1155"/>
        <source>Cannot burn a DVD.
The last run failed to create a DVD.</source>
        <translation>Kan inte bränna en DVD.
Den senaste körningen misslyckades skapa en DVD.</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="1161"/>
        <location filename="../mytharchive/mythburn.cpp" line="1173"/>
        <source>Burn DVD</source>
        <translation>Bränn DVD</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="1162"/>
        <source>
Place a blank DVD in the drive and select an option below.</source>
        <translation>Lägg en tom DVD i brännaren och välj ett alternativ nedan.</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="1174"/>
        <source>Burn DVD Rewritable</source>
        <translation>Bränn återskrivbar DVD</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="1175"/>
        <source>Burn DVD Rewritable (Force Erase)</source>
        <translation>Bränn återskrivbar DVD (tvinga radering)</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="1233"/>
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
        <location filename="../mytharchive/themeselector.cpp" line="219"/>
        <location filename="../mytharchive/themeselector.cpp" line="230"/>
        <source>No theme description file found!</source>
        <translation>Ingen temabeskrivningsfil hittades!</translation>
    </message>
    <message>
        <location filename="../mytharchive/themeselector.cpp" line="243"/>
        <source>Empty theme description!</source>
        <translation>Tom temabeskrivningsfil!</translation>
    </message>
    <message>
        <location filename="../mytharchive/themeselector.cpp" line="248"/>
        <source>Unable to open theme description file!</source>
        <translation>Kan inte öppna temabeskrivningsfilen!</translation>
    </message>
</context>
<context>
    <name>ExportNative</name>
    <message>
        <location filename="../mytharchive/exportnative.cpp" line="242"/>
        <source>You need to add at least one item to archive!</source>
        <translation>Du måste lägga till minst en post att arkivera!</translation>
    </message>
    <message>
        <location filename="../mytharchive/exportnative.cpp" line="400"/>
        <source>Remove Item</source>
        <translation>Ta bort post</translation>
    </message>
    <message>
        <location filename="../mytharchive/exportnative.cpp" line="495"/>
        <source>It was not possible to create the DVD. An error occured when running the scripts</source>
        <translation>Det var inte möjligt att skapa DVD:n. Ett fel uppstod när skripten kördes</translation>
    </message>
    <message>
        <location filename="../mytharchive/exportnative.cpp" line="531"/>
        <source>You don&apos;t have any videos!</source>
        <translation>Du har inga videor!</translation>
    </message>
    <message>
        <location filename="../mytharchive/exportnative.cpp" line="393"/>
        <source>Menu</source>
        <translation>Meny</translation>
    </message>
</context>
<context>
    <name>FileSelector</name>
    <message>
        <location filename="../mytharchive/fileselector.cpp" line="321"/>
        <source>The selected item is not a directory!</source>
        <translation>Vald post är inte en katalog!</translation>
    </message>
    <message>
        <location filename="../mytharchive/fileselector.cpp" line="81"/>
        <source>Find File</source>
        <translation>Sök efter fil</translation>
    </message>
    <message>
        <location filename="../mytharchive/fileselector.cpp" line="84"/>
        <source>Find Directory</source>
        <translation>Sök efter katalog</translation>
    </message>
    <message>
        <location filename="../mytharchive/fileselector.cpp" line="87"/>
        <source>Find Files</source>
        <translation>Sök efter filer</translation>
    </message>
</context>
<context>
    <name>ImportNative</name>
    <message>
        <location filename="../mytharchive/importnative.cpp" line="433"/>
        <source>You need to select a valid channel id!</source>
        <translation>Du måste välja ett giltigt kanal-ID!</translation>
    </message>
    <message>
        <location filename="../mytharchive/importnative.cpp" line="464"/>
        <source>It was not possible to import the Archive.  An error occured when running &apos;mytharchivehelper&apos;</source>
        <translation>Det var inte möjligt att importera arkivet. Ett fel uppstod när &apos;mytharchivehelper&apos; kördes</translation>
    </message>
    <message>
        <location filename="../mytharchive/importnative.cpp" line="594"/>
        <source>Select a channel id</source>
        <translation>Välj ett kanal-ID</translation>
    </message>
    <message>
        <location filename="../mytharchive/importnative.cpp" line="620"/>
        <source>Select a channel number</source>
        <translation>Välj ett kanalnummer</translation>
    </message>
    <message>
        <location filename="../mytharchive/importnative.cpp" line="646"/>
        <source>Select a channel name</source>
        <translation>Välj ett kanalnamn</translation>
    </message>
    <message>
        <location filename="../mytharchive/importnative.cpp" line="672"/>
        <source>Select a Callsign</source>
        <translation>Välj ett stationsnamn</translation>
    </message>
</context>
<context>
    <name>LogViewer</name>
    <message>
        <location filename="../mytharchive/logviewer.cpp" line="367"/>
        <source>Show Progress Log</source>
        <translation>Visa förloppslogg</translation>
    </message>
    <message>
        <location filename="../mytharchive/logviewer.cpp" line="368"/>
        <source>Show Full Log</source>
        <translation>Visa hela loggen</translation>
    </message>
    <message>
        <location filename="../mytharchive/logviewer.cpp" line="363"/>
        <source>Don&apos;t Auto Update</source>
        <translation>Ingen automatisk uppdatering</translation>
    </message>
    <message>
        <location filename="../mytharchive/logviewer.cpp" line="77"/>
        <source>Cannot find any logs to show!</source>
        <translation>Kan inte hitta några loggar att visa!</translation>
    </message>
    <message>
        <location filename="../mytharchive/logviewer.cpp" line="216"/>
        <source>Background creation has been asked to stop.
This may take a few minutes.</source>
        <translation>Bakgrundgenerering har skickats en begäran om att stoppa.
Detta kan ta ett par minuter.</translation>
    </message>
    <message>
        <location filename="../mytharchive/logviewer.cpp" line="365"/>
        <source>Auto Update</source>
        <translation>Automatisk uppdatering</translation>
    </message>
    <message>
        <location filename="../mytharchive/logviewer.cpp" line="355"/>
        <source>Menu</source>
        <translation>Meny</translation>
    </message>
</context>
<context>
    <name>MythBurn</name>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="356"/>
        <location filename="../mytharchive/mythburn.cpp" line="480"/>
        <source>No Cut List</source>
        <translation>Ingen klipplista</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="367"/>
        <source>You need to add at least one item to archive!</source>
        <translation>Du måste lägga till minst en post att arkivera!</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="413"/>
        <source>Retrieving File Information. Please Wait...</source>
        <translation>Hämtar information om filen. Vänta...</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="483"/>
        <source>Encoder: </source>
        <translation>Kodare: </translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="345"/>
        <location filename="../mytharchive/mythburn.cpp" line="469"/>
        <source>Using Cut List</source>
        <translation>Använder klipplista</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="350"/>
        <location filename="../mytharchive/mythburn.cpp" line="474"/>
        <source>Not Using Cut List</source>
        <translation>Använder inte klipplista</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="820"/>
        <source>Don&apos;t Use Cut List</source>
        <translation>Använd inte klipplista</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="823"/>
        <source>Use Cut List</source>
        <translation>Använd klipplista</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="827"/>
        <source>Remove Item</source>
        <translation>Ta bort post</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="828"/>
        <source>Edit Details</source>
        <translation>Redigera information</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="829"/>
        <source>Change Encoding Profile</source>
        <translation>Ändra kodningsprofil</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="830"/>
        <source>Edit Thumbnails</source>
        <translation>Redigera miniatyrbilder</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="965"/>
        <source>It was not possible to create the DVD.  An error occured when running the scripts</source>
        <translation>Det var inte möjligt att skapa DVD. Ett fel uppstod när skripten kördes</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="1008"/>
        <source>You don&apos;t have any videos!</source>
        <translation>Du har inga videor!</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="809"/>
        <source>Menu</source>
        <translation>Meny</translation>
    </message>
</context>
<context>
    <name>MythControls</name>
    <message>
        <location filename="../mytharchive/main.cpp" line="302"/>
        <source>Toggle use cut list state for selected program</source>
        <translation>Växla använd klipplista för valt program</translation>
    </message>
    <message>
        <location filename="../mytharchive/main.cpp" line="305"/>
        <source>Create DVD</source>
        <translation>Skapa DVD</translation>
    </message>
    <message>
        <location filename="../mytharchive/main.cpp" line="307"/>
        <source>Create Archive</source>
        <translation>Skapa arkiv</translation>
    </message>
    <message>
        <location filename="../mytharchive/main.cpp" line="309"/>
        <source>Import Archive</source>
        <translation>Importera arkiv</translation>
    </message>
    <message>
        <location filename="../mytharchive/main.cpp" line="311"/>
        <source>View Archive Log</source>
        <translation>Visa arkiveringslogg</translation>
    </message>
    <message>
        <location filename="../mytharchive/main.cpp" line="313"/>
        <source>Play Created DVD</source>
        <translation>Spela skapad DVD</translation>
    </message>
    <message>
        <location filename="../mytharchive/main.cpp" line="315"/>
        <source>Burn DVD</source>
        <translation>Bränn DVD</translation>
    </message>
</context>
<context>
    <name>RecordingSelector</name>
    <message>
        <location filename="../mytharchive/recordingselector.cpp" line="133"/>
        <source>Retrieving Recording List.
Please Wait...</source>
        <translation>Hämtar inspelningslista.
Vänta...</translation>
    </message>
    <message>
        <location filename="../mytharchive/recordingselector.cpp" line="213"/>
        <source>Clear All</source>
        <translation>Rensa alla</translation>
    </message>
    <message>
        <location filename="../mytharchive/recordingselector.cpp" line="214"/>
        <source>Select All</source>
        <translation>Välj alla</translation>
    </message>
    <message>
        <location filename="../mytharchive/recordingselector.cpp" line="112"/>
        <location filename="../mytharchive/recordingselector.cpp" line="420"/>
        <location filename="../mytharchive/recordingselector.cpp" line="525"/>
        <source>All Recordings</source>
        <translation>Alla inspelningar</translation>
    </message>
    <message>
        <location filename="../mytharchive/recordingselector.cpp" line="157"/>
        <source>Either you don&apos;t have any recordings or no recordings are available locally!</source>
        <translation>Antingen har du inga inspelningar, eller så är inga inspelningar lokalt tillgängliga!</translation>
    </message>
    <message>
        <location filename="../mytharchive/recordingselector.cpp" line="206"/>
        <source>Menu</source>
        <translation>Meny</translation>
    </message>
</context>
<context>
    <name>SelectDestination</name>
    <message>
        <location filename="../mytharchive/archiveutil.cpp" line="35"/>
        <source>Single Layer DVD</source>
        <translation>DVD med ett lager</translation>
    </message>
    <message>
        <location filename="../mytharchive/archiveutil.cpp" line="39"/>
        <source>Dual Layer DVD</source>
        <translation>DVD med två lager</translation>
    </message>
    <message>
        <location filename="../mytharchive/archiveutil.cpp" line="36"/>
        <source>Single Layer DVD (4,482 MB)</source>
        <translation>DVD med ett lager (4482 MiB)</translation>
    </message>
    <message>
        <location filename="../mytharchive/archiveutil.cpp" line="40"/>
        <source>Dual Layer DVD (8,964 MB)</source>
        <translation>DVD med två lager (8964 MiB)</translation>
    </message>
    <message>
        <location filename="../mytharchive/archiveutil.cpp" line="43"/>
        <source>DVD +/- RW</source>
        <translation>DVD +/- RW</translation>
    </message>
    <message>
        <location filename="../mytharchive/archiveutil.cpp" line="44"/>
        <source>Rewritable DVD</source>
        <translation>Återskrivbar DVD</translation>
    </message>
    <message>
        <location filename="../mytharchive/archiveutil.cpp" line="47"/>
        <source>File</source>
        <translation>Fil</translation>
    </message>
    <message>
        <location filename="../mytharchive/archiveutil.cpp" line="48"/>
        <source>Any file accessable from your filesystem.</source>
        <translation>Vilken fil som helst, tillgänglig i filsystemet.</translation>
    </message>
    <message>
        <location filename="../mytharchive/selectdestination.cpp" line="294"/>
        <location filename="../mytharchive/selectdestination.cpp" line="350"/>
        <source>Unknown</source>
        <translation>Okänt</translation>
    </message>
</context>
<context>
    <name>ThemeUI</name>
    <message>
        <location filename="themestrings.h" line="45"/>
        <source>Select Archive Items</source>
        <translation>Välj arkivposter</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="9"/>
        <source>No files are selected for archive</source>
        <translation>Inga filer är valda för arkivet</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="98"/>
        <source>title goes here</source>
        <translation>titel här</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="102"/>
        <source>sep 13, 2004 11:00 pm (1h 15m)</source>
        <translation>13 sep, 2004 23:00 (1tim 15m)</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="96"/>
        <source>description goes here.</source>
        <translation>beskrivning här.</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="104"/>
        <source>x.xx Gb</source>
        <translation>x.xx GiB</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="58"/>
        <source>File Finder To Import</source>
        <translation>Hitta filer att importera</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="13"/>
        <source>0 mb</source>
        <translation>0 MiB</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="14"/>
        <source>xxxxx mb</source>
        <translation>xxxxx MiB</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="47"/>
        <source>Title:</source>
        <translation>Titel:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="48"/>
        <source>Subtitle:</source>
        <translation>Undertitel:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="59"/>
        <source>Start Time:</source>
        <translation>Starttid:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="60"/>
        <source>Select Associated Channel</source>
        <translation>Välj tillhörande kanal</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="61"/>
        <source>Archived Channel</source>
        <translation>Arkiverad kanal</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="62"/>
        <source>Chan. ID:</source>
        <translation>Kanal-ID:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="63"/>
        <source>Chan. No:</source>
        <translation>Kanal-nr:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="64"/>
        <source>Callsign:</source>
        <translation>Stationsnamn:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="65"/>
        <source>Name:</source>
        <translation>Namn:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="66"/>
        <source>Local Channel</source>
        <translation>Lokal kanal</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="18"/>
        <source>Select Destination</source>
        <translation>Välj destination</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="20"/>
        <source>Free Space:</source>
        <translation>Ledigt:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="86"/>
        <source>0.00 GB</source>
        <oldsource>0.00Gb</oldsource>
        <translation>0,00 GiB</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="21"/>
        <source>Make ISO Image</source>
        <translation>Skapa ISO-avbild</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="22"/>
        <source>Burn to DVD</source>
        <translation>Bränn DVD</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="23"/>
        <source>Force Overwrite of DVD-RW Media</source>
        <translation>Tvinga överskrivning av DVD-RW media</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="26"/>
        <source>Select Recordings</source>
        <translation>Välj inspelningar</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="27"/>
        <source>Show Recordings</source>
        <translation>Visa inspelningar</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="29"/>
        <source>File Finder</source>
        <translation>Filbläddrare</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="31"/>
        <source>Select Videos</source>
        <translation>Välj video</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="32"/>
        <source>Video Category</source>
        <translation>Videokategori</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="33"/>
        <source>PL:</source>
        <translation>SL:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="99"/>
        <source>1</source>
        <translation>1</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="34"/>
        <source>No videos available</source>
        <translation>Ingen video tillgänglig</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="35"/>
        <source>Log Viewer</source>
        <translation>Loggvisare</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="38"/>
        <source>Change Encoding Profile</source>
        <translation>Ändra kodningsprofil</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="80"/>
        <source>12.34 GB</source>
        <oldsource>12.34GB</oldsource>
        <translation>12,34 GiB</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="39"/>
        <source>DVD Menu Theme</source>
        <translation>DVD menytema</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="40"/>
        <source>Select a theme</source>
        <translation>Välj ett tema</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="41"/>
        <source>Intro</source>
        <translation>Inledning</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="71"/>
        <source>Choose where you would like your files archived.</source>
        <translation>Välj var du vill lagra dina arkiv.</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="72"/>
        <source>Output Type:</source>
        <translation>Utdatatyp:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="73"/>
        <source>Destination:</source>
        <translation>Destination:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="74"/>
        <source>Click here to find an output location...</source>
        <translation>Klicka här för att hitta en utdataplats...</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="75"/>
        <source>Erase DVD-RW before burning</source>
        <translation>Töm DVD-RW innan bränning</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="15"/>
        <source>Cancel</source>
        <translation>Avbryt</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="76"/>
        <source>Previous</source>
        <translation>Föregående</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="25"/>
        <source>Next</source>
        <translation>Nästa</translation>
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
        <location filename="themestrings.h" line="78"/>
        <source>Select the file you wish to use.</source>
        <translation>Välj den fil som du vill använda.</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="16"/>
        <source>Back</source>
        <translation>Tillbaka</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="30"/>
        <source>Home</source>
        <translation>Hem</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="79"/>
        <source>See logs from your archive runs.</source>
        <translation>Se loggar från dina arkivkörningar.</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="36"/>
        <source>Update</source>
        <translation>Uppdatera</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="37"/>
        <source>Exit</source>
        <translation>Avsluta</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="85"/>
        <source>Up Level</source>
        <translation>Upp nivå</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="103"/>
        <source>x.xx GB</source>
        <translation>x,xx GiB</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="70"/>
        <source>%DATE%, %TIME%</source>
        <translation>%DATE%, %TIME%</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="97"/>
        <source>Video Category:</source>
        <translation>Videokategori:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="81"/>
        <source>Choose the appearance of your DVD.</source>
        <translation>Välj utseende på din DVD.</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="82"/>
        <source>Theme:</source>
        <translation>Tema:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="42"/>
        <source>Main Menu</source>
        <translation>Huvudmeny</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="43"/>
        <source>Chapter Menu</source>
        <translation>Kapitelmeny</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="44"/>
        <source>Details</source>
        <translation>Information</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="83"/>
        <source>Select the recordings and videos you wish to save.</source>
        <translation>Välj de inspelningar och videor du vill spara.</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="10"/>
        <source>Add Recording</source>
        <translation>Lägg till inspelning</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="11"/>
        <source>Add Video</source>
        <translation>Lägg till video</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="12"/>
        <source>Add File</source>
        <translation>Lägg till fil</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="46"/>
        <source>Archive Item Details</source>
        <translation>Information om arkivposten</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="49"/>
        <source>Start Date:</source>
        <translation>Startdatum:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="50"/>
        <source>Time:</source>
        <translation>Tid:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="51"/>
        <source>Description:</source>
        <translation>Beskrivning:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="52"/>
        <source>Thumb Image Selector</source>
        <translation>Miniatyrbildsväljare</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="53"/>
        <source>Current Position</source>
        <translation>Aktuell position</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="84"/>
        <source>0:00:00.00</source>
        <translation>0:00:00.00</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="54"/>
        <source>Seek Amount</source>
        <translation>Söklängd</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="55"/>
        <source>Frame</source>
        <translation>Bild</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="5"/>
        <source>A high quality profile giving approx. 1 hour of video on a single layer DVD</source>
        <translation>En högkvalitativ profil ger ungefär 1 timme video på en enkellagers DVD</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="6"/>
        <source>A standard play profile giving approx. 2 hour of video on a single layer DVD</source>
        <translation>En standard profil ger ungefär 2 timmar video på en enkellagers DVD</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="7"/>
        <source>A long play profile giving approx. 4 hour of video on a single layer DVD</source>
        <translation>En long-play profil ger ungefär 4 timmar video på en enkellagers DVD</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="8"/>
        <source>A extended play profile giving approx. 6 hour of video on a single layer DVD</source>
        <translation>En utökad profil ger ungefär 6 timmar video på en enkellagers DVD</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="56"/>
        <source>Save</source>
        <translation>Spara</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="100"/>
        <source>xxxxx MB</source>
        <translation>xxxxx MiB</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="19"/>
        <source>Find</source>
        <translation>Sök</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="101"/>
        <source>0 MB</source>
        <translation>0 MiB</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="24"/>
        <source>Prev</source>
        <translation>Föregående</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="67"/>
        <source>Search Channel</source>
        <translation>Sök kanal</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="68"/>
        <source>Search Callsign</source>
        <translation>Sök stationsnamn</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="69"/>
        <source>Search Name</source>
        <translation>Sök namn</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="17"/>
        <source>Finish</source>
        <translation>Slutför</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="105"/>
        <source>Select Destination:</source>
        <translation>Välj destination:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="106"/>
        <source>Parental level: %1</source>
        <translation>Föräldranivå: %1</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="107"/>
        <source>Old size:</source>
        <translation>Tidigare storlek:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="108"/>
        <source>New size:</source>
        <translation>Ny storlek:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="109"/>
        <source>Select a theme:</source>
        <translation>Välj ett tema:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="110"/>
        <source>Menu</source>
        <translation>Meny</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="111"/>
        <source>Chapter</source>
        <translation>Kapitel</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="112"/>
        <source>Detail</source>
        <translation>Detaljer</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="113"/>
        <source>Select File to Import</source>
        <translation>Välj fil att importera</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="57"/>
        <source>Add video</source>
        <translation>Lägg till video</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="87"/>
        <source>Filesize: %1</source>
        <translation>Filstorlek: %1</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="88"/>
        <source>Recorded Time: %1</source>
        <translation>Inspelad tid: %1</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="89"/>
        <source>%&quot;|SUBTITLE|&quot; %%(|ORIGINALAIRDATE|) %%(|STARS|) %%DESCRIPTION%</source>
        <translation>%&quot;|SUBTITLE|&quot; %%(|ORIGINALAIRDATE|) %%(|STARS|) %%DESCRIPTION%</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="90"/>
        <source>Ok</source>
        <translation>Ok</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="91"/>
        <source>Parental Level: %1</source>
        <translation>Föräldranivå: %1</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="92"/>
        <source>Archive Items</source>
        <translation>Arkivposter</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="93"/>
        <source>Profile:</source>
        <translation>Profil:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="94"/>
        <source>Old Size:</source>
        <translation>Tidigare storlek:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="95"/>
        <source>New Size:</source>
        <translation>Ny storlek:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="114"/>
        <source>Channel ID:</source>
        <translation>Kanal-ID:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="115"/>
        <source>Channel Number:</source>
        <translation>Kanalnummer:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="116"/>
        <source>Create DVD</source>
        <translation>Skapa DVD</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="117"/>
        <source>Create Archive</source>
        <translation>Skapa arkiv</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="118"/>
        <source>Encode Video File</source>
        <translation>Omkoda videofil</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="119"/>
        <source>Import Archive</source>
        <translation>Importera arkiv</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="120"/>
        <source>Archive Utilities</source>
        <translation>Arkivverktyg</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="121"/>
        <source>Show Log Viewer</source>
        <translation>Loggvisaren</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="122"/>
        <source>Play Created DVD</source>
        <translation>Spela skapad DVD</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="123"/>
        <source>Burn DVD</source>
        <translation>Bränn DVD</translation>
    </message>
</context>
<context>
    <name>ThumbFinder</name>
    <message>
        <location filename="../mytharchive/thumbfinder.cpp" line="922"/>
        <source>Exit, Save Thumbnails</source>
        <translation>Avbryt, spara miniatyrbilder</translation>
    </message>
    <message>
        <location filename="../mytharchive/thumbfinder.cpp" line="923"/>
        <source>Exit, Don&apos;t Save Thumbnails</source>
        <translation>Avbryt, spara inte miniatyrbilder</translation>
    </message>
    <message>
        <location filename="../mytharchive/thumbfinder.cpp" line="915"/>
        <source>Menu</source>
        <translation>Meny</translation>
    </message>
</context>
<context>
    <name>VideoSelector</name>
    <message>
        <location filename="../mytharchive/videoselector.cpp" line="162"/>
        <source>Clear All</source>
        <translation>Rensa alla</translation>
    </message>
    <message>
        <location filename="../mytharchive/videoselector.cpp" line="163"/>
        <source>Select All</source>
        <translation>Välj alla</translation>
    </message>
    <message>
        <location filename="../mytharchive/videoselector.cpp" line="562"/>
        <source>You need to enter a valid password for this parental level</source>
        <translation>Du måste ange ett giltigt lösenord för den här barnlåsnivån</translation>
    </message>
    <message>
        <location filename="../mytharchive/videoselector.cpp" line="368"/>
        <location filename="../mytharchive/videoselector.cpp" line="505"/>
        <source>All Videos</source>
        <translation>Alla videor</translation>
    </message>
    <message>
        <location filename="../mytharchive/videoselector.cpp" line="155"/>
        <source>Menu</source>
        <translation>Meny</translation>
    </message>
</context>
</TS>
