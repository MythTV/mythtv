// ANSI C includes
#include <cstdio>
#include <cstring>

// Unix C includes
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>

// Linux C includes
#include <cdaudio.h>
extern "C" {
#include <cdda_interface.h>
#include <cdda_paranoia.h>
}

// C++ includes
#include <iostream>
using namespace std;

// Qt includes
#include <qapplication.h>
#include <qdir.h>
#include <qdialog.h>
#include <qlayout.h>
#include <qlabel.h>
#include <qcursor.h>
#include <qcheckbox.h>
#include <qpushbutton.h>
#include <qregexp.h>
#include <qvbox.h>
#include <qprogressbar.h>
#include <qradiobutton.h>

// MythTV plugin includes
#include <mythtv/mythcontext.h>
#include <mythtv/mythdbcon.h>
#include <mythtv/mythwidgets.h>
#include <mythtv/lcddevice.h>

// MythMusic includes
#include "cdrip.h"
#include "cddecoder.h"
#include "encoder.h"
#include "vorbisencoder.h"
#include "lameencoder.h"
#include "flacencoder.h"

Ripper::Ripper(MythMainWindow *parent, const char *name)
      : MythDialog(parent, name)
{

    // Set this to false so we can tell if the ripper has done anything
    // (i.e. we can tell if the user quit prior to ripping)
    somethingwasripped = false;

    QString cddevice = gContext->GetSetting("CDDevice");

    int cdrom_fd = cd_init_device((char*)cddevice.ascii());
    if (cdrom_fd == -1)
    {
        perror("Could not open cdrom_fd");
        return;
    }

    cd_close(cdrom_fd);  //Close the CD tray

    cd_finish(cdrom_fd);
    
    bigvb = new QVBoxLayout(this, 0);

    firstdiag = new QFrame(this);
    firstdiag->setPalette(palette());
    firstdiag->setFont(font());
    bigvb->addWidget(firstdiag, 1);

    QVBoxLayout *vbox = new QVBoxLayout(firstdiag, (int)(24 * wmult));

    QLabel *inst = new QLabel(tr("Please select a quality level and check the "
                                 "album information below:"), firstdiag);
    inst->setBackgroundOrigin(WindowOrigin);
    vbox->addWidget(inst);

    QLabel *quality = new QLabel(tr("Quality: "), firstdiag);
    quality->setBackgroundOrigin(WindowOrigin);

    qualchooser = new MythComboBox(false, firstdiag);
    qualchooser->insertItem(tr("Low"));
    qualchooser->insertItem(tr("Medium"));
    qualchooser->insertItem(tr("High"));
    qualchooser->insertItem(tr("Perfect"));
    qualchooser->setCurrentItem(gContext->GetNumSetting("DefaultRipQuality", 1));
    qualchooser->setMaximumWidth(screenwidth / 2);

    QGridLayout *grid = new QGridLayout(vbox, 1, 1, 20);
    
    QLabel *artistl = new QLabel(tr("Artist: "), firstdiag);
    artistl->setBackgroundOrigin(WindowOrigin);
    artistedit = new MythComboBox(true, firstdiag);

    // Quick and dirty hack: Find a better way of calc'ing a max width!!
    // Large artist lists will create a massively wide listbox here
    // which totally messes up the rest of the page.
    artistedit->setMaximumWidth((int)(0.7 * screenwidth));

    fillComboBox(*artistedit, "artist");

    QLabel *albuml = new QLabel(tr("Album: "), firstdiag);
    albuml->setBackgroundOrigin(WindowOrigin);
    albumedit = new MythLineEdit(firstdiag);
    albumedit->setRW();

    QLabel *genrelabel = new QLabel(tr("Genre: "), firstdiag);
    genrelabel->setBackgroundOrigin(WindowOrigin);
    genreedit = new MythComboBox(true, firstdiag);
    fillComboBox (*genreedit, "genre");


    compilation = new MythCheckBox(firstdiag);
    compilation->setBackgroundOrigin(WindowOrigin);
    compilation->setText(tr("Multi-Artist?"));


    // Create a button for swapping the names of the artist and the track title.
    switchtitleartist = new MythPushButton(tr("Switch Titles && Artists"), firstdiag);

    connect(switchtitleartist, SIGNAL(clicked()), this, SLOT(switchTitlesAndArtists())); 
    

    grid->addMultiCellWidget(quality, 0, 0, 0, 0);
    grid->addMultiCellWidget(qualchooser,  0, 0, 1, 2);
    grid->addMultiCellWidget(artistl, 1, 1, 0, 0);
    grid->addMultiCellWidget(artistedit,  1, 1, 1, 2);
    grid->addMultiCellWidget(albuml, 2, 2, 0, 0);
    grid->addMultiCellWidget(albumedit,  2, 2, 1, 2);
    grid->addMultiCellWidget(genrelabel, 3, 3, 0, 0);
    grid->addMultiCellWidget(genreedit, 3, 3, 1, 2);
    grid->addMultiCellWidget(compilation, 4, 4, 0, 0);
    grid->addMultiCellWidget(switchtitleartist, 4, 4, 1, 2);

    table = new MythTable(firstdiag);
    grid->addMultiCellWidget(table, 5, 5, 0, 2);
    table->setNumCols(4);
    table->setLeftMargin(0);
    table->setNumRows(1);
    table->setTopMargin(36);    
    table->horizontalHeader()->setLabel(0, "#");
    table->horizontalHeader()->setLabel(1, tr("Title"));
    table->horizontalHeader()->setLabel(2, tr("Artist"));
    table->horizontalHeader()->setLabel(3, tr("Length"));
    table->setColumnReadOnly(0, true);
    table->setColumnReadOnly(3, true);
    table->setColumnStretchable(1, true);
    table->setColumnStretchable(2, true);
    table->setCurrentCell(0, 1);


    // Set the values of the widgets.

    bool iscompilation = false;

    CdDecoder *decoder = new CdDecoder("cda", NULL, NULL, NULL);

    if (decoder)
    {

        int row = 0;
        QString label;
        int length, min, sec;
        Metadata *track;

        for (int trackno = 0; trackno < decoder->getNumTracks(); trackno++)
        {
            track = decoder->getMetadata(trackno + 1);
            if (track)
            {
                if (track->Compilation())
                {
                    iscompilation = true;
                    artistname = track->CompilationArtist();
                }
                else if ("" == artistname)
                {
                    artistname = track->Artist();
                }

                if ("" == albumname)
                    albumname = track->Album();

                if ("" == genrename 
                    && "" != track->Genre())
                {
                    genrename = track->Genre();
                }

                length = track->Length() / 1000;
                min = length / 60;
                sec = length % 60;
                
                table->setNumRows(row + 1);
                
                table->setRowHeight(row, (int)(30 * hmult));
                
                label.sprintf("%d", trackno + 1);
                table->setText(row, 0, label);
                
                table->setText(row, 1, track->Title());
                
                table->setText(row, 2, track->Artist());
                
                label.sprintf("%02d:%02d", min, sec);
                table->setText(row, 3, label);
                
                row++;
                delete track;
            }
        }
        
        artistedit->setCurrentText(artistname);
        albumedit->setText(albumname);
        genreedit->setCurrentText(genrename);
        compilation->setChecked(iscompilation);

        if (!iscompilation)
        {
          switchtitleartist->hide();
          table->hideColumn(2);
        }

        totaltracks = decoder->getNumCDAudioTracks();

        
        delete decoder;
    }



    // Now that all defualt values are set, let's connect some events...
    connect(artistedit, SIGNAL(activated(const QString &)),
            artistedit, SIGNAL(textChanged(const QString &)));
    connect(artistedit, SIGNAL(textChanged(const QString &)),
            this, SLOT(artistChanged(const QString &)));

    
    connect(albumedit, SIGNAL(textChanged(const QString &)), 
            this, SLOT(albumChanged(const QString &))); 

    connect(genreedit, SIGNAL(activated(const QString &)),
            genreedit, SIGNAL(textChanged(const QString &)));
    connect(genreedit, SIGNAL(textChanged(const QString &)),
            this, SLOT(genreChanged(const QString &)));
 
    connect(compilation, SIGNAL(toggled(bool)),
            this, SLOT(compilationChanged(bool)));

    connect(table, SIGNAL(valueChanged(int, int)), this, 
            SLOT(tableChanged(int, int)));



    MythPushButton *ripit = new MythPushButton(tr("Import this CD"), firstdiag);
    ripit->setFocus ();
    vbox->addWidget(ripit);

    connect(ripit, SIGNAL(clicked()), this, SLOT(ripthedisc())); 
}

Ripper::~Ripper(void)
{   
}       

QSizePolicy Ripper::sizePolicy(void)
{
    return QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
}

bool Ripper::somethingWasRipped()
{
    return somethingwasripped;
}

void Ripper::artistChanged(const QString &newartist)
{
    CdDecoder *decoder = new CdDecoder("cda", NULL, NULL, NULL);
    Metadata *data = decoder->getMetadata(1);
    
    if (!decoder || !data)
        return;

    if (compilation->isChecked())
    {
      data->setCompilationArtist(newartist);
      decoder->commitMetadata(data);
    }
    else
    {
        data->setArtist(newartist);
        data->setCompilationArtist("");
        decoder->commitMetadata(data);
    }

    artistname = newartist;

    delete data;
    delete decoder;
}

void Ripper::albumChanged(const QString &newalbum)
{
    CdDecoder *decoder = new CdDecoder("cda", NULL, NULL, NULL);
    Metadata *data = decoder->getMetadata(1);

    if (!decoder || !data)
        return;

    data->setAlbum(newalbum);
    decoder->commitMetadata(data);

    albumname = newalbum;

    delete data;
    delete decoder;
}

void Ripper::genreChanged(const QString &newgenre)
{
    CdDecoder *decoder = new CdDecoder("cda", NULL, NULL, NULL);
    Metadata *data = decoder->getMetadata(1);

    if (!decoder || !data)
        return;

    data->setGenre(newgenre);
    decoder->commitMetadata(data);

    genrename = newgenre;

    delete data;
    delete decoder;
} 

void Ripper::compilationChanged(bool state)
{
    CdDecoder *decoder = new CdDecoder("cda", NULL, NULL, NULL);
    Metadata *data;
  
    if (!decoder)
        return;

    if (!state)
    {
        // Update artist MetaData of each track on the ablum...
        for (int trackno = 1; trackno <= totaltracks; ++trackno)
        {
            data = decoder->getMetadata(trackno);

            //  Metadata will be NULL for non-audio tracks
            if(data)
            {
                // Make metadata appear to be just a normal track.
                data->setCompilationArtist("");
                data->setArtist(artistname);
                data->setCompilation(false);
                decoder->commitMetadata(data);
                delete data;
            }
        }
        
        // Visual updates
        table->hideColumn(2);
        switchtitleartist->hide();
    }
    else
    {
        // Update artist MetaData of each track on the ablum...
        for (int trackno = 1; trackno <= totaltracks; ++trackno)
        {
            data = decoder->getMetadata(trackno);

            //  Metadata will be NULL for non-audio tracks
            if(data)
            {
                // Make metadata appear to be just a normal track.
                data->setCompilationArtist(artistname);
                data->setArtist(table->text(trackno - 1, 2));
                data->setCompilation(true);
                decoder->commitMetadata(data);
                delete data;
            }
        }

        // Visual updates
        table->showColumn(2);
        switchtitleartist->show();
    }

    delete decoder;

}

void Ripper::tableChanged(int row, int col)
{
    CdDecoder *decoder = new CdDecoder("cda", NULL, NULL, NULL);
    Metadata *data = decoder->getMetadata(row + 1 );

    if (!decoder || !data)
        return;

    // We only update the title if col 1 is edited...
    if (1 == col)
    {
        data->setTitle(table->text(row, 1));
    }
    else if (2 == col
             && compilation->isChecked())
    {
        if ("" == table->text(row, 2))
        {
            // This is a compilation layout, but the user has put a blank
            // into the artist column. This probably means this track is 
            // also performed by the person who is also the album artist.
            data->setArtist(artistname);
        }
        else
        {
            data->setArtist(table->text(row, 2));
        }
    }

    decoder->commitMetadata(data);

    delete data;
    delete decoder;
}

void Ripper::switchTitlesAndArtists()
{
    if (!compilation->isChecked())
        return;

    CdDecoder *decoder = new CdDecoder("cda", NULL, NULL, NULL);
    Metadata *data;
  
    if (!decoder)
        return;

    // Switch title and artist for each track
    QString tmp;
    for (int row = 0; row < totaltracks; ++row)
    {
        data = decoder->getMetadata(row + 1);
      
        if (data)
        {
            // Switch the columns first
            tmp = table->text(row, 2);
            table->setText(row, 2, table->text(row, 1));
            table->setText(row, 1, tmp);

            // Just set both title and artist
            data->setTitle(table->text(row, 1));

            if ("" == table->text(row, 2))
            {
                // This is a compilation layout, but the user has put a blank
                // into the artist column. This probably means this track is 
                // also performed by the person who is also the album artist.
                data->setArtist(artistname);
            }
            else
            {
                data->setArtist(table->text(row, 2));
            }

            decoder->commitMetadata(data);
            delete data;
        }
    }

    delete decoder;
}

void Ripper::fillComboBox(MythComboBox &box, const QString &db_column)
{
    QString querystr = QString("SELECT DISTINCT %1 FROM musicmetadata;")
                               .arg(db_column);
      
    MSqlQuery query(MSqlQuery::InitCon());
    query.exec(querystr);
   
    QValueList<QString> list;
   
    if (query.isActive() && query.size() > 0)
    { 
        while (query.next())
        {
            list.push_front(query.value(0).toString());
        }
    }
   
    QStringList strlist(list);
   
    strlist.sort();
   
    box.insertStringList(strlist);
}

void Ripper::handleFileTokens(QString &filename, Metadata *track)
{
    QString original = filename;

    QString fntempl = gContext->GetSetting("FilenameTemplate");
    bool no_ws = gContext->GetNumSetting("NoWhitespace", 0);

    QRegExp rx_ws("\\s{1,}");
    QRegExp rx("(?:\\s?)(GENRE|ARTIST|ALBUM|TRACK|TITLE|YEAR)(?:\\s?)/");

    int i = 0;
    while (i >= 0)
    {
        i = rx.search(fntempl, i);
        if (i >= 0)
        {
            i += rx.matchedLength();
            if ((rx.capturedTexts()[1] == "GENRE") && (track->Genre() != ""))
                filename += fixFileToken(track->Genre()) + "/";
            if ((rx.capturedTexts()[1] == "ARTIST") && (track->FormatArtist() != ""))
                filename += fixFileToken(track->FormatArtist()) + "/";
            if ((rx.capturedTexts()[1] == "ALBUM") && (track->Album() != ""))
                filename += fixFileToken(track->Album()) + "/";
            if ((rx.capturedTexts()[1] == "TRACK") && (track->Track() >= 0))
                filename += fixFileToken(QString::number(track->Track(), 10)) + "/";
            if ((rx.capturedTexts()[1] == "TITLE") && (track->FormatTitle() != ""))
                filename += fixFileToken(track->FormatTitle()) + "/";
            if ((rx.capturedTexts()[1] == "YEAR") && (track->Year() >= 0))
                filename += fixFileToken(QString::number(track->Year(), 10)) + "/";

            if (no_ws)
                filename.replace(rx_ws, "_");

            mkdir(filename, 0775);
        }
    }

    // remove the dir part and other cruft
    fntempl.replace(QRegExp("(.*/)|\\s*"), "");
    QString tagsep = gContext->GetSetting("TagSeparator");
    QStringList tokens = QStringList::split("-", fntempl);
    QStringList fileparts;
    QString filepart;

    for (unsigned i = 0; i < tokens.size(); i++)
    {
        if ((tokens[i] == "GENRE") && (track->Genre() != ""))
            fileparts += track->Genre();
        else if ((tokens[i] == "ARTIST") && (track->FormatArtist() != ""))
            fileparts += track->FormatArtist();
        else if ((tokens[i] == "ALBUM") && (track->Album() != ""))
            fileparts += track->Album();
        else if ((tokens[i] == "TRACK") && (track->Track() >= 0))
        {
            QString tempstr = QString::number(track->Track(), 10);
            if (track->Track() < 10)
                tempstr.prepend('0');
            fileparts += tempstr;
        }
        else if ((tokens[i] == "TITLE") && (track->FormatTitle() != ""))
            fileparts += track->FormatTitle();
        else if ((tokens[i] == "YEAR") && (track->Year() >= 0))
            fileparts += QString::number(track->Year(), 10);
    }

    filepart = fileparts.join( tagsep );
    filename += fixFileToken(filepart);
    
    if (filename == original || filename.length() > FILENAME_MAX)
    {
        QString tempstr = QString::number(track->Track(), 10);
        tempstr += " - " + track->FormatTitle();
        filename += fixFileToken(tempstr);
        VERBOSE(VB_GENERAL, QString("Invalid file storage definition."));
    }

    if (no_ws)
        filename.replace(rx_ws, "_");
}

inline QString Ripper::fixFileToken(QString token)
{
    token.replace(QRegExp("(/|\\\\|:|\'|\"|\\?|\\|)"), QString("_"));
    return token;
}

void Ripper::reject() 
{
    QString cddevice = gContext->GetSetting("CDDevice");
    bool EjectCD = gContext->GetNumSetting("EjectCDAfterRipping",1);
    if (EjectCD) 
        ejectCD(cddevice);	
    done(Rejected);
}

static long int getSectorCount (QString &cddevice, int tracknum)
{
    cdrom_drive *device = cdda_identify(cddevice.ascii(), 0, NULL);

    if (!device)
        return -1;

    if (cdda_open(device))
    {
        cdda_close(device);
        return -1;
    }

    // we only care about audio tracks
    if (cdda_track_audiop (device, tracknum))
    {
        cdda_verbose_set(device, CDDA_MESSAGE_FORGETIT, CDDA_MESSAGE_FORGETIT);
        long int start = cdda_track_firstsector(device, tracknum);
        long int end   = cdda_track_lastsector( device, tracknum);
        cdda_close(device);
        return end - start + 1;        
    }

    cdda_close(device);
    return 0;
}

void Ripper::ripthedisc(void)
{
    firstdiag->hide();

    QString tots = tr("Importing CD:\n") + artistname + "\n" + albumname;

    int screenwidth = 0, screenheight = 0;
    float wmult = 0, hmult = 0;

    gContext->GetScreenSettings(screenwidth, wmult, screenheight, hmult);

    MythDialog *newdiag = new MythDialog(gContext->GetMainWindow(), 
                                         tr("Ripping..."));
    
    newdiag->setFont(gContext->GetBigFont());

    QVBoxLayout *vb = new QVBoxLayout(newdiag, 20);

    QLabel *totallabel = new QLabel(tots, newdiag);
    totallabel->setBackgroundOrigin(WindowOrigin);
    totallabel->setAlignment(AlignAuto | AlignVCenter | ExpandTabs | WordBreak);
    vb->addWidget(totallabel);

    overall = new QProgressBar(totaltracks, newdiag);
    overall->setBackgroundOrigin(WindowOrigin);
    overall->setProgress(0);
    vb->addWidget(overall);

    statusline = new QLabel(" ", newdiag);
    statusline->setBackgroundOrigin(WindowOrigin);
    statusline->setAlignment(AlignAuto | AlignVCenter | ExpandTabs | WordBreak);
    vb->addWidget(statusline);
    
    current = new QProgressBar(1, newdiag);
    current->setBackgroundOrigin(WindowOrigin);
    current->setProgress(0);
    vb->addWidget(current);

    newdiag->show();

    qApp->processEvents(5);
    qApp->processEvents();

    QString textstatus;
    QString cddevice = gContext->GetSetting("CDDevice");
    QString encodertype = gContext->GetSetting("EncoderType");
    int encodequal = qualchooser->currentItem();
    bool mp3usevbr = gContext->GetNumSetting("Mp3UseVBR", 0);

    CdDecoder *decoder = new CdDecoder("cda", NULL, NULL, NULL);

    QString musicdir = gContext->GetSetting("MusicLocation");
    musicdir = QDir::cleanDirPath(musicdir);
    if (!musicdir.endsWith("/"))
        musicdir += "/";

    totalSectors = 0;
    totalSectorsDone = 0;
    for (int trackno = 0; trackno < decoder->getNumTracks(); trackno++)
    {
        totalSectors += getSectorCount (cddevice, trackno + 1);
    }
    overall->setTotalSteps (totalSectors);

    if (class LCD * lcd = LCD::Get()) 
    {
        QString lcd_tots = tr("Importing ") + artistname + " - " + albumname;
        QPtrList<LCDTextItem> textItems;
        textItems.setAutoDelete(true);
        textItems.append(new LCDTextItem(1, ALIGN_CENTERED, lcd_tots, "Generic", false));
        lcd->switchToGeneric(&textItems);
    }

    for (int trackno = 0; trackno < decoder->getNumTracks(); trackno++)
    {
        Encoder *encoder;

        current->setProgress(0);
        current->reset();

        Metadata *track = decoder->getMetadata(trackno + 1);

        if (track)
        {
            QString outfile = musicdir;

            //
            // cddb_genre from cdda structure is just an enum that
            // gets mapped to a string -- kind of useless for custom
            // genres.  Override the value here with the value from
            // the Genre combo box.
            //
            
            track->setGenre(genreedit->currentText());

            textstatus = tr("Copying from CD:\n") + track->Title();       
            statusline->setText(textstatus);

            current->setProgress(0);
            current->reset();

            qApp->processEvents();

            handleFileTokens(outfile, track);

            if (encodequal < 3)
            {
                if (encodertype == "mp3")
                {
                    outfile += ".mp3";
                    encoder = new LameEncoder(outfile, encodequal, track, 
                                              mp3usevbr);
                }
                else // ogg
                {
                    outfile += ".ogg";
                    encoder = new VorbisEncoder(outfile, encodequal, track); 
                }
            }
            else
            {
                outfile += ".flac";
                encoder = new FlacEncoder(outfile, encodequal, track); 
            }

            if (!encoder->isValid()) 
            {
                delete encoder;
                delete track;
                break;
            }

            ripTrack(cddevice, encoder, trackno + 1);

            // Set the flag to show that we have ripped new files
            somethingwasripped = true;

            qApp->processEvents();

            delete encoder;
            delete track;
        }
    }

    bool EjectCD = gContext->GetNumSetting("EjectCDAfterRipping",1);
    if (EjectCD) 
        ejectCD(cddevice);

    QString PostRipCDScript = gContext->GetSetting("PostCDRipScript");

    if (!PostRipCDScript.isEmpty()) 
    {
        VERBOSE(VB_ALL, QString("PostCDRipScript: %1").arg(PostRipCDScript));
        pid_t child = fork();
        if (child < 0)
        {
            perror("fork");
        }
        else if (child == 0)
        {
            execl("/bin/sh", "sh", "-c", PostRipCDScript.ascii(), NULL);
            perror("exec");
            _exit(1);
        }
    }

    delete newdiag;

    hide();
}


void Ripper::ejectCD(QString& cddev)
{
    int cdrom_fd;
    cdrom_fd = cd_init_device((char*)cddev.ascii());
    if (cdrom_fd != -1)
    {
        if (cd_eject(cdrom_fd) == -1) 
            perror("Failed on cd_eject");
        cd_finish(cdrom_fd);              
    } 
    else  
        perror("Failed on cd_init_device");
}

static void paranoia_cb(long inpos, int function)
{
    inpos = inpos; function = function;
}

int Ripper::ripTrack(QString &cddevice, Encoder *encoder, int tracknum)
{
    cdrom_drive *device = cdda_identify(cddevice.ascii(), 0, NULL);

    if (!device)
        return -1;

    if (cdda_open(device))
    {
        cdda_close(device);
        return -1;
    }

    cdda_verbose_set(device, CDDA_MESSAGE_FORGETIT, CDDA_MESSAGE_FORGETIT);
    long int start = cdda_track_firstsector(device, tracknum);
    long int end = cdda_track_lastsector(device, tracknum);

    cdrom_paranoia *paranoia = paranoia_init(device);
    if (gContext->GetSetting("ParanoiaLevel") == "full")
        paranoia_modeset(paranoia, PARANOIA_MODE_FULL | 
                                   PARANOIA_MODE_NEVERSKIP);
    else
        paranoia_modeset(paranoia, PARANOIA_MODE_OVERLAP);

    paranoia_seek(paranoia, start, SEEK_SET);

    long int curpos = start;
    int16_t *buffer;

    current->setTotalSteps(end - start + 1);

    qApp->processEvents();

    int every15 = 15;
    while (curpos < end)
    {
        buffer = paranoia_read(paranoia, paranoia_cb);

        if (encoder->addSamples(buffer, CD_FRAMESIZE_RAW))
            break;

        curpos++;

        every15--;
 
        if (every15 <= 0) 
        {
            every15 = 15;
            current->setProgress(curpos - start);           
            overall->setProgress(totalSectorsDone + (curpos - start));
            if (class LCD * lcd = LCD::Get()) 
            {
                float fProgress = (float)(totalSectorsDone + (curpos - start))/totalSectors;
                lcd->setGenericProgress(fProgress);
            }
            qApp->processEvents();
        }
    }

    totalSectorsDone += end - start + 1;
    current->setProgress(end);
    qApp->processEvents();
        
    paranoia_free(paranoia);
    cdda_close(device);

    return (curpos - start + 1) * CD_FRAMESIZE_RAW;
}
