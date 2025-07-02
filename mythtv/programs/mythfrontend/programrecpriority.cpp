// C/C++ headers
#include <vector> // For std::vector

// QT headers
#include <QDateTime>

// MythTV headers
#include "libmythbase/mythdb.h"
#include "libmythbase/mythcorecontext.h"
#include "libmythbase/mythlogging.h"
#include "libmythbase/remoteutil.h"
#include "libmythbase/stringutil.h"
#include "libmythbase/ternarycompare.h"
#include "libmythtv/recordingrule.h"
#include "libmythtv/scheduledrecording.h"
#include "libmythui/mythdialogbox.h"
#include "libmythui/mythuibuttonlist.h"
#include "libmythui/mythuihelper.h"
#include "libmythui/mythuistatetype.h"
#include "libmythui/mythuitext.h"

// MythFrontend
#include "proglist.h"
#include "programrecpriority.h"
#include "scheduleeditor.h"

void ProgramRecPriorityInfo::clone(
    const ProgramRecPriorityInfo &other, bool ignore_non_serialized_data)
{
    RecordingInfo::clone(other, ignore_non_serialized_data);

    if (!ignore_non_serialized_data)
    {
        m_recType          = other.m_recType;
        m_matchCount       = other.m_matchCount;
        m_recCount         = other.m_recCount;
        m_last_record      = other.m_last_record;
        m_avg_delay        = other.m_avg_delay;
        m_profile          = other.m_profile;
        m_storageGroup     = other.m_storageGroup;
        m_recordingGroup   = other.m_recordingGroup;
    }
}

void ProgramRecPriorityInfo::clone(
    const RecordingInfo &other, bool ignore_non_serialized_data)
{
    RecordingInfo::clone(other, ignore_non_serialized_data);

    if (!ignore_non_serialized_data)
    {
        m_recType          = kNotRecording;
        m_matchCount       = 0;
        m_recCount         = 0;
        m_last_record      = QDateTime();
        m_avg_delay        = 0;
        m_profile.clear();
        m_storageGroup.clear();
        m_recordingGroup.clear();
    }
}

void ProgramRecPriorityInfo::clone(
    const ProgramInfo &other, bool ignore_non_serialized_data)
{
    RecordingInfo::clone(other, ignore_non_serialized_data);

    if (!ignore_non_serialized_data)
    {
        m_recType          = kNotRecording;
        m_matchCount       = 0;
        m_recCount         = 0;
        m_last_record      = QDateTime();
        m_avg_delay        = 0;
        m_profile.clear();
        m_storageGroup.clear();
        m_recordingGroup.clear();
    }
}

void ProgramRecPriorityInfo::clear(void)
{
    RecordingInfo::clear();

    m_recType          = kNotRecording;
    m_matchCount       = 0;
    m_recCount         = 0;
    m_last_record      = QDateTime();
    m_avg_delay        = 0;
    m_profile.clear();
    m_storageGroup.clear();
    m_recordingGroup.clear();
}

void ProgramRecPriorityInfo::ToMap(InfoMap &progMap,
                                   bool showrerecord, uint star_range,
                                   uint date_format) const
{
    RecordingInfo::ToMap(progMap, showrerecord, star_range, date_format);
    progMap["title"] = (m_title == "Default (Template)") ?
        QObject::tr("Default (Template)") : m_title;
    progMap["category"] = (m_category == "Default") ?
        QObject::tr("Default") : m_category;
    progMap["storagegroup"] = (m_storageGroup == "Default") ?
        QObject::tr("Default") : m_storageGroup;
    progMap["recordinggroup"] = (m_recordingGroup == "Default") ?
        QObject::tr("Default") : m_recordingGroup;
}

class TitleSort
{
  public:
    explicit TitleSort(bool reverse) : m_reverse(reverse) {}

    bool operator()(const ProgramRecPriorityInfo *a,
                    const ProgramRecPriorityInfo *b) const
    {
        int cmp = StringUtil::naturalCompare(a->GetSortTitle(), b->GetSortTitle());
        if (cmp == 0)
            cmp = StringUtil::naturalCompare(a->GetSortSubtitle(), b->GetSortSubtitle());
        // sort higher recording priority before
        if (cmp == 0)
            cmp = ternary_compare(b->GetRecordingPriority(), a->GetRecordingPriority());
        // sort lower RecordingTypePrecedence before
        if (cmp == 0)
            cmp = ternary_compare(RecTypePrecedence(a->m_recType), RecTypePrecedence(b->m_recType));
        // sort lower RecordingRuleID before
        if (cmp == 0)
            cmp = ternary_compare(a->GetRecordingRuleID(), b->GetRecordingRuleID());

        return m_reverse ? cmp > 0 : cmp < 0;
    }

  private:
    bool m_reverse {false};
};

class ProgramRecPrioritySort
{
  public:
    explicit ProgramRecPrioritySort(bool reverse) : m_reverse(reverse) {}

    bool operator()(const ProgramRecPriorityInfo *a,
                    const ProgramRecPriorityInfo *b) const
    {
        // sort higher recording priority before
        int cmp = ternary_compare(b->GetRecordingPriority(), a->GetRecordingPriority());
        // sort lower RecTypePrecedence before
        if (cmp == 0)
            cmp = ternary_compare(RecTypePrecedence(a->m_recType), RecTypePrecedence(b->m_recType));
        // sort lower RecordingRuleID before
        if (cmp == 0)
            cmp = ternary_compare(a->GetRecordingRuleID(), b->GetRecordingRuleID());

        return m_reverse ? cmp > 0 : cmp < 0;
    }

  private:
    bool m_reverse {false};
};

class ProgramRecTypeSort
{
  public:
    explicit ProgramRecTypeSort(bool reverse) : m_reverse(reverse) {}

    bool operator()(const ProgramRecPriorityInfo *a,
                    const ProgramRecPriorityInfo *b) const
    {
        // sort lower RecTypePrecedence before
        int cmp = ternary_compare(RecTypePrecedence(a->m_recType), RecTypePrecedence(b->m_recType));
        // sort higher recording priority before
        if (cmp == 0)
            cmp = ternary_compare(b->GetRecordingPriority(), a->GetRecordingPriority());;
        // sort lower RecordingRuleID before
        if (cmp == 0)
            cmp = ternary_compare(a->GetRecordingRuleID(), b->GetRecordingRuleID());

        return m_reverse ? cmp > 0 : cmp < 0;
    }

  private:
    bool m_reverse {false};
};

class ProgramCountSort
{
  public:
    explicit ProgramCountSort(bool reverse) : m_reverse(reverse) {}

    bool operator()(const ProgramRecPriorityInfo *a,
                    const ProgramRecPriorityInfo *b) const
    {
        // sort higher match count before
        int cmp = ternary_compare(b->m_matchCount, a->m_matchCount);
        // sort higher recCount before
        if (cmp == 0)
            cmp = ternary_compare(b->m_recCount, a->m_recCount);
        if (cmp == 0)
            cmp = StringUtil::naturalCompare(a->GetSortTitle(), b->GetSortTitle());
        if (cmp == 0)
            cmp = StringUtil::naturalCompare(a->GetSortSubtitle(), b->GetSortSubtitle());

        return m_reverse ? cmp > 0 : cmp < 0;
    }

  private:
    bool m_reverse {false};
};

class ProgramRecCountSort
{
  public:
    explicit ProgramRecCountSort(bool reverse) : m_reverse(reverse) {}

    bool operator()(const ProgramRecPriorityInfo *a,
                    const ProgramRecPriorityInfo *b) const
    {
        // sort higher recCount before
        int cmp = ternary_compare(b->m_recCount, a->m_recCount);
        // sort higher match count before
        if (cmp == 0)
            cmp = ternary_compare(b->m_matchCount, a->m_matchCount);
        if (cmp == 0)
            cmp = StringUtil::naturalCompare(a->GetSortTitle(), b->GetSortTitle());
        if (cmp == 0)
            cmp = StringUtil::naturalCompare(a->GetSortSubtitle(), b->GetSortSubtitle());

        return m_reverse ? cmp > 0 : cmp < 0;
    }

  private:
    bool m_reverse {false};
};

class ProgramLastRecordSort
{
  public:
    explicit ProgramLastRecordSort(bool reverse) : m_reverse(reverse) {}

    bool operator()(const ProgramRecPriorityInfo *a,
                    const ProgramRecPriorityInfo *b) const
    {
        // sort later date time before
        int cmp = ternary_compare(b->m_last_record, a->m_last_record);
        if (cmp == 0)
            cmp = StringUtil::naturalCompare(a->GetSortTitle(), b->GetSortTitle());
        if (cmp == 0)
            cmp = StringUtil::naturalCompare(a->GetSortSubtitle(), b->GetSortSubtitle());

        return m_reverse ? cmp > 0 : cmp < 0;
    }

  private:
    bool m_reverse {false};
};

class ProgramAvgDelaySort
{
  public:
    explicit ProgramAvgDelaySort(bool reverse) : m_reverse(reverse) {}

    bool operator()(const ProgramRecPriorityInfo *a,
                    const ProgramRecPriorityInfo *b) const
    {
        int cmp = ternary_compare(a->m_avg_delay, b->m_avg_delay);
        if (cmp == 0)
            cmp = StringUtil::naturalCompare(a->GetSortTitle(), b->GetSortTitle());
        if (cmp == 0)
            cmp = StringUtil::naturalCompare(a->GetSortSubtitle(), b->GetSortSubtitle());

        return m_reverse ? cmp > 0 : cmp < 0;
    }

  private:
    bool m_reverse {false};
};

////////////////////////////////////////////////////////

ProgramRecPriority::ProgramRecPriority(MythScreenStack *parent,
                                       const QString &name)
                   : ScheduleCommon(parent, name)
{
    m_sortType = (SortType)gCoreContext->GetNumSetting("ProgramRecPrioritySorting",
                                                 (int)byTitle);
    m_reverseSort = gCoreContext->GetBoolSetting("ProgramRecPriorityReverse", false);
}

bool ProgramRecPriority::Create()
{
    if (!LoadWindowFromXML("schedule-ui.xml", "managerecrules", this))
        return false;

    m_programList = dynamic_cast<MythUIButtonList *> (GetChild("programs"));

    m_schedInfoText = dynamic_cast<MythUIText *> (GetChild("scheduleinfo"));
    m_recPriorityText = dynamic_cast<MythUIText *> (GetChild("recpriority"));
    m_recPriorityBText = dynamic_cast<MythUIText *> (GetChild("recpriorityB"));
    m_finalPriorityText = dynamic_cast<MythUIText *> (GetChild("finalpriority"));
    m_lastRecordedText = dynamic_cast<MythUIText *> (GetChild("lastrecorded"));
    m_lastRecordedDateText = dynamic_cast<MythUIText *> (GetChild("lastrecordeddate"));
    m_lastRecordedTimeText = dynamic_cast<MythUIText *> (GetChild("lastrecordedtime"));
    m_chanNameText = dynamic_cast<MythUIText *> (GetChild("channel"));
    m_chanNumText = dynamic_cast<MythUIText *> (GetChild("channum"));
    m_callSignText = dynamic_cast<MythUIText *> (GetChild("callsign"));
    m_recProfileText = dynamic_cast<MythUIText *> (GetChild("recordingprofile"));

    if (!m_programList)
    {
        LOG(VB_GENERAL, LOG_ERR, "Theme is missing critical theme elements.");
        return false;
    }

    connect(m_programList, &MythUIButtonList::itemSelected,
            this, &ProgramRecPriority::updateInfo);
    connect(m_programList, &MythUIButtonList::itemClicked,
            this, &ProgramRecPriority::edit);

    m_programList->SetLCDTitles(tr("Schedule Priorities"),
                          "rec_type|titlesubtitle|progpriority");
    m_programList->SetSearchFields("titlesubtitle");

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

    QStringList actions;
    bool handled = GetMythMainWindow()->TranslateKeyPress("TV Frontend", event, actions);

    for (int i = 0; i < actions.size() && !handled; i++)
    {
        const QString& action = actions[i];
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
            {
                m_reverseSort = !m_reverseSort;
            }
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
            {
                m_reverseSort = !m_reverseSort;
            }
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
            {
                m_reverseSort = !m_reverseSort;
            }
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
            {
                m_reverseSort = !m_reverseSort;
            }
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
            {
                m_reverseSort = !m_reverseSort;
            }
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
            {
                m_reverseSort = !m_reverseSort;
            }
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
            EditCustom();
        }
        else if (action == "DELETE")
        {
            saveRecPriority();
            remove();
        }
        else if (action == "UPCOMING")
        {
            saveRecPriority();
            ShowUpcoming();
        }
        else if (action == "INFO" || action == "DETAILS")
        {
            ShowDetails();
        }
        else
        {
            handled = false;
        }
    }

    if (!handled && MythScreenType::keyPressEvent(event))
        handled = true;

    return handled;
}

void ProgramRecPriority::showMenu(void)
{
    QString label = tr("Options");

    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");
    auto *menuPopup = new MythDialogBox(label, popupStack, "menuPopup");

    if (menuPopup->Create())
    {
        menuPopup->SetReturnEvent(this, "menu");

        menuPopup->AddButton(tr("Increase Priority"));
        menuPopup->AddButton(tr("Decrease Priority"));
        menuPopup->AddButton(tr("Sort"), nullptr, true);
        menuPopup->AddButton(tr("Program Details"));
        menuPopup->AddButton(tr("Upcoming"));
        menuPopup->AddButton(tr("Custom Edit"));
        menuPopup->AddButton(tr("Delete Rule"));
        menuPopup->AddButton(tr("New Template"));

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
    auto *menuPopup = new MythDialogBox(label, popupStack, "menuPopup");

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
        auto *dce = (DialogCompletionEvent*)(event);

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
                ShowDetails();
            }
            else if (resulttext == tr("Upcoming"))
            {
                saveRecPriority();
                ShowUpcoming();
            }
            else if (resulttext == tr("Custom Edit"))
            {
                saveRecPriority();
                EditCustom();
            }
            else if (resulttext == tr("Delete Rule"))
            {
                saveRecPriority();
                remove();
            }
            else if (resulttext == tr("New Template"))
            {
                MythScreenStack *popupStack =
                    GetMythMainWindow()->GetStack("popup stack");
                auto *textInput = new MythTextInputDialog(popupStack,
                                                          tr("Template Name"));
                if (textInput->Create())
                {
                    textInput->SetReturnEvent(this, "templatecat");
                    popupStack->AddScreen(textInput);
                }
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
                {
                    m_reverseSort = !m_reverseSort;
                }
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
                {
                    m_reverseSort = !m_reverseSort;
                }
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
                {
                    m_reverseSort = !m_reverseSort;
                }
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
                {
                    m_reverseSort = !m_reverseSort;
                }
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
                {
                    m_reverseSort = !m_reverseSort;
                }
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
                {
                    m_reverseSort = !m_reverseSort;
                }
                SortList();
            }
        }
        else if (resultid == "deleterule")
        {
            auto *record = dce->GetData().value<RecordingRule *>();
            if (record)
            {
                if (buttonnum > 0)
                {
                    MythUIButtonListItem *item =
                        m_programList->GetItemCurrent();

                    if (record->Delete() && item)
                        RemoveItemFromList(item);
                    else
                        LOG(VB_GENERAL, LOG_ERR,
                            "Failed to delete recording rule");
                }
                delete record;
            }
        }
        else if (resultid == "templatecat")
        {
            newTemplate(resulttext);
        }
        else
        {
            ScheduleCommon::customEvent(event);
        }
    }
}

void ProgramRecPriority::edit(MythUIButtonListItem *item) const
{
    if (!item)
        return;

    auto *pgRecInfo = item->GetData().value<ProgramRecPriorityInfo*>();
    if (!pgRecInfo)
        return;

    auto *record = new RecordingRule();
    record->m_recordID = pgRecInfo->GetRecordingRuleID();
    if (record->m_searchType == kNoSearch)
        record->LoadByProgram(pgRecInfo);

    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
    auto *schededit = new ScheduleEditor(mainStack, record);
    if (schededit->Create())
    {
        mainStack->AddScreen(schededit);
        connect(schededit, &ScheduleEditor::ruleSaved, this, &ProgramRecPriority::scheduleChanged);
        connect(schededit, &ScheduleEditor::ruleDeleted, this, &ProgramRecPriority::scheduleChanged);
    }
    else
    {
        delete schededit;
    }
}

void ProgramRecPriority::newTemplate(QString category)
{
    category = category.trimmed();
    if (category.isEmpty())
        return;

    // Try to find an existing template and use it.
    QMap<int, ProgramRecPriorityInfo>::Iterator it;
    for (it = m_programData.begin(); it != m_programData.end(); ++it)
    {
        ProgramRecPriorityInfo *progInfo = &(*it);
        if (progInfo->GetRecordingRuleType() == kTemplateRecord &&
            category.compare(progInfo->GetCategory(),
                             Qt::CaseInsensitive) == 0)
        {
            m_programList->SetValueByData(QVariant::fromValue(progInfo));
            edit(m_programList->GetItemCurrent());
            return;
        }
    }

    auto *record = new RecordingRule();
    if (!record)
        return;
    record->MakeTemplate(category);

    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
    auto *schededit = new ScheduleEditor(mainStack, record);
    if (schededit->Create())
    {
        mainStack->AddScreen(schededit);
        connect(schededit, &ScheduleEditor::ruleSaved, this, &ProgramRecPriority::scheduleChanged);
        connect(schededit, &ScheduleEditor::ruleDeleted, this, &ProgramRecPriority::scheduleChanged);
    }
    else
    {
        delete schededit;
    }
}

void ProgramRecPriority::scheduleChanged(int recid)
{
    // Assumes that the current item didn't change, which isn't guaranteed
    MythUIButtonListItem *item = m_programList->GetItemCurrent();
    ProgramRecPriorityInfo *pgRecInfo = nullptr;
    if (item)
        pgRecInfo = item->GetData().value<ProgramRecPriorityInfo*>();

    // If the recording id doesn't match, the user created a new
    // template.
    if (!pgRecInfo || recid != pgRecInfo->getRecordID())
    {
        RecordingRule record;
        record.m_recordID = recid;
        if (!record.Load() || record.m_type == kNotRecording)
            return;

        ProgramRecPriorityInfo progInfo;
        progInfo.SetRecordingRuleID(record.m_recordID);
        progInfo.SetRecordingRuleType(record.m_type);
        progInfo.SetTitle(record.m_title, record.m_sortTitle);
        progInfo.SetSubtitle(record.m_subtitle, record.m_sortSubtitle);
        progInfo.SetCategory(record.m_category);
        progInfo.SetRecordingPriority(record.m_recPriority);
        progInfo.m_recType = record.m_type;
        progInfo.m_recStatus = record.m_isInactive ?
            RecStatus::Inactive : RecStatus::Unknown;
        progInfo.m_profile = record.m_recProfile;
        progInfo.m_last_record = record.m_lastRecorded;
        progInfo.m_storageGroup = record.m_storageGroup;
        progInfo.m_recordingGroup = RecordingInfo::GetRecgroupString(record.m_recGroupID);

        m_programData[recid] = progInfo;
        m_origRecPriorityData[record.m_recordID] =
            record.m_recPriority;
        SortList(&m_programData[recid]);

        return;
    }

    // We need to refetch the recording priority values since the Advanced
    // Recording Options page could've been used to change them

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

        // set the recording priorities of that program
        pgRecInfo->SetRecordingPriority(recPriority);
        pgRecInfo->m_recType = (RecordingType)rectype;
        // also set the m_origRecPriorityData with new recording
        // priority so we don't save to db again when we exit
        m_origRecPriorityData[pgRecInfo->GetRecordingRuleID()] =
            pgRecInfo->GetRecordingPriority();
        // also set the active/inactive state
        pgRecInfo->m_recStatus = inactive ? RecStatus::Inactive : RecStatus::Unknown;

        SortList();
    }
    else
    {
        RemoveItemFromList(item);
    }

    countMatches();
}

void ProgramRecPriority::remove(void)
{
    MythUIButtonListItem *item = m_programList->GetItemCurrent();
    if (!item)
        return;

    auto *pgRecInfo = item->GetData().value<ProgramRecPriorityInfo*>();
    if (!pgRecInfo ||
        (pgRecInfo->m_recType == kTemplateRecord &&
         pgRecInfo->GetCategory()
         .compare("Default", Qt::CaseInsensitive) == 0))
    {
        return;
    }

    auto *record = new RecordingRule();
    record->m_recordID = pgRecInfo->GetRecordingRuleID();
    if (!record->Load())
    {
        delete record;
        return;
    }

    QString message = tr("Delete '%1' %2 rule?")
        .arg(record->m_title, toString(pgRecInfo->GetRecordingRuleType()));

    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");

    auto *okPopup = new MythConfirmationDialog(popupStack, message, true);
    okPopup->SetReturnEvent(this, "deleterule");
    okPopup->SetData(QVariant::fromValue(record));

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

    auto *pgRecInfo = item->GetData().value<ProgramRecPriorityInfo*>();
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
                ScheduledRecording::ReschedulePlace(
                    QString("DeactivateRule %1 %2")
                    .arg(pgRecInfo->GetRecordingRuleID())
                    .arg(pgRecInfo->GetTitle()));
                pgRecInfo->m_recStatus = inactive ? RecStatus::Inactive : RecStatus::Unknown;
                item->DisplayState("disabled", "status");
            }
        }
    }
}

void ProgramRecPriority::changeRecPriority(int howMuch)
{
    MythUIButtonListItem *item = m_programList->GetItemCurrent();
    if (!item)
        return;

    auto *pgRecInfo = item->GetData().value<ProgramRecPriorityInfo*>();
    if (!pgRecInfo)
        return;

    // inc/dec recording priority
    int tempRecPriority = pgRecInfo->GetRecordingPriority() + howMuch;
    if (tempRecPriority > -100 && tempRecPriority < 100)
    {
        pgRecInfo->m_recPriority = tempRecPriority;

        // order may change if sorting by recording priority, so resort
        if (m_sortType == byRecPriority)
            SortList();
        else
        {
            // No need to re-fill the entire list, just update this entry
            int progRecPriority = pgRecInfo->GetRecordingPriority();

            item->SetText(QString::number(progRecPriority), "progpriority");
            item->SetText(QString::number(progRecPriority), "recpriority");
            if (m_recPriorityText)
                m_recPriorityText->SetText(QString::number(progRecPriority));

            item->SetText(QString::number(progRecPriority), "recpriorityB");
            if (m_recPriorityBText)
                m_recPriorityBText->SetText(QString::number(progRecPriority));

            item->SetText(QString::number(progRecPriority), "finalpriority");
            if (m_finalPriorityText)
                m_finalPriorityText->SetText(QString::number(progRecPriority));
        }
    }
}

void ProgramRecPriority::saveRecPriority(void)
{
    QMap<int, ProgramRecPriorityInfo>::Iterator it;

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
    std::vector<ProgramInfo *> recordinglist;

    m_programData.clear();

    RemoteGetAllScheduledRecordings(recordinglist);

    for (auto & pgiter : recordinglist)
    {
        ProgramInfo *progInfo = pgiter;
        m_programData[pgiter->GetRecordingRuleID()] =
            (*progInfo);

        // save recording priority value in map so we don't have to
        // save all program's recording priority values when we exit
        m_origRecPriorityData[pgiter->GetRecordingRuleID()] =
            pgiter->GetRecordingPriority();

        delete pgiter;
    }

    // get recording types associated with each program from db
    // (hope this is ok to do here, it's so much lighter doing
    // it all at once than once per program)

    MSqlQuery result(MSqlQuery::InitCon());
    result.prepare("SELECT recordid, title, chanid, starttime, startdate, "
                   "type, inactive, last_record, avg_delay, profile, "
                   "recgroup, storagegroup "
                   "FROM record;");

    if (!result.exec())
    {
        MythDB::DBError("Get program recording priorities query", result);
    }

    countMatches();
    while (result.next()) {
        uint recordid = result.value(0).toUInt();
//      QString title = result.value(1).toString();
//      QString chanid = result.value(2).toString();
//      QString tempTime = result.value(3).toString();
//      QString tempDate = result.value(4).toString();
        RecordingType recType = (RecordingType)result.value(5).toInt();
        int inactive = result.value(6).toInt();
        QDateTime lastrec = MythDate::as_utc(result.value(7).toDateTime());
        int avgd = result.value(8).toInt();
        QString profile = result.value(9).toString();
        QString recordingGroup = result.value(10).toString();
        QString storageGroup = result.value(11).toString();

        // find matching program in m_programData and set
        // recType
        QMap<int, ProgramRecPriorityInfo>::Iterator it;
        it = m_programData.find(recordid);
        if (it != m_programData.end())
        {
            ProgramRecPriorityInfo *progInfo = &(*it);

            progInfo->m_recType = recType;
            progInfo->m_matchCount =
                m_listMatch[progInfo->GetRecordingRuleID()];
            progInfo->m_recCount =
                m_recMatch[progInfo->GetRecordingRuleID()];
            progInfo->m_last_record = lastrec;
            progInfo->m_avg_delay = avgd;
            progInfo->m_profile = profile;
            progInfo->m_recordingGroup  = recordingGroup;
            progInfo->m_storageGroup = storageGroup;

            if (inactive)
                progInfo->m_recStatus = RecStatus::Inactive;
            else if (m_conMatch[progInfo->GetRecordingRuleID()] > 0)
                progInfo->m_recStatus = RecStatus::Conflict;
            else if (m_nowMatch[progInfo->GetRecordingRuleID()] > 0)
                progInfo->m_recStatus = RecStatus::Recording;
            else if (m_recMatch[progInfo->GetRecordingRuleID()] > 0)
                progInfo->m_recStatus = RecStatus::WillRecord;
            else
                progInfo->m_recStatus = RecStatus::Unknown;
        }
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
    QDateTime now = MythDate::current();

    for (auto *program : schedList)
    {
        const RecStatus::Type recstatus = (*program).GetRecordingStatus();
        const uint          recordid  = (*program).GetRecordingRuleID();
        if ((*program).GetRecordingEndTime() > now && recstatus != RecStatus::NotListed)
        {
            m_listMatch[recordid]++;
            if (recstatus == RecStatus::Conflict || recstatus == RecStatus::Offline)
                m_conMatch[recordid]++;
            else if (recstatus == RecStatus::WillRecord)
                m_recMatch[recordid]++;
            else if (recstatus == RecStatus::Recording)
            {
                m_nowMatch[recordid]++;
                m_recMatch[recordid]++;
            }
        }
    }
}

void ProgramRecPriority::SortList(ProgramRecPriorityInfo *newCurrentItem)
{
    if (newCurrentItem)
        m_currentItem = newCurrentItem;
    else
    {
        MythUIButtonListItem *item = m_programList->GetItemCurrent();
        if (item)
            m_currentItem =
                item->GetData().value<ProgramRecPriorityInfo*>();
    }

    QMap<int, ProgramRecPriorityInfo>::Iterator pit;

    // copy m_programData into m_sortedProgram
    m_sortedProgram.clear();
    for (pit = m_programData.begin(); pit != m_programData.end(); ++pit)
    {
        ProgramRecPriorityInfo *progInfo = &(*pit);
        m_sortedProgram.push_back(progInfo);
    }

    // sort m_sortedProgram
    switch (m_sortType)
    {
        case byTitle :
            sort(m_sortedProgram.begin(), m_sortedProgram.end(),
                 TitleSort(m_reverseSort));
            break;
        case byRecPriority :
            sort(m_sortedProgram.begin(), m_sortedProgram.end(),
                 ProgramRecPrioritySort(m_reverseSort));
            break;
        case byRecType :
            sort(m_sortedProgram.begin(), m_sortedProgram.end(),
                 ProgramRecTypeSort(m_reverseSort));
            break;
        case byCount :
            sort(m_sortedProgram.begin(), m_sortedProgram.end(),
                 ProgramCountSort(m_reverseSort));
            break;
        case byRecCount :
            sort(m_sortedProgram.begin(), m_sortedProgram.end(),
                 ProgramRecCountSort(m_reverseSort));
            break;
        case byLastRecord :
            sort(m_sortedProgram.begin(), m_sortedProgram.end(),
                 ProgramLastRecordSort(m_reverseSort));
            break;
        case byAvgDelay :
            sort(m_sortedProgram.begin(), m_sortedProgram.end(),
                 ProgramAvgDelaySort(m_reverseSort));
            break;
    }

    UpdateList();
}

void ProgramRecPriority::UpdateList()
{
    if (!m_currentItem && !m_programList->IsEmpty())
        m_currentItem = m_programList->GetItemCurrent()->GetData()
                                    .value<ProgramRecPriorityInfo*>();

    m_programList->Reset();

    std::vector<ProgramRecPriorityInfo*>::iterator it;
    for (it = m_sortedProgram.begin(); it != m_sortedProgram.end(); ++it)
    {
        ProgramRecPriorityInfo *progInfo = *it;

        auto *item = new MythUIButtonListItem(m_programList, "",
                                              QVariant::fromValue(progInfo));

        int progRecPriority = progInfo->GetRecordingPriority();

        if ((progInfo->m_recType == kSingleRecord ||
                progInfo->m_recType == kOverrideRecord ||
                progInfo->m_recType == kDontRecord) &&
            !(progInfo->GetSubtitle()).trimmed().isEmpty())
        {
            QString rating = QString::number(progInfo->GetStars(10));

            item->DisplayState(rating, "ratingstate");
        }
        else
        {
            progInfo->m_subtitle.clear();
        }

        QString state;
        if (progInfo->m_recType == kDontRecord ||
            (progInfo->m_recType != kTemplateRecord &&
             progInfo->m_recStatus == RecStatus::Inactive))
            state = "disabled";
        else if (m_conMatch[progInfo->GetRecordingRuleID()] > 0)
            state = "error";
        else if (m_recMatch[progInfo->GetRecordingRuleID()] > 0 ||
                 progInfo->m_recType == kTemplateRecord)
            state = "normal";
        else if (m_nowMatch[progInfo->GetRecordingRuleID()] > 0)
            state = "running";
        else
            state = "warning";

        InfoMap infoMap;
        progInfo->ToMap(infoMap);
        item->SetTextFromMap(infoMap, state);

        QString subtitle;
        if (progInfo->m_subtitle != "(null)" &&
            (progInfo->m_recType == kSingleRecord ||
             progInfo->m_recType == kOverrideRecord ||
             progInfo->m_recType == kDontRecord))
        {
            subtitle = progInfo->m_subtitle;
        }

        QString matchInfo;
        if (progInfo->GetRecordingStatus() == RecStatus::Inactive)
        {
            matchInfo = QString("%1 %2")
                        .arg(m_listMatch[progInfo->GetRecordingRuleID()])
                        .arg(RecStatus::toString(progInfo->GetRecordingStatus(),
                                      progInfo->GetRecordingRuleType()));
        }
        else
        {
            matchInfo = tr("Recording %1 of %2")
                        .arg(m_recMatch[progInfo->GetRecordingRuleID()])
                        .arg(m_listMatch[progInfo->GetRecordingRuleID()]);
        }

        subtitle = QString("(%1) %2").arg(matchInfo, subtitle);
        item->SetText(subtitle, "scheduleinfo", state);

        item->SetText(QString::number(progRecPriority), "progpriority", state);
        item->SetText(QString::number(progRecPriority), "finalpriority", state);

        item->SetText(QString::number(progRecPriority), "recpriority", state);
        item->SetText(QString::number(progRecPriority), "recpriorityB", state);

        QString tempDateTime = MythDate::toString(progInfo->m_last_record,
                                                    MythDate::kDateTimeFull | MythDate::kSimplify |
                                                    MythDate::kAddYear);
        item->SetText(tempDateTime, "lastrecorded", state);
        QString tempDate = MythDate::toString(progInfo->m_last_record,
                                                MythDate::kDateFull | MythDate::kSimplify |
                                                MythDate::kAddYear);
        item->SetText(tempDate, "lastrecordeddate", state);
        QString tempTime = MythDate::toString(
            progInfo->m_last_record, MythDate::kTime);
        item->SetText(tempTime, "lastrecordedtime", state);

        QString channame = progInfo->m_chanName;
        QString channum = progInfo->m_chanStr;
        QString callsign = progInfo->m_chanSign;
        if (progInfo->m_recType != kSingleRecord &&
            progInfo->m_recType != kOverrideRecord &&
            progInfo->m_recType != kDontRecord &&
            !(progInfo->GetRecordingRule()->m_filter & 1024) &&
            progInfo->GetRecordingRule()->m_searchType != kManualSearch)
        {
            channame = tr("Any");
            channum = tr("Any");
            callsign = tr("Any");
        }
        item->SetText(channame, "channel", state);
        item->SetText(channum, "channum", state);
        item->SetText(callsign, "callsign", state);

        QString profile = progInfo->m_profile;
        if ((profile == "Default") || (profile == "Live TV") ||
            (profile == "High Quality") || (profile == "Low Quality"))
            profile = tr(profile.toUtf8().constData());
        item->SetText(profile, "recordingprofile", state);
        item->SetText(progInfo->m_recordingGroup, "recordinggroup", state);
        item->SetText(progInfo->m_storageGroup, "storagegroup", state);
        item->DisplayState(state, "status");

        if (m_currentItem == progInfo)
            m_programList->SetItemCurrent(item);
    }

    m_currentItem = nullptr;

    MythUIText *norecordingText = dynamic_cast<MythUIText*>
                                                (GetChild("norecordings_info"));

    if (norecordingText)
        norecordingText->SetVisible(m_programData.isEmpty());
}

// Called whenever a new recording is selected from the list of
// recordings. This function updates the screen with the information
// on the newly selected recording.
void ProgramRecPriority::updateInfo(MythUIButtonListItem *item)
{
    if (!item)
        return;

    auto *pgRecInfo = item->GetData().value<ProgramRecPriorityInfo *>();

    if (!pgRecInfo)
        return;

    int progRecPriority = pgRecInfo->GetRecordingPriority();

    QString subtitle;
    if (pgRecInfo->m_subtitle != "(null)" &&
        (pgRecInfo->m_recType == kSingleRecord ||
            pgRecInfo->m_recType == kOverrideRecord ||
            pgRecInfo->m_recType == kDontRecord))
    {
        subtitle = pgRecInfo->m_subtitle;
    }

    QString matchInfo;
    if (pgRecInfo->GetRecordingStatus() == RecStatus::Inactive)
    {
        matchInfo = QString("%1 %2")
            .arg(m_listMatch[pgRecInfo->GetRecordingRuleID()])
            .arg(RecStatus::toString(pgRecInfo->GetRecordingStatus(),
                          pgRecInfo->GetRecordingRuleType()));
    }
    else
    {
        matchInfo = tr("Recording %1 of %2")
            .arg(m_recMatch[pgRecInfo->GetRecordingRuleID()])
            .arg(m_listMatch[pgRecInfo->GetRecordingRuleID()]);
    }

    subtitle = QString("(%1) %2").arg(matchInfo, subtitle);

    InfoMap infoMap;
    pgRecInfo->ToMap(infoMap);
    SetTextFromMap(infoMap);

    if (m_schedInfoText)
        m_schedInfoText->SetText(subtitle);

    if (m_recPriorityText)
        m_recPriorityText->SetText(QString::number(progRecPriority));

    if (m_recPriorityBText)
        m_recPriorityBText->SetText(QString::number(progRecPriority));

    if (m_finalPriorityText)
        m_finalPriorityText->SetText(QString::number(progRecPriority));

    if (m_lastRecordedText)
    {
        QString tempDateTime = MythDate::toString(pgRecInfo->m_last_record,
                                                    MythDate::kDateTimeFull | MythDate::kSimplify |
                                                    MythDate::kAddYear);
        m_lastRecordedText->SetText(tempDateTime);
    }

    if (m_lastRecordedDateText)
    {
        QString tempDate = MythDate::toString(pgRecInfo->m_last_record,
                                                MythDate::kDateFull | MythDate::kSimplify |
                                                MythDate::kAddYear);
        m_lastRecordedDateText->SetText(tempDate);
    }

    if (m_lastRecordedTimeText)
    {
        QString tempTime = MythDate::toString(
            pgRecInfo->m_last_record, MythDate::kTime);
        m_lastRecordedTimeText->SetText(tempTime);
    }

    if (m_chanNameText || m_chanNumText || m_callSignText)
    {
        QString channame = pgRecInfo->m_chanName;
        QString channum = pgRecInfo->m_chanStr;
        QString callsign = pgRecInfo->m_chanSign;
        if (pgRecInfo->m_recType != kSingleRecord &&
            pgRecInfo->m_recType != kOverrideRecord &&
            pgRecInfo->m_recType != kDontRecord &&
            !(pgRecInfo->GetRecordingRule()->m_filter & 1024) &&
            pgRecInfo->GetRecordingRule()->m_searchType != kManualSearch)
        {
            channame = tr("Any");
            channum = tr("Any");
            callsign = tr("Any");
        }
        if (m_chanNameText)
            m_chanNameText->SetText(channame);
        if (m_chanNumText)
            m_chanNumText->SetText(channum);
        if (m_callSignText)
            m_callSignText->SetText(callsign);
    }

    if (m_recProfileText)
    {
        QString profile = pgRecInfo->m_profile;
        if ((profile == "Default") || (profile == "Live TV") ||
            (profile == "High Quality") || (profile == "Low Quality"))
            profile = tr(profile.toUtf8().constData());
        m_recProfileText->SetText(profile);
    }
}

void ProgramRecPriority::RemoveItemFromList(MythUIButtonListItem *item)
{
    if (!item)
        return;

    auto *pgRecInfo = item->GetData().value<ProgramRecPriorityInfo *>();

    if (!pgRecInfo)
        return;

    QMap<int, ProgramRecPriorityInfo>::iterator it;
    it = m_programData.find(pgRecInfo->GetRecordingRuleID());
    if  (it != m_programData.end())
        m_programData.erase(it);

    m_programList->RemoveItem(item);
}

ProgramInfo *ProgramRecPriority::GetCurrentProgram(void) const
{
    MythUIButtonListItem *item = m_programList->GetItemCurrent();
    return item ? item->GetData().value<ProgramRecPriorityInfo*>() : nullptr;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
