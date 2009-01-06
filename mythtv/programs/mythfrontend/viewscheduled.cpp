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
    m_dateformat = gContext->GetSetting("ShortDateFormat", "M/d");
    m_timeformat = gContext->GetSetting("TimeFormat", "h:mm AP");
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

    FillList();

    if (m_player)
        EmbedTVWindow();

    BuildFocusList();

    return true;
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

        if (action == "MENU" || action == "INFO")
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
        FillList();

    if (!handled && MythScreenType::keyPressEvent(event))
        handled = true;

    m_inEvent = false;

    return handled;
}

void ViewScheduled::FillList(void)
{
    if (m_inFill)
        return;

    m_inFill = true;

    int listPos = m_schedulesList->GetCurrentPos();

    m_schedulesList->Reset();

    QString callsign;
    QDateTime startts, recstartts;

    if (m_recList[listPos])
    {
        callsign = m_recList[listPos]->chansign;
        startts = m_recList[listPos]->startts;
        recstartts = m_recList[listPos]->recstartts;
    }

    m_recList.FromScheduler(m_conflictBool);

    QDateTime now = QDateTime::currentDateTime();

    QMap<int, int> toomanycounts;

    MythUIText *norecordingText = dynamic_cast<MythUIText*>
                                                (GetChild("norecordings_info"));

    if (norecordingText)
        norecordingText->SetVisible(m_recList.isEmpty());

    if (m_recList.isEmpty())
        return;

    ProgramList::iterator pit = m_recList.begin();
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

            ++pit;
        }
        else
        {
            pit = m_recList.erase(pit);
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

        QString temp;
        temp = (pginfo->recstartts).toString(m_dateformat);
        temp += " " + (pginfo->recstartts).toString(m_timeformat);
        item->SetText(temp, "time", state); //,font

        item->SetText(pginfo->ChannelText(m_channelFormat), "channel", state);

        temp = pginfo->title;
        if ((pginfo->subtitle).trimmed().length() > 0)
            temp += " - \"" + pginfo->subtitle + "\"";
        item->SetText(temp, "title", state); //,font

        temp = pginfo->RecStatusChar();
        item->SetText(temp, "card", state); //,font

        item->DisplayState(state, "status");
    }

    // Restore position after a list update
    if (!callsign.isNull())
    {
        listPos = m_recList.count() - 1;
        int i;
        for (i = listPos; i >= 0; i--)
        {
            if (callsign == m_recList[i]->chansign &&
                startts == m_recList[i]->startts)
            {
                listPos = i;
                break;
            }
            else if (recstartts <= m_recList[i]->recstartts)
                listPos = i;
        }
        m_schedulesList->SetItemCurrent(listPos);
    }

    MythUIText *statusText = dynamic_cast<MythUIText*>(GetChild("status"));
    if (statusText)
    {
        if (m_conflictBool)
        {
            // Find first conflict and store in m_conflictDate field
            for (uint i = 0; i < m_recList.count(); i++)
            {
                ProgramInfo *p = m_recList[i];
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
                                .arg(m_conflictDate.toString(m_dateformat));

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

    m_inFill = false;
    m_needFill = false;
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
        SetTextFromMap(this, infoMap);
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

        FillList();

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

void ViewScheduled::SetTextFromMap(MythUIType *parent,
                                   QMap<QString, QString> &infoMap)
{
    if (!parent)
        return;

    QList<MythUIType *> *children = parent->GetAllChildren();

    MythUIText *textType;
    QMutableListIterator<MythUIType *> i(*children);
    while (i.hasNext())
    {
        MythUIType *type = i.next();
        if (!type->IsVisible())
            continue;

        textType = dynamic_cast<MythUIText *> (type);
        if (textType && infoMap.contains(textType->objectName()))
        {
            QString newText = textType->GetDefaultText();
            QRegExp regexp("%(\\|(.))?([^\\|]+)(\\|(.))?%");
            regexp.setMinimal(true);
            if (newText.contains(regexp))
            {
                int pos = 0;
                QString tempString = newText;
                while ((pos = regexp.indexIn(newText, pos)) != -1)
                {
                    QString key = regexp.cap(3).toLower().trimmed();
                    QString replacement;
                    if (!infoMap.value(key).isEmpty())
                    {
                        replacement = QString("%1%2%3")
                                                .arg(regexp.cap(2))
                                                .arg(infoMap.value(key))
                                                .arg(regexp.cap(5));
                    }
                    tempString.replace(regexp.cap(0), replacement);
                    pos += regexp.matchedLength();
                }
                newText = tempString;
            }
            else
                newText = infoMap.value(textType->objectName());

            textType->SetText(newText);
        }
    }
}
