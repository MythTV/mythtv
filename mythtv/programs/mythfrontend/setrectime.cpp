#include <qlayout.h>
#include <qpushbutton.h>
#include <qbuttongroup.h>
#include <qlabel.h>
#include <qcursor.h>
#include <qsqldatabase.h>
#include <qlistview.h>
#include <qdatetime.h>
#include <qapplication.h>
#include <qregexp.h>
#include <qdatetimeedit.h>
#include <qheader.h>

#include <iostream>
using namespace std;

#include "setrectime.h"
#include "tv.h"
#include "programlistitem.h"

#include "libmyth/dialogbox.h"
#include "libmyth/mythcontext.h"

class SRTListItem : public QListViewItem
{
public:
    SRTListItem(QListView *parent, QString text, int type)
        : QListViewItem(parent, text)
    { m_type = type; }

    ~SRTListItem() { }

    int GetType(void) { return m_type; }

private:
    int m_type;
};

SetRecTimeDialog::SetRecTimeDialog(ProgramInfo *rec, QSqlDatabase *ldb, 
                                   QWidget *parent, const char *name)
                : MythDialog(parent, name)
{
    m_proginfo = rec;
    db = ldb;

    int bigfont = gContext->GetBigFontSize();
    int mediumfont = gContext->GetMediumFontSize();

    QVBoxLayout *vbox = new QVBoxLayout(this, (int)(20 * wmult));

    QLabel *desclabel = new QLabel("Change recording time:", this);
    desclabel->setBackgroundOrigin(WindowOrigin);
    vbox->addWidget(desclabel);

    QGridLayout *grid = new QGridLayout(vbox, 6, 2, (int)(10 * wmult));

    QLabel *titlefield = new QLabel(rec->title, this);
    titlefield->setBackgroundOrigin(WindowOrigin);
    titlefield->setFont(QFont("Arial", (int)(bigfont * hmult), QFont::Bold));

    QLabel *date = getDateLabel(rec);
    date->setBackgroundOrigin(WindowOrigin);


    QLabel *subtitlelabel = new QLabel("Episode:", this);
    subtitlelabel->setBackgroundOrigin(WindowOrigin);
    QLabel *subtitlefield = new QLabel(rec->subtitle, this);
    subtitlefield->setBackgroundOrigin(WindowOrigin);
    subtitlefield->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    QLabel *descriptionlabel = new QLabel("Description:", this);
    descriptionlabel->setBackgroundOrigin(WindowOrigin);
    QLabel *descriptionfield = new QLabel(rec->description, this);
    descriptionfield->setBackgroundOrigin(WindowOrigin);
    descriptionfield->setAlignment(Qt::WordBreak | Qt::AlignLeft |
                                   Qt::AlignTop);

    descriptionfield->setMinimumWidth((int)(800 * wmult -
                                      descriptionlabel->width() - 100 * wmult));

    QLabel *startlabel = new QLabel("Start Time: ", this);
    startlabel->setFont(QFont("Arial", (int)(bigfont * hmult), QFont::Bold));
    startlabel->setBackgroundOrigin(WindowOrigin);

    QLabel *endlabel = new QLabel("End Time: ", this);
    endlabel->setFont(QFont("Arial", (int)(bigfont * hmult), QFont::Bold));

    endlabel->setBackgroundOrigin(WindowOrigin);

    m_startte = new QDateTimeEdit (rec->startts,this);
    m_startte->setFont(QFont("Arial", (int)(bigfont * hmult), QFont::Bold));
    m_startte->setBackgroundOrigin(WindowOrigin);
    m_startte->setAutoAdvance(TRUE);

    m_endte = new QDateTimeEdit (rec->endts,this);
    m_endte->setFont(QFont("Arial", (int)(bigfont * hmult), QFont::Bold));
    m_endte->setBackgroundOrigin(WindowOrigin);
    m_endte->setAutoAdvance(TRUE);

    grid->addMultiCellWidget(titlefield, 0, 0, 0, 1, Qt::AlignLeft);
    grid->addMultiCellWidget(date, 1, 1, 0, 1, Qt::AlignLeft);
    grid->addWidget(subtitlelabel, 2, 0, Qt::AlignLeft | Qt::AlignTop);
    grid->addWidget(subtitlefield, 2, 1, Qt::AlignLeft | Qt::AlignTop);
    grid->addWidget(descriptionlabel, 3, 0, Qt::AlignLeft | Qt::AlignTop);
    grid->addWidget(descriptionfield, 3, 1, Qt::AlignLeft | Qt::AlignTop);

    grid->setColStretch(1, 2);
    grid->setRowStretch(3, 1);

    grid->addWidget(startlabel, 4, 0, Qt::AlignLeft | Qt::AlignTop);
    grid->addWidget(m_startte, 4, 1, Qt::AlignLeft | Qt::AlignTop);
    grid->addWidget(endlabel, 5, 0, Qt::AlignLeft | Qt::AlignTop);
    grid->addWidget(m_endte, 5, 1, Qt::AlignLeft | Qt::AlignTop);

    QFrame *f = new QFrame(this);
    f->setFrameStyle(QFrame::HLine | QFrame::Plain);
    f->setLineWidth((int)(4 * hmult));
    vbox->addWidget(f);

    SRTListItem *selectItem = NULL;

    lview = new MythListView(this);
    lview->addColumn("Selections");
    lview->setColumnWidth(0, (int)(650 * wmult));
    lview->setSorting(-1, false);
    lview->setAllColumnsShowFocus(true);
    lview->setItemMargin((int)(hmult * mediumfont / 2));
    lview->setFixedHeight((int)(225 * hmult));
    lview->header()->hide();

    connect(lview, SIGNAL(returnPressed(QListViewItem *)), this,
            SLOT(selected(QListViewItem *)));
    connect(lview, SIGNAL(spacePressed(QListViewItem *)), this,
            SLOT(selected(QListViewItem *)));

    QString msg;

    if (m_proginfo->IsProgramRecurring())
    {
        msg = "Change recording timeslot";
    }
    else
    {
        msg = "Change recording time";
    }

    SRTListItem *item = new SRTListItem(lview,msg,1);
    selectItem = item;

    vbox->addWidget(lview, 0);

    lview->setCurrentItem(selectItem);
    lview->setSelected(selectItem, true);
}

QLabel *SetRecTimeDialog::getDateLabel(ProgramInfo *pginfo)
{
    QDateTime startts = pginfo->startts;
    QDateTime endts = pginfo->endts;

    QString dateformat = gContext->GetSetting("DateFormat", "ddd MMMM d");
    QString timeformat = gContext->GetSetting("TimeFormat", "h:mm AP");

    QString timedate = endts.date().toString(dateformat) + QString(", ") +
                       startts.time().toString(timeformat) + QString(" - ") +
                       endts.time().toString(timeformat);

    QLabel *date = new QLabel(timedate, this);

    return date;
}

void SetRecTimeDialog::hideEvent(QHideEvent *e)
{
//    selected(NULL);
    QDialog::hideEvent(e);
}

void SetRecTimeDialog::selected(QListViewItem *selitem)
{
    int currentSelected = 0;

    QListViewItem *item = lview->firstChild();

    while (item && item != lview->currentItem())
        item = item->nextSibling();

    if (!item)
        item = lview->firstChild();

    SRTListItem *realitem = (SRTListItem *)item;

    currentSelected = realitem->GetType();

    if (currentSelected == 1)
    {
        QDateTime sdt = m_startte->dateTime();
        QDateTime edt = m_endte->dateTime();
        m_proginfo->ApplyRecordTimeChange(db, sdt, edt);
    }

    if (selitem)
        accept();
}

