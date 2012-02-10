
#include "scheduleeditor.h"

// QT
#include <QString>
#include <QHash>
#include <QCoreApplication>

// Libmyth
#include "mythcorecontext.h"
#include "storagegroup.h"
#include "programtypes.h"

// Libmythtv
#include "playgroup.h"
#include "tv_play.h"
#include "recordingprofile.h"
#include "cardutil.h"

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
#include "mythprogressdialog.h"
#include "mythuifilebrowser.h"
#include "mythuimetadataresults.h"
#include "mythuiimageresults.h"
#include "videoutils.h"
#include "mythuiutils.h"

#include "metadataimagehelper.h"

// Mythfrontend
#include "proglist.h"
#include "viewschedulediff.h"

#define ENUM_TO_QVARIANT(a) qVariantFromValue(static_cast<int>(a))

// Define the strings inserted into the recordfilter table in the
// database.  This should make them available to the translators.
static QString fs0(QT_TRANSLATE_NOOP("SchedFilterEditor", "New episode"));
static QString fs1(QT_TRANSLATE_NOOP("SchedFilterEditor", "Identifiable episode"));
static QString fs2(QT_TRANSLATE_NOOP("SchedFilterEditor", "First showing"));
static QString fs3(QT_TRANSLATE_NOOP("SchedFilterEditor", "Prime time"));
static QString fs4(QT_TRANSLATE_NOOP("SchedFilterEditor", "Commercial free"));
static QString fs5(QT_TRANSLATE_NOOP("SchedFilterEditor", "High definition"));
static QString fs6(QT_TRANSLATE_NOOP("SchedFilterEditor", "This episode"));
static QString fs7(QT_TRANSLATE_NOOP("SchedFilterEditor", "This series"));

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
            m_previewButton(NULL), m_metadataButton(NULL),
            m_player(player)
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
            m_previewButton(NULL), m_metadataButton(NULL),
            m_player(player)
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
    UIUtilE::Assign(this, m_metadataButton, "metadata", &err);

    UIUtilE::Assign(this, m_cancelButton, "cancel", &err);
    UIUtilE::Assign(this, m_saveButton, "save", &err);

    if (err)
    {
        LOG(VB_GENERAL, LOG_ERR, "ScheduleEditor, theme is missing "
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
    connect(m_metadataButton, SIGNAL(Clicked()), SLOT(ShowMetadataOptions()));

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
            LOG(VB_GENERAL, LOG_ERR,
                "ScheduleEditor::Create() - Failed to load recording rule");
            return false;
        }
    }

    if (m_player)
        m_player->StartEmbedding(QRect());

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

    InfoMap progMap;
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

    ShowUpcoming(title, m_recordingRule->m_seriesid);
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

void ScheduleEditor::ShowMetadataOptions(void)
{
    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
    MetadataOptions *rad = new MetadataOptions(mainStack, m_recInfo,
                                                    m_recordingRule);
    if (rad->Create())
        mainStack->AddScreen(rad);
    else
        delete rad;
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
      m_filtersButton(NULL), m_ruleactiveCheck(NULL)
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

    UIUtilW::Assign(this, m_filtersButton, "filters");
    UIUtilW::Assign(this, m_backButton, "back");

    UIUtilE::Assign(this, m_ruleactiveCheck, "ruleactive", &err);

    if (err)
    {
        LOG(VB_GENERAL, LOG_ERR, "SchedOptEditor, theme is missing "
                                 "required elements");
        return false;
    }

    if (m_backButton)
        connect(m_backButton, SIGNAL(Clicked()), SLOT(Close()));

    if (m_filtersButton && m_recordingRule->m_type == kOverrideRecord)
        m_filtersButton->SetEnabled(false);
    if (m_recordingRule->m_type == kSingleRecord ||
        m_recordingRule->m_type == kOverrideRecord)
    {
        m_dupmethodList->SetEnabled(false);
        m_dupscopeList->SetEnabled(false);
    }

    connect(m_dupmethodList, SIGNAL(itemSelected(MythUIButtonListItem *)),
            SLOT(dupMatchChanged(MythUIButtonListItem *)));

    if (m_filtersButton)
        connect(m_filtersButton, SIGNAL(Clicked()), SLOT(ShowFilters()));

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

    vector<uint> inputids = CardUtil::GetInputIDs(0);
    for (uint i = 0; i < inputids.size(); ++i)
    {
        new MythUIButtonListItem(m_inputList, tr("Prefer input %1")
                                 .arg(CardUtil::GetDisplayName(inputids[i])),
                                 inputids[i]);
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

    if (gCoreContext->GetNumSetting("HaveRepeats", 0))
    {
        new MythUIButtonListItem(m_dupscopeList,
                                 tr("Record new episodes only"),
                                 ENUM_TO_QVARIANT(kDupsNewEpi | kDupsInAll));
    }

    m_dupscopeList->SetValueByData(ENUM_TO_QVARIANT(m_recordingRule->m_dupIn));

    // Active/Disabled
    m_ruleactiveCheck->SetCheckState(!m_recordingRule->m_isInactive);

    InfoMap progMap;
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

void SchedOptEditor::ShowFilters(void)
{
    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
    SchedFilterEditor *schedfilteredit = new SchedFilterEditor(mainStack,
                                                               m_recInfo,
                                                               m_recordingRule);
    if (schedfilteredit->Create())
        mainStack->AddScreen(schedfilteredit);
    else
        delete schedfilteredit;
}

////////////////////////////////////////////////////////

/** \class SchedFilterEditor
 *  \brief Select schedule filters
 *
 */

SchedFilterEditor::SchedFilterEditor(MythScreenStack *parent,
                                     RecordingInfo *recInfo,
                                     RecordingRule *rule)
    : MythScreenType(parent, "ScheduleFilterEditor"),
      m_recInfo(NULL), m_recordingRule(rule),
      m_backButton(NULL), m_filtersList(NULL)
{
    if (recInfo)
        m_recInfo = new RecordingInfo(*recInfo);
}

SchedFilterEditor::~SchedFilterEditor(void)
{
}

bool SchedFilterEditor::Create()
{
    if (!LoadWindowFromXML("schedule-ui.xml", "schedulefiltereditor", this))
        return false;

    bool err = false;

    UIUtilE::Assign(this, m_filtersList, "filters", &err);
    UIUtilW::Assign(this, m_backButton, "back");

    if (err)
    {
        LOG(VB_GENERAL, LOG_ERR, "SchedFilterEditor, theme is missing "
                                 "required elements");
        return false;
    }

    if (m_backButton)
        connect(m_backButton, SIGNAL(Clicked()), SLOT(Close()));

    connect(m_filtersList, SIGNAL(itemClicked(MythUIButtonListItem *)),
            SLOT(ToggleSelected(MythUIButtonListItem *)));
    BuildFocusList();

    return true;
}

void SchedFilterEditor::Load()
{
    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("SELECT filterid, description, newruledefault "
                  "FROM recordfilter ORDER BY filterid");

    if (query.exec())
    {
        MythUIButtonListItem *button;

        while (query.next())
        {
            uint32_t filterid       = query.value(0).toInt();
            QString  description    = tr(query.value(1).toString()
                                         .toUtf8().constData());
            // bool     filter_default = query.value(2).toInt();

            // Fill in list of possible filters
            button = new MythUIButtonListItem(m_filtersList, description,
                                              filterid);
            button->setCheckable(true);
            button->setChecked(m_recordingRule->m_filter & (1 << filterid) ?
                               MythUIButtonListItem::FullChecked :
                               MythUIButtonListItem::NotChecked);
        }
    }

    InfoMap progMap;
    if (m_recInfo)
        m_recInfo->ToMap(progMap);
    else
        m_recordingRule->ToMap(progMap);
    SetTextFromMap(progMap);
}

void SchedFilterEditor::ToggleSelected(MythUIButtonListItem *item)
{
    item->setChecked(item->state() == MythUIButtonListItem::FullChecked ?
                     MythUIButtonListItem::NotChecked :
                     MythUIButtonListItem::FullChecked);
}

void SchedFilterEditor::Save()
{
    // Iterate through button list, and build the mask
    MythUIButtonListItem *button;
    uint32_t filter_mask = 0;
    int idx, end;

    end = m_filtersList->GetCount();
    for (idx = 0; idx < end; ++idx)
    {
        if ((button = m_filtersList->GetItemAt(idx)) &&
            button->state() == MythUIButtonListItem::FullChecked)
            filter_mask |= (1 << qVariantValue<uint32_t>(button->GetData()));
    }
    m_recordingRule->m_filter = filter_mask;
}

void SchedFilterEditor::Close()
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
    UIUtilW::Assign(this, m_backButton, "back");

    if (err)
    {
        LOG(VB_GENERAL, LOG_ERR, "StoreOptEditor, theme is missing "
                                 "required elements");
        return false;
    }

    if (m_backButton)
        connect(m_backButton, SIGNAL(Clicked()), SLOT(Close()));

    connect(m_maxepSpin, SIGNAL(itemSelected(MythUIButtonListItem *)),
                         SLOT(maxEpChanged(MythUIButtonListItem *)));

    connect(m_recgroupList, SIGNAL(LosingFocus()),
                            SLOT(PromptForRecgroup()));

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

            if (value != "Deleted")
            {
                groups += value;
                if (value == "Default")
                    foundDefault = true;
            }
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
    groups.removeDuplicates();
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
            dispValue = tr("Live TV");
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


    InfoMap progMap;
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
            m_userjob4Check(NULL), m_metadataLookupCheck(NULL)
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
    UIUtilW::Assign(this, m_metadataLookupCheck, "metadatalookup");
    UIUtilW::Assign(this, m_backButton, "back");

    if (err)
    {
        LOG(VB_GENERAL, LOG_ERR, "PostProcEditor, theme is missing "
                                 "required elements");
        return false;
    }

    if (m_backButton)
        connect(m_backButton, SIGNAL(Clicked()), SLOT(Close()));

    connect(m_transcodeCheck, SIGNAL(toggled(bool)),
            SLOT(transcodeEnable(bool)));

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

    // Auto Metadata Lookup
    if (m_metadataLookupCheck)
        m_metadataLookupCheck->SetCheckState(m_recordingRule->m_autoMetadataLookup);

    InfoMap progMap;
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

    // Auto Metadata Lookup
    if (m_metadataLookupCheck)
        m_recordingRule->m_autoMetadataLookup = m_metadataLookupCheck->GetBooleanCheckState();
}

void PostProcEditor::Close()
{
    Save();
    MythScreenType::Close();
}

/////////////////////////////

/** \class MetadataOptions
 *  \brief Select artwork and inetref for recordings
 *
 */

MetadataOptions::MetadataOptions(MythScreenStack *parent,
                               RecordingInfo *recInfo,
                               RecordingRule *rule)
          : MythScreenType(parent, "MetadataOptions"),
            m_recInfo(NULL), m_recordingRule(rule), m_lookup(NULL),
            m_busyPopup(NULL),  m_fanart(NULL), m_coverart(NULL),
            m_banner(NULL), m_inetrefEdit(NULL), m_seasonSpin(NULL),
            m_episodeSpin(NULL), m_queryButton(NULL), m_localFanartButton(NULL),
            m_localCoverartButton(NULL), m_localBannerButton(NULL),
            m_onlineFanartButton(NULL), m_onlineCoverartButton(NULL),
            m_onlineBannerButton(NULL), m_backButton(NULL)
{
    m_popupStack = GetMythMainWindow()->GetStack("popup stack");

    m_metadataFactory = new MetadataFactory(this);
    m_imageLookup = new MetadataDownload(this);
    m_imageDownload = new MetadataImageDownload(this);

    m_artworkMap = GetArtwork(m_recordingRule->m_inetref,
                              m_recordingRule->m_season);

    if (recInfo)
        m_recInfo = new RecordingInfo(*recInfo);
}

MetadataOptions::~MetadataOptions(void)
{
    if (m_imageLookup)
    {
        m_imageLookup->cancel();
        delete m_imageLookup;
        m_imageLookup = NULL;
    }

    if (m_imageDownload)
    {
        m_imageDownload->cancel();
        delete m_imageDownload;
        m_imageDownload = NULL;
    }
}

bool MetadataOptions::Create()
{
    if (!LoadWindowFromXML("schedule-ui.xml", "metadataoptions", this))
        return false;

    bool err = false;

    UIUtilE::Assign(this, m_inetrefEdit, "inetref_edit", &err);
    UIUtilE::Assign(this, m_seasonSpin, "season_spinbox", &err);
    UIUtilE::Assign(this, m_episodeSpin, "episode_spinbox", &err);
    UIUtilE::Assign(this, m_queryButton, "query_button", &err);
    UIUtilE::Assign(this, m_localFanartButton, "local_fanart_button", &err);
    UIUtilE::Assign(this, m_localCoverartButton, "local_coverart_button", &err);
    UIUtilE::Assign(this, m_localBannerButton, "local_banner_button", &err);
    UIUtilE::Assign(this, m_onlineFanartButton, "online_fanart_button", &err);
    UIUtilE::Assign(this, m_onlineCoverartButton, "online_coverart_button", &err);
    UIUtilE::Assign(this, m_onlineBannerButton, "online_banner_button", &err);
    UIUtilW::Assign(this, m_fanart, "fanart");
    UIUtilW::Assign(this, m_coverart, "coverart");
    UIUtilW::Assign(this, m_banner, "banner");
    UIUtilW::Assign(this, m_backButton, "back");

    if (err)
    {
        LOG(VB_GENERAL, LOG_ERR, "MetadataOptions, theme is missing "
                                 "required elements");
        return false;
    }

    if (m_backButton)
        connect(m_backButton, SIGNAL(Clicked()), SLOT(Close()));
    connect(m_queryButton, SIGNAL(Clicked()),
            SLOT(PerformQuery()));
    connect(m_localFanartButton, SIGNAL(Clicked()),
            SLOT(SelectLocalFanart()));
    connect(m_localCoverartButton, SIGNAL(Clicked()),
            SLOT(SelectLocalCoverart()));
    connect(m_localBannerButton, SIGNAL(Clicked()),
            SLOT(SelectLocalBanner()));
    connect(m_onlineFanartButton, SIGNAL(Clicked()),
            SLOT(SelectOnlineFanart()));
    connect(m_onlineCoverartButton, SIGNAL(Clicked()),
            SLOT(SelectOnlineCoverart()));
    connect(m_onlineBannerButton, SIGNAL(Clicked()),
            SLOT(SelectOnlineBanner()));

    connect(m_seasonSpin, SIGNAL(itemSelected(MythUIButtonListItem*)),
                          SLOT(ValuesChanged()));

    // InetRef
    m_inetrefEdit->SetText(m_recordingRule->m_inetref);

    // Season
    m_seasonSpin->SetRange(0,9999,1,1);
    m_seasonSpin->SetValue(m_recordingRule->m_season);

    // Episode
    m_episodeSpin->SetRange(0,9999,1,1);
    m_episodeSpin->SetValue(m_recordingRule->m_episode);

    if (m_coverart)
    {
        m_coverart->SetFilename(m_artworkMap.value(kArtworkCoverart).url);
        m_coverart->Load();
    }

    if (m_fanart)
    {
        m_fanart->SetFilename(m_artworkMap.value(kArtworkFanart).url);
        m_fanart->Load();
    }

    if (m_banner)
    {
        m_banner->SetFilename(m_artworkMap.value(kArtworkBanner).url);
        m_banner->Load();
    }

    BuildFocusList();

    return true;
}

void MetadataOptions::Load()
{
    if (m_recordingRule->m_inetref.isEmpty())
    {
        CreateBusyDialog("Trying to automatically find this "
                     "recording online...");

        m_metadataFactory->Lookup(m_recordingRule, false, false);
    }

    InfoMap progMap;
    if (m_recInfo)
        m_recInfo->ToMap(progMap);
    else
        m_recordingRule->ToMap(progMap);
    SetTextFromMap(progMap);
}

void MetadataOptions::CreateBusyDialog(QString title)
{
    if (m_busyPopup)
        return;

    QString message = title;

    m_busyPopup = new MythUIBusyDialog(message, m_popupStack,
            "metaoptsdialog");

    if (m_busyPopup->Create())
        m_popupStack->AddScreen(m_busyPopup);
}

void MetadataOptions::PerformQuery()
{
    m_lookup = new MetadataLookup();

    CreateBusyDialog("Trying to manually find this "
                     "recording online...");

    m_lookup->SetStep(kLookupSearch);
    m_lookup->SetType(kMetadataRecording);
    if (m_seasonSpin->GetIntValue() > 0 ||
           m_episodeSpin->GetIntValue() > 0)
        m_lookup->SetSubtype(kProbableTelevision);
    else
        m_lookup->SetSubtype(kProbableMovie);
    m_lookup->SetAutomatic(false);
    m_lookup->SetHandleImages(false);
    m_lookup->SetHost(gCoreContext->GetMasterHostName());
    m_lookup->SetTitle(m_recordingRule->m_title);
    m_lookup->SetSubtitle(m_recordingRule->m_subtitle);
    m_lookup->SetInetref(m_inetrefEdit->GetText());
    m_lookup->SetSeason(m_seasonSpin->GetIntValue());
    m_lookup->SetEpisode(m_episodeSpin->GetIntValue());

    m_metadataFactory->Lookup(m_lookup);
}

void MetadataOptions::OnSearchListSelection(MetadataLookup *lookup)
{
    if (!lookup)
        return;

    m_lookup = lookup;

    m_metadataFactory->Lookup(lookup);
}

void MetadataOptions::OnImageSearchListSelection(ArtworkInfo info,
                                                 VideoArtworkType type)
{
    QString msg = tr("Downloading selected artwork...");
    CreateBusyDialog(msg);

    m_lookup = new MetadataLookup();
    m_lookup->SetType(kMetadataVideo);
    m_lookup->SetHost(gCoreContext->GetMasterHostName());
    m_lookup->SetAutomatic(true);
    m_lookup->SetData(qVariantFromValue<VideoArtworkType>(type));

    ArtworkMap downloads;
    downloads.insert(type, info);
    m_lookup->SetDownloads(downloads);
    m_lookup->SetAllowOverwrites(true);
    m_lookup->SetTitle(m_recordingRule->m_title);
    m_lookup->SetSubtitle(m_recordingRule->m_subtitle);
    m_lookup->SetInetref(m_inetrefEdit->GetText());
    m_lookup->SetSeason(m_seasonSpin->GetIntValue());
    m_lookup->SetEpisode(m_episodeSpin->GetIntValue());

    m_imageDownload->addDownloads(m_lookup);
}

void MetadataOptions::SelectLocalFanart()
{
    if (!CanSetArtwork())
        return;

    QString url = generate_file_url("Fanart",
                  gCoreContext->GetMasterHostName(),
                  "");
    FindImagePopup(url,"",*this, "fanart");
}

void MetadataOptions::SelectLocalCoverart()
{
    if (!CanSetArtwork())
        return;

    QString url = generate_file_url("Coverart",
                  gCoreContext->GetMasterHostName(),
                  "");
    FindImagePopup(url,"",*this, "coverart");
}

void MetadataOptions::SelectLocalBanner()
{
    if (!CanSetArtwork())
        return;

    QString url = generate_file_url("Banners",
                  gCoreContext->GetMasterHostName(),
                  "");
    FindImagePopup(url,"",*this, "banner");
}

void MetadataOptions::SelectOnlineFanart()
{
    FindNetArt(kArtworkFanart);
}

void MetadataOptions::SelectOnlineCoverart()
{
    FindNetArt(kArtworkCoverart);
}

void MetadataOptions::SelectOnlineBanner()
{
    FindNetArt(kArtworkBanner);
}

void MetadataOptions::Close()
{
    Save();
    MythScreenType::Close();
}

void MetadataOptions::Save()
{
    // Season
    if (m_seasonSpin)
        m_recordingRule->m_season = m_seasonSpin->GetIntValue();

    // Episode
    if (m_episodeSpin)
        m_recordingRule->m_episode = m_episodeSpin->GetIntValue();

    // InetRef
    if (m_inetrefEdit)
        m_recordingRule->m_inetref = m_inetrefEdit->GetText();
}

void MetadataOptions::QueryComplete(MetadataLookup *lookup)
{
    if (!lookup)
        return;

    m_lookup = lookup;

    // InetRef
    m_inetrefEdit->SetText(m_lookup->GetInetref());

    // Season
    m_seasonSpin->SetValue(m_lookup->GetSeason());

    // Episode
    m_episodeSpin->SetValue(m_lookup->GetEpisode());

    MetadataMap metadataMap;
    lookup->toMap(metadataMap);
    SetTextFromMap(metadataMap);
}

void MetadataOptions::FindImagePopup(const QString &prefix,
                                     const QString &prefixAlt,
                                     QObject &inst,
                                     const QString &returnEvent)
{
    QString fp;

    if (prefix.startsWith("myth://"))
        fp = prefix;
    else
        fp = prefix.isEmpty() ? prefixAlt : prefix;

    MythScreenStack *popupStack =
            GetMythMainWindow()->GetStack("popup stack");

    MythUIFileBrowser *fb = new MythUIFileBrowser(popupStack, fp);
    fb->SetNameFilter(GetSupportedImageExtensionFilter());
    if (fb->Create())
    {
        fb->SetReturnEvent(&inst, returnEvent);
        popupStack->AddScreen(fb);
    }
    else
        delete fb;
}

QStringList MetadataOptions::GetSupportedImageExtensionFilter()
{
    QStringList ret;

    QList<QByteArray> exts = QImageReader::supportedImageFormats();
    for (QList<QByteArray>::iterator p = exts.begin(); p != exts.end(); ++p)
    {
        ret.append(QString("*.").append(*p));
    }

    return ret;
}

bool MetadataOptions::CanSetArtwork()
{
    if (m_inetrefEdit->GetText().isEmpty())
    {
        ShowOkPopup(tr("You must set a reference number "
               "on this rule to set artwork.  For items "
               "without a metadata source, you can set "
               "any unique value."));
        return false;
    }

    return true;
}

void MetadataOptions::FindNetArt(VideoArtworkType type)
{
    if (!CanSetArtwork())
        return;

    m_lookup = new MetadataLookup();

    QString msg = tr("Searching for available artwork...");
    CreateBusyDialog(msg);

    m_lookup->SetStep(kLookupSearch);
    m_lookup->SetType(kMetadataVideo);
    m_lookup->SetAutomatic(true);
    m_lookup->SetHandleImages(false);
    if (m_seasonSpin->GetIntValue() > 0 ||
           m_episodeSpin->GetIntValue() > 0)
        m_lookup->SetSubtype(kProbableTelevision);
    else
        m_lookup->SetSubtype(kProbableMovie);
    m_lookup->SetData(qVariantFromValue<VideoArtworkType>(type));
    m_lookup->SetHost(gCoreContext->GetMasterHostName());
    m_lookup->SetTitle(m_recordingRule->m_title);
    m_lookup->SetSubtitle(m_recordingRule->m_subtitle);
    m_lookup->SetInetref(m_inetrefEdit->GetText());
    m_lookup->SetSeason(m_seasonSpin->GetIntValue());
    m_lookup->SetEpisode(m_episodeSpin->GetIntValue());

    m_imageLookup->addLookup(m_lookup);
}

void MetadataOptions::OnArtworkSearchDone(MetadataLookup *lookup)
{
    if (!lookup)
        return;

    if (m_busyPopup)
    {
        m_busyPopup->Close();
        m_busyPopup = NULL;
    }

    VideoArtworkType type = qVariantValue<VideoArtworkType>(lookup->GetData());
    ArtworkList list = lookup->GetArtwork(type);

    if (list.count() == 0)
        return;

    ImageSearchResultsDialog *resultsdialog =
          new ImageSearchResultsDialog(m_popupStack, list, type);

    connect(resultsdialog, SIGNAL(haveResult(ArtworkInfo, VideoArtworkType)),
            SLOT(OnImageSearchListSelection(ArtworkInfo, VideoArtworkType)));

    if (resultsdialog->Create())
        m_popupStack->AddScreen(resultsdialog);
}

void MetadataOptions::HandleDownloadedImages(MetadataLookup *lookup)
{
    if (!lookup)
        return;

    DownloadMap map = lookup->GetDownloads();

    if (!map.size())
        return;

    for (DownloadMap::const_iterator i = map.begin(); i != map.end(); ++i)
    {
        VideoArtworkType type = i.key();
        ArtworkInfo info = i.value();

        if (type == kArtworkCoverart)
            m_artworkMap.replace(kArtworkCoverart, info);
        else if (type == kArtworkFanart)
            m_artworkMap.replace(kArtworkFanart, info);
        else if (type == kArtworkBanner)
            m_artworkMap.replace(kArtworkBanner, info);
    }

    SetArtwork(m_inetrefEdit->GetText(), m_seasonSpin->GetIntValue(),
               gCoreContext->GetMasterHostName(), m_artworkMap);

    ValuesChanged();
}

void MetadataOptions::ValuesChanged()
{
    m_artworkMap = GetArtwork(m_inetrefEdit->GetText(),
                              m_seasonSpin->GetIntValue());

    if (m_coverart)
    {
        m_coverart->SetFilename(m_artworkMap.value(kArtworkCoverart).url);
        m_coverart->Load();
    }

    if (m_fanart)
    {
        m_fanart->SetFilename(m_artworkMap.value(kArtworkFanart).url);
        m_fanart->Load();
    }

    if (m_banner)
    {
        m_banner->SetFilename(m_artworkMap.value(kArtworkBanner).url);
        m_banner->Load();
    }
}

void MetadataOptions::customEvent(QEvent *levent)
{
    if (levent->type() == MetadataFactoryMultiResult::kEventType)
    {
        if (m_busyPopup)
        {
            m_busyPopup->Close();
            m_busyPopup = NULL;
        }

        MetadataFactoryMultiResult *mfmr = dynamic_cast<MetadataFactoryMultiResult*>(levent);

        if (!mfmr)
            return;

        MetadataLookupList list = mfmr->results;

        if (list.count() > 1)
        {
            int yearindex = -1;

            for (int p = 0; p != list.size(); ++p)
            {
                if (!m_recordingRule->m_seriesid.isEmpty() &&
                    m_recordingRule->m_seriesid == (list[p])->GetTMSref())
                {
                    MetadataLookup *lookup = list.takeAt(p);
                    QueryComplete(lookup);
                    qDeleteAll(list);
                    return;
                }
                else if (m_recInfo &&
                         m_recInfo->GetYearOfInitialRelease() != 0 &&
                         (list[p])->GetYear() != 0 &&
                         m_recInfo->GetYearOfInitialRelease() == (list[p])->GetYear())
                {
                    if (yearindex > -1)
                    {
                        LOG(VB_GENERAL, LOG_INFO, "Multiple results matched on year. No definite "
                                      "match could be found based on year alone.");
                        yearindex = -2;
                    }
                    else if (yearindex == -1)
                    {
                        LOG(VB_GENERAL, LOG_INFO, "Matched based on year. ");
                        yearindex = p;
                    }
                }
            }

            if (yearindex > -1)
            {
                MetadataLookup *lookup = list.takeAt(yearindex);
                QueryComplete(lookup);
                qDeleteAll(list);
                return;
            }

            LOG(VB_GENERAL, LOG_INFO, "Falling through to selection dialog.");
            MetadataResultsDialog *resultsdialog =
                  new MetadataResultsDialog(m_popupStack, list);

            connect(resultsdialog, SIGNAL(haveResult(MetadataLookup*)),
                    SLOT(OnSearchListSelection(MetadataLookup*)),
                    Qt::QueuedConnection);

            if (resultsdialog->Create())
                m_popupStack->AddScreen(resultsdialog);
        }
    }
    else if (levent->type() == MetadataFactorySingleResult::kEventType)
    {
        if (m_busyPopup)
        {
            m_busyPopup->Close();
            m_busyPopup = NULL;
        }

        MetadataFactorySingleResult *mfsr = dynamic_cast<MetadataFactorySingleResult*>(levent);

        if (!mfsr)
            return;

        MetadataLookup *lookup = mfsr->result;

        if (!lookup)
            return;

        QueryComplete(lookup);
    }
    else if (levent->type() == MetadataFactoryNoResult::kEventType)
    {
        if (m_busyPopup)
        {
            m_busyPopup->Close();
            m_busyPopup = NULL;
        }

        MetadataFactoryNoResult *mfnr = dynamic_cast<MetadataFactoryNoResult*>(levent);

        if (!mfnr)
            return;

        MetadataLookup *lookup = mfnr->result;

        delete lookup;
        lookup = NULL;

        QString title = "No match found for this recording. You can "
                        "try entering a TVDB/TMDB number, season, and "
                        "episode manually.";

        MythConfirmationDialog *okPopup =
                new MythConfirmationDialog(m_popupStack, title, false);

        if (okPopup->Create())
            m_popupStack->AddScreen(okPopup);
    }
    else if (levent->type() == MetadataLookupEvent::kEventType)
    {
        if (m_busyPopup)
        {
            m_busyPopup->Close();
            m_busyPopup = NULL;
        }

        MetadataLookupEvent *lue = (MetadataLookupEvent *)levent;

        MetadataLookupList lul = lue->lookupList;

        if (lul.isEmpty())
            return;

        if (lul.count() >= 1)
        {
            OnArtworkSearchDone(lul.takeFirst());
        }
    }
    else if (levent->type() == MetadataLookupFailure::kEventType)
    {
        if (m_busyPopup)
        {
            m_busyPopup->Close();
            m_busyPopup = NULL;
        }

        MetadataLookupFailure *luf = (MetadataLookupFailure *)levent;

        MetadataLookupList lul = luf->lookupList;

        if (lul.size())
        {
            MetadataLookup *lookup = lul.takeFirst();

            QString title = "This number, season, and episode combination "
                            "does not appear to be valid (or the site may "
                            "be down).  Check your information and try again.";

            MythConfirmationDialog *okPopup =
                    new MythConfirmationDialog(m_popupStack, title, false);

            if (okPopup->Create())
                m_popupStack->AddScreen(okPopup);

            delete lookup;
            lookup = NULL;
        }
    }
    else if (levent->type() == ImageDLEvent::kEventType)
    {
        if (m_busyPopup)
        {
            m_busyPopup->Close();
            m_busyPopup = NULL;
        }

        ImageDLEvent *ide = (ImageDLEvent *)levent;

        MetadataLookup *lookup = ide->item;

        if (!lookup)
            return;

        HandleDownloadedImages(lookup);
    }
    else if (levent->type() == ImageDLFailureEvent::kEventType)
    {
        if (m_busyPopup)
        {
            m_busyPopup->Close();
            m_busyPopup = NULL;
        }

        ImageDLFailureEvent *ide = (ImageDLFailureEvent *)levent;

        MetadataLookup *lookup = ide->item;

        if (!lookup)
            return;

        delete lookup;
        lookup = NULL;
    }
    else if (levent->type() == DialogCompletionEvent::kEventType)
    {
        DialogCompletionEvent *dce = (DialogCompletionEvent*)(levent);

        const QString resultid = dce->GetId();
        ArtworkInfo info;
        info.url = dce->GetResultText();

        if (resultid == "coverart")
        {
            m_artworkMap.replace(kArtworkCoverart, info);
        }
        else if (resultid == "fanart")
        {
            m_artworkMap.replace(kArtworkFanart, info);
        }
        else if (resultid == "banner")
        {
            m_artworkMap.replace(kArtworkBanner, info);
        }

        SetArtwork(m_inetrefEdit->GetText(), m_seasonSpin->GetIntValue(),
               gCoreContext->GetMasterHostName(), m_artworkMap);

        ValuesChanged();
    }

}

