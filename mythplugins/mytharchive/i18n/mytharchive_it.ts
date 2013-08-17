<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE TS>
<TS version="2.0" language="it_IT">
<context>
    <name>(ArchiveUtils)</name>
    <message>
        <location filename="../mytharchive/archiveutil.cpp" line="80"/>
        <source>Cannot find the MythArchive work directory.
Have you set the correct path in the settings?</source>
        <translation>Impossibile trovare la directory di lavoro di MythArchive.
È stato impostato il percorso corretto nelle impostazioni?</translation>
    </message>
</context>
<context>
    <name>(MythArchiveMain)</name>
    <message>
        <location filename="../mytharchive/main.cpp" line="93"/>
        <source>Found a lock file but the owning process isn&apos;t running!
Removing stale lock file.</source>
        <translation>Trovato un file bloccato ma il processo proprietario non è in esecuzione!
Rimuove lo stato di blocco del file.</translation>
    </message>
    <message>
        <location filename="../mytharchive/main.cpp" line="212"/>
        <source>Last run did not create a playable DVD.</source>
        <translation>L&apos;ultima esecuzione non ha creato un DVD riproducibile.</translation>
    </message>
    <message>
        <location filename="../mytharchive/main.cpp" line="219"/>
        <source>Last run failed to create a DVD.</source>
        <translation>L&apos;ultima esecuzione ha fallito la creazione di un DVD.</translation>
    </message>
</context>
<context>
    <name>ArchiveFileSelector</name>
    <message>
        <location filename="../mytharchive/importnative.cpp" line="212"/>
        <source>Find File To Import</source>
        <translation>Trovare file da importare</translation>
    </message>
    <message>
        <location filename="../mytharchive/importnative.cpp" line="272"/>
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
        <translation>Percorso dove MythArchive dovrebbe creare i suoi file di lavoro temporanei. Richiesto molto spazio libero su disco.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="33"/>
        <source>MythArchive Share Directory</source>
        <translation>Directory condivisa da MythArchive</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="36"/>
        <source>Location where MythArchive stores its scripts, intro movies and theme files</source>
        <translation>Percorso dove MythArchive archivia i suioi script, filmati introduttivi e file di tema</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="46"/>
        <source>Video format</source>
        <translation>Formato video</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="51"/>
        <source>Video format for DVD recordings, PAL or NTSC.</source>
        <translation>Formato video per registrazioni DVD, PAL o NTSC.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="60"/>
        <source>File Selector Filter</source>
        <translation>Filtro del selettore del file</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="63"/>
        <source>The file name filter to use in the file selector.</source>
        <translation>Il nome del file del filtro usato nel selettore dei file.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="72"/>
        <source>Location of DVD</source>
        <translation>Percorso del DVD</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="75"/>
        <source>Which DVD drive to use when burning discs.</source>
        <translation>Quale unità DVD da usare quando si masterizzano dischi.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="84"/>
        <source>DVD Drive Write Speed</source>
        <translation>Velocità di scrittura dell&apos;unità DVD</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="87"/>
        <source>This is the write speed to use when burning a DVD. Set to 0 to allow growisofs to choose the fastest available speed.</source>
        <translation>Questa è la velocità di scrittura da usare quando si masterizza un DVD. Impostare a 0 permette a growisofs di scegliere la velocità disponibile più veloce.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="98"/>
        <source>Command to play DVD</source>
        <translation>Comando per riprodurre DVD</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="101"/>
        <source>Command to run when test playing a created DVD. &apos;Internal&apos; will use the internal MythTV player. %f will be replaced with the path to the created DVD structure eg. &apos;xine -pfhq --no-splash dvd:/%f&apos;.</source>
        <translation>Comando da eseguire quando si testa la riproduzione di un DVD creato &apos;Interno&apos;userà il riproduttore interno MythTV. %f sarà sostituito con il percorso della strutura del DVD creato es. &apos;xine -pfhq --no-splash dvd:/%f&apos;.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="114"/>
        <source>Copy remote files</source>
        <translation>Copiare i file remoti</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="117"/>
        <source>If set files on remote filesystems will be copied over to the local filesystem before processing. Speeds processing and reduces bandwidth on the network</source>
        <translation>Se impostato i file sul filesystem remoto saranno copiati sul filesystem locali prima dell&apos;elaborazione. La velocità dell&apos; elaborazione riduce la larghezza di banda sulla rete</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="129"/>
        <source>Always Use Mythtranscode</source>
        <translation>Usare sempre Mythtranscode</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="132"/>
        <source>If set mpeg2 files will always be passed though mythtranscode to clean up any errors. May help to fix some audio problems. Ignored if &apos;Use ProjectX&apos; is set.</source>
        <translation>Se impostato i file mpeg2 saranno sempre passato tramite mythtranscode per pulire ogni errore. Può aiutare a fissare molti problemi audio. Ignorato se è impostato &quot;Usare ProjectX&quot;.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="144"/>
        <source>Use ProjectX</source>
        <translation>Usare ProjectX</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="147"/>
        <source>If set ProjectX will be used to cut commercials and split mpeg2 files instead of mythtranscode and mythreplex.</source>
        <translation>Se impostato ProjectX verrà usato per tagliare pubblicità e dividere file mpeg2 invece di mythtranscode e mythreplex.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="158"/>
        <source>Use FIFOs</source>
        <translation>Usare FIFOs</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="161"/>
        <source>The script will use FIFOs to pass the output of mplex into dvdauthor rather than creating intermediate files. Saves time and disk space during multiplex operations but not supported on Windows platform</source>
        <translation>Lo script userà FIFOs per passare l&apos;uscita di mplex dentro dvdauthor piuttosto che creare dei file intermedi.Risparmia tempo e spazio su disco durante le operazioni di multiplex ma non supportati dalla piattaforma Windows</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="174"/>
        <source>Add Subtitles</source>
        <translation>Aggiungere sottotitoli</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="177"/>
        <source>If available this option will add subtitles to the final DVD. Requires &apos;Use ProjectX&apos; to be on.</source>
        <translation>Se disponibile questa opzione aggiungerà sottotitoli al DVD finale. Richiede di essere su &quot;Usare ProjectX&quot;.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="187"/>
        <source>Main Menu Aspect Ratio</source>
        <translation>Menù principale proporzione</translation>
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
        <translation>Formato da usare quando si crea il menù principale.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="203"/>
        <source>Chapter Menu Aspect Ratio</source>
        <translation>Formato menù capitoli</translation>
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
        <translation>Formato da usare quando si crea il menù capitoli.&quot;%1&quot; significa usare lo stesso formato come nel video associato.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="223"/>
        <source>Date format</source>
        <translation>Formato data</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="226"/>
        <source>Samples are shown using today&apos;s date.</source>
        <translation>I campioni sono mostrati usando la data di oggi.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="232"/>
        <source>Samples are shown using tomorrow&apos;s date.</source>
        <translation>I campioni sono mostrati usando la data di domani.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="250"/>
        <source>Your preferred date format to use on DVD menus. %1</source>
        <extracomment>%1 gives additional info on the date used</extracomment>
        <translation>Formaro data preferita da usare su i menù DVD. %1</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="259"/>
        <source>Time format</source>
        <translation>Formato ora</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="266"/>
        <source>Your preferred time format to display on DVD menus. You must choose a format with &quot;AM&quot; or &quot;PM&quot; in it, otherwise your time display will be 24-hour or &quot;military&quot; time.</source>
        <translation>Formato ora preferito da visualizzare su i menù DVD. Scegliere un formato con &quot;AM&quot; o &quot;PM&quot;, altrimenti l&apos;ora visualizzata sarà di 24 ore o ora &quot;militare&quot;.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="277"/>
        <source>Default Encoder Profile</source>
        <translation>Profilo di codificazione preferito</translation>
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
        <translation>Profilo di codificazione preferito da usare se un file necessita di ricodifica.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="295"/>
        <source>mplex Command</source>
        <translation>Comando mplex</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="299"/>
        <source>Command to run mplex</source>
        <translation>Comando per eseguire mplex</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="308"/>
        <source>dvdauthor command</source>
        <translation>Comando dvdauthor</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="312"/>
        <source>Command to run dvdauthor.</source>
        <translation>Comando per eseguire dvdauthor.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="321"/>
        <source>mkisofs command</source>
        <translation>Comando mkisofs</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="325"/>
        <source>Command to run mkisofs. (Used to create ISO images)</source>
        <translation>Comando per eseguire mkisofs. (Usato per creare immagini ISO)</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="334"/>
        <source>growisofs command</source>
        <translation>Comando growisofs</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="338"/>
        <source>Command to run growisofs. (Used to burn DVDs)</source>
        <translation>Comando per eseguire growisofs. (Usato per masterizzare DVD)</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="347"/>
        <source>M2VRequantiser command</source>
        <translation>Comando M2VRequantiser</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="351"/>
        <source>Command to run M2VRequantiser. Optional - leave blank if you don&apos;t have M2VRequantiser installed.</source>
        <translation>Comando per eseguire M2VRequantiser. Opzionale - lasciare vuoto se non si ha M2VRequantiser installato.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="361"/>
        <source>jpeg2yuv command</source>
        <translation>Comando jpeg2yuv</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="365"/>
        <source>Command to run jpeg2yuv. Part of mjpegtools package</source>
        <translation>Comando per eseguire jpeg2yuv. Parte del pacchetto mjpegtools</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="374"/>
        <source>spumux command</source>
        <translation>Comando spumux</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="378"/>
        <source>Command to run spumux. Part of dvdauthor package</source>
        <translation>Comando per eseguire spumux. Parte del pacchetto dvdauthor</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="387"/>
        <source>mpeg2enc command</source>
        <translation>Comando mpeg2enc</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="391"/>
        <source>Command to run mpeg2enc. Part of mjpegtools package</source>
        <translation>Comando per eseguire mpeg2enc. Parte del pacchetto mjpegtools</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="400"/>
        <source>projectx command</source>
        <translation>Comando projectx</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="404"/>
        <source>Command to run ProjectX. Will be used to cut commercials and split mpegs files instead of mythtranscode and mythreplex.</source>
        <translation>Comando usato per eseguire ProjectX. Sarà usato per tagliare pubblicità e dividere file mpeg2 invece di mythtranscode e mythreplex.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="414"/>
        <source>MythArchive Settings</source>
        <translation>Impostazioni MythArchive</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="425"/>
        <source>MythArchive Settings (2)</source>
        <translation>Impostazioni MythArchive (2)</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="435"/>
        <source>DVD Menu Settings</source>
        <translation>Impostazioni menù DVD</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="443"/>
        <source>MythArchive External Commands (1)</source>
        <translation>Comandi esterni MythArchive (1)</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="451"/>
        <source>MythArchive External Commands (2)</source>
        <translation>Comandi esterni MythArchive (2)</translation>
    </message>
</context>
<context>
    <name>BurnMenu</name>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="1155"/>
        <source>Cannot burn a DVD.
The last run failed to create a DVD.</source>
        <translation>Impossibile masterizzare un DVD.
L&apos;ultima esecuzione ha fallito la creazione di un DVD.</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="1161"/>
        <location filename="../mytharchive/mythburn.cpp" line="1173"/>
        <source>Burn DVD</source>
        <translation>Masterizzare DVD</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="1162"/>
        <source>
Place a blank DVD in the drive and select an option below.</source>
        <translation>
Collocare un DVD vergine nell&apos;unità e selezionare un opzione sotto.</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="1174"/>
        <source>Burn DVD Rewritable</source>
        <translation>Masterizzare DVD riscrivibile</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="1175"/>
        <source>Burn DVD Rewritable (Force Erase)</source>
        <translation>Masterizzare DVD riscrivibile (forzare cancellazione)</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="1231"/>
        <source>It was not possible to run mytharchivehelper to burn the DVD.</source>
        <translation>Non è stato possibile eseguire mytharchivehelper per masterizzare il DVD.</translation>
    </message>
</context>
<context>
    <name>BurnThemeUI</name>
    <message>
        <location filename="../mythburn/themes/burnthemestrings.h" line="5"/>
        <source>Has an intro and contains a main menu with 4 recordings per page. Does not have a chapter selection submenu.</source>
        <translation>Ha un introduzione e contiene un menù principale con 4 registrazioni per pagina. Non ha un menù secondario di selezione del capitolo.</translation>
    </message>
    <message>
        <location filename="../mythburn/themes/burnthemestrings.h" line="6"/>
        <source>Has an intro and contains a summary main menu with 10 recordings per page. Does not have a chapter selection submenu, recording titles, dates or category.</source>
        <translation>Ha un introduzione e contiene un sommario del menù principale con 10 registrazioni per pagina. Non ha un menù secondario di selezione del capitolo, titoli di registrazione, date o categorie.</translation>
    </message>
    <message>
        <location filename="../mythburn/themes/burnthemestrings.h" line="7"/>
        <source>Has an intro and contains a main menu with 6 recordings per page. Does not have a scene selection submenu.</source>
        <translation>Ha un introduzione e contiene un menù principale con 6 registrazioni per pagina. Non ha un menù secondario di selezione del capitolo.</translation>
    </message>
    <message>
        <location filename="../mythburn/themes/burnthemestrings.h" line="8"/>
        <source>Has an intro and contains a main menu with 3 recordings per page and a scene selection submenu with 8 chapters points. Shows a program details page before each recording.</source>
        <translation>Ha un introduzione e contiene un menù principale con 3 registrazioni per pagina e un menù secondario di selezione scene con 8 punti capitolo. Mostrare la pagina dei dettagli del programma prima di ogni registrazione.</translation>
    </message>
    <message>
        <location filename="../mythburn/themes/burnthemestrings.h" line="9"/>
        <source>Has an intro and contains a main menu with 3 recordings per page and a scene selection submenu with 8 chapters points. Shows a program details page before each recording. Uses animated thumb images.</source>
        <translation>Ha un introduzione e contiene un menù principale con 3 registrazioni per pagina e un menù secondario di selezione scene con 8 punti capitolo. Mostrare la pagina dei dettagli del programma prima di ogni registrazione. Usare immagini animate e miniaturizzate.</translation>
    </message>
    <message>
        <location filename="../mythburn/themes/burnthemestrings.h" line="10"/>
        <source>Has an intro and contains a main menu with 3 recordings per page and a scene selection submenu with 8 chapters points.</source>
        <translation>Ha un introduzione e contiene un menù principale con 3 registrazioni per pagina e un menù secondario di selezione scene con 8 punti capitolo.</translation>
    </message>
    <message>
        <location filename="../mythburn/themes/burnthemestrings.h" line="11"/>
        <source>Has an intro and contains a main menu with 3 recordings per page and a scene selection submenu with 8 chapters points. All the thumb images are animated.</source>
        <translatorcomment>Ha un introduzione e contiene un menù principale con 3 registrazioni per pagina e un menù secondario di selezione scene con 8 punti capitolo. Tutte le imagini miniaturizzate sono animate.</translatorcomment>
        <translation>Has an intro and contains a main menu with 3 recordings per page and a scene selection submenu with 8 chapters points. All the thumb images are animated.</translation>
    </message>
    <message>
        <location filename="../mythburn/themes/burnthemestrings.h" line="12"/>
        <source>Creates an auto play DVD with no menus. Shows an intro movie then for each title shows a details page followed by the video in sequence.</source>
        <translation>Creare un DVD con riproduzione automatica senza menù. Mostrare un video introduttivo quindi per ogni titolo mostare una pagina di dettagli seguito dal video in sequenza.</translation>
    </message>
    <message>
        <location filename="../mythburn/themes/burnthemestrings.h" line="13"/>
        <source>Creates an auto play DVD with no menus and no intro.</source>
        <translation>Creare un DVD con riproduzione automatica senza menù e introduzione.</translation>
    </message>
</context>
<context>
    <name>DVDThemeSelector</name>
    <message>
        <location filename="../mytharchive/themeselector.cpp" line="219"/>
        <location filename="../mytharchive/themeselector.cpp" line="230"/>
        <source>No theme description file found!</source>
        <translation>Nessun file di descrizione del tema trovato!</translation>
    </message>
    <message>
        <location filename="../mytharchive/themeselector.cpp" line="243"/>
        <source>Empty theme description!</source>
        <translation>Riempire la descrizione del tema!</translation>
    </message>
    <message>
        <location filename="../mytharchive/themeselector.cpp" line="248"/>
        <source>Unable to open theme description file!</source>
        <translation>Impossibile aprire il file di descrizione del tema!</translation>
    </message>
</context>
<context>
    <name>ExportNative</name>
    <message>
        <location filename="../mytharchive/exportnative.cpp" line="242"/>
        <source>You need to add at least one item to archive!</source>
        <translation>È richiesto l&apos;aggiunta di almeno un oggetto nell&apos;archivio!</translation>
    </message>
    <message>
        <location filename="../mytharchive/exportnative.cpp" line="393"/>
        <source>Menu</source>
        <translation>Menù</translation>
    </message>
    <message>
        <location filename="../mytharchive/exportnative.cpp" line="400"/>
        <source>Remove Item</source>
        <translation>Rimuovere oggetti</translation>
    </message>
    <message>
        <location filename="../mytharchive/exportnative.cpp" line="495"/>
        <source>It was not possible to create the DVD. An error occured when running the scripts</source>
        <translation>Non è stato possibile creare il DVD. Un errore è occorso quando gli script sono stati in esecuzione</translation>
    </message>
    <message>
        <location filename="../mytharchive/exportnative.cpp" line="531"/>
        <source>You don&apos;t have any videos!</source>
        <translation>Non si ha nessun video!</translation>
    </message>
</context>
<context>
    <name>FileSelector</name>
    <message>
        <location filename="../mytharchive/fileselector.cpp" line="81"/>
        <source>Find File</source>
        <translation>Trovare file</translation>
    </message>
    <message>
        <location filename="../mytharchive/fileselector.cpp" line="84"/>
        <source>Find Directory</source>
        <translation>Trovare directory</translation>
    </message>
    <message>
        <location filename="../mytharchive/fileselector.cpp" line="87"/>
        <source>Find Files</source>
        <translation>Trovare i file</translation>
    </message>
    <message>
        <location filename="../mytharchive/fileselector.cpp" line="321"/>
        <source>The selected item is not a directory!</source>
        <translation>L&apos;oggetto selezionato non è una directory!</translation>
    </message>
</context>
<context>
    <name>ImportNative</name>
    <message>
        <location filename="../mytharchive/importnative.cpp" line="433"/>
        <source>You need to select a valid channel id!</source>
        <translation>È richiesto la selezione di un id canale valido!</translation>
    </message>
    <message>
        <location filename="../mytharchive/importnative.cpp" line="464"/>
        <source>It was not possible to import the Archive.  An error occured when running &apos;mytharchivehelper&apos;</source>
        <translation>Non è possibile impostare l&apos;archivio. Si è verificato un errore durante l&apos;esecuzione di &quot;mytharchivehelper&quot;</translation>
    </message>
    <message>
        <location filename="../mytharchive/importnative.cpp" line="594"/>
        <source>Select a channel id</source>
        <translation>Selezionare un id canale</translation>
    </message>
    <message>
        <location filename="../mytharchive/importnative.cpp" line="620"/>
        <source>Select a channel number</source>
        <translation>Selezionare un numero canale</translation>
    </message>
    <message>
        <location filename="../mytharchive/importnative.cpp" line="646"/>
        <source>Select a channel name</source>
        <translation>Selezionare un nome canale</translation>
    </message>
    <message>
        <location filename="../mytharchive/importnative.cpp" line="672"/>
        <source>Select a Callsign</source>
        <translation>Selezionare nome delle stazione</translation>
    </message>
</context>
<context>
    <name>LogViewer</name>
    <message>
        <location filename="../mytharchive/logviewer.cpp" line="77"/>
        <source>Cannot find any logs to show!</source>
        <translation>Impossibile trovare ogni log da visualizzare!</translation>
    </message>
    <message>
        <location filename="../mytharchive/logviewer.cpp" line="216"/>
        <source>Background creation has been asked to stop.
This may take a few minutes.</source>
        <translation>La creazione sullo sfondo è stato richiesto di fermarsi.
Questo può richiedere alcuni minuti.</translation>
    </message>
    <message>
        <location filename="../mytharchive/logviewer.cpp" line="355"/>
        <source>Menu</source>
        <translation>Menù</translation>
    </message>
    <message>
        <location filename="../mytharchive/logviewer.cpp" line="363"/>
        <source>Don&apos;t Auto Update</source>
        <translation>Nessun aggiornamento automatico</translation>
    </message>
    <message>
        <location filename="../mytharchive/logviewer.cpp" line="365"/>
        <source>Auto Update</source>
        <translation>Aggiornamento automatico</translation>
    </message>
    <message>
        <location filename="../mytharchive/logviewer.cpp" line="367"/>
        <source>Show Progress Log</source>
        <translation>Mostrare log di progresso</translation>
    </message>
    <message>
        <location filename="../mytharchive/logviewer.cpp" line="368"/>
        <source>Show Full Log</source>
        <translation>Mostrare log completo</translation>
    </message>
</context>
<context>
    <name>MythBurn</name>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="356"/>
        <location filename="../mytharchive/mythburn.cpp" line="480"/>
        <source>No Cut List</source>
        <translation>Nessun elenco taglia</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="367"/>
        <source>You need to add at least one item to archive!</source>
        <translation>È richiesto l&apos;aggiunta di almeno un oggetto nell&apos;archivio!</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="413"/>
        <source>Retrieving File Information. Please Wait...</source>
        <translation>Recupero informazioni sui file. Attendere...</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="483"/>
        <source>Encoder: </source>
        <translation>Codificatore:</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="809"/>
        <source>Menu</source>
        <translation>Menù</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="345"/>
        <location filename="../mytharchive/mythburn.cpp" line="469"/>
        <source>Using Cut List</source>
        <translation>Usare elenco taglia</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="350"/>
        <location filename="../mytharchive/mythburn.cpp" line="474"/>
        <source>Not Using Cut List</source>
        <translation>Non usare elenco taglia</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="820"/>
        <source>Don&apos;t Use Cut List</source>
        <translation>Non usare elenco taglia</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="823"/>
        <source>Use Cut List</source>
        <translation>Usare elenco taglia</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="827"/>
        <source>Remove Item</source>
        <translation>Rimuovere oggetti</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="828"/>
        <source>Edit Details</source>
        <translation>Modifica dettagli</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="829"/>
        <source>Change Encoding Profile</source>
        <translation>Cambia profilo decodificazione</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="830"/>
        <source>Edit Thumbnails</source>
        <translation>Modifica anteprima</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="965"/>
        <source>It was not possible to create the DVD.  An error occured when running the scripts</source>
        <translation>Non è stato possibile creare il DVD, Un errore è occorso quando gli script sono stati in esecuzione</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="1008"/>
        <source>You don&apos;t have any videos!</source>
        <translation>Non si ha nessun video!</translation>
    </message>
</context>
<context>
    <name>MythControls</name>
    <message>
        <location filename="../mytharchive/main.cpp" line="302"/>
        <source>Toggle use cut list state for selected program</source>
        <translation>Attivare/Disattivare stato dell&apos;elenco lista tagli per il programma selezionato</translation>
    </message>
    <message>
        <location filename="../mytharchive/main.cpp" line="305"/>
        <source>Create DVD</source>
        <translation>Creare DVD</translation>
    </message>
    <message>
        <location filename="../mytharchive/main.cpp" line="307"/>
        <source>Create Archive</source>
        <translation>Creare archivio</translation>
    </message>
    <message>
        <location filename="../mytharchive/main.cpp" line="309"/>
        <source>Import Archive</source>
        <translation>Importare archivio</translation>
    </message>
    <message>
        <location filename="../mytharchive/main.cpp" line="311"/>
        <source>View Archive Log</source>
        <translation>Mostrare log archivio</translation>
    </message>
    <message>
        <location filename="../mytharchive/main.cpp" line="313"/>
        <source>Play Created DVD</source>
        <translation>Riproduzione DVD creato</translation>
    </message>
    <message>
        <location filename="../mytharchive/main.cpp" line="315"/>
        <source>Burn DVD</source>
        <translation>Masterizzare DVD</translation>
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
        <location filename="../mytharchive/recordingselector.cpp" line="112"/>
        <location filename="../mytharchive/recordingselector.cpp" line="420"/>
        <location filename="../mytharchive/recordingselector.cpp" line="525"/>
        <source>All Recordings</source>
        <translation>Tutte le registrazioni</translation>
    </message>
    <message>
        <location filename="../mytharchive/recordingselector.cpp" line="133"/>
        <source>Retrieving Recording List.
Please Wait...</source>
        <translation>Recupero elenco registrazioni.
Attendere...</translation>
    </message>
    <message>
        <location filename="../mytharchive/recordingselector.cpp" line="157"/>
        <source>Either you don&apos;t have any recordings or no recordings are available locally!</source>
        <translation>O non si dispone di nessuna registrazione o le registrazioni non sono disponibile localmente!</translation>
    </message>
    <message>
        <location filename="../mytharchive/recordingselector.cpp" line="206"/>
        <source>Menu</source>
        <translation>Menù</translation>
    </message>
    <message>
        <location filename="../mytharchive/recordingselector.cpp" line="213"/>
        <source>Clear All</source>
        <translation>Pulisci tutto</translation>
    </message>
    <message>
        <location filename="../mytharchive/recordingselector.cpp" line="214"/>
        <source>Select All</source>
        <translation>Selezionare tutto</translation>
    </message>
</context>
<context>
    <name>SelectDestination</name>
    <message>
        <location filename="../mytharchive/archiveutil.cpp" line="35"/>
        <source>Single Layer DVD</source>
        <translation>DVD singolo strato</translation>
    </message>
    <message>
        <location filename="../mytharchive/archiveutil.cpp" line="39"/>
        <source>Dual Layer DVD</source>
        <translation>DVD doppio strato</translation>
    </message>
    <message>
        <location filename="../mytharchive/archiveutil.cpp" line="36"/>
        <source>Single Layer DVD (4,482 MB)</source>
        <translation>DVD singolo strato (4,482 MB)</translation>
    </message>
    <message>
        <location filename="../mytharchive/archiveutil.cpp" line="40"/>
        <source>Dual Layer DVD (8,964 MB)</source>
        <translation>DVD doppio strato (8,964 MB)</translation>
    </message>
    <message>
        <location filename="../mytharchive/archiveutil.cpp" line="43"/>
        <source>DVD +/- RW</source>
        <translation>DVD +/- RW</translation>
    </message>
    <message>
        <location filename="../mytharchive/archiveutil.cpp" line="44"/>
        <source>Rewritable DVD</source>
        <translation>DVD rescrivibile</translation>
    </message>
    <message>
        <location filename="../mytharchive/archiveutil.cpp" line="47"/>
        <source>File</source>
        <translation>File</translation>
    </message>
    <message>
        <location filename="../mytharchive/archiveutil.cpp" line="48"/>
        <source>Any file accessable from your filesystem.</source>
        <translation>Tutti file accesibili dal filesystem.</translation>
    </message>
    <message>
        <location filename="../mytharchive/selectdestination.cpp" line="294"/>
        <location filename="../mytharchive/selectdestination.cpp" line="350"/>
        <source>Unknown</source>
        <translation>Sconosciuto</translation>
    </message>
</context>
<context>
    <name>ThemeUI</name>
    <message>
        <location filename="themestrings.h" line="5"/>
        <source>A high quality profile giving approx. 1 hour of video on a single layer DVD</source>
        <translation>Un profilo ad altà qualità dando approsimativamente 1 ora di video su un DVD singolo strato</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="6"/>
        <source>A standard play profile giving approx. 2 hour of video on a single layer DVD</source>
        <translation>Un profilo con riproduzione standard dando approsimativamente 2 ore di video su un DVD singolo strato</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="7"/>
        <source>A long play profile giving approx. 4 hour of video on a single layer DVD</source>
        <translation>Un profilo con lunga riproduzione approsimativamente 4 ore di video su un DVD singolo strato</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="8"/>
        <source>A extended play profile giving approx. 6 hour of video on a single layer DVD</source>
        <translation>Un profilo con riproduzione estesa approsimativamente 6 ore di video su un DVD singolo strato</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="70"/>
        <source>%DATE%, %TIME%</source>
        <translation>%DATE%, %TIME%</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="18"/>
        <source>Select Destination</source>
        <translation>Selezionare destinazione</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="71"/>
        <source>Choose where you would like your files archived.</source>
        <translation>Scegliere dove si desidera archiviare i file.</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="72"/>
        <source>Output Type:</source>
        <translation>Tipo uscita:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="73"/>
        <source>Destination:</source>
        <translation>Destinazione:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="20"/>
        <source>Free Space:</source>
        <translation>Spazio libero:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="74"/>
        <source>Click here to find an output location...</source>
        <translation>Fare click qui per trovare un destinazione d&apos;uscita...</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="21"/>
        <source>Make ISO Image</source>
        <translation>Fare immagine ISO</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="22"/>
        <source>Burn to DVD</source>
        <translation>Mastrerizzare DVD</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="75"/>
        <source>Erase DVD-RW before burning</source>
        <translation>Cancellare DVD-RW prima della masterizzazione</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="15"/>
        <source>Cancel</source>
        <translation>Annullato</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="76"/>
        <source>Previous</source>
        <translation>Precedente</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="25"/>
        <source>Next</source>
        <translation>Prossimo</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="77"/>
        <source>Filter:</source>
        <translation>Filtro:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="28"/>
        <source>OK</source>
        <translation>OK</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="29"/>
        <source>File Finder</source>
        <translation>Trova file</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="78"/>
        <source>Select the file you wish to use.</source>
        <translation>Selezionare il file che si desidera utilizzare.</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="16"/>
        <source>Back</source>
        <translation>Indietro</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="30"/>
        <source>Home</source>
        <translation>Home</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="34"/>
        <source>No videos available</source>
        <translation>Nessun video disponibile</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="35"/>
        <source>Log Viewer</source>
        <translation>Visualizzatore log</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="79"/>
        <source>See logs from your archive runs.</source>
        <translation>Vedere i log dagli archivi eseguiti.</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="36"/>
        <source>Update</source>
        <translation>Aggiornare</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="37"/>
        <source>Exit</source>
        <translation>Uscire</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="85"/>
        <source>Up Level</source>
        <translation>Livello su</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="96"/>
        <source>description goes here.</source>
        <translation>la descrizione va messa qui.</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="86"/>
        <source>0.00 GB</source>
        <translation>0.00 GB</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="23"/>
        <source>Force Overwrite of DVD-RW Media</source>
        <translation>Forzare la sovrascrittura di un supporto DVD-RW</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="26"/>
        <source>Select Recordings</source>
        <translation>Selezionare le registrazioni</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="27"/>
        <source>Show Recordings</source>
        <translation>Mostrare le registrazioni</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="31"/>
        <source>Select Videos</source>
        <translation>Video selezionato</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="97"/>
        <source>Video Category:</source>
        <translation>Categoria video:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="102"/>
        <source>sep 13, 2004 11:00 pm (1h 15m)</source>
        <translation>sett 13, 2004 11:00 pm (1h 15m)</translation>
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
        <translation>Categoria video</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="98"/>
        <source>title goes here</source>
        <translation>i titoli vanno qui</translation>
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
        <translation>Cambia profilo codificazione</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="80"/>
        <source>12.34 GB</source>
        <translation>12.34GB</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="39"/>
        <source>DVD Menu Theme</source>
        <translation>Tema menù DVD</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="81"/>
        <source>Choose the appearance of your DVD.</source>
        <translation>Scegliere l&apos;aspetto del DVD.</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="82"/>
        <source>Theme:</source>
        <translation>Tema:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="41"/>
        <source>Intro</source>
        <translation>Introduzione</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="42"/>
        <source>Main Menu</source>
        <translation>Menù principale</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="43"/>
        <source>Chapter Menu</source>
        <translation>Menù capitoli</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="44"/>
        <source>Details</source>
        <translation>Dettagli</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="45"/>
        <source>Select Archive Items</source>
        <translation>Selezionare oggetti dall&apos;archivio</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="83"/>
        <source>Select the recordings and videos you wish to save.</source>
        <translation>Selezionare le registrazioni e i video che si desidera salvare.</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="9"/>
        <source>No files are selected for archive</source>
        <translation>Non ci sono file selezionati per l&apos;archivio</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="10"/>
        <source>Add Recording</source>
        <translation>Aggiungere registrazioni</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="11"/>
        <source>Add Video</source>
        <translation>Aggiungere video</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="12"/>
        <source>Add File</source>
        <translation>Aggiungere file</translation>
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
        <location filename="themestrings.h" line="46"/>
        <source>Archive Item Details</source>
        <translation>Dettagli oggetto dell&apos;archivio</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="47"/>
        <source>Title:</source>
        <translation>Titolo:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="48"/>
        <source>Subtitle:</source>
        <translation>Sottotitolo:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="49"/>
        <source>Start Date:</source>
        <translation>Data iniziale:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="50"/>
        <source>Time:</source>
        <translation>Ora:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="51"/>
        <source>Description:</source>
        <translation>Descrizione:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="52"/>
        <source>Thumb Image Selector</source>
        <translation>Selettore dell&apos;anteprima</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="53"/>
        <source>Current Position</source>
        <translation>Posizione corrente</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="84"/>
        <source>0:00:00.00</source>
        <translation>0:00:00.00</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="54"/>
        <source>Seek Amount</source>
        <translation>Ricerca totale</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="55"/>
        <source>Frame</source>
        <translation>Frame</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="56"/>
        <source>Save</source>
        <translation>Salvare</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="40"/>
        <source>Select a theme</source>
        <translation>Selezionare un tema</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="100"/>
        <source>xxxxx MB</source>
        <translation>xxxxx MB</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="19"/>
        <source>Find</source>
        <translation>Trovare</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="101"/>
        <source>0 MB</source>
        <translation>0 MB</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="24"/>
        <source>Prev</source>
        <translation>Preced</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="58"/>
        <source>File Finder To Import</source>
        <translation>Trova file da importare</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="59"/>
        <source>Start Time:</source>
        <translation>Inizio ora:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="60"/>
        <source>Select Associated Channel</source>
        <translation>Selezionare canale associato</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="61"/>
        <source>Archived Channel</source>
        <translation>Canale archiviato</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="62"/>
        <source>Chan. ID:</source>
        <translation>ID can.:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="63"/>
        <source>Chan. No:</source>
        <translation>N. can.:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="64"/>
        <source>Callsign:</source>
        <translation>Nome della stazione:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="65"/>
        <source>Name:</source>
        <translation>Nome:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="66"/>
        <source>Local Channel</source>
        <translation>Canale locale</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="67"/>
        <source>Search Channel</source>
        <translation>Ricercare canale</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="68"/>
        <source>Search Callsign</source>
        <translation>Ricercare nome della stazione</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="69"/>
        <source>Search Name</source>
        <translation>Ricerca nome</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="17"/>
        <source>Finish</source>
        <translation>Finito</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="105"/>
        <source>Select Destination:</source>
        <translation>Selezionare destinazione:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="106"/>
        <source>Parental level: %1</source>
        <translation>Controllo genitori: %1</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="107"/>
        <source>Old size:</source>
        <translation>Vecchia dimensione:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="108"/>
        <source>New size:</source>
        <translation>Nuova dimensione:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="109"/>
        <source>Select a theme:</source>
        <translation>Selezionare un tema:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="110"/>
        <source>Menu</source>
        <translation>Menù</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="111"/>
        <source>Chapter</source>
        <translation>Capitolo</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="112"/>
        <source>Detail</source>
        <translation>Dettagli</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="113"/>
        <source>Select File to Import</source>
        <translation>Selezionare file da importare</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="57"/>
        <source>Add video</source>
        <translation>Aggiungere video</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="87"/>
        <source>Filesize: %1</source>
        <translation>Dimensione file: %1</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="88"/>
        <source>Recorded Time: %1</source>
        <translation>Tempo registrato: %1</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="89"/>
        <source>%&quot;|SUBTITLE|&quot; %%(|ORIGINALAIRDATE|) %%(|STARS|) %%DESCRIPTION%</source>
        <translation>%&quot;|SUBTITLE|&quot; %%(|ORIGINALAIRDATE|) %%(|STARS|) %%DESCRIPTION%</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="91"/>
        <source>Parental Level: %1</source>
        <translation>Controllo genitori: %1</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="92"/>
        <source>Archive Items</source>
        <translation>Archivio oggetti</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="93"/>
        <source>Profile:</source>
        <translation>Profilo:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="94"/>
        <source>Old Size:</source>
        <translation>Vecchia dimensione:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="95"/>
        <source>New Size:</source>
        <translation>Nuova dimensione:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="114"/>
        <source>Channel ID:</source>
        <translation>ID canale:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="115"/>
        <source>Channel Number:</source>
        <translation>Numero canale:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="116"/>
        <source>Create DVD</source>
        <translation>Creare DVD</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="117"/>
        <source>Create Archive</source>
        <translation>Creare archivio</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="118"/>
        <source>Encode Video File</source>
        <translation>Codificare file video</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="119"/>
        <source>Import Archive</source>
        <translation>Importare archivio</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="120"/>
        <source>Archive Utilities</source>
        <translation>Utilità dell&apos;archivio</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="121"/>
        <source>Show Log Viewer</source>
        <translation>Mostrare visualizzatore log</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="122"/>
        <source>Play Created DVD</source>
        <translation>Riproduzione DVD creato</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="123"/>
        <source>Burn DVD</source>
        <translation>Mastrerizzare DVD</translation>
    </message>
</context>
<context>
    <name>ThumbFinder</name>
    <message>
        <location filename="../mytharchive/thumbfinder.cpp" line="921"/>
        <source>Menu</source>
        <translation>Menù</translation>
    </message>
    <message>
        <location filename="../mytharchive/thumbfinder.cpp" line="928"/>
        <source>Exit, Save Thumbnails</source>
        <translation>Uscire, salvare le anteprime</translation>
    </message>
    <message>
        <location filename="../mytharchive/thumbfinder.cpp" line="929"/>
        <source>Exit, Don&apos;t Save Thumbnails</source>
        <translation>Uscire, non salvare le anteprime</translation>
    </message>
</context>
<context>
    <name>VideoSelector</name>
    <message>
        <location filename="../mytharchive/videoselector.cpp" line="153"/>
        <source>Menu</source>
        <translation>Menù</translation>
    </message>
    <message>
        <location filename="../mytharchive/videoselector.cpp" line="160"/>
        <source>Clear All</source>
        <translation>Pulisci tutto</translation>
    </message>
    <message>
        <location filename="../mytharchive/videoselector.cpp" line="161"/>
        <source>Select All</source>
        <translation>Selezionare tutto</translation>
    </message>
    <message>
        <location filename="../mytharchive/videoselector.cpp" line="359"/>
        <location filename="../mytharchive/videoselector.cpp" line="489"/>
        <source>All Videos</source>
        <translation>Tutti i video</translation>
    </message>
    <message>
        <location filename="../mytharchive/videoselector.cpp" line="546"/>
        <source>You need to enter a valid password for this parental level</source>
        <translation>È richiesto l&apos;inserimento di una password valida per questo controllo genitori</translation>
    </message>
</context>
</TS>
