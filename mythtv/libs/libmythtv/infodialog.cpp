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
#include "proglist.h"

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

InfoDialog::InfoDialog(ProgramInfo *pginfo, MythMainWindow *parent, 
                       const char *name)
          : MythDialog(parent, name)
{
    myinfo = new ProgramInfo(*pginfo);

    QFont mediumfont = gContext->GetMediumFont();

    QVBoxLayout *vbox = new QVBoxLayout(this, (int)(20 * wmult));

    if (myinfo)
    {
        QGridLayout *grid = myinfo->DisplayWidget(this, "");
        vbox->addLayout(grid);
    }

    QFrame *f = new QFrame(this);
    f->setFrameStyle(QFrame::HLine | QFrame::Plain);
    f->setLineWidth((int)(4 * hmult));
    vbox->addWidget(f);    

    programtype = myinfo->IsProgramRecurring();
    recordstatus = myinfo->GetProgramRecordingStatus();

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
    lview->setItemMargin((int)(hmult * mediumfont.pointSize() / 2));
    lview->setFixedHeight((int)(225 * hmult));
    lview->header()->hide();

    lview->installEventFilter(this);

    RecListItem *item;

    if (recordstatus != kOverrideRecord && recordstatus != kDontRecord)
    {

        item = new RecListItem(lview, tr("Record this program whenever "
                                         "it's shown anywhere"), kAllRecord);
        if (recordstatus == kAllRecord)
            selectItem = item;

        item = new RecListItem(lview, tr("Record this program whenever it's "
                                         "shown on this channel"), 
                               kChannelRecord);
        if (recordstatus == kChannelRecord)
            selectItem = item;

        QString msg = "Shouldn't show up.";
        RecordingType rt = kNotRecording;
        if (programtype == 2 ||
            programtype == -1 ||
            recordstatus == kWeekslotRecord)
        {
            msg = tr("Record this program in this timeslot every week");
            rt = kTimeslotRecord;
        }
        else if (programtype == 1)
        {
            msg = tr("Record this program in this timeslot every day");
            rt = kTimeslotRecord;
        }

        if (programtype != 0)
        {
            item = new RecListItem(lview, msg, rt);
            if ((recordstatus == kTimeslotRecord) ||
                (recordstatus == kWeekslotRecord))
                selectItem = item;
        }

        if (recordstatus == kFindOneRecord)
        {
            msg = tr("Record one showing of this title");
            rt = kFindOneRecord;
            item = new RecListItem(lview, msg, kFindOneRecord);
            selectItem = item;
        }

        item = new RecListItem(lview, tr("Record only this showing of the "
                                         "program"), kSingleRecord);
        if (recordstatus == kSingleRecord)
            selectItem = item;

        item = new RecListItem(lview, tr("Don't record this program"), 
                               kNotRecording);
        if (selectItem == NULL)
            selectItem = item;
    }
    else
    {
        item = new RecListItem(lview, tr("Don't record this showing"), 
                               kDontRecord);
        if (recordstatus == kDontRecord)
            selectItem = item;

        item = new RecListItem(lview, tr("Record this showing with override "
                                         "options"), kOverrideRecord);
        if (recordstatus == kOverrideRecord)
            selectItem = item;

        item = new RecListItem(lview, tr("Record this showing with normal "
                                         "options"), kNotRecording);
        if (selectItem == NULL)
            selectItem = item;
    }

    vbox->addWidget(lview, 0);

    f = new QFrame(this);
    f->setFrameStyle(QFrame::HLine | QFrame::Plain);
    f->setLineWidth((int)(4 * hmult));
    vbox->addWidget(f);

    QLabel *legend1 = new QLabel(tr("To go to the advanced recordings screen, "
                                    "press 'i'"), this);
    legend1->setBackgroundOrigin(WindowOrigin);
    vbox->addWidget(legend1, 0);

    QLabel *legend2 = new QLabel(tr("To see a list of all up-coming showings "
                                    "of this program, press '5'"), this);
    legend2->setBackgroundOrigin(WindowOrigin);
    vbox->addWidget(legend2, 0);

    lview->setCurrentItem(selectItem);
    lview->setSelected(selectItem, true);

    lview->setFocus();
 
}

InfoDialog::~InfoDialog()
{
    delete myinfo;
} 

QLabel *InfoDialog::getDateLabel(ProgramInfo *pginfo)
{
    QDateTime startts = pginfo->startts;
    QDateTime endts = pginfo->endts;

    QString dateformat = gContext->GetSetting("DateFormat", "ddd MMMM d");
    QString timeformat = gContext->GetSetting("TimeFormat", "h:mm AP");

    QString timedate = startts.date().toString(dateformat) + QString(", ") +
                       startts.time().toString(timeformat) + QString(" - ") +
                       endts.time().toString(timeformat);

    QLabel *date = new QLabel(timedate, this);

    return date;
}

bool InfoDialog::eventFilter(QObject *o, QEvent *e)
{
    (void)o;

    if (e->type() != QEvent::KeyPress)
        return false;

    QKeyEvent *ke = (QKeyEvent *)e;

    bool handled = false;
    QStringList actions;
    gContext->GetMainWindow()->TranslateKeyPress("qt", ke, actions);

    for (unsigned int i = 0; i < actions.size() && !handled; i++)
    {
        QString action = actions[i];
        handled = true;

        if (action == "MENU" || action == "INFO")
            advancedEdit(lview->currentItem());
        else if (action == "SELECT")
            selected(lview->currentItem());
        else if (action == "0" || action == "1" || action == "2" ||
                 action == "3" || action == "4" || action == "5" ||
                 action == "6" || action == "7" || action == "8" ||
                 action == "9")
        {
            numberPress(lview->currentItem(), action.toInt());
        }
        else
            handled = false;
    }

    return handled;
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

void InfoDialog::advancedEdit(QListViewItem *)
{
    ScheduledRecording record;
    record.loadByProgram(myinfo);

    setFocusPolicy(QWidget::NoFocus);
    lview->setFocusPolicy(QWidget::NoFocus);
    record.exec();

    reject();
}

void InfoDialog::numberPress(QListViewItem *, int num)
{
    if (num == 5)
    {
        ProgLister *pl = new ProgLister(plTitle, myinfo->title, 
                                        gContext->GetMainWindow(), "proglist");
        pl->exec();
        delete pl;

        lview->setFocus();
    }
}
