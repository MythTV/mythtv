#include <qlayout.h>
#include <qpushbutton.h>
#include <qlabel.h>
#include <qdatetime.h>
#include <qcursor.h>
#include <qcheckbox.h>
#include <qvgroupbox.h>
#include <qapplication.h>
#include <qlistview.h>
#include <qheader.h>

#include <iostream>
using namespace std;

#include "infodialog.h"
#include "infostructs.h"
#include "programinfo.h"
#include "oldsettings.h"
#include "mythcontext.h"

class RecListItem : public QListViewItem
{
  public:
    RecListItem(QListView *parent, QString text, RecordingType type)
               : QListViewItem(parent, text)
             { m_type = type; }

   ~RecListItem() { }

    RecordingType GetType(void) { return m_type; }
 
  private:
    RecordingType m_type;
};

InfoDialog::InfoDialog(MythContext *context, ProgramInfo *pginfo, 
                       QWidget *parent, const char *name)
          : MythDialog(context, parent, name)
{
    int bigfont = m_context->GetBigFontSize();
    int mediumfont = m_context->GetMediumFontSize();

    QVBoxLayout *vbox = new QVBoxLayout(this, (int)(20 * wmult));

    QGridLayout *grid = new QGridLayout(vbox, 4, 2, (int)(10 * wmult));
    
    QLabel *titlefield = new QLabel(pginfo->title, this);
    titlefield->setBackgroundOrigin(WindowOrigin);
    titlefield->setFont(QFont("Arial", (int)(bigfont * hmult), QFont::Bold));

    QLabel *date = getDateLabel(pginfo);
    date->setBackgroundOrigin(WindowOrigin);

    QLabel *subtitlelabel = new QLabel("Episode:", this);
    subtitlelabel->setBackgroundOrigin(WindowOrigin);
    QLabel *subtitlefield = new QLabel(pginfo->subtitle, this);
    subtitlefield->setBackgroundOrigin(WindowOrigin);
    subtitlefield->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    QLabel *descriptionlabel = new QLabel("Description:", this);
    descriptionlabel->setBackgroundOrigin(WindowOrigin);
    QLabel *descriptionfield = new QLabel(pginfo->description, this);
    descriptionfield->setBackgroundOrigin(WindowOrigin);
    descriptionfield->setAlignment(Qt::WordBreak | Qt::AlignLeft | 
                                   Qt::AlignTop);

    qApp->processEvents();

    int descwidth = (int)(800 * wmult) - descriptionlabel->width() - 100;
    int titlewidth = (int)(760 * wmult);

    titlefield->setMinimumWidth(titlewidth);
    titlefield->setMaximumWidth(titlewidth);

    subtitlefield->setMinimumWidth(descwidth);
    subtitlefield->setMaximumWidth(descwidth);

    descriptionfield->setMinimumWidth(descwidth);
    descriptionfield->setMaximumWidth(descwidth);

    grid->addMultiCellWidget(titlefield, 0, 0, 0, 1, Qt::AlignLeft);
    grid->addMultiCellWidget(date, 1, 1, 0, 1, Qt::AlignLeft);
    grid->addWidget(subtitlelabel, 2, 0, Qt::AlignLeft | Qt::AlignTop);
    grid->addWidget(subtitlefield, 2, 1, Qt::AlignLeft | Qt::AlignTop);
    grid->addWidget(descriptionlabel, 3, 0, Qt::AlignLeft | Qt::AlignTop);
    grid->addWidget(descriptionfield, 3, 1, Qt::AlignLeft | Qt::AlignTop);

    grid->setColStretch(1, 2);
    grid->setRowStretch(3, 1);

    //vbox->addStretch(1);

    QFrame *f = new QFrame(this);
    f->setFrameStyle(QFrame::HLine | QFrame::Plain);
    f->setLineWidth((int)(4 * hmult));
    vbox->addWidget(f);    

    recordstatus = pginfo->GetProgramRecordingStatus();

    if (recordstatus == kTimeslotRecord && programtype == 0)
    {
        cerr << "error, somehow set to record timeslot and it doesn't seem to "
                "have one\n";
        recordstatus = kSingleRecord;
    }

    RecListItem *selectItem = NULL;

    lview = new MythListView(this);
    lview->addColumn("Selections");
    lview->setColumnWidth(0, (int)(750 * wmult));
    lview->setSorting(-1, false);
    lview->setAllColumnsShowFocus(true);
    lview->setItemMargin((int)(hmult * mediumfont / 2));
    lview->setFixedHeight((int)(225 * hmult));
    lview->header()->hide();

    connect(lview, SIGNAL(returnPressed(QListViewItem *)), this,
            SLOT(selected(QListViewItem *)));
    connect(lview, SIGNAL(spacePressed(QListViewItem *)), this,
            SLOT(selected(QListViewItem *)));

    RecListItem *item = new RecListItem(lview, "Record this program whenever "
                                        "it's shown anywhere", kAllRecord);
    if (recordstatus == kAllRecord)
        selectItem = item;

    item = new RecListItem(lview, "Record this program whenever it's shown "
                                  "on this channel", kChannelRecord);
    if (recordstatus == kChannelRecord)
        selectItem = item;

    programtype = pginfo->IsProgramRecurring();

    QString msg = "Shouldn't show up.";
    if (programtype == 1)
        msg = "Record this program in this timeslot every day";
    else if (programtype == 2 || programtype == -1)
        msg = "Record this program in this timeslot every week";
    if (programtype != 0)
    {
        item = new RecListItem(lview, msg, kTimeslotRecord);
        if (recordstatus == kTimeslotRecord)
            selectItem = item;
    }

    item = new RecListItem(lview, "Record only this showing of the program",
                           kSingleRecord);
    if (recordstatus == kSingleRecord)
        selectItem = item;

    item = new RecListItem(lview, "Don't record this program", kNotRecording);
    if (selectItem == NULL)
        selectItem = item;

    vbox->addWidget(lview, 0);

    lview->setCurrentItem(selectItem);
    lview->setSelected(selectItem, true);
 
    myinfo = pginfo;
     
    showFullScreen();
    setActiveWindow();
}

QLabel *InfoDialog::getDateLabel(ProgramInfo *pginfo)
{
    QDateTime startts = pginfo->startts;
    QDateTime endts = pginfo->endts;

    QString dateformat = m_context->GetSetting("DateFormat");
    if (dateformat == "")
        dateformat = "ddd MMMM d";
    QString timeformat = m_context->GetSetting("TimeFormat");
    if (timeformat == "")
        timeformat = "h:mm AP";

    QString timedate = endts.date().toString(dateformat) + QString(", ") +
                       startts.time().toString(timeformat) + QString(" - ") +
                       endts.time().toString(timeformat);

    QLabel *date = new QLabel(timedate, this);

    return date;
}

void InfoDialog::hideEvent(QHideEvent *e)
{
//    selected(NULL);
    QDialog::hideEvent(e);
}

void InfoDialog::selected(QListViewItem *selitem)
{
    RecordingType currentSelected = kNotRecording;

    QListViewItem *item = lview->firstChild();

    while (item && item != lview->currentItem())
        item = item->nextSibling();

    if (!item)
        item = lview->firstChild();    

    RecListItem *realitem = (RecListItem *)item;

    currentSelected = realitem->GetType();

    if (currentSelected != recordstatus)
        myinfo->ApplyRecordStateChange(currentSelected);   

    if (selitem)
        accept();
}
