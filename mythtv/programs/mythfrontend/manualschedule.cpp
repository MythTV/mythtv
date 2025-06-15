// Qt
#include <QDateTime>
#include <QTimeZone>

// MythTV
#include "libmythbase/mythcorecontext.h"
#include "libmythbase/mythdate.h"
#include "libmythbase/mythdbcon.h"
#include "libmythbase/mythlogging.h"
#include "libmythbase/programinfo.h"
#include "libmythbase/recordingtypes.h"
#include "libmythtv/channelinfo.h"
#include "libmythtv/channelutil.h"
#include "libmythtv/recordingrule.h"
#include "libmythui/mythmainwindow.h"
#include "libmythui/mythuibutton.h"
#include "libmythui/mythuibuttonlist.h"
#include "libmythui/mythuispinbox.h"
#include "libmythui/mythuitextedit.h"

// MythFrontend
#include "manualschedule.h"
#include "scheduleeditor.h"

ManualSchedule::ManualSchedule(MythScreenStack *parent)
    : MythScreenType(parent, "ManualSchedule"),
      m_nowDateTime(MythDate::current()),
      m_startDateTime(m_nowDateTime)
{
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
        LOG(VB_GENERAL, LOG_ERR,
            "ManualSchedule, theme is missing required elements");
        return false;
    }

    QString startchan = gCoreContext->GetSetting("DefaultTVChannel", "");
    QString chanorder = gCoreContext->GetSetting("ChannelOrdering", "channum");
    QString lastManualRecordChan = gCoreContext->GetSetting("LastManualRecordChan", startchan);
    int manStartChanType = gCoreContext->GetNumSetting("ManualRecordStartChanType", 1);
    ChannelInfoList channels = ChannelUtil::GetChannels(0, true, "channum,callsign");
    ChannelUtil::SortChannels(channels, chanorder);

    for (size_t i = 0; i < channels.size(); i++)
    {
        QString chantext = channels[i].GetFormatted(ChannelInfo::kChannelLong);

        auto *item = new MythUIButtonListItem(m_channelList, chantext);
        InfoMap infomap;
        channels[i].ToMap(infomap);
        item->SetTextFromMap(infomap);
        if (manStartChanType == 1)
        {
            // Use DefaultTVChannel as starting channel
            if (channels[i].m_chanNum == startchan)
            {
                m_channelList->SetItemCurrent(i);
                startchan = "";
            }
        }
        else
        {
            // Use LastManualRecordChan as starting channel
            if (channels[i].m_chanNum == lastManualRecordChan)
            {
                m_channelList->SetItemCurrent(i);
                lastManualRecordChan = "";
            }
        }
        m_chanids.push_back(channels[i].m_chanId);
    }

    for (uint index = 0; index <= 60; index++)
    {
        QString dinfo = MythDate::toString(
            m_nowDateTime.addDays(index),
            MythDate::kDateFull | MythDate::kSimplify);
        if (m_nowDateTime.addDays(index).toLocalTime().date().dayOfWeek() < 6)
            dinfo += QString(" (%1)").arg(tr("5 weekdays if daily"));
        else
            dinfo += QString(" (%1)").arg(tr("7 days per week if daily"));
        new MythUIButtonListItem(m_startdateList, dinfo);
        if (m_nowDateTime.addDays(index).toLocalTime().toString("MMdd") ==
            m_startDateTime.toLocalTime().toString("MMdd"))
            m_startdateList->SetItemCurrent(m_startdateList->GetCount() - 1);
    }

    QTime thisTime = m_nowDateTime.toLocalTime().time();
    thisTime = thisTime.addSecs((30 - (thisTime.minute() % 30)) * 60);

    if (thisTime < QTime(0,30))
        m_startdateList->SetItemCurrent(m_startdateList->GetCurrentPos() + 1);

    m_starthourSpin->SetRange(0,23,1);
    m_starthourSpin->SetValue(thisTime.hour());
    int minute_increment =
        gCoreContext->GetNumSetting("ManualScheduleMinuteIncrement", 5);
    m_startminuteSpin->SetRange(0, 60-minute_increment, minute_increment);
    m_startminuteSpin->SetValue((thisTime.minute()/5)*5);
    m_durationSpin->SetRange(5,1440,5);
    m_durationSpin->SetValue(60);

    connectSignals();
    connect(m_recordButton, &MythUIButton::Clicked, this, &ManualSchedule::recordClicked);
    connect(m_cancelButton, &MythUIButton::Clicked, this, &MythScreenType::Close);

    m_titleEdit->SetMaxLength(128);

    BuildFocusList();

    return true;
}

void ManualSchedule::connectSignals()
{
    connect(m_startdateList, &MythUIButtonList::itemSelected,
                         this, &ManualSchedule::dateChanged);
    connect(m_starthourSpin, &MythUIButtonList::itemSelected,
                         this, &ManualSchedule::dateChanged);
    connect(m_startminuteSpin, &MythUIButtonList::itemSelected,
                           this, &ManualSchedule::dateChanged);
}

void ManualSchedule::disconnectSignals()
{
    disconnect(m_startdateList, nullptr, this, nullptr);
    disconnect(m_starthourSpin, nullptr, this, nullptr);
    disconnect(m_startminuteSpin, nullptr, this, nullptr);
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
    int hr = m_starthourSpin->GetIntValue();
    int min = m_startminuteSpin->GetIntValue();

#if QT_VERSION < QT_VERSION_CHECK(6,5,0)
    m_startDateTime = QDateTime(
        m_nowDateTime.toLocalTime().addDays(m_daysahead).date(),
        QTime(hr, min), Qt::LocalTime).toUTC();
#else
    m_startDateTime =
        QDateTime(m_nowDateTime.toLocalTime().addDays(m_daysahead).date(),
                  QTime(hr, min),
                  QTimeZone(QTimeZone::LocalTime))
        .toUTC();
#endif

    LOG(VB_SCHEDULE, LOG_INFO, QString("Start Date Time: %1")
        .arg(m_startDateTime.toString(Qt::ISODate)));

    // Note we allow start times up to one hour in the past so
    // if it is 20:25 the user can start a recording at 20:30
    // by first setting the hour and then the minute.
#if QT_VERSION < QT_VERSION_CHECK(6,5,0)
    QDateTime tmp = QDateTime(
        m_startDateTime.toLocalTime().date(),
        QTime(m_startDateTime.toLocalTime().time().hour(),59,59),
        Qt::LocalTime).toUTC();
#else
    QDateTime tmp =
        QDateTime(m_startDateTime.toLocalTime().date(),
                  QTime(m_startDateTime.toLocalTime().time().hour(),59,59),
                  QTimeZone(QTimeZone::LocalTime))
        .toUTC();
#endif
    if (tmp < m_nowDateTime)
    {
        hr = m_nowDateTime.toLocalTime().time().hour();
        m_starthourSpin->SetValue(hr);
#if QT_VERSION < QT_VERSION_CHECK(6,5,0)
        m_startDateTime =
            QDateTime(m_nowDateTime.toLocalTime().date(),
                      QTime(hr, min), Qt::LocalTime).toUTC();
#else
        m_startDateTime = QDateTime(m_nowDateTime.toLocalTime().date(),
                                    QTime(hr, min),
                                    QTimeZone(QTimeZone::LocalTime))
            .toUTC();
#endif
    }
    connectSignals();
}

void ManualSchedule::recordClicked(void)
{
    QDateTime endts = m_startDateTime
        .addSecs(std::max(m_durationSpin->GetIntValue() * 60, 60));

    if (m_channelList->GetCurrentPos() >= m_chanids.size())
    {
        LOG(VB_GENERAL, LOG_ERR, "Channel out of range.");
        return; // this can happen if there are no channels..
    }

    ProgramInfo p(m_titleEdit->GetText().trimmed(),
                  m_chanids[m_channelList->GetCurrentPos()],
                  m_startDateTime, endts);

    // Save the channel because we might want to use it as the
    // starting channel for the next Manual Record rule.
    gCoreContext->SaveSetting("LastManualRecordChan",
        ChannelUtil::GetChanNum(m_chanids[m_channelList->GetCurrentPos()]));

    auto *record = new RecordingRule();
    record->LoadByProgram(&p);
    record->m_searchType = kManualSearch;
    record->m_dupMethod = kDupCheckNone;

    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
    auto *schededit = new ScheduleEditor(mainStack, record);
    if (schededit->Create())
    {
        mainStack->AddScreen(schededit);
        connect(schededit, &ScheduleEditor::ruleSaved, this, &ManualSchedule::scheduleCreated);
    }
    else
    {
        delete schededit;
    }
}

void ManualSchedule::scheduleCreated(int ruleid)
{
    if (ruleid > 0)
        Close();
}
