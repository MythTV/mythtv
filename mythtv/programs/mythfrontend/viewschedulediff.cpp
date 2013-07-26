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

ViewScheduleDiff::ViewScheduleDiff(MythScreenStack *parent, QString altTable,
                                   int recordidDiff, QString title)
        : MythScreenType(parent, "ViewScheduleDiff"),
            m_inEvent(false), m_inFill(false),
            m_altTable(altTable), m_title(title),
            m_conflictList(NULL), m_titleText(NULL),
            m_noChangesText(NULL), m_recordid(recordidDiff)
{
}

ViewScheduleDiff::~ViewScheduleDiff()
{
}

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

    bool handled = false;
    QStringList actions;

    handled = GetMythMainWindow()->TranslateKeyPress("TV Frontend", e, actions);

    if (!handled && MythScreenType::keyPressEvent(e))
        handled = true;

    m_inEvent = false;

    return handled;
}

void ViewScheduleDiff::showStatus(MythUIButtonListItem *item)
{
    ProgramInfo *pi = CurrentProgram();
    if (!pi)
        return;

    QString timeFormat = gCoreContext->GetSetting("TimeFormat", "h:mm AP");

    QString message = pi->toString(ProgramInfo::kTitleSubtitle, " - ");
    message += "\n\n";
    message += toDescription(pi->GetRecordingStatus(),
                             pi->GetRecordingRuleType(),
                             pi->GetRecordingStartTime());

    if (pi->GetRecordingStatus() == rsConflict ||
        pi->GetRecordingStatus() == rsLaterShowing)
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
                (pa->GetRecordingStatus() == rsWillRecord ||
                 pa->GetRecordingStatus() == rsRecording))
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
        else
            return -1;
    }
    if (a->GetRecordingEndTime() != b->GetRecordingEndTime())
    {
        if (a->GetRecordingEndTime() > b->GetRecordingEndTime())
            return 1;
        else
            return -1;
    }
    if (a->GetChannelSchedulingID() != b->GetChannelSchedulingID())
    {
        if (a->GetChannelSchedulingID() < b->GetChannelSchedulingID())
            return 1;
        else
            return -1;
    }
    if (a->GetRecordingPriority() != b->GetRecordingPriority() &&
        (a->GetRecordingStatus() == rsWillRecord ||
         b->GetRecordingStatus() == rsWillRecord))
    {
        if (a->GetRecordingPriority() < b->GetRecordingPriority())
            return 1;
        else
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
    bool dummy;

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
        s.before = (pb != m_recListBefore.end()) ? *pb : NULL;
        s.after  = (pa != m_recListAfter.end())  ? *pa : NULL;

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
                    s.after = NULL;
                    break;
                case 1: // pa BEFORE pb
                    s.before = NULL;
                    ++pa;
                    break;
            }
        }

        if (s.before && s.after &&
            (s.before->GetCardID() == s.after->GetCardID()) &&
            (s.before->GetRecordingStatus() == s.after->GetRecordingStatus()))
        {
            continue;
        }

        m_recList.push_back(s);
    }

    m_inFill = false;
}

void ViewScheduleDiff::updateUIList(void)
{
    for (uint i = 0; i < m_recList.size(); i++)
    {
        class ProgramStruct s = m_recList[i];
        class ProgramInfo *pginfo = s.after;
        if (!pginfo)
            pginfo = s.before;

        MythUIButtonListItem *item = new MythUIButtonListItem(
            m_conflictList, "", qVariantFromValue(pginfo));

        InfoMap infoMap;
        pginfo->ToMap(infoMap);

        QString state = toUIState(pginfo->GetRecordingStatus());

        item->DisplayState(state, "status");
        item->SetTextFromMap(infoMap, state);

        if (s.before)
            item->SetText(toString(s.before->GetRecordingStatus(),
                                   s.before->GetCardID()), "statusbefore",
                          state);
        else
            item->SetText("-", "statusbefore");

        if (s.after)
            item->SetText(toString(s.after->GetRecordingStatus(),
                                   s.after->GetCardID()), "statusafter",
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
        return NULL;

    ProgramStruct s = m_recList[pos];

    if (s.after)
        return s.after;
    else
        return s.before;
}
