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

InfoDialog::InfoDialog(ProgramInfo *pginfo, MythMainWindow *parent, 
                       const char *name)
          : MythDialog(parent, name)
{
    int mediumfont = gContext->GetMediumFontSize();

    QVBoxLayout *vbox = new QVBoxLayout(this, (int)(20 * wmult));

    if (pginfo)
    {
        QGridLayout *grid = pginfo->DisplayWidget(NULL, this);
        vbox->addLayout(grid);
    }

    QFrame *f = new QFrame(this);
    f->setFrameStyle(QFrame::HLine | QFrame::Plain);
    f->setLineWidth((int)(4 * hmult));
    vbox->addWidget(f);    

    recordstatus = pginfo->GetProgramRecordingStatus(QSqlDatabase::database());

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
    connect(lview, SIGNAL(infoPressed(QListViewItem *)), this,
            SLOT(advancedEdit(QListViewItem *)));

    RecListItem *item = new RecListItem(lview, tr("Record this program whenever"
                                        " it's shown anywhere"), kAllRecord);
    if (recordstatus == kAllRecord)
        selectItem = item;

    item = new RecListItem(lview, tr("Record this program whenever it's shown "
                           "on this channel"), kChannelRecord);
    if (recordstatus == kChannelRecord)
        selectItem = item;

    programtype = pginfo->IsProgramRecurring();

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

    item = new RecListItem(lview, tr("Record only this showing of the program"),
                           kSingleRecord);
    if (recordstatus == kSingleRecord)
        selectItem = item;

    item = new RecListItem(lview, tr("Don't record this program"), 
                           kNotRecording);
    if (selectItem == NULL)
        selectItem = item;

    vbox->addWidget(lview, 0);

    lview->setCurrentItem(selectItem);
    lview->setSelected(selectItem, true);

    lview->setFocus();
 
    myinfo = pginfo;
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
        myinfo->ApplyRecordStateChange(QSqlDatabase::database(),
                                       currentSelected);

    if (selitem)
        accept();
}

void InfoDialog::advancedEdit(QListViewItem *)
{
    QSqlDatabase *m_db = QSqlDatabase::database();;
    ScheduledRecording record;
    record.loadByProgram(m_db, myinfo);
    record.exec(m_db);

    reject();
}

