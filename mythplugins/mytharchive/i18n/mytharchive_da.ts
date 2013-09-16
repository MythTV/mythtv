<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE TS>
<TS version="2.0" language="da_DK">
<context>
    <name>(ArchiveUtils)</name>
    <message>
        <location filename="../mytharchive/archiveutil.cpp" line="80"/>
        <source>Cannot find the MythArchive work directory.
Have you set the correct path in the settings?</source>
        <translation>Det er ikke muligt at finde MythArchive&apos;s arbejdsbibliotek.
Har du sat den korrekte sti i indstillingerne?</translation>
    </message>
</context>
<context>
    <name>(MythArchiveMain)</name>
    <message>
        <location filename="../mytharchive/main.cpp" line="93"/>
        <source>Found a lock file but the owning process isn&apos;t running!
Removing stale lock file.</source>
        <translation>Fandt en låse-fil, men den tilhørende process kører ikke!
Fjerner ugyldig låse-fil.</translation>
    </message>
    <message>
        <location filename="../mytharchive/main.cpp" line="212"/>
        <source>Last run did not create a playable DVD.</source>
        <translation>Sidste kørsel oprettede ikke en DVD der kan afspilles.</translation>
    </message>
    <message>
        <location filename="../mytharchive/main.cpp" line="219"/>
        <source>Last run failed to create a DVD.</source>
        <translation>Sidste kørsel fejlede ved oprettelse af DVD&apos;en.</translation>
    </message>
</context>
<context>
    <name>ArchiveFileSelector</name>
    <message>
        <location filename="../mytharchive/importnative.cpp" line="212"/>
        <source>Find File To Import</source>
        <translation>Vælg fil som skal importeres</translation>
    </message>
    <message>
        <location filename="../mytharchive/importnative.cpp" line="272"/>
        <source>The selected item is not a valid archive file!</source>
        <translation>Den valgte fil er ikke en gyldig arkiv-fil!</translation>
    </message>
</context>
<context>
    <name>ArchiveSettings</name>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="20"/>
        <source>MythArchive Temp Directory</source>
        <translation>MythArchive temp-mappe</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="23"/>
        <source>Location where MythArchive should create its temporary work files. LOTS of free space required here.</source>
        <translation>Sti hvor MythArchive skal oprette sine midlertidige arbejdsfiler. Der kræves MEGET fri plads her.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="33"/>
        <source>MythArchive Share Directory</source>
        <translation>MythArchive dele-mappe</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="36"/>
        <source>Location where MythArchive stores its scripts, intro movies and theme files</source>
        <translation>Sti hvor MythArchive gemmer scripts, intro-film og temafiler</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="46"/>
        <source>Video format</source>
        <translation>Videoformat</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="51"/>
        <source>Video format for DVD recordings, PAL or NTSC.</source>
        <translation>Videoformat for DVD optagelser (PAL eller NTSC).</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="60"/>
        <source>File Selector Filter</source>
        <translation>Filvælger-filter</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="63"/>
        <source>The file name filter to use in the file selector.</source>
        <translation>Filteret der anvendes i filvælgeren.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="72"/>
        <source>Location of DVD</source>
        <translation>Sti til dvd</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="75"/>
        <source>Which DVD drive to use when burning discs.</source>
        <translation>DVD-drevet der skal anvendes ved brænding.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="84"/>
        <source>DVD Drive Write Speed</source>
        <translation>Skrivehastighed for DVD-drev</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="87"/>
        <source>This is the write speed to use when burning a DVD. Set to 0 to allow growisofs to choose the fastest available speed.</source>
        <translation>Dette er skrivehastigheden under brænding af en DVD. Sæt den til 0 for at tillade growisofs at vælge den hurtigst tilgængelige hastighed.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="98"/>
        <source>Command to play DVD</source>
        <translation>Kommando for at afspille en DVD</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="101"/>
        <source>Command to run when test playing a created DVD. &apos;Internal&apos; will use the internal MythTV player. %f will be replaced with the path to the created DVD structure eg. &apos;xine -pfhq --no-splash dvd:/%f&apos;.</source>
        <translation>Kommando for at foretage en testafspilning af en ny dvd. &apos;Intern&apos; vil benytte MythTVs indbyggede afspiller. %f vil blive erstattet af stien til den oprettede DVD-struktur, f.eks. &apos;xine -pfhq --no-splash dvd:/%f&apos;.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="114"/>
        <source>Copy remote files</source>
        <translation>Kopier ikke-lokale filer</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="117"/>
        <source>If set files on remote filesystems will be copied over to the local filesystem before processing. Speeds processing and reduces bandwidth on the network</source>
        <translation>Hvis valgt, vil filer på andre maskiner blive kopieret til det lokale filsystem før behandling. Dette gør behandlingen hurtigere og sparer båndbredde på netværket</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="129"/>
        <source>Always Use Mythtranscode</source>
        <translation>Anvend altid Mythtranscode</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="132"/>
        <source>If set mpeg2 files will always be passed though mythtranscode to clean up any errors. May help to fix some audio problems. Ignored if &apos;Use ProjectX&apos; is set.</source>
        <translation>Hvis valgt, vil MPEG2 filer altid blive sendt igennem mythtranscode for at rette eventuelle fejl. Dette kan afhjælpe visse lydproblemer. Ignoreres hvis &apos;Anvend ProjectX&apos; er valgt.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="144"/>
        <source>Use ProjectX</source>
        <translation>Anvend ProjectX</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="147"/>
        <source>If set ProjectX will be used to cut commercials and split mpeg2 files instead of mythtranscode and mythreplex.</source>
        <translation>Hvis valgt, anvendes ProjectX til at fjerne reklamer og opdele MPEG2 filer i stedet for mythtranscode og mythreplex.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="158"/>
        <source>Use FIFOs</source>
        <translation>Anvend FIFOer</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="161"/>
        <source>The script will use FIFOs to pass the output of mplex into dvdauthor rather than creating intermediate files. Saves time and disk space during multiplex operations but not supported on Windows platform</source>
        <translation>Scriptet vil benytte FIFOer til at videreføre output fra mplex ind i dvdauthor fremfor at oprette midlertidige filer. Sparer tid og diskplads under multiplex-operationer men supporteres ikke på Windows platforme</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="174"/>
        <source>Add Subtitles</source>
        <translation>Tilføj undertekster</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="177"/>
        <source>If available this option will add subtitles to the final DVD. Requires &apos;Use ProjectX&apos; to be on.</source>
        <translation>Hvis valgt, tilføjes der undertekster til den færdige DVD. Kræver at &apos;Anvend ProjectX&apos; er valgt.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="187"/>
        <source>Main Menu Aspect Ratio</source>
        <translation>Aspektforhold for hovedmenu</translation>
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
        <translation>Det aspektforhold der anvendes ved oprettelse af hovedmenuen.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="203"/>
        <source>Chapter Menu Aspect Ratio</source>
        <translation>Aspektforhold for kapitelmenu</translation>
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
        <translation>Aspektforhold som benyttes til kapitelmenu. &apos;%1&apos; betyder at aspektforholdet fra videoen bibeholdes.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="223"/>
        <source>Date format</source>
        <translation>Datoformat</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="226"/>
        <source>Samples are shown using today&apos;s date.</source>
        <translation>Eksempler vises med datoen i dag.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="232"/>
        <source>Samples are shown using tomorrow&apos;s date.</source>
        <translation>Eksempler vises med datoen i morgen.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="250"/>
        <source>Your preferred date format to use on DVD menus. %1</source>
        <extracomment>%1 gives additional info on the date used</extracomment>
        <translation>Dit foretrukne datoformat som skal benyttes på DVD menuer. %1</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="259"/>
        <source>Time format</source>
        <translation>Tidsformat</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="266"/>
        <source>Your preferred time format to display on DVD menus. You must choose a format with &quot;AM&quot; or &quot;PM&quot; in it, otherwise your time display will be 24-hour or &quot;military&quot; time.</source>
        <translation>Dit foretrukne tidsformat i DVD-menuer. Hvis du ikke vælger et format med &apos;AM&apos; eller &apos;PM&apos;, benyttes der 24-timers tidsformat.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="277"/>
        <source>Default Encoder Profile</source>
        <translation>Standard encoderprofil</translation>
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
        <translation>Standard encoderprofil som benyttes, hvis re-encoding er nødvendig.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="295"/>
        <source>mplex Command</source>
        <translation>mplex-kommando</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="299"/>
        <source>Command to run mplex</source>
        <translation>Kommando for at køre mplex</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="308"/>
        <source>dvdauthor command</source>
        <translation>dvdauthor-kommando</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="312"/>
        <source>Command to run dvdauthor.</source>
        <translation>Kommando for at køre dvdauthor.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="321"/>
        <source>mkisofs command</source>
        <translation>mkisofs-kommando</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="325"/>
        <source>Command to run mkisofs. (Used to create ISO images)</source>
        <translation>Kommando for at køre mkisofs (Anvendes til at oprette ISO images)</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="334"/>
        <source>growisofs command</source>
        <translation>growisofs-kommando</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="338"/>
        <source>Command to run growisofs. (Used to burn DVDs)</source>
        <translation>Kommando for at køre growisofs (Anvendes til at brænde DVDer)</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="347"/>
        <source>M2VRequantiser command</source>
        <translation>M2VRequantiser kommando</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="351"/>
        <source>Command to run M2VRequantiser. Optional - leave blank if you don&apos;t have M2VRequantiser installed.</source>
        <translation>Kommando til at køre M2VRequantiser. Frivillig - efterlad blank hvis du ikke har M2VRequantiser installeret.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="361"/>
        <source>jpeg2yuv command</source>
        <translation>jpeg2yuv-kommando</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="365"/>
        <source>Command to run jpeg2yuv. Part of mjpegtools package</source>
        <translation>Kommando for at køre jpeg2yuv. En del af mjpegtools-pakken</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="374"/>
        <source>spumux command</source>
        <translation>spumux-kommando</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="378"/>
        <source>Command to run spumux. Part of dvdauthor package</source>
        <translation>Kommando for at køre spumux. En del af dvdauthor-pakken</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="387"/>
        <source>mpeg2enc command</source>
        <translation>mpeg2enc-kommando</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="391"/>
        <source>Command to run mpeg2enc. Part of mjpegtools package</source>
        <translation>Kommando for at køre mpeg2enc. En del af mjpegtools-pakken</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="400"/>
        <source>projectx command</source>
        <translation>projectx-kommando</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="404"/>
        <source>Command to run ProjectX. Will be used to cut commercials and split mpegs files instead of mythtranscode and mythreplex.</source>
        <translation>Kommando for at køre ProjectX. Anvendes til at fjerne reklamer og opdele MPEG filer i stedet for mythtranscode og mythreplex.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="414"/>
        <source>MythArchive Settings</source>
        <translation>MythArchive indstillinger</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="425"/>
        <source>MythArchive Settings (2)</source>
        <translation>MythArchive indstillinger (2)</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="435"/>
        <source>DVD Menu Settings</source>
        <translation>Indstillinger for DVD-menu</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="443"/>
        <source>MythArchive External Commands (1)</source>
        <translation>MythArchive eksterne kommandoer (1)</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="451"/>
        <source>MythArchive External Commands (2)</source>
        <translation>MythArchive eksterne kommandoer (2)</translation>
    </message>
</context>
<context>
    <name>BurnMenu</name>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="1155"/>
        <source>Cannot burn a DVD.
The last run failed to create a DVD.</source>
        <translation>Kan ikke brænde DVD&apos;en.
Den sidste kørsel fejlede ved oprettelse af DVD&apos;en.</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="1161"/>
        <location filename="../mytharchive/mythburn.cpp" line="1173"/>
        <source>Burn DVD</source>
        <translation>Brænd DVD</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="1162"/>
        <source>
Place a blank DVD in the drive and select an option below.</source>
        <translation>
Indsæt en blank DVD i drevet og vælg en af mulighederne nedenfor.</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="1174"/>
        <source>Burn DVD Rewritable</source>
        <translation>Brænd på en genskrivnings-DVD</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="1175"/>
        <source>Burn DVD Rewritable (Force Erase)</source>
        <translation>Brænd på en genkskrivnings-DVD (Gennemtving sletning)</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="1233"/>
        <source>It was not possible to run mytharchivehelper to burn the DVD.</source>
        <translation>Det var ikke muligt at køre mytharchivehelper for at brænde DVD&apos;en.</translation>
    </message>
</context>
<context>
    <name>BurnThemeUI</name>
    <message>
        <location filename="../mythburn/themes/burnthemestrings.h" line="5"/>
        <source>Has an intro and contains a main menu with 4 recordings per page. Does not have a chapter selection submenu.</source>
        <translation>Har en intro og indeholder en hovedmenu med 4 optagelser pr. side. Indeholder ikke en undermenu til valg af kapitel.</translation>
    </message>
    <message>
        <location filename="../mythburn/themes/burnthemestrings.h" line="6"/>
        <source>Has an intro and contains a summary main menu with 10 recordings per page. Does not have a chapter selection submenu, recording titles, dates or category.</source>
        <translation>Har en intro og indeholder en hovedmenu med 10 optagelser pr. side. Indeholder ikke kapitel-undermenu, titler på optagelser, datoer eller kategori.</translation>
    </message>
    <message>
        <location filename="../mythburn/themes/burnthemestrings.h" line="7"/>
        <source>Has an intro and contains a main menu with 6 recordings per page. Does not have a scene selection submenu.</source>
        <translation>Har en intro og indeholder en hovedmenu med 6 optagelser pr. side. Indeholder ikke en undermenu til valg af scene.</translation>
    </message>
    <message>
        <location filename="../mythburn/themes/burnthemestrings.h" line="8"/>
        <source>Has an intro and contains a main menu with 3 recordings per page and a scene selection submenu with 8 chapters points. Shows a program details page before each recording.</source>
        <translation>Har en intro og indeholder en hovedmenu med 3 optagelser pr. side samt en undermenu til valg af scene med 8 punkter. Viser en side med programdetaljer for hver optagelse.</translation>
    </message>
    <message>
        <location filename="../mythburn/themes/burnthemestrings.h" line="9"/>
        <source>Has an intro and contains a main menu with 3 recordings per page and a scene selection submenu with 8 chapters points. Shows a program details page before each recording. Uses animated thumb images.</source>
        <translation>Har en intro og indeholder en hovedmenu med 3 optagelser pr. side samt en undermenu til valg af scene med 8 punkter. Viser en side med programdetaljer for hver optagelse. Benytter animerede mini-billeder.</translation>
    </message>
    <message>
        <location filename="../mythburn/themes/burnthemestrings.h" line="10"/>
        <source>Has an intro and contains a main menu with 3 recordings per page and a scene selection submenu with 8 chapters points.</source>
        <translation>Har en intro og indeholder en hovedmenu med 3 optagelser pr. side samt en undermenu til valg af scene med 8 punkter.</translation>
    </message>
    <message>
        <location filename="../mythburn/themes/burnthemestrings.h" line="11"/>
        <source>Has an intro and contains a main menu with 3 recordings per page and a scene selection submenu with 8 chapters points. All the thumb images are animated.</source>
        <translation>Har en intro og indeholder en hovedmenu med 3 optagelser pr. side samt en undermenu til valg af scene med 8 punkter. Benytter animerede mini-billeder.</translation>
    </message>
    <message>
        <location filename="../mythburn/themes/burnthemestrings.h" line="12"/>
        <source>Creates an auto play DVD with no menus. Shows an intro movie then for each title shows a details page followed by the video in sequence.</source>
        <translation>Opretter en DVD uden menuer. Viser en introduktionsvideo og for hver titel vises en detaljeside efterfulgt af den pågældende video.</translation>
    </message>
    <message>
        <location filename="../mythburn/themes/burnthemestrings.h" line="13"/>
        <source>Creates an auto play DVD with no menus and no intro.</source>
        <translation>Opretter en DVD uden menuer og uden intro.</translation>
    </message>
</context>
<context>
    <name>DVDThemeSelector</name>
    <message>
        <location filename="../mytharchive/themeselector.cpp" line="219"/>
        <location filename="../mytharchive/themeselector.cpp" line="230"/>
        <source>No theme description file found!</source>
        <translation>Fil med temabeskrivelse kunne ikke findes!</translation>
    </message>
    <message>
        <location filename="../mytharchive/themeselector.cpp" line="243"/>
        <source>Empty theme description!</source>
        <translation>Temabeskrivelsen er tom!</translation>
    </message>
    <message>
        <location filename="../mytharchive/themeselector.cpp" line="248"/>
        <source>Unable to open theme description file!</source>
        <translation>Ikke i stand til at åbne fil med temabeskrivelse!</translation>
    </message>
</context>
<context>
    <name>ExportNative</name>
    <message>
        <location filename="../mytharchive/exportnative.cpp" line="242"/>
        <source>You need to add at least one item to archive!</source>
        <translation>Du skal tilføje mindst en fil til arkivering!</translation>
    </message>
    <message>
        <location filename="../mytharchive/exportnative.cpp" line="400"/>
        <source>Remove Item</source>
        <translation>Fjern</translation>
    </message>
    <message>
        <location filename="../mytharchive/exportnative.cpp" line="495"/>
        <source>It was not possible to create the DVD. An error occured when running the scripts</source>
        <translation>Det var ikke muligt at oprette DVD&apos;en. Der opstod en fejl ved kørsel af scripts</translation>
    </message>
    <message>
        <location filename="../mytharchive/exportnative.cpp" line="531"/>
        <source>You don&apos;t have any videos!</source>
        <translation>Du har ingen videoer!</translation>
    </message>
    <message>
        <location filename="../mytharchive/exportnative.cpp" line="393"/>
        <source>Menu</source>
        <translation>Menu</translation>
    </message>
</context>
<context>
    <name>FileSelector</name>
    <message>
        <location filename="../mytharchive/fileselector.cpp" line="321"/>
        <source>The selected item is not a directory!</source>
        <translation>Det valgte objekt er ikke en mappe!</translation>
    </message>
    <message>
        <location filename="../mytharchive/fileselector.cpp" line="81"/>
        <source>Find File</source>
        <translation>Find fil</translation>
    </message>
    <message>
        <location filename="../mytharchive/fileselector.cpp" line="84"/>
        <source>Find Directory</source>
        <translation>Find bibliotek</translation>
    </message>
    <message>
        <location filename="../mytharchive/fileselector.cpp" line="87"/>
        <source>Find Files</source>
        <translation>Find filer</translation>
    </message>
</context>
<context>
    <name>ImportNative</name>
    <message>
        <location filename="../mytharchive/importnative.cpp" line="433"/>
        <source>You need to select a valid channel id!</source>
        <translation>Du skal vælge et gyldigt kanal-ID!</translation>
    </message>
    <message>
        <location filename="../mytharchive/importnative.cpp" line="464"/>
        <source>It was not possible to import the Archive.  An error occured when running &apos;mytharchivehelper&apos;</source>
        <translation>Det var ikke muligt at importere arkivet.  Der opstod en fejl under kørsel af &apos;mytharchivehelper&apos;</translation>
    </message>
    <message>
        <location filename="../mytharchive/importnative.cpp" line="594"/>
        <source>Select a channel id</source>
        <translation>Vælg et kanal-ID</translation>
    </message>
    <message>
        <location filename="../mytharchive/importnative.cpp" line="620"/>
        <source>Select a channel number</source>
        <translation>Vælg et kanalnummer</translation>
    </message>
    <message>
        <location filename="../mytharchive/importnative.cpp" line="646"/>
        <source>Select a channel name</source>
        <translation>Vælg et kanalnavn</translation>
    </message>
    <message>
        <location filename="../mytharchive/importnative.cpp" line="672"/>
        <source>Select a Callsign</source>
        <translation>Vælg et kaldenavn</translation>
    </message>
</context>
<context>
    <name>LogViewer</name>
    <message>
        <location filename="../mytharchive/logviewer.cpp" line="367"/>
        <source>Show Progress Log</source>
        <translation>Vis log for fremskridt</translation>
    </message>
    <message>
        <location filename="../mytharchive/logviewer.cpp" line="368"/>
        <source>Show Full Log</source>
        <translation>Vis fuld log</translation>
    </message>
    <message>
        <location filename="../mytharchive/logviewer.cpp" line="363"/>
        <source>Don&apos;t Auto Update</source>
        <translation>Opdater ikke automatisk</translation>
    </message>
    <message>
        <location filename="../mytharchive/logviewer.cpp" line="77"/>
        <source>Cannot find any logs to show!</source>
        <translation>Kan ikke finde nogen logfiler til visning!</translation>
    </message>
    <message>
        <location filename="../mytharchive/logviewer.cpp" line="216"/>
        <source>Background creation has been asked to stop.
This may take a few minutes.</source>
        <translation>Baggrundsprocessen stoppes.
Dette kan tage et par minutter.</translation>
    </message>
    <message>
        <location filename="../mytharchive/logviewer.cpp" line="365"/>
        <source>Auto Update</source>
        <translation>Opdater automatisk</translation>
    </message>
    <message>
        <location filename="../mytharchive/logviewer.cpp" line="355"/>
        <source>Menu</source>
        <translation>Menu</translation>
    </message>
</context>
<context>
    <name>MythBurn</name>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="345"/>
        <location filename="../mytharchive/mythburn.cpp" line="469"/>
        <source>Using Cut List</source>
        <translation>Anvender klippeliste</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="350"/>
        <location filename="../mytharchive/mythburn.cpp" line="474"/>
        <source>Not Using Cut List</source>
        <translation>Anvender ikke klippeliste</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="356"/>
        <location filename="../mytharchive/mythburn.cpp" line="480"/>
        <source>No Cut List</source>
        <translation>Ingen klippeliste</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="367"/>
        <source>You need to add at least one item to archive!</source>
        <translation>Du skal tilføje mindst en fil til arkivering!</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="413"/>
        <source>Retrieving File Information. Please Wait...</source>
        <translation>Modtager information for fil. Vent venligst...</translation>
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
        <location filename="../mytharchive/mythburn.cpp" line="820"/>
        <source>Don&apos;t Use Cut List</source>
        <translation>Anvend ikke klippeliste</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="823"/>
        <source>Use Cut List</source>
        <translation>Anvend klippeliste</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="827"/>
        <source>Remove Item</source>
        <translation>Fjern</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="828"/>
        <source>Edit Details</source>
        <translation>Rediger detaljer</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="829"/>
        <source>Change Encoding Profile</source>
        <translation>Skift encodingsprofil</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="830"/>
        <source>Edit Thumbnails</source>
        <translation>Rediger thumbnails</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="965"/>
        <source>It was not possible to create the DVD.  An error occured when running the scripts</source>
        <translation>Det var ikke muligt at oprette DVD&apos;en.  Der opstod en fejl under kørsel af scripts</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="1008"/>
        <source>You don&apos;t have any videos!</source>
        <translation>Du har ingen videoer!</translation>
    </message>
</context>
<context>
    <name>MythControls</name>
    <message>
        <location filename="../mytharchive/main.cpp" line="302"/>
        <source>Toggle use cut list state for selected program</source>
        <translation>Skift anvendelse af klippeliste for det valgte program</translation>
    </message>
    <message>
        <location filename="../mytharchive/main.cpp" line="305"/>
        <source>Create DVD</source>
        <translation>Opret DVD</translation>
    </message>
    <message>
        <location filename="../mytharchive/main.cpp" line="307"/>
        <source>Create Archive</source>
        <translation>Opret arkiv</translation>
    </message>
    <message>
        <location filename="../mytharchive/main.cpp" line="309"/>
        <source>Import Archive</source>
        <translation>Importer arkiv</translation>
    </message>
    <message>
        <location filename="../mytharchive/main.cpp" line="311"/>
        <source>View Archive Log</source>
        <translation>Vis arkivlog</translation>
    </message>
    <message>
        <location filename="../mytharchive/main.cpp" line="313"/>
        <source>Play Created DVD</source>
        <translation>Afspil nyoprettet DVD</translation>
    </message>
    <message>
        <location filename="../mytharchive/main.cpp" line="315"/>
        <source>Burn DVD</source>
        <translation>Brænd DVD</translation>
    </message>
</context>
<context>
    <name>RecordingSelector</name>
    <message>
        <location filename="../mytharchive/recordingselector.cpp" line="133"/>
        <source>Retrieving Recording List.
Please Wait...</source>
        <translation>Modtager optagelseslisten.
Vent venligst...</translation>
    </message>
    <message>
        <location filename="../mytharchive/recordingselector.cpp" line="213"/>
        <source>Clear All</source>
        <translation>Slet alt</translation>
    </message>
    <message>
        <location filename="../mytharchive/recordingselector.cpp" line="214"/>
        <source>Select All</source>
        <translation>Vælg alt</translation>
    </message>
    <message>
        <location filename="../mytharchive/recordingselector.cpp" line="112"/>
        <location filename="../mytharchive/recordingselector.cpp" line="420"/>
        <location filename="../mytharchive/recordingselector.cpp" line="525"/>
        <source>All Recordings</source>
        <translation>Alle optagelser</translation>
    </message>
    <message>
        <location filename="../mytharchive/recordingselector.cpp" line="157"/>
        <source>Either you don&apos;t have any recordings or no recordings are available locally!</source>
        <translation>Enten har du ikke nogen optagelser eller også er de ikke lokalt tilgængelige!</translation>
    </message>
    <message>
        <location filename="../mytharchive/recordingselector.cpp" line="206"/>
        <source>Menu</source>
        <translation>Menu</translation>
    </message>
</context>
<context>
    <name>SelectDestination</name>
    <message>
        <location filename="../mytharchive/archiveutil.cpp" line="35"/>
        <source>Single Layer DVD</source>
        <translation>Enkeltlags DVD</translation>
    </message>
    <message>
        <location filename="../mytharchive/archiveutil.cpp" line="39"/>
        <source>Dual Layer DVD</source>
        <translation>Dobbeltlags DVD</translation>
    </message>
    <message>
        <location filename="../mytharchive/archiveutil.cpp" line="36"/>
        <source>Single Layer DVD (4,482 MB)</source>
        <translation>Enkeltlags DVD (4482MB)</translation>
    </message>
    <message>
        <location filename="../mytharchive/archiveutil.cpp" line="40"/>
        <source>Dual Layer DVD (8,964 MB)</source>
        <translation>Dobbeltlags DVD (8964MB)</translation>
    </message>
    <message>
        <location filename="../mytharchive/archiveutil.cpp" line="43"/>
        <source>DVD +/- RW</source>
        <translation>DVD +/- RW</translation>
    </message>
    <message>
        <location filename="../mytharchive/archiveutil.cpp" line="44"/>
        <source>Rewritable DVD</source>
        <translation>Genskrivnings-DVD</translation>
    </message>
    <message>
        <location filename="../mytharchive/archiveutil.cpp" line="47"/>
        <source>File</source>
        <translation>Fil</translation>
    </message>
    <message>
        <location filename="../mytharchive/archiveutil.cpp" line="48"/>
        <source>Any file accessable from your filesystem.</source>
        <translation>Alle tilgængelige filer fra dit filsystem.</translation>
    </message>
    <message>
        <location filename="../mytharchive/selectdestination.cpp" line="294"/>
        <location filename="../mytharchive/selectdestination.cpp" line="350"/>
        <source>Unknown</source>
        <translation>ukendt</translation>
    </message>
</context>
<context>
    <name>ThemeUI</name>
    <message>
        <location filename="themestrings.h" line="18"/>
        <source>Select Destination</source>
        <translation>Vælg destination</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="96"/>
        <source>description goes here.</source>
        <translation>beskrivelse skrives her.</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="20"/>
        <source>Free Space:</source>
        <translation>Ledig plads:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="21"/>
        <source>Make ISO Image</source>
        <translation>Opret ISO image</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="22"/>
        <source>Burn to DVD</source>
        <translation>Brænd til DVD</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="23"/>
        <source>Force Overwrite of DVD-RW Media</source>
        <translation>Gennemtving overskrivning af DVD-RW medie</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="26"/>
        <source>Select Recordings</source>
        <translation>Vælg optagelser</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="27"/>
        <source>Show Recordings</source>
        <translation>Vis optagelser</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="29"/>
        <source>File Finder</source>
        <translation>Fil-finder</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="31"/>
        <source>Select Videos</source>
        <translation>Vælg videoer</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="32"/>
        <source>Video Category</source>
        <translation>Video kategori</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="98"/>
        <source>title goes here</source>
        <translation>titlen skrives her</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="104"/>
        <source>x.xx Gb</source>
        <translation>x,xx GB</translation>
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
        <location filename="themestrings.h" line="34"/>
        <source>No videos available</source>
        <translation>Der er ingen tilgængelige videoer</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="35"/>
        <source>Log Viewer</source>
        <translation>Log-viser</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="38"/>
        <source>Change Encoding Profile</source>
        <translation>Skift encoding profil</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="39"/>
        <source>DVD Menu Theme</source>
        <translation>DVD menu tema</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="40"/>
        <source>Select a theme</source>
        <translation>Vælg et tema</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="41"/>
        <source>Intro</source>
        <translation>Introduktion</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="90"/>
        <source>Ok</source>
        <translation>Ok</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="42"/>
        <source>Main Menu</source>
        <translation>Hovedmenu</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="43"/>
        <source>Chapter Menu</source>
        <translation>Kapitelmenu</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="44"/>
        <source>Details</source>
        <translation>Detaljer</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="45"/>
        <source>Select Archive Items</source>
        <translation>Vælg arkivemner</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="9"/>
        <source>No files are selected for archive</source>
        <translation>Der er ikke valgt nogen filer til arkivering</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="46"/>
        <source>Archive Item Details</source>
        <translation>Arkiv emne detaljer</translation>
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
        <location filename="themestrings.h" line="49"/>
        <source>Start Date:</source>
        <translation>Start dato:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="50"/>
        <source>Time:</source>
        <translation>Tid:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="51"/>
        <source>Description:</source>
        <translation>Beskrivelse:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="52"/>
        <source>Thumb Image Selector</source>
        <translation>Mini-billede vælger</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="53"/>
        <source>Current Position</source>
        <translation>Aktuel position</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="84"/>
        <source>0:00:00.00</source>
        <translation>0:00:00,00</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="54"/>
        <source>Seek Amount</source>
        <translation>Søgnings omfang</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="55"/>
        <source>Frame</source>
        <translation>Billede</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="100"/>
        <source>xxxxx MB</source>
        <translation>xxxxx MB</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="101"/>
        <source>0 MB</source>
        <translation>0 MB</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="58"/>
        <source>File Finder To Import</source>
        <translation>Fil-finder til import</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="59"/>
        <source>Start Time:</source>
        <translation>Start tid:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="60"/>
        <source>Select Associated Channel</source>
        <translation>Vælg tilknyttet kanal</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="61"/>
        <source>Archived Channel</source>
        <translation>Arkiveret kanal</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="62"/>
        <source>Chan. ID:</source>
        <translation>Kanal ID:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="63"/>
        <source>Chan. No:</source>
        <translation>Kanal nr:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="64"/>
        <source>Callsign:</source>
        <translation>Kaldenavn:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="65"/>
        <source>Name:</source>
        <translation>Navn:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="66"/>
        <source>Local Channel</source>
        <translation>Lokal kanal</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="5"/>
        <source>A high quality profile giving approx. 1 hour of video on a single layer DVD</source>
        <translation>En profil med høj kvalitet giver ca. 1 times video på en enkeltlags DVD</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="6"/>
        <source>A standard play profile giving approx. 2 hour of video on a single layer DVD</source>
        <translation>En profil med almindelig kvalitet giver ca. 2 times video på en enkeltlags DVD</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="7"/>
        <source>A long play profile giving approx. 4 hour of video on a single layer DVD</source>
        <translation>En profil med lav kvalitet giver ca. 4 times video på en enkeltlags DVD</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="8"/>
        <source>A extended play profile giving approx. 6 hour of video on a single layer DVD</source>
        <translation>En profil med ekstra lav kvalitet giver ca. 6 times video på en enkeltlags DVD</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="13"/>
        <source>0 mb</source>
        <translation>0 mb</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="14"/>
        <source>xxxxx mb</source>
        <translation>xxxxx mb</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="85"/>
        <source>Up Level</source>
        <translation>Gå niveau op</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="87"/>
        <source>Filesize: %1</source>
        <translation>Filestørrelse: %1</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="88"/>
        <source>Recorded Time: %1</source>
        <translation>Optaget tid: %1</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="89"/>
        <source>%&quot;|SUBTITLE|&quot; %%(|ORIGINALAIRDATE|) %%(|STARS|) %%DESCRIPTION%</source>
        <translation>%&quot;|SUBTITLE|&quot; %%(|ORIGINALAIRDATE|) %%(|STARS|) %%DESCRIPTION%</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="91"/>
        <source>Parental Level: %1</source>
        <translation>Forældreniveau: %1</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="92"/>
        <source>Archive Items</source>
        <translation>Arkiver emner</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="93"/>
        <source>Profile:</source>
        <translation>Profil:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="94"/>
        <source>Old Size:</source>
        <translation>Gammel str:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="95"/>
        <source>New Size:</source>
        <translation>Ny str:</translation>
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
        <location filename="themestrings.h" line="105"/>
        <source>Select Destination:</source>
        <translation>Vælg destination:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="106"/>
        <source>Parental level: %1</source>
        <translation>Forældreniveau: %1</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="107"/>
        <source>Old size:</source>
        <translation>Gammel str:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="108"/>
        <source>New size:</source>
        <translation>Ny str:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="109"/>
        <source>Select a theme:</source>
        <translation>Vælg et tema:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="110"/>
        <source>Menu</source>
        <translation>Menu</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="111"/>
        <source>Chapter</source>
        <translation>Kapitel</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="112"/>
        <source>Detail</source>
        <translation>Detalje</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="113"/>
        <source>Select File to Import</source>
        <translation>Vælg fil som skal importeres</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="114"/>
        <source>Channel ID:</source>
        <translation>Kanal ID:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="115"/>
        <source>Channel Number:</source>
        <translation>Kanal nummer:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="116"/>
        <source>Create DVD</source>
        <translation>Opret en DVD</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="117"/>
        <source>Create Archive</source>
        <translation>Opret et arkiv</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="118"/>
        <source>Encode Video File</source>
        <translation>Encode video fil</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="119"/>
        <source>Import Archive</source>
        <translation>Importer arkiv</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="120"/>
        <source>Archive Utilities</source>
        <translation>Arkivværktøjer</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="121"/>
        <source>Show Log Viewer</source>
        <translation>Vis log-viser</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="122"/>
        <source>Play Created DVD</source>
        <translation>Afspil oprettet DVD</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="123"/>
        <source>Burn DVD</source>
        <translation>Brænd DVD</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="19"/>
        <source>Find</source>
        <translation>Find</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="15"/>
        <source>Cancel</source>
        <translation>Annuller</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="24"/>
        <source>Prev</source>
        <translation>Forrige</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="25"/>
        <source>Next</source>
        <translation>Næste</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="70"/>
        <source>%DATE%, %TIME%</source>
        <translation>%DATE%, %TIME%</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="28"/>
        <source>OK</source>
        <translation>OK</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="16"/>
        <source>Back</source>
        <translation>Tilbage</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="30"/>
        <source>Home</source>
        <translation>Hjem</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="36"/>
        <source>Update</source>
        <translation>Opdater</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="37"/>
        <source>Exit</source>
        <translation>Afslut</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="86"/>
        <source>0.00 GB</source>
        <translation>0.00 GB</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="97"/>
        <source>Video Category:</source>
        <translation>Videokategori:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="80"/>
        <source>12.34 GB</source>
        <translation>12.34 GB</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="10"/>
        <source>Add Recording</source>
        <translation>Tilføj optagelse</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="11"/>
        <source>Add Video</source>
        <translation>Tilføj video</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="12"/>
        <source>Add File</source>
        <translation>Tilføj fil</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="56"/>
        <source>Save</source>
        <translation>Gem</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="67"/>
        <source>Search Channel</source>
        <translation>Søg kanal</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="68"/>
        <source>Search Callsign</source>
        <translation>Søg kaldenavn</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="69"/>
        <source>Search Name</source>
        <translation>Søg navn</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="17"/>
        <source>Finish</source>
        <translation>Færdig</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="57"/>
        <source>Add video</source>
        <translation>Tilføj video</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="71"/>
        <source>Choose where you would like your files archived.</source>
        <translation>Vælg hvor du gerne vil arkivere dine filer.</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="72"/>
        <source>Output Type:</source>
        <translation>Output type:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="73"/>
        <source>Destination:</source>
        <translation>Destination:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="74"/>
        <source>Click here to find an output location...</source>
        <translation>Klik her for at finde en output lokation...</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="75"/>
        <source>Erase DVD-RW before burning</source>
        <translation>Slet DVD-RW før brænding</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="76"/>
        <source>Previous</source>
        <translation>Forrige</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="77"/>
        <source>Filter:</source>
        <translation>Filter:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="78"/>
        <source>Select the file you wish to use.</source>
        <translation>Vælg filen som du vil benytte.</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="79"/>
        <source>See logs from your archive runs.</source>
        <translation>Se log for dine arkiveringskørsler.</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="81"/>
        <source>Choose the appearance of your DVD.</source>
        <translation>Vælg din DVDs udseende.</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="82"/>
        <source>Theme:</source>
        <translation>Tema:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="83"/>
        <source>Select the recordings and videos you wish to save.</source>
        <translation>Vælg optagelserne og videoerne som du vil benytte.</translation>
    </message>
</context>
<context>
    <name>ThumbFinder</name>
    <message>
        <location filename="../mytharchive/thumbfinder.cpp" line="922"/>
        <source>Exit, Save Thumbnails</source>
        <translation>Afslut, Gem thumbnails</translation>
    </message>
    <message>
        <location filename="../mytharchive/thumbfinder.cpp" line="923"/>
        <source>Exit, Don&apos;t Save Thumbnails</source>
        <translation>Afslut, Gem ikke thumbnails</translation>
    </message>
    <message>
        <location filename="../mytharchive/thumbfinder.cpp" line="915"/>
        <source>Menu</source>
        <translation>Menu</translation>
    </message>
</context>
<context>
    <name>VideoSelector</name>
    <message>
        <location filename="../mytharchive/videoselector.cpp" line="162"/>
        <source>Clear All</source>
        <translation>Slet alt</translation>
    </message>
    <message>
        <location filename="../mytharchive/videoselector.cpp" line="163"/>
        <source>Select All</source>
        <translation>Vælg alt</translation>
    </message>
    <message>
        <location filename="../mytharchive/videoselector.cpp" line="562"/>
        <source>You need to enter a valid password for this parental level</source>
        <translation>Du skal indtaste et gyldigt kodeord for dette voksenniveau</translation>
    </message>
    <message>
        <location filename="../mytharchive/videoselector.cpp" line="368"/>
        <location filename="../mytharchive/videoselector.cpp" line="505"/>
        <source>All Videos</source>
        <translation>Alle videoer</translation>
    </message>
    <message>
        <location filename="../mytharchive/videoselector.cpp" line="155"/>
        <source>Menu</source>
        <translation>Menu</translation>
    </message>
</context>
</TS>
