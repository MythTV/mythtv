#include <stdio.h>
#include <string.h>
#include <qapplication.h>
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
#include <qsqldatabase.h>
#include <iostream>

#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <cdaudio.h>

extern "C" {
#include <cdda_interface.h>
#include <cdda_paranoia.h>
}

#include "cdrip.h"
#include "cddecoder.h"
#include "encoder.h"
#include "vorbisencoder.h"
#include "lameencoder.h"
#include "flacencoder.h"

#include <mythtv/mythcontext.h>
#include <mythtv/mythwidgets.h>

using namespace std;

Ripper::Ripper(QSqlDatabase *ldb, MythMainWindow *parent, const char *name)
      : MythDialog(parent, name)
{
    db = ldb;

    QString cddevice = gContext->GetSetting("CDDevice");

    int cdrom_fd = cd_init_device((char*)cddevice.ascii());
    if (cdrom_fd == -1)
    {
        perror("Could not open cdrom_fd");
        return;
    }

    cd_close(cdrom_fd);  //Close the CD tray

    cd_finish(cdrom_fd);
    
    CdDecoder *decoder = new CdDecoder("cda", NULL, NULL, NULL);
    

    Metadata *track = decoder->getLastMetadata();

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

    QHBoxLayout *qualbox = new QHBoxLayout(vbox, 10);
    qualitygroup = new MythButtonGroup(firstdiag);
    qualitygroup->setFrameStyle(QFrame::NoFrame);
    qualitygroup->hide();

    QRadioButton *lowquality = new QRadioButton(tr("Low"), firstdiag);
    lowquality->setBackgroundOrigin(WindowOrigin);
    qualbox->addWidget(lowquality);
    qualitygroup->insert(lowquality);

    QRadioButton *mediumquality = new QRadioButton(tr("Medium"), firstdiag);
    mediumquality->setBackgroundOrigin(WindowOrigin);
    qualbox->addWidget(mediumquality);
    qualitygroup->insert(mediumquality);

    QRadioButton *highquality = new QRadioButton(tr("High"), firstdiag);
    highquality->setBackgroundOrigin(WindowOrigin);
    qualbox->addWidget(highquality);
    qualitygroup->insert(highquality);

    QRadioButton *perfectflac = new QRadioButton(tr("Perfect"), firstdiag);
    perfectflac->setBackgroundOrigin(WindowOrigin);
    qualbox->addWidget(perfectflac);
    qualitygroup->insert(perfectflac);

    qualitygroup->setRadioButtonExclusive(true);
    qualitygroup->setButton(gContext->GetNumSetting("DefaultRipQuality", 1));

    QGridLayout *grid = new QGridLayout(vbox, 1, 1, 20);
    
    QLabel *artistl = new QLabel(tr("Artist: "), firstdiag);
    artistl->setBackgroundOrigin(WindowOrigin);
    artistedit = new MythComboBox(true, firstdiag);
    fillComboBox(*artistedit, "artist");
    if (track)
    {
        artistedit->setCurrentText(track->Artist());
        artistname = track->Artist();
    }
    connect(artistedit, SIGNAL(activated(const QString &)),
            artistedit, SIGNAL(textChanged(const QString &)));
    connect(artistedit, SIGNAL(textChanged(const QString &)),
            this, SLOT(artistChanged(const QString &)));

    QLabel *albuml = new QLabel(tr("Album: "), firstdiag);
    albuml->setBackgroundOrigin(WindowOrigin);
    albumedit = new MythLineEdit(firstdiag);
    albumedit->setRW();
    if (track)
    {
        albumedit->setText(track->Album());
        albumname = track->Album();
    }
    connect(albumedit, SIGNAL(textChanged(const QString &)), 
            this, SLOT(albumChanged(const QString &))); 

    QLabel *genrelabel = new QLabel(tr("Genre: "), firstdiag);
    genrelabel->setBackgroundOrigin(WindowOrigin);
    genreedit = new MythComboBox(true, firstdiag);
    fillComboBox (*genreedit, "genre");
    if (track)
    {
       genreedit->setCurrentText(track->Genre());
       genrename = track->Genre();
    }
    connect(genreedit, SIGNAL(activated(const QString &)),
            genreedit, SIGNAL(textChanged(const QString &)));
    connect(genreedit, SIGNAL(textChanged(const QString &)),
            this, SLOT(genreChanged(const QString &)));
 
    grid->addMultiCellWidget(artistl, 0, 0, 0, 0);
    grid->addMultiCellWidget(artistedit,  0, 0, 1, 2);
    grid->addMultiCellWidget(albuml, 1, 1, 0, 0);
    grid->addMultiCellWidget(albumedit,  1, 1, 1, 2);
    grid->addMultiCellWidget(genrelabel, 2, 2, 0, 0);
    grid->addMultiCellWidget(genreedit, 2, 2, 1, 2);

    table = new MythTable(firstdiag);
    grid->addMultiCellWidget(table, 3, 3, 0, 2);
    table->setNumCols(3);
    table->setTopMargin(0);    
    table->setLeftMargin(0);
    table->setNumRows(1);
    table->setColumnReadOnly(0, true);
    table->setColumnReadOnly(2, true);
    table->setColumnStretchable(1, true);
    table->setCurrentCell(0, 1);

    int row = 0;
    QString label;
    int length, min, sec;

    for(int i = 0; i < decoder->getNumTracks(); i++)
    {
        track = decoder->getMetadata(i + 1);
        if(track)
        {
            length = track->Length() / 1000;
            min = length / 60;
            sec = length % 60;

            table->setNumRows(row + 1);

            table->setRowHeight(row, (int)(30 * hmult));

            label.sprintf("%d", i+1);
            table->setText(row, 0, label);

            table->setText(row, 1, track->Title());

            label.sprintf("%02d:%02d", min, sec);
            table->setText(row, 2, label);

            row++;
            delete track;
        }
    }

    totaltracks = decoder->getNumCDAudioTracks();

    delete decoder;

    connect(table, SIGNAL(valueChanged(int, int)), this, 
            SLOT(tableChanged(int, int)));



    MythPushButton *ripit = new MythPushButton(tr("Import this CD"), firstdiag);
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

void Ripper::artistChanged(const QString &newartist)
{
    CdDecoder *decoder = new CdDecoder("cda", NULL, NULL, NULL);
    Metadata *data = decoder->getMetadata(db, 1);
    
    data->setArtist(newartist);
    decoder->commitMetadata(data);

    artistname = newartist;

    delete data;
    delete decoder;
}

void Ripper::albumChanged(const QString &newalbum)
{
    CdDecoder *decoder = new CdDecoder("cda", NULL, NULL, NULL);
    Metadata *data = decoder->getMetadata(db, 1);

    data->setAlbum(newalbum);
    decoder->commitMetadata(data);

    albumname = newalbum;

    delete data;
    delete decoder;
}

void Ripper::genreChanged(const QString &newgenre)
{
    CdDecoder *decoder = new CdDecoder("cda", NULL, NULL, NULL);
    Metadata *data = decoder->getMetadata(db, 1);

    data->setGenre(newgenre);
    decoder->commitMetadata(data);

    genrename = newgenre;

    delete data;
    delete decoder;
} 

void Ripper::tableChanged(int row, int col)
{
    CdDecoder *decoder = new CdDecoder("cda", NULL, NULL, NULL);
    Metadata *data = decoder->getMetadata(db, row + 1 );

    data->setTitle(table->text(row, col));
    decoder->commitMetadata(data);

    delete data;
    delete decoder;
}

void Ripper::fillComboBox (MythComboBox & box, const QString & db_column)
{
   QString querystr = QString("SELECT DISTINCT %1 FROM musicmetadata;")
                             .arg(db_column);
      
   QSqlQuery query(querystr, db);
   
   QValueList<QString> list;
   
   if (query.isActive() && query.numRowsAffected() > 0)
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

void Ripper::fixFilename(QString &filename, const QString &addition)
{
    QString tempcopy = addition;

    tempcopy.replace(QRegExp("/"), QString("_"));
    tempcopy.replace(QRegExp("\\"), QString("_"));
    tempcopy.replace(QRegExp(":"), QString("_"));
    tempcopy.replace(QRegExp("?"), QString("_"));
    tempcopy.replace(QRegExp("\'"), QString("_"));
    tempcopy.replace(QRegExp("\""), QString("_"));

    filename += "/" + tempcopy;
}

void Ripper::reject() 
{
    QString cddevice = gContext->GetSetting("CDDevice");
    bool EjectCD = gContext->GetNumSetting("EjectCDAfterRipping",1);
    if (EjectCD) 
        ejectCD(cddevice);	
    done(Rejected);
}

void Ripper::ripthedisc(void)
{
    firstdiag->hide();

    QString tots = tr("Importing CD:\n") + artistname + "\n" + albumname;

    int screenwidth = 0, screenheight = 0;
    float wmult = 0, hmult = 0;

    gContext->GetScreenSettings(screenwidth, wmult, screenheight, hmult);

    MythDialog *newdiag = new MythDialog(gContext->GetMainWindow(), "ripping");
    
    newdiag->setFont(QFont("Arial", (int)(gContext->GetBigFontSize() * hmult),
                     QFont::Bold));

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

    QString outfile;
    CdDecoder *decoder = new CdDecoder("cda", NULL, NULL, NULL);

    int encodequal = qualitygroup->id(qualitygroup->selected());

    QString findir = gContext->GetSetting("MusicLocation");

    bool fileundergenre = gContext->GetNumSetting("FileUnderGenre", 0);
    bool fileunderartist = gContext->GetNumSetting("FileUnderArtist", 1);
    bool fileunderalbum = gContext->GetNumSetting("FileUnderAlbum", 1);

    for (int i = 0; i < decoder->getNumTracks(); i++)
    {
        Encoder *encoder;

        current->setProgress(0);
        current->reset();

        Metadata *track = decoder->getMetadata(db, i + 1);
        if(track)
        {
            textstatus = tr("Copying from CD:\n") + track->Title();       
            statusline->setText(textstatus);

            current->setProgress(0);
            current->reset();

            qApp->processEvents();

            outfile = findir;

            if (fileundergenre)
            {
                fixFilename(outfile, track->Genre());
                mkdir(outfile, 0777);
            }

            if (fileunderartist)
            {
                fixFilename(outfile, track->Artist());
                mkdir(outfile, 0777);
            }

            if (fileunderalbum)
            {
                fixFilename(outfile, track->Album());
                mkdir(outfile, 0777);
            }

            QString tempstr;
            tempstr.sprintf("%02d - %s", track->Track(), 
                            track->Title().latin1());

            fixFilename(outfile, tempstr);

            if (encodequal < 3)
            {
                if (encodertype == "mp3")
                {
                    outfile += ".mp3";
                    encoder = new LameEncoder(outfile, encodequal, track);
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

            ripTrack(cddevice, encoder, i + 1);

            overall->setProgress(i + 1);
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
        cout << "PostCDRipScript: " << PostRipCDScript << endl;
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
            qApp->processEvents();
        }
    }

    current->setProgress(end);
    qApp->processEvents();
        
    paranoia_free(paranoia);
    cdda_close(device);

    return (curpos - start + 1) * CD_FRAMESIZE_RAW;
}
