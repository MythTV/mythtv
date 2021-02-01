<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE TS>
<TS version="2.1" language="it_IT" sourcelanguage="en_US">
<context>
    <name>(ArchiveUtils)</name>
    <message>
        <location filename="../mytharchive/archiveutil.cpp" line="77"/>
        <source>Cannot find the MythArchive work directory.
Have you set the correct path in the settings?</source>
        <translation>Impossibile trovare la directory di lavoro di MythArchive.
È stato impostato il percorso corretto nelle impostazioni?</translation>
    </message>
</context>
<context>
    <name>(MythArchiveMain)</name>
    <message>
        <location filename="../mytharchive/main.cpp" line="94"/>
        <source>Found a lock file but the owning process isn&apos;t running!
Removing stale lock file.</source>
        <translation>Trovato un file bloccato ma il processo proprietario non è in esecuzione!
Rimuovo lo stato di blocco del file.</translation>
    </message>
    <message>
        <location filename="../mytharchive/main.cpp" line="215"/>
        <source>Last run did not create a playable DVD.</source>
        <translation>L&apos;ultima esecuzione non ha creato un DVD riproducibile.</translation>
    </message>
    <message>
        <location filename="../mytharchive/main.cpp" line="222"/>
        <source>Last run failed to create a DVD.</source>
        <translation>L&apos;ultima esecuzione ha fallito la creazione del DVD.</translation>
    </message>
</context>
<context>
    <name>ArchiveFileSelector</name>
    <message>
        <location filename="../mytharchive/importnative.cpp" line="205"/>
        <source>Find File To Import</source>
        <translation>Trova file da importare</translation>
    </message>
    <message>
        <location filename="../mytharchive/importnative.cpp" line="265"/>
        <source>The selected item is not a valid archive file!</source>
        <translation>L&apos;oggetto selezionato non è un archivio di file valido!</translation>
    </message>
</context>
<context>
    <name>ArchiveSettings</name>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="20"/>
        <source>MythArchive Temp Directory</source>
        <translation>Directory temporanea di MythArchive</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="23"/>
        <source>Location where MythArchive should create its temporary work files. LOTS of free space required here.</source>
        <translation>Percorso dove MythArchive dovrebbe creare i suoi file di lavoro temporanei. Richiede molto spazio libero su disco.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="35"/>
        <source>MythArchive Share Directory</source>
        <translation>Directory condivisa da MythArchive</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="38"/>
        <source>Location where MythArchive stores its scripts, intro movies and theme files</source>
        <translation>Percorso dove MythArchive archivia i suioi script, filmati introduttivi e file di tema</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="50"/>
        <source>Video format</source>
        <translation>Formato video</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="55"/>
        <source>Video format for DVD recordings, PAL or NTSC.</source>
        <translation>Formato video per registrazioni DVD, PAL o NTSC.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="64"/>
        <source>File Selector Filter</source>
        <translation>Filtro del selettore del file</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="67"/>
        <source>The file name filter to use in the file selector.</source>
        <translation>Il nome del file del filtro usato nel selettore dei file.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="76"/>
        <source>Location of DVD</source>
        <translation>Percorso del DVD</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="79"/>
        <source>Which DVD drive to use when burning discs.</source>
        <translation>Quale unità DVD da usare quando si masterizzano dischi.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="89"/>
        <source>DVD Drive Write Speed</source>
        <translation>Velocità di scrittura dell&apos;unità DVD</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="92"/>
        <source>This is the write speed to use when burning a DVD. Set to 0 to allow growisofs to choose the fastest available speed.</source>
        <translation>Velocità di scrittura da usare quando si masterizza un DVD. Imposta a 0 per permettere a growisofs di scegliere la velocità disponibile più veloce.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="103"/>
        <source>Command to play DVD</source>
        <translation>Comando per riprodurre DVD</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="106"/>
        <source>Command to run when test playing a created DVD. &apos;Internal&apos; will use the internal MythTV player. %f will be replaced with the path to the created DVD structure eg. &apos;xine -pfhq --no-splash dvd:/%f&apos;.</source>
        <translation>Comando da eseguire quando si testa la riproduzione di un DVD creato &quot;Internal&quot; userà il riproduttore interno MythTV. %f sarà sostituito con il percorso della strutura del DVD creato es. &apos;xine -pfhq --no-splash dvd:/%f&apos;.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="119"/>
        <source>Copy remote files</source>
        <translation>Copia i file remoti</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="122"/>
        <source>If set files on remote filesystems will be copied over to the local filesystem before processing. Speeds processing and reduces bandwidth on the network</source>
        <translation>Se impostato i file sul filesystem remoto saranno copiati sul filesystem locali prima dell&apos;elaborazione. Velocizza l&apos;elaborazione riduce la larghezza di banda sulla rete</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="134"/>
        <source>Always Use Mythtranscode</source>
        <translation>Usa sempre Mythtranscode</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="137"/>
        <source>If set mpeg2 files will always be passed though mythtranscode to clean up any errors. May help to fix some audio problems. Ignored if &apos;Use ProjectX&apos; is set.</source>
        <translation>Se impostato i file mpeg2 saranno sempre trattati da mythtranscode per pulire gli errori. Può aiutare a risolvere molti problemi audio. Ignorato se è impostato &quot;Usa ProjectX&quot;.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="149"/>
        <source>Use ProjectX</source>
        <translation>Usa ProjectX</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="152"/>
        <source>If set ProjectX will be used to cut commercials and split mpeg2 files instead of mythtranscode and mythreplex.</source>
        <translation>Se impostato invece di mythtranscode e mythreplex, per tagliare la pubblicità e dividere file mpeg2 verrà usato ProjectX.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="163"/>
        <source>Use FIFOs</source>
        <translation>Usa FIFO</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="166"/>
        <source>The script will use FIFOs to pass the output of mplex into dvdauthor rather than creating intermediate files. Saves time and disk space during multiplex operations but not supported on Windows platform</source>
        <translation>Lo script userà FIFO per passare l&apos;uscita di mplex dentro dvdauthor piuttosto che creare dei file intermedi. Risparmia tempo e spazio su disco durante le operazioni di multiplex ma non è supportato dalla piattaforma Windows</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="179"/>
        <source>Add Subtitles</source>
        <translation>Aggiungi sottotitoli</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="182"/>
        <source>If available this option will add subtitles to the final DVD. Requires &apos;Use ProjectX&apos; to be on.</source>
        <translation>Se disponibile questa opzione aggiungerà sottotitoli al DVD finale. Richiede di essere su &quot;Usare ProjectX&quot;.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="192"/>
        <source>Main Menu Aspect Ratio</source>
        <translation>Proporzione menù principale</translation>
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
        <translation>Formato da usare quando si crea il menù principale.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="208"/>
        <source>Chapter Menu Aspect Ratio</source>
        <translation>Formato menù capitoli</translation>
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
        <translation>Formato da usare quando si crea il menù capitoli.&quot;%1&quot; significa usare lo stesso formato come nel video associato.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="228"/>
        <source>Date format</source>
        <translation>Formato data</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="231"/>
        <source>Samples are shown using today&apos;s date.</source>
        <translation>I campioni sono mostrati usando la data di oggi.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="237"/>
        <source>Samples are shown using tomorrow&apos;s date.</source>
        <translation>I campioni sono mostrati usando la data di domani.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="255"/>
        <source>Your preferred date format to use on DVD menus. %1</source>
        <extracomment>%1 gives additional info on the date used</extracomment>
        <translation>Formaro data preferita da usare su i menù DVD. %1</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="264"/>
        <source>Time format</source>
        <translation>Formato ora</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="271"/>
        <source>Your preferred time format to display on DVD menus. You must choose a format with &quot;AM&quot; or &quot;PM&quot; in it, otherwise your time display will be 24-hour or &quot;military&quot; time.</source>
        <translation>Formato ora preferito da visualizzare su i menù DVD. Scegli un formato con &quot;AM&quot; o &quot;PM&quot;, altrimenti l&apos;ora visualizzata sarà di 24 ore o ora &quot;militare&quot;.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="282"/>
        <source>Default Encoder Profile</source>
        <translation>Profilo encoder preferito</translation>
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
        <translation>Profilo encoder preferito da usare se un file necessita di ricodifica.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="300"/>
        <source>mplex Command</source>
        <translation>Comando mplex</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="304"/>
        <source>Command to run mplex</source>
        <translation>Comando per eseguire mplex</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="313"/>
        <source>dvdauthor command</source>
        <translation>Comando dvdauthor</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="317"/>
        <source>Command to run dvdauthor.</source>
        <translation>Comando per eseguire dvdauthor.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="326"/>
        <source>mkisofs command</source>
        <translation>Comando mkisofs</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="330"/>
        <source>Command to run mkisofs. (Used to create ISO images)</source>
        <translation>Comando per eseguire mkisofs. (Usato per creare immagini ISO)</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="339"/>
        <source>growisofs command</source>
        <translation>Comando growisofs</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="343"/>
        <source>Command to run growisofs. (Used to burn DVDs)</source>
        <translation>Comando per eseguire growisofs. (Usato per masterizzare DVD)</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="352"/>
        <source>M2VRequantiser command</source>
        <translation>Comando M2VRequantiser</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="356"/>
        <source>Command to run M2VRequantiser. Optional - leave blank if you don&apos;t have M2VRequantiser installed.</source>
        <translation>Comando per eseguire M2VRequantiser. Opzionale - lascia vuoto se non hai M2VRequantiser installato.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="366"/>
        <source>jpeg2yuv command</source>
        <translation>Comando jpeg2yuv</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="370"/>
        <source>Command to run jpeg2yuv. Part of mjpegtools package</source>
        <translation>Comando per eseguire jpeg2yuv. Parte del pacchetto mjpegtools</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="379"/>
        <source>spumux command</source>
        <translation>Comando spumux</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="383"/>
        <source>Command to run spumux. Part of dvdauthor package</source>
        <translation>Comando per eseguire spumux. Parte del pacchetto dvdauthor</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="392"/>
        <source>mpeg2enc command</source>
        <translation>Comando mpeg2enc</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="396"/>
        <source>Command to run mpeg2enc. Part of mjpegtools package</source>
        <translation>Comando per eseguire mpeg2enc. Parte del pacchetto mjpegtools</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="405"/>
        <source>projectx command</source>
        <translation>Comando projectx</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="409"/>
        <source>Command to run ProjectX. Will be used to cut commercials and split mpegs files instead of mythtranscode and mythreplex.</source>
        <translation>Comando usato per eseguire ProjectX. Sarà usato per tagliare pubblicità e dividere file mpeg2 invece di mythtranscode e mythreplex.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="418"/>
        <source>MythArchive Settings</source>
        <translation>Impostazioni MythArchive</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="442"/>
        <source>MythArchive External Commands</source>
        <translation>Comandi esterni MythArchive </translation>
    </message>
    <message>
        <source>MythArchive Settings (2)</source>
        <translation type="vanished">Impostazioni MythArchive (2)</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="434"/>
        <source>DVD Menu Settings</source>
        <translation>Impostazioni menù DVD</translation>
    </message>
    <message>
        <source>MythArchive External Commands (1)</source>
        <translation type="vanished">Comandi esterni MythArchive (1)</translation>
    </message>
    <message>
        <source>MythArchive External Commands (2)</source>
        <translation type="vanished">Comandi esterni MythArchive (2)</translation>
    </message>
</context>
<context>
    <name>BurnMenu</name>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="1095"/>
        <source>Cannot burn a DVD.
The last run failed to create a DVD.</source>
        <translation>Impossibile masterizzare un DVD.
L&apos;ultima  creazione di un DVD è fallita.</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="1101"/>
        <location filename="../mytharchive/mythburn.cpp" line="1113"/>
        <source>Burn DVD</source>
        <translation>Masterizza DVD</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="1102"/>
        <source>
Place a blank DVD in the drive and select an option below.</source>
        <translation>
Colloca un DVD vergine nell&apos;unità e seleziona una opzione.</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="1114"/>
        <source>Burn DVD Rewritable</source>
        <translation>Masterizza DVD riscrivibile</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="1115"/>
        <source>Burn DVD Rewritable (Force Erase)</source>
        <translation>Masterizza DVD riscrivibile (forza cancellazione)</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="1170"/>
        <source>It was not possible to run mytharchivehelper to burn the DVD.</source>
        <translation>Non è stato possibile eseguire mytharchivehelper per scrivere il DVD.</translation>
    </message>
</context>
<context>
    <name>BurnThemeUI</name>
    <message>
        <location filename="../mythburn/themes/burnthemestrings.h" line="5"/>
        <source>Has an intro and contains a main menu with 4 recordings per page. Does not have a chapter selection submenu.</source>
        <translation>Ha una introduzione e contiene un menù principale con 4 registrazioni per pagina. Non ha un menù secondario di selezione del capitolo.</translation>
    </message>
    <message>
        <location filename="../mythburn/themes/burnthemestrings.h" line="6"/>
        <source>Has an intro and contains a summary main menu with 10 recordings per page. Does not have a chapter selection submenu, recording titles, dates or category.</source>
        <translation>Ha una introduzione e contiene un sommario del menù principale con 10 registrazioni per pagina. Non ha un menù secondario di selezione del capitolo, titoli di registrazione, date o categorie.</translation>
    </message>
    <message>
        <location filename="../mythburn/themes/burnthemestrings.h" line="7"/>
        <source>Has an intro and contains a main menu with 6 recordings per page. Does not have a scene selection submenu.</source>
        <translation>Ha una introduzione e contiene un menù principale con 6 registrazioni per pagina. Non ha un menù secondario di selezione del capitolo.</translation>
    </message>
    <message>
        <location filename="../mythburn/themes/burnthemestrings.h" line="8"/>
        <source>Has an intro and contains a main menu with 3 recordings per page and a scene selection submenu with 8 chapters points. Shows a program details page before each recording.</source>
        <translation>Ha una introduzione e contiene un menù principale con 3 registrazioni per pagina e un menù secondario di selezione scene con 8 punti capitolo. Mostra la pagina dei dettagli del programma prima di ogni registrazione.</translation>
    </message>
    <message>
        <location filename="../mythburn/themes/burnthemestrings.h" line="9"/>
        <source>Has an intro and contains a main menu with 3 recordings per page and a scene selection submenu with 8 chapters points. Shows a program details page before each recording. Uses animated thumb images.</source>
        <translation>Ha una introduzione e contiene un menù principale con 3 registrazioni per pagina e un menù secondario di selezione scene con 8 punti capitolo. Mostra la pagina dei dettagli del programma prima di ogni registrazione. Usa immagini animate e miniaturizzate.</translation>
    </message>
    <message>
        <location filename="../mythburn/themes/burnthemestrings.h" line="10"/>
        <source>Has an intro and contains a main menu with 3 recordings per page and a scene selection submenu with 8 chapters points.</source>
        <translation>Ha una introduzione e contiene un menù principale con 3 registrazioni per pagina e un menù secondario di selezione scene con 8 punti capitolo.</translation>
    </message>
    <message>
        <location filename="../mythburn/themes/burnthemestrings.h" line="11"/>
        <source>Has an intro and contains a main menu with 3 recordings per page and a scene selection submenu with 8 chapters points. All the thumb images are animated.</source>
        <translatorcomment>Ha una introduzione e contiene un menù principale con 3 registrazioni per pagina e un menù secondario di selezione scene con 8 punti capitolo. Tutte le immagini miniaturizzate sono animate.</translatorcomment>
        <translation>Has an intro and contains a main menu with 3 recordings per page and a scene selection submenu with 8 chapters points. All the thumb images are animated.</translation>
    </message>
    <message>
        <location filename="../mythburn/themes/burnthemestrings.h" line="12"/>
        <source>Creates an auto play DVD with no menus. Shows an intro movie then for each title shows a details page followed by the video in sequence.</source>
        <translation>Crea un DVD con riproduzione automatica senza menù. Mostra un video introduttivo quindi per ogni titolo mosta una pagina di dettagli seguito dal video in sequenza.</translation>
    </message>
    <message>
        <location filename="../mythburn/themes/burnthemestrings.h" line="13"/>
        <source>Creates an auto play DVD with no menus and no intro.</source>
        <translation>Crea un DVD con riproduzione automatica senza menù e introduzione.</translation>
    </message>
</context>
<context>
    <name>DVDThemeSelector</name>
    <message>
        <location filename="../mytharchive/themeselector.cpp" line="201"/>
        <location filename="../mytharchive/themeselector.cpp" line="212"/>
        <source>No theme description file found!</source>
        <translation>Nessun file di descrizione del tema trovato!</translation>
    </message>
    <message>
        <location filename="../mytharchive/themeselector.cpp" line="225"/>
        <source>Empty theme description!</source>
        <translation>Descrizione tema vuota!</translation>
    </message>
    <message>
        <location filename="../mytharchive/themeselector.cpp" line="230"/>
        <source>Unable to open theme description file!</source>
        <translation>Impossibile aprire il file di descrizione del tema!</translation>
    </message>
</context>
<context>
    <name>ExportNative</name>
    <message>
        <location filename="../mytharchive/exportnative.cpp" line="199"/>
        <source>You need to add at least one item to archive!</source>
        <translation>È richiesta l&apos;aggiunta di almeno un oggetto nell&apos;archivio!</translation>
    </message>
    <message>
        <location filename="../mytharchive/exportnative.cpp" line="344"/>
        <source>Menu</source>
        <translation>Menù</translation>
    </message>
    <message>
        <location filename="../mytharchive/exportnative.cpp" line="351"/>
        <source>Remove Item</source>
        <translation>Rimuovi oggetti</translation>
    </message>
    <message>
        <location filename="../mytharchive/exportnative.cpp" line="443"/>
        <source>It was not possible to create the DVD. An error occured when running the scripts</source>
        <translation>Non è stato possibile creare il DVD. E&apos; occorso un errore durante l&apos;esecuzione degli script</translation>
    </message>
    <message>
        <location filename="../mytharchive/exportnative.cpp" line="479"/>
        <source>You don&apos;t have any videos!</source>
        <translation>Non si hai nessun video!</translation>
    </message>
</context>
<context>
    <name>FileSelector</name>
    <message>
        <location filename="../mytharchive/fileselector.cpp" line="60"/>
        <source>Find File</source>
        <translation>Trova file</translation>
    </message>
    <message>
        <location filename="../mytharchive/fileselector.cpp" line="63"/>
        <source>Find Directory</source>
        <translation>Trova directory</translation>
    </message>
    <message>
        <location filename="../mytharchive/fileselector.cpp" line="66"/>
        <source>Find Files</source>
        <translation>Trova i file</translation>
    </message>
    <message>
        <location filename="../mytharchive/fileselector.cpp" line="296"/>
        <source>The selected item is not a directory!</source>
        <translation>L&apos;oggetto selezionato non è una directory!</translation>
    </message>
</context>
<context>
    <name>ImportNative</name>
    <message>
        <location filename="../mytharchive/importnative.cpp" line="389"/>
        <source>You need to select a valid channel id!</source>
        <translation>È richiesta la selezione di un id canale valido!</translation>
    </message>
    <message>
        <location filename="../mytharchive/importnative.cpp" line="420"/>
        <source>It was not possible to import the Archive.  An error occured when running &apos;mytharchivehelper&apos;</source>
        <translation>Non è possibile impostare l&apos;archivio. Si è verificato un errore durante l&apos;esecuzione di &quot;mytharchivehelper&quot;</translation>
    </message>
    <message>
        <location filename="../mytharchive/importnative.cpp" line="550"/>
        <source>Select a channel id</source>
        <translation>Seleziona un id canale</translation>
    </message>
    <message>
        <location filename="../mytharchive/importnative.cpp" line="576"/>
        <source>Select a channel number</source>
        <translation>Seleziona un numero canale</translation>
    </message>
    <message>
        <location filename="../mytharchive/importnative.cpp" line="602"/>
        <source>Select a channel name</source>
        <translation>Seleziona un nome canale</translation>
    </message>
    <message>
        <location filename="../mytharchive/importnative.cpp" line="628"/>
        <source>Select a Callsign</source>
        <translation>Seleziona nome delle stazione</translation>
    </message>
</context>
<context>
    <name>LogViewer</name>
    <message>
        <location filename="../mytharchive/logviewer.cpp" line="75"/>
        <source>Cannot find any logs to show!</source>
        <translation>Impossibile trovare log da visualizzare!</translation>
    </message>
    <message>
        <location filename="../mytharchive/logviewer.cpp" line="197"/>
        <source>Background creation has been asked to stop.
This may take a few minutes.</source>
        <translation>E&apos; stato richiesto l&apos;arresto della creazione dello sfondo.
Questo può richiedere alcuni minuti.</translation>
    </message>
    <message>
        <location filename="../mytharchive/logviewer.cpp" line="336"/>
        <source>Menu</source>
        <translation>Menù</translation>
    </message>
    <message>
        <location filename="../mytharchive/logviewer.cpp" line="344"/>
        <source>Don&apos;t Auto Update</source>
        <translation>Nessun aggiornamento automatico</translation>
    </message>
    <message>
        <location filename="../mytharchive/logviewer.cpp" line="346"/>
        <source>Auto Update</source>
        <translation>Aggiornamento automatico</translation>
    </message>
    <message>
        <location filename="../mytharchive/logviewer.cpp" line="348"/>
        <source>Show Progress Log</source>
        <translation>Mostra log interattivo</translation>
    </message>
    <message>
        <location filename="../mytharchive/logviewer.cpp" line="349"/>
        <source>Show Full Log</source>
        <translation>Mostra log completo</translation>
    </message>
</context>
<context>
    <name>MythBurn</name>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="332"/>
        <location filename="../mytharchive/mythburn.cpp" line="452"/>
        <source>No Cut List</source>
        <translation>Nessun elenco tagli</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="343"/>
        <source>You need to add at least one item to archive!</source>
        <translation>Devi aggiungere almeno un oggetto nell&apos;archivio!</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="389"/>
        <source>Retrieving File Information. Please Wait...</source>
        <translation>Recupero informazioni sui file. Attendere...</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="455"/>
        <source>Encoder: </source>
        <translation>Encoder: </translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="767"/>
        <source>Menu</source>
        <translation>Menù</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="321"/>
        <location filename="../mytharchive/mythburn.cpp" line="441"/>
        <source>Using Cut List</source>
        <translation>Usa elenco tagli</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="326"/>
        <location filename="../mytharchive/mythburn.cpp" line="446"/>
        <source>Not Using Cut List</source>
        <translation>Non uso elenco tagli</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="778"/>
        <source>Don&apos;t Use Cut List</source>
        <translation>Non usare elenco tagli</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="783"/>
        <source>Use Cut List</source>
        <translation>Usa elenco tagli</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="788"/>
        <source>Remove Item</source>
        <translation>Rimuovi elemento</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="789"/>
        <source>Edit Details</source>
        <translation>Modifica dettagli</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="790"/>
        <source>Change Encoding Profile</source>
        <translation>Cambia profilo encoder</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="791"/>
        <source>Edit Thumbnails</source>
        <translation>Modifica anteprima</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="926"/>
        <source>It was not possible to create the DVD.  An error occured when running the scripts</source>
        <translation>Non è stato possibile creare il DVD, E&apos; occorso un errore durante l&apos;esecuzione degli script</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="968"/>
        <source>You don&apos;t have any videos!</source>
        <translation>Non hai nessun video!</translation>
    </message>
</context>
<context>
    <name>MythControls</name>
    <message>
        <location filename="../mytharchive/main.cpp" line="335"/>
        <source>Toggle use cut list state for selected program</source>
        <translation>Attiva/Disattiva stato elenco lista tagli per il programma selezionato</translation>
    </message>
    <message>
        <location filename="../mytharchive/main.cpp" line="338"/>
        <source>Create DVD</source>
        <translation>Crea DVD</translation>
    </message>
    <message>
        <location filename="../mytharchive/main.cpp" line="340"/>
        <source>Create Archive</source>
        <translation>Crea archivio</translation>
    </message>
    <message>
        <location filename="../mytharchive/main.cpp" line="342"/>
        <source>Import Archive</source>
        <translation>Importa archivio</translation>
    </message>
    <message>
        <location filename="../mytharchive/main.cpp" line="344"/>
        <source>View Archive Log</source>
        <translation>Mostra archivio log</translation>
    </message>
    <message>
        <location filename="../mytharchive/main.cpp" line="346"/>
        <source>Play Created DVD</source>
        <translation>Riproduci DVD creato</translation>
    </message>
    <message>
        <location filename="../mytharchive/main.cpp" line="348"/>
        <source>Burn DVD</source>
        <translation>Masterizza DVD</translation>
    </message>
</context>
<context>
    <name>QObject</name>
    <message>
        <source>MythArchive Temp Directory</source>
        <translation type="obsolete">Directory temporanea di MythArchive</translation>
    </message>
    <message>
        <source>Location where MythArchive should create its temporary work files. LOTS of free space required here.</source>
        <translation type="obsolete">Percorso dove MythArchive dovrebbe creare i suoi file di lavoro temporanei. Richiesto molto spazio libero su disco.</translation>
    </message>
    <message>
        <source>MythArchive Share Directory</source>
        <translation type="obsolete">Directory condivisa da MythArchive</translation>
    </message>
    <message>
        <source>Location where MythArchive stores its scripts, intro movies and theme files</source>
        <translation type="obsolete">Percorso dove MythArchive archivia i suioi script, filmati introduttivi e file di tema</translation>
    </message>
    <message>
        <source>Video format</source>
        <translation type="obsolete">Formato video</translation>
    </message>
    <message>
        <source>Video format for DVD recordings, PAL or NTSC.</source>
        <translation type="obsolete">Formato video per registrazioni DVD, PAL o NTSC.</translation>
    </message>
    <message>
        <source>File Selector Filter</source>
        <translation type="obsolete">Filtro del selettore del file</translation>
    </message>
    <message>
        <source>The file name filter to use in the file selector.</source>
        <translation type="obsolete">Il nome del file del filtro usato nel selettore dei file.</translation>
    </message>
    <message>
        <source>Location of DVD</source>
        <translation type="obsolete">Percorso del DVD</translation>
    </message>
    <message>
        <source>Which DVD drive to use when burning discs.</source>
        <translation type="obsolete">Quale unità DVD da usare quando si masterizzano dischi.</translation>
    </message>
    <message>
        <source>DVD Drive Write Speed</source>
        <translation type="obsolete">Velocità di scrittura dell&apos;unità DVD</translation>
    </message>
    <message>
        <source>This is the write speed to use when burning a DVD. Set to 0 to allow growisofs to choose the fastest available speed.</source>
        <translation type="obsolete">Questa è la velocità di scrittura da usare quando si masterizza un DVD. Impostare a 0 permette a growisofs di scegliere la velocità disponibile più veloce.</translation>
    </message>
    <message>
        <source>Command to play DVD</source>
        <translation type="obsolete">Comando per riprodurre DVD</translation>
    </message>
    <message>
        <source>Command to run when test playing a created DVD. &apos;Internal&apos; will use the internal MythTV player. %f will be replaced with the path to the created DVD structure eg. &apos;xine -pfhq --no-splash dvd:/%f&apos;.</source>
        <translation type="obsolete">Comando da eseguire quando si testa la riproduzione di un DVD creato &apos;Interno&apos;userà il riproduttore interno MythTV. %f sarà sostituito con il percorso della strutura del DVD creato es. &apos;xine -pfhq --no-splash dvd:/%f&apos;.</translation>
    </message>
    <message>
        <source>Copy remote files</source>
        <translation type="obsolete">Copiare i file remoti</translation>
    </message>
    <message>
        <source>If set files on remote filesystems will be copied over to the local filesystem before processing. Speeds processing and reduces bandwidth on the network</source>
        <translation type="obsolete">Se impostato i file sul filesystem remoto saranno copiati sul filesystem locali prima dell&apos;elaborazione. La velocità dell&apos; elaborazione riduce la larghezza di banda sulla rete</translation>
    </message>
    <message>
        <source>Always Use Mythtranscode</source>
        <translation type="obsolete">Usare sempre Mythtranscode</translation>
    </message>
    <message>
        <source>If set mpeg2 files will always be passed though mythtranscode to clean up any errors. May help to fix some audio problems. Ignored if &apos;Use ProjectX&apos; is set.</source>
        <translation type="obsolete">Se impostato i file mpeg2 saranno sempre passato tramite mythtranscode per pulire ogni errore. Può aiutare a fissare molti problemi audio. Ignorato se è impostato &quot;Usare ProjectX&quot;.</translation>
    </message>
    <message>
        <source>Use ProjectX</source>
        <translation type="obsolete">Usare ProjectX</translation>
    </message>
    <message>
        <source>If set ProjectX will be used to cut commercials and split mpeg2 files instead of mythtranscode and mythreplex.</source>
        <translation type="obsolete">Se impostato ProjectX verrà usato per tagliare pubblicità e dividere file mpeg2 invece di mythtranscode e mythreplex.</translation>
    </message>
    <message>
        <source>Use FIFOs</source>
        <translation type="obsolete">Usare FIFOs</translation>
    </message>
    <message>
        <source>The script will use FIFOs to pass the output of mplex into dvdauthor rather than creating intermediate files. Saves time and disk space during multiplex operations but not  supported on Windows platform</source>
        <translation type="obsolete">Lo script userà FIFOs per passare l&apos;uscita di mplex dentro dvdauthor piuttosto che creare dei file intermedi.Risparmia tempo e spazio su disco durante le operazioni di multiplex ma non supportati dalla piattaforma Windows</translation>
    </message>
    <message>
        <source>Add Subtitles</source>
        <translation type="obsolete">Aggiungere sottotitoli</translation>
    </message>
    <message>
        <source>If available this option will add subtitles to the final DVD. Requires &apos;Use ProjectX&apos; to be on.</source>
        <translation type="obsolete">Se disponibile questa opzione aggiungerà sottotitoli al DVD finale. Richiede di essere su &quot;Usare ProjectX&quot;.</translation>
    </message>
    <message>
        <source>Main Menu Aspect Ratio</source>
        <translation type="obsolete">Menù principale proporzione</translation>
    </message>
    <message>
        <source>Aspect ratio to use when creating the main menu.</source>
        <translation type="obsolete">Proporzione da usare quando si crea il menù principale.</translation>
    </message>
    <message>
        <source>Chapter Menu Aspect Ratio</source>
        <translation type="obsolete">Proporzione menù capitoli</translation>
    </message>
    <message>
        <source>Aspect ratio to use when creating the chapter menu. Video means use the same aspect ratio as the associated video.</source>
        <translation type="obsolete">Proporzione da usare quando si crea il menù capitoli. Video significa usare la stessa proporzione come nel video associato.</translation>
    </message>
    <message>
        <source>Date format</source>
        <translation type="obsolete">Formato data</translation>
    </message>
    <message>
        <source>Samples are shown using today&apos;s date.</source>
        <translation type="obsolete">I campioni sono mostrati usando la data di oggi.</translation>
    </message>
    <message>
        <source>Samples are shown using tomorrow&apos;s date.</source>
        <translation type="obsolete">I campioni sono mostrati usando la data di domani.</translation>
    </message>
    <message>
        <source>Your preferred date format to use on DVD menus.</source>
        <translation type="obsolete">Formaro data preferita da usare su i menù DVD.</translation>
    </message>
    <message>
        <source>Time format</source>
        <translation type="obsolete">Formato ora</translation>
    </message>
    <message>
        <source>Your preferred time format to display on DVD menus. You must choose a format with &quot;AM&quot; or &quot;PM&quot; in it, otherwise your time display will be 24-hour or &quot;military&quot; time.</source>
        <translation type="obsolete">Formato ora preferito da visualizzare su i menù DVD. Scegliere un formato con &quot;AM&quot; o &quot;PM&quot;, altrimenti l&apos;ora visualizzata sarà di 24 ore o ora &quot;militare&quot;.</translation>
    </message>
    <message>
        <source>Default Encoder Profile</source>
        <translation type="obsolete">Profilo di codificazione preferito</translation>
    </message>
    <message>
        <source>Default encoding profile to use if a file needs re-encoding.</source>
        <translation type="obsolete">Profilo di codificazione preferito da usare se un file necessita di ricodifica.</translation>
    </message>
    <message>
        <source>mplex Command</source>
        <translation type="obsolete">Comando mplex</translation>
    </message>
    <message>
        <source>Command to run mplex</source>
        <translation type="obsolete">Comando per eseguire mplex</translation>
    </message>
    <message>
        <source>dvdauthor command</source>
        <translation type="obsolete">Comando dvdauthor</translation>
    </message>
    <message>
        <source>Command to run dvdauthor.</source>
        <translation type="obsolete">Comando per eseguire dvdauthor.</translation>
    </message>
    <message>
        <source>mkisofs command</source>
        <translation type="obsolete">Comando mkisofs</translation>
    </message>
    <message>
        <source>Command to run mkisofs. (Used to create ISO images)</source>
        <translation type="obsolete">Comando per eseguire mkisofs. (Usato per creare immagini ISO)</translation>
    </message>
    <message>
        <source>growisofs command</source>
        <translation type="obsolete">Comando growisofs</translation>
    </message>
    <message>
        <source>Command to run growisofs. (Used to burn DVD&apos;s)</source>
        <translation type="obsolete">Comando per eseguire growisofs. (Usato per masterizzare DVD)</translation>
    </message>
    <message>
        <source>M2VRequantiser command</source>
        <translation type="obsolete">Comando M2VRequantiser</translation>
    </message>
    <message>
        <source>Command to run M2VRequantiser. Optional - leave blank if you don&apos;t have M2VRequantiser installed.</source>
        <translation type="obsolete">Comando per eseguire M2VRequantiser. Opzionale - lasciare vuoto se non si ha M2VRequantiser installato.</translation>
    </message>
    <message>
        <source>jpeg2yuv command</source>
        <translation type="obsolete">Comando jpeg2yuv</translation>
    </message>
    <message>
        <source>Command to run jpeg2yuv. Part of mjpegtools package</source>
        <translation type="obsolete">Comando per eseguire jpeg2yuv. Parte del pacchetto mjpegtools</translation>
    </message>
    <message>
        <source>spumux command</source>
        <translation type="obsolete">Comando spumux</translation>
    </message>
    <message>
        <source>Command to run spumux. Part of dvdauthor package</source>
        <translation type="obsolete">Comando per eseguire spumux. Parte del pacchetto dvdauthor</translation>
    </message>
    <message>
        <source>mpeg2enc command</source>
        <translation type="obsolete">Comando mpeg2enc</translation>
    </message>
    <message>
        <source>Command to run mpeg2enc. Part of mjpegtools package</source>
        <translation type="obsolete">Comando per eseguire mpeg2enc. Parte del pacchetto mjpegtools</translation>
    </message>
    <message>
        <source>projectx command</source>
        <translation type="obsolete">Comando projectx</translation>
    </message>
    <message>
        <source>Command to run ProjectX. Will be used to cut commercials and split mpegs files instead of mythtranscode and mythreplex.</source>
        <translation type="obsolete">Comando usato per eseguire ProjectX. Sarà usato per tagliare pubblicità e dividere file mpeg2 invece di mythtranscode e mythreplex.</translation>
    </message>
    <message>
        <source>MythArchive Settings</source>
        <translation type="obsolete">Impostazioni MythArchive</translation>
    </message>
    <message>
        <source>MythArchive Settings (2)</source>
        <translation type="obsolete">Impostazioni MythArchive (2)</translation>
    </message>
    <message>
        <source>DVD Menu Settings</source>
        <translation type="obsolete">Impostazioni menù DVD</translation>
    </message>
    <message>
        <source>MythArchive External Commands (1)</source>
        <translation type="obsolete">Comandi esterni MythArchive (1)</translation>
    </message>
    <message>
        <source>MythArchive External Commands (2)</source>
        <translation type="obsolete">Comandi esterni MythArchive (2)</translation>
    </message>
    <message>
        <source>Cannot find the MythArchive work directory.
Have you set the correct path in the settings?</source>
        <translation type="obsolete">Impossibile trovare la directory di lavoro di MythArchive.
Avere impostato il percorso corretto nelle impostazioni?</translation>
    </message>
    <message>
        <source>It was not possible to create the DVD. An error occured when running the scripts</source>
        <translation type="obsolete">Non è stato possibile creare il DVD, Un errore è occorso quando gli script sono stati in esecuzione</translation>
    </message>
    <message>
        <source>Cannot find any logs to show!</source>
        <translation type="obsolete">Impossibile trovare ogni log da visualizzare!</translation>
    </message>
    <message>
        <source>Background creation has been asked to stop.
This may take a few minutes.</source>
        <translation type="obsolete">La creazione dello sfondo è stato richiesto di fermarsi.
Questo può richiedere alcuni minuti.</translation>
    </message>
    <message>
        <source>Found a lock file but the owning process isn&apos;t running!
Removing stale lock file.</source>
        <translation type="obsolete">Trovato un file bloccato ma il processo proprietario non è in esecuzione!
Rimuove lo stato di blocco del file.</translation>
    </message>
    <message>
        <source>Last run did not create a playable DVD.</source>
        <translation type="obsolete">L&apos;ultima esecuzione non ha creato un DVD riproducibile.</translation>
    </message>
    <message>
        <source>Last run failed to create a DVD.</source>
        <translation type="obsolete">L&apos;ultima esecuzione ha fallito la creazione di un DVD.</translation>
    </message>
    <message>
        <source>You don&apos;t have any videos!</source>
        <translation type="obsolete">Nessun video!</translation>
    </message>
    <message>
        <source>Cannot burn a DVD.
The last run failed to create a DVD.</source>
        <translation type="obsolete">Impossibile masterizzare un DVD.
L&apos;ultima esecuzione ha fallito la creazione di un DVD.</translation>
    </message>
    <message>
        <source>Burn DVD</source>
        <translation type="obsolete">Mastrerizzare DVD</translation>
    </message>
    <message>
        <source>
Place a blank DVD in the drive and select an option below.</source>
        <translation type="obsolete">
Collocare un DVD vergine nell&apos;unità e selezionare un opzione sotto.</translation>
    </message>
    <message>
        <source>Burn DVD Rewritable</source>
        <translation type="obsolete">Masterizzare DVD riscrivibile</translation>
    </message>
    <message>
        <source>Burn DVD Rewritable (Force Erase)</source>
        <translation type="obsolete">Masterizzare DVD riscrivibile (forzare cancellazione)</translation>
    </message>
    <message>
        <source>It was not possible to run mytharchivehelper to burn the DVD.</source>
        <translation type="obsolete">Non è stato possibile eseguire mytharchivehelper per masterizzare il DVD.</translation>
    </message>
</context>
<context>
    <name>RecordingSelector</name>
    <message>
        <location filename="../mytharchive/recordingselector.cpp" line="88"/>
        <location filename="../mytharchive/recordingselector.cpp" line="373"/>
        <location filename="../mytharchive/recordingselector.cpp" line="478"/>
        <source>All Recordings</source>
        <translation>Tutte le registrazioni</translation>
    </message>
    <message>
        <location filename="../mytharchive/recordingselector.cpp" line="109"/>
        <source>Retrieving Recording List.
Please Wait...</source>
        <translation>Recupero elenco registrazioni.
Attendere...</translation>
    </message>
    <message>
        <location filename="../mytharchive/recordingselector.cpp" line="133"/>
        <source>Either you don&apos;t have any recordings or no recordings are available locally!</source>
        <translation>O non si dispone di nessuna registrazione o le registrazioni non sono disponibile localmente!</translation>
    </message>
    <message>
        <location filename="../mytharchive/recordingselector.cpp" line="181"/>
        <source>Menu</source>
        <translation>Menù</translation>
    </message>
    <message>
        <location filename="../mytharchive/recordingselector.cpp" line="188"/>
        <source>Clear All</source>
        <translation>Cancella tutto</translation>
    </message>
    <message>
        <location filename="../mytharchive/recordingselector.cpp" line="189"/>
        <source>Select All</source>
        <translation>Seleziona tutto</translation>
    </message>
</context>
<context>
    <name>SelectDestination</name>
    <message>
        <location filename="../mytharchive/archiveutil.cpp" line="31"/>
        <source>Single Layer DVD</source>
        <translation>DVD singolo strato</translation>
    </message>
    <message>
        <location filename="../mytharchive/archiveutil.cpp" line="35"/>
        <source>Dual Layer DVD</source>
        <translation>DVD doppio strato</translation>
    </message>
    <message>
        <location filename="../mytharchive/archiveutil.cpp" line="32"/>
        <source>Single Layer DVD (4,482 MB)</source>
        <translation>DVD singolo strato (4,482 MB)</translation>
    </message>
    <message>
        <location filename="../mytharchive/archiveutil.cpp" line="36"/>
        <source>Dual Layer DVD (8,964 MB)</source>
        <translation>DVD doppio strato (8,964 MB)</translation>
    </message>
    <message>
        <location filename="../mytharchive/archiveutil.cpp" line="39"/>
        <source>DVD +/- RW</source>
        <translation>DVD +/- RW</translation>
    </message>
    <message>
        <location filename="../mytharchive/archiveutil.cpp" line="40"/>
        <source>Rewritable DVD</source>
        <translation>DVD riscrivibile</translation>
    </message>
    <message>
        <location filename="../mytharchive/archiveutil.cpp" line="43"/>
        <source>File</source>
        <translation>File</translation>
    </message>
    <message>
        <location filename="../mytharchive/archiveutil.cpp" line="44"/>
        <source>Any file accessable from your filesystem.</source>
        <translation>Tutti file accesibili dal filesystem.</translation>
    </message>
    <message>
        <location filename="../mytharchive/selectdestination.cpp" line="261"/>
        <location filename="../mytharchive/selectdestination.cpp" line="317"/>
        <source>Unknown</source>
        <translation>Sconosciuto</translation>
    </message>
</context>
<context>
    <name>ThemeUI</name>
    <message>
        <location filename="themestrings.h" line="21"/>
        <source>A high quality profile giving approx. 1 hour of video on a single layer DVD</source>
        <translation>Un profilo di altà qualità che consente circa 1 ora di video su un DVD singolo strato</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="23"/>
        <source>A standard play profile giving approx. 2 hour of video on a single layer DVD</source>
        <translation>Un profilo di riproduzione standard che consente circa 2 ore di video su un DVD singolo strato</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="22"/>
        <source>A long play profile giving approx. 4 hour of video on a single layer DVD</source>
        <translation>Un profilo di riproduzione lunga che consente circa 4 ore di video su un DVD singolo strato</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="20"/>
        <source>A extended play profile giving approx. 6 hour of video on a single layer DVD</source>
        <translation>Un profilo di riproduzione estesa che consente circa 6 ore di video su un DVD singolo strato</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="5"/>
        <source>%DATE%, %TIME%</source>
        <translation>%DATE%, %TIME%</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="221"/>
        <source>Select Destination</source>
        <translation>Seleziona destinazione</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="78"/>
        <source>Choose where you would like your files archived.</source>
        <translation>Scegli dove si desidi archiviare i file.</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="185"/>
        <source>Output Type:</source>
        <translation>Tipo uscita:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="100"/>
        <source>Destination:</source>
        <translation>Destinazione:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="138"/>
        <source>Free Space:</source>
        <translation>Spazio libero:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="79"/>
        <source>Click here to find an output location...</source>
        <translation>Clic qui per trovare una locazione d&apos;uscita...</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="160"/>
        <source>Make ISO Image</source>
        <translation>Crea immagine ISO</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="52"/>
        <source>Burn to DVD</source>
        <translation>Mastrerizza DVD</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="115"/>
        <source>Erase DVD-RW before burning</source>
        <translation>Cancella DVD-RW prima di masterizzre</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="58"/>
        <source>Cancel</source>
        <translation>Annulla</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="199"/>
        <source>Previous</source>
        <translation>Precedente</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="173"/>
        <source>Next</source>
        <translation>Successivo</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="130"/>
        <source>Filter:</source>
        <translation>Filtro:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="180"/>
        <source>OK</source>
        <translation>OK</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="123"/>
        <source>File Finder</source>
        <translation>Trova file</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="231"/>
        <source>Select the file you wish to use.</source>
        <translation>Seleziona il file che desideri utilizzare.</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="48"/>
        <source>Back</source>
        <translation>Indietro</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="139"/>
        <source>Home</source>
        <translation>Home</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="176"/>
        <source>No videos available</source>
        <translation>Nessun video disponibile</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="154"/>
        <source>Log Viewer</source>
        <translation>Visualizzatore log</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="214"/>
        <source>See logs from your archive runs.</source>
        <translation>Vedi log archiviati delle esecuzioni.</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="267"/>
        <source>Update</source>
        <translation>Aggiorna</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="117"/>
        <source>Exit</source>
        <translation>Esci</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="266"/>
        <source>Up Level</source>
        <translation>Livello su</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="280"/>
        <source>description goes here.</source>
        <translation>la descrizione va messa qui.</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="12"/>
        <source>0.00 GB</source>
        <translation>0.00 GB</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="6"/>
        <source>%SIZE% ~ %PROFILE%</source>
        <translation type="unfinished">%SIZE% ~ %PROFILE%</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="8"/>
        <source>%date% / %profile%</source>
        <translation type="unfinished">%date% / %profile%</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="9"/>
        <source>%size% (%profile%)</source>
        <translation type="unfinished">%size% (%profile%)</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="13"/>
        <source>0.00Gb</source>
        <translation type="unfinished">0.00Gb</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="17"/>
        <source>12.34GB</source>
        <translation type="unfinished">12.34GB</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="18"/>
        <source>&lt;-- Details</source>
        <translation type="unfinished">&lt;-- Dettagli</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="19"/>
        <source>&lt;-- Main Menu</source>
        <translation type="unfinished">&lt;-- Menu Principale</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="26"/>
        <source>Add Recordings</source>
        <translation type="unfinished">Agg.Registrazioni</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="28"/>
        <source>Add Videos</source>
        <translation type="unfinished">Aggiungi video</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="29"/>
        <source>Add a recording or video to archive.</source>
        <translation type="unfinished">Aggiungi una registrazione o un video da archiviare.</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="30"/>
        <source>Add a recording, video or file to archive.</source>
        <translation type="unfinished">Aggiungi una registrazione, un video o un file da archiviare.</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="32"/>
        <source>Archive</source>
        <translation type="unfinished">Archivio</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="33"/>
        <source>Archive Callsign:</source>
        <translation type="unfinished">Archivio Stazioni:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="34"/>
        <source>Archive Chan ID:</source>
        <translation type="unfinished">Archivio Chan ID:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="35"/>
        <source>Archive Chan No:</source>
        <translation type="unfinished">Archivio Chan No:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="36"/>
        <source>Archive Files</source>
        <translation type="unfinished">Archivio File</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="38"/>
        <source>Archive Item:</source>
        <translation type="unfinished">Archivio Elementi:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="40"/>
        <source>Archive Items to DVD</source>
        <translation type="unfinished">Archivio elementi a DVD</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="41"/>
        <source>Archive Log Viewer</source>
        <translation type="unfinished">Visualizza Log archivio</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="42"/>
        <source>Archive Media</source>
        <translation type="unfinished">Archivio Media</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="43"/>
        <source>Archive Name:</source>
        <translation type="unfinished">Nome archivio:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="46"/>
        <source>Associate Channel</source>
        <translation type="unfinished">Associa canale</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="47"/>
        <source>Associated Channel</source>
        <translation type="unfinished">Canale associato</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="49"/>
        <source>Burn</source>
        <translation type="unfinished">Masterizza</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="50"/>
        <source>Burn Created DVD</source>
        <translation type="unfinished">Masterizza DVD creato</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="51"/>
        <source>Burn the last created archive to DVD</source>
        <translation type="unfinished">Masterizza su DVD l&apos;ultimo archivio creato</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="53"/>
        <source>Burn to DVD:</source>
        <translation type="unfinished">Masterizza su DVD:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="54"/>
        <source>CUTLIST</source>
        <translation type="unfinished">LISTATAGLI</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="55"/>
        <source>Callsign</source>
        <translation type="unfinished">Emittente</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="57"/>
        <source>Callsign:  %1</source>
        <translation type="unfinished">Emittente:  %1</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="59"/>
        <source>Cancel Job</source>
        <translation type="unfinished">Annulla lavoro</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="61"/>
        <source>Chan. Id:</source>
        <translation type="unfinished">Id Can.:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="63"/>
        <source>Change</source>
        <translation type="unfinished">Modifica</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="65"/>
        <source>Channel ID</source>
        <translation type="unfinished">ID Canale</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="67"/>
        <source>Channel ID:  %1</source>
        <translation type="unfinished">ID Canale:  %1</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="68"/>
        <source>Channel Name:</source>
        <translation type="unfinished">Nome canale:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="69"/>
        <source>Channel No</source>
        <translation type="unfinished">N. canale</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="71"/>
        <source>Channel Number:  %1</source>
        <translation type="unfinished">Numero canale:  %1</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="74"/>
        <source>Chapter Menu --&gt;</source>
        <translation type="unfinished">Menu capitolo --&gt;</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="75"/>
        <source>Chapter Menu:</source>
        <translation type="unfinished">Menu capitolo:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="76"/>
        <source>Chapters</source>
        <translation type="unfinished">Capitoli</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="82"/>
        <source>Create ISO Image:</source>
        <translation type="unfinished">Crea immagine ISO:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="83"/>
        <source>Create a</source>
        <translation type="unfinished">Crea un</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="84"/>
        <source>Create a DVD of your videos</source>
        <translation type="unfinished">Crea un DVD dei tuoi video</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="85"/>
        <source>Current Destination:</source>
        <translation type="unfinished">Destinazione attuale:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="87"/>
        <source>Current Position:</source>
        <translation type="unfinished">Posizione attuale:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="88"/>
        <source>Current Size:</source>
        <translation type="unfinished">Dimensione attuale:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="89"/>
        <source>Current selected item size: %1</source>
        <translation type="unfinished">Dimensione elemento selezionato: %1</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="90"/>
        <source>Current size: %1</source>
        <translation type="unfinished">Dimensione attuale: %1</translation>
    </message>
    <message numerus="yes">
        <location filename="themestrings.h" line="91"/>
        <source>Current: %n</source>
        <translation type="unfinished">
            <numerusform>Corrente: %n</numerusform>
            <numerusform>Correnti: %n</numerusform>
        </translation>
    </message>
    <message>
        <location filename="themestrings.h" line="92"/>
        <source>DVD</source>
        <translation type="unfinished">DVD</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="93"/>
        <source>DVD Media</source>
        <translation type="unfinished">DVD Media</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="94"/>
        <source>DVD Menu</source>
        <translation type="unfinished">Menu DVD</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="96"/>
        <source>Date (time)</source>
        <translation type="unfinished">Data (ora)</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="98"/>
        <source>Destination</source>
        <translation type="unfinished">Destinazione</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="99"/>
        <source>Destination Free Space:</source>
        <translation type="unfinished">Spazio libero su destinazione:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="103"/>
        <source>Details:</source>
        <translation type="unfinished">Dettagli:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="104"/>
        <source>Done</source>
        <translation type="unfinished">Fatto</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="105"/>
        <source>Edit Archive item Information</source>
        <translation type="unfinished">Modifica info elemento dell&apos;archivio</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="106"/>
        <source>Edit Details</source>
        <translation type="unfinished">Modifica dettagli</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="107"/>
        <source>Edit Thumbnails</source>
        <translation type="unfinished">Modifica anteprima</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="108"/>
        <source>Eject your</source>
        <translation type="unfinished">Espelli il tuo</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="110"/>
        <source>Encode your video</source>
        <translation type="unfinished">Codifica il video</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="111"/>
        <source>Encode your video to a file</source>
        <translation type="unfinished">Codifica il video su un file</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="112"/>
        <source>Encoded Size:</source>
        <translation type="unfinished">Dimensione codificata:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="113"/>
        <source>Encoder Profile:</source>
        <translation type="unfinished">Profilo  encoder:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="114"/>
        <source>Encoding Profile</source>
        <translation type="unfinished">Profilo codifica</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="116"/>
        <source>Error: %1</source>
        <translation type="unfinished">Errore: %1</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="118"/>
        <source>Export</source>
        <translation type="unfinished">Esporta</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="119"/>
        <source>Export your</source>
        <translation type="unfinished">Esporta il tuo</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="120"/>
        <source>Export your videos</source>
        <translation type="unfinished">Esporta i tuoi video</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="121"/>
        <source>File</source>
        <translation type="unfinished">File</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="122"/>
        <source>File Browser</source>
        <translation type="unfinished">Browser File</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="125"/>
        <source>File browser</source>
        <translation type="unfinished">Browser file</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="126"/>
        <source>File...</source>
        <translation type="unfinished">File...</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="127"/>
        <source>Filename:</source>
        <translation type="unfinished">Nome file:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="128"/>
        <source>Filesize</source>
        <translation type="unfinished">Dimensione file</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="132"/>
        <source>Find Location</source>
        <translation type="unfinished">Trova posizione</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="134"/>
        <source>First, select the thumb image you want to change from the overview. Second, press the &apos;tab&apos; key to move to the select thumb frame button to change the image.</source>
        <translation type="unfinished">Seleziona prima dall&apos;elenco l&apos;icona che desideri modificare, poi premi il tasto &quot;Tab&quot; per passare al pulsante di selezione del riquadro dell&apos;icona per modificare l&apos;immagine.</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="135"/>
        <source>Force Overwrite of DVD-RW Media</source>
        <translation>Forza sovrascrittura supporto DVD-RW</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="136"/>
        <source>Force Overwrite of DVD-RW Media:</source>
        <translation type="unfinished">Forza sovrascrittura supporti DVD-RW:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="140"/>
        <source>Import</source>
        <translation type="unfinished">Importa</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="142"/>
        <source>Import an</source>
        <translation type="unfinished">Importa un</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="143"/>
        <source>Import recordings from a native archive</source>
        <translation type="unfinished">Importa registrazioni da archivio originale</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="144"/>
        <source>Import videos from an Archive</source>
        <translation type="unfinished">Importa registrazioni da un archivio</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="146"/>
        <source>Intro --&gt;</source>
        <translation type="unfinished">Intro --&gt;</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="147"/>
        <source>Intro:</source>
        <translation type="unfinished">Intro:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="148"/>
        <source>Local Callsign:</source>
        <translation type="unfinished">Emittente locale:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="149"/>
        <source>Local Chan ID:</source>
        <translation type="unfinished">ID emittente locale:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="150"/>
        <source>Local Chan No:</source>
        <translation type="unfinished">N. Can. locale:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="152"/>
        <source>Local Name:</source>
        <translation type="unfinished">Nome locale:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="153"/>
        <source>Log</source>
        <translation type="unfinished">Log</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="155"/>
        <source>Log viewer</source>
        <translation type="unfinished">Visualizz. Log</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="156"/>
        <source>MENU changes focus. Numbers 0-9 jump to that thumb image.
When the preview image has focus, UP/DOWN changes the seek amount, LEFT/RIGHT jumps forward/backward by the seek amount, and SELECT chooses the current preview image for the selected thumb image.</source>
        <translation type="unfinished">MENU cambia selezione. I numeri da 0 a 9 spostano sull&apos;immagine dell&apos;icona.
Quando l&apos;immagine di anteprima è selezionata, SU/GIÙ cambia l&apos;entità dello spostamento, SINISTRA/DESTRA salta in avanti/indietro dell&apos;entità di spostamento definita e SELEZIONA sceglie l&apos;immagine di anteprima corrente come immagine della stazione selezionata.</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="158"/>
        <source>Main Menu:</source>
        <translation type="unfinished">Menù principale:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="159"/>
        <source>Main menu</source>
        <translation type="unfinished">Menù principale</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="161"/>
        <source>Make ISO image</source>
        <translation type="unfinished">Crea immagine ISO</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="162"/>
        <source>Make ISO image:</source>
        <translation type="unfinished">Crea immagine ISO:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="163"/>
        <source>Media</source>
        <translation type="unfinished">Media</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="165"/>
        <source>Metadata</source>
        <translation type="unfinished">Metadati</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="166"/>
        <source>Name</source>
        <translation type="unfinished">Nome</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="168"/>
        <source>Name:  %1</source>
        <translation type="unfinished">Nome:  %1</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="169"/>
        <source>Navigation</source>
        <translation type="unfinished">Navigazione</translation>
    </message>
    <message numerus="yes">
        <location filename="themestrings.h" line="172"/>
        <source>New: %n</source>
        <translation type="unfinished">
            <numerusform>Nuovo: %n</numerusform>
            <numerusform>Nuovi: %n</numerusform>
        </translation>
    </message>
    <message>
        <location filename="themestrings.h" line="174"/>
        <source>No files are selected for DVD</source>
        <translation type="unfinished">Non ci sono file selezionati per il DVD</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="177"/>
        <source>Not Applicable</source>
        <translation type="unfinished">Non applicabile</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="178"/>
        <source>Not Available in this Theme</source>
        <translation type="unfinished">Non disponibile in questo tema</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="179"/>
        <source>Not available in this theme</source>
        <translation type="unfinished">Non disponibile in questo tema</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="184"/>
        <source>Original Size:</source>
        <translation type="unfinished">Dimensione originale:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="186"/>
        <source>Overwrite DVD-RW Media:</source>
        <translation type="unfinished">Sovrascrivi media DVD-RW:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="188"/>
        <source>Parental Level</source>
        <translation type="unfinished">Livello parentale</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="189"/>
        <source>Parental Level:</source>
        <translation type="unfinished">Livello parentale:</translation>
    </message>
    <message numerus="yes">
        <location filename="themestrings.h" line="191"/>
        <source>Parental Level: %n</source>
        <translation type="unfinished">
            <numerusform>Livello parentale: %n</numerusform>
            <numerusform>Livelli parentali: %n</numerusform>
        </translation>
    </message>
    <message>
        <location filename="themestrings.h" line="194"/>
        <source>Play the last created archive DVD</source>
        <translation type="unfinished">Riproduci ultimo DVD archivio creato</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="195"/>
        <source>Position View:</source>
        <translation type="unfinished">Vista posizione:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="196"/>
        <source>Press &apos;left&apos; and &apos;right&apos; arrows to move through the frames. Press &apos;up&apos; and &apos;down&apos; arrows to change seek amount. Press &apos;select&apos; or &apos;enter&apos; key to lock the selected thumb image.</source>
        <translation type="unfinished">Premi le frecce &quot;sinistra&quot; e &quot;destra&quot; per spostarti tra i frame. Premi le frecce &quot;su&quot; e &quot;giù&quot; per modificare l&apos;entità della ricerca. Premi il tasto &quot;seleziona&quot; o &quot;invio&quot; per bloccare l&apos;immagine dell&apos;icona selezionata.</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="198"/>
        <source>Preview</source>
        <translation type="unfinished">Anteprima</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="201"/>
        <source>Read video from data dvd or file</source>
        <translation type="unfinished">Leggi video da dvd o file dati</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="203"/>
        <source>Recordings</source>
        <translation type="unfinished">Registrazioni</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="204"/>
        <source>Recordings group:</source>
        <translation type="unfinished">Gruppo registrazioni:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="206"/>
        <source>Save recordings and videos to a native archive</source>
        <translation type="unfinished">Salva registrazioni e video in un archivio originale</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="207"/>
        <source>Save recordings and videos to video DVD</source>
        <translation type="unfinished">Salva registrazioni e video su DVD video</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="209"/>
        <source>Search Chan ID</source>
        <translation type="unfinished">Cerca ID Can</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="210"/>
        <source>Search Chan NO</source>
        <translation type="unfinished">Cerca N. Can</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="212"/>
        <source>Search Local</source>
        <translation type="unfinished">Cerca localmente</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="216"/>
        <source>Seek Amount:</source>
        <translation type="unfinished">Entità spostamento:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="217"/>
        <source>Seek amount:</source>
        <translation type="unfinished">Entità spostamento:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="220"/>
        <source>Select Channel</source>
        <translation type="unfinished">Seleziona canale</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="224"/>
        <source>Select Files</source>
        <translation type="unfinished">Selesiona file</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="225"/>
        <source>Select Recordings</source>
        <translation>Seleziona registrazioni</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="226"/>
        <source>Select Recordings for your archive or image</source>
        <translation type="unfinished">Seleziona registrazioni per l&apos;archivio o immagine</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="228"/>
        <source>Select a destination for your archive or image</source>
        <translation type="unfinished">Seleziona destinazione per l&apos;archivio o immagine</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="233"/>
        <source>Select thumb image:</source>
        <translation type="unfinished">Seleziona immagine icona:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="234"/>
        <source>Select your DVD menu theme</source>
        <translation type="unfinished">Seleziona tema del menu del DVD</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="235"/>
        <source>Selected Items to be archived</source>
        <translation type="unfinished">Elementi selezionati da archiviare</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="236"/>
        <source>Selected Items to be burned</source>
        <translation type="unfinished">Elementi selezionati da masterizzare</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="237"/>
        <source>Selected recording item size: %1</source>
        <translation type="unfinished">Dimensioni della registrazione selezionata: %1</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="238"/>
        <source>Selected video item size: %1</source>
        <translation type="unfinished">Dimensioni del video selezionato: %1</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="239"/>
        <source>Sep 13, 2004 11:00 pm (1h 15m)</source>
        <translation type="unfinished">Set 13, 2004 11:00 pm (1h 15m)</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="240"/>
        <source>Show</source>
        <translation type="unfinished">Mostra</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="241"/>
        <source>Show Recordings</source>
        <translation>Mostra registrazioni</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="293"/>
        <source>~</source>
        <translation type="unfinished">~</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="227"/>
        <source>Select Videos</source>
        <translation>Seleziona video</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="242"/>
        <source>Show Videos</source>
        <translation type="unfinished">Mostra video</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="243"/>
        <source>Show log with archive activities</source>
        <translation type="unfinished">Mostra log attività di archiviazione</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="244"/>
        <source>Show the Archive Log Viewer</source>
        <translation type="unfinished">Mostra visualizzatore log dell&apos;archivio</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="245"/>
        <source>Size:</source>
        <translation type="unfinished">Dimensione:</translation>
    </message>
    <message numerus="yes">
        <location filename="themestrings.h" line="246"/>
        <source>Size: %n</source>
        <translation type="unfinished">
            <numerusform>Dimensione: %n</numerusform>
            <numerusform>Dimensioni: %n</numerusform>
        </translation>
    </message>
    <message>
        <location filename="themestrings.h" line="249"/>
        <source>Start Time: %1</source>
        <translation type="unfinished">Ora inizio: %1</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="250"/>
        <source>Start to</source>
        <translation type="unfinished">Inizia a</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="251"/>
        <source>Start to Burn your archive to DVD</source>
        <translation type="unfinished">Inizia a masterizzare l&apos;archivio su DVD</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="253"/>
        <source>Subtitle: %1</source>
        <translation type="unfinished">Sottotitolo: %1</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="254"/>
        <source>Test</source>
        <translation type="unfinished">Test</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="255"/>
        <source>Test created</source>
        <translation type="unfinished">Test creato</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="256"/>
        <source>Test created DVD</source>
        <translation type="unfinished">Prova il DVD creato</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="257"/>
        <source>Theme description</source>
        <translation type="unfinished">Descrizione tema</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="258"/>
        <source>Theme for the DVD menu:</source>
        <translation type="unfinished">Tema del menu del DVD:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="259"/>
        <source>Theme preview:</source>
        <translation type="unfinished">Anteprima tema:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="264"/>
        <source>Title: %1</source>
        <translation type="unfinished">Titolo: %1</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="265"/>
        <source>Tools and Utilities for archives</source>
        <translation type="unfinished">Strumenti e utilità per gli archivi</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="268"/>
        <source>Use archive</source>
        <translation type="unfinished">Usa archivio</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="269"/>
        <source>Used: %1</source>
        <translation type="unfinished">Usato: %1</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="270"/>
        <source>Utilities</source>
        <translation type="unfinished">Utilità</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="271"/>
        <source>Utilities for MythArchive</source>
        <translation type="unfinished">Utilità per MythArchive</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="273"/>
        <source>Video Category:</source>
        <translation>Categoria video:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="274"/>
        <source>Video category:</source>
        <translation type="unfinished">Categoria video:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="275"/>
        <source>Videos</source>
        <translation type="unfinished">Video</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="276"/>
        <source>View progress of your archive or image</source>
        <translation type="unfinished">Mostra stato di avanzamento dell&apos;archivio o immagine</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="277"/>
        <source>Write video to data dvd or file</source>
        <translation type="unfinished">Scrivi video su dvd o file dati</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="278"/>
        <source>XML File to Import</source>
        <translation type="unfinished">File XML da importare</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="279"/>
        <source>decrease seek amount</source>
        <translation type="unfinished">riduci entità ricerca</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="281"/>
        <source>frame</source>
        <translation type="unfinished">frame</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="282"/>
        <source>increase seek amount</source>
        <translation type="unfinished">aumenta entità ricerca</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="283"/>
        <source>move left</source>
        <translation type="unfinished">muovi a sinistra</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="284"/>
        <source>move right</source>
        <translation type="unfinished">muovi a destra</translation>
    </message>
    <message numerus="yes">
        <location filename="themestrings.h" line="285"/>
        <source>profile: %n</source>
        <translation type="unfinished">
            <numerusform>profilo: %n</numerusform>
            <numerusform>profili: %n</numerusform>
        </translation>
    </message>
    <message>
        <location filename="themestrings.h" line="286"/>
        <source>sep 13, 2004 11:00 pm (1h 15m)</source>
        <translation type="unfinished">set 13, 2004 11:00 pm (1h 15m)</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="288"/>
        <source>to</source>
        <translation type="unfinished">a</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="289"/>
        <source>x.xx GB</source>
        <translation>x.xx GB</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="181"/>
        <source>Ok</source>
        <translation>OK</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="272"/>
        <source>Video Category</source>
        <translation>Categoria video</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="287"/>
        <source>title goes here</source>
        <translation>il titolo va qui</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="187"/>
        <source>PL:</source>
        <translation>PL:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="15"/>
        <source>1</source>
        <translation>1</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="290"/>
        <source>x.xx Gb</source>
        <translation>x.xx GB</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="64"/>
        <source>Change Encoding Profile</source>
        <translation>Cambia profilo encoder</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="16"/>
        <source>12.34 GB</source>
        <translation>12.34GB</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="95"/>
        <source>DVD Menu Theme</source>
        <translation>Tema menù DVD</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="77"/>
        <source>Choose the appearance of your DVD.</source>
        <translation>Scegli l&apos;aspetto del tuo DVD.</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="260"/>
        <source>Theme:</source>
        <translation>Tema:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="145"/>
        <source>Intro</source>
        <translation>Introduzione</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="157"/>
        <source>Main Menu</source>
        <translation>Menù principale</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="73"/>
        <source>Chapter Menu</source>
        <translation>Menù capitoli</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="102"/>
        <source>Details</source>
        <translation>Dettagli</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="218"/>
        <source>Select Archive Items</source>
        <translation>Seleziona oggetti dall&apos;archivio</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="232"/>
        <source>Select the recordings and videos you wish to save.</source>
        <translation>Seleziona le registrazioni e i video che si desideri salvare.</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="175"/>
        <source>No files are selected for archive</source>
        <translation>Non ci sono file selezionati per l&apos;archivio</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="25"/>
        <source>Add Recording</source>
        <translation>Agg.Registraz.</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="27"/>
        <source>Add Video</source>
        <translation>Aggiungi video</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="24"/>
        <source>Add File</source>
        <translation>Aggiungi file</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="11"/>
        <source>0 mb</source>
        <translation>0 mb</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="292"/>
        <source>xxxxx mb</source>
        <translation>xxxxx mb</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="37"/>
        <source>Archive Item Details</source>
        <translation>Dettagli oggetto dell&apos;archivio</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="263"/>
        <source>Title:</source>
        <translation>Titolo:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="252"/>
        <source>Subtitle:</source>
        <translation>Sottotitolo:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="247"/>
        <source>Start Date:</source>
        <translation>Data iniziale:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="262"/>
        <source>Time:</source>
        <translation>Ora:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="97"/>
        <source>Description:</source>
        <translation>Descrizione:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="261"/>
        <source>Thumb Image Selector</source>
        <translation>Selettore anteprima</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="86"/>
        <source>Current Position</source>
        <translation>Posizione corrente</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="14"/>
        <source>0:00:00.00</source>
        <translation>0:00:00.00</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="215"/>
        <source>Seek Amount</source>
        <translation>Campo ricerca</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="137"/>
        <source>Frame</source>
        <translation>Frame</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="205"/>
        <source>Save</source>
        <translation>Salva</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="229"/>
        <source>Select a theme</source>
        <translation>Seleziona un tema</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="291"/>
        <source>xxxxx MB</source>
        <translation>xxxxx MB</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="131"/>
        <source>Find</source>
        <translation>Trova</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="10"/>
        <source>0 MB</source>
        <translation>0 MB</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="197"/>
        <source>Prev</source>
        <translation>Preced</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="124"/>
        <source>File Finder To Import</source>
        <translation>Trova file da importare</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="248"/>
        <source>Start Time:</source>
        <translation>Inizio ora:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="219"/>
        <source>Select Associated Channel</source>
        <translation>Seleziona canale associato</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="45"/>
        <source>Archived Channel</source>
        <translation>Canale archiviato</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="60"/>
        <source>Chan. ID:</source>
        <translation>ID can.:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="62"/>
        <source>Chan. No:</source>
        <translation>N. can.:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="56"/>
        <source>Callsign:</source>
        <translation>Nome della stazione:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="167"/>
        <source>Name:</source>
        <translation>Nome:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="151"/>
        <source>Local Channel</source>
        <translation>Canale locale</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="211"/>
        <source>Search Channel</source>
        <translation>Cerca canale</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="208"/>
        <source>Search Callsign</source>
        <translation>Cerca nome stazione</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="213"/>
        <source>Search Name</source>
        <translation>Cerca nome</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="133"/>
        <source>Finish</source>
        <translation>Finito</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="222"/>
        <source>Select Destination:</source>
        <translation>Seleziona destinazione:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="192"/>
        <source>Parental level: %1</source>
        <translation>Livello parentale: %1</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="183"/>
        <source>Old size:</source>
        <translation>Vecchia dimensione:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="171"/>
        <source>New size:</source>
        <translation>Nuova dimensione:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="230"/>
        <source>Select a theme:</source>
        <translation>Seleziona tema:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="164"/>
        <source>Menu</source>
        <translation>Menù</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="72"/>
        <source>Chapter</source>
        <translation>Capitolo</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="101"/>
        <source>Detail</source>
        <translation>Dettagli</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="223"/>
        <source>Select File to Import</source>
        <translation>Seleziona file da importare</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="31"/>
        <source>Add video</source>
        <translation>Aggiungi video</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="129"/>
        <source>Filesize: %1</source>
        <translation>Dimensione file: %1</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="202"/>
        <source>Recorded Time: %1</source>
        <translation>Tempo registrato: %1</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="7"/>
        <source>%&quot;|SUBTITLE|&quot; %%(|ORIGINALAIRDATE|) %%(|STARS|) %%DESCRIPTION%</source>
        <translation>%&quot;|SUBTITLE|&quot; %%(|ORIGINALAIRDATE|) %%(|STARS|) %%DESCRIPTION%</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="190"/>
        <source>Parental Level: %1</source>
        <translation>Livello parentale: %1</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="39"/>
        <source>Archive Items</source>
        <translation>Archivio oggetti</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="200"/>
        <source>Profile:</source>
        <translation>Profilo:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="182"/>
        <source>Old Size:</source>
        <translation>Vecchia dimensione:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="170"/>
        <source>New Size:</source>
        <translation>Nuova dimensione:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="66"/>
        <source>Channel ID:</source>
        <translation>ID canale:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="70"/>
        <source>Channel Number:</source>
        <translation>Numero canale:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="81"/>
        <source>Create DVD</source>
        <translation>Crea DVD</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="80"/>
        <source>Create Archive</source>
        <translation>Crea archivio</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="109"/>
        <source>Encode Video File</source>
        <translation>Codifica file video</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="141"/>
        <source>Import Archive</source>
        <translation>Importa archivio</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="44"/>
        <source>Archive Utilities</source>
        <translation>Utilità archivio</translation>
    </message>
    <message>
        <source>Show Log Viewer</source>
        <translation type="vanished">Mostra visualizzatore log</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="193"/>
        <source>Play Created DVD</source>
        <translation>Riproduci DVD creato</translation>
    </message>
    <message>
        <source>Burn DVD</source>
        <translation type="vanished">Mastrerizza DVD</translation>
    </message>
</context>
<context>
    <name>ThumbFinder</name>
    <message>
        <location filename="../mytharchive/thumbfinder.cpp" line="868"/>
        <source>Menu</source>
        <translation>Menù</translation>
    </message>
    <message>
        <location filename="../mytharchive/thumbfinder.cpp" line="875"/>
        <source>Exit, Save Thumbnails</source>
        <translation>Esci, salva anteprime</translation>
    </message>
    <message>
        <location filename="../mytharchive/thumbfinder.cpp" line="876"/>
        <source>Exit, Don&apos;t Save Thumbnails</source>
        <translation>Esci, non salvare anteprime</translation>
    </message>
</context>
<context>
    <name>VideoSelector</name>
    <message>
        <location filename="../mytharchive/videoselector.cpp" line="141"/>
        <source>Menu</source>
        <translation>Menù</translation>
    </message>
    <message>
        <location filename="../mytharchive/videoselector.cpp" line="148"/>
        <source>Clear All</source>
        <translation>Cancella tutto</translation>
    </message>
    <message>
        <location filename="../mytharchive/videoselector.cpp" line="149"/>
        <source>Select All</source>
        <translation>Seleziona tutto</translation>
    </message>
    <message>
        <location filename="../mytharchive/videoselector.cpp" line="331"/>
        <location filename="../mytharchive/videoselector.cpp" line="499"/>
        <source>All Videos</source>
        <translation>Tutti i video</translation>
    </message>
    <message>
        <location filename="../mytharchive/videoselector.cpp" line="552"/>
        <source>You need to enter a valid password for this parental level</source>
        <translation>Devi inserire una password valida per questo livello parentale</translation>
    </message>
</context>
</TS>
