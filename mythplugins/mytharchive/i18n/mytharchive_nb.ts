<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE TS>
<TS version="2.0" language="nb_NO">
<context>
    <name>(ArchiveUtils)</name>
    <message>
        <location filename="../mytharchive/archiveutil.cpp" line="80"/>
        <source>Cannot find the MythArchive work directory.
Have you set the correct path in the settings?</source>
        <translation>Kan ikke finne arbeidskatalog for Myth-arkiv.
Har du satt opp riktig katalog in instillingene?</translation>
    </message>
</context>
<context>
    <name>(MythArchiveMain)</name>
    <message>
        <location filename="../mytharchive/main.cpp" line="93"/>
        <source>Found a lock file but the owning process isn&apos;t running!
Removing stale lock file.</source>
        <translation>Fant en låsfil men prosessen som eier denne kjører ikke!
Fjerner gammel låsfil.</translation>
    </message>
    <message>
        <location filename="../mytharchive/main.cpp" line="212"/>
        <source>Last run did not create a playable DVD.</source>
        <translation>Forrige kjøring produserte ikke en spillbar DVD.</translation>
    </message>
    <message>
        <location filename="../mytharchive/main.cpp" line="219"/>
        <source>Last run failed to create a DVD.</source>
        <translation>Forrige kjøring klarte ikke å lage en DVD.</translation>
    </message>
</context>
<context>
    <name>ArchiveFileSelector</name>
    <message>
        <location filename="../mytharchive/importnative.cpp" line="212"/>
        <source>Find File To Import</source>
        <translation>Finn fil som skal importeres</translation>
    </message>
    <message>
        <location filename="../mytharchive/importnative.cpp" line="272"/>
        <source>The selected item is not a valid archive file!</source>
        <translation>Det valgte elementet er ikke en gyld arkivfil!</translation>
    </message>
</context>
<context>
    <name>ArchiveSettings</name>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="20"/>
        <source>MythArchive Temp Directory</source>
        <translation>Mytharkivs katalog for midlertidige filer</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="23"/>
        <source>Location where MythArchive should create its temporary work files. LOTS of free space required here.</source>
        <translation>Plassering hvor Myth-arkiv skal lagre midlertidige arbeidsfiler. Her må det være MYE ledig plass.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="33"/>
        <source>MythArchive Share Directory</source>
        <translation>Mytharkivs katalog for delte filer</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="36"/>
        <source>Location where MythArchive stores its scripts, intro movies and theme files</source>
        <translation>Plassering hvor Myth-arkiv lagrer sine skript, intro- og temafiler</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="46"/>
        <source>Video format</source>
        <translation>Videoformat</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="51"/>
        <source>Video format for DVD recordings, PAL or NTSC.</source>
        <translation>Videoformat for DVD-opptak, PAL eller NTSC.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="60"/>
        <source>File Selector Filter</source>
        <translation>Filvelgingsfilter</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="63"/>
        <source>The file name filter to use in the file selector.</source>
        <translation>Filnavnfilter for bruk i filvelgeren.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="72"/>
        <source>Location of DVD</source>
        <translation>Plassering til DVD</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="75"/>
        <source>Which DVD drive to use when burning discs.</source>
        <translation>Hvilken DVD-stasjon som skal brukes til brenning.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="84"/>
        <source>DVD Drive Write Speed</source>
        <translation>Skrivehastighet for DVD-stasjon</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="87"/>
        <source>This is the write speed to use when burning a DVD. Set to 0 to allow growisofs to choose the fastest available speed.</source>
        <translation>Dette er skrivehastigheten som blir brukt under DVD-brenning. Sett denne til 0 hvis growisofs skal velge den raskeste tilgjengelige hastigheten.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="98"/>
        <source>Command to play DVD</source>
        <translation>Kommando for å spille DVD</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="101"/>
        <source>Command to run when test playing a created DVD. &apos;Internal&apos; will use the internal MythTV player. %f will be replaced with the path to the created DVD structure eg. &apos;xine -pfhq --no-splash dvd:/%f&apos;.</source>
        <translation>Kommando som kjøres under testing av en ny DVD. Settes denne til &apos;Internal&apos; vil den interne MythtTV avspilleren brukes. %f vil bli erstattet av stien til DVD-strukturen, dvs. &apos;xine -pfhq --no-splash dvd:/%f&apos;. </translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="114"/>
        <source>Copy remote files</source>
        <translation>Kopier fjerne filer</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="117"/>
        <source>If set files on remote filesystems will be copied over to the local filesystem before processing. Speeds processing and reduces bandwidth on the network</source>
        <translation>Kopierer filer på fjerne filsystemer over til det lokale filsystemet før de behandles. Gjør behandlingen raskere og senker bruken av båndbredde på nettverket</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="129"/>
        <source>Always Use Mythtranscode</source>
        <translation>Alltid bruk Mythtranscode</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="132"/>
        <source>If set mpeg2 files will always be passed though mythtranscode to clean up any errors. May help to fix some audio problems. Ignored if &apos;Use ProjectX&apos; is set.</source>
        <translation>Hvis på vil alltid MPEG-2-filer kjøres gjennom mythtranscode for å rette opp eventuelle feil. Dette kan kanskje ordne noen lydproblemer.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="144"/>
        <source>Use ProjectX</source>
        <translation>Bruk ProjectX</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="147"/>
        <source>If set ProjectX will be used to cut commercials and split mpeg2 files instead of mythtranscode and mythreplex.</source>
        <translation>Hvis valgt vil ProjectX bli brukt til å fjerne reklamer og splitte mpeg2 filer istendenfor mythtranscode og mythreplex.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="158"/>
        <source>Use FIFOs</source>
        <translation>Bruk FIFO&apos;er</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="161"/>
        <source>The script will use FIFOs to pass the output of mplex into dvdauthor rather than creating intermediate files. Saves time and disk space during multiplex operations but not supported on Windows platform</source>
        <translation>Gjør at skriptet bruker FIFO&apos;er for å sende utdata fra mplex til dvdauthor, i stedet for å lage midlertidige filer. Sparer tid og diskplass under multiplex-operasjoner, men virker ikke i Windows</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="174"/>
        <source>Add Subtitles</source>
        <translation>Legg til undertekster</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="177"/>
        <source>If available this option will add subtitles to the final DVD. Requires &apos;Use ProjectX&apos; to be on.</source>
        <translation>Hvis tilgjengelig vil denne instillingen legge til undertekster på DVDen. Krever at &apos;Bruk ProjectX&apos; er på.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="187"/>
        <source>Main Menu Aspect Ratio</source>
        <translation>Høyde/bredde-forhold i hovedmenyen</translation>
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
        <translation>Høyde/bredde-forholdet som skal brukes ved laging av hovedmenyen.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="203"/>
        <source>Chapter Menu Aspect Ratio</source>
        <translation>Høyde/breddeforhold for kapittelmenyen</translation>
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
        <translation>Høyde/breddeforhold som skal brukess ved laging av kapittelmenyen. &apos;%1&apos; betyr at det skal brukes det samme forholdet som i den tilknyttede videoen.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="223"/>
        <source>Date format</source>
        <translation>Datoformat</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="226"/>
        <source>Samples are shown using today&apos;s date.</source>
        <translation>Eksempler vises med dagens dato.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="232"/>
        <source>Samples are shown using tomorrow&apos;s date.</source>
        <translation>Eksempler vises med dato for i morgen.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="250"/>
        <source>Your preferred date format to use on DVD menus. %1</source>
        <extracomment>%1 gives additional info on the date used</extracomment>
        <translation>Ditt prefererte datoformat i DVD menyen. %1</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="259"/>
        <source>Time format</source>
        <translation>Tidsformat</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="266"/>
        <source>Your preferred time format to display on DVD menus. You must choose a format with &quot;AM&quot; or &quot;PM&quot; in it, otherwise your time display will be 24-hour or &quot;military&quot; time.</source>
        <translation>Ditt prefererte tidsformat i DVD menyen. Du må velge et format med &quot;AM&quot; eller &quot;PM&quot;, ellers vil tidsvisningen være basert på 24 timer formatet.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="277"/>
        <source>Default Encoder Profile</source>
        <translation>Standard kodingsprofil</translation>
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
        <translation>Standard kodingsprofil som vil bli brukt hvis filen trenger å rekodes.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="295"/>
        <source>mplex Command</source>
        <translation>mplex-kommando</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="299"/>
        <source>Command to run mplex</source>
        <translation>Kommando for å kjøre mplex</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="308"/>
        <source>dvdauthor command</source>
        <translation>dvdauthor-kommando</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="312"/>
        <source>Command to run dvdauthor.</source>
        <translation>Kommando fro å kjøre dvdauthor.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="321"/>
        <source>mkisofs command</source>
        <translation>mkisofs-kommando</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="325"/>
        <source>Command to run mkisofs. (Used to create ISO images)</source>
        <translation>Kommando for å kjøre mkisofs. (Brukes til å lage ISO-bilder.)</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="334"/>
        <source>growisofs command</source>
        <translation>growisofs-kommando</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="338"/>
        <source>Command to run growisofs. (Used to burn DVDs)</source>
        <translation>Kommando for å kjøre growisofs. (Brukes for å brenne DVD&apos;er.)</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="347"/>
        <source>M2VRequantiser command</source>
        <translation>Kommando for M2VRequantiser</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="351"/>
        <source>Command to run M2VRequantiser. Optional - leave blank if you don&apos;t have M2VRequantiser installed.</source>
        <translation>Kommando for å kjøreM2VRequantiser. Valgfritt - bruk at tomt felt hvis M2VRequantiser ikke er installert.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="361"/>
        <source>jpeg2yuv command</source>
        <translation>jpeg2yuv-kommando</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="365"/>
        <source>Command to run jpeg2yuv. Part of mjpegtools package</source>
        <translation>Kommando for åkjøre jpeg2yuv. Del av mjpegtools-pakken</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="374"/>
        <source>spumux command</source>
        <translation>spumux-kommando</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="378"/>
        <source>Command to run spumux. Part of dvdauthor package</source>
        <translation>Kommand for å kjøre spumux. Del av dvdauthor-pakken</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="387"/>
        <source>mpeg2enc command</source>
        <translation>mpeg2enc-kommando</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="391"/>
        <source>Command to run mpeg2enc. Part of mjpegtools package</source>
        <translation>Kommando for å kjøre mpeg2enc. Del av mjpegtools-pakken</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="400"/>
        <source>projectx command</source>
        <translation>projectx-kommando</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="404"/>
        <source>Command to run ProjectX. Will be used to cut commercials and split mpegs files instead of mythtranscode and mythreplex.</source>
        <translation>Kommando for ProjectX. Vil bli brukt å kutte reklamer og splitte mpeg filer istedenfor mythtranscode og mythreplex.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="414"/>
        <source>MythArchive Settings</source>
        <translation>Oppsett av Myth-arkiv</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="425"/>
        <source>MythArchive Settings (2)</source>
        <translation>Oppsett av Myth-arkiv (2)</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="435"/>
        <source>DVD Menu Settings</source>
        <translation>DVD menyinstillinger</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="443"/>
        <source>MythArchive External Commands (1)</source>
        <translation>Eksterne kommandoer for Myth-arkiv (1)</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="451"/>
        <source>MythArchive External Commands (2)</source>
        <translation>Eksterne kommandoer for Myth-arkiv (2)</translation>
    </message>
</context>
<context>
    <name>BurnMenu</name>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="1155"/>
        <source>Cannot burn a DVD.
The last run failed to create a DVD.</source>
        <translation>Kan ikke brenne en DVD.
Forrige kjøring kunne ikke lage en DVD.</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="1161"/>
        <location filename="../mytharchive/mythburn.cpp" line="1173"/>
        <source>Burn DVD</source>
        <translation>Brenn DVD</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="1162"/>
        <source>
Place a blank DVD in the drive and select an option below.</source>
        <translation>Sett i en tom DVD og velg et alternativ under.</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="1174"/>
        <source>Burn DVD Rewritable</source>
        <translation>Brenn DVD Rewritable</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="1175"/>
        <source>Burn DVD Rewritable (Force Erase)</source>
        <translation>Brenn DVD Rewritable (Tving Sletting)</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="1231"/>
        <source>It was not possible to run mytharchivehelper to burn the DVD.</source>
        <translation>Det var ikke mulig å kjøre mytharchivehelper for å brenne en DVD.</translation>
    </message>
</context>
<context>
    <name>BurnThemeUI</name>
    <message>
        <location filename="../mythburn/themes/burnthemestrings.h" line="5"/>
        <source>Has an intro and contains a main menu with 4 recordings per page. Does not have a chapter selection submenu.</source>
        <translation>Har en intro og inneholder en hovedmeny med 4 opptak per side. Har ikke kapittelmeny.</translation>
    </message>
    <message>
        <location filename="../mythburn/themes/burnthemestrings.h" line="6"/>
        <source>Has an intro and contains a summary main menu with 10 recordings per page. Does not have a chapter selection submenu, recording titles, dates or category.</source>
        <translation>Har en intro og inneholder en oppsummerende hovedmeny med 10 opptak per side. Har ikke kapittelmeny, tittel for opptak, dato eller kategori.</translation>
    </message>
    <message>
        <location filename="../mythburn/themes/burnthemestrings.h" line="7"/>
        <source>Has an intro and contains a main menu with 6 recordings per page. Does not have a scene selection submenu.</source>
        <translation>Har en intro og inneholder en hovedmeny med 6 opptak per side. Har ikke undemeny for valg av scene.</translation>
    </message>
    <message>
        <location filename="../mythburn/themes/burnthemestrings.h" line="8"/>
        <source>Has an intro and contains a main menu with 3 recordings per page and a scene selection submenu with 8 chapters points. Shows a program details page before each recording.</source>
        <translation>Har en intro og inneholder en hovedmeny med 3 opptak per side og en undermeny for scener med 8 kapittelpunkt. Viser en side med programdetaljer før hvert opptak.</translation>
    </message>
    <message>
        <location filename="../mythburn/themes/burnthemestrings.h" line="9"/>
        <source>Has an intro and contains a main menu with 3 recordings per page and a scene selection submenu with 8 chapters points. Shows a program details page before each recording. Uses animated thumb images.</source>
        <translation>Har en intro og inneholder en hovedmeny med 3 opptak per side og en undermeny for scener med 8 kapittelpunkt. Viser en side med programdetaljer før hvert opptak. Bruker animerte miniatyrbilder.</translation>
    </message>
    <message>
        <location filename="../mythburn/themes/burnthemestrings.h" line="10"/>
        <source>Has an intro and contains a main menu with 3 recordings per page and a scene selection submenu with 8 chapters points.</source>
        <translation>Har en intro og inneholder en hovedmeny med 3 opptak per side og en undermeny for scener med 8 kapittelpunkt.</translation>
    </message>
    <message>
        <location filename="../mythburn/themes/burnthemestrings.h" line="11"/>
        <source>Has an intro and contains a main menu with 3 recordings per page and a scene selection submenu with 8 chapters points. All the thumb images are animated.</source>
        <translation>Har en intro og inneholder en hovedmeny med 3 opptak per side og en undermeny for scener med 8 kapittelpunkt. Alle miniatyrbilder er animert.</translation>
    </message>
    <message>
        <location filename="../mythburn/themes/burnthemestrings.h" line="12"/>
        <source>Creates an auto play DVD with no menus. Shows an intro movie then for each title shows a details page followed by the video in sequence.</source>
        <translation>Lager en DVD uten menyer med automatisk avspilling. Viser en introduksjonsvideo for så å vise en detaljside for hver tittel fulgt av selve videoen.</translation>
    </message>
    <message>
        <location filename="../mythburn/themes/burnthemestrings.h" line="13"/>
        <source>Creates an auto play DVD with no menus and no intro.</source>
        <translation>Lager en DVD uten menyer og intro med automatisk avspilling.</translation>
    </message>
</context>
<context>
    <name>DVDThemeSelector</name>
    <message>
        <location filename="../mytharchive/themeselector.cpp" line="219"/>
        <location filename="../mytharchive/themeselector.cpp" line="230"/>
        <source>No theme description file found!</source>
        <translation>Ingen beskrivelsesfil for tema funnet!</translation>
    </message>
    <message>
        <location filename="../mytharchive/themeselector.cpp" line="243"/>
        <source>Empty theme description!</source>
        <translation>Tom temabeskrivelse!</translation>
    </message>
    <message>
        <location filename="../mytharchive/themeselector.cpp" line="248"/>
        <source>Unable to open theme description file!</source>
        <translation>Kunne ikke åpne beskrivelsesfil for tema!</translation>
    </message>
</context>
<context>
    <name>ExportNative</name>
    <message>
        <location filename="../mytharchive/exportnative.cpp" line="242"/>
        <source>You need to add at least one item to archive!</source>
        <translation>Du må legge til minst ett element for å arkivere!</translation>
    </message>
    <message>
        <location filename="../mytharchive/exportnative.cpp" line="400"/>
        <source>Remove Item</source>
        <translation>Fjern element</translation>
    </message>
    <message>
        <location filename="../mytharchive/exportnative.cpp" line="495"/>
        <source>It was not possible to create the DVD. An error occured when running the scripts</source>
        <translation>Klarte ikke lage DVD&apos;en; en feil oppstod under kjøringen av skriptene</translation>
    </message>
    <message>
        <location filename="../mytharchive/exportnative.cpp" line="531"/>
        <source>You don&apos;t have any videos!</source>
        <translation>Du har ingen videoer!</translation>
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
        <translation>Det valgte element er ikke en katalog!</translation>
    </message>
    <message>
        <location filename="../mytharchive/fileselector.cpp" line="81"/>
        <source>Find File</source>
        <translation>Finn fil</translation>
    </message>
    <message>
        <location filename="../mytharchive/fileselector.cpp" line="84"/>
        <source>Find Directory</source>
        <translation>Finn mappe</translation>
    </message>
    <message>
        <location filename="../mytharchive/fileselector.cpp" line="87"/>
        <source>Find Files</source>
        <translation>Finn filer</translation>
    </message>
</context>
<context>
    <name>ImportNative</name>
    <message>
        <location filename="../mytharchive/importnative.cpp" line="433"/>
        <source>You need to select a valid channel id!</source>
        <translation>Du må velge en gydlig kanal-id!</translation>
    </message>
    <message>
        <location filename="../mytharchive/importnative.cpp" line="464"/>
        <source>It was not possible to import the Archive.  An error occured when running &apos;mytharchivehelper&apos;</source>
        <translation>Klarte ikke importere arkivet; en feil oppsetod ved kjøring av «mytharchivehelper»</translation>
    </message>
    <message>
        <location filename="../mytharchive/importnative.cpp" line="594"/>
        <source>Select a channel id</source>
        <translation>Velg en kanalid</translation>
    </message>
    <message>
        <location filename="../mytharchive/importnative.cpp" line="620"/>
        <source>Select a channel number</source>
        <translation>Velg et kanalnummer</translation>
    </message>
    <message>
        <location filename="../mytharchive/importnative.cpp" line="646"/>
        <source>Select a channel name</source>
        <translation>Velg et kanalnavn</translation>
    </message>
    <message>
        <location filename="../mytharchive/importnative.cpp" line="672"/>
        <source>Select a Callsign</source>
        <translation>Velg et kanaltegn</translation>
    </message>
</context>
<context>
    <name>LogViewer</name>
    <message>
        <location filename="../mytharchive/logviewer.cpp" line="367"/>
        <source>Show Progress Log</source>
        <translation>Vis fremdriftslogg</translation>
    </message>
    <message>
        <location filename="../mytharchive/logviewer.cpp" line="368"/>
        <source>Show Full Log</source>
        <translation>Vis hele loggen</translation>
    </message>
    <message>
        <location filename="../mytharchive/logviewer.cpp" line="363"/>
        <source>Don&apos;t Auto Update</source>
        <translation>Ikke oppdater automatisk</translation>
    </message>
    <message>
        <location filename="../mytharchive/logviewer.cpp" line="77"/>
        <source>Cannot find any logs to show!</source>
        <translation>Kan ikke finne noen logger å vise!</translation>
    </message>
    <message>
        <location filename="../mytharchive/logviewer.cpp" line="216"/>
        <source>Background creation has been asked to stop.
This may take a few minutes.</source>
        <translation>Bakgrunnslagingen har blitt bedt om å stoppe. 
Dette kan ta et par minutter.</translation>
    </message>
    <message>
        <location filename="../mytharchive/logviewer.cpp" line="365"/>
        <source>Auto Update</source>
        <translation>Oppdater automatisk</translation>
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
        <translation>Ingen kuttliste</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="367"/>
        <source>You need to add at least one item to archive!</source>
        <translation>Du må legge til minst ett element for å arkivere!</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="413"/>
        <source>Retrieving File Information. Please Wait...</source>
        <translation>Henter filinformasjon. Vennligst vent...</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="483"/>
        <source>Encoder: </source>
        <translation>Koder: </translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="345"/>
        <location filename="../mytharchive/mythburn.cpp" line="469"/>
        <source>Using Cut List</source>
        <translation>Bruker kuttliste</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="350"/>
        <location filename="../mytharchive/mythburn.cpp" line="474"/>
        <source>Not Using Cut List</source>
        <translation>Bruker ikke kuttliste</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="820"/>
        <source>Don&apos;t Use Cut List</source>
        <translation>Ikke bruk kuttliste</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="823"/>
        <source>Use Cut List</source>
        <translation>Bruk kuttliste</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="827"/>
        <source>Remove Item</source>
        <translation>Fjern element</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="828"/>
        <source>Edit Details</source>
        <translation>Rediger detaljer</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="829"/>
        <source>Change Encoding Profile</source>
        <translation>Endre kodingsprofil</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="830"/>
        <source>Edit Thumbnails</source>
        <translation>Rediger miniatyrbilder</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="965"/>
        <source>It was not possible to create the DVD.  An error occured when running the scripts</source>
        <translation>Klarte ikke lage DVD&apos;en; en feil oppstod under kjøringen av skriptene</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="1008"/>
        <source>You don&apos;t have any videos!</source>
        <translation>Du har ingen videoer!</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="809"/>
        <source>Menu</source>
        <translation>meny</translation>
    </message>
</context>
<context>
    <name>MythControls</name>
    <message>
        <location filename="../mytharchive/main.cpp" line="302"/>
        <source>Toggle use cut list state for selected program</source>
        <translation>Slå av/på bruk av kuttliste for valgt program</translation>
    </message>
    <message>
        <location filename="../mytharchive/main.cpp" line="305"/>
        <source>Create DVD</source>
        <translation>Lag DVD</translation>
    </message>
    <message>
        <location filename="../mytharchive/main.cpp" line="307"/>
        <source>Create Archive</source>
        <translation>Lag arkiv</translation>
    </message>
    <message>
        <location filename="../mytharchive/main.cpp" line="309"/>
        <source>Import Archive</source>
        <translation>Importer arkiv</translation>
    </message>
    <message>
        <location filename="../mytharchive/main.cpp" line="311"/>
        <source>View Archive Log</source>
        <translation>Vis arkivlogg</translation>
    </message>
    <message>
        <location filename="../mytharchive/main.cpp" line="313"/>
        <source>Play Created DVD</source>
        <translation>Spill DVD som er laget</translation>
    </message>
    <message>
        <location filename="../mytharchive/main.cpp" line="315"/>
        <source>Burn DVD</source>
        <translation>Brenn DVD</translation>
    </message>
</context>
<context>
    <name>QObject</name>
    <message>
        <source>Location where MythArchive stores its scripts, intro movies and theme files</source>
        <translation type="obsolete">Plassering hvor Myth-arkiv lagrer sine skript, intro- og temafiler</translation>
    </message>
    <message>
        <source>Video format</source>
        <translation type="obsolete">Videoformat</translation>
    </message>
    <message>
        <source>Video format for DVD recordings, PAL or NTSC.</source>
        <translation type="obsolete">Videoformat for DVD-opptak, PAL eller NTSC.</translation>
    </message>
    <message>
        <source>File Selector Filter</source>
        <translation type="obsolete">Filvelgingsfilter</translation>
    </message>
    <message>
        <source>The file name filter to use in the file selector.</source>
        <translation type="obsolete">Filnavnfilter for bruk i filvelgeren.</translation>
    </message>
    <message>
        <source>Location of DVD</source>
        <translation type="obsolete">Plassering til DVD</translation>
    </message>
    <message>
        <source>Which DVD drive to use when burning discs.</source>
        <translation type="obsolete">Hvilken DVD-stasjon som skal brukes til brenning.</translation>
    </message>
    <message>
        <source>mplex Command</source>
        <translation type="obsolete">mplex-kommando</translation>
    </message>
    <message>
        <source>Command to run mplex</source>
        <translation type="obsolete">Kommando for å kjøre mplex</translation>
    </message>
    <message>
        <source>dvdauthor command</source>
        <translation type="obsolete">dvdauthor-kommando</translation>
    </message>
    <message>
        <source>Command to run dvdauthor.</source>
        <translation type="obsolete">Kommando fro å kjøre dvdauthor.</translation>
    </message>
    <message>
        <source>mkisofs command</source>
        <translation type="obsolete">mkisofs-kommando</translation>
    </message>
    <message>
        <source>Command to run mkisofs. (Used to create ISO images)</source>
        <translation type="obsolete">Kommando for å kjøre mkisofs. (Brukes til å lage ISO-bilder.)</translation>
    </message>
    <message>
        <source>growisofs command</source>
        <translation type="obsolete">growisofs-kommando</translation>
    </message>
    <message>
        <source>Command to run growisofs. (Used to burn DVD&apos;s)</source>
        <translation type="obsolete">Kommando for å kjøre growisofs. (Brukes for å brenne DVD&apos;er.)</translation>
    </message>
    <message>
        <source>spumux command</source>
        <translation type="obsolete">spumux-kommando</translation>
    </message>
    <message>
        <source>Command to run spumux. Part of dvdauthor package</source>
        <translation type="obsolete">Kommand for å kjøre spumux. Del av dvdauthor-pakken</translation>
    </message>
    <message>
        <source>mpeg2enc command</source>
        <translation type="obsolete">mpeg2enc-kommando</translation>
    </message>
    <message>
        <source>Command to run mpeg2enc. Part of mjpegtools package</source>
        <translation type="obsolete">Kommando for å kjøre mpeg2enc. Del av mjpegtools-pakken</translation>
    </message>
    <message>
        <source>MythArchive Settings</source>
        <translation type="obsolete">Oppsett av Myth-arkiv</translation>
    </message>
    <message>
        <source>MythArchive External Commands (1)</source>
        <translation type="obsolete">Eksterne kommandoer for Myth-arkiv (1)</translation>
    </message>
    <message>
        <source>MythArchive External Commands (2)</source>
        <translation type="obsolete">Eksterne kommandoer for Myth-arkiv (2)</translation>
    </message>
    <message>
        <source>Background creation has been asked to stop.
This may take a few minutes.</source>
        <translation type="obsolete">Bakgrunnslagingen har blitt bedt om å stoppe. 
Dette kan ta et par minutter.</translation>
    </message>
    <message>
        <source>Copy remote files</source>
        <translation type="obsolete">Kopier fjerne filer</translation>
    </message>
    <message>
        <source>If set files on remote filesystems will be copied over to the local filesystem before processing. Speeds processing and reduces bandwidth on the network</source>
        <translation type="obsolete">Kopierer filer på fjerne filsystemer over til det lokale filsystemet før de behandles. Gjør behandlingen raskere og senker bruken av båndbredde på nettverket</translation>
    </message>
    <message>
        <source>Always Use Mythtranscode</source>
        <translation type="obsolete">Alltid bruk Mythtranscode</translation>
    </message>
    <message>
        <source>Use FIFOs</source>
        <translation type="obsolete">Bruk FIFO&apos;er</translation>
    </message>
    <message>
        <source>Main Menu Aspect Ratio</source>
        <translation type="obsolete">Høyde/bredde-forhold i hovedmenyen</translation>
    </message>
    <message>
        <source>Aspect ratio to use when creating the main menu.</source>
        <translation type="obsolete">Høyde/bredde-forholdet som skal brukes ved laging av hovedmenyen.</translation>
    </message>
    <message>
        <source>Chapter Menu Aspect Ratio</source>
        <translation type="obsolete">Høyde/breddeforhold for kapittelmenyen</translation>
    </message>
    <message>
        <source>Aspect ratio to use when creating the chapter menu. Video means use the same aspect ratio as the associated video.</source>
        <translation type="obsolete">Høyde/breddeforhold som skal brukess ved laging av kapittelmenyen. «Video» betyr at det skal brukes det samme forholdet som i den tilknyttede videoen.</translation>
    </message>
    <message>
        <source>MythArchive Settings (2)</source>
        <translation type="obsolete">Oppsett av Myth-arkiv (2)</translation>
    </message>
    <message>
        <source>DVD Drive Write Speed</source>
        <translation type="obsolete">Skrivehastighet for DVD-stasjon</translation>
    </message>
    <message>
        <source>MythArchive Temp Directory</source>
        <translation type="obsolete">Mytharkivs katalog for midlertidige filer</translation>
    </message>
    <message>
        <source>Location where MythArchive should create its temporary work files. LOTS of free space required here.</source>
        <translation type="obsolete">Plassering hvor Myth-arkiv skal lagre midlertidige arbeidsfiler. Her må det være MYE ledig plass.</translation>
    </message>
    <message>
        <source>MythArchive Share Directory</source>
        <translation type="obsolete">Mytharkivs katalog for delte filer</translation>
    </message>
    <message>
        <source>This is the write speed to use when burning a DVD. Set to 0 to allow growisofs to choose the fastest available speed.</source>
        <translation type="obsolete">Dette er skrivehastigheten som blir brukt under DVD-brenning. Sett denne til 0 hvis growisofs skal velge den raskeste tilgjengelige hastigheten.</translation>
    </message>
    <message>
        <source>Command to play DVD</source>
        <translation type="obsolete">Kommando for å spille DVD</translation>
    </message>
    <message>
        <source>Command to run when test playing a created DVD. &apos;Internal&apos; will use the internal MythTV player. %f will be replaced with the path to the created DVD structure eg. &apos;xine -pfhq --no-splash dvd:/%f&apos;.</source>
        <translation type="obsolete">Kommando som kjøres under testing av en ny DVD. Settes denne til &apos;Internal&apos; vil den interne MythtTV avspilleren brukes. %f vil bli erstattet av stien til DVD-strukturen, dvs. &apos;xine -pfhq --no-splash dvd:/%f&apos;. </translation>
    </message>
    <message>
        <source>If set mpeg2 files will always be passed though mythtranscode to clean up any errors. May help to fix some audio problems. Ignored if &apos;Use ProjectX&apos; is set.</source>
        <translation type="obsolete">Hvis på vil alltid MPEG-2-filer kjøres gjennom mythtranscode for å rette opp eventuelle feil. Dette kan kanskje ordne noen lydproblemer.</translation>
    </message>
    <message>
        <source>Use ProjectX</source>
        <translation type="obsolete">Bruk ProjectX</translation>
    </message>
    <message>
        <source>If set ProjectX will be used to cut commercials and split mpeg2 files instead of mythtranscode and mythreplex.</source>
        <translation type="obsolete">Hvis valgt vil ProjectX bli brukt til å fjerne reklamer og splitte mpeg2 filer istendenfor mythtranscode og mythreplex.</translation>
    </message>
    <message>
        <source>The script will use FIFOs to pass the output of mplex into dvdauthor rather than creating intermediate files. Saves time and disk space during multiplex operations but not  supported on Windows platform</source>
        <translation type="obsolete">Gjør at skriptet bruker FIFO&apos;er for å sende utdata fra mplex til dvdauthor, i stedet for å lage midlertidige filer. Sparer tid og diskplass under multiplex-operasjoner, men virker ikke i Windows</translation>
    </message>
    <message>
        <source>Add Subtitles</source>
        <translation type="obsolete">Legg til undertekster</translation>
    </message>
    <message>
        <source>If available this option will add subtitles to the final DVD. Requires &apos;Use ProjectX&apos; to be on.</source>
        <translation type="obsolete">Hvis tilgjengelig vil denne instillingen legge til undertekster på DVDen. Krever at &apos;Bruk ProjectX&apos; er på.</translation>
    </message>
    <message>
        <source>Date format</source>
        <translation type="obsolete">Datoformat</translation>
    </message>
    <message>
        <source>Samples are shown using today&apos;s date.</source>
        <translation type="obsolete">Eksempler vises med dagens dato.</translation>
    </message>
    <message>
        <source>Samples are shown using tomorrow&apos;s date.</source>
        <translation type="obsolete">Eksempler vises med dato for i morgen.</translation>
    </message>
    <message>
        <source>Your preferred date format to use on DVD menus.</source>
        <translation type="obsolete">Ditt prefererte datoformat i DVD menyen.</translation>
    </message>
    <message>
        <source>Time format</source>
        <translation type="obsolete">Tidsformat</translation>
    </message>
    <message>
        <source>Your preferred time format to display on DVD menus. You must choose a format with &quot;AM&quot; or &quot;PM&quot; in it, otherwise your time display will be 24-hour or &quot;military&quot; time.</source>
        <translation type="obsolete">Ditt prefererte tidsformat i DVD menyen. Du må velge et format med &quot;AM&quot; eller &quot;PM&quot;, ellers vil tidsvisningen være basert på 24 timer formatet.</translation>
    </message>
    <message>
        <source>Default Encoder Profile</source>
        <translation type="obsolete">Standard kodingsprofil</translation>
    </message>
    <message>
        <source>Default encoding profile to use if a file needs re-encoding.</source>
        <translation type="obsolete">Standard kodingsprofil som vil bli brukt hvis filen trenger å rekodes.</translation>
    </message>
    <message>
        <source>M2VRequantiser command</source>
        <translation type="obsolete">Kommando for M2VRequantiser</translation>
    </message>
    <message>
        <source>Command to run M2VRequantiser. Optional - leave blank if you don&apos;t have M2VRequantiser installed.</source>
        <translation type="obsolete">Kommando for å kjøreM2VRequantiser. Valgfritt - bruk at tomt felt hvis M2VRequantiser ikke er installert.</translation>
    </message>
    <message>
        <source>jpeg2yuv command</source>
        <translation type="obsolete">jpeg2yuv-kommando</translation>
    </message>
    <message>
        <source>Command to run jpeg2yuv. Part of mjpegtools package</source>
        <translation type="obsolete">Kommando for åkjøre jpeg2yuv. Del av mjpegtools-pakken</translation>
    </message>
    <message>
        <source>projectx command</source>
        <translation type="obsolete">projectx-kommando</translation>
    </message>
    <message>
        <source>Command to run ProjectX. Will be used to cut commercials and split mpegs files instead of mythtranscode and mythreplex.</source>
        <translation type="obsolete">Kommando for ProjectX. Vil bli brukt å kutte reklamer og splitte mpeg filer istedenfor mythtranscode og mythreplex.</translation>
    </message>
    <message>
        <source>DVD Menu Settings</source>
        <translation type="obsolete">DVD menyinstillinger</translation>
    </message>
    <message>
        <source>Cannot find the MythArchive work directory.
Have you set the correct path in the settings?</source>
        <translation type="obsolete">Kan ikke finne arbeidskatalog for Myth-arkiv.
Har du satt opp riktig katalog in instillingene?</translation>
    </message>
    <message>
        <source>It was not possible to create the DVD. An error occured when running the scripts</source>
        <translation type="obsolete">Klarte ikke lage DVD&apos;en; en feil oppstod under kjøringen av skriptene</translation>
    </message>
    <message>
        <source>Cannot find any logs to show!</source>
        <translation type="obsolete">Kan ikke finne noen logger å vise!</translation>
    </message>
    <message>
        <source>Found a lock file but the owning process isn&apos;t running!
Removing stale lock file.</source>
        <translation type="obsolete">Fant en låsfil men prosessen som eier denne kjører ikke!
Fjerner gammel låsfil.</translation>
    </message>
    <message>
        <source>Last run did not create a playable DVD.</source>
        <translation type="obsolete">Forrige kjøring produserte ikke en spillbar DVD.</translation>
    </message>
    <message>
        <source>Last run failed to create a DVD.</source>
        <translation type="obsolete">Forrige kjøring klarte ikke å lage en DVD.</translation>
    </message>
    <message>
        <source>You don&apos;t have any videos!</source>
        <translation type="obsolete">Du har ingen videoer!</translation>
    </message>
    <message>
        <source>Cannot burn a DVD.
The last run failed to create a DVD.</source>
        <translation type="obsolete">Kan ikke brenne en DVD.
Forrige kjøring kunne ikke lage en DVD.</translation>
    </message>
    <message>
        <source>
Place a blank DVD in the drive and select an option below.</source>
        <translation type="obsolete">Sett i en tom DVD og velg et alternativ under.</translation>
    </message>
    <message>
        <source>Burn DVD</source>
        <translation type="obsolete">Brenn DVD</translation>
    </message>
    <message>
        <source>Burn DVD Rewritable</source>
        <translation type="obsolete">Brenn DVD Rewritable</translation>
    </message>
    <message>
        <source>Burn DVD Rewritable (Force Erase)</source>
        <translation type="obsolete">Brenn DVD Rewritable (Tving Sletting)</translation>
    </message>
    <message>
        <source>It was not possible to run mytharchivehelper to burn the DVD.</source>
        <translation type="obsolete">Det var ikke mulig å kjøre mytharchivehelper for å brenne en DVD.</translation>
    </message>
</context>
<context>
    <name>RecordingSelector</name>
    <message>
        <location filename="../mytharchive/recordingselector.cpp" line="133"/>
        <source>Retrieving Recording List.
Please Wait...</source>
        <translation>Henter opptaksliste.
Vennligst vent...</translation>
    </message>
    <message>
        <location filename="../mytharchive/recordingselector.cpp" line="213"/>
        <source>Clear All</source>
        <translation>Avmerk alle</translation>
    </message>
    <message>
        <location filename="../mytharchive/recordingselector.cpp" line="214"/>
        <source>Select All</source>
        <translation>Velg alle</translation>
    </message>
    <message>
        <location filename="../mytharchive/recordingselector.cpp" line="112"/>
        <location filename="../mytharchive/recordingselector.cpp" line="420"/>
        <location filename="../mytharchive/recordingselector.cpp" line="525"/>
        <source>All Recordings</source>
        <translation>Alle opptak</translation>
    </message>
    <message>
        <location filename="../mytharchive/recordingselector.cpp" line="157"/>
        <source>Either you don&apos;t have any recordings or no recordings are available locally!</source>
        <translation>Enter har du ingen opptak eller så er ikke opptakene tilgjengelig lokalt!</translation>
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
        <translation>Enkeltlags DVD (4,482 MB)</translation>
    </message>
    <message>
        <location filename="../mytharchive/archiveutil.cpp" line="40"/>
        <source>Dual Layer DVD (8,964 MB)</source>
        <translation>Dobbeltlags DVD (8,964 MB)</translation>
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
        <translation>Fil</translation>
    </message>
    <message>
        <location filename="../mytharchive/archiveutil.cpp" line="48"/>
        <source>Any file accessable from your filesystem.</source>
        <translation>Enhver fil tilgjengelig i ditt filsystem.</translation>
    </message>
    <message>
        <location filename="../mytharchive/selectdestination.cpp" line="294"/>
        <location filename="../mytharchive/selectdestination.cpp" line="350"/>
        <source>Unknown</source>
        <translation>Ukjent</translation>
    </message>
</context>
<context>
    <name>ThemeUI</name>
    <message>
        <location filename="themestrings.h" line="35"/>
        <source>Select Destination</source>
        <translation>Velg destinasjon</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="113"/>
        <source>description goes here.</source>
        <translation>beskrivelse her.</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="37"/>
        <source>Free Space:</source>
        <translation>Ledig plass:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="38"/>
        <source>Make ISO Image</source>
        <translation>Lag ISO image</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="39"/>
        <source>Burn to DVD</source>
        <translation>Brenn til DVD</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="40"/>
        <source>Force Overwrite of DVD-RW Media</source>
        <translation>Tving overskriving av DVD RW medie</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="43"/>
        <source>Select Recordings</source>
        <translation>Velg opptak</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="44"/>
        <source>Show Recordings</source>
        <translation>Vis opptak</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="46"/>
        <source>File Finder</source>
        <translation>Filfinner</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="48"/>
        <source>Select Videos</source>
        <translation>Velg videoer</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="49"/>
        <source>Video Category</source>
        <translation>Videokategori</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="115"/>
        <source>title goes here</source>
        <translation>tittel her</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="121"/>
        <source>x.xx Gb</source>
        <translation>x.xx Gb</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="50"/>
        <source>PL:</source>
        <translation>PL:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="116"/>
        <source>1</source>
        <translation>1</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="51"/>
        <source>No videos available</source>
        <translation>Ingen videoer tilgjengelig</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="52"/>
        <source>Log Viewer</source>
        <translation>Loggviser</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="55"/>
        <source>Change Encoding Profile</source>
        <translation>Endre kodingsprofil</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="56"/>
        <source>DVD Menu Theme</source>
        <translation>DVD menytema</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="57"/>
        <source>Select a theme</source>
        <translation>Velg et tema</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="58"/>
        <source>Intro</source>
        <translation>Intro</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="59"/>
        <source>Main Menu</source>
        <translation>Hovedmeny</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="60"/>
        <source>Chapter Menu</source>
        <translation>Kapittelmeny</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="61"/>
        <source>Details</source>
        <translation>Detaljer</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="62"/>
        <source>Select Archive Items</source>
        <translation>Velg arkivelementer</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="26"/>
        <source>No files are selected for archive</source>
        <translation>Ingen filer er valgt for arkivet</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="63"/>
        <source>Archive Item Details</source>
        <translation>Detaljer for arkivelement</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="64"/>
        <source>Title:</source>
        <translation>Tittel:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="65"/>
        <source>Subtitle:</source>
        <translation>Undertekst:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="66"/>
        <source>Start Date:</source>
        <translation>Startdato:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="67"/>
        <source>Time:</source>
        <translation>Tid:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="68"/>
        <source>Description:</source>
        <translation>Beskrivelse:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="69"/>
        <source>Thumb Image Selector</source>
        <translation>velger for miniatyrbilde</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="70"/>
        <source>Current Position</source>
        <translation>Gjeldende posisjon</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="101"/>
        <source>0:00:00.00</source>
        <translation>0:00:00.00</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="122"/>
        <source>Write video to data dvd or file</source>
        <translation>Skriv video til data-dvd eller fil</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="13"/>
        <source>File</source>
        <translation>Fil</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="123"/>
        <source>Find Location</source>
        <translation>Finn lokasjon</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="124"/>
        <source>Navigation</source>
        <translation>Navigasjon</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="125"/>
        <source>Filesize</source>
        <translation>Filstørrelse</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="126"/>
        <source>Date (time)</source>
        <translation>Dato (tid)</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="127"/>
        <source>Show Videos</source>
        <translation>Vis videoer</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="128"/>
        <source>Parental Level</source>
        <translation>Foreldrenivå</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="129"/>
        <source>Archive Item:</source>
        <translation>Arkiver element:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="130"/>
        <source>Encoder Profile:</source>
        <translation>Kodingsprofil:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="163"/>
        <source>Old size:</source>
        <oldsource>Current Size:</oldsource>
        <translation>Gjeldende størrelse:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="112"/>
        <source>New Size:</source>
        <translation>Ny størrelse:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="9"/>
        <source>DVD</source>
        <translation>DVD</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="10"/>
        <source>Create a</source>
        <translation>Lag en</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="11"/>
        <source>Videos</source>
        <translation>Videoer</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="12"/>
        <source>Export your</source>
        <translation>Eksporter dine</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="14"/>
        <source>Encode your video</source>
        <translation>Kod din video</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="15"/>
        <source>Archive</source>
        <translation>Arkiv</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="16"/>
        <source>Import an</source>
        <translation>Importer et</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="17"/>
        <source>Utilities</source>
        <translation>Verktøy</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="18"/>
        <source>Use archive</source>
        <translation>Bruk arkiv</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="19"/>
        <source>Media</source>
        <translation>Media</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="20"/>
        <source>Eject your</source>
        <translation>Løs ut din</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="21"/>
        <source>Log</source>
        <translation>Logg</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="22"/>
        <source>Show</source>
        <translation>Vis</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="23"/>
        <source>Test created</source>
        <translation>Test laget</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="24"/>
        <source>Burn</source>
        <translation>Brenn</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="25"/>
        <source>Start to</source>
        <translation>Start med å</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="30"/>
        <source>0 mb</source>
        <translation>0 mb</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="31"/>
        <source>xxxxx mb</source>
        <translation>xxxxx mb</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="131"/>
        <source>Current Size:</source>
        <translation>Gjeldende størrelse:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="133"/>
        <source>Not Applicable</source>
        <translation>Ikke relevant</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="134"/>
        <source>Intro:</source>
        <translation>Intro:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="135"/>
        <source>Main Menu:</source>
        <translation>Hovedmeny:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="136"/>
        <source>Chapter Menu:</source>
        <translation>Kapittelmeny:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="137"/>
        <source>Details:</source>
        <translation>Detaljer:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="138"/>
        <source>%date% / %profile%</source>
        <translation>%date% / %profile%</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="139"/>
        <source>Current size: %1</source>
        <translation>Gjeldende størrelse: %1</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="140"/>
        <source>Current Position:</source>
        <translation>Gjeldende posisjon:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="141"/>
        <source>Seek Amount:</source>
        <translation>Søkemengde:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="142"/>
        <source>increase seek amount</source>
        <translation>Øk søkemengde</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="143"/>
        <source>decrease seek amount</source>
        <translation>reduser søkemengde</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="144"/>
        <source>move left</source>
        <translation>Flytt til venstre</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="145"/>
        <source>move right</source>
        <translation>Flytt til høyre</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="146"/>
        <source>Read video from data dvd or file</source>
        <translation>Les video fra data-dvd eller fil</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="147"/>
        <source>Associated Channel</source>
        <translation>Assosiert kanal</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="148"/>
        <source>Archive Chan ID:</source>
        <translation>Kanal ID for arkiv:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="149"/>
        <source>Archive Chan No:</source>
        <translation>Kanal nr for arkiv:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="150"/>
        <source>Archive Callsign:</source>
        <translation>Kallesignal for arkiv:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="151"/>
        <source>Archive Name:</source>
        <translation>Arkivnavn:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="152"/>
        <source>Local Chan ID:</source>
        <translation>Lokalt kanal-ID:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="153"/>
        <source>Local Chan No:</source>
        <translation>Lokalt kanal-nr:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="154"/>
        <source>Local Callsign:</source>
        <translation>Lokalt kallesignal:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="155"/>
        <source>Local Name:</source>
        <translation>Lokalt navn:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="156"/>
        <source>Search Local</source>
        <translation>Søk lokalt</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="157"/>
        <source>Channel ID</source>
        <translation>Kanal ID</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="158"/>
        <source>Channel No</source>
        <translation>Kanal nr</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="159"/>
        <source>Callsign</source>
        <translation>Kallesignal</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="160"/>
        <source>Name</source>
        <translation>Navn</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="169"/>
        <source>Channel ID:</source>
        <translation>Kanal ID:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="170"/>
        <source>Channel Number:</source>
        <translation>Kanalnummer:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="71"/>
        <source>Seek Amount</source>
        <translation>Søkemengde</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="72"/>
        <source>Frame</source>
        <translation>Ramme</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="102"/>
        <source>Up Level</source>
        <translation>Opp et nivå</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="117"/>
        <source>xxxxx MB</source>
        <translation>xxxxx MB</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="118"/>
        <source>0 MB</source>
        <translation>0 MB</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="119"/>
        <source>sep 13, 2004 11:00 pm (1h 15m)</source>
        <translation>13 sept, 2004 11:00 (1t 15min)</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="120"/>
        <source>x.xx GB</source>
        <translation>x.xx GB</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="75"/>
        <source>File Finder To Import</source>
        <translation>Filfinner for importering</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="76"/>
        <source>Start Time:</source>
        <translation>Starttid:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="77"/>
        <source>Select Associated Channel</source>
        <translation>Velg assosiert kanal</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="78"/>
        <source>Archived Channel</source>
        <translation>Arkivert kanal</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="79"/>
        <source>Chan. ID:</source>
        <translation>Kanal ID:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="80"/>
        <source>Chan. No:</source>
        <translation>Kanal Nr:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="81"/>
        <source>Callsign:</source>
        <translation>Kallesignal:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="82"/>
        <source>Name:</source>
        <translation>Navn:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="83"/>
        <source>Local Channel</source>
        <translation>Lokal kanal</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="5"/>
        <source>A high quality profile giving approx. 1 hour of video on a single layer DVD</source>
        <translation>En profil med høy kvalitet som gir omtrent 1 time med video på en enkeltlags DVD</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="6"/>
        <source>A standard play profile giving approx. 2 hour of video on a single layer DVD</source>
        <translation>En profil med standard kvalitet som gir omtrent 2 timer med video på en enkeltlags DVD</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="7"/>
        <source>A long play profile giving approx. 4 hour of video on a single layer DVD</source>
        <translation>En profil med lang avspillingstid som gir omtrent 4 timer med video på en enkeltlags DVD</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="8"/>
        <source>A extended play profile giving approx. 6 hour of video on a single layer DVD</source>
        <translation>En profil med utvidet avspillingstid som gir omtrent 6 timer med video på en enkeltlags DVD</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="104"/>
        <source>Filesize: %1</source>
        <translation>Filstørrelse: %1</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="105"/>
        <source>Recorded Time: %1</source>
        <translation>Tid tatt opp: %1</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="106"/>
        <source>%&quot;|SUBTITLE|&quot; %%(|ORIGINALAIRDATE|) %%(|STARS|) %%DESCRIPTION%</source>
        <translation>%&quot;|SUBTITLE|&quot; %%(|ORIGINALAIRDATE|) %%(|STARS|) %%DESCRIPTION%</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="108"/>
        <source>Parental Level: %1</source>
        <translation>Foreldrenivå: %1</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="109"/>
        <source>Archive Items</source>
        <translation>Arkiver elementer</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="110"/>
        <source>Profile:</source>
        <translation>Profil:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="111"/>
        <source>Old Size:</source>
        <translation>Gjeldende størrelse:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="161"/>
        <source>Select Destination:</source>
        <translation>Velg destinasjon:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="162"/>
        <source>Parental level: %1</source>
        <translation>Foreldrenivå: %1</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="164"/>
        <source>New size:</source>
        <translation>Ny størrelse:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="132"/>
        <source>Select a theme:</source>
        <translation>Velg et tema:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="165"/>
        <source>Menu</source>
        <translation>Meny</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="166"/>
        <source>Chapter</source>
        <translation>Kapittel</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="167"/>
        <source>Detail</source>
        <translation>Detalj</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="168"/>
        <source>Select File to Import</source>
        <translation>Velg fil som skal importeres</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="171"/>
        <source>Create DVD</source>
        <translation>Lag DVD</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="172"/>
        <source>Create Archive</source>
        <translation>Lag arkiv</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="173"/>
        <source>Encode Video File</source>
        <translation>Kod videofil</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="174"/>
        <source>Import Archive</source>
        <translation>Importer arkiv</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="175"/>
        <source>Archive Utilities</source>
        <translation>Arkiv-verktøy</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="176"/>
        <source>Show Log Viewer</source>
        <translation>Vis loggviser</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="177"/>
        <source>Play Created DVD</source>
        <translation>Spill DVD som er laget</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="178"/>
        <source>Burn DVD</source>
        <translation>Brenn DVD</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="88"/>
        <source>Choose where you would like your files archived.</source>
        <translation>Velg hvor du vil at filene dine skal bli arkivert.</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="87"/>
        <source>%DATE%, %TIME%</source>
        <translation>%DATE%, %TIME%</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="89"/>
        <source>Output Type:</source>
        <translation>Utdata type:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="90"/>
        <source>Destination:</source>
        <translation>Mål:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="91"/>
        <source>Click here to find an output location...</source>
        <translation>Klikk her for å finne et utdatasted...</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="92"/>
        <source>Erase DVD-RW before burning</source>
        <translation>Slett DVD-RW før brenning</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="32"/>
        <source>Cancel</source>
        <translation>Avbryt</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="93"/>
        <source>Previous</source>
        <translation>Forrige</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="42"/>
        <source>Next</source>
        <translation>Neste</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="94"/>
        <source>Filter:</source>
        <translation>Filter:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="45"/>
        <source>OK</source>
        <translation>OK</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="95"/>
        <source>Select the file you wish to use.</source>
        <translation>Velg hvilken fil du vil bruke.</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="33"/>
        <source>Back</source>
        <translation>Tilbake</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="47"/>
        <source>Home</source>
        <translation>Hjem</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="96"/>
        <source>See logs from your archive runs.</source>
        <translation>Se logger fra arkiveringskjøringen.</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="53"/>
        <source>Update</source>
        <translation>Oppdater</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="54"/>
        <source>Exit</source>
        <translation>Avslutt</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="103"/>
        <source>0.00 GB</source>
        <translation>0.00 GB</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="114"/>
        <source>Video Category:</source>
        <translation>Videokategori:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="107"/>
        <source>Ok</source>
        <translation>OK</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="97"/>
        <source>12.34 GB</source>
        <translation>12.34 GB</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="98"/>
        <source>Choose the appearance of your DVD.</source>
        <translation>Velg utseende for DVDen din.</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="99"/>
        <source>Theme:</source>
        <translation>Tema:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="100"/>
        <source>Select the recordings and videos you wish to save.</source>
        <translation>Velg hvilke opptak og videoer du vil lagre.</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="27"/>
        <source>Add Recording</source>
        <translation>Legg til opptak</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="28"/>
        <source>Add Video</source>
        <translation>Legg til video</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="29"/>
        <source>Add File</source>
        <translation>Legg til fil</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="73"/>
        <source>Save</source>
        <translation>Lagre</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="36"/>
        <source>Find</source>
        <translation>Finn</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="41"/>
        <source>Prev</source>
        <translation>Forrige</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="84"/>
        <source>Search Channel</source>
        <translation>Søk i kanal</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="85"/>
        <source>Search Callsign</source>
        <translation>Søk i kallesignal</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="86"/>
        <source>Search Name</source>
        <translation>Søk i navn</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="34"/>
        <source>Finish</source>
        <translation>Fullfør</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="74"/>
        <source>Add video</source>
        <translation>Legg til video</translation>
    </message>
</context>
<context>
    <name>ThumbFinder</name>
    <message>
        <location filename="../mytharchive/thumbfinder.cpp" line="928"/>
        <source>Exit, Save Thumbnails</source>
        <translation>Avslutt, og lagre miniatyrbildene</translation>
    </message>
    <message>
        <location filename="../mytharchive/thumbfinder.cpp" line="929"/>
        <source>Exit, Don&apos;t Save Thumbnails</source>
        <translation>Avslutt, Ikke lagre miniatyrbildene</translation>
    </message>
    <message>
        <location filename="../mytharchive/thumbfinder.cpp" line="921"/>
        <source>Menu</source>
        <translation>Meny</translation>
    </message>
</context>
<context>
    <name>VideoSelector</name>
    <message>
        <location filename="../mytharchive/videoselector.cpp" line="160"/>
        <source>Clear All</source>
        <translation>Velg ingen</translation>
    </message>
    <message>
        <location filename="../mytharchive/videoselector.cpp" line="161"/>
        <source>Select All</source>
        <translation>Velg alle</translation>
    </message>
    <message>
        <location filename="../mytharchive/videoselector.cpp" line="359"/>
        <location filename="../mytharchive/videoselector.cpp" line="489"/>
        <source>All Videos</source>
        <translation>Alle videoer</translation>
    </message>
    <message>
        <location filename="../mytharchive/videoselector.cpp" line="546"/>
        <source>You need to enter a valid password for this parental level</source>
        <translation>Du må angi et gyldig passord for dette foreldrenivået</translation>
    </message>
    <message>
        <location filename="../mytharchive/videoselector.cpp" line="153"/>
        <source>Menu</source>
        <translation>Meny</translation>
    </message>
</context>
</TS>
