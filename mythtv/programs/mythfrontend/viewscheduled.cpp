#include <stdlib.h>

#include <iostream>
using namespace std;

#include <QDateTime>
#include <QRegExp>

#include "viewscheduled.h"
#include "scheduledrecording.h"
#include "customedit.h"
#include "programinfo.h"
#include "proglist.h"
#include "tv_play.h"

#include "mythcontext.h"
#include "mythverbose.h"
#include "remoteutil.h"

#include "mythuitext.h"
#include "mythuistatetype.h"
#include "mythuibuttonlist.h"
#include "mythdialogbox.h"

void *ViewScheduled::RunViewScheduled(void *player, bool showTV)
{
    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
    ViewScheduled *vsb = new ViewScheduled(mainStack, (TV*)player, showTV);

    if (vsb->Create())
        mainStack->AddScreen(vsb);
    else
        delete vsb;

    return NULL;
}

ViewScheduled::ViewScheduled(MythScreenStack *parent, TV* player, bool showTV)
             : MythScreenType(parent, "ViewScheduled")
{
    m_shortdateFormat = gContext->GetSetting("ShortDateFormat", "M/d");
    m_dateFormat = gContext->GetSetting("DateFormat", "ddd MMMM d");
    m_timeFormat = gContext->GetSetting("TimeFormat", "h:mm AP");
    m_channelFormat = gContext->GetSetting("ChannelFormat", "<num> <sign>");
    m_showAll = !gContext->GetNumSetting("ViewSchedShowLevel", 0);

    m_player = player;

    m_inEvent = false;
    m_inFill = false;
    m_needFill = false;

    m_curcard  = 0;
    m_maxcard  = 0;
    m_curinput = 0;
    m_maxinput = 0;

    m_defaultGroup = QDate();
    m_currentGroup = m_defaultGroup;

    gContext->addListener(this);
}

ViewScheduled::~ViewScheduled()
{
    gContext->removeListener(this);
    gContext->SaveSetting("ViewSchedShowLevel", !m_showAll);
}

bool ViewScheduled::Create()
{
    if (!LoadWindowFromXML("schedule-ui.xml", "viewscheduled", this))
        return false;

    //if (m_player && m_player->IsRunning() && showTV)

    m_groupList     = dynamic_cast<MythUIButtonList *> (GetChild("groups"));
    m_schedulesList = dynamic_cast<MythUIButtonList *> (GetChild("schedules"));

    if (!m_schedulesList)
    {
        VERBOSE(VB_IMPORTANT, "Theme is missing critical theme elements.");
        return false;
    }

    connect(m_schedulesList, SIGNAL(itemSelected(MythUIButtonListItem*)),
            SLOT(updateInfo(MythUIButtonListItem*)));
    connect(m_schedulesList, SIGNAL(itemClicked(MythUIButtonListItem*)),
            SLOT(selected(MythUIButtonListItem*)));

    LoadList();

    if (m_groupList)
    {
        connect(m_groupList, SIGNAL(itemSelected(MythUIButtonListItem*)),
                SLOT(ChangeGroup(MythUIButtonListItem*)));
        connect(m_groupList, SIGNAL(itemClicked(MythUIButtonListItem*)),
                SLOT(SwitchList()));
    }

    if (m_player)
        EmbedTVWindow();

    BuildFocusList();

    return true;
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

    bool handled = false;
    QStringList actions;
    gContext->GetMainWindow()->TranslateKeyPress("TV Frontend", event, actions);

    for (int i = 0; i < actions.size() && !handled; i++)
    {
        QString action = actions[i];
        handled = true;

        if (action == "INFO")
            edit();
        else if (action == "CUSTOMEDIT")
            customEdit();
        else if (action == "DELETE")
            deleteRule();
        else if (action == "UPCOMING")
            upcoming();
        else if (action == "DETAILS")
            details();
        else if (action == "1")
            setShowAll(true);
        else if (action == "2")
            setShowAll(false);
        else if (action == "PREVVIEW" || action == "NEXTVIEW")
            setShowAll(!m_showAll);
        else if (action == "VIEWCARD")
            viewCards();
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

void ViewScheduled::LoadList(void)
{
    if (m_inFill)
        return;

    m_inFill = true;

    MythUIButtonListItem *currentItem = m_schedulesList->GetItemCurrent();

    QString callsign;
    QDateTime startts, recstartts;

    if (currentItem)
    {
        ProgramInfo *currentpginfo = qVariantValue<ProgramInfo*>
                                                    (currentItem->GetData());
        if (currentpginfo)
        {
            callsign = currentpginfo->chansign;
            startts = currentpginfo->startts;
            recstartts = currentpginfo->recstartts;
        }
    }

    QDateTime now = QDateTime::currentDateTime();

    QMap<int, int> toomanycounts;

    m_schedulesList->Reset();
    if (m_groupList)
        m_groupList->Reset();

    m_recgroupList.clear();

    m_recList.FromScheduler(m_conflictBool);

    ProgramList::iterator pit = m_recList.begin();
    QString currentDate;
    m_recgroupList[m_defaultGroup] = ProgramList(false);
    m_recgroupList[m_defaultGroup].setAutoDelete(false);
    while (pit != m_recList.end())
    {

        ProgramInfo *pginfo = *pit;
        if ((pginfo->recendts >= now || pginfo->endts >= now) &&
            (m_showAll || pginfo->recstatus <= rsWillRecord ||
             pginfo->recstatus == rsDontRecord ||
             (pginfo->recstatus == rsTooManyRecordings &&
              ++toomanycounts[pginfo->recordid] <= 1) ||
             (pginfo->recstatus > rsTooManyRecordings &&
              pginfo->recstatus != rsRepeat &&
              pginfo->recstatus != rsNeverRecord)))
        {
            m_cardref[pginfo->cardid]++;
            if (pginfo->cardid > m_maxcard)
                m_maxcard = pginfo->cardid;

            m_inputref[pginfo->inputid]++;
            if (pginfo->inputid > m_maxinput)
                m_maxinput = pginfo->inputid;

            QDate date = (pginfo->recstartts).date();
            m_recgroupList[date].append(pginfo);
            m_recgroupList[date].setAutoDelete(false);

            m_recgroupList[m_defaultGroup].append(pginfo);

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
                label = dateit.key().toString(m_dateFormat);

            new MythUIButtonListItem(m_groupList, label,
                                     qVariantFromValue(dateit.key()));
            ++dateit;
        }
        if (!m_recgroupList.contains(m_currentGroup))
            m_groupList->SetValueByData(qVariantFromValue(m_currentGroup));
    }

    FillList();

    // Restore position after a list update
    if (!callsign.isNull())
    {
        ProgramList plist;

        if (!m_recgroupList.contains(m_currentGroup))
            m_currentGroup = m_defaultGroup;

        plist = m_recgroupList[m_currentGroup];

        int listPos = plist.count() - 1;
        int i;
        for (i = listPos; i >= 0; i--)
        {
            if (callsign == plist[i]->chansign &&
                startts == plist[i]->startts)
            {
                listPos = i;
                break;
            }
            else if (recstartts <= plist[i]->recstartts)
                listPos = i;
        }
        m_schedulesList->SetItemCurrent(listPos);
    }

    m_inFill = false;
    m_needFill = false;
}


void ViewScheduled::ChangeGroup(MythUIButtonListItem* item)
{
    if (!item || m_recList.isEmpty())
        return;

    QDate group = qVariantValue<QDate>(item->GetData());

    m_currentGroup = group;

    if (!m_inFill)
        FillList();
}

void ViewScheduled::FillList()
{
    m_schedulesList->Reset();

    MythUIText *norecordingText = dynamic_cast<MythUIText*>
                                                (GetChild("norecordings_info"));

    if (norecordingText)
        norecordingText->SetVisible(m_recList.isEmpty());

    if (m_recList.isEmpty())
        return;

    ProgramList plist;

    if (!m_recgroupList.contains(m_currentGroup))
        m_currentGroup = m_defaultGroup;

    plist = m_recgroupList[m_currentGroup];

    ProgramList::iterator pit = plist.begin();
    while (pit != plist.end())
    {
        ProgramInfo *pginfo = *pit;
        if (!pginfo)
        {
            ++pit;
            continue;
        }

        QString state;

        if (pginfo->recstatus == rsRecording)
            state = "running";
        else if (pginfo->recstatus == rsConflict ||
                    pginfo->recstatus == rsOffLine ||
                    pginfo->recstatus == rsAborted)
            state = "error";
        else if (pginfo->recstatus == rsWillRecord)
        {
            if ((m_curcard == 0 && m_curinput == 0) ||
                pginfo->cardid == m_curcard || pginfo->inputid == m_curinput)
                state = "normal";
        }
        else if (pginfo->recstatus == rsRepeat ||
                    pginfo->recstatus == rsOtherShowing ||
                    pginfo->recstatus == rsNeverRecord ||
                    pginfo->recstatus == rsDontRecord ||
                    (pginfo->recstatus != rsDontRecord &&
                    pginfo->recstatus <= rsEarlierShowing))
            state = "disabled";
        else
            state = "warning";

        MythUIButtonListItem *item =
                                new MythUIButtonListItem(m_schedulesList,"",
                                                    qVariantFromValue(pginfo));

        QMap<QString, QString> infoMap;
        pginfo->ToMap(infoMap);
        item->SetTextFromMap(infoMap, state);

        QString rating = QString::number((int)((pginfo->stars * 10.0) + 0.5));
        item->DisplayState(rating, "ratingstate");
        item->DisplayState(state, "status");

        ++pit;
    }

    MythUIText *statusText = dynamic_cast<MythUIText*>(GetChild("status"));
    if (statusText)
    {
        if (m_conflictBool)
        {
            // Find first conflict and store in m_conflictDate field
            for (uint i = 0; i < plist.count(); i++)
            {
                ProgramInfo *p = plist[i];
                if (p->recstatus == rsConflict)
                {
                    m_conflictDate = p->recstartts.date();
                    break;
                }
            }

            // figure out caption based on m_conflictDate
            QString cstring = tr("Time Conflict");
            QDate now = QDate::currentDate();
            int daysToConflict = now.daysTo(m_conflictDate);

            if (daysToConflict == 0)
                cstring = tr("Conflict Today");
            else if (daysToConflict > 0)
                cstring = QString(tr("Conflict %1"))
                                .arg(m_conflictDate.toString(m_dateFormat));

            statusText->SetText(cstring);
        }
        else
            statusText->SetText(tr("No Conflicts"));
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

void ViewScheduled::updateInfo(MythUIButtonListItem *item)
{
    if (!item)
        return;

    ProgramInfo *pginfo = qVariantValue<ProgramInfo*> (item->GetData());
    if (pginfo)
    {
        QMap<QString, QString> infoMap;
        pginfo->ToMap(infoMap);
        SetTextFromMap(infoMap);
    }
}

void ViewScheduled::edit()
{
    MythUIButtonListItem *item = m_schedulesList->GetItemCurrent();

    if (!item)
        return;

    ProgramInfo *pginfo = qVariantValue<ProgramInfo*>(item->GetData());
    if (!pginfo)
        return;

    pginfo->EditScheduled();
}

void ViewScheduled::customEdit()
{
    MythUIButtonListItem *item = m_schedulesList->GetItemCurrent();

    if (!item)
        return;

    ProgramInfo *pginfo = qVariantValue<ProgramInfo*>(item->GetData());
    if (!pginfo)
        return;

    CustomEdit *ce = new CustomEdit(gContext->GetMainWindow(),
                                    "customedit", pginfo);
    ce->exec();
    delete ce;
}

void ViewScheduled::deleteRule()
{
    MythUIButtonListItem *item = m_schedulesList->GetItemCurrent();

    if (!item)
        return;

    ProgramInfo *pginfo = qVariantValue<ProgramInfo*>(item->GetData());
    if (!pginfo)
        return;

    ScheduledRecording *record = new ScheduledRecording();
    int recid = pginfo->recordid;
    record->loadByID(recid);

    QString message =
        tr("Delete '%1' %2 rule?").arg(record->getRecordTitle())
                                  .arg(pginfo->RecTypeText());

    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");

    MythConfirmationDialog *okPopup = new MythConfirmationDialog(popupStack,
                                                                 message, true);

    okPopup->SetReturnEvent(this, "deleterule");
    okPopup->SetData(qVariantFromValue(record));

    if (okPopup->Create())
        popupStack->AddScreen(okPopup);
    else
        delete okPopup;
}

void ViewScheduled::upcoming()
{
    MythUIButtonListItem *item = m_schedulesList->GetItemCurrent();
    if (!item)
        return;

    ProgramInfo *pginfo = qVariantValue<ProgramInfo*>(item->GetData());
    if (!pginfo)
        return;

    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
    ProgLister *pl = new ProgLister(mainStack, plTitle, pginfo->title, "");
    if (pl->Create())
    {
        mainStack->AddScreen(pl);
    }
    else
        delete pl;

    //FIXME:
    //EmbedTVWindow();
}

void ViewScheduled::details()
{
    MythUIButtonListItem *item = m_schedulesList->GetItemCurrent();

    if (!item)
        return;

    ProgramInfo *pginfo = qVariantValue<ProgramInfo*>(item->GetData());
    if (pginfo)
        pginfo->showDetails();

    EmbedTVWindow();
}

void ViewScheduled::selected(MythUIButtonListItem *item)
{
    if (!item)
        return;

    ProgramInfo *pginfo = qVariantValue<ProgramInfo*> (item->GetData());
    if (!pginfo)
        return;

    pginfo->EditRecording();

    EmbedTVWindow();
}

void ViewScheduled::setShowAll(bool all)
{
    m_showAll = all;
    m_needFill = true;
}

void ViewScheduled::viewCards()
{
    m_curinput = 0;
    m_needFill = true;

    m_curcard++;
    while (m_curcard <= m_maxcard)
    {
        if (m_cardref[m_curcard] > 0)
            return;
        m_curcard++;
    }
    m_curcard = 0;
}

void ViewScheduled::viewInputs()
{
    m_curcard = 0;
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
    {
//         m_player->EmbedOutput(this->winId(), m_tvRect.x(), m_tvRect.y(),
//                             m_tvRect.width(), m_tvRect.height());
    }
}

void ViewScheduled::customEvent(QEvent *event)
{
    if ((MythEvent::Type)(event->type()) == MythEvent::MythEventMessage)
    {
        MythEvent *me = (MythEvent *)event;
        QString message = me->Message();

        if (message != "SCHEDULE_CHANGE")
            return;

        m_needFill = true;

        if (m_inEvent)
            return;

        m_inEvent = true;

        LoadList();

        m_inEvent = false;
    }
    else if (event->type() == kMythDialogBoxCompletionEventType)
    {
        DialogCompletionEvent *dce =
                                dynamic_cast<DialogCompletionEvent*>(event);

        QString resultid= dce->GetId();
        int buttonnum  = dce->GetResult();

        if (resultid == "deleterule")
        {
            ScheduledRecording *record = qVariantValue<ScheduledRecording *>(dce->GetData());
            if (record)
            {
                if (buttonnum > 0)
                {
                    record->remove();
                    ScheduledRecording::signalChange(record->getRecordID());
                }
                record->deleteLater();
            }

            EmbedTVWindow();
        }
    }

}
