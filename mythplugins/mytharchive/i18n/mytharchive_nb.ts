<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE TS>
<TS version="2.1" language="nb_NO" sourcelanguage="en_US">
<context>
    <name>(ArchiveUtils)</name>
    <message>
        <location filename="../mytharchive/archiveutil.cpp" line="52"/>
        <source>Cannot find the MythArchive work directory.
Have you set the correct path in the settings?</source>
        <translation>Kan ikke finne arbeidskatalog for Myth-arkiv.
Har du satt opp riktig katalog in instillingene?</translation>
    </message>
</context>
<context>
    <name>(MythArchiveMain)</name>
    <message>
        <location filename="../mytharchive/mytharchive.cpp" line="100"/>
        <source>Found a lock file but the owning process isn&apos;t running!
Removing stale lock file.</source>
        <translation>Fant en låsfil men prosessen som eier denne kjører ikke!
Fjerner gammel låsfil.</translation>
    </message>
    <message>
        <location filename="../mytharchive/mytharchive.cpp" line="211"/>
        <source>Last run did not create a playable DVD.</source>
        <translation>Forrige kjøring produserte ikke en spillbar DVD.</translation>
    </message>
    <message>
        <location filename="../mytharchive/mytharchive.cpp" line="218"/>
        <source>Last run failed to create a DVD.</source>
        <translation>Forrige kjøring klarte ikke å lage en DVD.</translation>
    </message>
</context>
<context>
    <name>ArchiveFileSelector</name>
    <message>
        <location filename="../mytharchive/importnative.cpp" line="205"/>
        <source>Find File To Import</source>
        <translation>Finn fil som skal importeres</translation>
    </message>
    <message>
        <location filename="../mytharchive/importnative.cpp" line="265"/>
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
        <location filename="../mytharchive/archivesettings.cpp" line="35"/>
        <source>MythArchive Share Directory</source>
        <translation>Mytharkivs katalog for delte filer</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="38"/>
        <source>Location where MythArchive stores its scripts, intro movies and theme files</source>
        <translation>Plassering hvor Myth-arkiv lagrer sine skript, intro- og temafiler</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="50"/>
        <source>Video format</source>
        <translation>Videoformat</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="55"/>
        <source>Video format for DVD recordings, PAL or NTSC.</source>
        <translation>Videoformat for DVD-opptak, PAL eller NTSC.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="64"/>
        <source>File Selector Filter</source>
        <translation>Filvelgingsfilter</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="67"/>
        <source>The file name filter to use in the file selector.</source>
        <translation>Filnavnfilter for bruk i filvelgeren.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="76"/>
        <source>Location of DVD</source>
        <translation>Plassering til DVD</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="79"/>
        <source>Which DVD drive to use when burning discs.</source>
        <translation>Hvilken DVD-stasjon som skal brukes til brenning.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="89"/>
        <source>DVD Drive Write Speed</source>
        <translation>Skrivehastighet for DVD-stasjon</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="92"/>
        <source>This is the write speed to use when burning a DVD. Set to 0 to allow growisofs to choose the fastest available speed.</source>
        <translation>Dette er skrivehastigheten som blir brukt under DVD-brenning. Sett denne til 0 hvis growisofs skal velge den raskeste tilgjengelige hastigheten.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="103"/>
        <source>Command to play DVD</source>
        <translation>Kommando for å spille DVD</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="106"/>
        <source>Command to run when test playing a created DVD. &apos;Internal&apos; will use the internal MythTV player. %f will be replaced with the path to the created DVD structure eg. &apos;xine -pfhq --no-splash dvd:/%f&apos;.</source>
        <translation>Kommando som kjøres under testing av en ny DVD. Settes denne til &apos;Internal&apos; vil den interne MythtTV avspilleren brukes. %f vil bli erstattet av stien til DVD-strukturen, dvs. &apos;xine -pfhq --no-splash dvd:/%f&apos;. </translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="119"/>
        <source>Copy remote files</source>
        <translation>Kopier fjerne filer</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="122"/>
        <source>If set files on remote filesystems will be copied over to the local filesystem before processing. Speeds processing and reduces bandwidth on the network</source>
        <translation>Kopierer filer på fjerne filsystemer over til det lokale filsystemet før de behandles. Gjør behandlingen raskere og senker bruken av båndbredde på nettverket</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="134"/>
        <source>Always Use Mythtranscode</source>
        <translation>Alltid bruk Mythtranscode</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="137"/>
        <source>If set mpeg2 files will always be passed though mythtranscode to clean up any errors. May help to fix some audio problems. Ignored if &apos;Use ProjectX&apos; is set.</source>
        <translation>Hvis på vil alltid MPEG-2-filer kjøres gjennom mythtranscode for å rette opp eventuelle feil. Dette kan kanskje ordne noen lydproblemer.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="149"/>
        <source>Use ProjectX</source>
        <translation>Bruk ProjectX</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="152"/>
        <source>If set ProjectX will be used to cut commercials and split mpeg2 files instead of mythtranscode and mythreplex.</source>
        <translation>Hvis valgt vil ProjectX bli brukt til å fjerne reklamer og splitte mpeg2 filer istendenfor mythtranscode og mythreplex.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="163"/>
        <source>Use FIFOs</source>
        <translation>Bruk FIFO&apos;er</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="166"/>
        <source>The script will use FIFOs to pass the output of mplex into dvdauthor rather than creating intermediate files. Saves time and disk space during multiplex operations but not supported on Windows platform</source>
        <translation>Gjør at skriptet bruker FIFO&apos;er for å sende utdata fra mplex til dvdauthor, i stedet for å lage midlertidige filer. Sparer tid og diskplass under multiplex-operasjoner, men virker ikke i Windows</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="179"/>
        <source>Add Subtitles</source>
        <translation>Legg til undertekster</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="182"/>
        <source>If available this option will add subtitles to the final DVD. Requires &apos;Use ProjectX&apos; to be on.</source>
        <translation>Hvis tilgjengelig vil denne instillingen legge til undertekster på DVDen. Krever at &apos;Bruk ProjectX&apos; er på.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="192"/>
        <source>Main Menu Aspect Ratio</source>
        <translation>Høyde/bredde-forhold i hovedmenyen</translation>
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
        <translation>Høyde/bredde-forholdet som skal brukes ved laging av hovedmenyen.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="208"/>
        <source>Chapter Menu Aspect Ratio</source>
        <translation>Høyde/breddeforhold for kapittelmenyen</translation>
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
        <translation>Høyde/breddeforhold som skal brukess ved laging av kapittelmenyen. &apos;%1&apos; betyr at det skal brukes det samme forholdet som i den tilknyttede videoen.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="228"/>
        <source>Date format</source>
        <translation>Datoformat</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="231"/>
        <source>Samples are shown using today&apos;s date.</source>
        <translation>Eksempler vises med dagens dato.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="237"/>
        <source>Samples are shown using tomorrow&apos;s date.</source>
        <translation>Eksempler vises med dato for i morgen.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="255"/>
        <source>Your preferred date format to use on DVD menus. %1</source>
        <extracomment>%1 gives additional info on the date used</extracomment>
        <translation>Ditt prefererte datoformat i DVD menyen. %1</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="264"/>
        <source>Time format</source>
        <translation>Tidsformat</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="271"/>
        <source>Your preferred time format to display on DVD menus. You must choose a format with &quot;AM&quot; or &quot;PM&quot; in it, otherwise your time display will be 24-hour or &quot;military&quot; time.</source>
        <translation>Ditt prefererte tidsformat i DVD menyen. Du må velge et format med &quot;AM&quot; eller &quot;PM&quot;, ellers vil tidsvisningen være basert på 24 timer formatet.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="282"/>
        <source>Default Encoder Profile</source>
        <translation>Standard kodingsprofil</translation>
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
        <translation>Standard kodingsprofil som vil bli brukt hvis filen trenger å rekodes.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="300"/>
        <source>mplex Command</source>
        <translation>mplex-kommando</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="304"/>
        <source>Command to run mplex</source>
        <translation>Kommando for å kjøre mplex</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="313"/>
        <source>dvdauthor command</source>
        <translation>dvdauthor-kommando</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="317"/>
        <source>Command to run dvdauthor.</source>
        <translation>Kommando fro å kjøre dvdauthor.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="326"/>
        <source>mkisofs command</source>
        <translation>mkisofs-kommando</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="330"/>
        <source>Command to run mkisofs. (Used to create ISO images)</source>
        <translation>Kommando for å kjøre mkisofs. (Brukes til å lage ISO-bilder.)</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="339"/>
        <source>growisofs command</source>
        <translation>growisofs-kommando</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="343"/>
        <source>Command to run growisofs. (Used to burn DVDs)</source>
        <translation>Kommando for å kjøre growisofs. (Brukes for å brenne DVD&apos;er.)</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="352"/>
        <source>M2VRequantiser command</source>
        <translation>Kommando for M2VRequantiser</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="356"/>
        <source>Command to run M2VRequantiser. Optional - leave blank if you don&apos;t have M2VRequantiser installed.</source>
        <translation>Kommando for å kjøreM2VRequantiser. Valgfritt - bruk at tomt felt hvis M2VRequantiser ikke er installert.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="366"/>
        <source>jpeg2yuv command</source>
        <translation>jpeg2yuv-kommando</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="370"/>
        <source>Command to run jpeg2yuv. Part of mjpegtools package</source>
        <translation>Kommando for åkjøre jpeg2yuv. Del av mjpegtools-pakken</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="379"/>
        <source>spumux command</source>
        <translation>spumux-kommando</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="383"/>
        <source>Command to run spumux. Part of dvdauthor package</source>
        <translation>Kommand for å kjøre spumux. Del av dvdauthor-pakken</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="392"/>
        <source>mpeg2enc command</source>
        <translation>mpeg2enc-kommando</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="396"/>
        <source>Command to run mpeg2enc. Part of mjpegtools package</source>
        <translation>Kommando for å kjøre mpeg2enc. Del av mjpegtools-pakken</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="405"/>
        <source>projectx command</source>
        <translation>projectx-kommando</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="409"/>
        <source>Command to run ProjectX. Will be used to cut commercials and split mpegs files instead of mythtranscode and mythreplex.</source>
        <translation>Kommando for ProjectX. Vil bli brukt å kutte reklamer og splitte mpeg filer istedenfor mythtranscode og mythreplex.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="418"/>
        <source>MythArchive Settings</source>
        <translation>Oppsett av Myth-arkiv</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="442"/>
        <source>MythArchive External Commands</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <source>MythArchive Settings (2)</source>
        <translation type="vanished">Oppsett av Myth-arkiv (2)</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="434"/>
        <source>DVD Menu Settings</source>
        <translation>DVD menyinstillinger</translation>
    </message>
    <message>
        <source>MythArchive External Commands (1)</source>
        <translation type="vanished">Eksterne kommandoer for Myth-arkiv (1)</translation>
    </message>
    <message>
        <source>MythArchive External Commands (2)</source>
        <translation type="vanished">Eksterne kommandoer for Myth-arkiv (2)</translation>
    </message>
</context>
<context>
    <name>BurnMenu</name>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="1095"/>
        <source>Cannot burn a DVD.
The last run failed to create a DVD.</source>
        <translation>Kan ikke brenne en DVD.
Forrige kjøring kunne ikke lage en DVD.</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="1101"/>
        <location filename="../mytharchive/mythburn.cpp" line="1113"/>
        <source>Burn DVD</source>
        <translation>Brenn DVD</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="1102"/>
        <source>
Place a blank DVD in the drive and select an option below.</source>
        <translation>Sett i en tom DVD og velg et alternativ under.</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="1114"/>
        <source>Burn DVD Rewritable</source>
        <translation>Brenn DVD Rewritable</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="1115"/>
        <source>Burn DVD Rewritable (Force Erase)</source>
        <translation>Brenn DVD Rewritable (Tving Sletting)</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="1169"/>
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
        <location filename="../mytharchive/themeselector.cpp" line="204"/>
        <location filename="../mytharchive/themeselector.cpp" line="216"/>
        <source>No theme description file found!</source>
        <translation>Ingen beskrivelsesfil for tema funnet!</translation>
    </message>
    <message>
        <location filename="../mytharchive/themeselector.cpp" line="229"/>
        <source>Empty theme description!</source>
        <translation>Tom temabeskrivelse!</translation>
    </message>
    <message>
        <location filename="../mytharchive/themeselector.cpp" line="234"/>
        <source>Unable to open theme description file!</source>
        <translation>Kunne ikke åpne beskrivelsesfil for tema!</translation>
    </message>
</context>
<context>
    <name>ExportNative</name>
    <message>
        <location filename="../mytharchive/exportnative.cpp" line="200"/>
        <source>You need to add at least one item to archive!</source>
        <translation>Du må legge til minst ett element for å arkivere!</translation>
    </message>
    <message>
        <location filename="../mytharchive/exportnative.cpp" line="352"/>
        <source>Remove Item</source>
        <translation>Fjern element</translation>
    </message>
    <message>
        <location filename="../mytharchive/exportnative.cpp" line="444"/>
        <source>It was not possible to create the DVD. An error occured when running the scripts</source>
        <translation>Klarte ikke lage DVD&apos;en; en feil oppstod under kjøringen av skriptene</translation>
    </message>
    <message>
        <location filename="../mytharchive/exportnative.cpp" line="480"/>
        <source>You don&apos;t have any videos!</source>
        <translation>Du har ingen videoer!</translation>
    </message>
    <message>
        <location filename="../mytharchive/exportnative.cpp" line="345"/>
        <source>Menu</source>
        <translation>Meny</translation>
    </message>
</context>
<context>
    <name>FileSelector</name>
    <message>
        <location filename="../mytharchive/fileselector.cpp" line="282"/>
        <source>The selected item is not a directory!</source>
        <translation>Det valgte element er ikke en katalog!</translation>
    </message>
    <message>
        <location filename="../mytharchive/fileselector.cpp" line="61"/>
        <source>Find File</source>
        <translation>Finn fil</translation>
    </message>
    <message>
        <location filename="../mytharchive/fileselector.cpp" line="64"/>
        <source>Find Directory</source>
        <translation>Finn mappe</translation>
    </message>
    <message>
        <location filename="../mytharchive/fileselector.cpp" line="67"/>
        <source>Find Files</source>
        <translation>Finn filer</translation>
    </message>
</context>
<context>
    <name>ImportNative</name>
    <message>
        <location filename="../mytharchive/importnative.cpp" line="394"/>
        <source>You need to select a valid channel id!</source>
        <translation>Du må velge en gydlig kanal-id!</translation>
    </message>
    <message>
        <location filename="../mytharchive/importnative.cpp" line="425"/>
        <source>It was not possible to import the Archive.  An error occured when running &apos;mytharchivehelper&apos;</source>
        <translation>Klarte ikke importere arkivet; en feil oppsetod ved kjøring av «mytharchivehelper»</translation>
    </message>
    <message>
        <location filename="../mytharchive/importnative.cpp" line="555"/>
        <source>Select a channel id</source>
        <translation>Velg en kanalid</translation>
    </message>
    <message>
        <location filename="../mytharchive/importnative.cpp" line="581"/>
        <source>Select a channel number</source>
        <translation>Velg et kanalnummer</translation>
    </message>
    <message>
        <location filename="../mytharchive/importnative.cpp" line="607"/>
        <source>Select a channel name</source>
        <translation>Velg et kanalnavn</translation>
    </message>
    <message>
        <location filename="../mytharchive/importnative.cpp" line="633"/>
        <source>Select a Callsign</source>
        <translation>Velg et kanaltegn</translation>
    </message>
</context>
<context>
    <name>LogViewer</name>
    <message>
        <location filename="../mytharchive/logviewer.cpp" line="344"/>
        <source>Show Progress Log</source>
        <translation>Vis fremdriftslogg</translation>
    </message>
    <message>
        <location filename="../mytharchive/logviewer.cpp" line="345"/>
        <source>Show Full Log</source>
        <translation>Vis hele loggen</translation>
    </message>
    <message>
        <location filename="../mytharchive/logviewer.cpp" line="340"/>
        <source>Don&apos;t Auto Update</source>
        <translation>Ikke oppdater automatisk</translation>
    </message>
    <message>
        <location filename="../mytharchive/logviewer.cpp" line="76"/>
        <source>Cannot find any logs to show!</source>
        <translation>Kan ikke finne noen logger å vise!</translation>
    </message>
    <message>
        <location filename="../mytharchive/logviewer.cpp" line="191"/>
        <source>Background creation has been asked to stop.
This may take a few minutes.</source>
        <translation>Bakgrunnslagingen har blitt bedt om å stoppe. 
Dette kan ta et par minutter.</translation>
    </message>
    <message>
        <location filename="../mytharchive/logviewer.cpp" line="342"/>
        <source>Auto Update</source>
        <translation>Oppdater automatisk</translation>
    </message>
    <message>
        <location filename="../mytharchive/logviewer.cpp" line="332"/>
        <source>Menu</source>
        <translation>Meny</translation>
    </message>
</context>
<context>
    <name>MythBurn</name>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="330"/>
        <location filename="../mytharchive/mythburn.cpp" line="452"/>
        <source>No Cut List</source>
        <translation>Ingen kuttliste</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="341"/>
        <source>You need to add at least one item to archive!</source>
        <translation>Du må legge til minst ett element for å arkivere!</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="389"/>
        <source>Retrieving File Information. Please Wait...</source>
        <translation>Henter filinformasjon. Vennligst vent...</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="455"/>
        <source>Encoder: </source>
        <translation>Koder: </translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="319"/>
        <location filename="../mytharchive/mythburn.cpp" line="441"/>
        <source>Using Cut List</source>
        <translation>Bruker kuttliste</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="324"/>
        <location filename="../mytharchive/mythburn.cpp" line="446"/>
        <source>Not Using Cut List</source>
        <translation>Bruker ikke kuttliste</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="778"/>
        <source>Don&apos;t Use Cut List</source>
        <translation>Ikke bruk kuttliste</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="783"/>
        <source>Use Cut List</source>
        <translation>Bruk kuttliste</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="788"/>
        <source>Remove Item</source>
        <translation>Fjern element</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="789"/>
        <source>Edit Details</source>
        <translation>Rediger detaljer</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="790"/>
        <source>Change Encoding Profile</source>
        <translation>Endre kodingsprofil</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="791"/>
        <source>Edit Thumbnails</source>
        <translation>Rediger miniatyrbilder</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="926"/>
        <source>It was not possible to create the DVD.  An error occured when running the scripts</source>
        <translation>Klarte ikke lage DVD&apos;en; en feil oppstod under kjøringen av skriptene</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="968"/>
        <source>You don&apos;t have any videos!</source>
        <translation>Du har ingen videoer!</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="767"/>
        <source>Menu</source>
        <translation>meny</translation>
    </message>
</context>
<context>
    <name>MythControls</name>
    <message>
        <location filename="../mytharchive/mytharchive.cpp" line="329"/>
        <source>Toggle use cut list state for selected program</source>
        <translation>Slå av/på bruk av kuttliste for valgt program</translation>
    </message>
    <message>
        <location filename="../mytharchive/mytharchive.cpp" line="332"/>
        <source>Create DVD</source>
        <translation>Lag DVD</translation>
    </message>
    <message>
        <location filename="../mytharchive/mytharchive.cpp" line="334"/>
        <source>Create Archive</source>
        <translation>Lag arkiv</translation>
    </message>
    <message>
        <location filename="../mytharchive/mytharchive.cpp" line="336"/>
        <source>Import Archive</source>
        <translation>Importer arkiv</translation>
    </message>
    <message>
        <location filename="../mytharchive/mytharchive.cpp" line="338"/>
        <source>View Archive Log</source>
        <translation>Vis arkivlogg</translation>
    </message>
    <message>
        <location filename="../mytharchive/mytharchive.cpp" line="340"/>
        <source>Play Created DVD</source>
        <translation>Spill DVD som er laget</translation>
    </message>
    <message>
        <location filename="../mytharchive/mytharchive.cpp" line="342"/>
        <source>Burn DVD</source>
        <translation>Brenn DVD</translation>
    </message>
</context>
<context>
    <name>RecordingSelector</name>
    <message>
        <location filename="../mytharchive/recordingselector.cpp" line="110"/>
        <source>Retrieving Recording List.
Please Wait...</source>
        <translation>Henter opptaksliste.
Vennligst vent...</translation>
    </message>
    <message>
        <location filename="../mytharchive/recordingselector.cpp" line="191"/>
        <source>Clear All</source>
        <translation>Avmerk alle</translation>
    </message>
    <message>
        <location filename="../mytharchive/recordingselector.cpp" line="192"/>
        <source>Select All</source>
        <translation>Velg alle</translation>
    </message>
    <message>
        <location filename="../mytharchive/recordingselector.cpp" line="89"/>
        <location filename="../mytharchive/recordingselector.cpp" line="376"/>
        <location filename="../mytharchive/recordingselector.cpp" line="481"/>
        <source>All Recordings</source>
        <translation>Alle opptak</translation>
    </message>
    <message>
        <location filename="../mytharchive/recordingselector.cpp" line="134"/>
        <source>Either you don&apos;t have any recordings or no recordings are available locally!</source>
        <translation>Enter har du ingen opptak eller så er ikke opptakene tilgjengelig lokalt!</translation>
    </message>
    <message>
        <location filename="../mytharchive/recordingselector.cpp" line="184"/>
        <source>Menu</source>
        <translation>Meny</translation>
    </message>
</context>
<context>
    <name>SelectDestination</name>
    <message>
        <location filename="../mytharchive/archiveutil.cpp" line="29"/>
        <source>Single Layer DVD</source>
        <translation>Enkeltlags DVD</translation>
    </message>
    <message>
        <location filename="../mytharchive/archiveutil.cpp" line="33"/>
        <source>Dual Layer DVD</source>
        <translation>Dobbeltlags DVD</translation>
    </message>
    <message>
        <location filename="../mytharchive/archiveutil.cpp" line="30"/>
        <source>Single Layer DVD (4,482 MB)</source>
        <translation>Enkeltlags DVD (4,482 MB)</translation>
    </message>
    <message>
        <location filename="../mytharchive/archiveutil.cpp" line="34"/>
        <source>Dual Layer DVD (8,964 MB)</source>
        <translation>Dobbeltlags DVD (8,964 MB)</translation>
    </message>
    <message>
        <location filename="../mytharchive/archiveutil.cpp" line="37"/>
        <source>DVD +/- RW</source>
        <translation>DVD +/- RW</translation>
    </message>
    <message>
        <location filename="../mytharchive/archiveutil.cpp" line="38"/>
        <source>Rewritable DVD</source>
        <translation>Rewritable DVD</translation>
    </message>
    <message>
        <location filename="../mytharchive/archiveutil.cpp" line="41"/>
        <source>File</source>
        <translation>Fil</translation>
    </message>
    <message>
        <location filename="../mytharchive/archiveutil.cpp" line="42"/>
        <source>Any file accessable from your filesystem.</source>
        <translation>Enhver fil tilgjengelig i ditt filsystem.</translation>
    </message>
    <message>
        <location filename="../mytharchive/selectdestination.cpp" line="264"/>
        <location filename="../mytharchive/selectdestination.cpp" line="320"/>
        <source>Unknown</source>
        <translation>Ukjent</translation>
    </message>
</context>
<context>
    <name>ThemeUI</name>
    <message>
        <location filename="themestrings.h" line="178"/>
        <source>Select Destination</source>
        <translation>Velg destinasjon</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="229"/>
        <source>description goes here.</source>
        <translation>beskrivelse her.</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="114"/>
        <source>Free Space:</source>
        <translation>Ledig plass:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="129"/>
        <source>Make ISO Image</source>
        <translation>Lag ISO image</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="44"/>
        <source>Burn to DVD</source>
        <translation>Brenn til DVD</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="112"/>
        <source>Force Overwrite of DVD-RW Media</source>
        <translation>Tving overskriving av DVD RW medie</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="182"/>
        <source>Select Recordings</source>
        <translation>Velg opptak</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="193"/>
        <source>Show Recordings</source>
        <translation>Vis opptak</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="102"/>
        <source>File Finder</source>
        <translation>Filfinner</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="6"/>
        <source>%SIZE% ~ %PROFILE%</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="8"/>
        <source>%size% (%profile%)</source>
        <translation type="unfinished">%date% / %profile%</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="12"/>
        <source>0.00Gb</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="16"/>
        <source>12.34GB</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="17"/>
        <source>&lt;-- Details</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="18"/>
        <source>&lt;-- Main Menu</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="25"/>
        <source>Add Recordings</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="27"/>
        <source>Add Videos</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="28"/>
        <source>Add a recording or video to archive.</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="29"/>
        <source>Add a recording, video or file to archive.</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="32"/>
        <source>Archive Files</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="35"/>
        <source>Archive Items to DVD</source>
        <translation type="unfinished">Kanal nr for arkiv:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="36"/>
        <source>Archive Log Viewer</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="39"/>
        <source>Associate Channel</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="42"/>
        <source>Burn Created DVD</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="43"/>
        <source>Burn the last created archive to DVD</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="45"/>
        <source>Burn to DVD:</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="46"/>
        <source>CUTLIST</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="48"/>
        <source>Callsign:  %1</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="50"/>
        <source>Cancel Job</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="52"/>
        <source>Chan. Id:</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="54"/>
        <source>Change</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="57"/>
        <source>Channel ID:  %1</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="59"/>
        <source>Channel Number:  %1</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="62"/>
        <source>Chapter Menu --&gt;</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="63"/>
        <source>Chapters</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="69"/>
        <source>Create ISO Image:</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="71"/>
        <source>Create a DVD of your videos</source>
        <translation type="unfinished"></translation>
    </message>
    <message numerus="yes">
        <location filename="themestrings.h" line="74"/>
        <source>Current: %n</source>
        <translation type="unfinished">
            <numerusform></numerusform>
            <numerusform></numerusform>
        </translation>
    </message>
    <message>
        <location filename="themestrings.h" line="76"/>
        <source>DVD Media</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="77"/>
        <source>DVD Menu</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="81"/>
        <source>Destination Free Space:</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="85"/>
        <source>Edit Archive item Information</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="86"/>
        <source>Edit Details</source>
        <translation type="unfinished">Rediger detaljer</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="87"/>
        <source>Edit Thumbnails</source>
        <translation type="unfinished">Rediger miniatyrbilder</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="91"/>
        <source>Encode your video to a file</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="92"/>
        <source>Encoded Size:</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="93"/>
        <source>Encoding Profile</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="95"/>
        <source>Error: %1</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="97"/>
        <source>Export</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="99"/>
        <source>Export your videos</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="101"/>
        <source>File Browser</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="104"/>
        <source>File...</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="127"/>
        <source>Main Menu</source>
        <oldsource>Filename:</oldsource>
        <translation type="unfinished">Hovedmeny</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="111"/>
        <source>First, select the thumb image you want to change from the overview. Second, press the &apos;tab&apos; key to move to the select thumb frame button to change the image.</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="116"/>
        <source>Import</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="119"/>
        <source>Import recordings from a native archive</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="120"/>
        <source>Import videos from an Archive</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="122"/>
        <source>Intro --&gt;</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="126"/>
        <source>MENU changes focus. Numbers 0-9 jump to that thumb image.
When the preview image has focus, UP/DOWN changes the seek amount, LEFT/RIGHT jumps forward/backward by the seek amount, and SELECT chooses the current preview image for the selected thumb image.</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="128"/>
        <source>Main menu</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="130"/>
        <source>Make ISO image</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="133"/>
        <source>Metadata</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="135"/>
        <source>Name:  %1</source>
        <translation type="unfinished"></translation>
    </message>
    <message numerus="yes">
        <location filename="themestrings.h" line="139"/>
        <source>New: %n</source>
        <translation type="unfinished">
            <numerusform></numerusform>
            <numerusform></numerusform>
        </translation>
    </message>
    <message>
        <location filename="themestrings.h" line="141"/>
        <source>No files are selected for DVD</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="144"/>
        <source>Not Available in this Theme</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="145"/>
        <source>Not available in this theme</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="150"/>
        <source>Original Size:</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="152"/>
        <source>Overwrite DVD-RW Media:</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="155"/>
        <source>Parental Level:</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="159"/>
        <source>Play the last created archive DVD</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="160"/>
        <source>Position View:</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="161"/>
        <source>Press &apos;left&apos; and &apos;right&apos; arrows to move through the frames. Press &apos;up&apos; and &apos;down&apos; arrows to change seek amount. Press &apos;select&apos; or &apos;enter&apos; key to lock the selected thumb image.</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="163"/>
        <source>Preview</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="168"/>
        <source>Save recordings and videos to a native archive</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="169"/>
        <source>Save recordings and videos to video DVD</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="175"/>
        <source>Seek amount:</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="181"/>
        <source>Select Files</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="183"/>
        <source>Select Videos</source>
        <translation>Velg videoer</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="188"/>
        <source>Select thumb image:</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="189"/>
        <source>Select your DVD menu theme</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="190"/>
        <source>Selected Items to be burned</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="191"/>
        <source>Sep 13, 2004 11:00 pm (1h 15m)</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="195"/>
        <source>Show log with archive activities</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="196"/>
        <source>Show the Archive Log Viewer</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="197"/>
        <source>Size:</source>
        <translation type="unfinished"></translation>
    </message>
    <message numerus="yes">
        <location filename="themestrings.h" line="198"/>
        <source>Size: %n</source>
        <translation type="unfinished">
            <numerusform></numerusform>
            <numerusform></numerusform>
        </translation>
    </message>
    <message>
        <location filename="themestrings.h" line="201"/>
        <source>Start Time: %1</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="203"/>
        <source>Start to Burn your archive to DVD</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="205"/>
        <source>Subtitle: %1</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="206"/>
        <source>Test</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="208"/>
        <source>Test created DVD</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="209"/>
        <source>Theme description</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="210"/>
        <source>Theme for the DVD menu:</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="211"/>
        <source>Theme preview:</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="216"/>
        <source>Title: %1</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="217"/>
        <source>Tools and Utilities for archives</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="221"/>
        <source>Used: %1</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="223"/>
        <source>Utilities for MythArchive</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="224"/>
        <source>Video Category</source>
        <translation>Videokategori</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="228"/>
        <source>XML File to Import</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="230"/>
        <source>frame</source>
        <translation type="unfinished"></translation>
    </message>
    <message numerus="yes">
        <location filename="themestrings.h" line="231"/>
        <source>profile: %n</source>
        <translation type="unfinished">
            <numerusform></numerusform>
            <numerusform></numerusform>
        </translation>
    </message>
    <message>
        <location filename="themestrings.h" line="233"/>
        <source>title goes here</source>
        <translation>tittel her</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="234"/>
        <source>to</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="236"/>
        <source>x.xx Gb</source>
        <translation>x.xx Gb</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="239"/>
        <source>~</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="153"/>
        <source>PL:</source>
        <translation>PL:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="14"/>
        <source>1</source>
        <translation>1</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="143"/>
        <source>No videos available</source>
        <translation>Ingen videoer tilgjengelig</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="125"/>
        <source>Log Viewer</source>
        <translation>Loggviser</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="55"/>
        <source>Change Encoding Profile</source>
        <translation>Endre kodingsprofil</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="78"/>
        <source>DVD Menu Theme</source>
        <translation>DVD menytema</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="184"/>
        <source>Select a theme</source>
        <translation>Velg et tema</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="121"/>
        <source>Intro</source>
        <translation>Intro</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="127"/>
        <source>Main Menu</source>
        <translation>Hovedmeny</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="61"/>
        <source>Chapter Menu</source>
        <translation>Kapittelmeny</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="84"/>
        <source>Details</source>
        <translation>Detaljer</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="176"/>
        <source>Select Archive Items</source>
        <translation>Velg arkivelementer</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="142"/>
        <source>No files are selected for archive</source>
        <translation>Ingen filer er valgt for arkivet</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="33"/>
        <source>Archive Item Details</source>
        <translation>Detaljer for arkivelement</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="215"/>
        <source>Title:</source>
        <translation>Tittel:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="204"/>
        <source>Subtitle:</source>
        <translation>Undertekst:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="199"/>
        <source>Start Date:</source>
        <translation>Startdato:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="214"/>
        <source>Time:</source>
        <translation>Tid:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="80"/>
        <source>Description:</source>
        <translation>Beskrivelse:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="213"/>
        <source>Thumb Image Selector</source>
        <translation>velger for miniatyrbilde</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="72"/>
        <source>Current Position</source>
        <translation>Gjeldende posisjon</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="13"/>
        <source>0:00:00.00</source>
        <translation>0:00:00.00</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="227"/>
        <source>Write video to data dvd or file</source>
        <translation type="unfinished">Skriv video til data-dvd eller fil</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="100"/>
        <source>File</source>
        <translation type="unfinished">Fil</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="109"/>
        <source>Find Location</source>
        <translation type="unfinished">Finn lokasjon</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="136"/>
        <source>Navigation</source>
        <translation type="unfinished">Navigasjon</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="105"/>
        <source>Filesize</source>
        <translation type="unfinished">Filstørrelse</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="79"/>
        <source>Date (time)</source>
        <translation type="unfinished">Dato (tid)</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="194"/>
        <source>Show Videos</source>
        <translation type="unfinished">Vis videoer</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="154"/>
        <source>Parental Level</source>
        <translation type="unfinished">Foreldrenivå</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="38"/>
        <source>Archived Channel</source>
        <oldsource>Archive Item:</oldsource>
        <translation type="unfinished">Arkiver element:</translation>
    </message>
    <message>
        <source>Encoder Profile:</source>
        <translation type="obsolete">Kodingsprofil:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="149"/>
        <source>Old size:</source>
        <translation>Gjeldende størrelse:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="137"/>
        <source>New Size:</source>
        <translation>Ny størrelse:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="75"/>
        <source>DVD</source>
        <translation type="unfinished">DVD</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="70"/>
        <source>Create a</source>
        <translation type="unfinished">Lag en</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="226"/>
        <source>Videos</source>
        <translation type="unfinished">Videoer</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="98"/>
        <source>Export your</source>
        <translation type="unfinished">Eksporter dine</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="90"/>
        <source>Encode your video</source>
        <translation type="unfinished">Kod din video</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="31"/>
        <source>Archive</source>
        <translation type="unfinished">Arkiv</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="118"/>
        <source>Import an</source>
        <translation type="unfinished">Importer et</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="222"/>
        <source>Utilities</source>
        <translation type="unfinished">Verktøy</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="220"/>
        <source>Use archive</source>
        <translation type="unfinished">Bruk arkiv</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="131"/>
        <source>Media</source>
        <translation type="unfinished">Media</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="88"/>
        <source>Eject your</source>
        <translation type="unfinished">Løs ut din</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="124"/>
        <source>Log</source>
        <translation type="unfinished">Logg</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="192"/>
        <source>Show</source>
        <translation type="unfinished">Vis</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="207"/>
        <source>Test created</source>
        <translation type="unfinished">Test laget</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="41"/>
        <source>Burn</source>
        <translation type="unfinished">Brenn</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="202"/>
        <source>Start to</source>
        <translation type="unfinished">Start med å</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="10"/>
        <source>0 mb</source>
        <translation>0 mb</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="238"/>
        <source>xxxxx mb</source>
        <translation>xxxxx mb</translation>
    </message>
    <message>
        <source>Current Size:</source>
        <translation type="obsolete">Gjeldende størrelse:</translation>
    </message>
    <message>
        <source>Not Applicable</source>
        <translation type="obsolete">Ikke relevant</translation>
    </message>
    <message>
        <source>Intro:</source>
        <translation type="obsolete">Intro:</translation>
    </message>
    <message>
        <source>Main Menu:</source>
        <translation type="obsolete">Hovedmeny:</translation>
    </message>
    <message>
        <source>Chapter Menu:</source>
        <translation type="obsolete">Kapittelmeny:</translation>
    </message>
    <message>
        <source>Details:</source>
        <translation type="obsolete">Detaljer:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="8"/>
        <source>%size% (%profile%)</source>
        <oldsource>%date% / %profile%</oldsource>
        <translation type="unfinished">%date% / %profile%</translation>
    </message>
    <message>
        <source>Current size: %1</source>
        <translation type="obsolete">Gjeldende størrelse: %1</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="73"/>
        <source>Current Position:</source>
        <translation type="unfinished">Gjeldende posisjon:</translation>
    </message>
    <message>
        <source>Seek Amount:</source>
        <translation type="obsolete">Søkemengde:</translation>
    </message>
    <message>
        <source>increase seek amount</source>
        <translation type="obsolete">Øk søkemengde</translation>
    </message>
    <message>
        <source>decrease seek amount</source>
        <translation type="obsolete">reduser søkemengde</translation>
    </message>
    <message>
        <source>move left</source>
        <translation type="obsolete">Flytt til venstre</translation>
    </message>
    <message>
        <source>move right</source>
        <translation type="obsolete">Flytt til høyre</translation>
    </message>
    <message>
        <source>Read video from data dvd or file</source>
        <translation type="obsolete">Les video fra data-dvd eller fil</translation>
    </message>
    <message>
        <source>Associated Channel</source>
        <translation type="obsolete">Assosiert kanal</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="34"/>
        <source>Archive Items</source>
        <oldsource>Archive Chan ID:</oldsource>
        <translation type="unfinished">Kanal ID for arkiv:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="35"/>
        <source>Archive Items to DVD</source>
        <oldsource>Archive Chan No:</oldsource>
        <translation type="unfinished">Kanal nr for arkiv:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="33"/>
        <source>Archive Item Details</source>
        <oldsource>Archive Callsign:</oldsource>
        <translation type="unfinished">Kallesignal for arkiv:</translation>
    </message>
    <message>
        <source>Archive Name:</source>
        <translation type="obsolete">Arkivnavn:</translation>
    </message>
    <message>
        <source>Local Chan ID:</source>
        <translation type="obsolete">Lokalt kanal-ID:</translation>
    </message>
    <message>
        <source>Local Chan No:</source>
        <translation type="obsolete">Lokalt kanal-nr:</translation>
    </message>
    <message>
        <source>Local Callsign:</source>
        <translation type="obsolete">Lokalt kallesignal:</translation>
    </message>
    <message>
        <source>Local Name:</source>
        <translation type="obsolete">Lokalt navn:</translation>
    </message>
    <message>
        <source>Search Local</source>
        <translation type="obsolete">Søk lokalt</translation>
    </message>
    <message>
        <source>Channel ID</source>
        <translation type="obsolete">Kanal ID</translation>
    </message>
    <message>
        <source>Channel No</source>
        <translation type="obsolete">Kanal nr</translation>
    </message>
    <message>
        <source>Callsign</source>
        <translation type="obsolete">Kallesignal</translation>
    </message>
    <message>
        <source>Name</source>
        <translation type="obsolete">Navn</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="56"/>
        <source>Channel ID:</source>
        <translation>Kanal ID:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="58"/>
        <source>Channel Number:</source>
        <translation>Kanalnummer:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="174"/>
        <source>Seek Amount</source>
        <translation>Søkemengde</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="113"/>
        <source>Frame</source>
        <translation>Ramme</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="218"/>
        <source>Up Level</source>
        <translation>Opp et nivå</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="237"/>
        <source>xxxxx MB</source>
        <translation>xxxxx MB</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="9"/>
        <source>0 MB</source>
        <translation>0 MB</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="232"/>
        <source>sep 13, 2004 11:00 pm (1h 15m)</source>
        <translation>13 sept, 2004 11:00 (1t 15min)</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="235"/>
        <source>x.xx GB</source>
        <translation>x.xx GB</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="103"/>
        <source>File Finder To Import</source>
        <translation>Filfinner for importering</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="200"/>
        <source>Start Time:</source>
        <translation>Starttid:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="177"/>
        <source>Select Associated Channel</source>
        <translation>Velg assosiert kanal</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="38"/>
        <source>Archived Channel</source>
        <translation>Arkivert kanal</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="51"/>
        <source>Chan. ID:</source>
        <translation>Kanal ID:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="53"/>
        <source>Chan. No:</source>
        <translation>Kanal Nr:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="47"/>
        <source>Callsign:</source>
        <translation>Kallesignal:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="134"/>
        <source>Name:</source>
        <translation>Navn:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="123"/>
        <source>Local Channel</source>
        <translation>Lokal kanal</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="20"/>
        <source>A high quality profile giving approx. 1 hour of video on a single layer DVD</source>
        <translation>En profil med høy kvalitet som gir omtrent 1 time med video på en enkeltlags DVD</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="22"/>
        <source>A standard play profile giving approx. 2 hour of video on a single layer DVD</source>
        <translation>En profil med standard kvalitet som gir omtrent 2 timer med video på en enkeltlags DVD</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="21"/>
        <source>A long play profile giving approx. 4 hour of video on a single layer DVD</source>
        <translation>En profil med lang avspillingstid som gir omtrent 4 timer med video på en enkeltlags DVD</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="19"/>
        <source>A extended play profile giving approx. 6 hour of video on a single layer DVD</source>
        <translation>En profil med utvidet avspillingstid som gir omtrent 6 timer med video på en enkeltlags DVD</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="106"/>
        <source>Filesize: %1</source>
        <translation>Filstørrelse: %1</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="166"/>
        <source>Recorded Time: %1</source>
        <translation>Tid tatt opp: %1</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="7"/>
        <source>%&quot;|SUBTITLE|&quot; %%(|ORIGINALAIRDATE|) %%(|STARS|) %%DESCRIPTION%</source>
        <translation>%&quot;|SUBTITLE|&quot; %%(|ORIGINALAIRDATE|) %%(|STARS|) %%DESCRIPTION%</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="156"/>
        <source>Parental Level: %1</source>
        <translation>Foreldrenivå: %1</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="34"/>
        <source>Archive Items</source>
        <translation>Arkiver elementer</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="165"/>
        <source>Profile:</source>
        <translation>Profil:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="148"/>
        <source>Old Size:</source>
        <translation>Gjeldende størrelse:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="179"/>
        <source>Select Destination:</source>
        <translation>Velg destinasjon:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="157"/>
        <source>Parental level: %1</source>
        <translation>Foreldrenivå: %1</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="138"/>
        <source>New size:</source>
        <translation>Ny størrelse:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="185"/>
        <source>Select a theme:</source>
        <translation>Velg et tema:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="132"/>
        <source>Menu</source>
        <translation>Meny</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="60"/>
        <source>Chapter</source>
        <translation>Kapittel</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="83"/>
        <source>Detail</source>
        <translation>Detalj</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="180"/>
        <source>Select File to Import</source>
        <translation>Velg fil som skal importeres</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="68"/>
        <source>Create DVD</source>
        <translation>Lag DVD</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="67"/>
        <source>Create Archive</source>
        <translation>Lag arkiv</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="89"/>
        <source>Encode Video File</source>
        <translation>Kod videofil</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="117"/>
        <source>Import Archive</source>
        <translation>Importer arkiv</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="37"/>
        <source>Archive Utilities</source>
        <translation>Arkiv-verktøy</translation>
    </message>
    <message>
        <source>Show Log Viewer</source>
        <translation type="vanished">Vis loggviser</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="158"/>
        <source>Play Created DVD</source>
        <translation>Spill DVD som er laget</translation>
    </message>
    <message>
        <source>Burn DVD</source>
        <translation type="vanished">Brenn DVD</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="65"/>
        <source>Choose where you would like your files archived.</source>
        <translation>Velg hvor du vil at filene dine skal bli arkivert.</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="5"/>
        <source>%DATE%, %TIME%</source>
        <translation>%DATE%, %TIME%</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="151"/>
        <source>Output Type:</source>
        <translation>Utdata type:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="82"/>
        <source>Destination:</source>
        <translation>Mål:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="66"/>
        <source>Click here to find an output location...</source>
        <translation>Klikk her for å finne et utdatasted...</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="94"/>
        <source>Erase DVD-RW before burning</source>
        <translation>Slett DVD-RW før brenning</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="49"/>
        <source>Cancel</source>
        <translation>Avbryt</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="164"/>
        <source>Previous</source>
        <translation>Forrige</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="140"/>
        <source>Next</source>
        <translation>Neste</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="107"/>
        <source>Filter:</source>
        <translation>Filter:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="146"/>
        <source>OK</source>
        <translation>OK</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="186"/>
        <source>Select the file you wish to use.</source>
        <translation>Velg hvilken fil du vil bruke.</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="40"/>
        <source>Back</source>
        <translation>Tilbake</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="115"/>
        <source>Home</source>
        <translation>Hjem</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="173"/>
        <source>See logs from your archive runs.</source>
        <translation>Se logger fra arkiveringskjøringen.</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="219"/>
        <source>Update</source>
        <translation>Oppdater</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="96"/>
        <source>Exit</source>
        <translation>Avslutt</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="11"/>
        <source>0.00 GB</source>
        <translation>0.00 GB</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="225"/>
        <source>Video Category:</source>
        <translation>Videokategori:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="147"/>
        <source>Ok</source>
        <translation>OK</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="15"/>
        <source>12.34 GB</source>
        <translation>12.34 GB</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="64"/>
        <source>Choose the appearance of your DVD.</source>
        <translation>Velg utseende for DVDen din.</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="212"/>
        <source>Theme:</source>
        <translation>Tema:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="187"/>
        <source>Select the recordings and videos you wish to save.</source>
        <translation>Velg hvilke opptak og videoer du vil lagre.</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="24"/>
        <source>Add Recording</source>
        <translation>Legg til opptak</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="26"/>
        <source>Add Video</source>
        <translation>Legg til video</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="23"/>
        <source>Add File</source>
        <translation>Legg til fil</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="167"/>
        <source>Save</source>
        <translation>Lagre</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="108"/>
        <source>Find</source>
        <translation>Finn</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="162"/>
        <source>Prev</source>
        <translation>Forrige</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="171"/>
        <source>Search Channel</source>
        <translation>Søk i kanal</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="170"/>
        <source>Search Callsign</source>
        <translation>Søk i kallesignal</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="172"/>
        <source>Search Name</source>
        <translation>Søk i navn</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="110"/>
        <source>Finish</source>
        <translation>Fullfør</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="30"/>
        <source>Add video</source>
        <translation>Legg til video</translation>
    </message>
</context>
<context>
    <name>ThumbFinder</name>
    <message>
        <location filename="../mytharchive/thumbfinder.cpp" line="870"/>
        <source>Exit, Save Thumbnails</source>
        <translation>Avslutt, og lagre miniatyrbildene</translation>
    </message>
    <message>
        <location filename="../mytharchive/thumbfinder.cpp" line="871"/>
        <source>Exit, Don&apos;t Save Thumbnails</source>
        <translation>Avslutt, Ikke lagre miniatyrbildene</translation>
    </message>
    <message>
        <location filename="../mytharchive/thumbfinder.cpp" line="863"/>
        <source>Menu</source>
        <translation>Meny</translation>
    </message>
</context>
<context>
    <name>VideoSelector</name>
    <message>
        <location filename="../mytharchive/videoselector.cpp" line="150"/>
        <source>Clear All</source>
        <translation>Velg ingen</translation>
    </message>
    <message>
        <location filename="../mytharchive/videoselector.cpp" line="151"/>
        <source>Select All</source>
        <translation>Velg alle</translation>
    </message>
    <message>
        <location filename="../mytharchive/videoselector.cpp" line="333"/>
        <location filename="../mytharchive/videoselector.cpp" line="501"/>
        <source>All Videos</source>
        <translation>Alle videoer</translation>
    </message>
    <message>
        <location filename="../mytharchive/videoselector.cpp" line="554"/>
        <source>You need to enter a valid password for this parental level</source>
        <translation>Du må angi et gyldig passord for dette foreldrenivået</translation>
    </message>
    <message>
        <location filename="../mytharchive/videoselector.cpp" line="143"/>
        <source>Menu</source>
        <translation>Meny</translation>
    </message>
</context>
</TS>
