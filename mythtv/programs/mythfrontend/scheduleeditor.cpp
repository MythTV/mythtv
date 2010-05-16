
#include "scheduleeditor.h"

// QT
#include <QString>
#include <QHash>
#include <QCoreApplication>

// Libmyth
#include "mythcorecontext.h"
#include "storagegroup.h"

// Libmythtv
#include "playgroup.h"
#include "tv_play.h"

// Libmythui
#include "mythmainwindow.h"
#include "mythuihelper.h"
#include "mythuibuttonlist.h"
#include "mythuibutton.h"
#include "mythuitext.h"
#include "mythuistatetype.h"
#include "mythuispinbox.h"
#include "mythuicheckbox.h"
#include "mythdialogbox.h"
#include "mythuiutils.h"

// Mythfrontend
#include "proglist.h"
#include "viewschedulediff.h"

#define ENUM_TO_QVARIANT(a) qVariantFromValue(static_cast<int>(a))


void *ScheduleEditor::RunScheduleEditor(ProgramInfo *proginfo, void *player)
{
    RecordingRule *rule = new RecordingRule();
    rule->LoadByProgram(proginfo);

    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
    ScheduleEditor *se = new ScheduleEditor(mainStack, rule,
                                            static_cast<TV*>(player));

    if (se->Create())
        mainStack->AddScreen(se, (player == NULL));
    else
        delete se;

    return NULL;
}

/** \class ScheduleEditor
 *  \brief Construct a recording schedule
 *
 */

ScheduleEditor::ScheduleEditor(MythScreenStack *parent,
                               RecordingInfo *recInfo, TV *player)
          : ScheduleCommon(parent, "ScheduleEditor"),
            m_recInfo(new RecordingInfo(*recInfo)), m_recordingRule(NULL),
            m_sendSig(false),
            m_saveButton(NULL), m_cancelButton(NULL), m_rulesList(NULL),
            m_schedOptButton(NULL), m_storeOptButton(NULL),
            m_postProcButton(NULL), m_schedInfoButton(NULL),
            m_previewButton(NULL), m_player(player)
{
    m_recordingRule = new RecordingRule();
    m_recordingRule->m_recordID = m_recInfo->GetRecordingRuleID();
}

ScheduleEditor::ScheduleEditor(MythScreenStack *parent,
                               RecordingRule *recRule, TV *player)
          : ScheduleCommon(parent, "ScheduleEditor"),
            m_recInfo(NULL), m_recordingRule(recRule),
            m_sendSig(false),
            m_saveButton(NULL), m_cancelButton(NULL), m_rulesList(NULL),
            m_schedOptButton(NULL), m_storeOptButton(NULL),
            m_postProcButton(NULL), m_schedInfoButton(NULL),
            m_previewButton(NULL), m_player(player)
{
}

ScheduleEditor::~ScheduleEditor(void)
{
    delete m_recordingRule;

    // if we have a player, we need to tell we are done
    if (m_player)
    {
        QString message = QString("VIEWSCHEDULED_EXITING");
        qApp->postEvent(m_player, new MythEvent(message));
    }
}

bool ScheduleEditor::Create()
{
    if (!LoadWindowFromXML("schedule-ui.xml", "scheduleeditor", this))
        return false;

    bool err = false;

    UIUtilE::Assign(this, m_rulesList, "rules", &err);

    UIUtilE::Assign(this, m_schedOptButton, "schedoptions", &err);
    UIUtilE::Assign(this, m_storeOptButton, "storeoptions", &err);
    UIUtilE::Assign(this, m_postProcButton, "postprocessing", &err);
    UIUtilE::Assign(this, m_schedInfoButton, "schedinfo", &err);
    UIUtilE::Assign(this, m_previewButton, "preview", &err);

    UIUtilE::Assign(this, m_cancelButton, "cancel", &err);
    UIUtilE::Assign(this, m_saveButton, "save", &err);

    if (err)
    {
        VERBOSE(VB_IMPORTANT, "ScheduleEditor, theme is missing "
                              "required elements");
        return false;
    }

    connect(m_rulesList, SIGNAL(itemSelected(MythUIButtonListItem *)),
                         SLOT(RuleChanged(MythUIButtonListItem *)));

    connect(m_schedOptButton, SIGNAL(Clicked()), SLOT(ShowSchedOpt()));
    connect(m_storeOptButton, SIGNAL(Clicked()), SLOT(ShowStoreOpt()));
    connect(m_postProcButton, SIGNAL(Clicked()), SLOT(ShowPostProc()));
    connect(m_schedInfoButton, SIGNAL(Clicked()), SLOT(ShowSchedInfo()));
    connect(m_previewButton, SIGNAL(Clicked()), SLOT(ShowPreview()));

    connect(m_cancelButton, SIGNAL(Clicked()), SLOT(Close()));
    connect(m_saveButton, SIGNAL(Clicked()), SLOT(Save()));

    BuildFocusList();

    if (!m_recordingRule->IsLoaded())
    {
        if (m_recInfo)
            m_recordingRule->LoadByProgram(m_recInfo);
        else if (m_recordingRule->m_recordID)
            m_recordingRule->Load();

        if (!m_recordingRule->IsLoaded())
        {
            VERBOSE(VB_IMPORTANT, "ScheduleEditor::Create() - Failed to load "
                                  "recording rule");
            return false;
        }
    }

    return true;
}

void ScheduleEditor::Close()
{
    // don't fade the screen if we are returning to the player
    if (m_player)
        GetScreenStack()->PopScreen(this, false);
    else
        GetScreenStack()->PopScreen(this, true);
}

void ScheduleEditor::Load()
{
    // Copy this now, it will change briefly after the first item is inserted
    // into the list by design of MythUIButtonList::itemSelected()
    RecordingType type = m_recordingRule->m_type;

    // Rules List
    if (m_recordingRule->m_isOverride)
    {
        new MythUIButtonListItem(m_rulesList,
                                 tr("Record this showing with normal options"),
                                 ENUM_TO_QVARIANT(kNotRecording));
        new MythUIButtonListItem(m_rulesList,
                                 tr("Record this showing with override options"),
                                 ENUM_TO_QVARIANT(kOverrideRecord));
        new MythUIButtonListItem(m_rulesList,
                                 tr("Do not allow this showing to be recorded"),
                                 ENUM_TO_QVARIANT(kDontRecord));
    }
    else
    {
        bool hasChannel = !m_recordingRule->m_station.isEmpty();
        bool isManual = (m_recordingRule->m_searchType == kManualSearch);

        new MythUIButtonListItem(m_rulesList, tr("Do not record this program"),
                                 ENUM_TO_QVARIANT(kNotRecording));

        if (hasChannel)
            new MythUIButtonListItem(m_rulesList,
                                     tr("Record only this showing"),
                                     ENUM_TO_QVARIANT(kSingleRecord));
        if (!isManual)
            new MythUIButtonListItem(m_rulesList,
                                     tr("Record one showing of this title"),
                                     ENUM_TO_QVARIANT(kFindOneRecord));
        if (hasChannel)
            new MythUIButtonListItem(m_rulesList,
                                     tr("Record in this timeslot every week"),
                                     ENUM_TO_QVARIANT(kWeekslotRecord));
        if (!isManual)
            new MythUIButtonListItem(m_rulesList,
                                     tr("Record one showing of this title every week"),
                                     ENUM_TO_QVARIANT(kFindWeeklyRecord));
        if (hasChannel)
            new MythUIButtonListItem(m_rulesList,
                                     tr("Record in this timeslot every day"),
                                     ENUM_TO_QVARIANT(kTimeslotRecord));
        if (!isManual)
            new MythUIButtonListItem(m_rulesList,
                                     tr("Record one showing of this title every day"),
                                     ENUM_TO_QVARIANT(kFindDailyRecord));
        if (hasChannel && !isManual)
            new MythUIButtonListItem(m_rulesList,
                                     tr("Record at any time on this channel"),
                                     ENUM_TO_QVARIANT(kChannelRecord));
        if (!isManual)
            new MythUIButtonListItem(m_rulesList,
                                     tr("Record at any time on any channel"),
                                     ENUM_TO_QVARIANT(kAllRecord));
    }
    m_rulesList->SetValueByData(ENUM_TO_QVARIANT(type));

    QHash<QString, QString> progMap;
    if (m_recInfo)
        m_recInfo->ToMap(progMap);
    else
        m_recordingRule->ToMap(progMap);

    switch (m_recordingRule->m_searchType)
    {
        case kPowerSearch:
            progMap["searchType"] = tr("Power Search");
            break;
        case kTitleSearch:
            progMap["searchType"] = tr("Title Search");
            break;
        case kKeywordSearch:
            progMap["searchType"] = tr("Keyword Search");
            break;
        case kPeopleSearch:
            progMap["searchType"] = tr("People Search");
            break;
        default:
            progMap["searchType"] = tr("Unknown Search");
            break;
    }

    SetTextFromMap(progMap);
}

void ScheduleEditor::RuleChanged(MythUIButtonListItem *item)
{
    if (!item)
        return;

    RecordingType type = static_cast<RecordingType>(item->GetData().toInt());

    bool isScheduled = (type != kNotRecording);

    m_schedOptButton->SetEnabled(isScheduled);
    m_storeOptButton->SetEnabled(isScheduled);
    m_postProcButton->SetEnabled(isScheduled);

    m_recordingRule->m_type = type;
}

void ScheduleEditor::Save()
{
    MythUIButtonListItem *item = m_rulesList->GetItemCurrent();
    if (item)
    {
        RecordingType type = static_cast<RecordingType>(item->GetData().toInt());
        if (type == kNotRecording)
        {
            DeleteRule();
            Close();
            return;
        }
        else
            m_recordingRule->m_type = type;
    }

    m_recordingRule->Save(true);
    emit ruleSaved(m_recordingRule->m_recordID);
    Close();
}

void ScheduleEditor::DeleteRule()
{
    m_recordingRule->Delete();
}

void ScheduleEditor::ShowSchedOpt()
{
    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
    SchedOptEditor *schedoptedit = new SchedOptEditor(mainStack, m_recInfo,
                                                      m_recordingRule);
    if (schedoptedit->Create())
        mainStack->AddScreen(schedoptedit);
    else
        delete schedoptedit;
}

void ScheduleEditor::ShowStoreOpt()
{
    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
    StoreOptEditor *storeoptedit = new StoreOptEditor(mainStack, m_recInfo,
                                                      m_recordingRule);
    if (storeoptedit->Create())
        mainStack->AddScreen(storeoptedit);
    else
        delete storeoptedit;
}

void ScheduleEditor::ShowPostProc()
{
    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
    PostProcEditor *ppedit = new PostProcEditor(mainStack, m_recInfo,
                                                m_recordingRule);
    if (ppedit->Create())
        mainStack->AddScreen(ppedit);
    else
        delete ppedit;
}

void ScheduleEditor::ShowSchedInfo()
{
    QString label = tr("Schedule Information");

    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");
    MythDialogBox *menuPopup = new MythDialogBox(label, popupStack, "menuPopup");

    if (menuPopup->Create())
    {
        menuPopup->SetReturnEvent(this, "schedinfo");

        menuPopup->AddButton(tr("Program Details"));
        menuPopup->AddButton(tr("Upcoming episodes"));
        menuPopup->AddButton(tr("Upcoming recordings"));
        menuPopup->AddButton(tr("Previously scheduled"));

        popupStack->AddScreen(menuPopup);
    }
    else
        delete menuPopup;
}

void ScheduleEditor::customEvent(QEvent *event)
{
    if (event->type() == DialogCompletionEvent::kEventType)
    {
        DialogCompletionEvent *dce = (DialogCompletionEvent*)(event);

        QString resultid  = dce->GetId();
        int     buttonnum = dce->GetResult();

        if (resultid == "schedinfo")
        {
            switch (buttonnum)
            {
                case 0 :
                    if (m_recInfo)
                        ShowDetails(m_recInfo);
                    break;
                case 1 :
                    showUpcomingByTitle();
                    break;
                case 2 :
                    showUpcomingByRule();
                    break;
                case 3 :
                    showPrevious();
                    break;
            }
        }
    }
}

void ScheduleEditor::showPrevious(void)
{
    QString title;
    if (m_recInfo)
        title = m_recInfo->GetTitle();
    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
    ProgLister *pl = new ProgLister(mainStack, m_recordingRule->m_recordID,
                                    title);
    if (pl->Create())
        mainStack->AddScreen(pl);
    else
        delete pl;
}

void ScheduleEditor::showUpcomingByRule(void)
{
    // No rule? Search by title
    if (m_recordingRule->m_recordID <= 0)
    {
        showUpcomingByTitle();
        return;
    }

    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
    ProgLister *pl = new ProgLister(mainStack, plRecordid,
                                QString::number(m_recordingRule->m_recordID),
                                    "");

    if (pl->Create())
        mainStack->AddScreen(pl);
    else
        delete pl;
}

void ScheduleEditor::showUpcomingByTitle(void)
{
    QString title = m_recordingRule->m_title;

    if (m_recordingRule->m_searchType != kNoSearch)
        title.remove(QRegExp(" \\(.*\\)$"));

    ShowUpcoming(title);
}

void ScheduleEditor::ShowPreview(void)
{
    QString ttable = "record_tmp";
    m_recordingRule->UseTempTable(true, ttable);

    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
    ViewScheduleDiff *vsd = new ViewScheduleDiff(mainStack, ttable,
                                                 m_recordingRule->m_tempID,
                                                 m_recordingRule->m_title);
    if (vsd->Create())
        mainStack->AddScreen(vsd);
    else
        delete vsd;

    m_recordingRule->UseTempTable(false);
}

////////////////////////////////////////////////////////

/** \class SchedOptEditor
 *  \brief Select schedule options
 *
 */

SchedOptEditor::SchedOptEditor(MythScreenStack *parent,
                               RecordingInfo *recInfo,
                               RecordingRule *rule)
          : MythScreenType(parent, "ScheduleOptionsEditor"),
            m_recInfo(NULL), m_recordingRule(rule),
            m_backButton(NULL),
            m_prioritySpin(NULL), m_inputList(NULL), m_startoffsetSpin(NULL),
            m_endoffsetSpin(NULL), m_dupmethodList(NULL), m_dupscopeList(NULL),
            m_ruleactiveCheck(NULL)
{
    if (recInfo)
        m_recInfo = new RecordingInfo(*recInfo);
}

SchedOptEditor::~SchedOptEditor(void)
{
}

bool SchedOptEditor::Create()
{
    if (!LoadWindowFromXML("schedule-ui.xml", "scheduleoptionseditor", this))
        return false;

    bool err = false;

    UIUtilE::Assign(this, m_prioritySpin, "priority", &err);
    UIUtilE::Assign(this, m_inputList, "input", &err);
    UIUtilE::Assign(this, m_startoffsetSpin, "startoffset", &err);
    UIUtilE::Assign(this, m_endoffsetSpin, "endoffset", &err);
    UIUtilE::Assign(this, m_dupmethodList, "dupmethod", &err);
    UIUtilE::Assign(this, m_dupscopeList, "dupscope", &err);

    UIUtilE::Assign(this, m_ruleactiveCheck, "ruleactive", &err);

    UIUtilE::Assign(this, m_backButton, "back", &err);

    if (err)
    {
        VERBOSE(VB_IMPORTANT, "SchedOptEditor, theme is missing "
                              "required elements");
        return false;
    }

    if ((m_recordingRule->m_type == kSingleRecord) ||
        (m_recordingRule->m_type == kFindOneRecord) ||
        (m_recordingRule->m_type ==kOverrideRecord))
    {
        m_dupmethodList->SetEnabled(false);
        m_dupscopeList->SetEnabled(false);
    }

    connect(m_dupmethodList, SIGNAL(itemSelected(MythUIButtonListItem *)),
                         SLOT(dupMatchChanged(MythUIButtonListItem *)));

    connect(m_backButton, SIGNAL(Clicked()), SLOT(Close()));

    BuildFocusList();

    return true;
}

void SchedOptEditor::Load()
{
    MSqlQuery query(MSqlQuery::InitCon());

    // Priority
    m_prioritySpin->SetRange(-99,99,1,5);
    m_prioritySpin->SetValue(m_recordingRule->m_recPriority);

    // Preferred Input
    new MythUIButtonListItem(m_inputList, tr("Use any available input"),
                             qVariantFromValue(0));

    query.prepare("SELECT cardinputid, cardid, inputname, displayname "
                  "FROM cardinput ORDER BY cardinputid");

    if (query.exec())
    {
        while (query.next())
        {
            QString input_name = query.value(3).toString();
            if (input_name.isEmpty())
                input_name = QString("%1: %2").arg(query.value(1).toInt())
                                              .arg(query.value(2).toString());
            new MythUIButtonListItem(m_inputList, tr("Prefer input %1")
                                        .arg(input_name), query.value(0));
        }
    }

    m_inputList->SetValueByData(m_recordingRule->m_prefInput);

    // Start Offset
    m_startoffsetSpin->SetRange(480,-480,1,10);
    m_startoffsetSpin->SetValue(m_recordingRule->m_startOffset);

    // End Offset
    m_endoffsetSpin->SetRange(-480,480,1,10);
    m_endoffsetSpin->SetValue(m_recordingRule->m_endOffset);

    // Duplicate Match Type
    new MythUIButtonListItem(m_dupmethodList,
                             tr("Match duplicates using subtitle & "
                                "description"),
                             ENUM_TO_QVARIANT(kDupCheckSubDesc));
    new MythUIButtonListItem(m_dupmethodList,
                             tr("Match duplicates using subtitle then "
                                "description"),
                             ENUM_TO_QVARIANT(kDupCheckSubThenDesc));
    new MythUIButtonListItem(m_dupmethodList,
                             tr("Match duplicates using subtitle"),
                             ENUM_TO_QVARIANT(kDupCheckSub));
    new MythUIButtonListItem(m_dupmethodList,
                             tr("Match duplicates using description"),
                             ENUM_TO_QVARIANT(kDupCheckDesc));
    new MythUIButtonListItem(m_dupmethodList,
                             tr("Don't match duplicates"),
                             ENUM_TO_QVARIANT(kDupCheckNone));

    m_dupmethodList->SetValueByData(
                                ENUM_TO_QVARIANT(m_recordingRule->m_dupMethod));

    // Duplicate Matching Scope
    new MythUIButtonListItem(m_dupscopeList,
                             tr("Look for duplicates in current and previous "
                                "recordings"),
                             ENUM_TO_QVARIANT(kDupsInAll));
    new MythUIButtonListItem(m_dupscopeList,
                             tr("Look for duplicates in current recordings "
                                "only"),
                             ENUM_TO_QVARIANT(kDupsInRecorded));
    new MythUIButtonListItem(m_dupscopeList,
                             tr("Look for duplicates in previous recordings "
                                "only"),
                             ENUM_TO_QVARIANT(kDupsInOldRecorded));
    new MythUIButtonListItem(m_dupscopeList,
                             tr("Exclude unidentified episodes"),
                             ENUM_TO_QVARIANT(kDupsExGeneric | kDupsInAll));
    if (gCoreContext->GetNumSetting("HaveRepeats", 0))
    {
        new MythUIButtonListItem(m_dupscopeList,
                                 tr("Exclude old episodes"),
                                 ENUM_TO_QVARIANT(kDupsExRepeats | kDupsInAll));
        new MythUIButtonListItem(m_dupscopeList,
                                 tr("Record new episodes only"),
                                 ENUM_TO_QVARIANT(kDupsNewEpi | kDupsInAll));
        new MythUIButtonListItem(m_dupscopeList,
                                 tr("Record new episode first showings"),
                                 ENUM_TO_QVARIANT(kDupsFirstNew | kDupsInAll));
    }
    m_dupscopeList->SetValueByData(ENUM_TO_QVARIANT(m_recordingRule->m_dupIn));

    // Active/Disabled
    m_ruleactiveCheck->SetCheckState(!m_recordingRule->m_isInactive);

    QHash<QString, QString> progMap;
    if (m_recInfo)
        m_recInfo->ToMap(progMap);
    else
        m_recordingRule->ToMap(progMap);
    SetTextFromMap(progMap);
}

void SchedOptEditor::dupMatchChanged(MythUIButtonListItem *item)
{
    if (!item)
        return;

    int dupMethod = item->GetData().toInt();

    if (dupMethod <= 0)
        m_dupscopeList->SetEnabled(false);
    else
        m_dupscopeList->SetEnabled(true);
}

void SchedOptEditor::Save()
{
    // Priority
    m_recordingRule->m_recPriority = m_prioritySpin->GetIntValue();

    // Preferred Input
    m_recordingRule->m_prefInput = m_inputList->GetDataValue().toInt();

    // Start Offset
    m_recordingRule->m_startOffset = m_startoffsetSpin->GetIntValue();

    // End Offset
    m_recordingRule->m_endOffset = m_endoffsetSpin->GetIntValue();

    // Duplicate Match Type
    m_recordingRule->m_dupMethod = static_cast<RecordingDupMethodType>
                                    (m_dupmethodList->GetDataValue().toInt());

    // Duplicate Matching Scope
    m_recordingRule->m_dupIn = static_cast<RecordingDupInType>
                                    (m_dupscopeList->GetDataValue().toInt());

    // Active/Disabled
    m_recordingRule->m_isInactive = (!m_ruleactiveCheck->GetBooleanCheckState());
}

void SchedOptEditor::Close()
{
    Save();
    MythScreenType::Close();
}

/////////////////////////////

/** \class StoreOptEditor
 *  \brief Select storage options
 *
 */

StoreOptEditor::StoreOptEditor(MythScreenStack *parent,
                               RecordingInfo *recInfo,
                               RecordingRule *rule)
          : MythScreenType(parent, "StorageOptionsEditor"),
            m_recInfo(NULL), m_recordingRule(rule),
            m_backButton(NULL),
            m_recprofileList(NULL), m_recgroupList(NULL),
            m_storagegroupList(NULL), m_playgroupList(NULL),
            m_autoexpireCheck(NULL), m_maxepSpin(NULL), m_maxbehaviourList(NULL)
{
    if (recInfo)
        m_recInfo = new RecordingInfo(*recInfo);
}

StoreOptEditor::~StoreOptEditor(void)
{
}

bool StoreOptEditor::Create()
{
    if (!LoadWindowFromXML("schedule-ui.xml", "storageoptionseditor", this))
        return false;

    bool err = false;

    UIUtilE::Assign(this, m_recprofileList, "recprofile", &err);
    UIUtilE::Assign(this, m_recgroupList, "recgroup", &err);
    UIUtilE::Assign(this, m_storagegroupList, "storagegroup", &err);
    UIUtilE::Assign(this, m_playgroupList, "playgroup", &err);
    UIUtilE::Assign(this, m_maxepSpin, "maxepisodes", &err);
    UIUtilE::Assign(this, m_maxbehaviourList, "maxnewest", &err);

    UIUtilE::Assign(this, m_autoexpireCheck, "autoexpire", &err);

    UIUtilE::Assign(this, m_backButton, "back", &err);

    if (err)
    {
        VERBOSE(VB_IMPORTANT, "StoreOptEditor, theme is missing "
                              "required elements");
        return false;
    }

    connect(m_maxepSpin, SIGNAL(itemSelected(MythUIButtonListItem *)),
                         SLOT(maxEpChanged(MythUIButtonListItem *)));

    connect(m_recgroupList, SIGNAL(LosingFocus()),
                            SLOT(PromptForRecgroup()));

    connect(m_backButton, SIGNAL(Clicked()), SLOT(Close()));

    BuildFocusList();

    return true;
}

void StoreOptEditor::Load()
{
    MSqlQuery query(MSqlQuery::InitCon());

    // Recording Profile
    QString label = tr("Record using the %1 profile");
    QMap<int, QString> profiles = RecordingProfile::listProfiles(0);
    QMap<int, QString>::iterator pit;
    for (pit = profiles.begin(); pit != profiles.end(); ++pit)
    {
        new MythUIButtonListItem(m_recprofileList, label.arg(pit.value()),
                                 qVariantFromValue(pit.value()));
    }
    m_recprofileList->SetValueByData(m_recordingRule->m_recProfile);

    // Recording Group
    QStringList groups;
    QStringList::Iterator it;
    QString value, dispValue;
    bool foundDefault = false;

        // Count the ways the following is _wrong_

    new MythUIButtonListItem(m_recgroupList,
                             tr("Create a new recording group"),
                             qVariantFromValue(QString("__NEW_GROUP__")));

    query.prepare("SELECT DISTINCT recgroup FROM recorded");
    if (query.exec())
    {
        while (query.next())
        {
            value = query.value(0).toString();
            groups += value;

            if (value == "Default")
                foundDefault = true;
        }
    }

    query.prepare("SELECT DISTINCT recgroup FROM record");
    if (query.exec())
    {
        while (query.next())
        {
            value = query.value(0).toString();
            groups += value;

            if (value == "Default")
                foundDefault = true;
        }
    }

    groups.sort();
    for (it = groups.begin(); it != groups.end(); ++it)
    {
        label = tr("Include in the \"%1\" recording group");
        if (!foundDefault && *it > tr("Default"))
        {
            new MythUIButtonListItem(m_recgroupList, label.arg(tr("Default")),
                                     qVariantFromValue(QString("Default")));
            foundDefault = true;
        }

        if (*it == "Default")
            dispValue = tr("Default");
        else
            dispValue = *it;

        new MythUIButtonListItem(m_recgroupList, label.arg(dispValue),
                                 qVariantFromValue(*it));
    }

    m_recgroupList->SetValueByData(m_recordingRule->m_recGroup);

    // Storage Group
    groups = StorageGroup::getRecordingsGroups();
    foundDefault = false;
    for (it = groups.begin(); it != groups.end(); ++it)
    {
        if (*it == "Default")
            foundDefault = true;
    }

    for (it = groups.begin(); it != groups.end(); ++it)
    {
        label = tr("Store in the \"%1\" storage group");
        if (!foundDefault && *it > tr("Default"))
        {
            new MythUIButtonListItem(m_storagegroupList,
                                     label.arg(tr("Default")),
                                     qVariantFromValue(QString("Default")));
            foundDefault = true;
        }

        if (*it == "Default")
            dispValue = tr("Default");
        else if (*it == "LiveTV")
            dispValue = tr("LiveTV");
        else
            dispValue = *it;

        new MythUIButtonListItem(m_storagegroupList, label.arg(dispValue),
                                 qVariantFromValue(*it));
    }

    m_storagegroupList->SetValueByData(m_recordingRule->m_storageGroup);

    // Playback Group
    label = tr("Use \"%1\" playback group settings");
    new MythUIButtonListItem(m_playgroupList, label.arg(tr("Default")),
                             qVariantFromValue(QString("Default")));

    groups = PlayGroup::GetNames();

    for (it = groups.begin(); it != groups.end(); ++it)
    {
        new MythUIButtonListItem(m_playgroupList, label.arg(*it),
                                 qVariantFromValue(*it));
    }

    m_playgroupList->SetValueByData(m_recordingRule->m_playGroup);

    // Auto-Expire
    m_autoexpireCheck->SetCheckState(m_recordingRule->m_autoExpire);

    // Max Episodes
    m_maxepSpin->SetRange(0,100,1,5);
    m_maxepSpin->SetValue(m_recordingRule->m_maxEpisodes);

    // Max Episode Behaviour
    new MythUIButtonListItem(m_maxbehaviourList,
                             tr("Don't record if this would exceed the max "
                                "episodes"), qVariantFromValue(false));
    new MythUIButtonListItem(m_maxbehaviourList,
                             tr("Delete oldest if this would exceed the max "
                                "episodes"), qVariantFromValue(true));
    m_maxbehaviourList->SetValueByData(m_recordingRule->m_maxNewest);


    QHash<QString, QString> progMap;
    if (m_recInfo)
        m_recInfo->ToMap(progMap);
    else
        m_recordingRule->ToMap(progMap);
    SetTextFromMap(progMap);
}

void StoreOptEditor::maxEpChanged(MythUIButtonListItem *item)
{
    if (!item)
        return;

    if (item->GetData().toInt() == 0)
        m_maxbehaviourList->SetEnabled(false);
    else
        m_maxbehaviourList->SetEnabled(true);
}

void StoreOptEditor::PromptForRecgroup()
{
    if (m_recgroupList->GetDataValue().toString() != "__NEW_GROUP__")
        return;

    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");

    QString label = tr("Create New Recording Group. Enter group name: ");

    MythTextInputDialog *textDialog = new MythTextInputDialog(popupStack, label,
                         static_cast<InputFilter>(FilterSymbols | FilterPunct));

    textDialog->SetReturnEvent(this, "newrecgroup");

    if (textDialog->Create())
        popupStack->AddScreen(textDialog, false);
    return;
}

void StoreOptEditor::customEvent(QEvent *event)
{
    if (event->type() == DialogCompletionEvent::kEventType)
    {
        DialogCompletionEvent *dce = (DialogCompletionEvent*)(event);

        QString resultid   = dce->GetId();
        QString resulttext = dce->GetResultText();

        if (resultid == "newrecgroup")
        {
            if (!resulttext.isEmpty())
            {
                QString label = tr("Include in the \"%1\" recording group");
                MythUIButtonListItem *item =
                                    new MythUIButtonListItem(m_recgroupList,
                                                label.arg(resulttext),
                                                qVariantFromValue(resulttext));
                m_recgroupList->SetItemCurrent(item);
            }
            else
                m_recgroupList->SetValueByData(m_recordingRule->m_recGroup);
        }
    }
}

void StoreOptEditor::Save()
{
    // If the user has selected 'Create a new regroup' but failed to enter a
    // name when prompted, restore the original value
    if (m_recgroupList->GetDataValue().toString() == "__NEW_GROUP__")
        m_recgroupList->SetValueByData(m_recordingRule->m_recGroup);

    // Recording Profile
    m_recordingRule->m_recProfile = m_recprofileList->GetDataValue().toString();

    // Recording Group
    m_recordingRule->m_recGroup = m_recgroupList->GetDataValue().toString();

    // Storage Group
    m_recordingRule->m_storageGroup = m_storagegroupList->GetDataValue()
                                                                    .toString();

    // Playback Group
    m_recordingRule->m_playGroup = m_playgroupList->GetDataValue().toString();

    // Auto-Expire
    m_recordingRule->m_autoExpire = m_autoexpireCheck->GetBooleanCheckState();

    // Max Episodes
    m_recordingRule->m_maxEpisodes = m_maxepSpin->GetIntValue();

    // Max Episode Behaviour
    m_recordingRule->m_maxNewest = m_maxbehaviourList->GetDataValue().toBool();
}

void StoreOptEditor::Close()
{
    Save();
    MythScreenType::Close();
}

/////////////////////////////

/** \class PostProcEditor
 *  \brief Select post-processing options
 *
 */

PostProcEditor::PostProcEditor(MythScreenStack *parent,
                               RecordingInfo *recInfo,
                               RecordingRule *rule)
          : MythScreenType(parent, "PostProcOptionsEditor"),
            m_recInfo(NULL), m_recordingRule(rule),
            m_backButton(NULL),
            m_commflagCheck(NULL), m_transcodeCheck(NULL),
            m_transcodeprofileList(NULL), m_userjob1Check(NULL),
            m_userjob2Check(NULL), m_userjob3Check(NULL),
            m_userjob4Check(NULL)
{
    if (recInfo)
        m_recInfo = new RecordingInfo(*recInfo);
}

PostProcEditor::~PostProcEditor(void)
{
}

bool PostProcEditor::Create()
{
    if (!LoadWindowFromXML("schedule-ui.xml", "postproceditor", this))
        return false;

    bool err = false;

    UIUtilE::Assign(this, m_commflagCheck, "autocommflag", &err);
    UIUtilE::Assign(this, m_transcodeCheck, "autotranscode", &err);
    UIUtilE::Assign(this, m_transcodeprofileList, "transcodeprofile", &err);
    UIUtilE::Assign(this, m_userjob1Check, "userjob1", &err);
    UIUtilE::Assign(this, m_userjob2Check, "userjob2", &err);
    UIUtilE::Assign(this, m_userjob3Check, "userjob3", &err);
    UIUtilE::Assign(this, m_userjob4Check, "userjob4", &err);

    UIUtilE::Assign(this, m_backButton, "back", &err);

    if (err)
    {
        VERBOSE(VB_IMPORTANT, "PostProcEditor, theme is missing "
                              "required elements");
        return false;
    }

    connect(m_transcodeCheck, SIGNAL(toggled(bool)),
            SLOT(transcodeEnable(bool)));

    connect(m_backButton, SIGNAL(Clicked()), SLOT(Close()));

    BuildFocusList();

    return true;
}

void PostProcEditor::Load()
{
    // Auto-commflag
    m_commflagCheck->SetCheckState(m_recordingRule->m_autoCommFlag);

    // Auto-transcode
    m_transcodeCheck->SetCheckState(m_recordingRule->m_autoTranscode);

    // Transcode Method
    QMap<int, QString> profiles = RecordingProfile::listProfiles(
                                            RecordingProfile::TranscoderGroup);
    QMap<int, QString>::iterator it;
    for (it = profiles.begin(); it != profiles.end(); ++it)
    {
        new MythUIButtonListItem(m_transcodeprofileList, it.value(),
                                 qVariantFromValue(it.key()));
    }
    m_transcodeprofileList->SetValueByData(m_recordingRule->m_transcoder);

    // User Job #1
    m_userjob1Check->SetCheckState(m_recordingRule->m_autoUserJob1);
    MythUIText *userjob1Text = NULL;
    UIUtilW::Assign(this, userjob1Text, "userjob1text");
    if (userjob1Text)
        userjob1Text->SetText(tr("Run '%1'")
                    .arg(gCoreContext->GetSetting("UserJobDesc1"), "User Job 1"));

    // User Job #2
    m_userjob2Check->SetCheckState(m_recordingRule->m_autoUserJob2);
    MythUIText *userjob2Text = NULL;
    UIUtilW::Assign(this, userjob2Text, "userjob2text");
    if (userjob2Text)
        userjob2Text->SetText(tr("Run '%1'")
                    .arg(gCoreContext->GetSetting("UserJobDesc2", "User Job 2")));

    // User Job #3
    m_userjob3Check->SetCheckState(m_recordingRule->m_autoUserJob3);
    MythUIText *userjob3Text = NULL;
    UIUtilW::Assign(this, userjob3Text, "userjob3text");
    if (userjob3Text)
        userjob3Text->SetText(tr("Run '%1'")
                    .arg(gCoreContext->GetSetting("UserJobDesc3", "User Job 3")));

    // User Job #4
    m_userjob4Check->SetCheckState(m_recordingRule->m_autoUserJob4);
    MythUIText *userjob4Text = NULL;
    UIUtilW::Assign(this, userjob4Text, "userjob4text");
    if (userjob4Text)
        userjob4Text->SetText(tr("Run '%1'")
                    .arg(gCoreContext->GetSetting("UserJobDesc4", "User Job 4")));

    QHash<QString, QString> progMap;
    if (m_recInfo)
        m_recInfo->ToMap(progMap);
    else
        m_recordingRule->ToMap(progMap);
    SetTextFromMap(progMap);
}

void PostProcEditor::transcodeEnable(bool enable)
{
    m_transcodeprofileList->SetEnabled(enable);
}

void PostProcEditor::Save()
{
    // Auto-commflag
    m_recordingRule->m_autoCommFlag = m_commflagCheck->GetBooleanCheckState();

    // Auto-transcode
    m_recordingRule->m_autoTranscode = m_transcodeCheck->GetBooleanCheckState();

    // Transcode Method
    m_recordingRule->m_transcoder = m_transcodeprofileList->GetDataValue().toInt();

    // User Job #1
    m_recordingRule->m_autoUserJob1 = m_userjob1Check->GetBooleanCheckState();

    // User Job #2
    m_recordingRule->m_autoUserJob2 = m_userjob2Check->GetBooleanCheckState();

    // User Job #3
    m_recordingRule->m_autoUserJob3 = m_userjob3Check->GetBooleanCheckState();

    // User Job #4
    m_recordingRule->m_autoUserJob4 = m_userjob4Check->GetBooleanCheckState();
}

void PostProcEditor::Close()
{
    Save();
    MythScreenType::Close();
}
