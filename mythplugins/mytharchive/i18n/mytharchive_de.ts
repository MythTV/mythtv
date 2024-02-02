<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE TS>
<TS version="2.1" language="de_DE" sourcelanguage="en_US">
<context>
    <name>(ArchiveUtils)</name>
    <message>
        <location filename="../mytharchive/archiveutil.cpp" line="52"/>
        <source>Cannot find the MythArchive work directory.
Have you set the correct path in the settings?</source>
        <translation>Konnte das MythArchive-Arbeitsverzeichnis nicht finden.
Stimmt der Pfad in den Einstellungen?</translation>
    </message>
</context>
<context>
    <name>(MythArchiveMain)</name>
    <message>
        <location filename="../mytharchive/mytharchive.cpp" line="100"/>
        <source>Found a lock file but the owning process isn&apos;t running!
Removing stale lock file.</source>
        <translation>Eine Lock-Datei existiert aber der dazugehörige Prozess läuft nicht!
Lösche Lock-Datei.</translation>
    </message>
    <message>
        <location filename="../mytharchive/mytharchive.cpp" line="211"/>
        <source>Last run did not create a playable DVD.</source>
        <translation>Der letzte Durchgang hat keine spielbare DVD erzeugt.</translation>
    </message>
    <message>
        <location filename="../mytharchive/mytharchive.cpp" line="218"/>
        <source>Last run failed to create a DVD.</source>
        <translation>Der letzte Durchgang hat keine DVD erzeugt.</translation>
    </message>
</context>
<context>
    <name>ArchiveFileSelector</name>
    <message>
        <location filename="../mytharchive/importnative.cpp" line="205"/>
        <source>Find File To Import</source>
        <translation>Datei für den Import finden</translation>
    </message>
    <message>
        <source>Next</source>
        <translation type="obsolete">Weiter</translation>
    </message>
    <message>
        <source>Cancel</source>
        <translation type="obsolete">Abbrechen</translation>
    </message>
    <message>
        <source>Previous</source>
        <translation type="obsolete">Zurück</translation>
    </message>
    <message>
        <source>Back</source>
        <translation type="obsolete">Zurück</translation>
    </message>
    <message>
        <source>Home</source>
        <translation type="obsolete">Anfang</translation>
    </message>
    <message>
        <location filename="../mytharchive/importnative.cpp" line="265"/>
        <source>The selected item is not a valid archive file!</source>
        <translation>Der gewählte Eintrag ist keine gültige Archivdatei!</translation>
    </message>
</context>
<context>
    <name>ArchiveSettings</name>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="20"/>
        <source>MythArchive Temp Directory</source>
        <translation>MythArchive Temp Verzeichnis</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="23"/>
        <source>Location where MythArchive should create its temporary work files. LOTS of free space required here.</source>
        <translation>Pfad in welchem MythArchive seine temporären Arbeitsdateien erstellt. Hier wird VIEL Platz benötigt.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="35"/>
        <source>MythArchive Share Directory</source>
        <translation>MythArchive gemeinsames Verzeichnis</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="38"/>
        <source>Location where MythArchive stores its scripts, intro movies and theme files</source>
        <translation>Pfad in welchem MythArchive seine Skripte, Intros und Themendateien speichert</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="50"/>
        <source>Video format</source>
        <translation>Videoformat</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="55"/>
        <source>Video format for DVD recordings, PAL or NTSC.</source>
        <translation>Videoformat für DVD-Aufnahmen, PAL oder NTSC.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="64"/>
        <source>File Selector Filter</source>
        <translation>Dateiauswahlfilter</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="67"/>
        <source>The file name filter to use in the file selector.</source>
        <translation>Der Filter der im Dateiauswahlmenü benutzt wird.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="76"/>
        <source>Location of DVD</source>
        <translation>DVD-Gerät</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="79"/>
        <source>Which DVD drive to use when burning discs.</source>
        <translation>Welches DVD-Laufwerk zum Brennen benutzt werden soll.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="89"/>
        <source>DVD Drive Write Speed</source>
        <translation>DVD-Schreibgeschwindigkeit</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="92"/>
        <source>This is the write speed to use when burning a DVD. Set to 0 to allow growisofs to choose the fastest available speed.</source>
        <translation>Die Schreibgeschwindigkeit beim Brennen von DVDs. Bei 0 wird &quot;growisofs&quot; automatisch die höchste Geschwindigkeit wählen.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="103"/>
        <source>Command to play DVD</source>
        <translation>DVD-Abspielbefehl</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="106"/>
        <source>Command to run when test playing a created DVD. &apos;Internal&apos; will use the internal MythTV player. %f will be replaced with the path to the created DVD structure eg. &apos;xine -pfhq --no-splash dvd:/%f&apos;.</source>
        <translation>Befehl zum testweisen Abspielen einer neuen DVD. Bei &quot;Internal&quot; wird der MythTV-Player verwendet. &quot;%f&quot; wird durch den Pfad der erzeugten DVD-Struktur ersetzt, z.B. &quot;xine -pfhq --no-splash dvd:/%f&quot;.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="119"/>
        <source>Copy remote files</source>
        <translation>Kopiere entfernte Dateien</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="122"/>
        <source>If set files on remote filesystems will be copied over to the local filesystem before processing. Speeds processing and reduces bandwidth on the network</source>
        <translation>Falls gesetzt, werden Dateien auf entfernten Dateisystemen vor der Verarbeitung auf das lokale Dateisystem kopiert. Dies beschleunigt die Verarbeitung und reduziert die Auslastung des Netzwerkes</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="134"/>
        <source>Always Use Mythtranscode</source>
        <translation>Immer Mythtranscode benutzen</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="137"/>
        <source>If set mpeg2 files will always be passed though mythtranscode to clean up any errors. May help to fix some audio problems. Ignored if &apos;Use ProjectX&apos; is set.</source>
        <translation>Falls gesetzt, werden MPEG2-Dateien immer durch mythtranscode gefiltert um etwaige Fehler zu beheben. Dies könnte manche Tonprobleme beheben. Diese Option wird ignoriert falls &quot;Nutze ProjectX&quot; aktiv ist.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="149"/>
        <source>Use ProjectX</source>
        <translation>Nutze ProjectX</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="152"/>
        <source>If set ProjectX will be used to cut commercials and split mpeg2 files instead of mythtranscode and mythreplex.</source>
        <translation>Falls gesetzt, wird ProjectX statt mythtranscode und mythreplex verwendet um Werbung heraus zu schneiden und MPEG2-Dateien aufzuteilen.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="163"/>
        <source>Use FIFOs</source>
        <translation>FIFOs benutzen</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="166"/>
        <source>The script will use FIFOs to pass the output of mplex into dvdauthor rather than creating intermediate files. Saves time and disk space during multiplex operations but not supported on Windows platform</source>
        <translation>Das Skript wird FIFOs an Stelle von temporären Dateien verwenden um die Ausgabe von mplex an dvdauthor weiter zu leiten. Dies spart Zeit und Plattenplatz während Multiplex-Vorgängen, wird unter Windows aber nicht unterstützt</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="179"/>
        <source>Add Subtitles</source>
        <translation>Untertitel hinzufügen</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="182"/>
        <source>If available this option will add subtitles to the final DVD. Requires &apos;Use ProjectX&apos; to be on.</source>
        <translation>Falls verfügbar, werden der endgültigen DVD Untertitel hinzugefügt. Dazu muss &quot;Nutze ProjectX&quot; aktiviert sein.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="192"/>
        <source>Main Menu Aspect Ratio</source>
        <translation>Hauptmenü Seitenverhältnis</translation>
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
        <translation>Welches Seitenverhältnis beim Erstellen des Hauptmenüs benutzt werden soll.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="208"/>
        <source>Chapter Menu Aspect Ratio</source>
        <translation>Kapitelmenü Seitenverhältnis</translation>
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
        <translation>Welches Seitenverhältnis beim Erstellen des Kapitelmenüs benutzt werden soll. Bei &quot;%1&quot; wird das Seitenverhältnis des entsprechenden Videos verwendet.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="228"/>
        <source>Date format</source>
        <translation>Datumsformat</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="231"/>
        <source>Samples are shown using today&apos;s date.</source>
        <translation>Die Beispiele gelten für das heutige Datum.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="237"/>
        <source>Samples are shown using tomorrow&apos;s date.</source>
        <translation>Die Beispiele gelten für Morgen.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="255"/>
        <source>Your preferred date format to use on DVD menus. %1</source>
        <extracomment>%1 gives additional info on the date used</extracomment>
        <translation>Ihr bevorzugtes Zeitformat für DVD-Menüs. %1</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="264"/>
        <source>Time format</source>
        <translation>Zeitformat</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="271"/>
        <source>Your preferred time format to display on DVD menus. You must choose a format with &quot;AM&quot; or &quot;PM&quot; in it, otherwise your time display will be 24-hour or &quot;military&quot; time.</source>
        <translation>Ihr bevorzugtes Zeitformat für DVD-Menüs. Einträge mit &quot;am&quot; bzw &quot;pm&quot; zeigen die Zeit mit 12 Stunden Teilung an.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="282"/>
        <source>Default Encoder Profile</source>
        <translation>Standard Enkoderprofil</translation>
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
        <translation>E</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="291"/>
        <source>Default encoding profile to use if a file needs re-encoding.</source>
        <translation>Das Standard Enkoderprofil für Dateien neu enkodiert werden müssen.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="300"/>
        <source>mplex Command</source>
        <translation>Befehl für mplex</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="304"/>
        <source>Command to run mplex</source>
        <translation>Der Befehl zum Ausführen von mplex</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="313"/>
        <source>dvdauthor command</source>
        <translation>Befehl für dvdauthor</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="317"/>
        <source>Command to run dvdauthor.</source>
        <translation>Der Befehl zum Ausführen von dvdauthor.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="326"/>
        <source>mkisofs command</source>
        <translation>Befehl für mkisofs</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="330"/>
        <source>Command to run mkisofs. (Used to create ISO images)</source>
        <translation>Der Befehl zum Ausführen von mkisofs (wird benutzt um ISO-Abbilder zu erstellen)</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="339"/>
        <source>growisofs command</source>
        <translation>Befehl für growisofs</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="343"/>
        <source>Command to run growisofs. (Used to burn DVDs)</source>
        <translation>Der Befehl zum Ausführen von growisofs (wird benutzt um DVDs zu brennen)</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="352"/>
        <source>M2VRequantiser command</source>
        <translation>M2V-Grössenwandler-Kommando</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="356"/>
        <source>Command to run M2VRequantiser. Optional - leave blank if you don&apos;t have M2VRequantiser installed.</source>
        <translation>Kommando zum Ausführen des M2V-Grössenwandler. Optional - Leer lassen wenn der M2V-Grössenwandler nicht installiert ist.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="366"/>
        <source>jpeg2yuv command</source>
        <translation>Befehl für jpeg2yuv</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="370"/>
        <source>Command to run jpeg2yuv. Part of mjpegtools package</source>
        <translation>Befehl um jpeg2yuv zu starten. Dies ist Teil des mjpegtools Pakets</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="379"/>
        <source>spumux command</source>
        <translation>Befehl für spumux</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="383"/>
        <source>Command to run spumux. Part of dvdauthor package</source>
        <translation>Der Befehl zum Ausführen von spumux. Teil des dvdauthor Paketes</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="392"/>
        <source>mpeg2enc command</source>
        <translation>Befehl für mpeg2enc</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="396"/>
        <source>Command to run mpeg2enc. Part of mjpegtools package</source>
        <translation>Der Befehl zum Ausführen von mpeg2enc. Teil des mjpegtool Paketes</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="405"/>
        <source>projectx command</source>
        <translation>Befehl für projectx</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="409"/>
        <source>Command to run ProjectX. Will be used to cut commercials and split mpegs files instead of mythtranscode and mythreplex.</source>
        <translation>Befehl um ProjectX zu starten. Wird statt mythtranscode und mythreplex verwendet um Werbung heraus zu schneiden und MPEG-Dateien aufzuteilen.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="418"/>
        <source>MythArchive Settings</source>
        <translation>MythArchive Einstellungen</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="442"/>
        <source>MythArchive External Commands</source>
        <translation>MythArchive Externe Befehle</translation>
    </message>
    <message>
        <source>MythArchive Settings (2)</source>
        <translation type="vanished">MythArchive  Einstellungen (2)</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="434"/>
        <source>DVD Menu Settings</source>
        <translation>DVD-Menü Einstellungen</translation>
    </message>
    <message>
        <source>MythArchive External Commands (1)</source>
        <translation type="vanished">MythArchive externe Kommandos (1)</translation>
    </message>
    <message>
        <source>MythArchive External Commands (2)</source>
        <translation type="vanished">MythArchive externe Kommandos (2)</translation>
    </message>
</context>
<context>
    <name>BurnMenu</name>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="1091"/>
        <source>Cannot burn a DVD.
The last run failed to create a DVD.</source>
        <translation>Kann keine DVD brennen.
Der letzte Durchgang hat keine DVD erzeugt.</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="1097"/>
        <location filename="../mytharchive/mythburn.cpp" line="1109"/>
        <source>Burn DVD</source>
        <translation>DVD brennen</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="1098"/>
        <source>
Place a blank DVD in the drive and select an option below.</source>
        <translation>
Legen Sie eine leere DVD ins Laufwerk und wählen Sie eine Option.</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="1110"/>
        <source>Burn DVD Rewritable</source>
        <translation>DVD-RW brennen</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="1111"/>
        <source>Burn DVD Rewritable (Force Erase)</source>
        <translation>DVD-RW brennen (löschen forcieren)</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="1165"/>
        <source>It was not possible to run mytharchivehelper to burn the DVD.</source>
        <translation>Konnte mytharchivehelper nicht starten um die DVD zu brennen.</translation>
    </message>
</context>
<context>
    <name>BurnThemeUI</name>
    <message>
        <location filename="../mythburn/themes/burnthemestrings.h" line="5"/>
        <source>Has an intro and contains a main menu with 4 recordings per page. Does not have a chapter selection submenu.</source>
        <translation>Hat ein Intro und beinhaltet ein Hauptmenü mit 4 Aufnahmen pro Seite. Hat kein Kapitelmenü.</translation>
    </message>
    <message>
        <location filename="../mythburn/themes/burnthemestrings.h" line="6"/>
        <source>Has an intro and contains a summary main menu with 10 recordings per page. Does not have a chapter selection submenu, recording titles, dates or category.</source>
        <translation>Hat ein Intro und beinhaltet ein Hauptmenü mit 10 Aufnahmen pro Seite. Hat kein Kapitelmenü, Aufnahmetitel, Datum oder Kategorie.</translation>
    </message>
    <message>
        <location filename="../mythburn/themes/burnthemestrings.h" line="7"/>
        <source>Has an intro and contains a main menu with 6 recordings per page. Does not have a scene selection submenu.</source>
        <translation>Hat ein Intro und beinhaltet ein Hauptmenü mit 6 Aufnahmen pro Seite. Hat kein Szeneauswahlmenü.</translation>
    </message>
    <message>
        <location filename="../mythburn/themes/burnthemestrings.h" line="8"/>
        <source>Has an intro and contains a main menu with 3 recordings per page and a scene selection submenu with 8 chapters points. Shows a program details page before each recording.</source>
        <translation>Hat ein Intro und beinhaltet ein Hauptmenü mit 3 Aufnahmen pro Seite sowie ein Szeneauswahlmenü mit 8 Kapitelpunkten. Zeigt Programmdetails zu jeder Aufnahme.</translation>
    </message>
    <message>
        <location filename="../mythburn/themes/burnthemestrings.h" line="9"/>
        <source>Has an intro and contains a main menu with 3 recordings per page and a scene selection submenu with 8 chapters points. Shows a program details page before each recording. Uses animated thumb images.</source>
        <translation>Hat ein Intro und beinhaltet ein Hauptmenü mit 3 Aufnahmen pro Seite sowie ein Szeneauswahlmenü mit 8 Kapitelpunkten. Zeigt Programmdetails zu jeder Aufnahme. Benutzt animierte Vorschaubilder.</translation>
    </message>
    <message>
        <location filename="../mythburn/themes/burnthemestrings.h" line="10"/>
        <source>Has an intro and contains a main menu with 3 recordings per page and a scene selection submenu with 8 chapters points.</source>
        <translation>Hat ein Intro und beinhaltet ein Hauptmenü mit 3 Aufnahmen pro Seite sowie ein Szeneauswahlmenü mit 8 Kapitelpunkten.</translation>
    </message>
    <message>
        <location filename="../mythburn/themes/burnthemestrings.h" line="11"/>
        <source>Has an intro and contains a main menu with 3 recordings per page and a scene selection submenu with 8 chapters points. All the thumb images are animated.</source>
        <translation>Hat ein Intro und beinhaltet ein Hauptmenü mit 3 Aufnahmen pro Seite sowie ein Szeneauswahlmenü mit 8 Kapitelpunkten. Benutzt animierte Vorschaubilder.</translation>
    </message>
    <message>
        <location filename="../mythburn/themes/burnthemestrings.h" line="12"/>
        <source>Creates an auto play DVD with no menus. Shows an intro movie then for each title shows a details page followed by the video in sequence.</source>
        <translation>Erstelle eine automatisch abspielende DVD ohne Menüs. Zeigt ein Intro und für jeden Titel eine Detailseite.</translation>
    </message>
    <message>
        <location filename="../mythburn/themes/burnthemestrings.h" line="13"/>
        <source>Creates an auto play DVD with no menus and no intro.</source>
        <translation>Erstelle eine automatisch abspielende DVD ohne Menüs und ohne Intro.</translation>
    </message>
</context>
<context>
    <name>DVDThemeSelector</name>
    <message>
        <location filename="../mytharchive/themeselector.cpp" line="201"/>
        <location filename="../mytharchive/themeselector.cpp" line="212"/>
        <source>No theme description file found!</source>
        <translation>Keine Themebeschreibung gefunden!</translation>
    </message>
    <message>
        <location filename="../mytharchive/themeselector.cpp" line="225"/>
        <source>Empty theme description!</source>
        <translation>Leere Themebeschreibung!</translation>
    </message>
    <message>
        <location filename="../mytharchive/themeselector.cpp" line="230"/>
        <source>Unable to open theme description file!</source>
        <translation>Beschreibungsdatei kann nicht geöffnet werden!</translation>
    </message>
</context>
<context>
    <name>EditMetadataDialog</name>
    <message>
        <source>Cancel</source>
        <translation type="obsolete">Abbrechen</translation>
    </message>
    <message>
        <source>OK</source>
        <translation type="obsolete">Ok</translation>
    </message>
</context>
<context>
    <name>ExportNative</name>
    <message>
        <source>Finish</source>
        <translation type="obsolete">Fertigstellen</translation>
    </message>
    <message>
        <source>Previous</source>
        <translation type="obsolete">Zurück</translation>
    </message>
    <message>
        <source>Cancel</source>
        <translation type="obsolete">Abbrechen</translation>
    </message>
    <message>
        <source>Add Recording</source>
        <translation type="obsolete">Aufnahme hinzufügen</translation>
    </message>
    <message>
        <source>Add Video</source>
        <translation type="obsolete">Video hinzufügen</translation>
    </message>
    <message>
        <location filename="../mytharchive/exportnative.cpp" line="198"/>
        <source>You need to add at least one item to archive!</source>
        <translation>Sie müssen mindestens einen Eintrag zum Archivieren hinzufügen!</translation>
    </message>
    <message>
        <location filename="../mytharchive/exportnative.cpp" line="343"/>
        <source>Menu</source>
        <translation>Menü</translation>
    </message>
    <message>
        <location filename="../mytharchive/exportnative.cpp" line="350"/>
        <source>Remove Item</source>
        <translation>Eintrag entfernen</translation>
    </message>
    <message>
        <location filename="../mytharchive/exportnative.cpp" line="442"/>
        <source>It was not possible to create the DVD. An error occured when running the scripts</source>
        <translation>Die Erstellung der DVD war nicht möglich. Während des Ausführens der Skripte ist ein Fehler aufgetreten</translation>
    </message>
    <message>
        <location filename="../mytharchive/exportnative.cpp" line="478"/>
        <source>You don&apos;t have any videos!</source>
        <translation>Es existieren keine Videos!</translation>
    </message>
</context>
<context>
    <name>FileSelector</name>
    <message>
        <source>OK</source>
        <translation type="obsolete">Ok</translation>
    </message>
    <message>
        <source>Cancel</source>
        <translation type="obsolete">Abbrechen</translation>
    </message>
    <message>
        <source>Back</source>
        <translation type="obsolete">Zurück</translation>
    </message>
    <message>
        <source>Home</source>
        <translation type="obsolete">Anfang</translation>
    </message>
    <message>
        <location filename="../mytharchive/fileselector.cpp" line="280"/>
        <source>The selected item is not a directory!</source>
        <translation>Der gewählte Eintrag ist kein Verzeichnis!</translation>
    </message>
    <message>
        <location filename="../mytharchive/fileselector.cpp" line="61"/>
        <source>Find File</source>
        <translation>Datei finden</translation>
    </message>
    <message>
        <location filename="../mytharchive/fileselector.cpp" line="64"/>
        <source>Find Directory</source>
        <translation>Verzeichnis finden</translation>
    </message>
    <message>
        <location filename="../mytharchive/fileselector.cpp" line="67"/>
        <source>Find Files</source>
        <translation>Dateien finden</translation>
    </message>
</context>
<context>
    <name>ImportNative</name>
    <message>
        <source>Finish</source>
        <translation type="obsolete">Fertigstellen</translation>
    </message>
    <message>
        <source>Previous</source>
        <translation type="obsolete">Zurück</translation>
    </message>
    <message>
        <source>Cancel</source>
        <translation type="obsolete">Abbrechen</translation>
    </message>
    <message>
        <source>You need to select a valid chanID!</source>
        <translation type="obsolete">Sie müssen eine gültige chanID auswählen!</translation>
    </message>
    <message>
        <location filename="../mytharchive/importnative.cpp" line="389"/>
        <source>You need to select a valid channel id!</source>
        <translation>Sie müssen eine gültige Kanal ID auswählen!</translation>
    </message>
    <message>
        <location filename="../mytharchive/importnative.cpp" line="420"/>
        <source>It was not possible to import the Archive.  An error occured when running &apos;mytharchivehelper&apos;</source>
        <translation>Es war nicht möglich das Archiv zu importieren. Während der Ausführung von &quot;mytharchivehelper&quot; ist ein Fehler aufgetreten</translation>
    </message>
    <message>
        <location filename="../mytharchive/importnative.cpp" line="550"/>
        <source>Select a channel id</source>
        <translation>Sender ID wählen</translation>
    </message>
    <message>
        <location filename="../mytharchive/importnative.cpp" line="576"/>
        <source>Select a channel number</source>
        <translation>Sendernummer wählen</translation>
    </message>
    <message>
        <location filename="../mytharchive/importnative.cpp" line="602"/>
        <source>Select a channel name</source>
        <translation>Sender wählen</translation>
    </message>
    <message>
        <source>Select a ChanID</source>
        <translation type="obsolete">ChanID wählen</translation>
    </message>
    <message>
        <source>Select a ChanNo</source>
        <translation type="obsolete">ChanNo wählen</translation>
    </message>
    <message>
        <source>Select a Channel Name</source>
        <translation type="obsolete">Sendername wählen</translation>
    </message>
    <message>
        <location filename="../mytharchive/importnative.cpp" line="628"/>
        <source>Select a Callsign</source>
        <translation>Kurzname wählen</translation>
    </message>
</context>
<context>
    <name>LogViewer</name>
    <message>
        <source>Cancel</source>
        <translation type="obsolete">Abbrechen</translation>
    </message>
    <message>
        <source>Update</source>
        <translation type="obsolete">Aktualisieren</translation>
    </message>
    <message>
        <source>Exit</source>
        <translation type="obsolete">Beenden</translation>
    </message>
    <message>
        <location filename="../mytharchive/logviewer.cpp" line="75"/>
        <source>Cannot find any logs to show!</source>
        <translation>Keine Logdateien zum anzeigen gefunden!</translation>
    </message>
    <message>
        <location filename="../mytharchive/logviewer.cpp" line="189"/>
        <source>Background creation has been asked to stop.
This may take a few minutes.</source>
        <translation>Die Hintergrundverarbeitung wurde zum Anhalten angewiesen.
Dies kann einige Minuten in Anspruch nehmen.</translation>
    </message>
    <message>
        <location filename="../mytharchive/logviewer.cpp" line="328"/>
        <source>Menu</source>
        <translation>Menü</translation>
    </message>
    <message>
        <location filename="../mytharchive/logviewer.cpp" line="336"/>
        <source>Don&apos;t Auto Update</source>
        <translation>Nicht autom. aktualisieren</translation>
    </message>
    <message>
        <location filename="../mytharchive/logviewer.cpp" line="338"/>
        <source>Auto Update</source>
        <translation>Autom. aktualisieren</translation>
    </message>
    <message>
        <location filename="../mytharchive/logviewer.cpp" line="340"/>
        <source>Show Progress Log</source>
        <translation>Fortschritt anzeigen</translation>
    </message>
    <message>
        <location filename="../mytharchive/logviewer.cpp" line="341"/>
        <source>Show Full Log</source>
        <translation>ganzes Protokoll anzeigen</translation>
    </message>
</context>
<context>
    <name>MythBurn</name>
    <message>
        <source>Finish</source>
        <translation type="obsolete">Fertigstellen</translation>
    </message>
    <message>
        <source>Previous</source>
        <translation type="obsolete">Zurück</translation>
    </message>
    <message>
        <source>Cancel</source>
        <translation type="obsolete">Abbrechen</translation>
    </message>
    <message>
        <source>Add Recording</source>
        <translation type="obsolete">Aufnahme hinzufügen</translation>
    </message>
    <message>
        <source>Add Video</source>
        <translation type="obsolete">Video hinzufügen</translation>
    </message>
    <message>
        <source>Add File</source>
        <translation type="obsolete">Datei hinzufügen</translation>
    </message>
    <message>
        <source>Using Cutlist</source>
        <translation type="obsolete">Verwende Schnittliste</translation>
    </message>
    <message>
        <source>Not Using Cutlist</source>
        <translation type="obsolete">Verwende keine Schnittliste</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="317"/>
        <location filename="../mytharchive/mythburn.cpp" line="437"/>
        <source>Using Cut List</source>
        <translation>Verwende Schnittliste</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="322"/>
        <location filename="../mytharchive/mythburn.cpp" line="442"/>
        <source>Not Using Cut List</source>
        <translation>Verwende keine Schnittliste</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="328"/>
        <location filename="../mytharchive/mythburn.cpp" line="448"/>
        <source>No Cut List</source>
        <translation>Keine Schnittliste</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="339"/>
        <source>You need to add at least one item to archive!</source>
        <translation>Sie müssen mindestens einen Eintrag zum Archivieren hinzufügen!</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="385"/>
        <source>Retrieving File Information. Please Wait...</source>
        <translation>Hole Dateiinformationen. Bitte warten...</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="451"/>
        <source>Encoder: </source>
        <translation>Kodierer: </translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="763"/>
        <source>Menu</source>
        <translation>Menü</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="774"/>
        <source>Don&apos;t Use Cut List</source>
        <translation>Keine Schnittliste verwenden</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="779"/>
        <source>Use Cut List</source>
        <translation>Schnittliste verwenden</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="964"/>
        <source>You don&apos;t have any videos!</source>
        <translation>Es existieren keine Videos!</translation>
    </message>
    <message>
        <source>Don&apos;t Use Cutlist</source>
        <translation type="obsolete">Keine Schnittliste verwenden</translation>
    </message>
    <message>
        <source>Use Cutlist</source>
        <translation type="obsolete">Schnittliste verwenden</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="784"/>
        <source>Remove Item</source>
        <translation>Eintrag entfernen</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="785"/>
        <source>Edit Details</source>
        <translation>Details bearbeiten</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="786"/>
        <source>Change Encoding Profile</source>
        <translation>Kodierprofil wechseln</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="787"/>
        <source>Edit Thumbnails</source>
        <translation>Miniaturbilder bearbeiten</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="922"/>
        <source>It was not possible to create the DVD.  An error occured when running the scripts</source>
        <translation>Die Erstellung der DVD war nicht möglich. Während des Ausführens der Skripte ist ein Fehler aufgetreten</translation>
    </message>
</context>
<context>
    <name>MythControls</name>
    <message>
        <location filename="../mytharchive/mytharchive.cpp" line="329"/>
        <source>Toggle use cut list state for selected program</source>
        <translation>Schnittliste für die gewählte Sendung ein- ausschalten</translation>
    </message>
    <message>
        <location filename="../mytharchive/mytharchive.cpp" line="332"/>
        <source>Create DVD</source>
        <translation>DVD erzeugen</translation>
    </message>
    <message>
        <location filename="../mytharchive/mytharchive.cpp" line="334"/>
        <source>Create Archive</source>
        <translation>Archiv erzeugen</translation>
    </message>
    <message>
        <location filename="../mytharchive/mytharchive.cpp" line="336"/>
        <source>Import Archive</source>
        <translation>Archiv importieren</translation>
    </message>
    <message>
        <location filename="../mytharchive/mytharchive.cpp" line="338"/>
        <source>View Archive Log</source>
        <translation>Archiv-Protokoll ansehen</translation>
    </message>
    <message>
        <location filename="../mytharchive/mytharchive.cpp" line="340"/>
        <source>Play Created DVD</source>
        <translation>Erzeugte DVD abspielen</translation>
    </message>
    <message>
        <location filename="../mytharchive/mytharchive.cpp" line="342"/>
        <source>Burn DVD</source>
        <translation>DVD brennen</translation>
    </message>
</context>
<context>
    <name>ProfileDialog</name>
    <message>
        <source>Ok</source>
        <translation type="obsolete">Ok</translation>
    </message>
</context>
<context>
    <name>QObject</name>
    <message>
        <source>Myth Archive Temp Directory</source>
        <translation type="obsolete">MythArchive temporäres Verzeichnis</translation>
    </message>
    <message>
        <source>Myth Archive Share Directory</source>
        <translation type="obsolete">MythArchive gemeinsames Verzeichnis</translation>
    </message>
    <message>
        <source>Location where MythArchive stores its scripts, intro movies and theme files</source>
        <translation type="obsolete">Pfad in welchem MythArchive seine Skripte, Intros und Themendateien speichert</translation>
    </message>
    <message>
        <source>Video format</source>
        <translation type="obsolete">Videoformat</translation>
    </message>
    <message>
        <source>Video format for DVD recordings, PAL or NTSC.</source>
        <translation type="obsolete">Videoformat für DVD-Aufnahmen, PAL oder NTSC.</translation>
    </message>
    <message>
        <source>File Selector Filter</source>
        <translation type="obsolete">Dateiauswahlfilter</translation>
    </message>
    <message>
        <source>The file name filter to use in the file selector.</source>
        <translation type="obsolete">Der Filter der im Dateiauswahlmenü benutzt wird.</translation>
    </message>
    <message>
        <source>Location of DVD</source>
        <translation type="obsolete">DVD-Gerät</translation>
    </message>
    <message>
        <source>Which DVD drive to use when burning discs.</source>
        <translation type="obsolete">Welches DVD-Laufwerk zum Brennen benutzt werden soll.</translation>
    </message>
    <message>
        <source>FFmpeg Command</source>
        <translation type="obsolete">Befehl für FFmpeg</translation>
    </message>
    <message>
        <source>Command to run FFmpeg.</source>
        <translation type="obsolete">Der Befehl zum Ausführen von FFmpeg.</translation>
    </message>
    <message>
        <source>mplex Command</source>
        <translation type="obsolete">Befehl für mplex</translation>
    </message>
    <message>
        <source>Command to run mplex</source>
        <translation type="obsolete">Der Befehl zum Ausführen von mplex</translation>
    </message>
    <message>
        <source>dvdauthor command</source>
        <translation type="obsolete">Befehl für dvdauthor</translation>
    </message>
    <message>
        <source>Command to run dvdauthor.</source>
        <translation type="obsolete">Der Befehl zum Ausführen von dvdauthor.</translation>
    </message>
    <message>
        <source>mkisofs command</source>
        <translation type="obsolete">Befehl für mkisofs</translation>
    </message>
    <message>
        <source>Command to run mkisofs. (Used to create ISO images)</source>
        <translation type="obsolete">Der Befehl zum Ausführen von mkisofs (wird benutzt um ISO-Abbilder zu erstellen)</translation>
    </message>
    <message>
        <source>growisofs command</source>
        <translation type="obsolete">Befehl für growisofs</translation>
    </message>
    <message>
        <source>Command to run growisofs. (Used to burn DVD&apos;s)</source>
        <translation type="obsolete">Der Befehl zum Ausführen von growisofs (wird benutzt um DVDs zu brennen)</translation>
    </message>
    <message>
        <source>tcrequant command</source>
        <translation type="obsolete">Befehl für tcrequant</translation>
    </message>
    <message>
        <source>Command to run tcrequant (Part of transcode package). Optional - leave blank if you don&apos;t have the transcode package installed.</source>
        <translation type="obsolete">Der Befehl zum Ausführen von tcrequant (Teil des transcode Paketes). Optional - leer lassen wenn das transcode Paket nicht installiert ist.</translation>
    </message>
    <message>
        <source>spumux command</source>
        <translation type="obsolete">Befehl für spumus</translation>
    </message>
    <message>
        <source>Command to run spumux. Part of dvdauthor package</source>
        <translation type="obsolete">Der Befehl zum Ausführen von spumux. Teil des dvdauthor Paketes</translation>
    </message>
    <message>
        <source>mpeg2enc command</source>
        <translation type="obsolete">Befehl für mpeg2enc</translation>
    </message>
    <message>
        <source>Command to run mpeg2enc. Part of mjpegtools package</source>
        <translation type="obsolete">Der Befehl zum Ausführen von mpeg2enc. Teil des mjpegtool Paketes</translation>
    </message>
    <message>
        <source>MythArchive Settings</source>
        <translation type="obsolete">MythArchive Einstellungen</translation>
    </message>
    <message>
        <source>MythArchive External Commands (1)</source>
        <translation type="obsolete">MythArchive externe Kommandos (1)</translation>
    </message>
    <message>
        <source>MythArchive External Commands (2)</source>
        <translation type="obsolete">MythArchive externe Kommandos (2)</translation>
    </message>
    <message>
        <source>Background creation has been asked to stop.
This may take a few minutes.</source>
        <translation type="obsolete">Die Hintergrundverarbeitung wurde zum Anhalten angewiesen.
Dies kann einige Minuten in Anspruch nehmen.</translation>
    </message>
    <message>
        <source>Location where MythArchive should create its temporory work files. LOTS of free space required here.</source>
        <translation type="obsolete">Pfad in welchem MythArchive seine temporären Arbeitsdateien erstellt. Hier wird VIEL Platz benötigt.</translation>
    </message>
    <message>
        <source>Always Encode to AC3</source>
        <translation type="obsolete">Immer zu AC3 kodieren</translation>
    </message>
    <message>
        <source>If set audio tracks will always be re-encoded to AC3 for better compatibility with DVD players in NTSC countries.</source>
        <translation type="obsolete">Für eine bessere Kompatibilität mit den DVD-Abspielgeräten in NTSC-Ländern werden bei dieser Auswahl Tonspuren immer zu AC3 neu-kodiert.</translation>
    </message>
    <message>
        <source>Copy remote files</source>
        <translation type="obsolete">Kopiere entfernte Dateien</translation>
    </message>
    <message>
        <source>If set files on remote filesystems will be copied over to the local filesystem before processing. Speeds processing and reduces bandwidth on the network</source>
        <translation type="obsolete">Falls gesetzt, werden Dateien auf entfernten Dateisystemen vor der Verarbeitung auf das lokale Dateisystem kopiert. Dies beschleunigt die Verarbeitung und reduziert die Auslastung des Netzwerkes</translation>
    </message>
    <message>
        <source>Always Use Mythtranscode</source>
        <translation type="obsolete">Immer Mythtranscode benutzen</translation>
    </message>
    <message>
        <source>Use FIFOs</source>
        <translation type="obsolete">FIFOs benutzen</translation>
    </message>
    <message>
        <source>Main Menu Aspect Ratio</source>
        <translation type="obsolete">Hauptmenü Seitenverhältnis</translation>
    </message>
    <message>
        <source>Aspect ratio to use when creating the main menu.</source>
        <translation type="obsolete">Welches Seitenverhältnis beim Erstellen des Hauptmenüs benutzt werden soll.</translation>
    </message>
    <message>
        <source>Chapter Menu Aspect Ratio</source>
        <translation type="obsolete">Kapitelmenü Seitenverhältnis</translation>
    </message>
    <message>
        <source>Aspect ratio to use when creating the chapter menu. Video means use the same aspect ratio as the associated video.</source>
        <translation type="obsolete">Welches Seitenverhältnis beim Erstellen des Kapitelmenüs benutzt werden soll. Bei &quot;Video&quot; wird das Seitenverhältnis des entsprechenden Videos verwendet.</translation>
    </message>
    <message>
        <source>MythArchive Settings (2)</source>
        <translation type="obsolete">MythArchive  Einstellungen (2)</translation>
    </message>
    <message>
        <source>DVD Drive Write Speed</source>
        <translation type="obsolete">DVD-Schreibgeschwindigkeit</translation>
    </message>
    <message>
        <source>MythArchive Temp Directory</source>
        <translation type="obsolete">MythArchive Temp Verzeichnis</translation>
    </message>
    <message>
        <source>Location where MythArchive should create its temporary work files. LOTS of free space required here.</source>
        <translation type="obsolete">Pfad in welchem MythArchive seine temporären Arbeitsdateien erstellt. Hier wird VIEL Platz benötigt.</translation>
    </message>
    <message>
        <source>MythArchive Share Directory</source>
        <translation type="obsolete">MythArchive gemeinsames Verzeichnis</translation>
    </message>
    <message>
        <source>This is the write speed to use when burning a DVD. Set to 0 to allow growisofs to choose the fastest available speed.</source>
        <translation type="obsolete">Die Schreibgeschwindigkeit beim Brennen von DVDs. Bei 0 wird &quot;growisofs&quot; automatisch die höchste Geschwindigkeit wählen.</translation>
    </message>
    <message>
        <source>Command to play DVD</source>
        <translation type="obsolete">DVD-Abspielbefehl</translation>
    </message>
    <message>
        <source>Command to run when test playing a created DVD. &apos;Internal&apos; will use the internal MythTV player. %f will be replaced with the path to the created DVD structure eg. &apos;xine -pfhq --no-splash dvd:/%f&apos;.</source>
        <translation type="obsolete">Befehl zum testweisen Abspielen einer neuen DVD. Bei &quot;Internal&quot; wird der MythTV-Player verwendet. &quot;%f&quot; wird durch den Pfad der erzeugten DVD-Struktur ersetzt, z.B. &quot;xine -pfhq --no-splash dvd:/%f&quot;.</translation>
    </message>
    <message>
        <source>If set mpeg2 files will always be passed though mythtranscode to clean up any errors. May help to fix some audio problems. Ignored if &apos;Use ProjectX&apos; is set.</source>
        <translation type="obsolete">Falls gesetzt, werden MPEG2-Dateien immer durch mythtranscode gefiltert um etwaige Fehler zu beheben. Dies könnte manche Tonprobleme beheben. Diese Option wird ignoriert falls &quot;Nutze ProjectX&quot; aktiv ist.</translation>
    </message>
    <message>
        <source>Use ProjectX</source>
        <translation type="obsolete">Nutze ProjectX</translation>
    </message>
    <message>
        <source>If set ProjectX will be used to cut commercials and split mpeg2 files instead of mythtranscode and mythreplex.</source>
        <translation type="obsolete">Falls gesetzt, wird ProjectX statt mythtranscode und mythreplex verwendet um Werbung heraus zu schneiden und MPEG2-Dateien aufzuteilen.</translation>
    </message>
    <message>
        <source>The script will use FIFOs to pass the output of mplex into dvdauthor rather than creating intermediate files. Saves time and disk space during multiplex operations but not  supported on Windows platform</source>
        <translation type="obsolete">Das Skript wird FIFOs an Stelle von temporären Dateien verwenden um die Ausgabe von mplex an dvdauthor weiter zu leiten. Dies spart Zeit und Plattenplatz während Multiplex-Vorgängen, wird unter Windows aber nicht unterstützt</translation>
    </message>
    <message>
        <source>Add Subtitles</source>
        <translation type="obsolete">Untertitel hinzufügen</translation>
    </message>
    <message>
        <source>If available this option will add subtitles to the final DVD. Requires &apos;Use ProjectX&apos; to be on.</source>
        <translation type="obsolete">Falls verfügbar, werden der endgültigen DVD Untertitel hinzugefügt. Dazu muss &quot;Nutze ProjectX&quot; aktiviert sein.</translation>
    </message>
    <message>
        <source>Date format</source>
        <translation type="obsolete">Datumsformat</translation>
    </message>
    <message>
        <source>Samples are shown using today&apos;s date.</source>
        <translation type="obsolete">Die Beispiele gelten für das heutige Datum.</translation>
    </message>
    <message>
        <source>Samples are shown using tomorrow&apos;s date.</source>
        <translation type="obsolete">Die Beispiele gelten für Morgen.</translation>
    </message>
    <message>
        <source>Your preferred date format to use on DVD menus.</source>
        <translation type="obsolete">Ihr bevorzugtes Zeitformat für DVD-Menüs.</translation>
    </message>
    <message>
        <source>Time format</source>
        <translation type="obsolete">Zeitformat</translation>
    </message>
    <message>
        <source>Your preferred time format to display on DVD menus. You must choose a format with &quot;AM&quot; or &quot;PM&quot; in it, otherwise your time display will be 24-hour or &quot;military&quot; time.</source>
        <translation type="obsolete">Ihr bevorzugtes Zeitformat für DVD-Menüs. Einträge mit &quot;am&quot; bzw &quot;pm&quot; zeigen die Zeit mit 12 Stunden Teilung an.</translation>
    </message>
    <message>
        <source>Default Encoder Profile</source>
        <translation type="obsolete">Standard Enkoderprofil</translation>
    </message>
    <message>
        <source>Default encoding profile to use if a file needs re-encoding.</source>
        <translation type="obsolete">Das Standard Enkoderprofil für Dateien neu enkodiert werden müssen.</translation>
    </message>
    <message>
        <source>M2VRequantiser command</source>
        <translation type="obsolete">M2V-Grössenwandler-Kommando</translation>
    </message>
    <message>
        <source>Command to run M2VRequantiser. Optional - leave blank if you don&apos;t have M2VRequantiser installed.</source>
        <translation type="obsolete">Kommando zum Ausführen des M2V-Grössenwandler. Optional - Leer lassen wenn der M2V-Grössenwandler nicht installiert ist.</translation>
    </message>
    <message>
        <source>jpeg2yuv command</source>
        <translation type="obsolete">Befehl für jpeg2yuv</translation>
    </message>
    <message>
        <source>Command to run jpeg2yuv. Part of mjpegtools package</source>
        <translation type="obsolete">Befehl um jpeg2yuv zu starten. Dies ist Teil des mjpegtools Pakets</translation>
    </message>
    <message>
        <source>projectx command</source>
        <translation type="obsolete">Befehl für projectx</translation>
    </message>
    <message>
        <source>Command to run ProjectX. Will be used to cut commercials and split mpegs files instead of mythtranscode and mythreplex.</source>
        <translation type="obsolete">Befehl um ProjectX zu starten. Wird statt mythtranscode und mythreplex verwendet um Werbung heraus zu schneiden und MPEG-Dateien aufzuteilen.</translation>
    </message>
    <message>
        <source>DVD Menu Settings</source>
        <translation type="obsolete">DVD-Menü Einstellungen</translation>
    </message>
    <message>
        <source>You don&apos;t have any videos!</source>
        <translation type="obsolete">Es existieren keine Videos!</translation>
    </message>
    <message>
        <source>It was not possible to create the DVD. An error occured when running the scripts</source>
        <translation type="obsolete">Die Erstellung der DVD war nicht möglich. Während des Ausführens der Skripte ist ein Fehler aufgetreten</translation>
    </message>
    <message>
        <source>Cannot find the MythArchive work directory.
Have you set the correct path in the settings?</source>
        <translation type="obsolete">Konnte das MythArchive-Arbeitsverzeichnis nicht finden.
Stimmt der Pfad in den Einstellungen?</translation>
    </message>
    <message>
        <source>Found a lock file but the owning process isn&apos;t running!
Removing stale lock file.</source>
        <translation type="obsolete">Eine Lock-Datei existiert aber der dazugehörige Prozess läuft nicht!
Lösche Lock-Datei.</translation>
    </message>
    <message>
        <source>Cannot find any logs to show!</source>
        <translation type="obsolete">Keine Protokoll-Dateien anzuzeigen!</translation>
    </message>
    <message>
        <source>Last run did not create a playable DVD.</source>
        <translation type="obsolete">Der letzte Durchgang hat keine spielbare DVD erzeugt.</translation>
    </message>
    <message>
        <source>Last run failed to create a DVD.</source>
        <translation type="obsolete">Der letzte Durchgang hat keine DVD erzeugt.</translation>
    </message>
    <message>
        <source>Cannot burn a DVD.
The last run failed to create a DVD.</source>
        <translation type="obsolete">Kann keine DVD brennen.
Der letzte Durchgang hat keine DVD erzeugt.</translation>
    </message>
    <message>
        <source>
Place a blank DVD in the drive and select an option below.</source>
        <translation type="obsolete">
Legen Sie eine leere DVD ins Laufwerk und wählen Sie eine Option.</translation>
    </message>
    <message>
        <source>Burn DVD</source>
        <translation type="obsolete">DVD brennen</translation>
    </message>
    <message>
        <source>Burn DVD Rewritable</source>
        <translation type="obsolete">DVD-RW brennen</translation>
    </message>
    <message>
        <source>Burn DVD Rewritable (Force Erase)</source>
        <translation type="obsolete">DVD-RW brennen (löschen forcieren)</translation>
    </message>
    <message>
        <source>Cancel</source>
        <translation type="obsolete">Abbrechen</translation>
    </message>
    <message>
        <source>It was not possible to run mytharchivehelper to burn the DVD.</source>
        <translation type="obsolete">Konnte mytharchivehelper nicht starten um die DVD zu brennen.</translation>
    </message>
</context>
<context>
    <name>RecordingSelector</name>
    <message>
        <location filename="../mytharchive/recordingselector.cpp" line="110"/>
        <source>Retrieving Recording List.
Please Wait...</source>
        <translation>Hole Aufnahmeliste.
Bitte warten...</translation>
    </message>
    <message>
        <location filename="../mytharchive/recordingselector.cpp" line="189"/>
        <source>Clear All</source>
        <translation>Auswahl löschen</translation>
    </message>
    <message>
        <location filename="../mytharchive/recordingselector.cpp" line="190"/>
        <source>Select All</source>
        <translation>Alles auswählen</translation>
    </message>
    <message>
        <source>Cancel</source>
        <translation type="obsolete">Abbrechen</translation>
    </message>
    <message>
        <source>OK</source>
        <translation type="obsolete">Ok</translation>
    </message>
    <message>
        <location filename="../mytharchive/recordingselector.cpp" line="89"/>
        <location filename="../mytharchive/recordingselector.cpp" line="374"/>
        <location filename="../mytharchive/recordingselector.cpp" line="479"/>
        <source>All Recordings</source>
        <translation>Alle Aufnahmen</translation>
    </message>
    <message>
        <source>Retrieving Recording List. Please Wait...</source>
        <translation type="obsolete">Hole Aufnahmenliste. Bitte warten...</translation>
    </message>
    <message>
        <location filename="../mytharchive/recordingselector.cpp" line="134"/>
        <source>Either you don&apos;t have any recordings or no recordings are available locally!</source>
        <translation>Entweder es existieren keine Aufnahmen oder sie sind nicht lokal verfügbar!</translation>
    </message>
    <message>
        <location filename="../mytharchive/recordingselector.cpp" line="182"/>
        <source>Menu</source>
        <translation>Menü</translation>
    </message>
</context>
<context>
    <name>SelectDestination</name>
    <message>
        <source>Next</source>
        <translation type="obsolete">Weiter</translation>
    </message>
    <message>
        <source>Previous</source>
        <translation type="obsolete">Zurück</translation>
    </message>
    <message>
        <source>Cancel</source>
        <translation type="obsolete">Abbrechen</translation>
    </message>
    <message>
        <source>Find...</source>
        <translation type="obsolete">Finden...</translation>
    </message>
    <message>
        <location filename="../mytharchive/archiveutil.cpp" line="29"/>
        <source>Single Layer DVD</source>
        <translation>Single Layer DVD</translation>
    </message>
    <message>
        <source>Single Layer DVD (4482Mb)</source>
        <translation type="obsolete">Single Layer DVD (4482 Mb)</translation>
    </message>
    <message>
        <location filename="../mytharchive/archiveutil.cpp" line="33"/>
        <source>Dual Layer DVD</source>
        <translation>Dual Layer DVD</translation>
    </message>
    <message>
        <source>Dual Layer DVD (8964Mb)</source>
        <translation type="obsolete">Dual Layer DVD (8964 Mb)</translation>
    </message>
    <message>
        <location filename="../mytharchive/archiveutil.cpp" line="30"/>
        <source>Single Layer DVD (4,482 MB)</source>
        <translation>Single Layer DVD (4.482 MB)</translation>
    </message>
    <message>
        <location filename="../mytharchive/archiveutil.cpp" line="34"/>
        <source>Dual Layer DVD (8,964 MB)</source>
        <translation>Dual Layer DVD (8.964 MB)</translation>
    </message>
    <message>
        <location filename="../mytharchive/archiveutil.cpp" line="37"/>
        <source>DVD +/- RW</source>
        <translation>DVD +/- RW</translation>
    </message>
    <message>
        <location filename="../mytharchive/archiveutil.cpp" line="38"/>
        <source>Rewritable DVD</source>
        <translation>Wiederbeschreibbare DVD</translation>
    </message>
    <message>
        <location filename="../mytharchive/archiveutil.cpp" line="41"/>
        <source>File</source>
        <translation>Datei</translation>
    </message>
    <message>
        <location filename="../mytharchive/archiveutil.cpp" line="42"/>
        <source>Any file accessable from your filesystem.</source>
        <translation>Jede verfügbare Datei auf Ihrem Dateisystem.</translation>
    </message>
    <message>
        <location filename="../mytharchive/selectdestination.cpp" line="262"/>
        <location filename="../mytharchive/selectdestination.cpp" line="318"/>
        <source>Unknown</source>
        <translation>Unbekannt</translation>
    </message>
</context>
<context>
    <name>ThemeSelector</name>
    <message>
        <source>Next</source>
        <translation type="obsolete">Weiter</translation>
    </message>
    <message>
        <source>Previous</source>
        <translation type="obsolete">Zurück</translation>
    </message>
    <message>
        <source>Cancel</source>
        <translation type="obsolete">Abbrechen</translation>
    </message>
</context>
<context>
    <name>ThemeUI</name>
    <message>
        <location filename="themestrings.h" line="225"/>
        <source>Select Recordings</source>
        <translation>Aufnahmen auswählen</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="241"/>
        <source>Show Recordings</source>
        <translation>Aufnahmen ansehen</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="290"/>
        <source>x.xx Gb</source>
        <translation>x.xx Gb</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="123"/>
        <source>File Finder</source>
        <translation>Dateienfinder</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="272"/>
        <source>Video Category</source>
        <translation>Videokategorie</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="187"/>
        <source>PL:</source>
        <translation>PL:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="6"/>
        <source>%SIZE% ~ %PROFILE%</source>
        <translation>%SIZE% ~ %PROFILE%</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="8"/>
        <source>%date% / %profile%</source>
        <translation>%date% / %profile%</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="9"/>
        <source>%size% (%profile%)</source>
        <translation>%size% (%profile%)</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="15"/>
        <source>1</source>
        <translation>1</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="18"/>
        <source>&lt;-- Details</source>
        <translation>&lt;-- Details</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="19"/>
        <source>&lt;-- Main Menu</source>
        <translation>&lt;-- Hauptmenü</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="26"/>
        <source>Add Recordings</source>
        <translation>Aufnahmen hinzufügen</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="28"/>
        <source>Add Videos</source>
        <translation>Videos hinzufügen</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="29"/>
        <source>Add a recording or video to archive.</source>
        <translation>Eine Aufnahme oder ein Video zum Archiv hinzufügen.</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="30"/>
        <source>Add a recording, video or file to archive.</source>
        <translation>Eine Aufnahme, ein Video oder eine Datei zum Archiv hinzufügen.</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="32"/>
        <source>Archive</source>
        <translation>Archiv</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="33"/>
        <source>Archive Callsign:</source>
        <translation>Archiv Kurzname:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="34"/>
        <source>Archive Chan ID:</source>
        <translation>Archiv Sender-ID:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="35"/>
        <source>Archive Chan No:</source>
        <translation>Archiv Sender-Nr:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="36"/>
        <source>Archive Files</source>
        <translation>Archivdateien</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="38"/>
        <source>Archive Item:</source>
        <translation>Archivelement:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="40"/>
        <source>Archive Items to DVD</source>
        <translation>Archivierung auf DVD</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="41"/>
        <source>Archive Log Viewer</source>
        <translation>Archiv Protokollansicht</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="42"/>
        <source>Archive Media</source>
        <translation>Medienarchivierung</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="43"/>
        <source>Archive Name:</source>
        <translation>Archivname:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="46"/>
        <source>Associate Channel</source>
        <translation>Sender zuordnen</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="47"/>
        <source>Associated Channel</source>
        <translation>Zugeordneter Sender</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="49"/>
        <source>Burn</source>
        <translation>Brennen</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="50"/>
        <source>Burn Created DVD</source>
        <translation>DVD brennen</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="51"/>
        <source>Burn the last created archive to DVD</source>
        <translation>Zuletzt erstelltes Archiv auf DVD brennen</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="53"/>
        <source>Burn to DVD:</source>
        <translation>Auf DVD brennen:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="54"/>
        <source>CUTLIST</source>
        <translation>SCHNITTLISTE</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="55"/>
        <source>Callsign</source>
        <translation>Kurzname</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="57"/>
        <source>Callsign:  %1</source>
        <translation>Kurzname: %1</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="59"/>
        <source>Cancel Job</source>
        <translation>Auftrag abbrechen</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="61"/>
        <source>Chan. Id:</source>
        <translation>Sender-ID:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="63"/>
        <source>Change</source>
        <translation>Ändern</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="65"/>
        <source>Channel ID</source>
        <translation>Sender-ID</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="67"/>
        <source>Channel ID:  %1</source>
        <translation>Sender-ID: %1</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="68"/>
        <source>Channel Name:</source>
        <translation>Sendername:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="69"/>
        <source>Channel No</source>
        <translation>Sendernummer</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="71"/>
        <source>Channel Number:  %1</source>
        <translation>Sendernummer: %1</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="74"/>
        <source>Chapter Menu --&gt;</source>
        <translation>Kapitelmenü --&gt;</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="75"/>
        <source>Chapter Menu:</source>
        <translation>Kapitelmenü:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="76"/>
        <source>Chapters</source>
        <translation>Kapitel</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="82"/>
        <source>Create ISO Image:</source>
        <translation>ISO-Abbild erzeugen:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="83"/>
        <source>Create a</source>
        <translation>Erzeuge eine</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="84"/>
        <source>Create a DVD of your videos</source>
        <translation>Erzeuge eine DVD deiner Videos</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="85"/>
        <source>Current Destination:</source>
        <translation>Aktuelles Ziel:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="87"/>
        <source>Current Position:</source>
        <translation>Aktuelle Position:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="88"/>
        <source>Current Size:</source>
        <translation>Aktuelle Größe:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="89"/>
        <source>Current selected item size: %1</source>
        <translation>Größe aktuelles Element: %1</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="90"/>
        <source>Current size: %1</source>
        <translation>Aktuelle Größe: %1</translation>
    </message>
    <message numerus="yes">
        <location filename="themestrings.h" line="91"/>
        <source>Current: %n</source>
        <translation>
            <numerusform>Aktuell: %n</numerusform>
            <numerusform>Aktuell: %n</numerusform>
        </translation>
    </message>
    <message>
        <location filename="themestrings.h" line="92"/>
        <source>DVD</source>
        <translation>DVD</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="93"/>
        <source>DVD Media</source>
        <translation>DVD Medien</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="94"/>
        <source>DVD Menu</source>
        <translation>DVD Menü</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="96"/>
        <source>Date (time)</source>
        <translation>Datum (Zeit)</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="98"/>
        <source>Destination</source>
        <translation>Ziel</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="99"/>
        <source>Destination Free Space:</source>
        <translation>Freier Speicher auf Ziel:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="103"/>
        <source>Details:</source>
        <translation>Details:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="104"/>
        <source>Done</source>
        <translation>Fertig</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="105"/>
        <source>Edit Archive item Information</source>
        <translation>Information von Archivelement bearbeiten</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="106"/>
        <source>Edit Details</source>
        <translation>Details bearbeiten</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="107"/>
        <source>Edit Thumbnails</source>
        <translation>Miniaturbilder bearbeiten</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="108"/>
        <source>Eject your</source>
        <translation>Auswerfen von</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="110"/>
        <source>Encode your video</source>
        <translation>Umwandlung der Video</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="111"/>
        <source>Encode your video to a file</source>
        <translation>Umwandlung eines Video in eine Datei</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="112"/>
        <source>Encoded Size:</source>
        <translation>Neue Größe:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="113"/>
        <source>Encoder Profile:</source>
        <translation>Umwandlungsprofil:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="114"/>
        <source>Encoding Profile</source>
        <translation>Umwandlungsprofil</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="116"/>
        <source>Error: %1</source>
        <translation>Fehler: %1</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="118"/>
        <source>Export</source>
        <translation>Export</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="119"/>
        <source>Export your</source>
        <translation>Exportiere deine</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="120"/>
        <source>Export your videos</source>
        <translation>Exportiere deine Videos</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="121"/>
        <source>File</source>
        <translation>Datei</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="122"/>
        <source>File Browser</source>
        <translation>Dateimanager</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="125"/>
        <source>File browser</source>
        <translation>Dateimanager</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="126"/>
        <source>File...</source>
        <translation>Datei...</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="127"/>
        <source>Filename:</source>
        <translation>Dateiname:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="128"/>
        <source>Filesize</source>
        <translation>Dateigröße</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="132"/>
        <source>Find Location</source>
        <translation>Ort suchen</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="134"/>
        <source>First, select the thumb image you want to change from the overview. Second, press the &apos;tab&apos; key to move to the select thumb frame button to change the image.</source>
        <translation>Zuerst zu änderndes Miniaturbild aus der Übersicht auswählen. Dann &apos;TAB&apos;-Taste drücken um zur Schaltfläche für die Miniatubildauswahl zu wechseln.</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="136"/>
        <source>Force Overwrite of DVD-RW Media:</source>
        <translation>DVD-RW-Medien überschreiben:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="140"/>
        <source>Import</source>
        <translation>Import</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="142"/>
        <source>Import an</source>
        <translation>Importieren eines</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="143"/>
        <source>Import recordings from a native archive</source>
        <translation>Aufnahmen von einem eigenen Archiv importieren</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="144"/>
        <source>Import videos from an Archive</source>
        <translation>Videos von einem Archiv importieren</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="146"/>
        <source>Intro --&gt;</source>
        <translation>Intro --&gt;</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="147"/>
        <source>Intro:</source>
        <translation>Intro:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="148"/>
        <source>Local Callsign:</source>
        <translation>Lokaler Kurzname:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="149"/>
        <source>Local Chan ID:</source>
        <translation>Lokale Sender-ID:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="150"/>
        <source>Local Chan No:</source>
        <translation>Lokale Sendernr.:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="152"/>
        <source>Local Name:</source>
        <translation>Lokaler Name:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="153"/>
        <source>Log</source>
        <translation>Protokoll</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="155"/>
        <source>Log viewer</source>
        <translation>Protokollansicht</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="156"/>
        <source>MENU changes focus. Numbers 0-9 jump to that thumb image.
When the preview image has focus, UP/DOWN changes the seek amount, LEFT/RIGHT jumps forward/backward by the seek amount, and SELECT chooses the current preview image for the selected thumb image.</source>
        <translation>&apos;MENU&apos; ändert den Fokus. Nummern 0-9 springen zu diesem Miniaturbild.\nWenn das Miniaturbild fokussiert ist, ändert &apos;UP&apos;/&apos;DOWN&apos; die Sprungweite, &apos;LEFT&apos;/&apos;RIGHT&apos; springt vor-/rückwärts mit der Sprungweite und &apos;SELECT&apos; wählt das Miniaturbild aus.</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="158"/>
        <source>Main Menu:</source>
        <translation>Hauptmenü:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="159"/>
        <source>Main menu</source>
        <translation>Hauptmenü</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="161"/>
        <source>Make ISO image</source>
        <translation>ISO-Abbild erzeugen</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="162"/>
        <source>Make ISO image:</source>
        <translation>ISO-Abbild erzeugen:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="163"/>
        <source>Media</source>
        <translation>Medien</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="165"/>
        <source>Metadata</source>
        <translation>Metadaten</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="166"/>
        <source>Name</source>
        <translation>Name</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="168"/>
        <source>Name:  %1</source>
        <translation>Name: %1</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="169"/>
        <source>Navigation</source>
        <translation>Navigation</translation>
    </message>
    <message numerus="yes">
        <location filename="themestrings.h" line="172"/>
        <source>New: %n</source>
        <translation>
            <numerusform>Neu: %n</numerusform>
            <numerusform>Neu: %n</numerusform>
        </translation>
    </message>
    <message>
        <location filename="themestrings.h" line="174"/>
        <source>No files are selected for DVD</source>
        <translation>Keine Dateien für DVD ausgewählt</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="176"/>
        <source>No videos available</source>
        <translation>Keine Videos vorhanden</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="177"/>
        <source>Not Applicable</source>
        <translation>Nicht anwendbar</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="178"/>
        <source>Not Available in this Theme</source>
        <translation>Nicht verfügbar in diesem Erscheinungsbild</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="179"/>
        <source>Not available in this theme</source>
        <translation>Nicht verfügbar in diesem Erscheinungsbild</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="184"/>
        <source>Original Size:</source>
        <translation>Originalgröße:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="186"/>
        <source>Overwrite DVD-RW Media:</source>
        <translation>DVD-RW-Medien überschreiben:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="188"/>
        <source>Parental Level</source>
        <translation>Altersfreigabe</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="189"/>
        <source>Parental Level:</source>
        <translation>Altersfreigabe:</translation>
    </message>
    <message numerus="yes">
        <location filename="themestrings.h" line="191"/>
        <source>Parental Level: %n</source>
        <translation>
            <numerusform>Altersfreigabe: %n</numerusform>
            <numerusform>Altersfreigabe: %n</numerusform>
        </translation>
    </message>
    <message>
        <location filename="themestrings.h" line="194"/>
        <source>Play the last created archive DVD</source>
        <translation>Zuletzt erzeugte Archiv-DVD abspielen</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="195"/>
        <source>Position View:</source>
        <translation>Positionsansicht:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="196"/>
        <source>Press &apos;left&apos; and &apos;right&apos; arrows to move through the frames. Press &apos;up&apos; and &apos;down&apos; arrows to change seek amount. Press &apos;select&apos; or &apos;enter&apos; key to lock the selected thumb image.</source>
        <translation>Drücke &apos;LEFT&apos; und &apos;RIGHT&apos;-Pfeiltasten um zwischen Bildern zu navigieren. Drücke &apos;UP&apos; und &apos;DOWN&apos;-Pfeiltasten um Sprungweite zu ändern. &apos;SELECT&apos; oder &apos;ENTER&apos; drücken um Miniaturbild auszuwählen.</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="198"/>
        <source>Preview</source>
        <translation>Vorschau</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="201"/>
        <source>Read video from data dvd or file</source>
        <translation>Video von Daten-DVD oder Datei lesen</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="203"/>
        <source>Recordings</source>
        <translation>Aufnahmen</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="204"/>
        <source>Recordings group:</source>
        <translation>Aufnahmegruppe:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="206"/>
        <source>Save recordings and videos to a native archive</source>
        <translation>Aufnahmen und Videos in eigenes Archiv speichern</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="207"/>
        <source>Save recordings and videos to video DVD</source>
        <translation>Aufnahmen und Videos auf DVD speichern</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="209"/>
        <source>Search Chan ID</source>
        <translation>Sender-ID suchen</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="210"/>
        <source>Search Chan NO</source>
        <translation>Sender-Nr. suchen</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="212"/>
        <source>Search Local</source>
        <translation>Lokal suchen</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="216"/>
        <source>Seek Amount:</source>
        <translation>Sprungweite:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="217"/>
        <source>Seek amount:</source>
        <translation>Sprungweite:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="220"/>
        <source>Select Channel</source>
        <translation>Sender auswählen</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="221"/>
        <source>Select Destination</source>
        <translation>Ziel auswählen</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="293"/>
        <source>~</source>
        <translation>~</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="138"/>
        <source>Free Space:</source>
        <translation>Freier Platz:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="13"/>
        <source>0.00Gb</source>
        <translation>0.00 Gb</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="160"/>
        <source>Make ISO Image</source>
        <translation>ISO-Abbild erstellen</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="52"/>
        <source>Burn to DVD</source>
        <translation>Auf DVD schreiben</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="135"/>
        <source>Force Overwrite of DVD-RW Media</source>
        <translation>Überschreiben des DVD-RW-Mediums erzwingen</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="218"/>
        <source>Select Archive Items</source>
        <translation>Archivobjekte auswählen</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="224"/>
        <source>Select Files</source>
        <translation>Dateien auswählen</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="226"/>
        <source>Select Recordings for your archive or image</source>
        <translation>Aufnahmen für Archiv oder Abbild auswählen</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="228"/>
        <source>Select a destination for your archive or image</source>
        <translation>Ziel für Archiv oder Abbild auswählen</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="233"/>
        <source>Select thumb image:</source>
        <translation>Miniaturbild auswählen:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="234"/>
        <source>Select your DVD menu theme</source>
        <translation>DVD-Menu Erscheinungsbild auswählen</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="235"/>
        <source>Selected Items to be archived</source>
        <translation>Zu archivierende Elemente</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="236"/>
        <source>Selected Items to be burned</source>
        <translation>Zu brennende Elemente</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="237"/>
        <source>Selected recording item size: %1</source>
        <translation>Größe ausgewählter Aufnahme: %1</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="238"/>
        <source>Selected video item size: %1</source>
        <translation>Größe ausgewähltes Video: %1</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="239"/>
        <source>Sep 13, 2004 11:00 pm (1h 15m)</source>
        <translation>Sep 13, 2004 11:00 pm (1h 15m)</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="240"/>
        <source>Show</source>
        <translation>Anzeigen des</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="242"/>
        <source>Show Videos</source>
        <translation>Videos anzeigen</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="243"/>
        <source>Show log with archive activities</source>
        <translation>Protokoll mit Aktivitäten anzeigen</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="244"/>
        <source>Show the Archive Log Viewer</source>
        <translation>Protokollansicht anzeigen</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="245"/>
        <source>Size:</source>
        <translation>Größe:</translation>
    </message>
    <message numerus="yes">
        <location filename="themestrings.h" line="246"/>
        <source>Size: %n</source>
        <translation>
            <numerusform>Größe: %n</numerusform>
            <numerusform>Größe: %n</numerusform>
        </translation>
    </message>
    <message>
        <location filename="themestrings.h" line="249"/>
        <source>Start Time: %1</source>
        <translation>Startzeit: %1</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="250"/>
        <source>Start to</source>
        <translation>Beginne mit dem</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="251"/>
        <source>Start to Burn your archive to DVD</source>
        <translation>Archiv auf DVD brennen</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="253"/>
        <source>Subtitle: %1</source>
        <translation>Untertitel: %1</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="254"/>
        <source>Test</source>
        <translation>Test</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="255"/>
        <source>Test created</source>
        <translation>Teste erzeugte</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="256"/>
        <source>Test created DVD</source>
        <translation>Erzeugte DVD testen</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="257"/>
        <source>Theme description</source>
        <translation>Beschreibung Erscheinungsbild</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="258"/>
        <source>Theme for the DVD menu:</source>
        <translation>Erscheinungsbild für DVD-Menü:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="259"/>
        <source>Theme preview:</source>
        <translation>Vorschau Erscheinungsbild:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="264"/>
        <source>Title: %1</source>
        <translation>Titel: %1</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="265"/>
        <source>Tools and Utilities for archives</source>
        <translation>Werkzeuge und Hilfsprogramme für Archive</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="268"/>
        <source>Use archive</source>
        <translation>Verwende Archiv</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="269"/>
        <source>Used: %1</source>
        <translation>Benutzt: %1</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="270"/>
        <source>Utilities</source>
        <translation>Hilfsprogramme</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="271"/>
        <source>Utilities for MythArchive</source>
        <translation>Hilfsprogramme für MythArchive</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="274"/>
        <source>Video category:</source>
        <translation>Videokategorie:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="275"/>
        <source>Videos</source>
        <translation>Videos</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="276"/>
        <source>View progress of your archive or image</source>
        <translation>Fortschritt von Archiv oder Abbild ansehen</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="277"/>
        <source>Write video to data dvd or file</source>
        <translation>Video auf Daten-DVD oder in Datei schreiben</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="278"/>
        <source>XML File to Import</source>
        <translation>XML-Datei für Import</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="279"/>
        <source>decrease seek amount</source>
        <translation>Sprungweite verringern</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="281"/>
        <source>frame</source>
        <translation>Frame</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="282"/>
        <source>increase seek amount</source>
        <translation>Sprungweite erhöhen</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="283"/>
        <source>move left</source>
        <translation>nach links</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="284"/>
        <source>move right</source>
        <translation>nach rechts</translation>
    </message>
    <message numerus="yes">
        <location filename="themestrings.h" line="285"/>
        <source>profile: %n</source>
        <translation>
            <numerusform>Profil: %n</numerusform>
            <numerusform>Profil: %n</numerusform>
        </translation>
    </message>
    <message>
        <location filename="themestrings.h" line="288"/>
        <source>to</source>
        <translation>zu</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="292"/>
        <source>xxxxx mb</source>
        <translation>xxxxx Mb</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="11"/>
        <source>0 mb</source>
        <translation>0 Mb</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="95"/>
        <source>DVD Menu Theme</source>
        <translation>DVD-Menüthema</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="229"/>
        <source>Select a theme</source>
        <translation>Thema auswählen</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="145"/>
        <source>Intro</source>
        <translation>Einleitung</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="5"/>
        <source>%DATE%, %TIME%</source>
        <translation>%DATE%, %TIME%</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="78"/>
        <source>Choose where you would like your files archived.</source>
        <translation>Wählen Sie wohin Ihre Dateien archiviert werden sollen.</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="185"/>
        <source>Output Type:</source>
        <translation>Ausgabetyp:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="100"/>
        <source>Destination:</source>
        <translation>Ziel:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="79"/>
        <source>Click here to find an output location...</source>
        <translation>Klicken Sie hier um einen Ausgabeort auszuwählen...</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="115"/>
        <source>Erase DVD-RW before burning</source>
        <translation>DVD-RW vor dem brennen löschen</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="58"/>
        <source>Cancel</source>
        <translation>Abbrechen</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="199"/>
        <source>Previous</source>
        <translation>Zurück</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="173"/>
        <source>Next</source>
        <translation>Weiter</translation>
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
        <translation>Wählen Sie die Datei die Sie benutzen möchten.</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="48"/>
        <source>Back</source>
        <translation>Zurück</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="139"/>
        <source>Home</source>
        <translation>Anfang</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="214"/>
        <source>See logs from your archive runs.</source>
        <translation>Logfiles der Archivläufe ansehen.</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="267"/>
        <source>Update</source>
        <translation>Aktualisieren</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="117"/>
        <source>Exit</source>
        <translation>Beenden</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="266"/>
        <source>Up Level</source>
        <translation>Eine Ebene hoch</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="131"/>
        <source>Find</source>
        <translation>Auswählen</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="12"/>
        <source>0.00 GB</source>
        <translation>0.00 GB</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="273"/>
        <source>Video Category:</source>
        <translation>Videokategorie:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="181"/>
        <source>Ok</source>
        <translation>Ok</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="16"/>
        <source>12.34 GB</source>
        <translation>12.34 GB</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="77"/>
        <source>Choose the appearance of your DVD.</source>
        <translation>Wählen Sie das Aussehen Ihrer DVD.</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="260"/>
        <source>Theme:</source>
        <translation>Theme:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="157"/>
        <source>Main Menu</source>
        <translation>Hauptmenü</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="73"/>
        <source>Chapter Menu</source>
        <translation>Kapitelmenü</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="102"/>
        <source>Details</source>
        <translation>Details</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="232"/>
        <source>Select the recordings and videos you wish to save.</source>
        <translation>Wählen Sie die Aufnahmen die Sie speichern möchten.</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="25"/>
        <source>Add Recording</source>
        <translation>Aufnahme hinzufügen</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="27"/>
        <source>Add Video</source>
        <translation>Video hinzufügen</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="24"/>
        <source>Add File</source>
        <translation>Datei hinzufügen</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="37"/>
        <source>Archive Item Details</source>
        <translation>Archivobjekteigenschaften</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="263"/>
        <source>Title:</source>
        <translation>Titel:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="252"/>
        <source>Subtitle:</source>
        <translation>Untertitel:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="247"/>
        <source>Start Date:</source>
        <translation>Startzeit:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="262"/>
        <source>Time:</source>
        <translation>Zeit:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="97"/>
        <source>Description:</source>
        <translation>Beschreibung:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="205"/>
        <source>Save</source>
        <translation>Speichern</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="129"/>
        <source>Filesize: %1</source>
        <translation>Dateigrösse: %1</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="202"/>
        <source>Recorded Time: %1</source>
        <translation>Aufnahmezeit: %1</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="7"/>
        <source>%&quot;|SUBTITLE|&quot; %%(|ORIGINALAIRDATE|) %%(|STARS|) %%DESCRIPTION%</source>
        <translation>%&quot;|SUBTITLE|&quot; %%(|ORIGINALAIRDATE|) %%(|STARS|) %%DESCRIPTION%</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="190"/>
        <source>Parental Level: %1</source>
        <translation>Altersfreigabe Stufe: %1</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="39"/>
        <source>Archive Items</source>
        <translation>Einträge archivieren</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="200"/>
        <source>Profile:</source>
        <translation>Profile:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="182"/>
        <source>Old Size:</source>
        <translation>Alte Größe:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="170"/>
        <source>New Size:</source>
        <translation>Neue Größe:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="291"/>
        <source>xxxxx MB</source>
        <translation>xxxxx MB</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="10"/>
        <source>0 MB</source>
        <translation>0 MB</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="197"/>
        <source>Prev</source>
        <translation>Zurück</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="289"/>
        <source>x.xx GB</source>
        <translation>x.xx Gb</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="248"/>
        <source>Start Time:</source>
        <translation>Startzeit:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="219"/>
        <source>Select Associated Channel</source>
        <translation>Zugeordneten Sender auswählen</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="45"/>
        <source>Archived Channel</source>
        <translation>Archivierter Sender</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="60"/>
        <source>Chan. ID:</source>
        <translation>Sender ID:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="62"/>
        <source>Chan. No:</source>
        <translation>Sender Nr:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="56"/>
        <source>Callsign:</source>
        <translation>Kurzname:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="167"/>
        <source>Name:</source>
        <translation>Name:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="151"/>
        <source>Local Channel</source>
        <translation>Lokaler Sender</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="21"/>
        <source>A high quality profile giving approx. 1 hour of video on a single layer DVD</source>
        <translation>Ein Profil mit einer Spieldauer von etwa einer Stunde bei einer Single-Layer-DVD</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="23"/>
        <source>A standard play profile giving approx. 2 hour of video on a single layer DVD</source>
        <translation>Ein Profil mit einer Spieldauer von etwa zwei Stunden bei einer Single-Layer-DVD</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="22"/>
        <source>A long play profile giving approx. 4 hour of video on a single layer DVD</source>
        <translation>Ein Profil mit einer verlängerten Spieldauer von etwa vier Stunden bei einer Single-Layer-DVD</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="20"/>
        <source>A extended play profile giving approx. 6 hour of video on a single layer DVD</source>
        <translation>Ein Profil mit einer verlängerten Spieldauer von etwa sechs Stunden bei einer Single-Layer-DVD</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="280"/>
        <source>description goes here.</source>
        <translation>hier die Beschreibung eingeben.</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="227"/>
        <source>Select Videos</source>
        <translation>Videos auswählen</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="287"/>
        <source>title goes here</source>
        <translation>hier den Titel eintragen</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="154"/>
        <source>Log Viewer</source>
        <translation>Protokoll anzeigen</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="64"/>
        <source>Change Encoding Profile</source>
        <translation>Kodierprofil wechseln</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="17"/>
        <source>12.34GB</source>
        <translation>12,34 GB</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="175"/>
        <source>No files are selected for archive</source>
        <translation>Keine Dateien für das Archiv ausgewählt</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="261"/>
        <source>Thumb Image Selector</source>
        <translation>Vorschaubild wählen</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="86"/>
        <source>Current Position</source>
        <translation>Aktuelle Position</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="14"/>
        <source>0:00:00.00</source>
        <translation>0:00:00.00</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="215"/>
        <source>Seek Amount</source>
        <translation>Sprunglänge</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="137"/>
        <source>Frame</source>
        <translation>Frame</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="286"/>
        <source>sep 13, 2004 11:00 pm (1h 15m)</source>
        <translation>13.09.2004 23:00 (01:15 Std)</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="124"/>
        <source>File Finder To Import</source>
        <translation>Datei für den Import finden</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="211"/>
        <source>Search Channel</source>
        <translation>Kanal suchen</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="208"/>
        <source>Search Callsign</source>
        <translation>Kurzname wählen</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="213"/>
        <source>Search Name</source>
        <translation>Name suchen</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="133"/>
        <source>Finish</source>
        <translation>Fertigstellen</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="222"/>
        <source>Select Destination:</source>
        <translation>Ziel auswählen:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="192"/>
        <source>Parental level: %1</source>
        <translation>Altersfreigabe Stufe: %1</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="183"/>
        <source>Old size:</source>
        <translation>Alte Größe:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="171"/>
        <source>New size:</source>
        <translation>Neue Größe:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="230"/>
        <source>Select a theme:</source>
        <translation>Thema auswählen:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="164"/>
        <source>Menu</source>
        <translation>Menü</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="72"/>
        <source>Chapter</source>
        <translation>Kapitel</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="101"/>
        <source>Detail</source>
        <translation>Detail</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="223"/>
        <source>Select File to Import</source>
        <translation>Datei für Import auswählen</translation>
    </message>
    <message>
        <source>Search</source>
        <translation type="obsolete">Suche</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="31"/>
        <source>Add video</source>
        <translation>Video hinzufügen</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="66"/>
        <source>Channel ID:</source>
        <translation>Sender ID:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="70"/>
        <source>Channel Number:</source>
        <translation>Sender Nummer:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="81"/>
        <source>Create DVD</source>
        <translation>DVD erzeugen</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="80"/>
        <source>Create Archive</source>
        <translation>Archiv erzeugen</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="109"/>
        <source>Encode Video File</source>
        <translation>Videodatei kodieren</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="141"/>
        <source>Import Archive</source>
        <translation>Archiv importieren</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="44"/>
        <source>Archive Utilities</source>
        <translation>Zubehör</translation>
    </message>
    <message>
        <source>Show Log Viewer</source>
        <translation type="vanished">Protokoll anzeigen</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="193"/>
        <source>Play Created DVD</source>
        <translation>Erzeugte DVD abspielen</translation>
    </message>
    <message>
        <source>Burn DVD</source>
        <translation type="vanished">DVD brennen</translation>
    </message>
</context>
<context>
    <name>ThumbFinder</name>
    <message>
        <source>Save</source>
        <translation type="obsolete">Speichern</translation>
    </message>
    <message>
        <source>Cancel</source>
        <translation type="obsolete">Abbrechen</translation>
    </message>
    <message>
        <location filename="../mytharchive/thumbfinder.cpp" line="858"/>
        <source>Exit, Save Thumbnails</source>
        <translation>Beenden, Miniaturbilder speichern</translation>
    </message>
    <message>
        <location filename="../mytharchive/thumbfinder.cpp" line="859"/>
        <source>Exit, Don&apos;t Save Thumbnails</source>
        <translation>Beenden, Miniaturbilder nicht speichern</translation>
    </message>
    <message>
        <location filename="../mytharchive/thumbfinder.cpp" line="851"/>
        <source>Menu</source>
        <translation>Menü</translation>
    </message>
</context>
<context>
    <name>VideoSelector</name>
    <message>
        <location filename="../mytharchive/videoselector.cpp" line="148"/>
        <source>Clear All</source>
        <translation>Auswahl löschen</translation>
    </message>
    <message>
        <location filename="../mytharchive/videoselector.cpp" line="149"/>
        <source>Select All</source>
        <translation>Alles auswählen</translation>
    </message>
    <message>
        <source>Cancel</source>
        <translation type="obsolete">Abbrechen</translation>
    </message>
    <message>
        <location filename="../mytharchive/videoselector.cpp" line="549"/>
        <source>You need to enter a valid password for this parental level</source>
        <translation>Sie müssen ein gültiges Passwort für diese Stufe der Altersfreigabe angeben</translation>
    </message>
    <message>
        <source>OK</source>
        <translation type="obsolete">Ok</translation>
    </message>
    <message>
        <location filename="../mytharchive/videoselector.cpp" line="331"/>
        <location filename="../mytharchive/videoselector.cpp" line="497"/>
        <source>All Videos</source>
        <translation>Alle Videos</translation>
    </message>
    <message>
        <source>Parental Pin:</source>
        <translation type="obsolete">Kindersicherungs-PIN:</translation>
    </message>
    <message>
        <location filename="../mytharchive/videoselector.cpp" line="141"/>
        <source>Menu</source>
        <translation>Menü</translation>
    </message>
</context>
</TS>
