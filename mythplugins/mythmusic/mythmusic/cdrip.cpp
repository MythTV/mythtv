#include <stdio.h>
#include <string.h>
#include <qapplication.h>
#include <qdialog.h>
#include <qlayout.h>
#include <qlabel.h>
#include <qlineedit.h>
#include <qcursor.h>
#include <qcheckbox.h>
#include <qpushbutton.h>
#include <qvbox.h>
#include <qprogressbar.h>
#include <qtable.h>
#include <qbuttongroup.h>
#include <qradiobutton.h>

#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

extern "C" {
#include <cdda_interface.h>
#include <cdda_paranoia.h>
}

#include "cdrip.h"
#include "cddecoder.h"
#include "vorbisencoder.h"
#include "flacencoder.h"
#include "settings.h"

extern Settings *settings;

class MyLineEdit : public QLineEdit
{
  public:
    MyLineEdit(QWidget *parent) : QLineEdit(parent) { }

    void keyPressEvent(QKeyEvent *e);
};

void MyLineEdit::keyPressEvent(QKeyEvent *e)
{
    bool handled = false;;

    switch (e->key())
    {
        case Key_Up:
        {
            focusNextPrevChild(FALSE);
            handled = true;
            break;
        }
        case Key_Down:
        {
            focusNextPrevChild(TRUE);
            handled = true;
            break;
        }
    }

    if (!handled)
        QLineEdit::keyPressEvent(e);
}

class MyTable : public QTable
{
  public:
    MyTable(QWidget *parent) : QTable(parent) { }
  
    void keyPressEvent(QKeyEvent *e);
};

void MyTable::keyPressEvent(QKeyEvent *e)
{
    bool handled = false;

    if (isEditing() && item(currEditRow(), currEditCol()) &&
        item(currEditRow(), currEditCol())->editType() == QTableItem::OnTyping)
        return;

    int tmpRow = currentRow();

    switch (e->key())
    {
        case Key_Left:
        case Key_Right:
        {
            handled = true;
            break;
        }
        case Key_Up:
        {
            if (tmpRow == 0)
            {
                focusNextPrevChild(FALSE);
                handled = true;
            }
            break;
        }
        case Key_Down:
        {
            if (tmpRow == numRows() - 1)
            {
                focusNextPrevChild(TRUE);
                handled = true;
            }
            break;
        }
    }

    if (!handled)
        QTable::keyPressEvent(e);
}

class MyButtonGroup : public QButtonGroup
{
  public:
    MyButtonGroup(QWidget *parent = 0) : QButtonGroup(parent) { }

    void moveFocus(int key);
};

void MyButtonGroup::moveFocus(int key)
{
    QButton *currentSel = selected();
 
    QButtonGroup::moveFocus(key);

    if (selected() == currentSel)
    {
        switch (key)
        {
            case Key_Up: focusNextPrevChild(FALSE); break;
            case Key_Down: focusNextPrevChild(TRUE); break;
            default: break;
        }
    }
}

Ripper::Ripper(QSqlDatabase *ldb, QWidget *parent, const char *name)
      : QDialog(parent, name)
{
    db = ldb;
    
    int screenheight = QApplication::desktop()->height();
    int screenwidth = QApplication::desktop()->width();
  
    screenwidth = 800; screenheight = 600;
 
    float wmult = screenwidth / 800.0;
    float hmult = screenheight / 600.0;
 
    setGeometry(0, 0, screenwidth, screenheight);
    setFixedSize(QSize(screenwidth, screenheight));

    setFont(QFont("Arial", 16 * hmult, QFont::Bold));
    setCursor(QCursor(Qt::BlankCursor));

    CdDecoder *decoder = new CdDecoder("cda", NULL, NULL, NULL);
    Metadata *track = decoder->getMetadata(db, 1);

    bigvb = new QVBoxLayout(this, 0);

    firstdiag = new QFrame(this);
    bigvb->addWidget(firstdiag, 1);

    QVBoxLayout *vbox = new QVBoxLayout(firstdiag, 24 * wmult);

    QLabel *inst = new QLabel("Please select a quality level and check the album information below:", firstdiag);
    vbox->addWidget(inst);

    QHBoxLayout *qualbox = new QHBoxLayout(vbox, 10);
    qualitygroup = new MyButtonGroup(firstdiag);
    qualitygroup->setFrameStyle(QFrame::NoFrame);
    qualitygroup->hide();

    QRadioButton *lowvorb = new QRadioButton("Low", firstdiag);
    qualbox->addWidget(lowvorb);
    qualitygroup->insert(lowvorb);

    QRadioButton *medvorb = new QRadioButton("Medium", firstdiag);
    qualbox->addWidget(medvorb);
    qualitygroup->insert(medvorb);

    QRadioButton *highvorb = new QRadioButton("High", firstdiag);
    qualbox->addWidget(highvorb);
    qualitygroup->insert(highvorb);

    QRadioButton *perfectflac = new QRadioButton("Perfect", firstdiag);
    qualbox->addWidget(perfectflac);
    qualitygroup->insert(perfectflac);

    qualitygroup->setRadioButtonExclusive(true);
    qualitygroup->setButton(1);

    QGridLayout *grid = new QGridLayout(vbox, 1, 1, 20);
    
    QLabel *artistl = new QLabel("Artist: ", firstdiag);
    artistedit = new MyLineEdit(firstdiag);
    if (track)
    {
        artistedit->setText(track->Artist());
        artistname = track->Artist();
    }
    connect(artistedit, SIGNAL(textChanged(const QString &)),
            this, SLOT(artistChanged(const QString &)));

    QLabel *albuml = new QLabel("Album: ", firstdiag);
    albumedit = new MyLineEdit(firstdiag);
    if (track)
    {
        albumedit->setText(track->Album());
        albumname = track->Album();
    }
    connect(albumedit, SIGNAL(textChanged(const QString &)), 
            this, SLOT(albumChanged(const QString &))); 
 
    grid->addMultiCellWidget(artistl, 0, 0, 0, 0);
    grid->addMultiCellWidget(artistedit,  0, 0, 1, 2);
    grid->addMultiCellWidget(albuml, 1, 1, 0, 0);
    grid->addMultiCellWidget(albumedit,  1, 1, 1, 2);

    table = new MyTable(firstdiag);
    grid->addMultiCellWidget(table, 2, 2, 0, 2);
    table->setNumCols(3);
    table->setTopMargin(0);    
    table->setLeftMargin(0);
    table->setNumRows(1);
    table->setColumnReadOnly(0, true);
    table->setColumnReadOnly(2, true);
    table->setColumnStretchable(1, true);
    table->setCurrentCell(0, 1);

    int row = 0;
    int tracknum = 1;
    QString label;
    int length, min, sec;

    while (track)
    {
        length = track->Length() / 1000;
        min = length / 60;
        sec = length % 60;

        table->setNumRows(tracknum);

        table->setRowHeight(row, 30 * hmult);

        label.sprintf("%d", tracknum);
        table->setText(row, 0, label);

        table->setText(row, 1, track->Title());

        label.sprintf("%02d:%02d", min, sec);
        table->setText(row, 2, label);

        row++;
        tracknum++;

        delete track;
        track = decoder->getMetadata(db, tracknum);
    }

    if (track)
        delete track; 
    delete decoder;

    connect(table, SIGNAL(valueChanged(int, int)), this, 
            SLOT(tableChanged(int, int)));

    tracknum--;

    totaltracks = tracknum;

    QPushButton *ripit = new QPushButton("Import this CD", firstdiag);
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

void Ripper::Show(void)
{
    showFullScreen();
    setActiveWindow();
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

void Ripper::tableChanged(int row, int col)
{
    CdDecoder *decoder = new CdDecoder("cda", NULL, NULL, NULL);
    Metadata *data = decoder->getMetadata(db, row + 1 );

    data->setTitle(table->text(row, col));
    decoder->commitMetadata(data);

    delete data;
    delete decoder;
}

void Ripper::fixFilename(char *filename, const char *addition)
{
    char tempcopy[1025];
    memset(tempcopy, '\0', 1025);
    strncpy(tempcopy, addition, 1024);

    for (unsigned int i = 0; i < strlen(tempcopy); i++)
    {
        if (tempcopy[i] == '/' || tempcopy[i] == '\\' || tempcopy[i] == ':' ||
            tempcopy[i] == '?' || tempcopy[i] == '\'' || tempcopy[i] == '\"')
        { 
            tempcopy[i] = '_';
        }
    }

    strcat(filename, "/");
    strcat(filename, tempcopy);
}

void Ripper::ripthedisc(void)
{
    firstdiag->hide();

    QString tots = "Importing CD:\n" + artistname + "\n" + albumname;

    int screenheight = QApplication::desktop()->height();
    int screenwidth = QApplication::desktop()->width();

    screenwidth = 800; screenheight = 600;

    float hmult = screenheight / 600.0;

    QDialog *newdiag = new QDialog(NULL, 0, TRUE);
    newdiag->setGeometry(0, 0, screenwidth, screenheight);
    newdiag->setFixedSize(QSize(screenwidth, screenheight));
    
    newdiag->setFont(QFont("Arial", 20 * hmult, QFont::Bold));
    newdiag->setCursor(QCursor(Qt::BlankCursor));

    QVBoxLayout *vb = new QVBoxLayout(newdiag, 20);

    QLabel *totallabel = new QLabel(tots, newdiag);
    totallabel->setAlignment(AlignAuto | AlignVCenter | ExpandTabs | WordBreak);
    vb->addWidget(totallabel);

    overall = new QProgressBar(totaltracks * 2, newdiag);
    overall->setProgress(0);
    vb->addWidget(overall);
    
    statusline = new QLabel(" ", newdiag);
    statusline->setAlignment(AlignAuto | AlignVCenter | ExpandTabs | WordBreak);
    vb->addWidget(statusline);
    
    current = new QProgressBar(1, newdiag);
    current->setProgress(0);
    vb->addWidget(current);

    newdiag->showFullScreen();

    qApp->processEvents(5);
    qApp->processEvents();

    QString textstatus;
    QString cddevice = (char *)(settings->GetSetting("CDDevice").c_str());

    char tempfile[512], outfile[4096];
    CdDecoder *decoder = new CdDecoder("cda", NULL, NULL, NULL);

    int encodequal = qualitygroup->id(qualitygroup->selected());

    QString tempdir = (char *)(settings->GetSetting("TemporarySpace").c_str());
    QString findir = (char *)(settings->GetSetting("MusicLocation").c_str());

    for (int i = 0; i < totaltracks; i++)
    {
        current->setProgress(0);
        current->reset();

        Metadata *track = decoder->getMetadata(db, i + 1);

        textstatus = "Copying from CD:\n" + track->Title();       
        statusline->setText(textstatus);

        sprintf(tempfile, "/%s/%d.raw", tempdir.ascii(), i + 1);
        long totalbytes = ripTrack((char *)cddevice.ascii(), tempfile, i + 1);
 
        overall->setProgress((i * 2) + 1);

        current->setProgress(0);
        current->reset();

        textstatus = "Compressing:\n" + track->Title();
        statusline->setText(textstatus);

        sprintf(outfile, "%s", findir.ascii());
        fixFilename(outfile, track->Artist().ascii());
        mkdir(outfile, 0777);
        fixFilename(outfile, track->Album().ascii());
        mkdir(outfile, 0777);
        fixFilename(outfile, track->Title().ascii());

        if (encodequal < 3)
        {
            strcat(outfile, ".ogg");
            VorbisEncode(tempfile, outfile, encodequal, track, totalbytes,
                         current);
        }
        else
        {
            strcat(outfile, ".flac");
            FlacEncode(tempfile, outfile, encodequal, track, totalbytes, 
                       current);
        }

        unlink(tempfile);

        overall->setProgress((i + 1) * 2);
        qApp->processEvents();

        delete track;
    }

    delete newdiag;

    hide();
}

static void paranoia_cb(long inpos, int function)
{
    inpos = inpos; function = function;
}

int Ripper::ripTrack(char *cddevice, char *outputfilename, int tracknum)
{
    cdrom_drive *device = cdda_identify(cddevice, 0, NULL);

    if (!device)
        return -1;

    if (cdda_open(device))
    {
        cdda_close(device);
        return -1;
    }

    FILE *output = fopen(outputfilename, "w");
    if (!output)
    {
        cdda_close(device);
        return -1;
    }

    cdda_verbose_set(device, CDDA_MESSAGE_FORGETIT, CDDA_MESSAGE_FORGETIT);
    long int start = cdda_track_firstsector(device, tracknum);
    long int end = cdda_track_lastsector(device, tracknum);

    cdrom_paranoia *paranoia = paranoia_init(device);
    paranoia_modeset(paranoia, PARANOIA_MODE_FULL | PARANOIA_MODE_NEVERSKIP);
    paranoia_seek(paranoia, start, SEEK_SET);

    long int curpos = start;
    int16_t *buffer;

    current->setTotalSteps(end - start + 1);

    qApp->processEvents();

    int every15 = 15;
    while (curpos < end)
    {
        buffer = paranoia_read(paranoia, paranoia_cb);
        fwrite(buffer, CD_FRAMESIZE_RAW, 1, output);
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
    fclose(output);

    return (end - start + 1) * CD_FRAMESIZE_RAW;
}
