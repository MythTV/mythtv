#include <qlayout.h>
#include <qpushbutton.h>
#include <qlabel.h>
#include <qdatetime.h>
#include <qcursor.h>

#include "infodialog.h"
#include "guidegrid.h"

InfoDialog::InfoDialog(ProgramInfo *pginfo, QWidget *parent, const char *name)
          : QDialog(parent, name)
{
    setGeometry(0, 0, 800, 600);
    setFixedWidth(800); 
    setFixedHeight(600);

    setFont(QFont("Arial", 14, QFont::Bold));
    setCursor(QCursor(Qt::BlankCursor));

    QPushButton *ok = new QPushButton("Close", this, "close");
    ok->setFont(QFont("Arial", 20, QFont::Bold));

    QPushButton *cancel = new QPushButton("Cancel", this, "cancel");
    cancel->setFont(QFont("Arial", 20, QFont::Bold));

    connect(ok, SIGNAL(clicked()), this, SLOT(accept()));
    connect(cancel, SIGNAL(clicked()), this, SLOT(reject()));

    QVBoxLayout *vbox = new QVBoxLayout(this, 20);

    QGridLayout *grid = new QGridLayout(vbox, 4, 3, 10);
    
    QLabel *titlefield = new QLabel(pginfo->title, this);
    titlefield->setFont(QFont("Arial", 25, QFont::Bold));

    QLabel *date = getDateLabel(pginfo);

    QLabel *subtitlelabel = new QLabel("Episode:", this);
    QLabel *subtitlefield = new QLabel(pginfo->subtitle, this);
    QLabel *descriptionlabel = new QLabel("Description:", this);
    QLabel *descriptionfield = new QLabel(pginfo->description, this);
    descriptionfield->setAlignment(Qt::WordBreak | Qt::AlignLeft | 
                                   Qt::AlignTop);

    grid->addMultiCellWidget(titlefield, 0, 0, 0, 1, Qt::AlignLeft);
    grid->addMultiCellWidget(date, 1, 1, 0, 1, Qt::AlignLeft);
    grid->addWidget(subtitlelabel, 2, 0, Qt::AlignLeft | Qt::AlignTop);
    grid->addWidget(subtitlefield, 2, 1, Qt::AlignLeft | Qt::AlignTop);
    grid->addWidget(descriptionlabel, 3, 0, Qt::AlignLeft | Qt::AlignTop);
    grid->addWidget(descriptionfield, 3, 1, Qt::AlignLeft | Qt::AlignTop);

    grid->setColStretch(1, 10);
    grid->setRowStretch(3, 1);

    //vbox->addStretch(1);

    QHBoxLayout *bottomBox = new QHBoxLayout(vbox);

    bottomBox->addWidget(ok, 1, Qt::AlignCenter);
    bottomBox->addWidget(cancel, 1, Qt::AlignCenter);

    vbox->activate();

    showFullScreen();
}

QLabel *InfoDialog::getDateLabel(ProgramInfo *pginfo)
{
    QString hour, min;
    hour = pginfo->starttime.mid(11, 2);
    min = pginfo->starttime.mid(14, 2);

    QTime start(hour.toInt(), min.toInt());

    hour = pginfo->endtime.mid(11, 2);
    min = pginfo->endtime.mid(14, 2);

    QTime end(hour.toInt(), min.toInt());

    QString year, month, day;

    year = pginfo->endtime.mid(0, 4);
    month = pginfo->endtime.mid(5, 2);
    day = pginfo->endtime.mid(8, 2);

    QDate thedate(year.toInt(), month.toInt(), day.toInt());
    
    QString timedate = thedate.toString("ddd MMMM d") + QString(", ") +
                       start.toString("h:mm AP") + QString(" - ") +
                       end.toString("h:mm AP");
    QLabel *date = new QLabel(timedate, this);

    return date;
}
