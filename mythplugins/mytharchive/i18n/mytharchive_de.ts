<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE TS>
<TS version="2.0" language="de_DE">
<context>
    <name>(ArchiveUtils)</name>
    <message>
        <location filename="../mytharchive/archiveutil.cpp" line="80"/>
        <source>Cannot find the MythArchive work directory.
Have you set the correct path in the settings?</source>
        <translation>Konnte das MythArchive-Arbeitsverzeichnis nicht finden.
Stimmt der Pfad in den Einstellungen?</translation>
    </message>
</context>
<context>
    <name>(MythArchiveMain)</name>
    <message>
        <location filename="../mytharchive/main.cpp" line="93"/>
        <source>Found a lock file but the owning process isn&apos;t running!
Removing stale lock file.</source>
        <translation>Eine Lock-Datei existiert aber der dazugehörige Prozess läuft nicht!
Lösche Lock-Datei.</translation>
    </message>
    <message>
        <location filename="../mytharchive/main.cpp" line="212"/>
        <source>Last run did not create a playable DVD.</source>
        <translation>Der letzte Durchgang hat keine spielbare DVD erzeugt.</translation>
    </message>
    <message>
        <location filename="../mytharchive/main.cpp" line="219"/>
        <source>Last run failed to create a DVD.</source>
        <translation>Der letzte Durchgang hat keine DVD erzeugt.</translation>
    </message>
</context>
<context>
    <name>ArchiveFileSelector</name>
    <message>
        <location filename="../mytharchive/importnative.cpp" line="212"/>
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
        <location filename="../mytharchive/importnative.cpp" line="272"/>
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
        <location filename="../mytharchive/archivesettings.cpp" line="33"/>
        <source>MythArchive Share Directory</source>
        <translation>MythArchive gemeinsames Verzeichnis</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="36"/>
        <source>Location where MythArchive stores its scripts, intro movies and theme files</source>
        <translation>Pfad in welchem MythArchive seine Skripte, Intros und Themendateien speichert</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="46"/>
        <source>Video format</source>
        <translation>Videoformat</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="51"/>
        <source>Video format for DVD recordings, PAL or NTSC.</source>
        <translation>Videoformat für DVD-Aufnahmen, PAL oder NTSC.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="60"/>
        <source>File Selector Filter</source>
        <translation>Dateiauswahlfilter</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="63"/>
        <source>The file name filter to use in the file selector.</source>
        <translation>Der Filter der im Dateiauswahlmenü benutzt wird.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="72"/>
        <source>Location of DVD</source>
        <translation>DVD-Gerät</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="75"/>
        <source>Which DVD drive to use when burning discs.</source>
        <translation>Welches DVD-Laufwerk zum Brennen benutzt werden soll.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="84"/>
        <source>DVD Drive Write Speed</source>
        <translation>DVD-Schreibgeschwindigkeit</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="87"/>
        <source>This is the write speed to use when burning a DVD. Set to 0 to allow growisofs to choose the fastest available speed.</source>
        <translation>Die Schreibgeschwindigkeit beim Brennen von DVDs. Bei 0 wird &quot;growisofs&quot; automatisch die höchste Geschwindigkeit wählen.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="98"/>
        <source>Command to play DVD</source>
        <translation>DVD-Abspielbefehl</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="101"/>
        <source>Command to run when test playing a created DVD. &apos;Internal&apos; will use the internal MythTV player. %f will be replaced with the path to the created DVD structure eg. &apos;xine -pfhq --no-splash dvd:/%f&apos;.</source>
        <translation>Befehl zum testweisen Abspielen einer neuen DVD. Bei &quot;Internal&quot; wird der MythTV-Player verwendet. &quot;%f&quot; wird durch den Pfad der erzeugten DVD-Struktur ersetzt, z.B. &quot;xine -pfhq --no-splash dvd:/%f&quot;.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="114"/>
        <source>Copy remote files</source>
        <translation>Kopiere entfernte Dateien</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="117"/>
        <source>If set files on remote filesystems will be copied over to the local filesystem before processing. Speeds processing and reduces bandwidth on the network</source>
        <translation>Falls gesetzt, werden Dateien auf entfernten Dateisystemen vor der Verarbeitung auf das lokale Dateisystem kopiert. Dies beschleunigt die Verarbeitung und reduziert die Auslastung des Netzwerkes</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="129"/>
        <source>Always Use Mythtranscode</source>
        <translation>Immer Mythtranscode benutzen</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="132"/>
        <source>If set mpeg2 files will always be passed though mythtranscode to clean up any errors. May help to fix some audio problems. Ignored if &apos;Use ProjectX&apos; is set.</source>
        <translation>Falls gesetzt, werden MPEG2-Dateien immer durch mythtranscode gefiltert um etwaige Fehler zu beheben. Dies könnte manche Tonprobleme beheben. Diese Option wird ignoriert falls &quot;Nutze ProjectX&quot; aktiv ist.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="144"/>
        <source>Use ProjectX</source>
        <translation>Nutze ProjectX</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="147"/>
        <source>If set ProjectX will be used to cut commercials and split mpeg2 files instead of mythtranscode and mythreplex.</source>
        <translation>Falls gesetzt, wird ProjectX statt mythtranscode und mythreplex verwendet um Werbung heraus zu schneiden und MPEG2-Dateien aufzuteilen.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="158"/>
        <source>Use FIFOs</source>
        <translation>FIFOs benutzen</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="161"/>
        <source>The script will use FIFOs to pass the output of mplex into dvdauthor rather than creating intermediate files. Saves time and disk space during multiplex operations but not supported on Windows platform</source>
        <translation>Das Skript wird FIFOs an Stelle von temporären Dateien verwenden um die Ausgabe von mplex an dvdauthor weiter zu leiten. Dies spart Zeit und Plattenplatz während Multiplex-Vorgängen, wird unter Windows aber nicht unterstützt</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="174"/>
        <source>Add Subtitles</source>
        <translation>Untertitel hinzufügen</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="177"/>
        <source>If available this option will add subtitles to the final DVD. Requires &apos;Use ProjectX&apos; to be on.</source>
        <translation>Falls verfügbar, werden der endgültigen DVD Untertitel hinzugefügt. Dazu muss &quot;Nutze ProjectX&quot; aktiviert sein.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="187"/>
        <source>Main Menu Aspect Ratio</source>
        <translation>Hauptmenü Seitenverhältnis</translation>
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
        <translation>Welches Seitenverhältnis beim Erstellen des Hauptmenüs benutzt werden soll.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="203"/>
        <source>Chapter Menu Aspect Ratio</source>
        <translation>Kapitelmenü Seitenverhältnis</translation>
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
        <translation>Welches Seitenverhältnis beim Erstellen des Kapitelmenüs benutzt werden soll. Bei &quot;%1&quot; wird das Seitenverhältnis des entsprechenden Videos verwendet.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="223"/>
        <source>Date format</source>
        <translation>Datumsformat</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="226"/>
        <source>Samples are shown using today&apos;s date.</source>
        <translation>Die Beispiele gelten für das heutige Datum.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="232"/>
        <source>Samples are shown using tomorrow&apos;s date.</source>
        <translation>Die Beispiele gelten für Morgen.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="250"/>
        <source>Your preferred date format to use on DVD menus. %1</source>
        <extracomment>%1 gives additional info on the date used</extracomment>
        <translation>Ihr bevorzugtes Zeitformat für DVD-Menüs. %1</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="259"/>
        <source>Time format</source>
        <translation>Zeitformat</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="266"/>
        <source>Your preferred time format to display on DVD menus. You must choose a format with &quot;AM&quot; or &quot;PM&quot; in it, otherwise your time display will be 24-hour or &quot;military&quot; time.</source>
        <translation>Ihr bevorzugtes Zeitformat für DVD-Menüs. Einträge mit &quot;am&quot; bzw &quot;pm&quot; zeigen die Zeit mit 12 Stunden Teilung an.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="277"/>
        <source>Default Encoder Profile</source>
        <translation>Standard Enkoderprofil</translation>
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
        <translation>E</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="286"/>
        <source>Default encoding profile to use if a file needs re-encoding.</source>
        <translation>Das Standard Enkoderprofil für Dateien neu enkodiert werden müssen.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="295"/>
        <source>mplex Command</source>
        <translation>Befehl für mplex</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="299"/>
        <source>Command to run mplex</source>
        <translation>Der Befehl zum Ausführen von mplex</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="308"/>
        <source>dvdauthor command</source>
        <translation>Befehl für dvdauthor</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="312"/>
        <source>Command to run dvdauthor.</source>
        <translation>Der Befehl zum Ausführen von dvdauthor.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="321"/>
        <source>mkisofs command</source>
        <translation>Befehl für mkisofs</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="325"/>
        <source>Command to run mkisofs. (Used to create ISO images)</source>
        <translation>Der Befehl zum Ausführen von mkisofs (wird benutzt um ISO-Abbilder zu erstellen)</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="334"/>
        <source>growisofs command</source>
        <translation>Befehl für growisofs</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="338"/>
        <source>Command to run growisofs. (Used to burn DVDs)</source>
        <translation>Der Befehl zum Ausführen von growisofs (wird benutzt um DVDs zu brennen)</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="347"/>
        <source>M2VRequantiser command</source>
        <translation>M2V-Grössenwandler-Kommando</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="351"/>
        <source>Command to run M2VRequantiser. Optional - leave blank if you don&apos;t have M2VRequantiser installed.</source>
        <translation>Kommando zum Ausführen des M2V-Grössenwandler. Optional - Leer lassen wenn der M2V-Grössenwandler nicht installiert ist.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="361"/>
        <source>jpeg2yuv command</source>
        <translation>Befehl für jpeg2yuv</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="365"/>
        <source>Command to run jpeg2yuv. Part of mjpegtools package</source>
        <translation>Befehl um jpeg2yuv zu starten. Dies ist Teil des mjpegtools Pakets</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="374"/>
        <source>spumux command</source>
        <translation>Befehl für spumux</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="378"/>
        <source>Command to run spumux. Part of dvdauthor package</source>
        <translation>Der Befehl zum Ausführen von spumux. Teil des dvdauthor Paketes</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="387"/>
        <source>mpeg2enc command</source>
        <translation>Befehl für mpeg2enc</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="391"/>
        <source>Command to run mpeg2enc. Part of mjpegtools package</source>
        <translation>Der Befehl zum Ausführen von mpeg2enc. Teil des mjpegtool Paketes</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="400"/>
        <source>projectx command</source>
        <translation>Befehl für projectx</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="404"/>
        <source>Command to run ProjectX. Will be used to cut commercials and split mpegs files instead of mythtranscode and mythreplex.</source>
        <translation>Befehl um ProjectX zu starten. Wird statt mythtranscode und mythreplex verwendet um Werbung heraus zu schneiden und MPEG-Dateien aufzuteilen.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="414"/>
        <source>MythArchive Settings</source>
        <translation>MythArchive Einstellungen</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="425"/>
        <source>MythArchive Settings (2)</source>
        <translation>MythArchive  Einstellungen (2)</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="435"/>
        <source>DVD Menu Settings</source>
        <translation>DVD-Menü Einstellungen</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="443"/>
        <source>MythArchive External Commands (1)</source>
        <translation>MythArchive externe Kommandos (1)</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="451"/>
        <source>MythArchive External Commands (2)</source>
        <translation>MythArchive externe Kommandos (2)</translation>
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
        <location filename="../mytharchive/themeselector.cpp" line="219"/>
        <location filename="../mytharchive/themeselector.cpp" line="230"/>
        <source>No theme description file found!</source>
        <translation>Keine Themebeschreibung gefunden!</translation>
    </message>
    <message>
        <location filename="../mytharchive/themeselector.cpp" line="243"/>
        <source>Empty theme description!</source>
        <translation>Leere Themebeschreibung!</translation>
    </message>
    <message>
        <location filename="../mytharchive/themeselector.cpp" line="248"/>
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
        <location filename="../mytharchive/exportnative.cpp" line="242"/>
        <source>You need to add at least one item to archive!</source>
        <translation>Sie müssen mindestens einen Eintrag zum Archivieren hinzufügen!</translation>
    </message>
    <message>
        <location filename="../mytharchive/exportnative.cpp" line="393"/>
        <source>Menu</source>
        <translation>Menü</translation>
    </message>
    <message>
        <location filename="../mytharchive/exportnative.cpp" line="400"/>
        <source>Remove Item</source>
        <translation>Eintrag entfernen</translation>
    </message>
    <message>
        <location filename="../mytharchive/exportnative.cpp" line="495"/>
        <source>It was not possible to create the DVD. An error occured when running the scripts</source>
        <translation>Die Erstellung der DVD war nicht möglich. Während des Ausführens der Skripte ist ein Fehler aufgetreten</translation>
    </message>
    <message>
        <location filename="../mytharchive/exportnative.cpp" line="531"/>
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
        <location filename="../mytharchive/fileselector.cpp" line="321"/>
        <source>The selected item is not a directory!</source>
        <translation>Der gewählte Eintrag ist kein Verzeichnis!</translation>
    </message>
    <message>
        <location filename="../mytharchive/fileselector.cpp" line="81"/>
        <source>Find File</source>
        <translation>Datei finden</translation>
    </message>
    <message>
        <location filename="../mytharchive/fileselector.cpp" line="84"/>
        <source>Find Directory</source>
        <translation>Verzeichnis finden</translation>
    </message>
    <message>
        <location filename="../mytharchive/fileselector.cpp" line="87"/>
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
        <location filename="../mytharchive/importnative.cpp" line="433"/>
        <source>You need to select a valid channel id!</source>
        <translation>Sie müssen eine gültige Kanal ID auswählen!</translation>
    </message>
    <message>
        <location filename="../mytharchive/importnative.cpp" line="464"/>
        <source>It was not possible to import the Archive.  An error occured when running &apos;mytharchivehelper&apos;</source>
        <translation>Es war nicht möglich das Archiv zu importieren. Während der Ausführung von &quot;mytharchivehelper&quot; ist ein Fehler aufgetreten</translation>
    </message>
    <message>
        <location filename="../mytharchive/importnative.cpp" line="594"/>
        <source>Select a channel id</source>
        <translation>Sender ID wählen</translation>
    </message>
    <message>
        <location filename="../mytharchive/importnative.cpp" line="620"/>
        <source>Select a channel number</source>
        <translation>Sendernummer wählen</translation>
    </message>
    <message>
        <location filename="../mytharchive/importnative.cpp" line="646"/>
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
        <location filename="../mytharchive/importnative.cpp" line="672"/>
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
        <location filename="../mytharchive/logviewer.cpp" line="77"/>
        <source>Cannot find any logs to show!</source>
        <translation>Keine Logdateien zum anzeigen gefunden!</translation>
    </message>
    <message>
        <location filename="../mytharchive/logviewer.cpp" line="216"/>
        <source>Background creation has been asked to stop.
This may take a few minutes.</source>
        <translation>Die Hintergrundverarbeitung wurde zum Anhalten angewiesen.
Dies kann einige Minuten in Anspruch nehmen.</translation>
    </message>
    <message>
        <location filename="../mytharchive/logviewer.cpp" line="355"/>
        <source>Menu</source>
        <translation>Menü</translation>
    </message>
    <message>
        <location filename="../mytharchive/logviewer.cpp" line="363"/>
        <source>Don&apos;t Auto Update</source>
        <translation>Nicht autom. aktualisieren</translation>
    </message>
    <message>
        <location filename="../mytharchive/logviewer.cpp" line="365"/>
        <source>Auto Update</source>
        <translation>Autom. aktualisieren</translation>
    </message>
    <message>
        <location filename="../mytharchive/logviewer.cpp" line="367"/>
        <source>Show Progress Log</source>
        <translation>Fortschritt anzeigen</translation>
    </message>
    <message>
        <location filename="../mytharchive/logviewer.cpp" line="368"/>
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
        <location filename="../mytharchive/mythburn.cpp" line="345"/>
        <location filename="../mytharchive/mythburn.cpp" line="469"/>
        <source>Using Cut List</source>
        <translation>Verwende Schnittliste</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="350"/>
        <location filename="../mytharchive/mythburn.cpp" line="474"/>
        <source>Not Using Cut List</source>
        <translation>Verwende keine Schnittliste</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="356"/>
        <location filename="../mytharchive/mythburn.cpp" line="480"/>
        <source>No Cut List</source>
        <translation>Keine Schnittliste</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="367"/>
        <source>You need to add at least one item to archive!</source>
        <translation>Sie müssen mindestens einen Eintrag zum Archivieren hinzufügen!</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="413"/>
        <source>Retrieving File Information. Please Wait...</source>
        <translation>Hole Dateiinformationen. Bitte warten...</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="483"/>
        <source>Encoder: </source>
        <translation>Kodierer: </translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="809"/>
        <source>Menu</source>
        <translation>Menü</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="820"/>
        <source>Don&apos;t Use Cut List</source>
        <translation>Keine Schnittliste verwenden</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="823"/>
        <source>Use Cut List</source>
        <translation>Schnittliste verwenden</translation>
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
        <location filename="../mytharchive/mythburn.cpp" line="827"/>
        <source>Remove Item</source>
        <translation>Eintrag entfernen</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="828"/>
        <source>Edit Details</source>
        <translation>Details bearbeiten</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="829"/>
        <source>Change Encoding Profile</source>
        <translation>Kodierprofil wechseln</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="830"/>
        <source>Edit Thumbnails</source>
        <translation>Miniaturbilder bearbeiten</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="965"/>
        <source>It was not possible to create the DVD.  An error occured when running the scripts</source>
        <translation>Die Erstellung der DVD war nicht möglich. Während des Ausführens der Skripte ist ein Fehler aufgetreten</translation>
    </message>
</context>
<context>
    <name>MythControls</name>
    <message>
        <location filename="../mytharchive/main.cpp" line="302"/>
        <source>Toggle use cut list state for selected program</source>
        <translation>Schnittliste für die gewählte Sendung ein- ausschalten</translation>
    </message>
    <message>
        <location filename="../mytharchive/main.cpp" line="305"/>
        <source>Create DVD</source>
        <translation>DVD erzeugen</translation>
    </message>
    <message>
        <location filename="../mytharchive/main.cpp" line="307"/>
        <source>Create Archive</source>
        <translation>Archiv erzeugen</translation>
    </message>
    <message>
        <location filename="../mytharchive/main.cpp" line="309"/>
        <source>Import Archive</source>
        <translation>Archiv importieren</translation>
    </message>
    <message>
        <location filename="../mytharchive/main.cpp" line="311"/>
        <source>View Archive Log</source>
        <translation>Archiv-Protokoll ansehen</translation>
    </message>
    <message>
        <location filename="../mytharchive/main.cpp" line="313"/>
        <source>Play Created DVD</source>
        <translation>Erzeugte DVD abspielen</translation>
    </message>
    <message>
        <location filename="../mytharchive/main.cpp" line="315"/>
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
        <location filename="../mytharchive/mythburn.cpp" line="1008"/>
        <source>You don&apos;t have any videos!</source>
        <translation>Es existieren keine Videos!</translation>
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
        <location filename="../mytharchive/mythburn.cpp" line="1155"/>
        <source>Cannot burn a DVD.
The last run failed to create a DVD.</source>
        <translation>Kann keine DVD brennen.
Der letzte Durchgang hat keine DVD erzeugt.</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="1162"/>
        <source>
Place a blank DVD in the drive and select an option below.</source>
        <translation>
Legen Sie eine leere DVD ins Laufwerk und wählen Sie eine Option.</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="1161"/>
        <location filename="../mytharchive/mythburn.cpp" line="1173"/>
        <source>Burn DVD</source>
        <translation>DVD brennen</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="1174"/>
        <source>Burn DVD Rewritable</source>
        <translation>DVD-RW brennen</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="1175"/>
        <source>Burn DVD Rewritable (Force Erase)</source>
        <translation>DVD-RW brennen (löschen forcieren)</translation>
    </message>
    <message>
        <source>Cancel</source>
        <translation type="obsolete">Abbrechen</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="1231"/>
        <source>It was not possible to run mytharchivehelper to burn the DVD.</source>
        <translation>Konnte mytharchivehelper nicht starten um die DVD zu brennen.</translation>
    </message>
</context>
<context>
    <name>RecordingSelector</name>
    <message>
        <location filename="../mytharchive/recordingselector.cpp" line="133"/>
        <source>Retrieving Recording List.
Please Wait...</source>
        <translation>Hole Aufnahmeliste.
Bitte warten...</translation>
    </message>
    <message>
        <location filename="../mytharchive/recordingselector.cpp" line="213"/>
        <source>Clear All</source>
        <translation>Auswahl löschen</translation>
    </message>
    <message>
        <location filename="../mytharchive/recordingselector.cpp" line="214"/>
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
        <location filename="../mytharchive/recordingselector.cpp" line="112"/>
        <location filename="../mytharchive/recordingselector.cpp" line="420"/>
        <location filename="../mytharchive/recordingselector.cpp" line="525"/>
        <source>All Recordings</source>
        <translation>Alle Aufnahmen</translation>
    </message>
    <message>
        <source>Retrieving Recording List. Please Wait...</source>
        <translation type="obsolete">Hole Aufnahmenliste. Bitte warten...</translation>
    </message>
    <message>
        <location filename="../mytharchive/recordingselector.cpp" line="157"/>
        <source>Either you don&apos;t have any recordings or no recordings are available locally!</source>
        <translation>Entweder es existieren keine Aufnahmen oder sie sind nicht lokal verfügbar!</translation>
    </message>
    <message>
        <location filename="../mytharchive/recordingselector.cpp" line="206"/>
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
        <location filename="../mytharchive/archiveutil.cpp" line="35"/>
        <source>Single Layer DVD</source>
        <translation>Single Layer DVD</translation>
    </message>
    <message>
        <source>Single Layer DVD (4482Mb)</source>
        <translation type="obsolete">Single Layer DVD (4482 Mb)</translation>
    </message>
    <message>
        <location filename="../mytharchive/archiveutil.cpp" line="39"/>
        <source>Dual Layer DVD</source>
        <translation>Dual Layer DVD</translation>
    </message>
    <message>
        <source>Dual Layer DVD (8964Mb)</source>
        <translation type="obsolete">Dual Layer DVD (8964 Mb)</translation>
    </message>
    <message>
        <location filename="../mytharchive/archiveutil.cpp" line="36"/>
        <source>Single Layer DVD (4,482 MB)</source>
        <translation>Single Layer DVD (4.482 MB)</translation>
    </message>
    <message>
        <location filename="../mytharchive/archiveutil.cpp" line="40"/>
        <source>Dual Layer DVD (8,964 MB)</source>
        <translation>Dual Layer DVD (8.964 MB)</translation>
    </message>
    <message>
        <location filename="../mytharchive/archiveutil.cpp" line="43"/>
        <source>DVD +/- RW</source>
        <translation>DVD +/- RW</translation>
    </message>
    <message>
        <location filename="../mytharchive/archiveutil.cpp" line="44"/>
        <source>Rewritable DVD</source>
        <translation>Wiederbeschreibbare DVD</translation>
    </message>
    <message>
        <location filename="../mytharchive/archiveutil.cpp" line="47"/>
        <source>File</source>
        <translation>Datei</translation>
    </message>
    <message>
        <location filename="../mytharchive/archiveutil.cpp" line="48"/>
        <source>Any file accessable from your filesystem.</source>
        <translation>Jede verfügbare Datei auf Ihrem Dateisystem.</translation>
    </message>
    <message>
        <location filename="../mytharchive/selectdestination.cpp" line="294"/>
        <location filename="../mytharchive/selectdestination.cpp" line="350"/>
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
        <location filename="themestrings.h" line="18"/>
        <source>Select Recordings</source>
        <translation>Aufnahmen auswählen</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="19"/>
        <source>Show Recordings</source>
        <translation>Aufnahmen ansehen</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="93"/>
        <source>x.xx Gb</source>
        <translation>x.xx Gb</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="21"/>
        <source>File Finder</source>
        <translation>Dateienfinder</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="25"/>
        <source>Video Category</source>
        <translation>Videokategorie</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="26"/>
        <source>PL:</source>
        <translation>PL:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="88"/>
        <source>1</source>
        <translation>1</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="27"/>
        <source>No videos available</source>
        <translation>Keine Videos vorhanden</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="9"/>
        <source>Select Destination</source>
        <translation>Ziel auswählen</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="11"/>
        <source>Free Space:</source>
        <translation>Freier Platz:</translation>
    </message>
    <message>
        <source>0.00Gb</source>
        <translation type="obsolete">0.00 Gb</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="12"/>
        <source>Make ISO Image</source>
        <translation>ISO-Abbild erstellen</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="13"/>
        <source>Burn to DVD</source>
        <translation>Auf DVD schreiben</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="14"/>
        <source>Force Overwrite of DVD-RW Media</source>
        <translation>Überschreiben des DVD-RW-Mediums erzwingen</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="38"/>
        <source>Select Archive Items</source>
        <translation>Archivobjekte auswählen</translation>
    </message>
    <message>
        <source>xxxxx mb</source>
        <translation type="obsolete">xxxxx Mb</translation>
    </message>
    <message>
        <source>0 mb</source>
        <translation type="obsolete">0 Mb</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="32"/>
        <source>DVD Menu Theme</source>
        <translation>DVD-Menüthema</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="33"/>
        <source>Select a theme</source>
        <translation>Thema auswählen</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="34"/>
        <source>Intro</source>
        <translation>Einleitung</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="68"/>
        <source>%DATE%, %TIME%</source>
        <translation>%DATE%, %TIME%</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="69"/>
        <source>Choose where you would like your files archived.</source>
        <translation>Wählen Sie wohin Ihre Dateien archiviert werden sollen.</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="70"/>
        <source>Output Type:</source>
        <translation>Ausgabetyp:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="71"/>
        <source>Destination:</source>
        <translation>Ziel:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="72"/>
        <source>Click here to find an output location...</source>
        <translation>Klicken Sie hier um einen Ausgabeort auszuwählen...</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="73"/>
        <source>Erase DVD-RW before burning</source>
        <translation>DVD-RW vor dem brennen löschen</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="15"/>
        <source>Cancel</source>
        <translation>Abbrechen</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="74"/>
        <source>Previous</source>
        <translation>Zurück</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="17"/>
        <source>Next</source>
        <translation>Weiter</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="75"/>
        <source>Filter:</source>
        <translation>Filter:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="20"/>
        <source>OK</source>
        <translation>OK</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="76"/>
        <source>Select the file you wish to use.</source>
        <translation>Wählen Sie die Datei die Sie benutzen möchten.</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="22"/>
        <source>Back</source>
        <translation>Zurück</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="23"/>
        <source>Home</source>
        <translation>Anfang</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="77"/>
        <source>See logs from your archive runs.</source>
        <translation>Logfiles der Archivläufe ansehen.</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="29"/>
        <source>Update</source>
        <translation>Aktualisieren</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="30"/>
        <source>Exit</source>
        <translation>Beenden</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="83"/>
        <source>Up Level</source>
        <translation>Eine Ebene hoch</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="10"/>
        <source>Find</source>
        <translation>Auswählen</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="85"/>
        <source>0.00 GB</source>
        <translation>0.00 GB</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="86"/>
        <source>Video Category:</source>
        <translation>Videokategorie:</translation>
    </message>
    <message>
        <source>Ok</source>
        <translation type="obsolete">Ok</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="78"/>
        <source>12.34 GB</source>
        <translation>12.34 GB</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="79"/>
        <source>Choose the appearance of your DVD.</source>
        <translation>Wählen Sie das Aussehen Ihrer DVD.</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="80"/>
        <source>Theme:</source>
        <translation>Theme:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="35"/>
        <source>Main Menu</source>
        <translation>Hauptmenü</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="36"/>
        <source>Chapter Menu</source>
        <translation>Kapitelmenü</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="37"/>
        <source>Details</source>
        <translation>Details</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="81"/>
        <source>Select the recordings and videos you wish to save.</source>
        <translation>Wählen Sie die Aufnahmen die Sie speichern möchten.</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="40"/>
        <source>Add Recording</source>
        <translation>Aufnahme hinzufügen</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="41"/>
        <source>Add Video</source>
        <translation>Video hinzufügen</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="42"/>
        <source>Add File</source>
        <translation>Datei hinzufügen</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="43"/>
        <source>Archive Item Details</source>
        <translation>Archivobjekteigenschaften</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="44"/>
        <source>Title:</source>
        <translation>Titel:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="45"/>
        <source>Subtitle:</source>
        <translation>Untertitel:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="46"/>
        <source>Start Date:</source>
        <translation>Startzeit:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="47"/>
        <source>Time:</source>
        <translation>Zeit:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="48"/>
        <source>Description:</source>
        <translation>Beschreibung:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="53"/>
        <source>Save</source>
        <translation>Speichern</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="89"/>
        <source>xxxxx MB</source>
        <translation>xxxxx MB</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="90"/>
        <source>0 MB</source>
        <translation>0 MB</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="16"/>
        <source>Prev</source>
        <translation>Zurück</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="92"/>
        <source>x.xx GB</source>
        <translation>x.xx Gb</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="56"/>
        <source>Start Time:</source>
        <translation>Startzeit:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="57"/>
        <source>Select Associated Channel</source>
        <translation>Zugeordneten Sender auswählen</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="58"/>
        <source>Archived Channel</source>
        <translation>Archivierter Sender</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="59"/>
        <source>Chan. ID:</source>
        <translation>Sender ID:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="60"/>
        <source>Chan. No:</source>
        <translation>Sender Nr:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="61"/>
        <source>Callsign:</source>
        <translation>Kurzname:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="62"/>
        <source>Name:</source>
        <translation>Name:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="63"/>
        <source>Local Channel</source>
        <translation>Lokaler Sender</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="5"/>
        <source>A high quality profile giving approx. 1 hour of video on a single layer DVD</source>
        <translation>Ein Profil mit einer Spieldauer von etwa einer Stunde bei einer Single-Layer-DVD</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="6"/>
        <source>A standard play profile giving approx. 2 hour of video on a single layer DVD</source>
        <translation>Ein Profil mit einer Spieldauer von etwa zwei Stunden bei einer Single-Layer-DVD</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="7"/>
        <source>A long play profile giving approx. 4 hour of video on a single layer DVD</source>
        <translation>Ein Profil mit einer verlängerten Spieldauer von etwa vier Stunden bei einer Single-Layer-DVD</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="8"/>
        <source>A extended play profile giving approx. 6 hour of video on a single layer DVD</source>
        <translation>Ein Profil mit einer verlängerten Spieldauer von etwa sechs Stunden bei einer Single-Layer-DVD</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="84"/>
        <source>description goes here.</source>
        <translation>hier die Beschreibung eingeben.</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="24"/>
        <source>Select Videos</source>
        <translation>Videos auswählen</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="87"/>
        <source>title goes here</source>
        <translation>hier den Titel eintragen</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="28"/>
        <source>Log Viewer</source>
        <translation>Protokoll anzeigen</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="31"/>
        <source>Change Encoding Profile</source>
        <translation>Kodierprofil wechseln</translation>
    </message>
    <message>
        <source>12.34GB</source>
        <translation type="obsolete">12,34 GB</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="39"/>
        <source>No files are selected for archive</source>
        <translation>Keine Dateien für das Archiv ausgewählt</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="49"/>
        <source>Thumb Image Selector</source>
        <translation>Vorschaubild wählen</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="50"/>
        <source>Current Position</source>
        <translation>Aktuelle Position</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="82"/>
        <source>0:00:00.00</source>
        <translation>0:00:00.00</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="51"/>
        <source>Seek Amount</source>
        <translation>Sprunglänge</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="52"/>
        <source>Frame</source>
        <translation>Frame</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="91"/>
        <source>sep 13, 2004 11:00 pm (1h 15m)</source>
        <translation>13.09.2004 23:00 (01:15 Std)</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="55"/>
        <source>File Finder To Import</source>
        <translation>Datei für den Import finden</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="64"/>
        <source>Search Channel</source>
        <translation>Kanal suchen</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="65"/>
        <source>Search Callsign</source>
        <translation>Kurzname wählen</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="66"/>
        <source>Search Name</source>
        <translation>Name suchen</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="67"/>
        <source>Finish</source>
        <translation>Fertigstellen</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="94"/>
        <source>Select Destination:</source>
        <translation>Ziel auswählen:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="95"/>
        <source>Parental level: %1</source>
        <translation>Altersfreigabe Stufe: %1</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="96"/>
        <source>Old size:</source>
        <translation>Alte Größe:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="97"/>
        <source>New size:</source>
        <translation>Neue Größe:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="98"/>
        <source>Select a theme:</source>
        <translation>Thema auswählen:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="99"/>
        <source>Menu</source>
        <translation>Menü</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="100"/>
        <source>Chapter</source>
        <translation>Kapitel</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="101"/>
        <source>Detail</source>
        <translation>Detail</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="102"/>
        <source>Select File to Import</source>
        <translation>Datei für Import auswählen</translation>
    </message>
    <message>
        <source>Search</source>
        <translation type="obsolete">Suche</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="54"/>
        <source>Add video</source>
        <translation>Video hinzufügen</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="103"/>
        <source>Channel ID:</source>
        <translation>Sender ID:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="104"/>
        <source>Channel Number:</source>
        <translation>Sender Nummer:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="105"/>
        <source>Create DVD</source>
        <translation>DVD erzeugen</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="106"/>
        <source>Create Archive</source>
        <translation>Archiv erzeugen</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="107"/>
        <source>Encode Video File</source>
        <translation>Videodatei kodieren</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="108"/>
        <source>Import Archive</source>
        <translation>Archiv importieren</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="109"/>
        <source>Archive Utilities</source>
        <translation>Zubehör</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="110"/>
        <source>Show Log Viewer</source>
        <translation>Protokoll anzeigen</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="111"/>
        <source>Play Created DVD</source>
        <translation>Erzeugte DVD abspielen</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="112"/>
        <source>Burn DVD</source>
        <translation>DVD brennen</translation>
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
        <location filename="../mytharchive/thumbfinder.cpp" line="928"/>
        <source>Exit, Save Thumbnails</source>
        <translation>Beenden, Miniaturbilder speichern</translation>
    </message>
    <message>
        <location filename="../mytharchive/thumbfinder.cpp" line="929"/>
        <source>Exit, Don&apos;t Save Thumbnails</source>
        <translation>Beenden, Miniaturbilder nicht speichern</translation>
    </message>
    <message>
        <location filename="../mytharchive/thumbfinder.cpp" line="921"/>
        <source>Menu</source>
        <translation>Menü</translation>
    </message>
</context>
<context>
    <name>VideoSelector</name>
    <message>
        <location filename="../mytharchive/videoselector.cpp" line="160"/>
        <source>Clear All</source>
        <translation>Auswahl löschen</translation>
    </message>
    <message>
        <location filename="../mytharchive/videoselector.cpp" line="161"/>
        <source>Select All</source>
        <translation>Alles auswählen</translation>
    </message>
    <message>
        <source>Cancel</source>
        <translation type="obsolete">Abbrechen</translation>
    </message>
    <message>
        <location filename="../mytharchive/videoselector.cpp" line="546"/>
        <source>You need to enter a valid password for this parental level</source>
        <translation>Sie müssen ein gültiges Passwort für diese Stufe der Altersfreigabe angeben</translation>
    </message>
    <message>
        <source>OK</source>
        <translation type="obsolete">Ok</translation>
    </message>
    <message>
        <location filename="../mytharchive/videoselector.cpp" line="359"/>
        <location filename="../mytharchive/videoselector.cpp" line="489"/>
        <source>All Videos</source>
        <translation>Alle Videos</translation>
    </message>
    <message>
        <source>Parental Pin:</source>
        <translation type="obsolete">Kindersicherungs-PIN:</translation>
    </message>
    <message>
        <location filename="../mytharchive/videoselector.cpp" line="153"/>
        <source>Menu</source>
        <translation>Menü</translation>
    </message>
</context>
</TS>
