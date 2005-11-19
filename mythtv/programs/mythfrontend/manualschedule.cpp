#include <qlayout.h>
#include <qpushbutton.h>
#include <qbuttongroup.h>
#include <qlabel.h>
#include <qcursor.h>
#include <qlistview.h>
#include <qdatetime.h>
#include <qprogressbar.h>
#include <qapplication.h>
#include <qtimer.h>
#include <qimage.h>
#include <qpainter.h>
#include <qheader.h>
#include <qfile.h>
#include <qsqldatabase.h>
#include <qregexp.h>
#include <qhbox.h>
#include <qdatetimeedit.h>

#include <unistd.h>

#include <iostream>
using namespace std;

#include "tv.h"
#include "NuppelVideoPlayer.h"
#include "yuv2rgb.h"
#include "manualschedule.h"

#include "libmyth/mythcontext.h"
#include "libmyth/mythdbcon.h"
#include "libmyth/dialogbox.h"
#include "libmythtv/programinfo.h"
#include "libmythtv/scheduledrecording.h"
#include "libmythtv/recordingtypes.h"
#include "libmythtv/remoteutil.h"

ManualSchedule::ManualSchedule(MythMainWindow *parent, const char *name)
              : MythDialog(parent, name)
{
    m_nowDateTime = QDateTime::currentDateTime();
    m_startDateTime = m_nowDateTime;
    daysahead = 0;
    
    QVBoxLayout *vbox = new QVBoxLayout(this, (int)(20 * wmult));

    dateformat = gContext->GetSetting("DateFormat", "ddd MMMM d");
    shortdateformat = gContext->GetSetting("ShortDateFormat", "M/d");
    timeformat = gContext->GetSetting("TimeFormat", "h:mm AP");


    // Window title
    QString message = tr("Manual Recording Scheduler");
    QLabel *label = new QLabel(message, this);
    label->setBackgroundOrigin(WindowOrigin);
    label->setSizePolicy(QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed));
    vbox->addWidget(label);

    QVBoxLayout *vkbox = new QVBoxLayout(vbox, (int)(1 * wmult));
    QHBoxLayout *hbox = new QHBoxLayout(vkbox, (int)(1 * wmult));

    // Channel
    hbox = new QHBoxLayout(vbox, (int)(10 * wmult));

    message = tr("Channel:");
    label = new QLabel(message, this);
    label->setBackgroundOrigin(WindowOrigin);
    label->setSizePolicy(QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed));
    hbox->addWidget(label);

    m_channel = new MythComboBox( false, this, "channel");
    m_channel->setBackgroundOrigin(WindowOrigin);


    QString chanorder = gContext->GetSetting("ChannelOrdering", "channum + 0");

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(QString("SELECT chanid, channum, callsign, name "
                          "FROM channel GROUP BY channum, callsign "
                          "ORDER BY %1;").arg(chanorder));

    QString longChannelFormat = 
        gContext->GetSetting("LongChannelFormat", "<num> <name>");

    if (query.exec() && query.isActive() && query.size()) {
      while(query.next()) {
          QString channel = longChannelFormat;
          channel.replace("<num>", query.value(1).toString())
              .replace("<sign>", QString::fromUtf8(query.value(2).toString()))
              .replace("<name>", QString::fromUtf8(query.value(3).toString()));
          m_channel->insertItem(channel);
          m_chanids << query.value(0).toString();
      }
      
    }

    hbox->addWidget(m_channel);

    // Program Date
    hbox = new QHBoxLayout(vbox, (int)(10 * wmult));

    message = tr("Date or day of the week") + ": ";
    label = new QLabel(message, this);
    label->setBackgroundOrigin(WindowOrigin);
    label->setSizePolicy(QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed));
    hbox->addWidget(label);

    m_startdate = new MythComboBox(false, this, "startdate");

    for(int m_index = 0; m_index <= 60; m_index++)
    {
        m_startdate->insertItem(m_nowDateTime.addDays(m_index)
                              .toString(dateformat));
        if (m_nowDateTime.addDays(m_index).toString("MMdd") ==
            m_startDateTime.toString("MMdd"))
            m_startdate->setCurrentItem(m_startdate->count() - 1);
    }
    hbox->addWidget(m_startdate);

    hbox = new QHBoxLayout(vbox, (int)(10 * wmult));

    QTime thisTime = m_nowDateTime.time();
    thisTime = thisTime.addSecs((30 - thisTime.minute() % 30) * 60);
    
    if (thisTime < QTime::QTime(0,30))
        m_startdate->setCurrentItem(m_startdate->currentItem() + 1);

    message = tr("Time:");
    label = new QLabel(message, this);
    label->setBackgroundOrigin(WindowOrigin);
    label->setSizePolicy(QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed));
    hbox->addWidget(label);

    QString hr_format = "h";
    if (timeformat.contains("hh"))
        hr_format = "hh";
    if (timeformat.contains("AP"))
        hr_format += " AP";
    if (timeformat.contains("ap"))
        hr_format += " ap";

    m_starthour = new MythComboBox(false, this, "starthour");

    for(int m_index = -1; m_index <= 24; m_index++)
    {
        m_starthour->insertItem(QTime::QTime((m_index + 24) % 24, 0)
                                             .toString(hr_format));
        if (thisTime.hour() == m_index)
            m_starthour->setCurrentItem(m_starthour->count() - 1);
    }
    hbox->addWidget(m_starthour);

    m_startminute = new MythComboBox(false, this, "startminute");

    for(int m_index = -5; m_index <= 60; m_index += 5)
    {
        m_startminute->insertItem(QTime::QTime(0, (m_index + 60) % 60)
                                               .toString(":mm"));
        if (m_index == thisTime.minute())
            m_startminute->setCurrentItem(m_startminute->count() - 1);
    }
    hbox->addWidget(m_startminute);
    dateChanged();

    // Duration spin box
    message = tr("Duration:");
    label = new QLabel(message, this);
    label->setBackgroundOrigin(WindowOrigin);
    label->setSizePolicy(QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed));
    hbox->addWidget(label);

    m_duration = new MythComboBox(false, this, "duration");

    m_duration->insertItem(QString(" 1 %2").arg(tr("minute")));
    for(int m_index = 5; m_index <= 360; m_index += 5)
    {
        m_duration->insertItem(QString(" %1 %2").arg(m_index)
                                               .arg(tr("minutes")));
    }
    m_duration->setCurrentItem(12);
    hbox->addWidget(m_duration);

    // Title edit box
    hbox = new QHBoxLayout(vbox, (int)(10 * wmult));

    message = tr("Title (optional):");
    label = new QLabel(message, this);
    label->setBackgroundOrigin(WindowOrigin);
    label->setSizePolicy(QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed));
    hbox->addWidget(label);

    m_title = new MythRemoteLineEdit( this, "title" );
    m_title->setBackgroundOrigin(WindowOrigin);
    hbox->addWidget(m_title);

    //  Record Button
    hbox = new QHBoxLayout(vbox, (int)(10 * wmult));

    m_recordButton = new MythPushButton( this, "Program" );
    m_recordButton->setBackgroundOrigin(WindowOrigin);
    m_recordButton->setText( tr( "Set Recording Options" ) );
    m_recordButton->setEnabled(true);

    hbox->addWidget(m_recordButton);

    //  Cancel Button
    hbox = new QHBoxLayout(vbox, (int)(10 * wmult));

    m_cancelButton = new MythPushButton( this, "Program" );
    m_cancelButton->setBackgroundOrigin(WindowOrigin);
    m_cancelButton->setText( tr( "Cancel" ) );
    m_cancelButton->setEnabled(true);

    hbox->addWidget(m_cancelButton);



    connect(this, SIGNAL(dismissWindow()), this, SLOT(accept()));

     
    connect(m_startdate, SIGNAL(activated(int)), this, SLOT(dateChanged(void)));
    connect(m_startdate, SIGNAL(highlighted(int)), this, SLOT(dateChanged(void)));
    connect(m_starthour, SIGNAL(activated(int)), this, SLOT(hourChanged(void)));
    connect(m_starthour, SIGNAL(highlighted(int)), this, SLOT(hourChanged(void)));
    connect(m_startminute, SIGNAL(activated(int)), this, SLOT(minuteChanged(void)));
    connect(m_startminute, SIGNAL(highlighted(int)), this, SLOT(minuteChanged(void)));
    connect(m_recordButton, SIGNAL(clicked()), this, SLOT(recordClicked()));
    connect(m_cancelButton, SIGNAL(clicked()), this, SLOT(cancelClicked()));

    m_channel->setFocus();
    
    gContext->addListener(this);
}

ManualSchedule::~ManualSchedule(void)
{
    gContext->removeListener(this);
}

void ManualSchedule::minuteChanged(void)
{
    if (m_startminute->currentItem() == 0 ) {
        m_startminute->setCurrentItem(12);
        m_starthour->setCurrentItem(m_starthour->currentItem() - 1);
    }
    if (m_startminute->currentItem() == 13 ) {
        m_starthour->setCurrentItem(m_starthour->currentItem() + 1);
        m_startminute->setCurrentItem(1);
    }
    dateChanged();
}

void ManualSchedule::hourChanged(void)
{
    if (m_starthour->currentItem() == 0 ) {
        m_starthour->setCurrentItem(24);
        m_startdate->setCurrentItem(m_startdate->currentItem() - 1);
    }
    if (m_starthour->currentItem() == 25 ) {
        m_startdate->setCurrentItem(m_startdate->currentItem() + 1);
        m_starthour->setCurrentItem(1);
    }
    dateChanged();
}

void ManualSchedule::dateChanged(void)
{
   daysahead = m_startdate->currentItem();
   m_startDateTime.setDate(m_nowDateTime.addDays(daysahead).date());

   int hr = (m_starthour->currentItem() - 1) % 24;
   int min = (m_startminute->currentItem() - 1) * 5 % 60;
   m_startDateTime.setTime(QTime(hr, min));

   VERBOSE(VB_SCHEDULE, QString("Start Date Time: %1")
                                .arg(m_startDateTime.toString()));

   if (m_startDateTime < m_nowDateTime)
   {
        QTime thisTime = m_nowDateTime.time();
        m_starthour->setCurrentItem(thisTime.hour() + 1);
        m_startDateTime.setDate(m_nowDateTime.date());
        m_startDateTime.setTime(QTime(hr, min));
   }
}

void ManualSchedule::recordClicked(void)
{
    ProgramInfo p;

    QString channelFormat = gContext->GetSetting("ChannelFormat", "<num> <sign>");
    p.chanid = m_chanids[m_channel->currentItem()];

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT chanid, channum, callsign, name "
                  "FROM channel WHERE chanid=:CHANID");
    query.bindValue(":CHANID", p.chanid);

    query.exec();

    if (query.isActive() && query.size()) 
    {
        query.next();
        p.chanstr = query.value(1).toString();
        p.chansign = QString::fromUtf8(query.value(2).toString());
        p.channame = QString::fromUtf8(query.value(3).toString());
    }

    int addsec = m_duration->currentItem() * 300;

    if (!addsec)
        addsec = 60;

    p.startts = m_startDateTime;
    p.endts = p.startts.addSecs(addsec);

    if (m_title->text() > "")
        p.title = m_title->text();
    else
        p.title = p.ChannelText(channelFormat) + " - " + 
                  p.startts.toString(timeformat);

    p.title += " (" + tr("Manual Record") + ")";

    ScheduledRecording record;

    record.loadByProgram(&p);
    record.setSearchType(kManualSearch);
    record.exec();

    if (record.getRecordID())
        accept();
    else
        m_recordButton->setFocus();
}

void ManualSchedule::cancelClicked(void)
{
    accept();
}
