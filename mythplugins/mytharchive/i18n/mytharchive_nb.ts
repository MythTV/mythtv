<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE TS>
<TS version="2.0" language="nb_NO">
<context>
    <name>(ArchiveUtils)</name>
    <message>
        <source>Cannot find the MythArchive work directory.
Have you set the correct path in the settings?</source>
        <translation>Kan ikke finne arbeidskatalog for Myth-arkiv.
Har du satt opp riktig katalog in instillingene?</translation>
    </message>
</context>
<context>
    <name>(MythArchiveMain)</name>
    <message>
        <source>Found a lock file but the owning process isn&apos;t running!
Removing stale lock file.</source>
        <translation>Fant en låsfil men prosessen som eier denne kjører ikke!
Fjerner gammel låsfil.</translation>
    </message>
    <message>
        <source>Last run did not create a playable DVD.</source>
        <translation>Forrige kjøring produserte ikke en spillbar DVD.</translation>
    </message>
    <message>
        <source>Last run failed to create a DVD.</source>
        <translation>Forrige kjøring klarte ikke å lage en DVD.</translation>
    </message>
</context>
<context>
    <name>ArchiveFileSelector</name>
    <message>
        <source>Find File To Import</source>
        <translation>Finn fil som skal importeres</translation>
    </message>
    <message>
        <source>The selected item is not a valid archive file!</source>
        <translation>Det valgte elementet er ikke en gyld arkivfil!</translation>
    </message>
</context>
<context>
    <name>ArchiveSettings</name>
    <message>
        <source>MythArchive Temp Directory</source>
        <translation>Mytharkivs katalog for midlertidige filer</translation>
    </message>
    <message>
        <source>Location where MythArchive should create its temporary work files. LOTS of free space required here.</source>
        <translation>Plassering hvor Myth-arkiv skal lagre midlertidige arbeidsfiler. Her må det være MYE ledig plass.</translation>
    </message>
    <message>
        <source>MythArchive Share Directory</source>
        <translation>Mytharkivs katalog for delte filer</translation>
    </message>
    <message>
        <source>Location where MythArchive stores its scripts, intro movies and theme files</source>
        <translation>Plassering hvor Myth-arkiv lagrer sine skript, intro- og temafiler</translation>
    </message>
    <message>
        <source>Video format</source>
        <translation>Videoformat</translation>
    </message>
    <message>
        <source>Video format for DVD recordings, PAL or NTSC.</source>
        <translation>Videoformat for DVD-opptak, PAL eller NTSC.</translation>
    </message>
    <message>
        <source>File Selector Filter</source>
        <translation>Filvelgingsfilter</translation>
    </message>
    <message>
        <source>The file name filter to use in the file selector.</source>
        <translation>Filnavnfilter for bruk i filvelgeren.</translation>
    </message>
    <message>
        <source>Location of DVD</source>
        <translation>Plassering til DVD</translation>
    </message>
    <message>
        <source>Which DVD drive to use when burning discs.</source>
        <translation>Hvilken DVD-stasjon som skal brukes til brenning.</translation>
    </message>
    <message>
        <source>DVD Drive Write Speed</source>
        <translation>Skrivehastighet for DVD-stasjon</translation>
    </message>
    <message>
        <source>This is the write speed to use when burning a DVD. Set to 0 to allow growisofs to choose the fastest available speed.</source>
        <translation>Dette er skrivehastigheten som blir brukt under DVD-brenning. Sett denne til 0 hvis growisofs skal velge den raskeste tilgjengelige hastigheten.</translation>
    </message>
    <message>
        <source>Command to play DVD</source>
        <translation>Kommando for å spille DVD</translation>
    </message>
    <message>
        <source>Command to run when test playing a created DVD. &apos;Internal&apos; will use the internal MythTV player. %f will be replaced with the path to the created DVD structure eg. &apos;xine -pfhq --no-splash dvd:/%f&apos;.</source>
        <translation>Kommando som kjøres under testing av en ny DVD. Settes denne til &apos;Internal&apos; vil den interne MythtTV avspilleren brukes. %f vil bli erstattet av stien til DVD-strukturen, dvs. &apos;xine -pfhq --no-splash dvd:/%f&apos;. </translation>
    </message>
    <message>
        <source>Copy remote files</source>
        <translation>Kopier fjerne filer</translation>
    </message>
    <message>
        <source>If set files on remote filesystems will be copied over to the local filesystem before processing. Speeds processing and reduces bandwidth on the network</source>
        <translation>Kopierer filer på fjerne filsystemer over til det lokale filsystemet før de behandles. Gjør behandlingen raskere og senker bruken av båndbredde på nettverket</translation>
    </message>
    <message>
        <source>Always Use Mythtranscode</source>
        <translation>Alltid bruk Mythtranscode</translation>
    </message>
    <message>
        <source>If set mpeg2 files will always be passed though mythtranscode to clean up any errors. May help to fix some audio problems. Ignored if &apos;Use ProjectX&apos; is set.</source>
        <translation>Hvis på vil alltid MPEG-2-filer kjøres gjennom mythtranscode for å rette opp eventuelle feil. Dette kan kanskje ordne noen lydproblemer.</translation>
    </message>
    <message>
        <source>Use ProjectX</source>
        <translation>Bruk ProjectX</translation>
    </message>
    <message>
        <source>If set ProjectX will be used to cut commercials and split mpeg2 files instead of mythtranscode and mythreplex.</source>
        <translation>Hvis valgt vil ProjectX bli brukt til å fjerne reklamer og splitte mpeg2 filer istendenfor mythtranscode og mythreplex.</translation>
    </message>
    <message>
        <source>Use FIFOs</source>
        <translation>Bruk FIFO&apos;er</translation>
    </message>
    <message>
        <source>The script will use FIFOs to pass the output of mplex into dvdauthor rather than creating intermediate files. Saves time and disk space during multiplex operations but not supported on Windows platform</source>
        <translation>Gjør at skriptet bruker FIFO&apos;er for å sende utdata fra mplex til dvdauthor, i stedet for å lage midlertidige filer. Sparer tid og diskplass under multiplex-operasjoner, men virker ikke i Windows</translation>
    </message>
    <message>
        <source>Add Subtitles</source>
        <translation>Legg til undertekster</translation>
    </message>
    <message>
        <source>If available this option will add subtitles to the final DVD. Requires &apos;Use ProjectX&apos; to be on.</source>
        <translation>Hvis tilgjengelig vil denne instillingen legge til undertekster på DVDen. Krever at &apos;Bruk ProjectX&apos; er på.</translation>
    </message>
    <message>
        <source>Main Menu Aspect Ratio</source>
        <translation>Høyde/bredde-forhold i hovedmenyen</translation>
    </message>
    <message>
        <source>4:3</source>
        <comment>Aspect ratio</comment>
        <translation>4:3</translation>
    </message>
    <message>
        <source>16:9</source>
        <comment>Aspect ratio</comment>
        <translation>16:9</translation>
    </message>
    <message>
        <source>Aspect ratio to use when creating the main menu.</source>
        <translation>Høyde/bredde-forholdet som skal brukes ved laging av hovedmenyen.</translation>
    </message>
    <message>
        <source>Chapter Menu Aspect Ratio</source>
        <translation>Høyde/breddeforhold for kapittelmenyen</translation>
    </message>
    <message>
        <source>Video</source>
        <translation>Video</translation>
    </message>
    <message>
        <source>Aspect ratio to use when creating the chapter menu. &apos;%1&apos; means use the same aspect ratio as the associated video.</source>
        <extracomment>%1 is the translation of the &quot;Video&quot; combo box choice</extracomment>
        <translation>Høyde/breddeforhold som skal brukess ved laging av kapittelmenyen. &apos;%1&apos; betyr at det skal brukes det samme forholdet som i den tilknyttede videoen.</translation>
    </message>
    <message>
        <source>Date format</source>
        <translation>Datoformat</translation>
    </message>
    <message>
        <source>Samples are shown using today&apos;s date.</source>
        <translation>Eksempler vises med dagens dato.</translation>
    </message>
    <message>
        <source>Samples are shown using tomorrow&apos;s date.</source>
        <translation>Eksempler vises med dato for i morgen.</translation>
    </message>
    <message>
        <source>Your preferred date format to use on DVD menus. %1</source>
        <extracomment>%1 gives additional info on the date used</extracomment>
        <translation>Ditt prefererte datoformat i DVD menyen. %1</translation>
    </message>
    <message>
        <source>Time format</source>
        <translation>Tidsformat</translation>
    </message>
    <message>
        <source>Your preferred time format to display on DVD menus. You must choose a format with &quot;AM&quot; or &quot;PM&quot; in it, otherwise your time display will be 24-hour or &quot;military&quot; time.</source>
        <translation>Ditt prefererte tidsformat i DVD menyen. Du må velge et format med &quot;AM&quot; eller &quot;PM&quot;, ellers vil tidsvisningen være basert på 24 timer formatet.</translation>
    </message>
    <message>
        <source>Default Encoder Profile</source>
        <translation>Standard kodingsprofil</translation>
    </message>
    <message>
        <source>HQ</source>
        <comment>Encoder profile</comment>
        <translation>HQ</translation>
    </message>
    <message>
        <source>SP</source>
        <comment>Encoder profile</comment>
        <translation>SP</translation>
    </message>
    <message>
        <source>LP</source>
        <comment>Encoder profile</comment>
        <translation>LP</translation>
    </message>
    <message>
        <source>EP</source>
        <comment>Encoder profile</comment>
        <translation>EP</translation>
    </message>
    <message>
        <source>Default encoding profile to use if a file needs re-encoding.</source>
        <translation>Standard kodingsprofil som vil bli brukt hvis filen trenger å rekodes.</translation>
    </message>
    <message>
        <source>mplex Command</source>
        <translation>mplex-kommando</translation>
    </message>
    <message>
        <source>Command to run mplex</source>
        <translation>Kommando for å kjøre mplex</translation>
    </message>
    <message>
        <source>dvdauthor command</source>
        <translation>dvdauthor-kommando</translation>
    </message>
    <message>
        <source>Command to run dvdauthor.</source>
        <translation>Kommando fro å kjøre dvdauthor.</translation>
    </message>
    <message>
        <source>mkisofs command</source>
        <translation>mkisofs-kommando</translation>
    </message>
    <message>
        <source>Command to run mkisofs. (Used to create ISO images)</source>
        <translation>Kommando for å kjøre mkisofs. (Brukes til å lage ISO-bilder.)</translation>
    </message>
    <message>
        <source>growisofs command</source>
        <translation>growisofs-kommando</translation>
    </message>
    <message>
        <source>Command to run growisofs. (Used to burn DVDs)</source>
        <translation>Kommando for å kjøre growisofs. (Brukes for å brenne DVD&apos;er.)</translation>
    </message>
    <message>
        <source>M2VRequantiser command</source>
        <translation>Kommando for M2VRequantiser</translation>
    </message>
    <message>
        <source>Command to run M2VRequantiser. Optional - leave blank if you don&apos;t have M2VRequantiser installed.</source>
        <translation>Kommando for å kjøreM2VRequantiser. Valgfritt - bruk at tomt felt hvis M2VRequantiser ikke er installert.</translation>
    </message>
    <message>
        <source>jpeg2yuv command</source>
        <translation>jpeg2yuv-kommando</translation>
    </message>
    <message>
        <source>Command to run jpeg2yuv. Part of mjpegtools package</source>
        <translation>Kommando for åkjøre jpeg2yuv. Del av mjpegtools-pakken</translation>
    </message>
    <message>
        <source>spumux command</source>
        <translation>spumux-kommando</translation>
    </message>
    <message>
        <source>Command to run spumux. Part of dvdauthor package</source>
        <translation>Kommand for å kjøre spumux. Del av dvdauthor-pakken</translation>
    </message>
    <message>
        <source>mpeg2enc command</source>
        <translation>mpeg2enc-kommando</translation>
    </message>
    <message>
        <source>Command to run mpeg2enc. Part of mjpegtools package</source>
        <translation>Kommando for å kjøre mpeg2enc. Del av mjpegtools-pakken</translation>
    </message>
    <message>
        <source>projectx command</source>
        <translation>projectx-kommando</translation>
    </message>
    <message>
        <source>Command to run ProjectX. Will be used to cut commercials and split mpegs files instead of mythtranscode and mythreplex.</source>
        <translation>Kommando for ProjectX. Vil bli brukt å kutte reklamer og splitte mpeg filer istedenfor mythtranscode og mythreplex.</translation>
    </message>
    <message>
        <source>MythArchive Settings</source>
        <translation>Oppsett av Myth-arkiv</translation>
    </message>
    <message>
        <source>MythArchive Settings (2)</source>
        <translation>Oppsett av Myth-arkiv (2)</translation>
    </message>
    <message>
        <source>DVD Menu Settings</source>
        <translation>DVD menyinstillinger</translation>
    </message>
    <message>
        <source>MythArchive External Commands (1)</source>
        <translation>Eksterne kommandoer for Myth-arkiv (1)</translation>
    </message>
    <message>
        <source>MythArchive External Commands (2)</source>
        <translation>Eksterne kommandoer for Myth-arkiv (2)</translation>
    </message>
</context>
<context>
    <name>BurnMenu</name>
    <message>
        <source>Cannot burn a DVD.
The last run failed to create a DVD.</source>
        <translation>Kan ikke brenne en DVD.
Forrige kjøring kunne ikke lage en DVD.</translation>
    </message>
    <message>
        <source>Burn DVD</source>
        <translation>Brenn DVD</translation>
    </message>
    <message>
        <source>
Place a blank DVD in the drive and select an option below.</source>
        <translation>Sett i en tom DVD og velg et alternativ under.</translation>
    </message>
    <message>
        <source>Burn DVD Rewritable</source>
        <translation>Brenn DVD Rewritable</translation>
    </message>
    <message>
        <source>Burn DVD Rewritable (Force Erase)</source>
        <translation>Brenn DVD Rewritable (Tving Sletting)</translation>
    </message>
    <message>
        <source>It was not possible to run mytharchivehelper to burn the DVD.</source>
        <translation>Det var ikke mulig å kjøre mytharchivehelper for å brenne en DVD.</translation>
    </message>
</context>
<context>
    <name>BurnThemeUI</name>
    <message>
        <source>Has an intro and contains a main menu with 4 recordings per page. Does not have a chapter selection submenu.</source>
        <translation>Har en intro og inneholder en hovedmeny med 4 opptak per side. Har ikke kapittelmeny.</translation>
    </message>
    <message>
        <source>Has an intro and contains a summary main menu with 10 recordings per page. Does not have a chapter selection submenu, recording titles, dates or category.</source>
        <translation>Har en intro og inneholder en oppsummerende hovedmeny med 10 opptak per side. Har ikke kapittelmeny, tittel for opptak, dato eller kategori.</translation>
    </message>
    <message>
        <source>Has an intro and contains a main menu with 6 recordings per page. Does not have a scene selection submenu.</source>
        <translation>Har en intro og inneholder en hovedmeny med 6 opptak per side. Har ikke undemeny for valg av scene.</translation>
    </message>
    <message>
        <source>Has an intro and contains a main menu with 3 recordings per page and a scene selection submenu with 8 chapters points. Shows a program details page before each recording.</source>
        <translation>Har en intro og inneholder en hovedmeny med 3 opptak per side og en undermeny for scener med 8 kapittelpunkt. Viser en side med programdetaljer før hvert opptak.</translation>
    </message>
    <message>
        <source>Has an intro and contains a main menu with 3 recordings per page and a scene selection submenu with 8 chapters points. Shows a program details page before each recording. Uses animated thumb images.</source>
        <translation>Har en intro og inneholder en hovedmeny med 3 opptak per side og en undermeny for scener med 8 kapittelpunkt. Viser en side med programdetaljer før hvert opptak. Bruker animerte miniatyrbilder.</translation>
    </message>
    <message>
        <source>Has an intro and contains a main menu with 3 recordings per page and a scene selection submenu with 8 chapters points.</source>
        <translation>Har en intro og inneholder en hovedmeny med 3 opptak per side og en undermeny for scener med 8 kapittelpunkt.</translation>
    </message>
    <message>
        <source>Has an intro and contains a main menu with 3 recordings per page and a scene selection submenu with 8 chapters points. All the thumb images are animated.</source>
        <translation>Har en intro og inneholder en hovedmeny med 3 opptak per side og en undermeny for scener med 8 kapittelpunkt. Alle miniatyrbilder er animert.</translation>
    </message>
    <message>
        <source>Creates an auto play DVD with no menus. Shows an intro movie then for each title shows a details page followed by the video in sequence.</source>
        <translation>Lager en DVD uten menyer med automatisk avspilling. Viser en introduksjonsvideo for så å vise en detaljside for hver tittel fulgt av selve videoen.</translation>
    </message>
    <message>
        <source>Creates an auto play DVD with no menus and no intro.</source>
        <translation>Lager en DVD uten menyer og intro med automatisk avspilling.</translation>
    </message>
</context>
<context>
    <name>DVDThemeSelector</name>
    <message>
        <source>No theme description file found!</source>
        <translation>Ingen beskrivelsesfil for tema funnet!</translation>
    </message>
    <message>
        <source>Empty theme description!</source>
        <translation>Tom temabeskrivelse!</translation>
    </message>
    <message>
        <source>Unable to open theme description file!</source>
        <translation>Kunne ikke åpne beskrivelsesfil for tema!</translation>
    </message>
</context>
<context>
    <name>ExportNative</name>
    <message>
        <source>You need to add at least one item to archive!</source>
        <translation>Du må legge til minst ett element for å arkivere!</translation>
    </message>
    <message>
        <source>Remove Item</source>
        <translation>Fjern element</translation>
    </message>
    <message>
        <source>It was not possible to create the DVD. An error occured when running the scripts</source>
        <translation>Klarte ikke lage DVD&apos;en; en feil oppstod under kjøringen av skriptene</translation>
    </message>
    <message>
        <source>You don&apos;t have any videos!</source>
        <translation>Du har ingen videoer!</translation>
    </message>
    <message>
        <source>Menu</source>
        <translation>Meny</translation>
    </message>
</context>
<context>
    <name>FileSelector</name>
    <message>
        <source>The selected item is not a directory!</source>
        <translation>Det valgte element er ikke en katalog!</translation>
    </message>
    <message>
        <source>Find File</source>
        <translation>Finn fil</translation>
    </message>
    <message>
        <source>Find Directory</source>
        <translation>Finn mappe</translation>
    </message>
    <message>
        <source>Find Files</source>
        <translation>Finn filer</translation>
    </message>
</context>
<context>
    <name>ImportNative</name>
    <message>
        <source>You need to select a valid channel id!</source>
        <translation>Du må velge en gydlig kanal-id!</translation>
    </message>
    <message>
        <source>It was not possible to import the Archive.  An error occured when running &apos;mytharchivehelper&apos;</source>
        <translation>Klarte ikke importere arkivet; en feil oppsetod ved kjøring av «mytharchivehelper»</translation>
    </message>
    <message>
        <source>Select a channel id</source>
        <translation>Velg en kanalid</translation>
    </message>
    <message>
        <source>Select a channel number</source>
        <translation>Velg et kanalnummer</translation>
    </message>
    <message>
        <source>Select a channel name</source>
        <translation>Velg et kanalnavn</translation>
    </message>
    <message>
        <source>Select a Callsign</source>
        <translation>Velg et kanaltegn</translation>
    </message>
</context>
<context>
    <name>LogViewer</name>
    <message>
        <source>Show Progress Log</source>
        <translation>Vis fremdriftslogg</translation>
    </message>
    <message>
        <source>Show Full Log</source>
        <translation>Vis hele loggen</translation>
    </message>
    <message>
        <source>Don&apos;t Auto Update</source>
        <translation>Ikke oppdater automatisk</translation>
    </message>
    <message>
        <source>Cannot find any logs to show!</source>
        <translation>Kan ikke finne noen logger å vise!</translation>
    </message>
    <message>
        <source>Background creation has been asked to stop.
This may take a few minutes.</source>
        <translation>Bakgrunnslagingen har blitt bedt om å stoppe. 
Dette kan ta et par minutter.</translation>
    </message>
    <message>
        <source>Auto Update</source>
        <translation>Oppdater automatisk</translation>
    </message>
    <message>
        <source>Menu</source>
        <translation>Meny</translation>
    </message>
</context>
<context>
    <name>MythBurn</name>
    <message>
        <source>No Cut List</source>
        <translation>Ingen kuttliste</translation>
    </message>
    <message>
        <source>You need to add at least one item to archive!</source>
        <translation>Du må legge til minst ett element for å arkivere!</translation>
    </message>
    <message>
        <source>Retrieving File Information. Please Wait...</source>
        <translation>Henter filinformasjon. Vennligst vent...</translation>
    </message>
    <message>
        <source>Encoder: </source>
        <translation>Koder: </translation>
    </message>
    <message>
        <source>Using Cut List</source>
        <translation>Bruker kuttliste</translation>
    </message>
    <message>
        <source>Not Using Cut List</source>
        <translation>Bruker ikke kuttliste</translation>
    </message>
    <message>
        <source>Don&apos;t Use Cut List</source>
        <translation>Ikke bruk kuttliste</translation>
    </message>
    <message>
        <source>Use Cut List</source>
        <translation>Bruk kuttliste</translation>
    </message>
    <message>
        <source>Remove Item</source>
        <translation>Fjern element</translation>
    </message>
    <message>
        <source>Edit Details</source>
        <translation>Rediger detaljer</translation>
    </message>
    <message>
        <source>Change Encoding Profile</source>
        <translation>Endre kodingsprofil</translation>
    </message>
    <message>
        <source>Edit Thumbnails</source>
        <translation>Rediger miniatyrbilder</translation>
    </message>
    <message>
        <source>It was not possible to create the DVD.  An error occured when running the scripts</source>
        <translation>Klarte ikke lage DVD&apos;en; en feil oppstod under kjøringen av skriptene</translation>
    </message>
    <message>
        <source>You don&apos;t have any videos!</source>
        <translation>Du har ingen videoer!</translation>
    </message>
    <message>
        <source>Menu</source>
        <translation>meny</translation>
    </message>
</context>
<context>
    <name>MythControls</name>
    <message>
        <source>Toggle use cut list state for selected program</source>
        <translation>Slå av/på bruk av kuttliste for valgt program</translation>
    </message>
    <message>
        <source>Create DVD</source>
        <translation>Lag DVD</translation>
    </message>
    <message>
        <source>Create Archive</source>
        <translation>Lag arkiv</translation>
    </message>
    <message>
        <source>Import Archive</source>
        <translation>Importer arkiv</translation>
    </message>
    <message>
        <source>View Archive Log</source>
        <translation>Vis arkivlogg</translation>
    </message>
    <message>
        <source>Play Created DVD</source>
        <translation>Spill DVD som er laget</translation>
    </message>
    <message>
        <source>Burn DVD</source>
        <translation>Brenn DVD</translation>
    </message>
</context>
<context>
    <name>RecordingSelector</name>
    <message>
        <source>Retrieving Recording List.
Please Wait...</source>
        <translation>Henter opptaksliste.
Vennligst vent...</translation>
    </message>
    <message>
        <source>Clear All</source>
        <translation>Avmerk alle</translation>
    </message>
    <message>
        <source>Select All</source>
        <translation>Velg alle</translation>
    </message>
    <message>
        <source>All Recordings</source>
        <translation>Alle opptak</translation>
    </message>
    <message>
        <source>Either you don&apos;t have any recordings or no recordings are available locally!</source>
        <translation>Enter har du ingen opptak eller så er ikke opptakene tilgjengelig lokalt!</translation>
    </message>
    <message>
        <source>Menu</source>
        <translation>Meny</translation>
    </message>
</context>
<context>
    <name>SelectDestination</name>
    <message>
        <source>Single Layer DVD</source>
        <translation>Enkeltlags DVD</translation>
    </message>
    <message>
        <source>Dual Layer DVD</source>
        <translation>Dobbeltlags DVD</translation>
    </message>
    <message>
        <source>Single Layer DVD (4,482 MB)</source>
        <translation>Enkeltlags DVD (4,482 MB)</translation>
    </message>
    <message>
        <source>Dual Layer DVD (8,964 MB)</source>
        <translation>Dobbeltlags DVD (8,964 MB)</translation>
    </message>
    <message>
        <source>DVD +/- RW</source>
        <translation>DVD +/- RW</translation>
    </message>
    <message>
        <source>Rewritable DVD</source>
        <translation>Rewritable DVD</translation>
    </message>
    <message>
        <source>File</source>
        <translation>Fil</translation>
    </message>
    <message>
        <source>Any file accessable from your filesystem.</source>
        <translation>Enhver fil tilgjengelig i ditt filsystem.</translation>
    </message>
    <message>
        <source>Unknown</source>
        <translation>Ukjent</translation>
    </message>
</context>
<context>
    <name>ThemeUI</name>
    <message>
        <source>Select Destination</source>
        <translation>Velg destinasjon</translation>
    </message>
    <message>
        <source>description goes here.</source>
        <translation>beskrivelse her.</translation>
    </message>
    <message>
        <source>Free Space:</source>
        <translation>Ledig plass:</translation>
    </message>
    <message>
        <source>Make ISO Image</source>
        <translation>Lag ISO image</translation>
    </message>
    <message>
        <source>Burn to DVD</source>
        <translation>Brenn til DVD</translation>
    </message>
    <message>
        <source>Force Overwrite of DVD-RW Media</source>
        <translation>Tving overskriving av DVD RW medie</translation>
    </message>
    <message>
        <source>Select Recordings</source>
        <translation>Velg opptak</translation>
    </message>
    <message>
        <source>Show Recordings</source>
        <translation>Vis opptak</translation>
    </message>
    <message>
        <source>File Finder</source>
        <translation>Filfinner</translation>
    </message>
    <message>
        <source>Select Videos</source>
        <translation>Velg videoer</translation>
    </message>
    <message>
        <source>Video Category</source>
        <translation>Videokategori</translation>
    </message>
    <message>
        <source>title goes here</source>
        <translation>tittel her</translation>
    </message>
    <message>
        <source>x.xx Gb</source>
        <translation>x.xx Gb</translation>
    </message>
    <message>
        <source>PL:</source>
        <translation>PL:</translation>
    </message>
    <message>
        <source>1</source>
        <translation>1</translation>
    </message>
    <message>
        <source>No videos available</source>
        <translation>Ingen videoer tilgjengelig</translation>
    </message>
    <message>
        <source>Log Viewer</source>
        <translation>Loggviser</translation>
    </message>
    <message>
        <source>Change Encoding Profile</source>
        <translation>Endre kodingsprofil</translation>
    </message>
    <message>
        <source>DVD Menu Theme</source>
        <translation>DVD menytema</translation>
    </message>
    <message>
        <source>Select a theme</source>
        <translation>Velg et tema</translation>
    </message>
    <message>
        <source>Intro</source>
        <translation>Intro</translation>
    </message>
    <message>
        <source>Main Menu</source>
        <translation>Hovedmeny</translation>
    </message>
    <message>
        <source>Chapter Menu</source>
        <translation>Kapittelmeny</translation>
    </message>
    <message>
        <source>Details</source>
        <translation>Detaljer</translation>
    </message>
    <message>
        <source>Select Archive Items</source>
        <translation>Velg arkivelementer</translation>
    </message>
    <message>
        <source>No files are selected for archive</source>
        <translation>Ingen filer er valgt for arkivet</translation>
    </message>
    <message>
        <source>Archive Item Details</source>
        <translation>Detaljer for arkivelement</translation>
    </message>
    <message>
        <source>Title:</source>
        <translation>Tittel:</translation>
    </message>
    <message>
        <source>Subtitle:</source>
        <translation>Undertekst:</translation>
    </message>
    <message>
        <source>Start Date:</source>
        <translation>Startdato:</translation>
    </message>
    <message>
        <source>Time:</source>
        <translation>Tid:</translation>
    </message>
    <message>
        <source>Description:</source>
        <translation>Beskrivelse:</translation>
    </message>
    <message>
        <source>Thumb Image Selector</source>
        <translation>velger for miniatyrbilde</translation>
    </message>
    <message>
        <source>Current Position</source>
        <translation>Gjeldende posisjon</translation>
    </message>
    <message>
        <source>0:00:00.00</source>
        <translation>0:00:00.00</translation>
    </message>
    <message>
        <source>Write video to data dvd or file</source>
        <translation>Skriv video til data-dvd eller fil</translation>
    </message>
    <message>
        <source>File</source>
        <translation>Fil</translation>
    </message>
    <message>
        <source>Find Location</source>
        <translation>Finn lokasjon</translation>
    </message>
    <message>
        <source>Navigation</source>
        <translation>Navigasjon</translation>
    </message>
    <message>
        <source>Filesize</source>
        <translation>Filstørrelse</translation>
    </message>
    <message>
        <source>Date (time)</source>
        <translation>Dato (tid)</translation>
    </message>
    <message>
        <source>Show Videos</source>
        <translation>Vis videoer</translation>
    </message>
    <message>
        <source>Parental Level</source>
        <translation>Foreldrenivå</translation>
    </message>
    <message>
        <source>Archive Item:</source>
        <translation>Arkiver element:</translation>
    </message>
    <message>
        <source>Encoder Profile:</source>
        <translation>Kodingsprofil:</translation>
    </message>
    <message>
        <source>Old size:</source>
        <translation>Gjeldende størrelse:</translation>
    </message>
    <message>
        <source>New Size:</source>
        <translation>Ny størrelse:</translation>
    </message>
    <message>
        <source>DVD</source>
        <translation>DVD</translation>
    </message>
    <message>
        <source>Create a</source>
        <translation>Lag en</translation>
    </message>
    <message>
        <source>Videos</source>
        <translation>Videoer</translation>
    </message>
    <message>
        <source>Export your</source>
        <translation>Eksporter dine</translation>
    </message>
    <message>
        <source>Encode your video</source>
        <translation>Kod din video</translation>
    </message>
    <message>
        <source>Archive</source>
        <translation>Arkiv</translation>
    </message>
    <message>
        <source>Import an</source>
        <translation>Importer et</translation>
    </message>
    <message>
        <source>Utilities</source>
        <translation>Verktøy</translation>
    </message>
    <message>
        <source>Use archive</source>
        <translation>Bruk arkiv</translation>
    </message>
    <message>
        <source>Media</source>
        <translation>Media</translation>
    </message>
    <message>
        <source>Eject your</source>
        <translation>Løs ut din</translation>
    </message>
    <message>
        <source>Log</source>
        <translation>Logg</translation>
    </message>
    <message>
        <source>Show</source>
        <translation>Vis</translation>
    </message>
    <message>
        <source>Test created</source>
        <translation>Test laget</translation>
    </message>
    <message>
        <source>Burn</source>
        <translation>Brenn</translation>
    </message>
    <message>
        <source>Start to</source>
        <translation>Start med å</translation>
    </message>
    <message>
        <source>0 mb</source>
        <translation>0 mb</translation>
    </message>
    <message>
        <source>xxxxx mb</source>
        <translation>xxxxx mb</translation>
    </message>
    <message>
        <source>Current Size:</source>
        <translation>Gjeldende størrelse:</translation>
    </message>
    <message>
        <source>Not Applicable</source>
        <translation>Ikke relevant</translation>
    </message>
    <message>
        <source>Intro:</source>
        <translation>Intro:</translation>
    </message>
    <message>
        <source>Main Menu:</source>
        <translation>Hovedmeny:</translation>
    </message>
    <message>
        <source>Chapter Menu:</source>
        <translation>Kapittelmeny:</translation>
    </message>
    <message>
        <source>Details:</source>
        <translation>Detaljer:</translation>
    </message>
    <message>
        <source>%date% / %profile%</source>
        <translation>%date% / %profile%</translation>
    </message>
    <message>
        <source>Current size: %1</source>
        <translation>Gjeldende størrelse: %1</translation>
    </message>
    <message>
        <source>Current Position:</source>
        <translation>Gjeldende posisjon:</translation>
    </message>
    <message>
        <source>Seek Amount:</source>
        <translation>Søkemengde:</translation>
    </message>
    <message>
        <source>increase seek amount</source>
        <translation>Øk søkemengde</translation>
    </message>
    <message>
        <source>decrease seek amount</source>
        <translation>reduser søkemengde</translation>
    </message>
    <message>
        <source>move left</source>
        <translation>Flytt til venstre</translation>
    </message>
    <message>
        <source>move right</source>
        <translation>Flytt til høyre</translation>
    </message>
    <message>
        <source>Read video from data dvd or file</source>
        <translation>Les video fra data-dvd eller fil</translation>
    </message>
    <message>
        <source>Associated Channel</source>
        <translation>Assosiert kanal</translation>
    </message>
    <message>
        <source>Archive Chan ID:</source>
        <translation>Kanal ID for arkiv:</translation>
    </message>
    <message>
        <source>Archive Chan No:</source>
        <translation>Kanal nr for arkiv:</translation>
    </message>
    <message>
        <source>Archive Callsign:</source>
        <translation>Kallesignal for arkiv:</translation>
    </message>
    <message>
        <source>Archive Name:</source>
        <translation>Arkivnavn:</translation>
    </message>
    <message>
        <source>Local Chan ID:</source>
        <translation>Lokalt kanal-ID:</translation>
    </message>
    <message>
        <source>Local Chan No:</source>
        <translation>Lokalt kanal-nr:</translation>
    </message>
    <message>
        <source>Local Callsign:</source>
        <translation>Lokalt kallesignal:</translation>
    </message>
    <message>
        <source>Local Name:</source>
        <translation>Lokalt navn:</translation>
    </message>
    <message>
        <source>Search Local</source>
        <translation>Søk lokalt</translation>
    </message>
    <message>
        <source>Channel ID</source>
        <translation>Kanal ID</translation>
    </message>
    <message>
        <source>Channel No</source>
        <translation>Kanal nr</translation>
    </message>
    <message>
        <source>Callsign</source>
        <translation>Kallesignal</translation>
    </message>
    <message>
        <source>Name</source>
        <translation>Navn</translation>
    </message>
    <message>
        <source>Channel ID:</source>
        <translation>Kanal ID:</translation>
    </message>
    <message>
        <source>Channel Number:</source>
        <translation>Kanalnummer:</translation>
    </message>
    <message>
        <source>Seek Amount</source>
        <translation>Søkemengde</translation>
    </message>
    <message>
        <source>Frame</source>
        <translation>Ramme</translation>
    </message>
    <message>
        <source>Up Level</source>
        <translation>Opp et nivå</translation>
    </message>
    <message>
        <source>xxxxx MB</source>
        <translation>xxxxx MB</translation>
    </message>
    <message>
        <source>0 MB</source>
        <translation>0 MB</translation>
    </message>
    <message>
        <source>sep 13, 2004 11:00 pm (1h 15m)</source>
        <translation>13 sept, 2004 11:00 (1t 15min)</translation>
    </message>
    <message>
        <source>x.xx GB</source>
        <translation>x.xx GB</translation>
    </message>
    <message>
        <source>File Finder To Import</source>
        <translation>Filfinner for importering</translation>
    </message>
    <message>
        <source>Start Time:</source>
        <translation>Starttid:</translation>
    </message>
    <message>
        <source>Select Associated Channel</source>
        <translation>Velg assosiert kanal</translation>
    </message>
    <message>
        <source>Archived Channel</source>
        <translation>Arkivert kanal</translation>
    </message>
    <message>
        <source>Chan. ID:</source>
        <translation>Kanal ID:</translation>
    </message>
    <message>
        <source>Chan. No:</source>
        <translation>Kanal Nr:</translation>
    </message>
    <message>
        <source>Callsign:</source>
        <translation>Kallesignal:</translation>
    </message>
    <message>
        <source>Name:</source>
        <translation>Navn:</translation>
    </message>
    <message>
        <source>Local Channel</source>
        <translation>Lokal kanal</translation>
    </message>
    <message>
        <source>A high quality profile giving approx. 1 hour of video on a single layer DVD</source>
        <translation>En profil med høy kvalitet som gir omtrent 1 time med video på en enkeltlags DVD</translation>
    </message>
    <message>
        <source>A standard play profile giving approx. 2 hour of video on a single layer DVD</source>
        <translation>En profil med standard kvalitet som gir omtrent 2 timer med video på en enkeltlags DVD</translation>
    </message>
    <message>
        <source>A long play profile giving approx. 4 hour of video on a single layer DVD</source>
        <translation>En profil med lang avspillingstid som gir omtrent 4 timer med video på en enkeltlags DVD</translation>
    </message>
    <message>
        <source>A extended play profile giving approx. 6 hour of video on a single layer DVD</source>
        <translation>En profil med utvidet avspillingstid som gir omtrent 6 timer med video på en enkeltlags DVD</translation>
    </message>
    <message>
        <source>Filesize: %1</source>
        <translation>Filstørrelse: %1</translation>
    </message>
    <message>
        <source>Recorded Time: %1</source>
        <translation>Tid tatt opp: %1</translation>
    </message>
    <message>
        <source>%&quot;|SUBTITLE|&quot; %%(|ORIGINALAIRDATE|) %%(|STARS|) %%DESCRIPTION%</source>
        <translation>%&quot;|SUBTITLE|&quot; %%(|ORIGINALAIRDATE|) %%(|STARS|) %%DESCRIPTION%</translation>
    </message>
    <message>
        <source>Parental Level: %1</source>
        <translation>Foreldrenivå: %1</translation>
    </message>
    <message>
        <source>Archive Items</source>
        <translation>Arkiver elementer</translation>
    </message>
    <message>
        <source>Profile:</source>
        <translation>Profil:</translation>
    </message>
    <message>
        <source>Old Size:</source>
        <translation>Gjeldende størrelse:</translation>
    </message>
    <message>
        <source>Select Destination:</source>
        <translation>Velg destinasjon:</translation>
    </message>
    <message>
        <source>Parental level: %1</source>
        <translation>Foreldrenivå: %1</translation>
    </message>
    <message>
        <source>New size:</source>
        <translation>Ny størrelse:</translation>
    </message>
    <message>
        <source>Select a theme:</source>
        <translation>Velg et tema:</translation>
    </message>
    <message>
        <source>Menu</source>
        <translation>Meny</translation>
    </message>
    <message>
        <source>Chapter</source>
        <translation>Kapittel</translation>
    </message>
    <message>
        <source>Detail</source>
        <translation>Detalj</translation>
    </message>
    <message>
        <source>Select File to Import</source>
        <translation>Velg fil som skal importeres</translation>
    </message>
    <message>
        <source>Create DVD</source>
        <translation>Lag DVD</translation>
    </message>
    <message>
        <source>Create Archive</source>
        <translation>Lag arkiv</translation>
    </message>
    <message>
        <source>Encode Video File</source>
        <translation>Kod videofil</translation>
    </message>
    <message>
        <source>Import Archive</source>
        <translation>Importer arkiv</translation>
    </message>
    <message>
        <source>Archive Utilities</source>
        <translation>Arkiv-verktøy</translation>
    </message>
    <message>
        <source>Show Log Viewer</source>
        <translation>Vis loggviser</translation>
    </message>
    <message>
        <source>Play Created DVD</source>
        <translation>Spill DVD som er laget</translation>
    </message>
    <message>
        <source>Burn DVD</source>
        <translation>Brenn DVD</translation>
    </message>
    <message>
        <source>Choose where you would like your files archived.</source>
        <translation>Velg hvor du vil at filene dine skal bli arkivert.</translation>
    </message>
    <message>
        <source>%DATE%, %TIME%</source>
        <translation>%DATE%, %TIME%</translation>
    </message>
    <message>
        <source>Output Type:</source>
        <translation>Utdata type:</translation>
    </message>
    <message>
        <source>Destination:</source>
        <translation>Mål:</translation>
    </message>
    <message>
        <source>Click here to find an output location...</source>
        <translation>Klikk her for å finne et utdatasted...</translation>
    </message>
    <message>
        <source>Erase DVD-RW before burning</source>
        <translation>Slett DVD-RW før brenning</translation>
    </message>
    <message>
        <source>Cancel</source>
        <translation>Avbryt</translation>
    </message>
    <message>
        <source>Previous</source>
        <translation>Forrige</translation>
    </message>
    <message>
        <source>Next</source>
        <translation>Neste</translation>
    </message>
    <message>
        <source>Filter:</source>
        <translation>Filter:</translation>
    </message>
    <message>
        <source>OK</source>
        <translation>OK</translation>
    </message>
    <message>
        <source>Select the file you wish to use.</source>
        <translation>Velg hvilken fil du vil bruke.</translation>
    </message>
    <message>
        <source>Back</source>
        <translation>Tilbake</translation>
    </message>
    <message>
        <source>Home</source>
        <translation>Hjem</translation>
    </message>
    <message>
        <source>See logs from your archive runs.</source>
        <translation>Se logger fra arkiveringskjøringen.</translation>
    </message>
    <message>
        <source>Update</source>
        <translation>Oppdater</translation>
    </message>
    <message>
        <source>Exit</source>
        <translation>Avslutt</translation>
    </message>
    <message>
        <source>0.00 GB</source>
        <translation>0.00 GB</translation>
    </message>
    <message>
        <source>Video Category:</source>
        <translation>Videokategori:</translation>
    </message>
    <message>
        <source>Ok</source>
        <translation>OK</translation>
    </message>
    <message>
        <source>12.34 GB</source>
        <translation>12.34 GB</translation>
    </message>
    <message>
        <source>Choose the appearance of your DVD.</source>
        <translation>Velg utseende for DVDen din.</translation>
    </message>
    <message>
        <source>Theme:</source>
        <translation>Tema:</translation>
    </message>
    <message>
        <source>Select the recordings and videos you wish to save.</source>
        <translation>Velg hvilke opptak og videoer du vil lagre.</translation>
    </message>
    <message>
        <source>Add Recording</source>
        <translation>Legg til opptak</translation>
    </message>
    <message>
        <source>Add Video</source>
        <translation>Legg til video</translation>
    </message>
    <message>
        <source>Add File</source>
        <translation>Legg til fil</translation>
    </message>
    <message>
        <source>Save</source>
        <translation>Lagre</translation>
    </message>
    <message>
        <source>Find</source>
        <translation>Finn</translation>
    </message>
    <message>
        <source>Prev</source>
        <translation>Forrige</translation>
    </message>
    <message>
        <source>Search Channel</source>
        <translation>Søk i kanal</translation>
    </message>
    <message>
        <source>Search Callsign</source>
        <translation>Søk i kallesignal</translation>
    </message>
    <message>
        <source>Search Name</source>
        <translation>Søk i navn</translation>
    </message>
    <message>
        <source>Finish</source>
        <translation>Fullfør</translation>
    </message>
    <message>
        <source>Add video</source>
        <translation>Legg til video</translation>
    </message>
</context>
<context>
    <name>ThumbFinder</name>
    <message>
        <source>Exit, Save Thumbnails</source>
        <translation>Avslutt, og lagre miniatyrbildene</translation>
    </message>
    <message>
        <source>Exit, Don&apos;t Save Thumbnails</source>
        <translation>Avslutt, Ikke lagre miniatyrbildene</translation>
    </message>
    <message>
        <source>Menu</source>
        <translation>Meny</translation>
    </message>
</context>
<context>
    <name>VideoSelector</name>
    <message>
        <source>Clear All</source>
        <translation>Velg ingen</translation>
    </message>
    <message>
        <source>Select All</source>
        <translation>Velg alle</translation>
    </message>
    <message>
        <source>All Videos</source>
        <translation>Alle videoer</translation>
    </message>
    <message>
        <source>You need to enter a valid password for this parental level</source>
        <translation>Du må angi et gyldig passord for dette foreldrenivået</translation>
    </message>
    <message>
        <source>Menu</source>
        <translation>Meny</translation>
    </message>
</context>
</TS>
