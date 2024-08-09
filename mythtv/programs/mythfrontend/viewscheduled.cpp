// C++
#include <algorithm>

// Qt
#include <QCoreApplication>

// MythTV
#include "libmythbase/mythcorecontext.h"
#include "libmythbase/mythlogging.h"
#include "libmythbase/programtypes.h"      // for RecStatus, etc
#include "libmythbase/recordingtypes.h"    // for toString
#include "libmythbase/remoteutil.h"
#include "libmythtv/recordinginfo.h"
#include "libmythtv/recordingrule.h"
#include "libmythtv/tv_actions.h"          // for ACTION_CHANNELSEARCH
#include "libmythtv/tv_play.h"
#include "libmythui/mythdialogbox.h"
#include "libmythui/mythmainwindow.h"
#include "libmythui/mythuibuttonlist.h"
#include "libmythui/mythuiprogressbar.h"
#include "libmythui/mythuistatetype.h"
#include "libmythui/mythuitext.h"

// mythFrontend
#include "guidegrid.h"
#include "viewscheduled.h"

void *ViewScheduled::RunViewScheduled(void *player, bool showTV)
{
    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
    auto *vsb = new ViewScheduled(mainStack, static_cast<TV*>(player), showTV);

    if (vsb->Create())
        mainStack->AddScreen(vsb, (player == nullptr));
    else
        delete vsb;

    return nullptr;
}

ViewScheduled::ViewScheduled(MythScreenStack *parent, TV* player, bool /*showTV*/)
  : ScheduleCommon(parent, "ViewScheduled"),
    m_showAll(!gCoreContext->GetBoolSetting("ViewSchedShowLevel", false)),
    m_player(player)
{
    gCoreContext->addListener(this);
    if (m_player)
        m_player->IncrRef();
}

ViewScheduled::~ViewScheduled()
{
    gCoreContext->removeListener(this);
    gCoreContext->SaveBoolSetting("ViewSchedShowLevel", !m_showAll);

    // if we have a player, we need to tell we are done
    if (m_player)
    {
        emit m_player->RequestEmbedding(false);
        m_player->DecrRef();
    }
}

bool ViewScheduled::Create()
{
    if (!LoadWindowFromXML("schedule-ui.xml", "viewscheduled", this))
        return false;

    m_groupList     = dynamic_cast<MythUIButtonList *> (GetChild("groups"));
    m_schedulesList = dynamic_cast<MythUIButtonList *> (GetChild("schedules"));
    m_progressBar   = dynamic_cast<MythUIProgressBar *>(GetChild("recordedprogressbar"));

    if (!m_schedulesList)
    {
        LOG(VB_GENERAL, LOG_ERR, "Theme is missing critical theme elements.");
        return false;
    }

    connect(m_schedulesList, &MythUIButtonList::itemSelected,
            this, &ViewScheduled::updateInfo);
    connect(m_schedulesList, &MythUIButtonList::itemClicked,
            this, qOverload<MythUIButtonListItem*>(&ViewScheduled::EditRecording));

    m_schedulesList->SetLCDTitles(tr("Scheduled Recordings"),
                              "shortstarttimedate|channel|titlesubtitle|card");
    m_schedulesList->SetSearchFields("titlesubtitle");

    if (m_groupList)
    {
        connect(m_groupList, &MythUIButtonList::itemSelected,
                this, &ViewScheduled::ChangeGroup);
        connect(m_groupList, &MythUIButtonList::itemClicked,
                this, &ViewScheduled::SwitchList);
        m_groupList->SetLCDTitles(tr("Group List"), "");
    }

    BuildFocusList();
    LoadInBackground();

    EmbedTVWindow();

    return true;
}

void ViewScheduled::Load(void)
{
    LoadFromScheduler(m_recList, m_conflictBool);
}

void ViewScheduled::Init()
{
    LoadList(true);
}

void ViewScheduled::Close()
{
    // don't fade the screen if we are returning to the player
    if (m_player)
        GetScreenStack()->PopScreen(this, false);
    else
        GetScreenStack()->PopScreen(this, true);
}

void ViewScheduled::SwitchList()
{
    if (GetFocusWidget() == m_groupList)
        SetFocusWidget(m_schedulesList);
    else if (GetFocusWidget() == m_schedulesList)
        SetFocusWidget(m_groupList);
}

bool ViewScheduled::keyPressEvent(QKeyEvent *event)
{
    // FIXME: Blackholes keypresses, not good
    if (m_inEvent)
        return true;

    m_inEvent = true;

    if (GetFocusWidget()->keyPressEvent(event))
    {
        m_inEvent = false;
        return true;
    }

    QStringList actions;
    bool handled = GetMythMainWindow()->TranslateKeyPress("TV Frontend", event,
                                                     actions);

    for (int i = 0; i < actions.size() && !handled; i++)
    {
        const QString& action = actions[i];
        handled = true;

        if (action == "EDIT")
            EditScheduled();
        else if (action == "CUSTOMEDIT")
            EditCustom();
        else if (action == "DELETE")
            deleteRule();
        else if (action == "UPCOMING")
            ShowUpcoming();
        else if (action == "VIEWSCHEDULED")
            ShowUpcomingScheduled();
        else if (action == "PREVRECORDED")
            ShowPrevious();
        else if (action == "DETAILS" || action == "INFO")
            ShowDetails();
        else if (action == "GUIDE")
            ShowGuide();
        else if (action == ACTION_CHANNELSEARCH)
            ShowChannelSearch();
        else if (action == "1")
            setShowAll(true);
        else if (action == "2")
            setShowAll(false);
        else if (action == "PREVVIEW" || action == "NEXTVIEW")
            setShowAll(!m_showAll);
        else if (action == "VIEWINPUT")
            viewInputs();
        else
            handled = false;
    }

    if (m_needFill)
        LoadList();

    if (!handled && MythScreenType::keyPressEvent(event))
        handled = true;

    m_inEvent = false;

    return handled;
}

void ViewScheduled::ShowMenu(void)
{
    QString label = tr("Options");

    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");
    auto *menuPopup = new MythDialogBox(label, popupStack, "menuPopup");

    if (menuPopup->Create())
    {
        menuPopup->SetReturnEvent(this, "menu");

        if (m_showAll)
            menuPopup->AddButton(tr("Show Important"));
        else
            menuPopup->AddButton(tr("Show All"));
        menuPopup->AddButton(tr("Program Details"));
        menuPopup->AddButton(tr("Program Guide"));
        menuPopup->AddButton(tr("Channel Search"));
        menuPopup->AddButton(tr("Upcoming by title"));
        menuPopup->AddButton(tr("Upcoming scheduled"));
        menuPopup->AddButton(tr("Previously Recorded"));
        menuPopup->AddButton(tr("Custom Edit"));
        menuPopup->AddButton(tr("Delete Rule"));
        menuPopup->AddButton(tr("Show Inputs"));

        popupStack->AddScreen(menuPopup);
    }
    else
    {
        delete menuPopup;
    }
}

void ViewScheduled::LoadList(bool useExistingData)
{
    if (m_inFill)
        return;

    m_inFill = true;

    MythUIButtonListItem *currentItem = m_schedulesList->GetItemCurrent();

    QString callsign;
    QDateTime startts;
    QDateTime recstartts;
    QDate group = m_currentGroup;

    if (currentItem)
    {
        auto *currentpginfo = currentItem->GetData().value<ProgramInfo*>();
        if (currentpginfo)
        {
            callsign   = currentpginfo->GetChannelSchedulingID();
            startts    = currentpginfo->GetScheduledStartTime();
            recstartts = currentpginfo->GetRecordingStartTime();
        }
    }

    QDateTime now = MythDate::current();

    QMap<int, int> toomanycounts;

    m_schedulesList->Reset();
    if (m_groupList)
        m_groupList->Reset();

    m_recgroupList.clear();

    if (!useExistingData)
        LoadFromScheduler(m_recList, m_conflictBool);

    m_recgroupList[m_defaultGroup] = ProgramList(false);
    m_recgroupList[m_defaultGroup].setAutoDelete(false);

    auto pit = m_recList.begin();
    while (pit != m_recList.end())
    {
        ProgramInfo *pginfo = *pit;
        if (pginfo == nullptr)
        {
            pit = m_recList.erase(pit);
            continue;
        }

        pginfo->CalculateRecordedProgress();

        const RecStatus::Type recstatus = pginfo->GetRecordingStatus();
        if ((pginfo->GetRecordingEndTime() >= now ||
             pginfo->GetScheduledEndTime() >= now ||
             recstatus == RecStatus::Recording ||
             recstatus == RecStatus::Tuning ||
             recstatus == RecStatus::Failing) &&
            (m_showAll ||
             (recstatus >= RecStatus::Pending &&
              recstatus <= RecStatus::WillRecord) ||
             recstatus == RecStatus::DontRecord ||
             (recstatus == RecStatus::TooManyRecordings &&
              ++toomanycounts[pginfo->GetRecordingRuleID()] <= 1) ||
             (recstatus > RecStatus::TooManyRecordings &&
              recstatus != RecStatus::Repeat &&
              recstatus != RecStatus::NeverRecord)))
        {
            m_inputref[pginfo->GetInputID()]++;
            m_maxinput = std::max(pginfo->GetInputID(), m_maxinput);

            QDate date = pginfo->GetRecordingStartTime().toLocalTime().date();
            m_recgroupList[date].push_back(pginfo);
            m_recgroupList[date].setAutoDelete(false);

            m_recgroupList[m_defaultGroup].push_back(pginfo);

            ++pit;
        }
        else
        {
            pit = m_recList.erase(pit);
            continue;
        }
    }

    if (m_groupList)
    {
        QString label;
        QMap<QDate,ProgramList>::iterator dateit = m_recgroupList.begin();
        while (dateit != m_recgroupList.end())
        {
            if (dateit.key().isNull())
                label = tr("All");
            else
                label = MythDate::toString(
                    dateit.key(), MythDate::kDateFull | MythDate::kSimplify);

            new MythUIButtonListItem(m_groupList, label,
                                     QVariant::fromValue(dateit.key()));
            ++dateit;
        }

        // Restore group
        if (m_recgroupList.contains(group))
            m_currentGroup = group;
        else
            m_currentGroup = m_defaultGroup;

        m_groupList->SetValueByData(QVariant::fromValue(m_currentGroup));
    }

    FillList();

    // Restore position after a list update
    if (!callsign.isEmpty())
    {
        ProgramList plist = m_recgroupList[m_currentGroup];

        int listPos = ((int) plist.size()) - 1;
        for (int i = listPos; i >= 0; --i)
        {
            ProgramInfo *pginfo = plist[i];
            if (pginfo == nullptr)
                continue;
            if (callsign == pginfo->GetChannelSchedulingID() &&
                startts  == pginfo->GetScheduledStartTime())
            {
                listPos = i;
                break;
            }
            if (recstartts <= pginfo->GetRecordingStartTime())
                listPos = i;
        }
        m_schedulesList->SetItemCurrent(listPos);
    }

    m_inFill = false;
    m_needFill = false;
}


void ViewScheduled::ChangeGroup(MythUIButtonListItem* item)
{
    if (!item || m_recList.empty())
        return;

    auto group = item->GetData().value<QDate>();

    m_currentGroup = group;

    if (!m_inFill)
        FillList();
}

void ViewScheduled::UpdateUIListItem(MythUIButtonListItem* item,
                                     ProgramInfo *pginfo) const
{
    QString state;

    const RecStatus::Type recstatus = pginfo->GetRecordingStatus();
    if (recstatus == RecStatus::Recording      ||
        recstatus == RecStatus::Tuning)
        state = "running";
    else if (recstatus == RecStatus::Conflict  ||
             recstatus == RecStatus::Offline   ||
             recstatus == RecStatus::TunerBusy ||
             recstatus == RecStatus::Failed    ||
             recstatus == RecStatus::Failing   ||
             recstatus == RecStatus::Aborted   ||
             recstatus == RecStatus::Missed)
        state = "error";
    else if (recstatus == RecStatus::WillRecord ||
             recstatus == RecStatus::Pending)
    {
        if (m_curinput == 0 || pginfo->GetInputID() == m_curinput)
        {
            if (pginfo->GetRecordingPriority2() < 0)
                state = "warning";
            else
                state = "normal";
        }
    }
    else if (recstatus == RecStatus::Repeat ||
             recstatus == RecStatus::NeverRecord ||
             recstatus == RecStatus::DontRecord ||
             (recstatus != RecStatus::DontRecord &&
              recstatus <= RecStatus::EarlierShowing))
    {
        state = "disabled";
    }
    else
    {
        state = "warning";
    }

    InfoMap infoMap;
    pginfo->ToMap(infoMap);
    item->SetTextFromMap(infoMap, state);

    item->SetProgress2(0, 100, pginfo->GetRecordedPercent());

    QString rating = QString::number(pginfo->GetStars(10));
    item->DisplayState(rating, "ratingstate");
    item->DisplayState(state, "status");
}

void ViewScheduled::FillList()
{
    m_schedulesList->Reset();

    MythUIText *norecordingText = dynamic_cast<MythUIText*>
                                                (GetChild("norecordings_info"));

    if (norecordingText)
        norecordingText->SetVisible(m_recList.empty());

    if (m_recList.empty())
        return;

    ProgramList plist;

    if (!m_recgroupList.contains(m_currentGroup))
        m_currentGroup = m_defaultGroup;

    plist = m_recgroupList[m_currentGroup];

    auto pit = plist.begin();
    while (pit != plist.end())
    {
        ProgramInfo *pginfo = *pit;
        if (pginfo)
        {
            auto *item = new MythUIButtonListItem(m_schedulesList,"",
                                                  QVariant::fromValue(pginfo));
            UpdateUIListItem(item, pginfo);
        }
        ++pit;
    }

    MythUIText *statusText = dynamic_cast<MythUIText*>(GetChild("status"));
    if (statusText)
    {
        if (m_conflictBool)
        {
            // Find first conflict and store in m_conflictDate field
            for (auto & conflict : plist)
            {
                if (conflict->GetRecordingStatus() == RecStatus::Conflict)
                {
                    m_conflictDate = conflict->GetRecordingStartTime()
                        .toLocalTime().date();
                    break;
                }
            }

            // TODO: This can be templated instead of hardcoding
            //       Conflict/No Conflict
            QString cstring = tr("Conflict %1")
                                .arg(MythDate::toString(m_conflictDate,
                                     MythDate::kDateFull | MythDate::kSimplify));

            statusText->SetText(cstring);
        }
        else
        {
            statusText->SetText(tr("No Conflicts"));
        }
    }

    MythUIText *filterText = dynamic_cast<MythUIText*>(GetChild("filter"));
    if (filterText)
    {
        if (m_showAll)
            filterText->SetText(tr("All"));
        else
            filterText->SetText(tr("Important"));
    }
}

// Called whenever a new recording is selected from the list of
// recordings. This function updates the screen with the information
// on the newly selected recording.
void ViewScheduled::updateInfo(MythUIButtonListItem *item)
{
    if (!item)
        return;

    auto *pginfo = item->GetData().value<ProgramInfo*> ();
    if (pginfo)
    {
        InfoMap infoMap;
        pginfo->ToMap(infoMap);
        SetTextFromMap(infoMap);
        if (m_progressBar != nullptr)
            m_progressBar->Set(0, 100, pginfo->GetRecordedPercent());

        MythUIStateType *ratingState = dynamic_cast<MythUIStateType*>
                                                    (GetChild("ratingstate"));
        if (ratingState)
        {
            QString rating = QString::number(pginfo->GetStars(10));
            ratingState->DisplayState(rating);
        }
    }
}

void ViewScheduled::deleteRule()
{
    MythUIButtonListItem *item = m_schedulesList->GetItemCurrent();

    if (!item)
        return;

    auto *pginfo = item->GetData().value<ProgramInfo*>();
    if (!pginfo)
        return;

    auto *record = new RecordingRule();
    if (!record->LoadByProgram(pginfo))
    {
        delete record;
        return;
    }

    QString message = tr("Delete '%1' %2 rule?")
        .arg(record->m_title, toString(pginfo->GetRecordingRuleType()));

    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");

    auto *okPopup = new MythConfirmationDialog(popupStack, message, true);

    okPopup->SetReturnEvent(this, "deleterule");
    okPopup->SetData(QVariant::fromValue(record));

    if (okPopup->Create())
        popupStack->AddScreen(okPopup);
    else
        delete okPopup;
}

void ViewScheduled::setShowAll(bool all)
{
    m_showAll = all;
    m_needFill = true;
}

void ViewScheduled::viewInputs()
{
    m_needFill = true;

    m_curinput++;
    while (m_curinput <= m_maxinput)
    {
        if (m_inputref[m_curinput] > 0)
            return;
        m_curinput++;
    }
    m_curinput = 0;
}

void ViewScheduled::EmbedTVWindow(void)
{
    if (m_player)
        emit m_player->RequestEmbedding(true);
}

void ViewScheduled::customEvent(QEvent *event)
{
    if (event->type() == MythEvent::kMythEventMessage)
    {
        auto *me = dynamic_cast<MythEvent *>(event);
        if (me == nullptr)
            return;

        const QString& message = me->Message();
        if (message == "SCHEDULE_CHANGE")
        {
            m_needFill = true;

            if (m_inEvent)
                return;

            m_inEvent = true;

            LoadList();

            m_inEvent = false;
        }
        else if (message.startsWith("UPDATE_FILE_SIZE"))
        {
            QStringList tokens = message.simplified().split(" ");
            if (tokens.size() < 3)
                return;

            bool ok {false};
            uint recordingID  = tokens[1].toUInt();
            uint64_t filesize = tokens[2].toLongLong(&ok);

            // Locate program
            auto pit = m_recList.begin();
            while (pit != m_recList.end())
            {
                ProgramInfo* pginfo = *pit;
                if (pginfo && pginfo->GetRecordingID() == recordingID)
                {
                    // Update size & progress
                    pginfo->SetFilesize(filesize);
                    uint current = pginfo->GetRecordedPercent();
                    pginfo->CalculateRecordedProgress();
                    if (pginfo->GetRecordedPercent() != current)
                    {
                        // Update display, if it's shown
                        MythUIButtonListItem *item =
                            m_schedulesList->
                            GetItemByData(QVariant::fromValue(pginfo));
                        if (item)
                        {
                            UpdateUIListItem(item, pginfo);

                            // Update selected item if necessary
                            MythUIButtonListItem *selected =
                                m_schedulesList->GetItemCurrent();
                            if (item == selected)
                                updateInfo(selected);
                        }
                    }
                    break;
                }
                ++pit;
            }
        }
    }
    else if (event->type() == DialogCompletionEvent::kEventType)
    {
        auto *dce = (DialogCompletionEvent*)(event);

        QString resultid   = dce->GetId();
        QString resulttext = dce->GetResultText();
        int     buttonnum  = dce->GetResult();

        if (resultid == "deleterule")
        {
            auto *record = dce->GetData().value<RecordingRule *>();
            if (record)
            {
                if (buttonnum > 0)
                {
                    if (!record->Delete())
                        LOG(VB_GENERAL, LOG_ERR,
                            "Failed to delete recording rule");
                }
                delete record;
            }

            EmbedTVWindow();
        }
        else if (resultid == "menu")
        {
            if (resulttext == tr("Show Important"))
            {
                setShowAll(false);
            }
            else if (resulttext == tr("Show All"))
            {
                setShowAll(true);
            }
            else if (resulttext == tr("Program Details"))
            {
                ShowDetails();
            }
            else if (resulttext == tr("Program Guide"))
            {
                ShowGuide();
            }
            else if (resulttext == tr("Channel Search"))
            {
                ShowChannelSearch();
            }
            else if (resulttext == tr("Upcoming by title"))
            {
                ShowUpcoming();
            }
            else if (resulttext == tr("Upcoming scheduled"))
            {
                ShowUpcomingScheduled();
            }
            else if (resulttext == tr("Previously Recorded"))
            {
                ShowPrevious();
            }
            else if (resulttext == tr("Custom Edit"))
            {
                EditCustom();
            }
            else if (resulttext == tr("Delete Rule"))
            {
                deleteRule();
            }
            else if (resulttext == tr("Show Inputs"))
            {
                viewInputs();
            }

            if (m_needFill)
                LoadList();
        }
        else
        {
            ScheduleCommon::customEvent(event);
        }
    }
}

ProgramInfo *ViewScheduled::GetCurrentProgram(void) const
{
    MythUIButtonListItem *item = m_schedulesList->GetItemCurrent();
    return item ? item->GetData().value<ProgramInfo*>() : nullptr;
}
