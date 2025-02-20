<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE TS>
<TS version="2.1" language="el_GR" sourcelanguage="en_US">
<context>
    <name>(ArchiveUtils)</name>
    <message>
        <location filename="../mytharchive/archiveutil.cpp" line="52"/>
        <source>Cannot find the MythArchive work directory.
Have you set the correct path in the settings?</source>
        <translation>Δεν βρίσκω το φάκελο εργασίας του MythArchive.
Έχετε βάλει τη σωστή διαδρομή στις ρυθμίσεις;</translation>
    </message>
</context>
<context>
    <name>(MythArchiveMain)</name>
    <message>
        <location filename="../mytharchive/mytharchive.cpp" line="100"/>
        <source>Found a lock file but the owning process isn&apos;t running!
Removing stale lock file.</source>
        <translation>Βρήκα ένα κλειδωμένο αρχείο αλλά η γονική διεργασία δεν τρέχει!
Αφαιρώ το ξεχασμένο αρχείο κλειδώματος.</translation>
    </message>
    <message>
        <location filename="../mytharchive/mytharchive.cpp" line="211"/>
        <source>Last run did not create a playable DVD.</source>
        <translation>Η τελευταία εκτέλεση δεν δημιούργησε καλό DVD.</translation>
    </message>
    <message>
        <location filename="../mytharchive/mytharchive.cpp" line="218"/>
        <source>Last run failed to create a DVD.</source>
        <translation>Η τελευταία εκτέλεση απέτυχε να δημιουργήσει DVD.</translation>
    </message>
</context>
<context>
    <name>ArchiveFileSelector</name>
    <message>
        <location filename="../mytharchive/importnative.cpp" line="205"/>
        <source>Find File To Import</source>
        <translation>Εύρεση Αρχείου για Εισαγωγή</translation>
    </message>
    <message>
        <location filename="../mytharchive/importnative.cpp" line="265"/>
        <source>The selected item is not a valid archive file!</source>
        <translation>Το επιλεγμένο αρχείο δεν είναι έγκυρο αρχείο συμπίεσης!</translation>
    </message>
</context>
<context>
    <name>ArchiveSettings</name>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="20"/>
        <source>MythArchive Temp Directory</source>
        <translation>Φάκελος Προσωρινής Αποθήκευσης</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="23"/>
        <source>Location where MythArchive should create its temporary work files. LOTS of free space required here.</source>
        <translation>Θέση όπου το MythArchive μπορεί να δημιουργήσει τα προσωρινά του αρχεία. Χρειάζεται ΠΟΛΥΣ χώρος.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="35"/>
        <source>MythArchive Share Directory</source>
        <translation>Κοινόχρηστος Φάκελος</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="38"/>
        <source>Location where MythArchive stores its scripts, intro movies and theme files</source>
        <translation>Θέση όπου το MythArchive αποθηκεύει τα scripts, τις εισαγωγικές ταινίες και τα αρχεία θεμάτων</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="50"/>
        <source>Video format</source>
        <translation>Πρότυπο ταινίας</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="55"/>
        <source>Video format for DVD recordings, PAL or NTSC.</source>
        <translation>Το πρότυπο για εγγραφές DVD: PAL ή NTSC.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="64"/>
        <source>File Selector Filter</source>
        <translation>Φίλτρο Επιλογής Αρχείων</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="67"/>
        <source>The file name filter to use in the file selector.</source>
        <translation>Το φίλτρο προς χρήση κατά την επιλογή αρχείων.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="76"/>
        <source>Location of DVD</source>
        <translation>Συσκευή DVD</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="79"/>
        <source>Which DVD drive to use when burning discs.</source>
        <translation>Ποιά συσκευή DVD θα χρησιμοποιείται για την εγγραφή των δίσκων.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="89"/>
        <source>DVD Drive Write Speed</source>
        <translation>Ταχύτητα Εγγραφής DVD</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="92"/>
        <source>This is the write speed to use when burning a DVD. Set to 0 to allow growisofs to choose the fastest available speed.</source>
        <translation>Είναι η ταχύτητα εγγραφής των DVD. Δώστε 0 για να επιτρέψετε στο growisofs να επιλέξει την ταχύτερη διαθέσιμη.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="103"/>
        <source>Command to play DVD</source>
        <translation>Εντολή προβολής DVD</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="106"/>
        <source>Command to run when test playing a created DVD. &apos;Internal&apos; will use the internal MythTV player. %f will be replaced with the path to the created DVD structure eg. &apos;xine -pfhq --no-splash dvd:/%f&apos;.</source>
        <translation>Η εντολή που θα δοκιμάζει την αναπαραγωγή ενός δημιουργημένου DVD. Δίνοντας &apos;Internal&apos; θα χρησιμοποιείται το ενσωματωμένο πρόγραμμα του MythTV. To %f θα αντικατασταθεί με τη διαδρομή προς τη δομή του δημιουργημένου DVD. πχ  &apos;xine -pfhq --no-splash dvd:/%f&apos;.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="119"/>
        <source>Copy remote files</source>
        <translation>Αντιγραφή απομακρυσμένων αρχείων</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="122"/>
        <source>If set files on remote filesystems will be copied over to the local filesystem before processing. Speeds processing and reduces bandwidth on the network</source>
        <translation>Αν επιλεγεί, τα αρχεία από απομακρυσμένα συστήματα αρχείων θα αντιγραφούν στο τοπικό σύστημα πριν από την επεξεργασία. Επιταχύνει την διαδικασία και μειώνει το χρησιμοποιούμενο εύρος δικτύου</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="134"/>
        <source>Always Use Mythtranscode</source>
        <translation>Πάντα χρήση του Mythtranscode</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="137"/>
        <source>If set mpeg2 files will always be passed though mythtranscode to clean up any errors. May help to fix some audio problems. Ignored if &apos;Use ProjectX&apos; is set.</source>
        <translation>Αν επιλεγεί, τα αρχεία mpeg2 πάντα θα περνούν από το mythtranscode για καθαρισμό τυχόν σφαλμάτων. Πιθανώς να διορθώσει μερικά προβλήματα ήχου. Αγνοείται αν επιλέξετε &apos;Χρήση ProjectX&apos;.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="149"/>
        <source>Use ProjectX</source>
        <translation>Χρήση ProjectX</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="152"/>
        <source>If set ProjectX will be used to cut commercials and split mpeg2 files instead of mythtranscode and mythreplex.</source>
        <translation>Αν επιλεγεί, για το κόψιμο των διαφημίσεων και το χωρισμό των mpeg2 αρχείων θα χρησιμοποιείται το ProjectX αντί των mythtranscode και mythreplex.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="163"/>
        <source>Use FIFOs</source>
        <translation>Χρήση FIFOs</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="166"/>
        <source>The script will use FIFOs to pass the output of mplex into dvdauthor rather than creating intermediate files. Saves time and disk space during multiplex operations but not supported on Windows platform</source>
        <translation>Το σκριπτάκι θα χρησιμοποιήσει FIFOsγια να στείλει την έξοδο του mplex στο dvdauthor αντί να δημιουργήσει ενδιάμεσα αρχεία. Γλυτώνει χρόνο και χώρο στο δίσκο αλλά δεν υποστηρίζεται στην πλατφόρμα Windows</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="179"/>
        <source>Add Subtitles</source>
        <translation>Προσθήκη Υποτίτλων</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="182"/>
        <source>If available this option will add subtitles to the final DVD. Requires &apos;Use ProjectX&apos; to be on.</source>
        <translation>Αν είναι διαθέσιμοι, η επιλογή θα τους προσθέσει στο τελικό DVD. Απαιτεί τη χρήση του ProjectX.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="192"/>
        <source>Main Menu Aspect Ratio</source>
        <translation>Λόγος Πλευρών Κεντρικών Επιλογών</translation>
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
        <translation>Ο λόγος πλευρών για το κεντρικό μενού του DVD.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="208"/>
        <source>Chapter Menu Aspect Ratio</source>
        <translation>Λόγος Πλευρών Μενού Κεφαλαίων</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="212"/>
        <location filename="../mytharchive/archivesettings.cpp" line="221"/>
        <source>Video</source>
        <translation>Χωρίς αλλαγή</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="217"/>
        <source>Aspect ratio to use when creating the chapter menu. &apos;%1&apos; means use the same aspect ratio as the associated video.</source>
        <extracomment>%1 is the translation of the &quot;Video&quot; combo box choice</extracomment>
        <translation>Ο λόγος πλευρών που θα χρησιμοποιηθεί κατά τη δημιουργία των επιλογών κεφαλαίων. Το &apos;%1&apos; σημαίνει πως θα χρησιμοποιηθεί αυτός που βρέθηκε στην αρχική ταινία.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="228"/>
        <source>Date format</source>
        <translation>Μορφή Ημερομηνίας</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="231"/>
        <source>Samples are shown using today&apos;s date.</source>
        <translation>Εμφανίζονται δείγματα με βάση τη σημερινή ημερομηνία.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="237"/>
        <source>Samples are shown using tomorrow&apos;s date.</source>
        <translation>Εμφανίζονται δείγματα με βάση την αυριανή ημερομηνία.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="255"/>
        <source>Your preferred date format to use on DVD menus. %1</source>
        <extracomment>%1 gives additional info on the date used</extracomment>
        <translation>Η προτίμησή σας για τη μορφή ημερομηνίας όπως θα εμφανίζεται στις επιλογές του DVD. %1</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="264"/>
        <source>Time format</source>
        <translation>Μορφή Ώρας</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="271"/>
        <source>Your preferred time format to display on DVD menus. You must choose a format with &quot;AM&quot; or &quot;PM&quot; in it, otherwise your time display will be 24-hour or &quot;military&quot; time.</source>
        <translation>Η προτιμώμενη μορφή ώρας για εμφάνιση στις επιλογές του DVD. Πρέπει να επιλέξετε μια μορφή που να περιέχει &quot;AM&quot; ή &quot;PM&quot; αλλιώς θα φαίνεται σε 24ωρη βάση.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="282"/>
        <source>Default Encoder Profile</source>
        <translation>Προεπιλεγμένο Προφίλ Κωδικοποίησης</translation>
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
        <translation>Για την περίπτωση που κάποιο αρχείο χρειάζεται επανακωδικοποίηση.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="300"/>
        <source>mplex Command</source>
        <translation>Εντολή mplex</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="304"/>
        <source>Command to run mplex</source>
        <translation>Η εντολή που θα τρέξει το mplex</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="313"/>
        <source>dvdauthor command</source>
        <translation>Εντολή dvdauthor</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="317"/>
        <source>Command to run dvdauthor.</source>
        <translation>Η εντολή που θα τρέξει το dvdauthor.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="326"/>
        <source>mkisofs command</source>
        <translation>Εντολή mkisofs</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="330"/>
        <source>Command to run mkisofs. (Used to create ISO images)</source>
        <translation>Η εντολή που θα τρέξει το mkisofs (Χρησιμοποιείται για τη δημιουργεία ειδώλων ISO)</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="339"/>
        <source>growisofs command</source>
        <translation>Εντολή growisofs</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="343"/>
        <source>Command to run growisofs. (Used to burn DVDs)</source>
        <translation>Η εντολή που θα τρέξει το growisofs (Χρησιμοποιείται για το γράψιμο DVD)</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="352"/>
        <source>M2VRequantiser command</source>
        <translation>Εντολή M2VRequantiser</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="356"/>
        <source>Command to run M2VRequantiser. Optional - leave blank if you don&apos;t have M2VRequantiser installed.</source>
        <translation>Η εντολή που θα τρέξει το M2VRequantiser. Προαιρετική, αφήστε το κενό αν δεν το έχετε εγκατεστημένο.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="366"/>
        <source>jpeg2yuv command</source>
        <translation>Εντολή jpeg2yuv</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="370"/>
        <source>Command to run jpeg2yuv. Part of mjpegtools package</source>
        <translation>Η εντολή που θα τρέξει το jpeg2yuv. Ανήκει στο πακέτο mjpegtools</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="379"/>
        <source>spumux command</source>
        <translation>Εντολή spumux</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="383"/>
        <source>Command to run spumux. Part of dvdauthor package</source>
        <translation>Η εντολη που θα τρέξει το spumux. Ανήκει στο πακέτο dvdauthor</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="392"/>
        <source>mpeg2enc command</source>
        <translation>Εντολή mpeg2enc</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="396"/>
        <source>Command to run mpeg2enc. Part of mjpegtools package</source>
        <translation>Η εντολή που θα τρέξει το mpeg2enc. Ανήκει στο πακέτο mjpegtools</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="405"/>
        <source>projectx command</source>
        <translation>Εντολή projectx</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="409"/>
        <source>Command to run ProjectX. Will be used to cut commercials and split mpegs files instead of mythtranscode and mythreplex.</source>
        <translation>Η εντολή που θα τρέξει το ProjectX. Χρησιμοποιείται στην αφαίρεση διαφημίσεων και το διαχωρισμό των mpeg αρχείων αντί των mythtranscode και mythreplex.</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="418"/>
        <source>MythArchive Settings</source>
        <translation>Ρυθμίσεις Αρχειοθέτησης</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="442"/>
        <source>MythArchive External Commands</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <source>MythArchive Settings (2)</source>
        <translation type="vanished">Ρυθμίσεις Αρχειοθέτησης (2)</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="434"/>
        <source>DVD Menu Settings</source>
        <translation>Ρυθμίσεις Επιλογών DVD</translation>
    </message>
    <message>
        <source>MythArchive External Commands (1)</source>
        <translation type="vanished">Εξωτερικές εντολές του MythArchive (1)</translation>
    </message>
    <message>
        <source>MythArchive External Commands (2)</source>
        <translation type="vanished">Εξωτερικές εντολές του MythArchive (2)</translation>
    </message>
</context>
<context>
    <name>BurnMenu</name>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="1095"/>
        <source>Cannot burn a DVD.
The last run failed to create a DVD.</source>
        <translation>Δεν μπορώ να γράψω DVD.
Η τελευταία εκτέλεση απέτυχε να δημιουργήσει DVD.</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="1101"/>
        <location filename="../mytharchive/mythburn.cpp" line="1113"/>
        <source>Burn DVD</source>
        <translation>Εγγραφή DVD</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="1102"/>
        <source>
Place a blank DVD in the drive and select an option below.</source>
        <translation>
Τοποθετήστε ένα κενό DVD στη συσκευή και διαλέξτε μια από τις παρακάτω επιλογές.</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="1114"/>
        <source>Burn DVD Rewritable</source>
        <translation>Εγγραφή Επανεγγράψιμου DVD</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="1115"/>
        <source>Burn DVD Rewritable (Force Erase)</source>
        <translation>Εγγραφή Επανεγγράψιμου DVD (Αναγκαστική Διαγραφή)</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="1169"/>
        <source>It was not possible to run mytharchivehelper to burn the DVD.</source>
        <translation>Δεν ήταν δυνατή η εκτέλεση του mytharchivehelper για εγγραφή του DVD.</translation>
    </message>
</context>
<context>
    <name>BurnThemeUI</name>
    <message>
        <location filename="../mythburn/themes/burnthemestrings.h" line="5"/>
        <source>Has an intro and contains a main menu with 4 recordings per page. Does not have a chapter selection submenu.</source>
        <translation>Έχει εισαγωγή και περιέχει κεντρικό μενού με 4 εγγραφές ανά σελίδα. Δεν έχει υπομενού επιλογής κεφαλαίου.</translation>
    </message>
    <message>
        <location filename="../mythburn/themes/burnthemestrings.h" line="6"/>
        <source>Has an intro and contains a summary main menu with 10 recordings per page. Does not have a chapter selection submenu, recording titles, dates or category.</source>
        <translation>Έχει εισαγωγή και περιέχει ένα περιληπτικό κύριο μενού με 10 εγγραφές ανά σελίδα. Δεν έχει υπομενού επιλογής κεφαλαίου, τίτλων εγγραφής ή κατηγορίας.</translation>
    </message>
    <message>
        <location filename="../mythburn/themes/burnthemestrings.h" line="7"/>
        <source>Has an intro and contains a main menu with 6 recordings per page. Does not have a scene selection submenu.</source>
        <translation>Έχει εισαγωγή και περιέχει κεντρικό μενού με 6 εγγραφές ανά σελίδα. Δεν έχει υπομενού επιλογής σκηνής.</translation>
    </message>
    <message>
        <location filename="../mythburn/themes/burnthemestrings.h" line="8"/>
        <source>Has an intro and contains a main menu with 3 recordings per page and a scene selection submenu with 8 chapters points. Shows a program details page before each recording.</source>
        <translation>Έχει εισαγωγή και περιέχει κύριο μενού με 3 εγγραφές ανά σελίδα και υπομενού επιλογής σκηνής με 8 σημεία κεφαλαίων. Εμφανίζει σελίδα λεπτομερειών εκπομπής πριν από κάθε εγγραφή.</translation>
    </message>
    <message>
        <location filename="../mythburn/themes/burnthemestrings.h" line="9"/>
        <source>Has an intro and contains a main menu with 3 recordings per page and a scene selection submenu with 8 chapters points. Shows a program details page before each recording. Uses animated thumb images.</source>
        <translation>Έχει εισαγωγή και περιέχει κύριο μενού με 3 εγγραφές ανά σελίδα και υπομενού επιλογής σκηνής με 8 σημεία κεφαλαίων. Εμφανίζει σελίδα λεπτομερειών εκπομπής πριν από κάθε εγγραφή. Χρησιμοποιεί κινούμενες μικρογραφίες.</translation>
    </message>
    <message>
        <location filename="../mythburn/themes/burnthemestrings.h" line="10"/>
        <source>Has an intro and contains a main menu with 3 recordings per page and a scene selection submenu with 8 chapters points.</source>
        <translation>Έχει εισαγωγή και περιέχει κύριο μενού με 3 εγγραφές ανά σελίδα και υπομενού επιλογής σκηνής με 8 σημεία κεφαλαίων.</translation>
    </message>
    <message>
        <location filename="../mythburn/themes/burnthemestrings.h" line="11"/>
        <source>Has an intro and contains a main menu with 3 recordings per page and a scene selection submenu with 8 chapters points. All the thumb images are animated.</source>
        <translation>Έχει εισαγωγή και περιέχει κύριο μενού με 3 εγγραφές ανά σελίδα και υπομενού επιλογής σκηνής με 8 σημεία κεφαλαίων. Όλες οι εικόνες είναι κινούμενες.</translation>
    </message>
    <message>
        <location filename="../mythburn/themes/burnthemestrings.h" line="12"/>
        <source>Creates an auto play DVD with no menus. Shows an intro movie then for each title shows a details page followed by the video in sequence.</source>
        <translation>Δημιουργεί αυτόματα εκτελέσιμο DVD χωρίς μενού. Εμφανίζει μια εισαγωγική ταινία και για κάθε εκπομπή εμφανίζει σελίδα λεπτομερειών ακολουθούμενο από την εκπομπή.</translation>
    </message>
    <message>
        <location filename="../mythburn/themes/burnthemestrings.h" line="13"/>
        <source>Creates an auto play DVD with no menus and no intro.</source>
        <translation>Δημιουργεί αυτόματα εκτελέσιμο DVD χωρίς μενού και χωρίς εισαγωγή.</translation>
    </message>
</context>
<context>
    <name>DVDThemeSelector</name>
    <message>
        <location filename="../mytharchive/themeselector.cpp" line="204"/>
        <location filename="../mytharchive/themeselector.cpp" line="216"/>
        <source>No theme description file found!</source>
        <translation>Δεν βρέθηκε αρχείο περιγραφής για το θέμα!</translation>
    </message>
    <message>
        <location filename="../mytharchive/themeselector.cpp" line="229"/>
        <source>Empty theme description!</source>
        <translation>Κενή περιγραφή θέματος!</translation>
    </message>
    <message>
        <location filename="../mytharchive/themeselector.cpp" line="234"/>
        <source>Unable to open theme description file!</source>
        <translation>Αδύνατο το άνοιγμα του αρχείου περιγραφής για το θέμα!</translation>
    </message>
</context>
<context>
    <name>ExportNative</name>
    <message>
        <location filename="../mytharchive/exportnative.cpp" line="200"/>
        <source>You need to add at least one item to archive!</source>
        <translation>Πρέπει να προσθέσετε τουλάχιστον ένα αντικείμενο στο αρχείο συμπίεσης!</translation>
    </message>
    <message>
        <location filename="../mytharchive/exportnative.cpp" line="345"/>
        <source>Menu</source>
        <translation>Επιλογές</translation>
    </message>
    <message>
        <location filename="../mytharchive/exportnative.cpp" line="352"/>
        <source>Remove Item</source>
        <translation>Αφαίρεση Αντικειμένου</translation>
    </message>
    <message>
        <location filename="../mytharchive/exportnative.cpp" line="444"/>
        <source>It was not possible to create the DVD. An error occured when running the scripts</source>
        <translation>Αδύνατη η δημιουργία του DVD. Συνέβη σφάλμα κατά την εκτέλεση των scripts</translation>
    </message>
    <message>
        <location filename="../mytharchive/exportnative.cpp" line="480"/>
        <source>You don&apos;t have any videos!</source>
        <translation>Δεν έχετε ταινίες!</translation>
    </message>
</context>
<context>
    <name>FileSelector</name>
    <message>
        <location filename="../mytharchive/fileselector.cpp" line="61"/>
        <source>Find File</source>
        <translation>Εύρεση Αρχείου</translation>
    </message>
    <message>
        <location filename="../mytharchive/fileselector.cpp" line="64"/>
        <source>Find Directory</source>
        <translation>Εύρεση Φακέλου</translation>
    </message>
    <message>
        <location filename="../mytharchive/fileselector.cpp" line="67"/>
        <source>Find Files</source>
        <translation>Εύρεση Αρχείων</translation>
    </message>
    <message>
        <location filename="../mytharchive/fileselector.cpp" line="282"/>
        <source>The selected item is not a directory!</source>
        <translation>Το επιλεγμένο αντικείμενο δεν είναι φάκελος!</translation>
    </message>
</context>
<context>
    <name>ImportNative</name>
    <message>
        <location filename="../mytharchive/importnative.cpp" line="394"/>
        <source>You need to select a valid channel id!</source>
        <translation>Πρέπει να επιλέξετε έγκυρο αναγν. καναλιού!</translation>
    </message>
    <message>
        <location filename="../mytharchive/importnative.cpp" line="425"/>
        <source>It was not possible to import the Archive.  An error occured when running &apos;mytharchivehelper&apos;</source>
        <translation>Αδυναμία εισαγωγής του συμπιεσμένου αρχείου. Συνέβη σφάλμα κατά την εκτέλεση του &apos;mytharchivehelper&apos;</translation>
    </message>
    <message>
        <location filename="../mytharchive/importnative.cpp" line="555"/>
        <source>Select a channel id</source>
        <translation>Επιλογή αναγν. καναλιού</translation>
    </message>
    <message>
        <location filename="../mytharchive/importnative.cpp" line="581"/>
        <source>Select a channel number</source>
        <translation>Επιλογή αρ. καναλιού</translation>
    </message>
    <message>
        <location filename="../mytharchive/importnative.cpp" line="607"/>
        <source>Select a channel name</source>
        <translation>Επιλογή ονόματος καναλιού</translation>
    </message>
    <message>
        <location filename="../mytharchive/importnative.cpp" line="633"/>
        <source>Select a Callsign</source>
        <translation>Επιλογή Ονομασίας Καναλιού</translation>
    </message>
</context>
<context>
    <name>LogViewer</name>
    <message>
        <location filename="../mytharchive/logviewer.cpp" line="76"/>
        <source>Cannot find any logs to show!</source>
        <translation>Δε βρίσκω καταγραφές για να εμφανίσω!</translation>
    </message>
    <message>
        <location filename="../mytharchive/logviewer.cpp" line="191"/>
        <source>Background creation has been asked to stop.
This may take a few minutes.</source>
        <translation>Έχω ζητήσει από τη δημιουργία στο παρασκήνιο να σταματήσει.
Ίσως χρειαστούν μερικά λεπτά.</translation>
    </message>
    <message>
        <location filename="../mytharchive/logviewer.cpp" line="332"/>
        <source>Menu</source>
        <translation>Επιλογές</translation>
    </message>
    <message>
        <location filename="../mytharchive/logviewer.cpp" line="340"/>
        <source>Don&apos;t Auto Update</source>
        <translation>Χωρίς Αυτόματη Ενημέρωση</translation>
    </message>
    <message>
        <location filename="../mytharchive/logviewer.cpp" line="342"/>
        <source>Auto Update</source>
        <translation>Με Αυτόματη Ενημέρωση</translation>
    </message>
    <message>
        <location filename="../mytharchive/logviewer.cpp" line="344"/>
        <source>Show Progress Log</source>
        <translation>Εμφάνιση Καταγραφής Προόδου</translation>
    </message>
    <message>
        <location filename="../mytharchive/logviewer.cpp" line="345"/>
        <source>Show Full Log</source>
        <translation>Εμφάνιση Πλήρους Καταγραφής</translation>
    </message>
</context>
<context>
    <name>MythBurn</name>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="319"/>
        <location filename="../mytharchive/mythburn.cpp" line="441"/>
        <source>Using Cut List</source>
        <translation>Με χρήση Λίστας Κοψιμάτων</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="324"/>
        <location filename="../mytharchive/mythburn.cpp" line="446"/>
        <source>Not Using Cut List</source>
        <translation>Χωρίς χρήση Λίστας Κοψιμάτων</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="330"/>
        <location filename="../mytharchive/mythburn.cpp" line="452"/>
        <source>No Cut List</source>
        <translation>Δεν υπάρχει Λίστα Κοψιμάτων</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="341"/>
        <source>You need to add at least one item to archive!</source>
        <translation>Πρέπει να προσθέσετε τουλάχιστον μία στο αρχείο συμπίεσης!</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="389"/>
        <source>Retrieving File Information. Please Wait...</source>
        <translation>Αναζητώ Στοιχεία για το Αρχείο. Παρακαλώ Περιμένετε...</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="455"/>
        <source>Encoder: </source>
        <translation>Κωδικοποίηση:</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="767"/>
        <source>Menu</source>
        <translation>Επιλογές</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="778"/>
        <source>Don&apos;t Use Cut List</source>
        <translation>Χωρίς χρήση Λίστας Κοψιμάτων</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="783"/>
        <source>Use Cut List</source>
        <translation>Με χρήση Λίστας Κοψιμάτων</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="788"/>
        <source>Remove Item</source>
        <translation>Αφαίρεση Αντικειμένου</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="789"/>
        <source>Edit Details</source>
        <translation>Επεξεργασία Λεπτομερειών</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="790"/>
        <source>Change Encoding Profile</source>
        <translation>Αλλαγή Προφίλ Κωδικοποίησης</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="791"/>
        <source>Edit Thumbnails</source>
        <translation>Επεξεργασία Μικρογραφιών</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="926"/>
        <source>It was not possible to create the DVD.  An error occured when running the scripts</source>
        <translation>Αδύνατη η δημιουργία του DVD. Προκλήθηκε σφάλμα κατά την εκτέλεση των scripts</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="968"/>
        <source>You don&apos;t have any videos!</source>
        <translation>Δεν έχετε ταινίες!</translation>
    </message>
</context>
<context>
    <name>MythControls</name>
    <message>
        <location filename="../mytharchive/mytharchive.cpp" line="329"/>
        <source>Toggle use cut list state for selected program</source>
        <translation>Εναλλαγή χρήσης της λίστας κοψιμάτων για το επιλεγμένο πρόγραμμα</translation>
    </message>
    <message>
        <location filename="../mytharchive/mytharchive.cpp" line="332"/>
        <source>Create DVD</source>
        <translation>Δημιουργία DVD</translation>
    </message>
    <message>
        <location filename="../mytharchive/mytharchive.cpp" line="334"/>
        <source>Create Archive</source>
        <translation>Δημιουργία Αρχείου Συμπίεσης</translation>
    </message>
    <message>
        <location filename="../mytharchive/mytharchive.cpp" line="336"/>
        <source>Import Archive</source>
        <translation>Εισαγωγή Αρχείου Συμπίεσης</translation>
    </message>
    <message>
        <location filename="../mytharchive/mytharchive.cpp" line="338"/>
        <source>View Archive Log</source>
        <translation>Προβολή Καταγραφής Αρχείου Συμπίεσης</translation>
    </message>
    <message>
        <location filename="../mytharchive/mytharchive.cpp" line="340"/>
        <source>Play Created DVD</source>
        <translation>Αναπαραγωγή του δημιουργημένου DVD</translation>
    </message>
    <message>
        <location filename="../mytharchive/mytharchive.cpp" line="342"/>
        <source>Burn DVD</source>
        <translation>Εγγραφή του DVD</translation>
    </message>
</context>
<context>
    <name>RecordingSelector</name>
    <message>
        <location filename="../mytharchive/recordingselector.cpp" line="89"/>
        <location filename="../mytharchive/recordingselector.cpp" line="376"/>
        <location filename="../mytharchive/recordingselector.cpp" line="481"/>
        <source>All Recordings</source>
        <translation>Όλες οι Εγγραφές</translation>
    </message>
    <message>
        <location filename="../mytharchive/recordingselector.cpp" line="110"/>
        <source>Retrieving Recording List.
Please Wait...</source>
        <translation>Ανακτώ τη Λίστα Εγγραφών.
Παρακαλώ Περιμένετε...</translation>
    </message>
    <message>
        <location filename="../mytharchive/recordingselector.cpp" line="134"/>
        <source>Either you don&apos;t have any recordings or no recordings are available locally!</source>
        <translation>Είτε δεν έχετε εγγραφές ή δεν υπάρχει καμία διαθέσιμη τοπικά!</translation>
    </message>
    <message>
        <location filename="../mytharchive/recordingselector.cpp" line="184"/>
        <source>Menu</source>
        <translation>Επιλογές</translation>
    </message>
    <message>
        <location filename="../mytharchive/recordingselector.cpp" line="191"/>
        <source>Clear All</source>
        <translation>Καθαρισμός Όλων</translation>
    </message>
    <message>
        <location filename="../mytharchive/recordingselector.cpp" line="192"/>
        <source>Select All</source>
        <translation>Επιλογή Όλων</translation>
    </message>
</context>
<context>
    <name>SelectDestination</name>
    <message>
        <location filename="../mytharchive/archiveutil.cpp" line="29"/>
        <source>Single Layer DVD</source>
        <translation>DVD Μονής Επίστρωσης</translation>
    </message>
    <message>
        <location filename="../mytharchive/archiveutil.cpp" line="33"/>
        <source>Dual Layer DVD</source>
        <translation>DVD Διπλής Επίστρωσης</translation>
    </message>
    <message>
        <location filename="../mytharchive/archiveutil.cpp" line="30"/>
        <source>Single Layer DVD (4,482 MB)</source>
        <translation>DVD Μονής Επίστρωσης (4.482 MB)</translation>
    </message>
    <message>
        <location filename="../mytharchive/archiveutil.cpp" line="34"/>
        <source>Dual Layer DVD (8,964 MB)</source>
        <translation>DVD Διπλής Επίστρωσης (8.964 MB)</translation>
    </message>
    <message>
        <location filename="../mytharchive/archiveutil.cpp" line="37"/>
        <source>DVD +/- RW</source>
        <translation>DVD +/- RW</translation>
    </message>
    <message>
        <location filename="../mytharchive/archiveutil.cpp" line="38"/>
        <source>Rewritable DVD</source>
        <translation>Επανεγγράψιμο DVD</translation>
    </message>
    <message>
        <location filename="../mytharchive/archiveutil.cpp" line="41"/>
        <source>File</source>
        <translation>Αρχείο</translation>
    </message>
    <message>
        <location filename="../mytharchive/archiveutil.cpp" line="42"/>
        <source>Any file accessable from your filesystem.</source>
        <translation>Κάθε αρχείο προσβάσιμο από το σύστημα αρχείων σας.</translation>
    </message>
    <message>
        <location filename="../mytharchive/selectdestination.cpp" line="264"/>
        <location filename="../mytharchive/selectdestination.cpp" line="320"/>
        <source>Unknown</source>
        <translation>Άγνωστο</translation>
    </message>
</context>
<context>
    <name>ThemeUI</name>
    <message>
        <location filename="themestrings.h" line="20"/>
        <source>A high quality profile giving approx. 1 hour of video on a single layer DVD</source>
        <translation>Ένα υψηλής ποιότητας προφίλ που δίνει περίπου 1 ώρα ταινίας σε μονής επίστρωσης DVD</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="22"/>
        <source>A standard play profile giving approx. 2 hour of video on a single layer DVD</source>
        <translation>Ένα κανονικό προφίλ αναπαραγωγής που παρέχει περίπου 2 ώρες ταινίας σε μονής επίστρωσης DVD</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="21"/>
        <source>A long play profile giving approx. 4 hour of video on a single layer DVD</source>
        <translation>Ένα μεγάλης διάρκειας προφίλ που δίνει περίπου 4 ώρες ταινίας σε μονής επίστρωσης DVD</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="19"/>
        <source>A extended play profile giving approx. 6 hour of video on a single layer DVD</source>
        <translation>Ένα πολύ μεγάλης διάρκειας προφίλ που δίνει περίπου 6 ώρες ταινίας σε μονής επίστρωσης DVD</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="5"/>
        <source>%DATE%, %TIME%</source>
        <translation>%DATE%, %TIME%</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="178"/>
        <source>Select Destination</source>
        <translation>Επιλέξτε Προορισμό</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="65"/>
        <source>Choose where you would like your files archived.</source>
        <translation>Διαλέξτε πού θα θέλατε να αποθηκευτούν τα αρχεία σας.</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="151"/>
        <source>Output Type:</source>
        <translation>Τύπος Εξόδου:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="82"/>
        <source>Destination:</source>
        <translation>Προορισμός:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="114"/>
        <source>Free Space:</source>
        <translation>Ελεύθερος Χώρος:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="66"/>
        <source>Click here to find an output location...</source>
        <translation>Πατήστε εδώ για να βρείτε τοποθεσία εξόδου...</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="129"/>
        <source>Make ISO Image</source>
        <translation>Δημιουργία Ειδώλου ISO</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="44"/>
        <source>Burn to DVD</source>
        <translation>Εγγραφή σε DVD</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="94"/>
        <source>Erase DVD-RW before burning</source>
        <translation>Διαγραφή DVD-RW πριν την εγγραφή</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="49"/>
        <source>Cancel</source>
        <translation>Ακύρωση</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="164"/>
        <source>Previous</source>
        <translation>Προηγούμενο</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="140"/>
        <source>Next</source>
        <translation>Επόμενο</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="107"/>
        <source>Filter:</source>
        <translation>Φίλτρο:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="146"/>
        <source>OK</source>
        <translation>OK</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="102"/>
        <source>File Finder</source>
        <translation>Εύρεση Αρχείου</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="186"/>
        <source>Select the file you wish to use.</source>
        <translation>Επιλέξτε το αρχείο που θέλετε να χρησιμοποιήσετε.</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="40"/>
        <source>Back</source>
        <translation>Πίσω</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="115"/>
        <source>Home</source>
        <translation>Αρχικό</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="143"/>
        <source>No videos available</source>
        <translation>Δεν υπάρχουν ταινίες</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="125"/>
        <source>Log Viewer</source>
        <translation>Προβολή Καταγραφής</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="173"/>
        <source>See logs from your archive runs.</source>
        <translation>Δείτε τις καταγραφές από τις αρχειοθετήσεις σας.</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="219"/>
        <source>Update</source>
        <translation>Ενημέρωση</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="96"/>
        <source>Exit</source>
        <translation>Έξοδος</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="218"/>
        <source>Up Level</source>
        <translation>Επίπεδο Πάνω</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="229"/>
        <source>description goes here.</source>
        <translation>εδώ μπαίνει η περιγραφή.</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="108"/>
        <source>Find</source>
        <translation>Εύρεση</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="6"/>
        <source>%SIZE% ~ %PROFILE%</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="8"/>
        <source>%size% (%profile%)</source>
        <oldsource>%date% / %profile%</oldsource>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="8"/>
        <source>%size% (%profile%)</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="11"/>
        <source>0.00 GB</source>
        <translation>0.00 GB</translation>
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
        <location filename="themestrings.h" line="31"/>
        <source>Archive</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="33"/>
        <source>Archive Item Details</source>
        <oldsource>Archive Callsign:</oldsource>
        <translation type="unfinished">Λεπτομέρειες Στοιχείου</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="34"/>
        <source>Archive Items</source>
        <oldsource>Archive Chan ID:</oldsource>
        <translation type="unfinished">Αρχειοθέτηση Στοιχείων</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="35"/>
        <source>Archive Items to DVD</source>
        <oldsource>Archive Chan No:</oldsource>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="32"/>
        <source>Archive Files</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="38"/>
        <source>Archived Channel</source>
        <oldsource>Archive Item:</oldsource>
        <translation type="unfinished">Αρχειοθετημένο Κανάλι</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="35"/>
        <source>Archive Items to DVD</source>
        <translation type="unfinished"></translation>
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
        <location filename="themestrings.h" line="41"/>
        <source>Burn</source>
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
        <location filename="themestrings.h" line="70"/>
        <source>Create a</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="71"/>
        <source>Create a DVD of your videos</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="73"/>
        <source>Current Position:</source>
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
        <location filename="themestrings.h" line="75"/>
        <source>DVD</source>
        <translation type="unfinished"></translation>
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
        <location filename="themestrings.h" line="79"/>
        <source>Date (time)</source>
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
        <translation type="unfinished">Επεξεργασία Λεπτομερειών</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="87"/>
        <source>Edit Thumbnails</source>
        <translation type="unfinished">Επεξεργασία Μικρογραφιών</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="88"/>
        <source>Eject your</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="90"/>
        <source>Encode your video</source>
        <translation type="unfinished"></translation>
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
        <location filename="themestrings.h" line="98"/>
        <source>Export your</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="99"/>
        <source>Export your videos</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="100"/>
        <source>File</source>
        <translation type="unfinished">Αρχείο</translation>
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
        <translation type="unfinished">Κύριο Μενού</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="105"/>
        <source>Filesize</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="109"/>
        <source>Find Location</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="111"/>
        <source>First, select the thumb image you want to change from the overview. Second, press the &apos;tab&apos; key to move to the select thumb frame button to change the image.</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="112"/>
        <source>Force Overwrite of DVD-RW Media</source>
        <translation>Επιβολή Διαγραφής DVD-RW</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="116"/>
        <source>Import</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="118"/>
        <source>Import an</source>
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
        <location filename="themestrings.h" line="124"/>
        <source>Log</source>
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
        <location filename="themestrings.h" line="131"/>
        <source>Media</source>
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
    <message>
        <location filename="themestrings.h" line="136"/>
        <source>Navigation</source>
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
        <location filename="themestrings.h" line="154"/>
        <source>Parental Level</source>
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
        <location filename="themestrings.h" line="182"/>
        <source>Select Recordings</source>
        <translation>Επιλογή Εγγραφών</translation>
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
        <location filename="themestrings.h" line="192"/>
        <source>Show</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="193"/>
        <source>Show Recordings</source>
        <translation>Προβολή Εγγραφών</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="239"/>
        <source>~</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="183"/>
        <source>Select Videos</source>
        <translation>Επιλογή Ταινιών</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="225"/>
        <source>Video Category:</source>
        <translation>Κατηγορία Ταινιών:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="233"/>
        <source>title goes here</source>
        <translation>εδώ μπαίνει ο τίτλος</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="153"/>
        <source>PL:</source>
        <translation>ΓΕ:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="14"/>
        <source>1</source>
        <translation>1</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="194"/>
        <source>Show Videos</source>
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
        <location filename="themestrings.h" line="202"/>
        <source>Start to</source>
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
        <location filename="themestrings.h" line="207"/>
        <source>Test created</source>
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
        <location filename="themestrings.h" line="220"/>
        <source>Use archive</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="221"/>
        <source>Used: %1</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="222"/>
        <source>Utilities</source>
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
        <translation>Κατηγορία Ταινιών</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="226"/>
        <source>Videos</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="227"/>
        <source>Write video to data dvd or file</source>
        <translation type="unfinished"></translation>
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
        <location filename="themestrings.h" line="55"/>
        <source>Change Encoding Profile</source>
        <translation>Αλλαγή Προφίλ Εγγραφής</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="15"/>
        <source>12.34 GB</source>
        <translation>12.34 GB</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="78"/>
        <source>DVD Menu Theme</source>
        <translation>Θέμα Μενού DVD</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="64"/>
        <source>Choose the appearance of your DVD.</source>
        <translation>Επιλέξτε την εμφάνιση του DVD σας.</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="212"/>
        <source>Theme:</source>
        <translation>Θέμα:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="121"/>
        <source>Intro</source>
        <translation>Εισαγωγή</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="127"/>
        <source>Main Menu</source>
        <translation>Κύριο Μενού</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="61"/>
        <source>Chapter Menu</source>
        <translation>Μενού Κεφαλαίων</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="84"/>
        <source>Details</source>
        <translation>Λεπτομέρειες</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="176"/>
        <source>Select Archive Items</source>
        <translation>Επιλογή Αρχείων προς Αρχειοθέτηση</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="187"/>
        <source>Select the recordings and videos you wish to save.</source>
        <translation>Επιλέξτε τις εγγραφές και τις ταινίες που θέλετε να αποθηκεύσετε.</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="142"/>
        <source>No files are selected for archive</source>
        <translation>Δεν έχουν επιλεγεί αρχεία για αρχειοθέτηση</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="24"/>
        <source>Add Recording</source>
        <translation>Προσθήκη Εγγραφής</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="26"/>
        <source>Add Video</source>
        <translation>Προσθήκη Ταινίας</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="23"/>
        <source>Add File</source>
        <translation>Προσθήκη Αρχείου</translation>
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
        <location filename="themestrings.h" line="33"/>
        <source>Archive Item Details</source>
        <translation>Λεπτομέρειες Στοιχείου</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="215"/>
        <source>Title:</source>
        <translation>Τίτλος:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="204"/>
        <source>Subtitle:</source>
        <translation>Υπότιτλος:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="199"/>
        <source>Start Date:</source>
        <translation>Ημ/νία Εκκίνησης:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="214"/>
        <source>Time:</source>
        <translation>Ώρα:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="80"/>
        <source>Description:</source>
        <translation>Περιγραφή:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="213"/>
        <source>Thumb Image Selector</source>
        <translation>Επιλογή Μικρογραφίας</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="72"/>
        <source>Current Position</source>
        <translation>Τρέχουσα Θέση</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="13"/>
        <source>0:00:00.00</source>
        <translation>0:00:00.00</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="174"/>
        <source>Seek Amount</source>
        <translation>Τιμή Εύρεσης</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="113"/>
        <source>Frame</source>
        <translation>Καρέ</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="167"/>
        <source>Save</source>
        <translation>Αποθήκευση</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="184"/>
        <source>Select a theme</source>
        <translation>Επιλέξτε ένα Θέμα</translation>
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
        <location filename="themestrings.h" line="162"/>
        <source>Prev</source>
        <translation>Προηγούμενο</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="232"/>
        <source>sep 13, 2004 11:00 pm (1h 15m)</source>
        <translation>Σεπ 13,2004 11:00 πμ (1h 15m)</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="235"/>
        <source>x.xx GB</source>
        <translation>x.xx GB</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="103"/>
        <source>File Finder To Import</source>
        <translation>Εισαγωγή Ανιχνευτή Αρχείων</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="200"/>
        <source>Start Time:</source>
        <translation>Ώρα Εκκίνησης:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="177"/>
        <source>Select Associated Channel</source>
        <translation>Επιλέξτε το Συσχετιζόμενο Κανάλι</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="38"/>
        <source>Archived Channel</source>
        <translation>Αρχειοθετημένο Κανάλι</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="51"/>
        <source>Chan. ID:</source>
        <translation>Αναγν. Καν:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="53"/>
        <source>Chan. No:</source>
        <translation>Νο Καν:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="47"/>
        <source>Callsign:</source>
        <translation>Ονομ:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="134"/>
        <source>Name:</source>
        <translation>Όνομα:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="123"/>
        <source>Local Channel</source>
        <translation>Τοπικό Κανάλι</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="171"/>
        <source>Search Channel</source>
        <translation>Αναζήτηση Καναλιού</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="170"/>
        <source>Search Callsign</source>
        <translation>Αναζήτηση Ονομασίας</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="172"/>
        <source>Search Name</source>
        <translation>Αναζήτηση Ονόματος</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="110"/>
        <source>Finish</source>
        <translation>Τέλος</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="179"/>
        <source>Select Destination:</source>
        <translation>Επιλέξτε Προορισμό:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="157"/>
        <source>Parental level: %1</source>
        <translation>Γονικό επίπεδο: %1</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="149"/>
        <source>Old size:</source>
        <translation>Παλιό μέγεθος:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="138"/>
        <source>New size:</source>
        <translation>Νέο μέγεθος:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="185"/>
        <source>Select a theme:</source>
        <translation>Επιλέξτε ένα θέμα:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="132"/>
        <source>Menu</source>
        <translation>Επιλογές</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="60"/>
        <source>Chapter</source>
        <translation>Κεφάλαιο</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="83"/>
        <source>Detail</source>
        <translation>Λεπτομέρειες</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="180"/>
        <source>Select File to Import</source>
        <translation>Επιλέξτε Αρχείο προς Εισαγωγή</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="30"/>
        <source>Add video</source>
        <translation>Προσθήκη Ταινίας</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="106"/>
        <source>Filesize: %1</source>
        <translation>Μέγεθος: %1</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="166"/>
        <source>Recorded Time: %1</source>
        <translation>Διάρκεια Εγγραφής: %1</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="7"/>
        <source>%&quot;|SUBTITLE|&quot; %%(|ORIGINALAIRDATE|) %%(|STARS|) %%DESCRIPTION%</source>
        <translation>%&quot;|SUBTITLE|&quot; %%(|ORIGINALAIRDATE|) %%(|STARS|) %%DESCRIPTION%</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="147"/>
        <source>Ok</source>
        <translation>Ok</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="156"/>
        <source>Parental Level: %1</source>
        <translation>Γονικό Επίπεδο: %1</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="34"/>
        <source>Archive Items</source>
        <translation>Αρχειοθέτηση Στοιχείων</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="165"/>
        <source>Profile:</source>
        <translation>Πρότυπο:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="148"/>
        <source>Old Size:</source>
        <translation>Παλιό Μέγεθος:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="137"/>
        <source>New Size:</source>
        <translation>Νέο Μέγεθος:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="56"/>
        <source>Channel ID:</source>
        <translation>Αναγν. Καν:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="58"/>
        <source>Channel Number:</source>
        <translation>Αριθ. Καναλιού:</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="68"/>
        <source>Create DVD</source>
        <translation>Δημιουργία DVD</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="67"/>
        <source>Create Archive</source>
        <translation>Δημιουργία Αρχείου Συμπίεσης</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="89"/>
        <source>Encode Video File</source>
        <translation>Κωδικοποίηση Αρχείου Ταινίας</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="117"/>
        <source>Import Archive</source>
        <translation>Εισαγωγή Αρχείου Συμπίεσης</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="37"/>
        <source>Archive Utilities</source>
        <translation>Ευκολίες Αρχειοθέτησης</translation>
    </message>
    <message>
        <source>Show Log Viewer</source>
        <translation type="vanished">Εμφάνιση Προβολέα Καταγραφής</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="158"/>
        <source>Play Created DVD</source>
        <translation>Αναπαραγωγή του Δημιουργημένου DVD</translation>
    </message>
    <message>
        <source>Burn DVD</source>
        <translation type="vanished">Εγγραφή του DVD</translation>
    </message>
</context>
<context>
    <name>ThumbFinder</name>
    <message>
        <location filename="../mytharchive/thumbfinder.cpp" line="863"/>
        <source>Menu</source>
        <translation>Επιλογές</translation>
    </message>
    <message>
        <location filename="../mytharchive/thumbfinder.cpp" line="870"/>
        <source>Exit, Save Thumbnails</source>
        <translation>Έξοδος, Αποθήκευση Μικρογραφιών</translation>
    </message>
    <message>
        <location filename="../mytharchive/thumbfinder.cpp" line="871"/>
        <source>Exit, Don&apos;t Save Thumbnails</source>
        <translation>Έξοδος, Χωρίς Αποθήκευση Μικρογραφιών</translation>
    </message>
</context>
<context>
    <name>VideoSelector</name>
    <message>
        <location filename="../mytharchive/videoselector.cpp" line="143"/>
        <source>Menu</source>
        <translation>Επιλογές</translation>
    </message>
    <message>
        <location filename="../mytharchive/videoselector.cpp" line="150"/>
        <source>Clear All</source>
        <translation>Καθαρισμός Όλων</translation>
    </message>
    <message>
        <location filename="../mytharchive/videoselector.cpp" line="151"/>
        <source>Select All</source>
        <translation>Επιλογή Όλων</translation>
    </message>
    <message>
        <location filename="../mytharchive/videoselector.cpp" line="333"/>
        <location filename="../mytharchive/videoselector.cpp" line="501"/>
        <source>All Videos</source>
        <translation>Όλες οι Ταινίες</translation>
    </message>
    <message>
        <location filename="../mytharchive/videoselector.cpp" line="554"/>
        <source>You need to enter a valid password for this parental level</source>
        <translation>Πρέπει να εισάγετε τον σωστό κωδικό γι΄αυτό το γονικό επίπεδο</translation>
    </message>
</context>
</TS>
