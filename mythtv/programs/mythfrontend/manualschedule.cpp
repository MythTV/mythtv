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
#include "libmyth/dialogbox.h"
#include "libmythtv/programinfo.h"
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
    
    QSqlQuery query;

    QString chanorder = gContext->GetSetting("ChannelOrdering", "channum + 0");

    QString thequery= QString("SELECT chanid, channum, callsign, name "
                              "FROM channel GROUP BY channum, callsign "
                              "ORDER BY %1;").arg(chanorder);

    query.exec(thequery);

    QString longChannelFormat = 
        gContext->GetSetting("LongChannelFormat", "<num> <name>");

    if (query.isActive() && query.numRowsAffected()) {
      while(query.next()) {
          QString channel = longChannelFormat;
          channel.replace("<num>", query.value(1).toString())
              .replace("<sign>", QString::fromUtf8(query.value(2).toString()))
              .replace("<name>", QString::fromUtf8(query.value(3).toString()));
          m_channel->insertItem(channel);
          m_chanids << query.value(2).toString();
      }
      
    }

    hbox->addWidget(m_channel);

    // Program Date
    hbox = new QHBoxLayout(vbox, (int)(10 * wmult));

    message = tr("Date:");
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

    QTime thisTime = m_nowDateTime.time();
    thisTime = thisTime.addSecs((30 - thisTime.minute() % 30) * 60);
    
    if (thisTime < QTime::QTime(0,30))
        m_startdate->setCurrentItem(m_startdate->currentItem() + 1);

    //message = tr("Time:");
    //label = new QLabel(message, this);
    //label->setBackgroundOrigin(WindowOrigin);
    //label->setSizePolicy(QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed));
    //hbox->addWidget(label);

    m_starthour = new MythSpinBox( this, "starthour", true );
    m_starthour->setBackgroundOrigin(WindowOrigin);
    m_starthour->setMinValue(-4);
    m_starthour->setMaxValue(28);
    m_starthour->setLineStep(4);
    m_starthour->setValue(thisTime.hour());
    m_starthour->setSuffix(tr("hr"));
    label->setSizePolicy(QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed));
    hbox->addWidget(m_starthour);

    m_startminute = new MythSpinBox( this, "startminute", true );
    m_startminute->setBackgroundOrigin(WindowOrigin);
    m_startminute->setMinValue(-10);
    m_startminute->setMaxValue(70);
    m_startminute->setLineStep(10);
    m_startminute->setValue(thisTime.minute());
    m_startminute->setSuffix(tr("min"));
    label->setSizePolicy(QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed));
    hbox->addWidget(m_startminute);


    // Duration spin box
    message = tr("Duration:");
    label = new QLabel(message, this);
    label->setBackgroundOrigin(WindowOrigin);
    label->setSizePolicy(QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed));
    hbox->addWidget(label);

    m_duration = new MythSpinBox( this, "duration", true );
    m_duration->setMinValue(1);
    m_duration->setMaxValue(600);
    m_duration->setValue(60);
    m_duration->setLineStep(10);
    m_duration->setSuffix(tr("min"));
    m_duration->setBackgroundOrigin(WindowOrigin);
    hbox->addWidget(m_duration);

    // Title edit box
    hbox = new QHBoxLayout(vbox, (int)(10 * wmult));

    message = tr("Title:");
    label = new QLabel(message, this);
    label->setBackgroundOrigin(WindowOrigin);
    label->setSizePolicy(QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed));
    hbox->addWidget(label);

    m_title = new MythRemoteLineEdit( this, "title" );
    m_title->setBackgroundOrigin(WindowOrigin);
    hbox->addWidget(m_title);

    // Subtitle edit box
    hbox = new QHBoxLayout(vbox, (int)(10 * wmult));

    message = tr("Subtitle:");
    label = new QLabel(message, this);
    label->setBackgroundOrigin(WindowOrigin);
    label->setSizePolicy(QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed));
    hbox->addWidget(label);

    m_subtitle = new MythRemoteLineEdit( this, "subtitle" );
    m_subtitle->setBackgroundOrigin(WindowOrigin);
    hbox->addWidget(m_subtitle);

    // Description edit box
    hbox = new QHBoxLayout(vbox, (int)(10 * wmult));

    //message = tr("Description:");
    //label = new QLabel(message, this);
    //label->setBackgroundOrigin(WindowOrigin);
    //label->setSizePolicy(QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed));
    // hbox->addWidget(label);

    m_description = new MythRemoteLineEdit(4, this, "description" );
    m_description->setBackgroundOrigin(WindowOrigin);
    hbox->addWidget(m_description);

    //  Exit Button
    hbox = new QHBoxLayout(vbox, (int)(10 * wmult));

    m_exitButton = new MythPushButton( this, "Program" );
    m_exitButton->setBackgroundOrigin(WindowOrigin);
    m_exitButton->setText( tr( "Save and exit" ) );
    m_exitButton->setEnabled(false);

    hbox->addWidget(m_exitButton);

    //  Save Button
    m_saveButton = new MythPushButton( this, "Program" );
    m_saveButton->setBackgroundOrigin(WindowOrigin);
    m_saveButton->setText( tr( "Save and set another" ) );
    m_saveButton->setEnabled(false);

    hbox->addWidget(m_saveButton);

    //  Cancel Button
    m_cancelButton = new MythPushButton( this, "Program" );
    m_cancelButton->setBackgroundOrigin(WindowOrigin);
    m_cancelButton->setText( tr( "Cancel" ) );
    m_cancelButton->setEnabled(true);

    hbox->addWidget(m_cancelButton);



    connect(this, SIGNAL(dismissWindow()), this, SLOT(accept()));

     
    connect(m_channel, SIGNAL(activated(int)), this, SLOT(channelChanged(void)));
    connect(m_channel, SIGNAL(highlighted(int)), this, SLOT(channelChanged(void)));
    connect(m_startdate, SIGNAL(activated(int)), this, SLOT(dateChanged(void)));
    connect(m_startdate, SIGNAL(highlighted(int)), this, SLOT(dateChanged(void)));
    connect(m_starthour, SIGNAL(valueChanged(const QString &)), this, SLOT(hourChanged(void)));
    connect(m_startminute, SIGNAL(valueChanged(const QString &)), this, SLOT(minuteChanged(void)));
    connect(m_duration, SIGNAL(valueChanged(const QString &)), this, SLOT(durationChanged(void)));
    connect(m_title, SIGNAL(textChanged(void)), this, SLOT(textChanged(void)));
    connect(m_subtitle, SIGNAL(textChanged(void)), this, SLOT(textChanged(void)));
    connect(m_description, SIGNAL(textChanged(void)), this, SLOT(textChanged(void)));

    connect(m_saveButton, SIGNAL(clicked()), this, SLOT(saveClicked()));
    connect(m_exitButton, SIGNAL(clicked()), this, SLOT(exitClicked()));
    connect(m_cancelButton, SIGNAL(clicked()), this, SLOT(cancelClicked()));

    m_channel->setFocus();
    channelChanged();
    
    gContext->addListener(this);
}

ManualSchedule::~ManualSchedule(void)
{
    gContext->removeListener(this);
}

void ManualSchedule::textChanged(void)
{
    m_saveButton->setEnabled(!m_title->text().isEmpty());
    m_exitButton->setEnabled(!m_title->text().isEmpty());
}

void ManualSchedule::minuteChanged(void)
{
    if (m_startminute->value() < 0 ) {
        m_startminute->setValue(m_startminute->value() + 60);
        m_starthour->setValue(m_starthour->value() - 1);
    }
    if (m_startminute->value() > 59 ) {
        m_starthour->setValue(m_starthour->value() + 1);
        m_startminute->setValue(m_startminute->value() - 60);
    }
    dateChanged();
}

void ManualSchedule::hourChanged(void)
{
   if (m_starthour->value() < 0 ) { 
       m_starthour->setValue(m_starthour->value() + 24);
       m_startdate->setCurrentItem(m_startdate->currentItem() - 1);
   }
   if (m_starthour->value() > 23 ) {
       m_startdate->setCurrentItem(m_startdate->currentItem() + 1);
       m_starthour->setValue(m_starthour->value() - 24);
   }
   dateChanged();
}

void ManualSchedule::channelChanged(void)
{
//  if edited, preserves user entered text
    if (   m_title->text().startsWith(m_lastChannel)  || 
           m_lastChannel.isEmpty() || m_title->text().isEmpty() )
    {
        QString user_text;
        
        if (m_title->text().length() > m_lastChannel.length() &&
            (!m_lastChannel.isEmpty() && !m_title->text().isEmpty()) )
            user_text = m_title->text().right(
                m_title->text().length() - m_lastChannel.length());
        m_lastChannel = m_channel->currentText();
        m_title->setText( m_channel->currentText() + user_text ); 
    }
    textChanged();
    dateChanged();
}

void ManualSchedule::durationChanged(void)
{

// Preserve user entered text, if present
   if (!m_description->text().isEmpty())
   {
      QString userText = m_description->text().section(" - ",2);
      if (!userText.isEmpty())
        m_description->setText( m_startDateTime.toString(dateformat) + 
                        " - (" +
                        m_duration->text() +
                        ") - " +
                        userText );
      
   } else
   m_description->setText( m_startDateTime.toString(dateformat) + " - (" +
                        m_duration->text() +
                        ") - " +
                        tr("Manual recording") );
}

void ManualSchedule::dateChanged(void)
{
   daysahead = m_startdate->currentItem();
   m_startDateTime.setDate(m_nowDateTime.addDays(daysahead).date());
   

   m_startDateTime.setTime(QTime(00,00));
   m_startDateTime = m_startDateTime.addSecs((m_starthour->value() * 3600) +
                                             (m_startminute->value() * 60));

   VERBOSE(VB_SCHEDULE, QString("Start Date Time: %1")
                                .arg(m_startDateTime.toString()));

// don't know how to record the past, do you? :P
   if (m_startDateTime < m_nowDateTime)
   {
        QTime thisTime = m_nowDateTime.time();
        m_starthour->setValue(thisTime.hour());
        m_startDateTime.setDate(m_nowDateTime.date());
        m_startDateTime.setTime(QTime(m_starthour->value(),
                                      m_startminute->value()));
  } 
    
//  if edited, preserves user entered text
    if (   m_subtitle->text().startsWith(m_lastSubtitle)  || 
           m_lastSubtitle.isEmpty() || m_subtitle->text().isEmpty() )
    {
        QString user_text;
        
        if (m_subtitle->text().length() > m_lastSubtitle.length() &&
            (!m_lastSubtitle.isEmpty() && !m_subtitle->text().isEmpty()) )
            user_text = m_subtitle->text().right(
                m_subtitle->text().length() - m_lastSubtitle.length());
        m_lastSubtitle = m_startDateTime.toString("yyyy/MM/dd") + " " +
                        m_startDateTime.toString(timeformat);
        m_subtitle->setText( m_lastSubtitle + user_text ); 
    }
    textChanged();
    durationChanged();
}

void ManualSchedule::saveScheduledRecording(void)
{
    ProgramInfo progInfo;
    
    // int dayOffset = chooseDay->currentItem() - 1;
    // searchTime.setDate(startTime.addDays(dayOffset).date());
    progInfo.startts = m_startDateTime;

    progInfo.endts = progInfo.startts.addSecs(m_duration->value() * 60);

    QSqlDatabase *db = QSqlDatabase::database();

    progInfo.title = m_title->text();
    progInfo.subtitle = m_subtitle->text();
    progInfo.description = m_description->text();
    progInfo.category = tr("Manual recording");

    QSqlQuery query;

    QString callsign = m_chanids[m_channel->currentItem()];

    query.prepare("SELECT chanid, channum, callsign, name "
                  "FROM channel WHERE callsign=:CALLSIGN");
    query.bindValue(":CALLSIGN", callsign);

    query.exec();

    if (query.isActive() && query.numRowsAffected()) 
    {
        QString chanid;

        while (query.next())
        {
            chanid = progInfo.chanid = query.value(0).toString();;
            progInfo.chanstr = query.value(1).toString();
            progInfo.chansign = QString::fromUtf8(query.value(2).toString());
            progInfo.channame = QString::fromUtf8(query.value(3).toString());

            cout << "Program added to channel " 
                 << chanid << " - " << m_channel->currentText()
                 << " at "
                 << progInfo.startts.toString("yyyy/MM/dd hh:mm") 
                 << " to "
                 << progInfo.endts.toString("hh:mm") << endl;

            progInfo.Save(db);
        }

        ProgramList pglist;
        QString query;
        QString sqlcat = progInfo.category;

        sqlcat.replace(QRegExp("([^\\])\"|^\""), QString("\\1\\\""));
        query = QString("WHERE program.chanid = %1 AND program.starttime = %2 "
                        "AND program.endtime = %3 AND program.category = \"%4\" ")
                       .arg(chanid)
                       .arg(progInfo.startts.toString("yyyyMMddhhmmss"))
                       .arg(progInfo.endts.toString("yyyyMMddhhmmss"))
                       .arg(sqlcat);

        ProgramList reclist;
        reclist.FromScheduler();

        if (pglist.FromProgram(db, query, reclist))
        {
            if (pglist.count() > 0)
            {
                if (pglist.count() > 1)
                {
                    cerr << "Multiple manual recordings for the same chan/date\n";
                }

                ProgramInfo *pginfo = pglist.first();
                pginfo->ApplyRecordStateChange(db, kSingleRecord);
            }
            else
                cerr << "Couldn't find newly scheduled program\n";
        }
    }
}


void ManualSchedule::saveClicked(void)
{
    saveScheduledRecording();
    m_saveButton->setEnabled(false);
    m_exitButton->setEnabled(false);
}

void ManualSchedule::exitClicked(void)
{
    saveScheduledRecording();
    accept();
}

void ManualSchedule::cancelClicked(void)
{
    accept();
}

