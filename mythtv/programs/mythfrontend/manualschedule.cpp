
#include "manualschedule.h"

// qt
#include <QDateTime>

// libmythdb
#include "mythdbcon.h"
#include "mythverbose.h"

// libmyth
#include "mythcontext.h"
#include "programinfo.h"

// libmythtv
#include "recordingrule.h"
#include "recordingtypes.h"
#include "channelutil.h"

// libmythui
#include "mythuitextedit.h"
#include "mythuibutton.h"
#include "mythuibuttonlist.h"
#include "mythuispinbox.h"

// mythfrontend
#include "scheduleeditor.h"

ManualSchedule::ManualSchedule(MythScreenStack *parent)
               : MythScreenType(parent, "ManualSchedule")
{
    m_nowDateTime = QDateTime::currentDateTime();
    m_startDateTime = m_nowDateTime;

    m_dateformat = gContext->GetSetting("DateFormat", "ddd MMMM d");
    m_timeformat = gContext->GetSetting("TimeFormat", "h:mm AP");

    m_title = NULL;
    m_channel = NULL;
    m_recordButton = m_cancelButton = NULL;
    m_startdate = NULL;
    m_duration = m_starthour = m_startminute = NULL;
}

bool ManualSchedule::Create(void)
{
    if (!LoadWindowFromXML("schedule-ui.xml", "manualschedule", this))
        return false;

    m_channel = dynamic_cast<MythUIButtonList *>(GetChild("channel"));
    m_startdate = dynamic_cast<MythUIButtonList *>(GetChild("startdate"));

    m_starthour = dynamic_cast<MythUISpinBox *>(GetChild("starthour"));
    m_startminute = dynamic_cast<MythUISpinBox *>(GetChild("startminute"));
    m_duration = dynamic_cast<MythUISpinBox *>(GetChild("duration"));

    m_title = dynamic_cast<MythUITextEdit *>(GetChild("title"));

    m_recordButton = dynamic_cast<MythUIButton *>(GetChild("next"));
    m_cancelButton = dynamic_cast<MythUIButton *>(GetChild("cancel"));

    if (!m_channel || !m_startdate || !m_starthour || !m_startminute ||
        !m_duration || !m_title || !m_recordButton || !m_cancelButton)
    {
        VERBOSE(VB_IMPORTANT, "ManualSchedule, theme is missing "
                              "required elements");
        return false;
    }

    QString longChannelFormat = gContext->GetSetting("LongChannelFormat",
                                                     "<num> <name>");
    QString chanorder = gContext->GetSetting("ChannelOrdering", "channum");
    DBChanList channels = ChannelUtil::GetChannels(0, false, "channum,callsign");
    ChannelUtil::SortChannels(channels, chanorder);

    for (uint i = 0; i < channels.size(); i++)
    {
        QString chantext = longChannelFormat;
        chantext
            .replace("<num>",  channels[i].channum)
            .replace("<sign>", channels[i].callsign)
            .replace("<name>", channels[i].name);
        chantext.detach();
        new MythUIButtonListItem(m_channel, chantext);
        m_chanids << QString::number(channels[i].chanid);
    }

    for (uint index = 0; index <= 60; index++)
    {
        QString dinfo = m_nowDateTime.addDays(index).toString(m_dateformat);
        if (m_nowDateTime.addDays(index).date().dayOfWeek() < 6)
            dinfo += QString(" (%1)").arg(tr("5 weekdays if daily"));
        else
            dinfo += QString(" (%1)").arg(tr("7 days per week if daily"));
        new MythUIButtonListItem(m_startdate, dinfo);
        if (m_nowDateTime.addDays(index).toString("MMdd") ==
            m_startDateTime.toString("MMdd"))
            m_startdate->SetItemCurrent(m_startdate->GetCount() - 1);
    }

    QTime thisTime = m_nowDateTime.time();
    thisTime = thisTime.addSecs((30 - (thisTime.minute() % 30)) * 60);

    if (thisTime < QTime(0,30))
        m_startdate->SetItemCurrent(m_startdate->GetCurrentPos() + 1);

    m_starthour->SetRange(0,23,1);
    m_starthour->SetValue(thisTime.hour());
    int minute_increment =
        gContext->GetNumSetting("ManualScheduleMinuteIncrement", 5);
    m_startminute->SetRange(0, 60-minute_increment, minute_increment);
    m_startminute->SetValue((thisTime.minute()/5)*5);
    m_duration->SetRange(5,360,5);
    m_duration->SetValue(60);

    connectSignals();
    connect(m_recordButton, SIGNAL(Clicked()), SLOT(recordClicked()));
    connect(m_cancelButton, SIGNAL(Clicked()), SLOT(Close()));

    BuildFocusList();

    return true;
}

void ManualSchedule::connectSignals()
{
    connect(m_startdate, SIGNAL(itemSelected(MythUIButtonListItem*)),
                         SLOT(dateChanged(void)));
    connect(m_starthour, SIGNAL(itemSelected(MythUIButtonListItem*)),
                         SLOT(dateChanged(void)));
    connect(m_startminute, SIGNAL(itemSelected(MythUIButtonListItem*)),
                           SLOT(dateChanged(void)));
}

void ManualSchedule::disconnectSignals()
{
    disconnect(m_startdate, 0, this, 0);
    disconnect(m_starthour, 0, this, 0);
    disconnect(m_startminute, 0, this, 0);
}

void ManualSchedule::hourRollover(void)
{
    if (m_startminute->GetIntValue() == 0 )
    {
        m_startminute->SetValue(12);
        m_starthour->SetValue(m_starthour->GetIntValue() - 1);
    }
    if (m_startminute->GetIntValue() == 13 )
    {
        m_starthour->SetValue(m_starthour->GetIntValue() + 1);
        m_startminute->SetValue(1);
    }
}

void ManualSchedule::minuteRollover(void)
{
    if (m_starthour->GetIntValue() == 0 )
    {
        m_starthour->SetValue(24);
        m_startdate->SetItemCurrent(m_startdate->GetCurrentPos() - 1);
    }
    if (m_starthour->GetIntValue() == 25 )
    {
        m_startdate->SetItemCurrent(m_startdate->GetCurrentPos() + 1);
        m_starthour->SetValue(1);
    }
}

void ManualSchedule::dateChanged(void)
{
    disconnectSignals();
    daysahead = m_startdate->GetCurrentPos();
    m_startDateTime.setDate(m_nowDateTime.addDays(daysahead).date());

    int hr = m_starthour->GetIntValue();
    int min = m_startminute->GetIntValue();
    m_startDateTime.setTime(QTime(hr, min));

    VERBOSE(VB_SCHEDULE, QString("Start Date Time: %1")
                                    .arg(m_startDateTime.toString()));

    // Note we allow start times up to one hour in the past so
    // if it is 20:25 the user can start a recording at 20:30
    // by first setting the hour and then the minute.
    QDateTime tmp = QDateTime(m_startDateTime.date(),
                              QTime(m_startDateTime.time().hour(),59,59));
    if (tmp < m_nowDateTime)
    {
        hr = m_nowDateTime.time().hour();
        m_starthour->SetValue(hr);
        m_startDateTime.setDate(m_nowDateTime.date());
        m_startDateTime.setTime(QTime(hr, min));
    }
    connectSignals();
}

void ManualSchedule::recordClicked(void)
{
    ProgramInfo p;

    QString channelFormat = gContext->GetSetting("ChannelFormat", "<num> <sign>");
    p.chanid = m_chanids[m_channel->GetCurrentPos()];

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT chanid, channum, callsign, name "
                  "FROM channel WHERE chanid=:CHANID");
    query.bindValue(":CHANID", p.chanid);

    if (query.exec() && query.next())
    {
        p.chanstr = query.value(1).toString();
        p.chansign = query.value(2).toString();
        p.channame = query.value(3).toString();
    }

    int addsec = m_duration->GetIntValue() * 60;

    if (!addsec)
        addsec = 60;

    p.startts = m_startDateTime;
    p.endts = p.startts.addSecs(addsec);

    if (!m_title->GetText().isEmpty())
        p.title = m_title->GetText();
    else
        p.title = p.ChannelText(channelFormat) + " - " +
                  p.startts.toString(m_timeformat);

    p.title = QString("%1 (%2)").arg(p.title).arg(tr("Manual Record"));
    p.description = p.title; p.description.detach();

    RecordingRule *record = new RecordingRule();
    record->LoadByProgram(&p);
    record->m_searchType = kManualSearch;

    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
    ScheduleEditor *schededit = new ScheduleEditor(mainStack, record);
    if (schededit->Create())
    {
        mainStack->AddScreen(schededit);
        connect(schededit, SIGNAL(ruleSaved(int)), SLOT(scheduleCreated(int)));
    }
    else
        delete schededit;
}

void ManualSchedule::scheduleCreated(int ruleid)
{
    if (ruleid > 0)
        Close();
}
