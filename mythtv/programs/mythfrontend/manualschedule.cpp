
#include "manualschedule.h"

// qt
#include <QDateTime>

// libmythdb
#include "mythdbcon.h"
#include "mythverbose.h"

// libmyth
#include "mythcorecontext.h"
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
#include "mythmainwindow.h"

// mythfrontend
#include "scheduleeditor.h"

ManualSchedule::ManualSchedule(MythScreenStack *parent)
               : MythScreenType(parent, "ManualSchedule")
{
    m_nowDateTime = QDateTime::currentDateTime();
    m_startDateTime = m_nowDateTime;

    m_dateformat = gCoreContext->GetSetting("DateFormat", "ddd MMMM d");
    m_timeformat = gCoreContext->GetSetting("TimeFormat", "h:mm AP");

    m_daysahead = 0;
    m_titleEdit = NULL;
    m_channelList = m_startdateList = NULL;
    m_recordButton = m_cancelButton = NULL;
    m_durationSpin = m_starthourSpin = m_startminuteSpin = NULL;
}

bool ManualSchedule::Create(void)
{
    if (!LoadWindowFromXML("schedule-ui.xml", "manualschedule", this))
        return false;

    m_channelList = dynamic_cast<MythUIButtonList *>(GetChild("channel"));
    m_startdateList = dynamic_cast<MythUIButtonList *>(GetChild("startdate"));

    m_starthourSpin = dynamic_cast<MythUISpinBox *>(GetChild("starthour"));
    m_startminuteSpin = dynamic_cast<MythUISpinBox *>(GetChild("startminute"));
    m_durationSpin = dynamic_cast<MythUISpinBox *>(GetChild("duration"));

    m_titleEdit = dynamic_cast<MythUITextEdit *>(GetChild("title"));

    m_recordButton = dynamic_cast<MythUIButton *>(GetChild("next"));
    m_cancelButton = dynamic_cast<MythUIButton *>(GetChild("cancel"));

    if (!m_channelList || !m_startdateList || !m_starthourSpin ||
        !m_startminuteSpin || !m_durationSpin || !m_titleEdit ||
        !m_recordButton || !m_cancelButton)
    {
        VERBOSE(VB_IMPORTANT, "ManualSchedule, theme is missing "
                              "required elements");
        return false;
    }

    QString longChannelFormat = gCoreContext->GetSetting("LongChannelFormat",
                                                     "<num> <name>");
    QString chanorder = gCoreContext->GetSetting("ChannelOrdering", "channum");
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
        new MythUIButtonListItem(m_channelList, chantext);
        m_chanids.push_back(channels[i].chanid);
    }

    for (uint index = 0; index <= 60; index++)
    {
        QString dinfo = m_nowDateTime.addDays(index).toString(m_dateformat);
        if (m_nowDateTime.addDays(index).date().dayOfWeek() < 6)
            dinfo += QString(" (%1)").arg(tr("5 weekdays if daily"));
        else
            dinfo += QString(" (%1)").arg(tr("7 days per week if daily"));
        new MythUIButtonListItem(m_startdateList, dinfo);
        if (m_nowDateTime.addDays(index).toString("MMdd") ==
            m_startDateTime.toString("MMdd"))
            m_startdateList->SetItemCurrent(m_startdateList->GetCount() - 1);
    }

    QTime thisTime = m_nowDateTime.time();
    thisTime = thisTime.addSecs((30 - (thisTime.minute() % 30)) * 60);

    if (thisTime < QTime(0,30))
        m_startdateList->SetItemCurrent(m_startdateList->GetCurrentPos() + 1);

    m_starthourSpin->SetRange(0,23,1);
    m_starthourSpin->SetValue(thisTime.hour());
    int minute_increment =
        gCoreContext->GetNumSetting("ManualScheduleMinuteIncrement", 5);
    m_startminuteSpin->SetRange(0, 60-minute_increment, minute_increment);
    m_startminuteSpin->SetValue((thisTime.minute()/5)*5);
    m_durationSpin->SetRange(5,360,5);
    m_durationSpin->SetValue(60);

    connectSignals();
    connect(m_recordButton, SIGNAL(Clicked()), SLOT(recordClicked()));
    connect(m_cancelButton, SIGNAL(Clicked()), SLOT(Close()));

    m_titleEdit->SetMaxLength(128);

    BuildFocusList();

    return true;
}

void ManualSchedule::connectSignals()
{
    connect(m_startdateList, SIGNAL(itemSelected(MythUIButtonListItem*)),
                         SLOT(dateChanged(void)));
    connect(m_starthourSpin, SIGNAL(itemSelected(MythUIButtonListItem*)),
                         SLOT(dateChanged(void)));
    connect(m_startminuteSpin, SIGNAL(itemSelected(MythUIButtonListItem*)),
                           SLOT(dateChanged(void)));
}

void ManualSchedule::disconnectSignals()
{
    disconnect(m_startdateList, 0, this, 0);
    disconnect(m_starthourSpin, 0, this, 0);
    disconnect(m_startminuteSpin, 0, this, 0);
}

void ManualSchedule::hourRollover(void)
{
    if (m_startminuteSpin->GetIntValue() == 0 )
    {
        m_startminuteSpin->SetValue(12);
        m_starthourSpin->SetValue(m_starthourSpin->GetIntValue() - 1);
    }
    if (m_startminuteSpin->GetIntValue() == 13 )
    {
        m_starthourSpin->SetValue(m_starthourSpin->GetIntValue() + 1);
        m_startminuteSpin->SetValue(1);
    }
}

void ManualSchedule::minuteRollover(void)
{
    if (m_starthourSpin->GetIntValue() == 0 )
    {
        m_starthourSpin->SetValue(24);
        m_startdateList->SetItemCurrent(m_startdateList->GetCurrentPos() - 1);
    }
    if (m_starthourSpin->GetIntValue() == 25 )
    {
        m_startdateList->SetItemCurrent(m_startdateList->GetCurrentPos() + 1);
        m_starthourSpin->SetValue(1);
    }
}

void ManualSchedule::dateChanged(void)
{
    disconnectSignals();
    m_daysahead = m_startdateList->GetCurrentPos();
    m_startDateTime.setDate(m_nowDateTime.addDays(m_daysahead).date());

    int hr = m_starthourSpin->GetIntValue();
    int min = m_startminuteSpin->GetIntValue();
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
        m_starthourSpin->SetValue(hr);
        m_startDateTime.setDate(m_nowDateTime.date());
        m_startDateTime.setTime(QTime(hr, min));
    }
    connectSignals();
}

void ManualSchedule::recordClicked(void)
{
    QDateTime endts = m_startDateTime
        .addSecs(max(m_durationSpin->GetIntValue() * 60, 60));

    ProgramInfo p(m_titleEdit->GetText().trimmed(),
                  m_chanids[m_channelList->GetCurrentPos()],
                  m_startDateTime, endts);

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
