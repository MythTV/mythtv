
// own header
#include "programrecpriority.h"

// C/C++ headers
#include <vector> // For std::vector
using namespace std;

// QT headers
#include <QDateTime>
#include <QRegExp>

// libmythtv headers
#include "recordingrule.h"
#include "scheduledrecording.h"
#include "storagegroup.h"

// libmythdb
#include "mythdb.h"
#include "mythverbose.h"
#include "remoteutil.h"

// libmythui
#include "mythuihelper.h"
#include "mythuibuttonlist.h"
#include "mythuitext.h"
#include "mythuistatetype.h"
#include "mythdialogbox.h"

// mythfrontend
#include "customedit.h"
#include "proglist.h"
#include "scheduleeditor.h"

// overloaded version of RecordingInfo with additional recording priority
// values so we can keep everything together and don't
// have to hit the db mulitiple times
ProgramRecPriorityInfo::ProgramRecPriorityInfo(void) :
    RecordingInfo(),
    recTypeRecPriority(0), recType(kNotRecording),
    matchCount(0),         recCount(0),
    avg_delay(0),          autoRecPriority(0),
    profile("")
{
}

ProgramRecPriorityInfo::ProgramRecPriorityInfo(
    const ProgramRecPriorityInfo &other) :
    RecordingInfo::RecordingInfo(other),
    recTypeRecPriority(other.recTypeRecPriority),
    recType(other.recType),
    matchCount(other.matchCount),
    recCount(other.recCount),
    avg_delay(other.avg_delay),
    autoRecPriority(other.autoRecPriority),
    profile(other.profile)
{
}

ProgramRecPriorityInfo &ProgramRecPriorityInfo::operator=(
    const ProgramInfo &other)
{
    return clone(other);
}

ProgramRecPriorityInfo &ProgramRecPriorityInfo::operator=(
    const ProgramRecPriorityInfo &other)
{
    return clone(other);
}

ProgramRecPriorityInfo &ProgramRecPriorityInfo::operator=(
    const RecordingInfo &other)
{
    return clone((ProgramInfo&)other);
}

ProgramRecPriorityInfo &ProgramRecPriorityInfo::clone(
    const ProgramRecPriorityInfo &other)
{
    RecordingInfo::clone(other);

    recTypeRecPriority = other.recTypeRecPriority;
    recType            = other.recType;
    matchCount         = other.matchCount;
    recCount           = other.recCount;
    last_record        = other.last_record;
    avg_delay          = other.avg_delay;
    autoRecPriority    = other.autoRecPriority;
    profile            = other.profile;

    return *this;
}

ProgramRecPriorityInfo &ProgramRecPriorityInfo::clone(const ProgramInfo &other)
{
    RecordingInfo::clone(other);

    recTypeRecPriority = 0;
    recType            = kNotRecording;
    matchCount         = 0;
    recCount           = 0;
    last_record        = QDateTime();
    avg_delay          = 0;
    autoRecPriority    = 0;
    profile.clear();

    return *this;
}

void ProgramRecPriorityInfo::clear(void)
{
    RecordingInfo::clear();

    recTypeRecPriority = 0;
    recType            = kNotRecording;
    matchCount         = 0;
    recCount           = 0;
    last_record        = QDateTime();
    avg_delay          = 0;
    autoRecPriority    = 0;
    profile.clear();
}

typedef struct RecPriorityInfo
{
    ProgramRecPriorityInfo *prog;
    int cnt;
} RecPriorityInfo;

class titleSort
{
    public:
        titleSort(bool m_reverseSort = false) {m_reverse = m_reverseSort;}

        bool operator()(const RecPriorityInfo &a, const RecPriorityInfo &b)
        {
            if (a.prog->sortTitle != b.prog->sortTitle)
            {
                if (m_reverse)
                    return (a.prog->sortTitle < b.prog->sortTitle);
                else
                    return (a.prog->sortTitle > b.prog->sortTitle);
            }

            int finalA = a.prog->GetRecordingPriority() +
                a.prog->recTypeRecPriority;
            int finalB = b.prog->GetRecordingPriority() +
                b.prog->recTypeRecPriority;
            if (finalA != finalB)
            {
                if (m_reverse)
                    return finalA > finalB;
                else
                    return finalA < finalB;
            }

            int typeA = RecTypePriority(a.prog->recType);
            int typeB = RecTypePriority(b.prog->recType);
            if (typeA != typeB)
            {
                if (m_reverse)
                    return typeA < typeB;
                else
                    return typeA > typeB;
            }

            if (m_reverse)
                return (a.prog->GetRecordingRuleID() <
                        b.prog->GetRecordingRuleID());
            else
                return (a.prog->GetRecordingRuleID() >
                        b.prog->GetRecordingRuleID());
        }

    private:
        bool m_reverse;
};

class programRecPrioritySort
{
    public:
        programRecPrioritySort(bool m_reverseSort = false)
                               {m_reverse = m_reverseSort;}

        bool operator()(const RecPriorityInfo &a, const RecPriorityInfo &b)
        {
            int finalA = (a.prog->GetRecordingPriority() +
                          a.prog->autoRecPriority +
                          a.prog->recTypeRecPriority);
            int finalB = (b.prog->GetRecordingPriority() +
                          b.prog->autoRecPriority +
                          b.prog->recTypeRecPriority);
            if (finalA != finalB)
            {
                if (m_reverse)
                    return finalA > finalB;
                else
                    return finalA < finalB;
            }

            int typeA = RecTypePriority(a.prog->recType);
            int typeB = RecTypePriority(b.prog->recType);
            if (typeA != typeB)
            {
                if (m_reverse)
                    return typeA < typeB;
                else
                    return typeA > typeB;
            }

            if (m_reverse)
                return (a.prog->GetRecordingRuleID() <
                        b.prog->GetRecordingRuleID());
            else
                return (a.prog->GetRecordingRuleID() >
                        b.prog->GetRecordingRuleID());
        }

    private:
        bool m_reverse;
};

class programRecTypeSort
{
    public:
        programRecTypeSort(bool m_reverseSort = false)
                               {m_reverse = m_reverseSort;}

        bool operator()(const RecPriorityInfo &a, const RecPriorityInfo &b)
        {
            int typeA = RecTypePriority(a.prog->recType);
            int typeB = RecTypePriority(b.prog->recType);
            if (typeA != typeB)
            {
                if (m_reverse)
                    return (typeA < typeB);
                else
                    return (typeA > typeB);
            }

            int finalA = (a.prog->GetRecordingPriority() +
                          a.prog->recTypeRecPriority);
            int finalB = (b.prog->GetRecordingPriority() +
                          b.prog->recTypeRecPriority);
            if (finalA != finalB)
            {
                if (m_reverse)
                    return finalA > finalB;
                else
                    return finalA < finalB;
            }

            if (m_reverse)
                return (a.prog->GetRecordingRuleID() <
                        b.prog->GetRecordingRuleID());
            else
                return (a.prog->GetRecordingRuleID() >
                        b.prog->GetRecordingRuleID());
        }

    private:
        bool m_reverse;
};

class programCountSort
{
    public:
        programCountSort(bool m_reverseSort = false) {m_reverse = m_reverseSort;}

        bool operator()(const RecPriorityInfo &a, const RecPriorityInfo &b)
        {
            int countA = a.prog->matchCount;
            int countB = b.prog->matchCount;
            int recCountA = a.prog->recCount;
            int recCountB = b.prog->recCount;

            if (countA != countB)
            {
                if (m_reverse)
                    return countA > countB;
                else
                    return countA < countB;
            }
            if (recCountA != recCountB)
            {
                if (m_reverse)
                    return recCountA > recCountB;
                else
                    return recCountA < recCountB;
            }
            return (a.prog->sortTitle > b.prog->sortTitle);
        }

    private:
        bool m_reverse;
};

class programRecCountSort
{
    public:
        programRecCountSort(bool m_reverseSort=false)
        {
            m_reverse = m_reverseSort;
        }

        bool operator()(const RecPriorityInfo &a, const RecPriorityInfo &b)
        {
            int countA = a.prog->matchCount;
            int countB = b.prog->matchCount;
            int recCountA = a.prog->recCount;
            int recCountB = b.prog->recCount;

            if (recCountA != recCountB)
            {
                if (m_reverse)
                    return recCountA > recCountB;
                else
                    return recCountA < recCountB;
            }
            if (countA != countB)
            {
                if (m_reverse)
                    return countA > countB;
                else
                    return countA < countB;
            }
            return (a.prog->sortTitle > b.prog->sortTitle);
        }

    private:
        bool m_reverse;
};

class programLastRecordSort
{
    public:
        programLastRecordSort(bool m_reverseSort=false)
            {m_reverse = m_reverseSort;}

        bool operator()(const RecPriorityInfo &a, const RecPriorityInfo &b)
        {
            QDateTime lastRecA = a.prog->last_record;
            QDateTime lastRecB = b.prog->last_record;

            if (lastRecA != lastRecB)
            {
                if (m_reverse)
                    return lastRecA > lastRecB;
                else
                    return lastRecA < lastRecB;
            }
            return (a.prog->sortTitle > b.prog->sortTitle);
        }

    private:
        bool m_reverse;
};

class programAvgDelaySort
{
    public:
        programAvgDelaySort(bool m_reverseSort=false)
            {m_reverse = m_reverseSort;}

        bool operator()(const RecPriorityInfo &a, const RecPriorityInfo &b)
        {
            int avgA = a.prog->avg_delay;
            int avgB = b.prog->avg_delay;

            if (avgA != avgB)
            {
                if (m_reverse)
                    return avgA < avgB;
                else
                    return avgA > avgB;
            }
            return (a.prog->sortTitle > b.prog->sortTitle);
        }

    private:
        bool m_reverse;
};

////////////////////////////////////////////////////////

ProgramRecPriority::ProgramRecPriority(MythScreenStack *parent,
                                       const QString &name)
                   : ScheduleCommon(parent, name),
                     m_programList(NULL), m_categoryText(NULL),
                     m_descriptionText(NULL), m_schedInfoText(NULL),
                     m_rectypePriorityText(NULL), m_recPriorityText(NULL),
                     m_recPriorityBText(NULL), m_finalPriorityText(NULL),
                     m_recGroupText(NULL), m_storageGroupText(NULL),
                     m_lastRecordedText(NULL), m_lastRecordedDateText(NULL),
                     m_lastRecordedTimeText(NULL), m_channameText(NULL),
                     m_channumText(NULL), m_callsignText(NULL),
                     m_recProfileText(NULL), m_currentItem(NULL)
{
    m_sortType = (SortType)gCoreContext->GetNumSetting("ProgramRecPrioritySorting",
                                                 (int)byTitle);
    m_reverseSort = gCoreContext->GetNumSetting("ProgramRecPriorityReverse", 0);
    m_formatShortDate = gCoreContext->GetSetting("ShortDateFormat", "M/d");
    m_formatLongDate  = gCoreContext->GetSetting("DateFormat", "ddd MMMM d");
    m_formatTime      = gCoreContext->GetSetting("TimeFormat", "h:mm AP");
}

ProgramRecPriority::~ProgramRecPriority()
{
}

bool ProgramRecPriority::Create()
{
    QString window_name = "programrecpriority";
    if (objectName() == "ManageRecRules")
        window_name = "managerecrules";

    if (!LoadWindowFromXML("schedule-ui.xml", window_name, this))
        return false;

    m_programList = dynamic_cast<MythUIButtonList *> (GetChild("programs"));

    m_descriptionText = dynamic_cast<MythUIText *> (GetChild("description"));
    m_categoryText = dynamic_cast<MythUIText *> (GetChild("category"));
    m_schedInfoText = dynamic_cast<MythUIText *> (GetChild("scheduleinfo"));
    m_rectypePriorityText = dynamic_cast<MythUIText *>
                                                 (GetChild("rectypepriority"));
    m_recPriorityText = dynamic_cast<MythUIText *> (GetChild("recpriority"));
    m_recPriorityBText = dynamic_cast<MythUIText *> (GetChild("recpriorityB"));
    m_finalPriorityText = dynamic_cast<MythUIText *> (GetChild("finalpriority"));
    m_recGroupText = dynamic_cast<MythUIText *> (GetChild("recordinggroup"));
    m_storageGroupText = dynamic_cast<MythUIText *> (GetChild("storagegroup"));
    m_lastRecordedText = dynamic_cast<MythUIText *> (GetChild("lastrecorded"));
    m_lastRecordedDateText = dynamic_cast<MythUIText *> (GetChild("lastrecordeddate"));
    m_lastRecordedTimeText = dynamic_cast<MythUIText *> (GetChild("lastrecordedtime"));
    m_channameText = dynamic_cast<MythUIText *> (GetChild("channel"));
    m_channumText = dynamic_cast<MythUIText *> (GetChild("channum"));
    m_callsignText = dynamic_cast<MythUIText *> (GetChild("callsign"));
    m_recProfileText = dynamic_cast<MythUIText *> (GetChild("recordingprofile"));

    if (!m_programList)
    {
        VERBOSE(VB_IMPORTANT, "Theme is missing critical theme elements.");
        return false;
    }

    connect(m_programList, SIGNAL(itemSelected(MythUIButtonListItem*)),
            SLOT(updateInfo(MythUIButtonListItem*)));
    connect(m_programList, SIGNAL(itemClicked(MythUIButtonListItem*)),
            SLOT(edit(MythUIButtonListItem*)));

    m_programList->SetLCDTitles(tr("Schedule Priorities"), "rec_type|titlesubtitle|progpriority|finalpriority");

    BuildFocusList();
    LoadInBackground();

    return true;
}

void ProgramRecPriority::Load(void)
{
    FillList();
}

void ProgramRecPriority::Init(void)
{
    SortList();
}

bool ProgramRecPriority::keyPressEvent(QKeyEvent *event)
{
    if (GetFocusWidget()->keyPressEvent(event))
        return true;

    bool handled = false;
    QStringList actions;
    handled = GetMythMainWindow()->TranslateKeyPress("TV Frontend", event, actions);

    for (int i = 0; i < actions.size() && !handled; i++)
    {
        QString action = actions[i];
        handled = true;

        if (action == "RANKINC")
            changeRecPriority(1);
        else if (action == "RANKDEC")
            changeRecPriority(-1);
        else if (action == "ESCAPE")
        {
            saveRecPriority();
            gCoreContext->SaveSetting("ProgramRecPrioritySorting",
                                    (int)m_sortType);
            gCoreContext->SaveSetting("ProgramRecPriorityReverse",
                                    (int)m_reverseSort);
            Close();
        }
        else if (action == "1")
        {
            if (m_sortType != byTitle)
            {
                m_sortType = byTitle;
                m_reverseSort = false;
            }
            else
                m_reverseSort = !m_reverseSort;
            SortList();
        }
        else if (action == "2")
        {
            if (m_sortType != byRecPriority)
            {
                m_sortType = byRecPriority;
                m_reverseSort = false;
            }
            else
                m_reverseSort = !m_reverseSort;
            SortList();
        }
        else if (action == "4")
        {
            if (m_sortType != byRecType)
            {
                m_sortType = byRecType;
                m_reverseSort = false;
            }
            else
                m_reverseSort = !m_reverseSort;
            SortList();
        }
        else if (action == "5")
        {
            if (m_sortType != byCount)
            {
                m_sortType = byCount;
                m_reverseSort = false;
            }
            else
            {
                m_reverseSort = !m_reverseSort;
            }
            SortList();
        }
        else if (action == "6")
        {
            if (m_sortType != byRecCount)
            {
                m_sortType = byRecCount;
                m_reverseSort = false;
            }
            else
                m_reverseSort = !m_reverseSort;
            SortList();
        }
        else if (action == "7")
        {
            if (m_sortType != byLastRecord)
            {
                m_sortType = byLastRecord;
                m_reverseSort = false;
            }
            else
                m_reverseSort = !m_reverseSort;
            SortList();
        }
        else if (action == "8")
        {
            if (m_sortType != byAvgDelay)
            {
                m_sortType = byAvgDelay;
                m_reverseSort = false;
            }
            else
                m_reverseSort = !m_reverseSort;
            SortList();
        }
        else if (action == "PREVVIEW" || action == "NEXTVIEW")
        {
            m_reverseSort = false;
            if (m_sortType == byTitle)
                m_sortType = byRecPriority;
            else if (m_sortType == byRecPriority)
                m_sortType = byRecType;
            else
                m_sortType = byTitle;
            SortList();
        }
        else if (action == "SELECT" || action == "EDIT")
        {
            saveRecPriority();
            edit(m_programList->GetItemCurrent());
        }
        else if (action == "MENU")
        {
            showMenu();
        }
        else if (action == "CUSTOMEDIT")
        {
            saveRecPriority();
            customEdit();
        }
        else if (action == "DELETE")
        {
            saveRecPriority();
            remove();
        }
        else if (action == "UPCOMING")
        {
            saveRecPriority();
            upcoming();
        }
        else if (action == "INFO" || action == "DETAILS")
            details();
        else
            handled = false;
    }

    if (!handled && MythScreenType::keyPressEvent(event))
        handled = true;

    return handled;
}

void ProgramRecPriority::showMenu(void)
{
    QString label = tr("Options");

    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");
    MythDialogBox *menuPopup = new MythDialogBox(label, popupStack, "menuPopup");

    if (menuPopup->Create())
    {
        menuPopup->SetReturnEvent(this, "menu");

        menuPopup->AddButton(tr("Increase Priority"));
        menuPopup->AddButton(tr("Decrease Priority"));
        menuPopup->AddButton(tr("Sort"), NULL, true);
        menuPopup->AddButton(tr("Program Details"));
        menuPopup->AddButton(tr("Upcoming"));
        menuPopup->AddButton(tr("Custom Edit"));
        menuPopup->AddButton(tr("Delete Rule"));
        menuPopup->AddButton(tr("Cancel"));

        popupStack->AddScreen(menuPopup);
    }
    else
    {
        delete menuPopup;
    }
}

void ProgramRecPriority::showSortMenu(void)
{
    QString label = tr("Sort Options");

    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");
    MythDialogBox *menuPopup = new MythDialogBox(label, popupStack, "menuPopup");

    if (menuPopup->Create())
    {
        menuPopup->SetReturnEvent(this, "sortmenu");

        menuPopup->AddButton(tr("Reverse Sort Order"));
        menuPopup->AddButton(tr("Sort By Title"));
        menuPopup->AddButton(tr("Sort By Priority"));
        menuPopup->AddButton(tr("Sort By Type"));
        menuPopup->AddButton(tr("Sort By Count"));
        menuPopup->AddButton(tr("Sort By Record Count"));
        menuPopup->AddButton(tr("Sort By Last Recorded"));
        menuPopup->AddButton(tr("Sort By Average Delay"));
        menuPopup->AddButton(tr("Cancel"));

        popupStack->AddScreen(menuPopup);
    }
    else
    {
        delete menuPopup;
    }
}

void ProgramRecPriority::customEvent(QEvent *event)
{
    if (event->type() == DialogCompletionEvent::kEventType)
    {
        DialogCompletionEvent *dce = (DialogCompletionEvent*)(event);

        QString resultid   = dce->GetId();
        QString resulttext = dce->GetResultText();
        int     buttonnum  = dce->GetResult();

        if (resultid == "menu")
        {
            if (resulttext == tr("Increase Priority"))
            {
                changeRecPriority(1);
            }
            else if (resulttext == tr("Decrease Priority"))
            {
                changeRecPriority(-1);
            }
            else if (resulttext == tr("Sort"))
            {
                showSortMenu();
            }
            else if (resulttext == tr("Program Details"))
            {
                details();
            }
            else if (resulttext == tr("Upcoming"))
            {
                saveRecPriority();
                upcoming();
            }
            else if (resulttext == tr("Custom Edit"))
            {
                saveRecPriority();
                customEdit();
            }
            else if (resulttext == tr("Delete Rule"))
            {
                saveRecPriority();
                remove();
            }
        }
        else if (resultid == "sortmenu")
        {
            if (resulttext == tr("Reverse Sort Order"))
            {
                m_reverseSort = !m_reverseSort;
                SortList();
            }
            else if (resulttext == tr("Sort By Title"))
            {
                if (m_sortType != byTitle)
                {
                    m_sortType = byTitle;
                    m_reverseSort = false;
                }
                else
                    m_reverseSort = !m_reverseSort;
                SortList();
            }
            else if (resulttext == tr("Sort By Priority"))
            {
                if (m_sortType != byRecPriority)
                {
                    m_sortType = byRecPriority;
                    m_reverseSort = false;
                }
                else
                    m_reverseSort = !m_reverseSort;
                SortList();
            }
            else if (resulttext == tr("Sort By Type"))
            {
                if (m_sortType != byRecType)
                {
                    m_sortType = byRecType;
                    m_reverseSort = false;
                }
                else
                    m_reverseSort = !m_reverseSort;
                SortList();
            }
            else if (resulttext == tr("Sort By Count"))
            {
                if (m_sortType != byCount)
                {
                    m_sortType = byCount;
                    m_reverseSort = false;
                }
                else
                {
                    m_reverseSort = !m_reverseSort;
                }
                SortList();
            }
            else if (resulttext == tr("Sort By Record Count"))
            {
                if (m_sortType != byRecCount)
                {
                    m_sortType = byRecCount;
                    m_reverseSort = false;
                }
                else
                    m_reverseSort = !m_reverseSort;
                SortList();
            }
            else if (resulttext == tr("Sort By Last Recorded"))
            {
                if (m_sortType != byLastRecord)
                {
                    m_sortType = byLastRecord;
                    m_reverseSort = false;
                }
                else
                    m_reverseSort = !m_reverseSort;
                SortList();
            }
            else if (resulttext == tr("Sort By Average Delay"))
            {
                if (m_sortType != byAvgDelay)
                {
                    m_sortType = byAvgDelay;
                    m_reverseSort = false;
                }
                else
                    m_reverseSort = !m_reverseSort;
                SortList();
            }
        }
        else if (resultid == "deleterule")
        {
            RecordingRule *record = qVariantValue<RecordingRule *>(dce->GetData());
            if (record)
            {
                if (buttonnum > 0)
                {
                    MythUIButtonListItem *item = m_programList->GetItemCurrent();
                    if (record->Delete() && item)
                        RemoveItemFromList(item);
                    else
                        VERBOSE(VB_IMPORTANT, "Failed to delete recording rule");
                }
                delete record;
            }
        }
        else
            ScheduleCommon::customEvent(event);
    }
}

void ProgramRecPriority::edit(MythUIButtonListItem *item)
{
    if (!item)
        return;

    ProgramRecPriorityInfo *pgRecInfo =
                        qVariantValue<ProgramRecPriorityInfo*>(item->GetData());

    if (!pgRecInfo)
        return;

    RecordingRule *record = new RecordingRule();
    record->m_recordID = pgRecInfo->GetRecordingRuleID();
    if (record->m_searchType == kNoSearch)
        record->LoadByProgram(pgRecInfo);

    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
    ScheduleEditor *schededit = new ScheduleEditor(mainStack, record);
    if (schededit->Create())
    {
        mainStack->AddScreen(schededit);
        connect(schededit, SIGNAL(ruleSaved(int)), SLOT(scheduleChanged(int)));
    }
    else
        delete schededit;

}

void ProgramRecPriority::scheduleChanged(int recid)
{
    // Assumes that the current item didn't change, which isn't guaranteed
    MythUIButtonListItem *item = m_programList->GetItemCurrent();
    ProgramRecPriorityInfo *pgRecInfo =
                        qVariantValue<ProgramRecPriorityInfo*>(item->GetData());

    if (!pgRecInfo)
        return;

    // We need to refetch the recording priority values since the Advanced
    // Recording Options page could've been used to change them

    // Only time the recording id would not match is if this wasn't the same
    // item we started editing earlier.
    if (recid != pgRecInfo->getRecordID())
        return;

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT recpriority, type, inactive "
                    "FROM record "
                    "WHERE recordid = :RECORDID");
    query.bindValue(":RECORDID", recid);
    if (!query.exec())
    {
        MythDB::DBError("Get new recording priority query", query);
    }
    else if (query.next())
    {
        int recPriority = query.value(0).toInt();
        int rectype = query.value(1).toInt();
        int inactive = query.value(2).toInt();

        int rtRecPriors[11];
        rtRecPriors[0] = 0;
        rtRecPriors[kSingleRecord] =
            gCoreContext->GetNumSetting("SingleRecordRecPriority", 1);
        rtRecPriors[kTimeslotRecord] =
            gCoreContext->GetNumSetting("TimeslotRecordRecPriority", 0);
        rtRecPriors[kChannelRecord] =
            gCoreContext->GetNumSetting("ChannelRecordRecPriority", 0);
        rtRecPriors[kAllRecord] =
            gCoreContext->GetNumSetting("AllRecordRecPriority", 0);
        rtRecPriors[kWeekslotRecord] =
            gCoreContext->GetNumSetting("WeekslotRecordRecPriority", 0);
        rtRecPriors[kFindOneRecord] =
            gCoreContext->GetNumSetting("FindOneRecordRecPriority", -1);
        rtRecPriors[kOverrideRecord] =
            gCoreContext->GetNumSetting("OverrideRecordRecPriority", 0);
        rtRecPriors[kDontRecord] =
            gCoreContext->GetNumSetting("OverrideRecordRecPriority", 0);
        rtRecPriors[kFindDailyRecord] =
            gCoreContext->GetNumSetting("FindOneRecordRecPriority", -1);
        rtRecPriors[kFindWeeklyRecord] =
            gCoreContext->GetNumSetting("FindOneRecordRecPriority", -1);

        // set the recording priorities of that program
        pgRecInfo->SetRecordingPriority(recPriority);
        pgRecInfo->recType = (RecordingType)rectype;
        pgRecInfo->recTypeRecPriority = rtRecPriors[pgRecInfo->recType];
        // also set the m_origRecPriorityData with new recording
        // priority so we don't save to db again when we exit
        m_origRecPriorityData[pgRecInfo->GetRecordingRuleID()] =
            pgRecInfo->GetRecordingPriority();
        // also set the active/inactive state
        pgRecInfo->recstatus = inactive ? rsInactive : rsUnknown;

        SortList();
    }
    else
    {
        RemoveItemFromList(item);
    }

    countMatches();
}

void ProgramRecPriority::customEdit(void)
{
    MythUIButtonListItem *item = m_programList->GetItemCurrent();
    if (!item)
        return;

    ProgramRecPriorityInfo *pgRecInfo =
                        qVariantValue<ProgramRecPriorityInfo*>(item->GetData());

    EditCustom(pgRecInfo);
}

void ProgramRecPriority::remove(void)
{
    MythUIButtonListItem *item = m_programList->GetItemCurrent();
    if (!item)
        return;

    ProgramRecPriorityInfo *pgRecInfo =
                        qVariantValue<ProgramRecPriorityInfo*>(item->GetData());

    if (!pgRecInfo)
        return;

    RecordingRule *record = new RecordingRule();
    record->m_recordID = pgRecInfo->GetRecordingRuleID();
    if (!record->Load())
    {
        delete record;
        return;
    }

    QString message = tr("Delete '%1' %2 rule?").arg(record->m_title)
        .arg(toString(pgRecInfo->GetRecordingRuleType()));

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

void ProgramRecPriority::deactivate(void)
{
    MythUIButtonListItem *item = m_programList->GetItemCurrent();
    if (!item)
        return;

    ProgramRecPriorityInfo *pgRecInfo =
                        qVariantValue<ProgramRecPriorityInfo*>(item->GetData());

    if (pgRecInfo)
    {
        MSqlQuery query(MSqlQuery::InitCon());

        query.prepare("SELECT inactive "
                      "FROM record "
                      "WHERE recordid = :RECID");
        query.bindValue(":RECID", pgRecInfo->GetRecordingRuleID());


        if (!query.exec())
        {
            MythDB::DBError("ProgramRecPriority::deactivate()", query);
        }
        else if (query.next())
        {
            int inactive = query.value(0).toInt();
            if (inactive)
                inactive = 0;
            else
                inactive = 1;

            query.prepare("UPDATE record "
                          "SET inactive = :INACTIVE "
                          "WHERE recordid = :RECID");
            query.bindValue(":INACTIVE", inactive);
            query.bindValue(":RECID", pgRecInfo->GetRecordingRuleID());

            if (!query.exec())
            {
                MythDB::DBError(
                    "Update recording schedule inactive query", query);
            }
            else
            {
                ScheduledRecording::signalChange(0);
                pgRecInfo->recstatus = inactive ? rsInactive : rsUnknown;
                item->DisplayState("disabled", "status");
            }
        }
    }
}

void ProgramRecPriority::upcoming(void)
{
    MythUIButtonListItem *item = m_programList->GetItemCurrent();
    if (!item)
        return;

    ProgramRecPriorityInfo *pgRecInfo =
                        qVariantValue<ProgramRecPriorityInfo*>(item->GetData());

    if (!pgRecInfo)
        return;

    if (m_listMatch[pgRecInfo->GetRecordingRuleID()] > 0)
    {
        MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
        ProgLister *pl = new ProgLister(
            mainStack, plRecordid,
            QString::number(pgRecInfo->GetRecordingRuleID()), "");

        if (pl->Create())
            mainStack->AddScreen(pl);
        else
            delete pl;
    }
    else
    {
        ProgLister *pl = NULL;
        QString trimTitle = pgRecInfo->title;
        trimTitle.remove(QRegExp(" \\(.*\\)$"));
        MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
        pl = new ProgLister(mainStack, plTitle, trimTitle, "");
        if (pl->Create())
            mainStack->AddScreen(pl);
        else
            delete pl;
    }
}

void ProgramRecPriority::details(void)
{
    MythUIButtonListItem *item = m_programList->GetItemCurrent();
    if (!item)
        return;

    ProgramRecPriorityInfo *pgRecInfo = qVariantValue<ProgramRecPriorityInfo *>
                                                            (item->GetData());

    ShowDetails(pgRecInfo);
}

void ProgramRecPriority::changeRecPriority(int howMuch)
{
    MythUIButtonListItem *item = m_programList->GetItemCurrent();
    if (!item)
        return;

    ProgramRecPriorityInfo *pgRecInfo =
                        qVariantValue<ProgramRecPriorityInfo*>(item->GetData());

    if (!pgRecInfo)
        return;

    int tempRecPriority;
    // inc/dec recording priority
    tempRecPriority = pgRecInfo->GetRecordingPriority() + howMuch;
    if (tempRecPriority > -100 && tempRecPriority < 100)
    {
        pgRecInfo->recpriority = tempRecPriority;

        // order may change if sorting by recording priority, so resort
        if (m_sortType == byRecPriority)
            SortList();
        else
        {
            // No need to re-fill the entire list, just update this entry
            int progRecPriority = pgRecInfo->GetRecordingPriority();
            int autorecpri = pgRecInfo->autoRecPriority;
            int finalRecPriority = progRecPriority +
                                    autorecpri +
                                    pgRecInfo->recTypeRecPriority;

            item->SetText(QString::number(progRecPriority), "progpriority");
            item->SetText(QString::number(finalRecPriority), "finalpriority");

            if (m_recPriorityText)
            {
                QString msg = QString::number(progRecPriority);

                if(autorecpri != 0)
                    msg += tr(" + %1 automatic priority (%2hr)")
                                .arg(autorecpri).arg(pgRecInfo->avg_delay);
                m_recPriorityText->SetText(msg);
            }

            if (m_recPriorityBText)
                m_recPriorityBText->SetText(QString::number(progRecPriority +
                                                            autorecpri));

            if (m_finalPriorityText)
                m_finalPriorityText->SetText(QString::number(finalRecPriority));
        }
    }
}

void ProgramRecPriority::saveRecPriority(void)
{
    QMap<QString, ProgramRecPriorityInfo>::Iterator it;

    for (it = m_programData.begin(); it != m_programData.end(); ++it)
    {
        ProgramRecPriorityInfo *progInfo = &(*it);
        int key = progInfo->GetRecordingRuleID();

        // if this program's recording priority changed from when we entered
        // save new value out to db
        if (progInfo->GetRecordingPriority() != m_origRecPriorityData[key])
            progInfo->ApplyRecordRecPriorityChange(
                progInfo->GetRecordingPriority());
    }
}

void ProgramRecPriority::FillList(void)
{
    int cnt = 999, rtRecPriors[11];
    vector<ProgramInfo *> recordinglist;

    int autopriority = gCoreContext->GetNumSetting("AutoRecPriority", 0);

    m_programData.clear();
    m_sortedProgram.clear();

    RemoteGetAllScheduledRecordings(recordinglist);

    vector<ProgramInfo *>::reverse_iterator pgiter = recordinglist.rbegin();

    for (; pgiter != recordinglist.rend(); pgiter++)
    {
        m_programData[QString::number(cnt)] = *(*pgiter);

        // save recording priority value in map so we don't have to
        // save all program's recording priority values when we exit
        m_origRecPriorityData[(*pgiter)->GetRecordingRuleID()] =
            (*pgiter)->GetRecordingPriority();

        delete (*pgiter);
        cnt--;
    }

    // get all the recording type recording priority values
    rtRecPriors[0] = 0;
    rtRecPriors[kSingleRecord] =
        gCoreContext->GetNumSetting("SingleRecordRecPriority", 1);
    rtRecPriors[kTimeslotRecord] =
        gCoreContext->GetNumSetting("TimeslotRecordRecPriority", 0);
    rtRecPriors[kChannelRecord] =
        gCoreContext->GetNumSetting("ChannelRecordRecPriority", 0);
    rtRecPriors[kAllRecord] =
        gCoreContext->GetNumSetting("AllRecordRecPriority", 0);
    rtRecPriors[kWeekslotRecord] =
        gCoreContext->GetNumSetting("WeekslotRecordRecPriority", 0);
    rtRecPriors[kFindOneRecord] =
        gCoreContext->GetNumSetting("FindOneRecordRecPriority", -1);
    rtRecPriors[kOverrideRecord] =
        gCoreContext->GetNumSetting("OverrideRecordRecPriority", 0);
    rtRecPriors[kDontRecord] =
        gCoreContext->GetNumSetting("OverrideRecordRecPriority", 0);
    rtRecPriors[kFindDailyRecord] =
        gCoreContext->GetNumSetting("FindOneRecordRecPriority", -1);
    rtRecPriors[kFindWeeklyRecord] =
        gCoreContext->GetNumSetting("FindOneRecordRecPriority", -1);

    // get recording types associated with each program from db
    // (hope this is ok to do here, it's so much lighter doing
    // it all at once than once per program)

    MSqlQuery result(MSqlQuery::InitCon());
    result.prepare("SELECT recordid, title, chanid, starttime, startdate, "
                   "type, inactive, last_record, avg_delay, profile "
                   "FROM record;");

    if (!result.exec())
    {
        MythDB::DBError("Get program recording priorities query", result);
    }
    else if (result.next())
    {
        countMatches();
        do {
            uint recordid = result.value(0).toUInt();
            QString title = result.value(1).toString();
            QString chanid = result.value(2).toString();
            QString tempTime = result.value(3).toString();
            QString tempDate = result.value(4).toString();
            RecordingType recType = (RecordingType)result.value(5).toInt();
            int recTypeRecPriority = rtRecPriors[recType];
            int inactive = result.value(6).toInt();
            QDateTime lastrec = result.value(7).toDateTime();
            int avgd = result.value(8).toInt();
            QString profile = result.value(9).toString();

            // find matching program in m_programData and set
            // recTypeRecPriority and recType
            QMap<QString, ProgramRecPriorityInfo>::Iterator it;
            for (it = m_programData.begin(); it != m_programData.end(); ++it)
            {
                ProgramRecPriorityInfo *progInfo = &(*it);

                if (progInfo->GetRecordingRuleID() == recordid)
                {
                    progInfo->sortTitle = progInfo->title;
                    progInfo->sortTitle.remove(QRegExp(tr("^(The |A |An )")));

                    progInfo->recTypeRecPriority = recTypeRecPriority;
                    progInfo->recType = recType;
                    progInfo->matchCount =
                        m_listMatch[progInfo->GetRecordingRuleID()];
                    progInfo->recCount =
                        m_recMatch[progInfo->GetRecordingRuleID()];
                    progInfo->last_record = lastrec;
                    progInfo->avg_delay = avgd;
                    progInfo->profile = profile;

                    if (autopriority)
                        progInfo->autoRecPriority =
                            autopriority - (progInfo->avg_delay *
                            (autopriority * 2 + 1) / 200);
                    else
                        progInfo->autoRecPriority = 0;

                    if (inactive)
                        progInfo->recstatus = rsInactive;
                    else if (m_conMatch[progInfo->GetRecordingRuleID()] > 0)
                        progInfo->recstatus = rsConflict;
                    else if (m_nowMatch[progInfo->GetRecordingRuleID()] > 0)
                        progInfo->recstatus = rsRecording;
                    else if (m_recMatch[progInfo->GetRecordingRuleID()] > 0)
                        progInfo->recstatus = rsWillRecord;
                    else
                        progInfo->recstatus = rsUnknown;

                    break;
                }
            }
        } while (result.next());
    }
}

void ProgramRecPriority::countMatches()
{
    m_listMatch.clear();
    m_conMatch.clear();
    m_nowMatch.clear();
    m_recMatch.clear();
    ProgramList schedList;
    LoadFromScheduler(schedList);
    QDateTime now = QDateTime::currentDateTime();

    ProgramList::const_iterator it = schedList.begin();
    for (; it != schedList.end(); ++it)
    {
        const RecStatusType recstatus = (**it).GetRecordingStatus();
        const uint          recordid  = (**it).GetRecordingRuleID();
        if ((**it).GetRecordingEndTime() > now && recstatus != rsNotListed)
        {
            m_listMatch[recordid]++;
            if (recstatus == rsConflict || recstatus == rsOffLine)
                m_conMatch[recordid]++;
            else if (recstatus == rsWillRecord)
                m_recMatch[recordid]++;
            else if (recstatus == rsRecording)
            {
                m_nowMatch[recordid]++;
                m_recMatch[recordid]++;
            }
        }
    }
}

void ProgramRecPriority::SortList()
{
    MythUIButtonListItem *item = m_programList->GetItemCurrent();

    if (item)
        m_currentItem = qVariantValue<ProgramRecPriorityInfo*>(item->GetData());

    int i, j;
    vector<RecPriorityInfo> sortingList;
    QMap<QString, ProgramRecPriorityInfo>::Iterator pit;
    vector<RecPriorityInfo>::iterator sit;
    RecPriorityInfo *recPriorityInfo;

    // copy m_programData into sortingList and make a copy
    // of m_programData in pdCopy
    for (i = 0, pit = m_programData.begin(); pit != m_programData.end();
         ++pit, ++i)
    {
        ProgramRecPriorityInfo *progInfo = &(*pit);
        RecPriorityInfo tmp = {progInfo, i};
        sortingList.push_back(tmp);
    }

    // sort sortingList
    switch(m_sortType)
    {
        case byTitle :
                 if (m_reverseSort)
                     sort(sortingList.begin(), sortingList.end(),
                          titleSort(true));
                 else
                     sort(sortingList.begin(), sortingList.end(), titleSort());
                 break;
        case byRecPriority :
                 if (m_reverseSort)
                     sort(sortingList.begin(), sortingList.end(),
                          programRecPrioritySort(true));
                 else
                     sort(sortingList.begin(), sortingList.end(),
                          programRecPrioritySort());
                 break;
        case byRecType :
                 if (m_reverseSort)
                     sort(sortingList.begin(), sortingList.end(),
                          programRecTypeSort(true));
                 else
                     sort(sortingList.begin(), sortingList.end(),
                          programRecTypeSort());
                 break;
        case byCount :
                 if (m_reverseSort)
                     sort(sortingList.begin(), sortingList.end(),
                          programCountSort(true));
                 else
                     sort(sortingList.begin(), sortingList.end(),
                          programCountSort());
                 break;
        case byRecCount :
                 if (m_reverseSort)
                     sort(sortingList.begin(), sortingList.end(),
                          programRecCountSort(true));
                 else
                     sort(sortingList.begin(), sortingList.end(),
                          programRecCountSort());
                 break;
        case byLastRecord :
                 if (m_reverseSort)
                     sort(sortingList.begin(), sortingList.end(),
                          programLastRecordSort(true));
                 else
                     sort(sortingList.begin(), sortingList.end(),
                          programLastRecordSort());
                 break;
        case byAvgDelay :
                 if (m_reverseSort)
                     sort(sortingList.begin(), sortingList.end(),
                          programAvgDelaySort(true));
                 else
                     sort(sortingList.begin(), sortingList.end(),
                          programAvgDelaySort());
                 break;
    }

    m_sortedProgram.clear();

    // rebuild m_channelData in sortingList order from m_sortedProgram
    for (i = 0, sit = sortingList.begin(); sit != sortingList.end(); ++i, ++sit)
    {
        recPriorityInfo = &(*sit);

        // find recPriorityInfo[i] in m_sortedChannel
        for (j = 0, pit = m_programData.begin(); j !=recPriorityInfo->cnt; j++, ++pit);

        m_sortedProgram[QString::number(999-i)] = &(*pit);
    }

    UpdateList();
}

void ProgramRecPriority::UpdateList()
{
    if (!m_currentItem && !m_programList->IsEmpty())
        m_currentItem = qVariantValue<ProgramRecPriorityInfo*>
                                (m_programList->GetItemCurrent()->GetData());

    m_programList->Reset();

    QMap<QString, ProgramRecPriorityInfo*>::Iterator it;
    MythUIButtonListItem *item;
    for (it = m_sortedProgram.begin(); it != m_sortedProgram.end(); ++it)
    {
        ProgramRecPriorityInfo *progInfo = *it;

        item = new MythUIButtonListItem(m_programList, "",
                                                qVariantFromValue(progInfo));

        int progRecPriority = progInfo->GetRecordingPriority();
        int finalRecPriority = progRecPriority +
                                progInfo->autoRecPriority +
                                progInfo->recTypeRecPriority;

        if ((progInfo->rectype == kSingleRecord ||
                progInfo->rectype == kOverrideRecord ||
                progInfo->rectype == kDontRecord) &&
            !(progInfo->GetSubtitle()).trimmed().isEmpty())
        {
            QString rating = QString::number(progInfo->GetStars(10));

            item->DisplayState(rating, "ratingstate");
        }
        else
            progInfo->subtitle.clear();

        QString state;
        if (progInfo->recType == kDontRecord ||
            progInfo->recstatus == rsInactive)
            state = "disabled";
        else if (m_conMatch[progInfo->GetRecordingRuleID()] > 0)
            state = "error";
        else if (m_recMatch[progInfo->GetRecordingRuleID()] > 0)
            state = "normal";
        else if (m_nowMatch[progInfo->GetRecordingRuleID()] > 0)
            state = "running";

        InfoMap infoMap;
        progInfo->ToMap(infoMap);
        item->SetTextFromMap(infoMap, state);

        item->SetText(progInfo->description, "description", state);
        item->SetText(progInfo->category, "category", state);
        item->SetText(QString::number(progRecPriority), "progpriority", state);
        item->SetText(QString::number(finalRecPriority), "finalpriority", state);

        QString recgroup = progInfo->recgroup;
        if (recgroup == "Default")
            recgroup = tr("Default");
        item->SetText(recgroup, "recordinggroup", state);
        QString storagegroup = progInfo->storagegroup;
        if (storagegroup == "Default")
            storagegroup = tr("Default");
        else if (StorageGroup::kSpecialGroups.contains(storagegroup))
            storagegroup = tr(storagegroup.toLatin1().constData());
        item->SetText(storagegroup, "storagegroup", state);

        QString tempDateTime = (progInfo->last_record).toString(m_formatLongDate + ' ' + m_formatTime);
        item->SetText(tempDateTime, "lastrecorded", state);
        QString tempDate = (progInfo->last_record).toString(m_formatShortDate);
        item->SetText(tempDate, "lastrecordeddate", state);
        QString tempTime = (progInfo->last_record).toString(m_formatTime);
        item->SetText(tempTime, "lastrecordedtime", state);

        QString channame = progInfo->channame;
        if ((progInfo->recType == kAllRecord) ||
            (progInfo->recType == kFindOneRecord) ||
            (progInfo->recType == kFindDailyRecord) ||
            (progInfo->recType == kFindWeeklyRecord))
            channame = tr("Any");
        item->SetText(channame, "channel", state);
        QString channum = progInfo->chanstr;
        if ((progInfo->recType == kAllRecord) ||
            (progInfo->recType == kFindOneRecord) ||
            (progInfo->recType == kFindDailyRecord) ||
            (progInfo->recType == kFindWeeklyRecord))
            channum = tr("Any");
        item->SetText(channum, "channum", state);
        QString callsign = progInfo->chansign;
        if ((progInfo->recType == kAllRecord) ||
            (progInfo->recType == kFindOneRecord) ||
            (progInfo->recType == kFindDailyRecord) ||
            (progInfo->recType == kFindWeeklyRecord))
            callsign = tr("Any");
        item->SetText(callsign, "callsign", state);

        QString profile = progInfo->profile;
        if ((profile == "Default") || (profile == "Live TV") ||
            (profile == "High Quality") || (profile == "Low Quality"))
            profile = tr(profile.toLatin1().constData());
        item->SetText(profile, "recordingprofile", state);
        item->DisplayState(state, "status");

        if (m_currentItem == progInfo)
            m_programList->SetItemCurrent(item);
    }

    m_currentItem = NULL;

    MythUIText *norecordingText = dynamic_cast<MythUIText*>
                                                (GetChild("norecordings_info"));

    if (norecordingText)
        norecordingText->SetVisible(m_programData.isEmpty());
}

void ProgramRecPriority::updateInfo(MythUIButtonListItem *item)
{
    if (!item)
        return;

    ProgramRecPriorityInfo *pgRecInfo = qVariantValue<ProgramRecPriorityInfo *>
                                                            (item->GetData());

    if (!pgRecInfo)
        return;

    int progRecPriority, autorecpri, rectyperecpriority, finalRecPriority;

    progRecPriority = pgRecInfo->GetRecordingPriority();
    autorecpri = pgRecInfo->autoRecPriority;
    rectyperecpriority = pgRecInfo->recTypeRecPriority;
    finalRecPriority = progRecPriority + autorecpri + rectyperecpriority;

    QString subtitle;
    if (pgRecInfo->subtitle != "(null)" &&
        (pgRecInfo->rectype == kSingleRecord ||
            pgRecInfo->rectype == kOverrideRecord ||
            pgRecInfo->rectype == kDontRecord))
    {
        subtitle = pgRecInfo->subtitle;
    }

    QString matchInfo;
    if (pgRecInfo->GetRecordingStatus() == rsInactive)
    {
        matchInfo = QString("%1 %2")
            .arg(m_listMatch[pgRecInfo->GetRecordingRuleID()])
            .arg(toString(pgRecInfo->GetRecordingStatus(),
                          pgRecInfo->GetRecordingRuleType()));
    }
    else
        matchInfo = QString(tr("Recording %1 of %2"))
            .arg(m_recMatch[pgRecInfo->GetRecordingRuleID()])
            .arg(m_listMatch[pgRecInfo->GetRecordingRuleID()]);

    subtitle = QString("(%1) %2").arg(matchInfo).arg(subtitle);

    InfoMap infoMap;
    pgRecInfo->ToMap(infoMap);
    SetTextFromMap(infoMap);

    if (m_descriptionText)
        m_descriptionText->SetText(pgRecInfo->description);

    if (m_categoryText)
        m_categoryText->SetText(pgRecInfo->category);

    if (m_schedInfoText)
        m_schedInfoText->SetText(subtitle);

    if (m_rectypePriorityText)
        m_rectypePriorityText->SetText(QString::number(rectyperecpriority));

    if (m_recPriorityText)
    {
        QString msg = QString::number(pgRecInfo->GetRecordingPriority());

        if(autorecpri != 0)
            msg += tr(" + %1 automatic priority (%2hr)")
                        .arg(autorecpri).arg(pgRecInfo->avg_delay);
        m_recPriorityText->SetText(msg);
    }

    if (m_recPriorityBText)
        m_recPriorityBText->SetText(QString::number(progRecPriority +
                                                    autorecpri));

    if (m_finalPriorityText)
        m_finalPriorityText->SetText(QString::number(finalRecPriority));

    if (m_recGroupText)
    {
        QString recgroup = pgRecInfo->recgroup;
        if (recgroup == "Default")
            recgroup = tr("Default");
        m_recGroupText->SetText(recgroup);
    }

    if (m_storageGroupText)
    {
        QString storagegroup = pgRecInfo->storagegroup;
        if (storagegroup == "Default")
                storagegroup = tr("Default");
        else if (StorageGroup::kSpecialGroups.contains(storagegroup))
            storagegroup = tr(storagegroup.toLatin1().constData());
        m_storageGroupText->SetText(storagegroup);
    }

    if (m_lastRecordedText)
    {
        QString tempDateTime = (pgRecInfo->last_record).toString(m_formatLongDate + ' ' + m_formatTime);
        m_lastRecordedText->SetText(tempDateTime);
    }

    if (m_lastRecordedDateText)
    {
        QString tempDate = (pgRecInfo->last_record).toString(m_formatShortDate);
        m_lastRecordedDateText->SetText(tempDate);
    }

    if (m_lastRecordedTimeText)
    {
        QString tempTime = (pgRecInfo->last_record).toString(m_formatTime);
        m_lastRecordedTimeText->SetText(tempTime);
    }

    if (m_channameText)
    {
        QString channame = pgRecInfo->channame;
        if ((pgRecInfo->rectype == kAllRecord) ||
            (pgRecInfo->rectype == kFindOneRecord) ||
            (pgRecInfo->rectype == kFindDailyRecord) ||
            (pgRecInfo->rectype == kFindWeeklyRecord))
            channame = tr("Any");
        m_channameText->SetText(channame);
    }

    if (m_channumText)
    {
        QString channum = pgRecInfo->chanstr;
        if ((pgRecInfo->rectype == kAllRecord) ||
            (pgRecInfo->rectype == kFindOneRecord) ||
            (pgRecInfo->rectype == kFindDailyRecord) ||
            (pgRecInfo->rectype == kFindWeeklyRecord))
            channum = tr("Any");
        m_channumText->SetText(channum);
    }

    if (m_callsignText)
    {
        QString callsign = pgRecInfo->chansign;
        if ((pgRecInfo->rectype == kAllRecord) ||
            (pgRecInfo->rectype == kFindOneRecord) ||
            (pgRecInfo->rectype == kFindDailyRecord) ||
            (pgRecInfo->rectype == kFindWeeklyRecord))
            callsign = tr("Any");
        m_callsignText->SetText(callsign);
    }

    if (m_recProfileText)
    {
        QString profile = pgRecInfo->profile;
        if ((profile == "Default") || (profile == "Live TV") ||
            (profile == "High Quality") || (profile == "Low Quality"))
            profile = tr(profile.toLatin1().constData());
        m_recProfileText->SetText(profile);
    }

}

void ProgramRecPriority::RemoveItemFromList(MythUIButtonListItem *item)
{
    if (!item)
        return;

    ProgramRecPriorityInfo *pgRecInfo = qVariantValue<ProgramRecPriorityInfo *>
                                                            (item->GetData());

    if (!pgRecInfo)
        return;

    QMap<QString, ProgramRecPriorityInfo>::iterator it;
    for (it = m_programData.begin(); it != m_programData.end(); ++it)
    {
        ProgramRecPriorityInfo *value = &(it.value());
        if (value == pgRecInfo)
        {
            m_programData.erase(it);
            break;
        }
    }
    m_programList->RemoveItem(item);
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
