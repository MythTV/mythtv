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
    int m_index; 

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
  
    m_weekday = new MythComboBox(false, this, "weekday" );
    for(m_index = 0; m_index < 7; m_index++) 
        m_weekday->insertItem(
		m_nowDateTime.addDays(m_index).toString("dddd"));
    m_weekday->setBackgroundOrigin(WindowOrigin);
    label->setSizePolicy(QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed));
    hbox->addWidget(m_weekday);

    m_startday = new MythSpinBox( this, "startday" );
    m_startday->setMinValue(0);
    m_startday->setMaxValue(m_nowDateTime.date().daysInMonth() + 1);        
    m_startday->setValue(m_nowDateTime.date().day());
    m_startday->setBackgroundOrigin(WindowOrigin);
    label->setSizePolicy(QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed));
    hbox->addWidget(m_startday);

    m_startmonth = new MythComboBox(false, this, "startmonth" );
    for(m_index = 0; m_index < 12; m_index++) 
        m_startmonth->insertItem(
		m_nowDateTime.addMonths(m_index).toString("MMMM"));
    m_startmonth->setBackgroundOrigin(WindowOrigin);
    label->setSizePolicy(QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed));
    hbox->addWidget(m_startmonth);

    m_startyear = m_nowDateTime.date().year();

    //Program Time
//     hbox = new QHBoxLayout(vbox, (int)(10 * wmult));

    QTime thisTime = m_nowDateTime.time();

    message = tr("Time:");
    label = new QLabel(message, this);
    label->setBackgroundOrigin(WindowOrigin);
    label->setSizePolicy(QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed));
    hbox->addWidget(label);

    m_starthour = new MythSpinBox( this, "starthour" );
    m_starthour->setBackgroundOrigin(WindowOrigin);
    m_starthour->setMinValue(-1);
    m_starthour->setMaxValue(24);
    m_starthour->setValue(thisTime.hour());
    m_starthour->setSuffix(tr("hr"));
    label->setSizePolicy(QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed));
    hbox->addWidget(m_starthour);


    m_startminute = new MythSpinBox( this, "startminute", true );
    m_startminute->setBackgroundOrigin(WindowOrigin);
    m_startminute->setMinValue(-1);
    m_startminute->setMaxValue(60);
    m_startminute->setLineStep(5);
    m_startminute->setValue(thisTime.minute());
    m_startminute->setSuffix(tr("min"));
    label->setSizePolicy(QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed));
    hbox->addWidget(m_startminute);


    // Duration spin box
    hbox = new QHBoxLayout(vbox, (int)(10 * wmult));

    message = tr("Duration:");
    label = new QLabel(message, this);
    label->setBackgroundOrigin(WindowOrigin);
    label->setSizePolicy(QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed));
    hbox->addWidget(label);

    m_duration = new MythSpinBox( this, "duration", true );
    m_duration->setMinValue(1);
    m_duration->setMaxValue(300);
    m_duration->setValue(120);
    m_duration->setLineStep(5);
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

    message = tr("Description:");
    label = new QLabel(message, this);
    label->setBackgroundOrigin(WindowOrigin);
    label->setSizePolicy(QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed));
    hbox->addWidget(label);

    m_description = new MythRemoteLineEdit( this, "description" );
    m_description->setBackgroundOrigin(WindowOrigin);
    hbox->addWidget(m_description);

    //  Exit Button
    hbox = new QHBoxLayout(vbox, (int)(10 * wmult));

    m_exitButton = new MythPushButton( this, "Program" );
    m_exitButton->setBackgroundOrigin(WindowOrigin);
    m_exitButton->setText( tr( "Save this scheduled recording and exit" ) );
    m_exitButton->setEnabled(false);

    hbox->addWidget(m_exitButton);

    //  Save Button
    hbox = new QHBoxLayout(vbox, (int)(10 * wmult));

    m_saveButton = new MythPushButton( this, "Program" );
    m_saveButton->setBackgroundOrigin(WindowOrigin);
    m_saveButton->setText( tr( "Save this scheduled recording and set another" ) );
    m_saveButton->setEnabled(false);

    hbox->addWidget(m_saveButton);

    //  Cancel Button
    hbox = new QHBoxLayout(vbox, (int)(10 * wmult));

    m_cancelButton = new MythPushButton( this, "Program" );
    m_cancelButton->setBackgroundOrigin(WindowOrigin);
    m_cancelButton->setText( tr( "Cancel" ) );
    m_cancelButton->setEnabled(true);

    hbox->addWidget(m_cancelButton);



    connect(this, SIGNAL(dismissWindow()), this, SLOT(accept()));

     
    connect(m_channel, SIGNAL(activated(int)), this, SLOT(channelChanged(void)));
    connect(m_channel, SIGNAL(highlighted(int)), this, SLOT(channelChanged(void)));
    connect(m_weekday, SIGNAL(activated(int)), this, SLOT(weekdayChanged(void)));
    connect(m_weekday, SIGNAL(highlighted(int)), this, SLOT(weekdayChanged(void)));
    connect(m_startday, SIGNAL(valueChanged(const QString &)), this, SLOT(dayChanged(void)));
    connect(m_startmonth, SIGNAL(highlighted(const QString &)), this, SLOT(monthChanged(void)));
    connect(m_startmonth, SIGNAL(activated(const QString &)), this, SLOT(monthChanged(void)));
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
   if ( atoi(m_startminute->text()) == -1 ) {
     m_starthour->stepDown(); 
     m_startminute->setValue(55);
   }
   if ( atoi(m_startminute->text()) == 60 ) {
     m_starthour->stepUp(); 
     m_startminute->setValue(0);
   }
   dateChanged();
}

void ManualSchedule::hourChanged(void)
{
   if ( atoi(m_starthour->text()) == -1 ) {
     m_startday->stepDown(); 
     m_starthour->setValue(23);
   }
   if ( atoi(m_starthour->text()) == 24 ) {
     m_startday->stepUp(); 
     m_starthour->setValue(0);
   }

   dateChanged();
}

void ManualSchedule::dayChanged(void)
{
   if ( atoi(m_startday->text()) == 0 ) {
     m_startDateTime = m_startDateTime.addDays(-1);
     m_startmonth->setCurrentText(m_startDateTime.date().toString("MMMM"));
     m_startday->setMaxValue(m_startDateTime.date().daysInMonth() + 1);
     m_startday->setValue(m_startDateTime.date().day());
   }
   if ( atoi(m_startday->text()) == m_startday->maxValue() ) {
     m_startDateTime = m_startDateTime.addDays(1);
     m_startmonth->setCurrentText(m_startDateTime.date().toString("MMMM"));
     m_startday->setValue(1);
     m_startday->setMaxValue(m_startDateTime.date().daysInMonth() + 1);
   }

   dateChanged();
}

void ManualSchedule::weekdayChanged(void)
{
    if (m_startDateTime.toString("dddd") ==
	m_weekday->currentText() ) {
 	// That's OK, weekdate and startdate already synched
	prev_weekday = 99;
	return;
    }

    prev_weekday = m_startDateTime.date().dayOfWeek() -
	m_nowDateTime.date().dayOfWeek();
    if (prev_weekday < 0)
	prev_weekday = 7 + prev_weekday;

    if ((prev_weekday + 1 == m_weekday->currentItem()) ||
       ((prev_weekday == 6 ) && (m_weekday->currentItem()==0))) {
        // It's going forward
	m_startday->stepUp();
        return;
    }

    if ((prev_weekday == m_weekday->currentItem() + 1 ) ||
       ((prev_weekday == 0 ) && (m_weekday->currentItem()==6))) {
        // It's going backwards
	m_startday->stepDown();
        return;
    }

    m_startday->setValue(
       m_startDateTime.addDays(
              m_weekday->currentItem()-prev_weekday).date().day());
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

void ManualSchedule::monthChanged(void)
{ 
   if ((m_startmonth->currentItem() + m_nowDateTime.date().month() ) > 12 )
     m_startyear = m_nowDateTime.date().year() + 1;
   else
     m_startyear = m_nowDateTime.date().year();
}

void ManualSchedule::dateChanged(void)
{
   QDate thisDate;

   thisDate.setYMD(
     m_startyear,
     m_nowDateTime.addMonths(m_startmonth->currentItem()).date().month(),
     atoi(m_startday->text()) );

   m_startDateTime.setDate(thisDate);
   
   daysahead = m_nowDateTime.date().daysTo(m_startDateTime.date());

   m_weekday->setCurrentText(thisDate.toString("dddd"));

   m_startDateTime.setTime(QTime(00,00));
   m_startDateTime = m_startDateTime.addSecs( (atoi(m_starthour->text()) * 3600)
					+ (atoi(m_startminute->text()) * 60) );
					 

// don't know how to record the past, do you? :P
   if (m_startDateTime < m_nowDateTime)
   {
        QDate thisDate = m_nowDateTime.date();
        m_startyear = thisDate.year();
        m_startmonth->setCurrentItem(thisDate.month() -
		m_nowDateTime.date().month() );
        m_startday->setValue(thisDate.day());
	QTime thisTime = m_nowDateTime.time();
        m_starthour->setValue(thisTime.hour());
        m_startminute->setValue(thisTime.minute());
	m_startDateTime = m_nowDateTime;
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
            progInfo.chansign = query.value(2).toString();
            progInfo.channame = query.value(3).toString();

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

