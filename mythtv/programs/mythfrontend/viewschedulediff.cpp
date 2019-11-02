// myth
#include "mythcorecontext.h"
#include "mythlogging.h"
#include "remoteutil.h"
#include "recordinginfo.h"

//mythui
#include "mythuitext.h"
#include "mythuibuttonlist.h"
#include "mythmainwindow.h"
#include "mythdialogbox.h"

//mythtv
#include "scheduledrecording.h"
#include "tv.h"

//mythfrontend
#include "viewschedulediff.h"

using namespace std;

bool ViewScheduleDiff::Create()
{
    if (!LoadWindowFromXML("schedule-ui.xml", "schedulediff", this))
        return false;

    bool err = false;
    UIUtilE::Assign(this, m_conflictList, "conflictlist", &err);

    UIUtilW::Assign(this, m_titleText, "titletext");
    UIUtilW::Assign(this, m_noChangesText, "nochanges");

    if (err)
    {
        LOG(VB_GENERAL, LOG_ERR, "Cannot load screen 'schedulediff'");
        return false;
    }

    connect(m_conflictList, SIGNAL(itemSelected(MythUIButtonListItem*)),
            SLOT(updateInfo(MythUIButtonListItem*)));
    connect(m_conflictList, SIGNAL(itemClicked(MythUIButtonListItem*)),
            SLOT(showStatus(MythUIButtonListItem*)));

    if (m_titleText)
        m_titleText->SetText(m_title);

    BuildFocusList();
    LoadInBackground();

    return true;
}

void ViewScheduleDiff::Load(void)
{
    fillList();
}

void ViewScheduleDiff::Init()
{
    updateUIList();
}

bool ViewScheduleDiff::keyPressEvent(QKeyEvent *e)
{
    if (m_inEvent)
        return true;

    m_inEvent = true;

    QStringList actions;

    bool handled = GetMythMainWindow()->TranslateKeyPress("TV Frontend", e, actions);

    if (!handled && MythScreenType::keyPressEvent(e))
        handled = true;

    m_inEvent = false;

    return handled;
}

void ViewScheduleDiff::showStatus(MythUIButtonListItem */*item*/)
{
    ProgramInfo *pi = CurrentProgram();
    if (!pi)
        return;

    QString timeFormat = gCoreContext->GetSetting("TimeFormat", "h:mm AP");

    QString message = pi->toString(ProgramInfo::kTitleSubtitle, " - ");
    message += "\n\n";
    message += RecStatus::toDescription(pi->GetRecordingStatus(),
                             pi->GetRecordingRuleType(),
                             pi->GetRecordingStartTime());

    if (pi->GetRecordingStatus() == RecStatus::Conflict ||
        pi->GetRecordingStatus() == RecStatus::LaterShowing)
    {
        message += " " + QObject::tr("The following programs will be recorded "
                                     "instead:") + "\n\n";

        ProgramList::const_iterator it = m_recListAfter.begin();
        for (; it != m_recListAfter.end(); ++it)
        {
            const ProgramInfo *pa = *it;
            if (pa->GetRecordingStartTime() >= pi->GetRecordingEndTime())
                break;
            if (pa->GetRecordingEndTime() > pi->GetRecordingStartTime() &&
                (pa->GetRecordingStatus() == RecStatus::WillRecord ||
                 pa->GetRecordingStatus() == RecStatus::Pending ||
                 pa->GetRecordingStatus() == RecStatus::Recording ||
                 pa->GetRecordingStatus() == RecStatus::Tuning ||
                 pa->GetRecordingStatus() == RecStatus::Failing))
            {
                message += QString("%1 - %2  %3\n")
                    .arg(pa->GetRecordingStartTime()
                         .toLocalTime().toString(timeFormat))
                    .arg(pa->GetRecordingEndTime()
                         .toLocalTime().toString(timeFormat))
                    .arg(pa->toString(ProgramInfo::kTitleSubtitle, " - "));
            }
        }
    }

    QString title = QObject::tr("Program Status");
    MythScreenStack *mainStack = GetMythMainWindow()->GetStack("main stack");
    MythDialogBox   *dlg = new MythDialogBox(title, message, mainStack,
                                             "statusdialog", true);

    if (dlg->Create())
    {
        dlg->AddButton(QObject::tr("OK"));
        mainStack->AddScreen(dlg);
    }
    else
        delete dlg;
}

static int comp_recstart(const ProgramInfo *a, const ProgramInfo *b)
{
    if (a->GetRecordingStartTime() != b->GetRecordingStartTime())
    {
        if (a->GetRecordingStartTime() > b->GetRecordingStartTime())
            return 1;
        return -1;
    }
    if (a->GetRecordingEndTime() != b->GetRecordingEndTime())
    {
        if (a->GetRecordingEndTime() > b->GetRecordingEndTime())
            return 1;
        return -1;
    }
    if (a->GetChannelSchedulingID() != b->GetChannelSchedulingID())
    {
        if (a->GetChannelSchedulingID() < b->GetChannelSchedulingID())
            return 1;
        return -1;
    }
    if (a->GetRecordingPriority() != b->GetRecordingPriority() &&
        (a->GetRecordingStatus() == RecStatus::WillRecord ||
         a->GetRecordingStatus() == RecStatus::Pending ||
         b->GetRecordingStatus() == RecStatus::WillRecord ||
         b->GetRecordingStatus() == RecStatus::Pending))
    {
        if (a->GetRecordingPriority() < b->GetRecordingPriority())
            return 1;
        return -1;
    }
    return 0;
}

static bool comp_recstart_less_than(const ProgramInfo *a, const ProgramInfo *b)
{
    return comp_recstart(a,b) < 0;
}

void ViewScheduleDiff::fillList(void)
{
    m_inFill = true;

    QString callsign;
    QDateTime startts, recstartts;
    bool dummy = false;

    LoadFromScheduler(m_recListBefore, dummy);
    LoadFromScheduler(m_recListAfter,  dummy, m_altTable, m_recordid);

    std::stable_sort(m_recListBefore.begin(), m_recListBefore.end(),
                     comp_recstart_less_than);
    std::stable_sort(m_recListAfter.begin(), m_recListAfter.end(),
                     comp_recstart_less_than);

    QDateTime now = MythDate::current();

    ProgramList::iterator it = m_recListBefore.begin();
    while (it != m_recListBefore.end())
    {
        if ((*it)->GetRecordingEndTime() >= now ||
            (*it)->GetScheduledEndTime() >= now)
        {
            ++it;
        }
        else
        {
            it = m_recListBefore.erase(it);
        }
    }

    it = m_recListAfter.begin();
    while (it != m_recListAfter.end())
    {
        if ((*it)->GetRecordingEndTime() >= now ||
            (*it)->GetScheduledEndTime() >= now)
        {
            ++it;
        }
        else
        {
            it = m_recListAfter.erase(it);
        }
    }

    ProgramList::iterator pb = m_recListBefore.begin();
    ProgramList::iterator pa = m_recListAfter.begin();
    ProgramStruct s;

    m_recList.clear();
    while (pa != m_recListAfter.end() || pb != m_recListBefore.end())
    {
        s.m_before = (pb != m_recListBefore.end()) ? *pb : nullptr;
        s.m_after  = (pa != m_recListAfter.end())  ? *pa : nullptr;

        if (pa == m_recListAfter.end())
        {
            ++pb;
        }
        else if (pb == m_recListBefore.end())
        {
            ++pa;
        }
        else
        {
            switch (comp_recstart(*pb, *pa))
            {
                case 0:
                    ++pb;
                    ++pa;
                    break;
                case -1: // pb BEFORE pa
                    ++pb;
                    s.m_after = nullptr;
                    break;
                case 1: // pa BEFORE pb
                    s.m_before = nullptr;
                    ++pa;
                    break;
            }
        }

        if (s.m_before && s.m_after &&
            (s.m_before->GetInputID() == s.m_after->GetInputID()) &&
            (s.m_before->GetRecordingStatus() == s.m_after->GetRecordingStatus()))
        {
            continue;
        }

        m_recList.push_back(s);
    }

    m_inFill = false;
}

void ViewScheduleDiff::updateUIList(void)
{
    for (size_t i = 0; i < m_recList.size(); i++)
    {
        class ProgramStruct s = m_recList[i];
        class ProgramInfo *pginfo = s.m_after;
        if (!pginfo)
            pginfo = s.m_before;

        MythUIButtonListItem *item = new MythUIButtonListItem(
            m_conflictList, "", qVariantFromValue(pginfo));

        InfoMap infoMap;
        pginfo->ToMap(infoMap);

        QString state = RecStatus::toUIState(pginfo->GetRecordingStatus());

        item->DisplayState(state, "status");
        item->SetTextFromMap(infoMap, state);

        if (s.m_before)
            item->SetText(RecStatus::toString(s.m_before->GetRecordingStatus(),
                                   s.m_before->GetInputID()), "statusbefore",
                          state);
        else
            item->SetText("-", "statusbefore");

        if (s.m_after)
            item->SetText(RecStatus::toString(s.m_after->GetRecordingStatus(),
                                   s.m_after->GetInputID()), "statusafter",
                          state);
        else
            item->SetText("-", "statusafter");
    }

    if (m_noChangesText)
    {
        if (m_recList.empty())
            m_noChangesText->Show();
        else
            m_noChangesText->Hide();
    }
}

void ViewScheduleDiff::updateInfo(MythUIButtonListItem *item)
{
    if (!item)
        return;

    ProgramInfo *pginfo = item->GetData().value<ProgramInfo*> ();
    if (pginfo)
    {
        InfoMap infoMap;
        pginfo->ToMap(infoMap);
        SetTextFromMap(infoMap);
    }
}

ProgramInfo *ViewScheduleDiff::CurrentProgram()
{
    int pos = m_conflictList->GetCurrentPos();
    if (pos >= (int)m_recList.size())
        return nullptr;

    ProgramStruct s = m_recList[pos];

    if (s.m_after)
        return s.m_after;
    return s.m_before;
}
