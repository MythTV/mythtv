// C++
#include <utility>

// Qt
#include <QCoreApplication>
#include <QHash>
#include <QString>

// MythTV
#include "libmythbase/mythcorecontext.h"
#include "libmythbase/mythtypes.h"
#include "libmythbase/programtypes.h"
#include "libmythbase/recordingtypes.h"
#include "libmythbase/storagegroup.h"
#include "libmythmetadata/mythuiimageresults.h"
#include "libmythmetadata/mythuimetadataresults.h"
#include "libmythmetadata/videoutils.h"
#include "libmythtv/cardutil.h"
#include "libmythtv/metadataimagehelper.h"
#include "libmythtv/playgroup.h"
#include "libmythtv/recordingprofile.h"
#include "libmythtv/tv_play.h"
#include "libmythui/mythdialogbox.h"
#include "libmythui/mythmainwindow.h"
#include "libmythui/mythprogressdialog.h"
#include "libmythui/mythuibutton.h"
#include "libmythui/mythuibuttonlist.h"
#include "libmythui/mythuicheckbox.h"
#include "libmythui/mythuifilebrowser.h"
#include "libmythui/mythuihelper.h"
#include "libmythui/mythuiimage.h"
#include "libmythui/mythuispinbox.h"
#include "libmythui/mythuistatetype.h"
#include "libmythui/mythuitext.h"
#include "libmythui/mythuiutils.h"

// MythFrontend
#include "proglist.h"
#include "scheduleeditor.h"
#include "viewschedulediff.h"

//static const QString _Location = QObject::tr("Schedule Editor");

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
static QString fs8(QT_TRANSLATE_NOOP("SchedFilterEditor", "This time"));
static QString fs9(QT_TRANSLATE_NOOP("SchedFilterEditor", "This day and time"));
static QString fs10(QT_TRANSLATE_NOOP("SchedFilterEditor", "This channel"));
static QString fs11(QT_TRANSLATE_NOOP("SchedFilterEditor", "No episodes"));

void *ScheduleEditor::RunScheduleEditor(ProgramInfo *proginfo, void *player)
{
    auto *rule = new RecordingRule();
    rule->LoadByProgram(proginfo);

    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
    auto *se = new ScheduleEditor(mainStack, rule, static_cast<TV*>(player));

    if (se->Create())
        mainStack->AddScreen(se, (player == nullptr));
    else
        delete se;

    return nullptr;
}

/** \class ScheduleEditor
 *  \brief Construct a recording schedule
 *
 */

ScheduleEditor::ScheduleEditor(MythScreenStack *parent,
                               RecordingInfo *recInfo, TV *player)
          : ScheduleCommon(parent, "ScheduleEditor"),
            SchedOptMixin(*this, nullptr), FilterOptMixin(*this, nullptr),
            StoreOptMixin(*this, nullptr), PostProcMixin(*this, nullptr),
            m_recInfo(new RecordingInfo(*recInfo)),
            m_player(player)
{
    m_recordingRule = new RecordingRule();
    m_recordingRule->m_recordID = m_recInfo->GetRecordingRuleID();
    SchedOptMixin::SetRule(m_recordingRule);
    FilterOptMixin::SetRule(m_recordingRule);
    StoreOptMixin::SetRule(m_recordingRule);
    PostProcMixin::SetRule(m_recordingRule);

    if (m_player)
        m_player->IncrRef();
}

ScheduleEditor::ScheduleEditor(MythScreenStack *parent,
                               RecordingRule *recRule, TV *player)
          : ScheduleCommon(parent, "ScheduleEditor"),
            SchedOptMixin(*this, recRule),
            FilterOptMixin(*this, recRule),
            StoreOptMixin(*this, recRule),
            PostProcMixin(*this, recRule),
            m_recordingRule(recRule),
            m_player(player)
{
    if (m_player)
        m_player->IncrRef();
}

ScheduleEditor::~ScheduleEditor(void)
{
    delete m_recordingRule;

    // if we have a player, we need to tell we are done
    if (m_player)
    {
        emit m_player->RequestEmbedding(false);
        m_player->DecrRef();
    }
}

bool ScheduleEditor::Create()
{
    if (!LoadWindowFromXML("schedule-ui.xml", "scheduleeditor", this))
        return false;

    bool err = false;

    UIUtilE::Assign(this, m_rulesList, "rules", &err);

    UIUtilW::Assign(this, m_schedOptButton, "schedoptions");
    UIUtilW::Assign(this, m_storeOptButton, "storeoptions");
    UIUtilW::Assign(this, m_postProcButton, "postprocessing");
    UIUtilW::Assign(this, m_metadataButton, "metadata");
    UIUtilW::Assign(this, m_schedInfoButton, "schedinfo");
    UIUtilW::Assign(this, m_previewButton, "preview");
    UIUtilW::Assign(this, m_filtersButton, "filters");

    SchedOptMixin::Create(&err);
    FilterOptMixin::Create(&err);
    StoreOptMixin::Create(&err);
    PostProcMixin::Create(&err);

    UIUtilW::Assign(this, m_cancelButton, "cancel");
    UIUtilE::Assign(this, m_saveButton, "save", &err);

    if (err)
    {
        LOG(VB_GENERAL, LOG_ERR, "ScheduleEditor, theme is missing "
                                 "required elements");
        return false;
    }

    connect(m_rulesList, &MythUIButtonList::itemSelected,
            this,        &ScheduleEditor::RuleChanged);

    if (m_schedOptButton)
        connect(m_schedOptButton, &MythUIButton::Clicked, this, &ScheduleEditor::ShowSchedOpt);
    if (m_filtersButton)
        connect(m_filtersButton, &MythUIButton::Clicked, this, &ScheduleEditor::ShowFilters);
    if (m_storeOptButton)
        connect(m_storeOptButton, &MythUIButton::Clicked, this, &ScheduleEditor::ShowStoreOpt);
    if (m_postProcButton)
        connect(m_postProcButton, &MythUIButton::Clicked, this, &ScheduleEditor::ShowPostProc);
    if (m_schedInfoButton)
        connect(m_schedInfoButton, &MythUIButton::Clicked, this, &ScheduleEditor::ShowSchedInfo);
    if (m_previewButton)
        connect(m_previewButton, &MythUIButton::Clicked, this, &ScheduleEditor::ShowPreview);
    if (m_metadataButton)
        connect(m_metadataButton, &MythUIButton::Clicked, this, &ScheduleEditor::ShowMetadataOptions);

    if (m_cancelButton)
        connect(m_cancelButton, &MythUIButton::Clicked, this, &ScheduleEditor::Close);
    connect(m_saveButton, &MythUIButton::Clicked, this, &ScheduleEditor::Save);

    if (m_schedInfoButton)
        m_schedInfoButton->SetEnabled(!m_recordingRule->m_isTemplate);
    if (m_previewButton)
        m_previewButton->SetEnabled(!m_recordingRule->m_isTemplate);

    if (m_dupmethodList)
        connect(m_dupmethodList, &MythUIButtonList::itemSelected,
                this, &ScheduleEditor::DupMethodChanged);
    if (m_filtersList)
        connect(m_filtersList, &MythUIButtonList::itemClicked,
                this, &ScheduleEditor::FilterChanged);
    if (m_maxepSpin)
        connect(m_maxepSpin, &MythUIButtonList::itemClicked,
                this, &ScheduleEditor::MaxEpisodesChanged);
    if (m_recgroupList)
        connect(m_recgroupList, &MythUIType::LosingFocus,
                this, &ScheduleEditor::PromptForRecGroup);
    if (m_transcodeCheck)
        connect(m_transcodeCheck, &MythUICheckBox::toggled,
                this, &ScheduleEditor::TranscodeChanged);

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
        emit m_player->RequestEmbedding(true);

    return true;
}

void ScheduleEditor::Close()
{
    if (m_child)
        m_child->Close();

    // don't fade the screen if we are returning to the player
    if (m_player)
        GetScreenStack()->PopScreen(this, false);
    else
        GetScreenStack()->PopScreen(this, true);
}

void ScheduleEditor::Load()
{
    SchedOptMixin::Load();
    FilterOptMixin::Load();
    StoreOptMixin::Load();
    PostProcMixin::Load();

    if (!m_loaded)
    {
        // Copy this now, it will change briefly after the first item
        // is inserted into the list by design of
        // MythUIButtonList::itemSelected()
        RecordingType type = m_recordingRule->m_type;

        // Rules List
        if (m_recordingRule->m_isTemplate)
        {
            if (m_recordingRule->m_category
                .compare("Default", Qt::CaseInsensitive) != 0)
            {
                new MythUIButtonListItem(m_rulesList,
                                     tr("Delete this recording rule template"),
                                         toVariant(kNotRecording));
            }
            new MythUIButtonListItem(m_rulesList,
                                     toDescription(kTemplateRecord),
                                     toVariant(kTemplateRecord));
        }
        else if (m_recordingRule->m_isOverride)
        {
            new MythUIButtonListItem(m_rulesList,
                                     tr("Record this showing with normal options"),
                                     toVariant(kNotRecording));
            new MythUIButtonListItem(m_rulesList,
                                     toDescription(kOverrideRecord),
                                     toVariant(kOverrideRecord));
            new MythUIButtonListItem(m_rulesList,
                                     toDescription(kDontRecord),
                                     toVariant(kDontRecord));
        }
        else
        {
            bool hasChannel = !m_recordingRule->m_station.isEmpty();
            bool isManual = (m_recordingRule->m_searchType == kManualSearch);

            new MythUIButtonListItem(m_rulesList,
                                     toDescription(kNotRecording),
                                     toVariant(kNotRecording));
            if (hasChannel)
            {
                new MythUIButtonListItem(m_rulesList,
                                         toDescription(kSingleRecord),
                                         toVariant(kSingleRecord));
            }
            if (!isManual)
            {
                new MythUIButtonListItem(m_rulesList,
                                         toDescription(kOneRecord),
                                         toVariant(kOneRecord));
            }
            if (!hasChannel || isManual)
            {
                new MythUIButtonListItem(m_rulesList,
                                         toDescription(kWeeklyRecord),
                                         toVariant(kWeeklyRecord));
                new MythUIButtonListItem(m_rulesList,
                                         toDescription(kDailyRecord),
                                         toVariant(kDailyRecord));
            }
            if (!isManual)
            {
                new MythUIButtonListItem(m_rulesList,
                                         toDescription(kAllRecord),
                                         toVariant(kAllRecord));
            }
        }

        m_recordingRule->m_type = type;
    }
    m_rulesList->SetValueByData(toVariant(m_recordingRule->m_type));

    InfoMap progMap;

    m_recordingRule->ToMap(progMap);

    if (m_recInfo)
        m_recInfo->ToMap(progMap);

    SetTextFromMap(progMap);

    m_loaded = true;
}

void ScheduleEditor::LoadTemplate(const QString& name)
{
    m_recordingRule->LoadTemplate(name);
    Load();
    emit templateLoaded();
}

void ScheduleEditor::RuleChanged(MythUIButtonListItem *item)
{
    if (!item)
        return;

    m_recordingRule->m_type = static_cast<RecordingType>
        (item->GetData().toInt());

    bool isScheduled = (m_recordingRule->m_type != kNotRecording &&
                        m_recordingRule->m_type != kDontRecord);

    if (m_schedOptButton)
        m_schedOptButton->SetEnabled(isScheduled);
    if (m_filtersButton)
        m_filtersButton->SetEnabled(isScheduled);
    if (m_storeOptButton)
        m_storeOptButton->SetEnabled(isScheduled);
    if (m_postProcButton)
        m_postProcButton->SetEnabled(isScheduled);
    if (m_metadataButton)
        m_metadataButton->SetEnabled(isScheduled &&
                                     !m_recordingRule->m_isTemplate);

    SchedOptMixin::RuleChanged();
    FilterOptMixin::RuleChanged();
    StoreOptMixin::RuleChanged();
    PostProcMixin::RuleChanged();
}

void ScheduleEditor::DupMethodChanged(MythUIButtonListItem *item)
{
    SchedOptMixin::DupMethodChanged(item);
}

void ScheduleEditor::FilterChanged(MythUIButtonListItem *item)
{
    FilterOptMixin::ToggleSelected(item);
}

void ScheduleEditor::MaxEpisodesChanged(MythUIButtonListItem *item)
{
    StoreOptMixin::MaxEpisodesChanged(item);
}

void ScheduleEditor::PromptForRecGroup(void)
{
    StoreOptMixin::PromptForRecGroup();
}

void ScheduleEditor::TranscodeChanged(bool enable)
{
    PostProcMixin::TranscodeChanged(enable);
}

void ScheduleEditor::Save()
{
    if (m_child)
        m_child->Close();

    if (m_recordingRule->m_type == kNotRecording)
    {
        int recid = m_recordingRule->m_recordID;
        DeleteRule();
        if (recid)
            emit ruleDeleted(recid);
        Close();
        return;
    }

    SchedOptMixin::Save();
    FilterOptMixin::Save();
    StoreOptMixin::Save();
    PostProcMixin::Save();
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
    if (m_recordingRule->m_type == kNotRecording ||
        m_recordingRule->m_type == kDontRecord)
        return;

    if (m_child)
        m_child->Close();

    SchedOptMixin::Save();

    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
    auto *schedoptedit = new SchedOptEditor(mainStack, *this,
                                            *m_recordingRule, m_recInfo);
    if (!schedoptedit->Create())
    {
        delete schedoptedit;
        return;
    }

    m_view = kSchedOptView;
    m_child = schedoptedit;
    mainStack->AddScreen(schedoptedit);
}

void ScheduleEditor::ShowStoreOpt()
{
    if (m_recordingRule->m_type == kNotRecording ||
        m_recordingRule->m_type == kDontRecord)
        return;

    if (m_child)
        m_child->Close();

    StoreOptMixin::Save();

    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
    auto *storeoptedit = new StoreOptEditor(mainStack, *this,
                                            *m_recordingRule, m_recInfo);
    if (!storeoptedit->Create())
    {
        delete storeoptedit;
        return;
    }

    m_view = kStoreOptView;
    m_child = storeoptedit;
    mainStack->AddScreen(storeoptedit);
}

void ScheduleEditor::ShowPostProc()
{
    if (m_recordingRule->m_type == kNotRecording ||
        m_recordingRule->m_type == kDontRecord)
        return;

    if (m_child)
        m_child->Close();

    PostProcMixin::Save();

    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
    auto *ppedit = new PostProcEditor(mainStack, *this,
                                      *m_recordingRule, m_recInfo);
    if (!ppedit->Create())
    {
        delete ppedit;
        return;
    }

    m_view = kPostProcView;
    m_child = ppedit;
    mainStack->AddScreen(ppedit);
}

void ScheduleEditor::ShowSchedInfo()
{
    if (m_recordingRule->m_type == kTemplateRecord)
        return;

    QString label = tr("Schedule Information");

    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");
    auto *menuPopup = new MythDialogBox(label, popupStack, "menuPopup");

    if (menuPopup->Create())
    {
        menuPopup->SetReturnEvent(this, "schedinfo");

        if (m_recInfo)
            menuPopup->AddButton(tr("Program Details"));
        menuPopup->AddButton(tr("Upcoming Episodes"));
        menuPopup->AddButton(tr("Upcoming Recordings"));
        if (m_recordingRule->m_type != kTemplateRecord)
            menuPopup->AddButton(tr("Previously Recorded"));

        popupStack->AddScreen(menuPopup);
    }
    else
        delete menuPopup;
}

bool ScheduleEditor::keyPressEvent(QKeyEvent *event)
{
    if (GetFocusWidget()->keyPressEvent(event))
        return true;

    QStringList actions;
    bool handled = GetMythMainWindow()->
        TranslateKeyPress("TV Frontend", event, actions);

    for (int i = 0; i < actions.size() && !handled; i++)
    {
        QString action = actions[i];
        handled = true;

        if (action == "MENU")
            showMenu();
        else if (action == "INFO")
            ShowDetails();
        else if (action == "GUIDE")
            ShowGuide();
        else if (action == "UPCOMING")
            showUpcomingByTitle();
        else if (action == "PREVVIEW")
            ShowPreviousView();
        else if (action == "NEXTVIEW")
            ShowNextView();
        else
            handled = false;
    }

    if (!handled && MythScreenType::keyPressEvent(event))
        handled = true;

    return handled;
}

void ScheduleEditor::customEvent(QEvent *event)
{
    if (event->type() == DialogCompletionEvent::kEventType)
    {
        auto *dce = (DialogCompletionEvent*)(event);

        QString resultid  = dce->GetId();
        QString resulttext = dce->GetResultText();

        if (resultid == "menu")
        {
            if (resulttext == tr("Main Options"))
                m_child->Close();
            if (resulttext == tr("Schedule Options"))
                ShowSchedOpt();
            else if (resulttext == tr("Filter Options"))
                ShowFilters();
            else if (resulttext == tr("Storage Options"))
                ShowStoreOpt();
            else if (resulttext == tr("Post Processing"))
                ShowPostProc();
            else if (resulttext == tr("Metadata Options"))
                ShowMetadataOptions();
            else if (resulttext == tr("Use Template"))
                showTemplateMenu();
            else if (resulttext == tr("Schedule Info"))
                ShowSchedInfo();
            else if (resulttext == tr("Preview Changes"))
                ShowPreview();
        }
        else if (resultid == "templatemenu")
        {
            LoadTemplate(resulttext);
        }
        else if (resultid == "schedinfo")
        {
            if (resulttext == tr("Program Details"))
                ShowDetails();
            else if (resulttext == tr("Upcoming Episodes"))
                showUpcomingByTitle();
            else if (resulttext == tr("Upcoming Recordings"))
                showUpcomingByRule();
            else if (resulttext == tr("Previously Recorded"))
                ShowPrevious(m_recordingRule->m_recordID,
                             m_recordingRule->m_title);
        }
        else if (resultid == "newrecgroup")
        {
            int groupID = CreateRecordingGroup(resulttext);
            StoreOptMixin::SetRecGroup(groupID, resulttext);
        }
    }
}

void ScheduleEditor::showUpcomingByRule(void)
{
    if (m_recordingRule->m_type == kTemplateRecord)
        return;

    // No rule? Search by title
    if (m_recordingRule->m_recordID <= 0)
    {
        showUpcomingByTitle();
        return;
    }

    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
    auto *pl = new ProgLister(mainStack, plRecordid,
                              QString::number(m_recordingRule->m_recordID), "");

    if (pl->Create())
        mainStack->AddScreen(pl);
    else
        delete pl;
}

void ScheduleEditor::showUpcomingByTitle(void)
{
    if (m_recordingRule->m_type == kTemplateRecord)
        return;

    // Existing rule and search?  Search by rule
    if (m_recordingRule->m_recordID > 0 &&
        m_recordingRule->m_searchType != kNoSearch)
        showUpcomingByRule();

    QString title = m_recordingRule->m_title;

    if (m_recordingRule->m_searchType != kNoSearch)
        title.remove(RecordingInfo::kReSearchTypeName);

    ShowUpcoming(title, m_recordingRule->m_seriesid);
}

void ScheduleEditor::ShowPreview(void)
{
    if (m_recordingRule->m_type == kTemplateRecord)
        return;

    if (m_child)
    {
        m_child->Save();
        if (m_view == kSchedOptView)
            SchedOptMixin::Load();
        else if (m_view == kStoreOptView)
            StoreOptMixin::Load();
        else if (m_view == kPostProcView)
            PostProcMixin::Load();
    }

    SchedOptMixin::Save();
    FilterOptMixin::Save();
    StoreOptMixin::Save();
    PostProcMixin::Save();

    QString ttable = "record_tmp";
    m_recordingRule->UseTempTable(true, ttable);

    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
    auto *vsd = new ViewScheduleDiff(mainStack, ttable,
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
    if (m_recordingRule->m_type == kNotRecording ||
        m_recordingRule->m_type == kDontRecord ||
        m_recordingRule->m_type == kTemplateRecord)
        return;

    if (m_child)
        m_child->Close();

    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
    auto *rad = new MetadataOptions(mainStack, *this,
                                    *m_recordingRule, m_recInfo);
    if (!rad->Create())
    {
        delete rad;
        return;
    }

    m_view = kMetadataView;
    m_child = rad;
    mainStack->AddScreen(rad);
}

void ScheduleEditor::ShowFilters(void)
{
    if (m_recordingRule->m_type == kNotRecording ||
        m_recordingRule->m_type == kDontRecord)
        return;

    if (m_child)
        m_child->Close();

    FilterOptMixin::Save();

    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
    auto *schedfilteredit = new SchedFilterEditor(mainStack, *this,
                                                  *m_recordingRule, m_recInfo);
    if (!schedfilteredit->Create())
    {
        delete schedfilteredit;
        return;
    }

    m_view = kFilterView;
    m_child = schedfilteredit;
    mainStack->AddScreen(schedfilteredit);
}

void ScheduleEditor::ShowPreviousView(void)
{
    if (m_recordingRule->m_type == kNotRecording ||
        m_recordingRule->m_type == kDontRecord)
        return;

    if (m_view == kMainView && !m_recordingRule->m_isTemplate)
        ShowMetadataOptions();
    else if ((m_view == kMainView) || (m_view == kMetadataView))
        ShowPostProc();
    else if (m_view == kSchedOptView)
        m_child->Close();
    else if (m_view == kFilterView)
        ShowSchedOpt();
    else if (m_view == kStoreOptView)
        ShowFilters();
    else if (m_view == kPostProcView)
        ShowStoreOpt();
}

void ScheduleEditor::ShowNextView(void)
{
    if (m_recordingRule->m_type == kNotRecording ||
        m_recordingRule->m_type == kDontRecord)
        return;

    if (m_view == kMainView)
        ShowSchedOpt();
    else if (m_view == kSchedOptView)
        ShowFilters();
    else if (m_view == kFilterView)
        ShowStoreOpt();
    else if (m_view == kStoreOptView)
        ShowPostProc();
    else if (m_view == kPostProcView && !m_recordingRule->m_isTemplate)
        ShowMetadataOptions();
    else if ((m_view == kPostProcView) || (m_view == kMetadataView))
        m_child->Close();
}

void ScheduleEditor::ChildClosing(void)
{
    if (m_view == kSchedOptView)
        SchedOptMixin::Load();
    else if (m_view == kFilterView)
        FilterOptMixin::Load();
    else if (m_view == kStoreOptView)
        StoreOptMixin::Load();
    else if (m_view == kPostProcView)
        PostProcMixin::Load();

    m_child = nullptr;
    m_view = kMainView;
}

void ScheduleEditor::showMenu(void)
{
    QString label = tr("Options");
    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");
    auto *menuPopup = new MythDialogBox(label, popupStack, "menuPopup");

    MythUIButtonListItem *item = m_rulesList->GetItemCurrent();
    RecordingType type = static_cast<RecordingType>(item->GetData().toInt());
    bool isScheduled = (type != kNotRecording && type != kDontRecord);

    if (menuPopup->Create())
    {
        menuPopup->SetReturnEvent(this, "menu");
        if (m_view != kMainView)
            menuPopup->AddButton(tr("Main Options"));
        if (isScheduled && m_view != kSchedOptView)
            menuPopup->AddButton(tr("Schedule Options"));
        if (isScheduled && m_view != kFilterView)
            menuPopup->AddButton(tr("Filter Options"));
        if (isScheduled && m_view != kStoreOptView)
            menuPopup->AddButton(tr("Storage Options"));
        if (isScheduled && m_view != kPostProcView)
            menuPopup->AddButton(tr("Post Processing"));
        if (isScheduled && !m_recordingRule->m_isTemplate &&
            m_view != kMetadataView)
            menuPopup->AddButton(tr("Metadata Options"));
        if (!m_recordingRule->m_isTemplate)
        {
            menuPopup->AddButton(tr("Schedule Info"));
            menuPopup->AddButton(tr("Preview Changes"));
        }
        menuPopup->AddButton(tr("Use Template"));
        popupStack->AddScreen(menuPopup);
    }
    else
    {
        delete menuPopup;
    }
}

void ScheduleEditor::showTemplateMenu(void)
{
    QStringList templates = RecordingRule::GetTemplateNames();
    if (templates.empty())
    {
        ShowOkPopup(tr("No templates available"));
        return;
    }

    QString label = tr("Template Options");
    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");
    auto *menuPopup = new MythDialogBox(label, popupStack, "menuPopup");

    if (menuPopup->Create())
    {
        menuPopup->SetReturnEvent(this, "templatemenu");
        while (!templates.empty())
        {
            QString name = templates.front();
            if (name == "Default")
                menuPopup->AddButton(tr("Default"));
            else
                menuPopup->AddButton(name);
            templates.pop_front();
        }
        popupStack->AddScreen(menuPopup);
    }
    else
    {
        delete menuPopup;
    }
}

////////////////////////////////////////////////////////

/** \class SchedEditerChild
 *
 */

SchedEditChild::SchedEditChild(MythScreenStack *parent, const QString &name,
                               ScheduleEditor &editor, RecordingRule &rule,
                               RecordingInfo *recInfo)
    : MythScreenType(parent, name),
      m_editor(&editor), m_recordingRule(&rule), m_recInfo(recInfo)
{
}

bool SchedEditChild::keyPressEvent(QKeyEvent *event)
{
    if (GetFocusWidget()->keyPressEvent(event))
        return true;

    QStringList actions;
    bool handled = GetMythMainWindow()->
        TranslateKeyPress("TV Frontend", event, actions);

    for (int i = 0; i < actions.size() && !handled; i++)
    {
        QString action = actions[i];
        handled = true;

        if (action == "MENU")
            m_editor->showMenu();
        else if (action == "INFO")
            m_editor->ShowDetails();
        else if (action == "UPCOMING")
            m_editor->showUpcomingByTitle();
        if (action == "ESCAPE")
            Close();
        else if (action == "PREVVIEW")
            m_editor->ShowPreviousView();
        else if (action == "NEXTVIEW")
            m_editor->ShowNextView();
        else
            handled = false;
    }

    if (!handled && MythScreenType::keyPressEvent(event))
        handled = true;

    return handled;
}

bool SchedEditChild::CreateEditChild(
    const QString &xmlfile, const QString &winname, bool isTemplate)
{
    if (!LoadWindowFromXML(xmlfile, winname, this))
        return false;

    UIUtilW::Assign(this, m_backButton, "back");
    UIUtilW::Assign(this, m_saveButton, "save");
    UIUtilW::Assign(this, m_previewButton, "preview");

    connect(this, &SchedEditChild::Closing, m_editor, &ScheduleEditor::ChildClosing);
    connect(m_editor, &ScheduleEditor::templateLoaded, this, &SchedEditChild::Load);

    if (m_backButton)
        connect(m_backButton, &MythUIButton::Clicked, this, &SchedEditChild::Close);
    if (m_saveButton)
        connect(m_saveButton, &MythUIButton::Clicked, m_editor, &ScheduleEditor::Save);
    if (m_previewButton)
    {
        connect(m_previewButton, &MythUIButton::Clicked,
                m_editor, &ScheduleEditor::ShowPreview);
        m_previewButton->SetEnabled(!isTemplate);
    }

    return true;
}

void SchedEditChild::SetTextFromMaps(void)
{
    InfoMap progMap;

    m_recordingRule->ToMap(progMap);

    if (m_recInfo)
        m_recInfo->ToMap(progMap);

    SetTextFromMap(progMap);
}

void SchedEditChild::Close(void)
{
    Save();
    emit Closing();
    MythScreenType::Close();
}

////////////////////////////////////////////////////////

/** \class SchedOptEditor
 *  \brief Select schedule options
 *
 */

SchedOptEditor::SchedOptEditor(MythScreenStack *parent,
                               ScheduleEditor &editor,
                               RecordingRule &rule,
                               RecordingInfo *recInfo)
    : SchedEditChild(parent, "ScheduleOptionsEditor", editor, rule, recInfo),
      SchedOptMixin(*this, &rule, &editor)
{
}

bool SchedOptEditor::Create()
{
    if (!SchedEditChild::CreateEditChild(
            "schedule-ui.xml", "scheduleoptionseditor",
            m_recordingRule->m_isTemplate))
    {
        return false;
    }

    bool err = false;

    SchedOptMixin::Create(&err);

    UIUtilW::Assign(this, m_filtersButton, "filters");

    if (err)
    {
        LOG(VB_GENERAL, LOG_ERR, "SchedOptEditor, theme is missing "
                                 "required elements");
        return false;
    }

    if (m_dupmethodList)
        connect(m_dupmethodList, &MythUIButtonList::itemSelected,
                this, &SchedOptEditor::DupMethodChanged);

    if (m_filtersButton)
        connect(m_filtersButton, &MythUIButton::Clicked,
                m_editor, &ScheduleEditor::ShowFilters);

    BuildFocusList();

    return true;
}

void SchedOptEditor::Load()
{
    SchedOptMixin::Load();
    SetTextFromMaps();
}

void SchedOptEditor::Save()
{
    SchedOptMixin::Save();
}

void SchedOptEditor::DupMethodChanged(MythUIButtonListItem *item)
{
    SchedOptMixin::DupMethodChanged(item);
}

////////////////////////////////////////////////////////

/** \class SchedFilterEditor
 *  \brief Select schedule filters
 *
 */

SchedFilterEditor::SchedFilterEditor(MythScreenStack *parent,
                                     ScheduleEditor &editor,
                                     RecordingRule &rule,
                                     RecordingInfo *recInfo)
    : SchedEditChild(parent, "ScheduleFilterEditor", editor, rule, recInfo),
      FilterOptMixin(*this, &rule, &editor)
{
}

bool SchedFilterEditor::Create()
{
    if (!SchedEditChild::CreateEditChild(
            "schedule-ui.xml", "schedulefiltereditor",
            m_recordingRule->m_isTemplate))
    {
        return false;
    }

    bool err = false;

    FilterOptMixin::Create(&err);

    if (err)
    {
        LOG(VB_GENERAL, LOG_ERR, "SchedFilterEditor, theme is missing "
                                 "required elements");
        return false;
    }

    connect(m_filtersList, &MythUIButtonList::itemClicked,
            this, SchedFilterEditor::ToggleSelected);

    BuildFocusList();

    return true;
}

void SchedFilterEditor::Load(void)
{
    FilterOptMixin::Load();
    SetTextFromMaps();
}

void SchedFilterEditor::Save()
{
    FilterOptMixin::Save();
}

void SchedFilterEditor::ToggleSelected(MythUIButtonListItem *item)
{
    FilterOptMixin::ToggleSelected(item);
}

/////////////////////////////

/** \class StoreOptEditor
 *  \brief Select storage options
 *
 */

StoreOptEditor::StoreOptEditor(MythScreenStack *parent,
                               ScheduleEditor &editor,
                               RecordingRule &rule,
                               RecordingInfo *recInfo)
    : SchedEditChild(parent, "StorageOptionsEditor", editor, rule, recInfo),
      StoreOptMixin(*this, &rule, &editor)
{
}

bool StoreOptEditor::Create()
{
    if (!SchedEditChild::CreateEditChild(
            "schedule-ui.xml", "storageoptionseditor",
            m_recordingRule->m_isTemplate))
    {
        return false;
    }

    bool err = false;

    StoreOptMixin::Create(&err);

    if (err)
    {
        LOG(VB_GENERAL, LOG_ERR, "StoreOptEditor, theme is missing "
                                 "required elements");
        return false;
    }

    if (m_maxepSpin)
        connect(m_maxepSpin, &MythUIButtonList::itemSelected,
                this, &StoreOptEditor::MaxEpisodesChanged);
    if (m_recgroupList)
        connect(m_recgroupList, &MythUIType::LosingFocus,
                this, &StoreOptEditor::PromptForRecGroup);

    BuildFocusList();

    return true;
}

void StoreOptEditor::Load()
{
    StoreOptMixin::Load();
    SetTextFromMaps();
}

void StoreOptEditor::MaxEpisodesChanged(MythUIButtonListItem *item)
{
    StoreOptMixin::MaxEpisodesChanged(item);
}

void StoreOptEditor::PromptForRecGroup(void)
{
    StoreOptMixin::PromptForRecGroup();
}

void StoreOptEditor::customEvent(QEvent *event)
{
    if (event->type() == DialogCompletionEvent::kEventType)
    {
        auto *dce = (DialogCompletionEvent*)(event);

        QString resultid   = dce->GetId();
        QString resulttext = dce->GetResultText();

        if (resultid == "newrecgroup")
        {
            int groupID = CreateRecordingGroup(resulttext);
            StoreOptMixin::SetRecGroup(groupID, resulttext);
        }
    }
}

void StoreOptEditor::Save()
{
    StoreOptMixin::Save();
}

/////////////////////////////

/** \class PostProcEditor
 *  \brief Select post-processing options
 *
 */

PostProcEditor::PostProcEditor(MythScreenStack *parent,
                               ScheduleEditor &editor,
                               RecordingRule &rule,
                               RecordingInfo *recInfo)
    : SchedEditChild(parent, "PostProcOptionsEditor", editor, rule, recInfo),
      PostProcMixin(*this, &rule, &editor)
{
}

bool PostProcEditor::Create()
{
    if (!SchedEditChild::CreateEditChild(
            "schedule-ui.xml", "postproceditor",
            m_recordingRule->m_isTemplate))
    {
        return false;
    }

    bool err = false;

    PostProcMixin::Create(&err);

    if (err)
    {
        LOG(VB_GENERAL, LOG_ERR, "PostProcEditor, theme is missing "
                                 "required elements");
        return false;
    }

    if (m_transcodeCheck)
        connect(m_transcodeCheck, &MythUICheckBox::toggled,
                this, &PostProcEditor::TranscodeChanged);

    BuildFocusList();

    return true;
}

void PostProcEditor::Load()
{
    PostProcMixin::Load();
    SetTextFromMaps();
}

void PostProcEditor::TranscodeChanged(bool enable)
{
    PostProcMixin::TranscodeChanged(enable);
}

void PostProcEditor::Save()
{
    PostProcMixin::Save();
}

/////////////////////////////

/** \class MetadataOptions
 *  \brief Select artwork and inetref for recordings
 *
 */

MetadataOptions::MetadataOptions(MythScreenStack *parent,
                                 ScheduleEditor &editor,
                                 RecordingRule &rule,
                                 RecordingInfo *recInfo)
    : SchedEditChild(parent, "MetadataOptions", editor, rule, recInfo)
{
    m_popupStack = GetMythMainWindow()->GetStack("popup stack");

    m_metadataFactory = new MetadataFactory(this);
    m_imageLookup = new MetadataDownload(this);
    m_imageDownload = new MetadataImageDownload(this);

    m_artworkMap = GetArtwork(m_recordingRule->m_inetref,
                              m_recordingRule->m_season);
}

MetadataOptions::~MetadataOptions(void)
{
    if (m_imageLookup)
    {
        m_imageLookup->cancel();
        delete m_imageLookup;
        m_imageLookup = nullptr;
    }

    if (m_imageDownload)
    {
        m_imageDownload->cancel();
        delete m_imageDownload;
        m_imageDownload = nullptr;
    }
}

bool MetadataOptions::Create()
{
    if (!SchedEditChild::CreateEditChild(
            "schedule-ui.xml", "metadataoptions",
            m_recordingRule->m_isTemplate))
    {
        return false;
    }

    bool err = false;

    UIUtilE::Assign(this, m_inetrefEdit, "inetref_edit", &err);
    UIUtilW::Assign(this, m_inetrefClear, "inetref_clear", &err);
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

    if (err)
    {
        LOG(VB_GENERAL, LOG_ERR, "MetadataOptions, theme is missing "
                                 "required elements");
        return false;
    }

    connect(m_inetrefClear, &MythUIButton::Clicked,
            this, &MetadataOptions::ClearInetref);
    connect(m_queryButton, &MythUIButton::Clicked,
            this, &MetadataOptions::PerformQuery);
    connect(m_localFanartButton, &MythUIButton::Clicked,
            this, &MetadataOptions::SelectLocalFanart);
    connect(m_localCoverartButton, &MythUIButton::Clicked,
            this, &MetadataOptions::SelectLocalCoverart);
    connect(m_localBannerButton, &MythUIButton::Clicked,
            this, &MetadataOptions::SelectLocalBanner);
    connect(m_onlineFanartButton, &MythUIButton::Clicked,
            this, &MetadataOptions::SelectOnlineFanart);
    connect(m_onlineCoverartButton, &MythUIButton::Clicked,
            this, &MetadataOptions::SelectOnlineCoverart);
    connect(m_onlineBannerButton, &MythUIButton::Clicked,
            this, &MetadataOptions::SelectOnlineBanner);

    connect(m_seasonSpin, &MythUIButtonList::itemSelected,
                          this, &MetadataOptions::ValuesChanged);

    // InetRef
    m_inetrefEdit->SetText(m_recordingRule->m_inetref);

    // Season
    m_seasonSpin->SetRange(0,9999,1,5);
    m_seasonSpin->SetValue(m_recordingRule->m_season != 0 ? m_recordingRule->m_season : m_recInfo ? m_recInfo->GetSeason() : 0);

    // Episode
    m_episodeSpin->SetRange(0,9999,1,10);
    m_episodeSpin->SetValue(m_recordingRule->m_episode != 0 ? m_recordingRule->m_episode : m_recInfo ? m_recInfo->GetEpisode() : 0);

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
    SetTextFromMaps();
}

void MetadataOptions::CreateBusyDialog(const QString& title)
{
    if (m_busyPopup)
        return;

    const QString& message = title;

    m_busyPopup = new MythUIBusyDialog(message, m_popupStack,
            "metaoptsdialog");

    if (m_busyPopup->Create())
        m_popupStack->AddScreen(m_busyPopup);
}

void MetadataOptions::ClearInetref()
{
    m_recordingRule->m_inetref.clear();
    m_inetrefEdit->SetText(m_recordingRule->m_inetref);
}

void MetadataOptions::PerformQuery()
{
    CreateBusyDialog(tr("Trying to manually find this "
                        "recording online..."));

    MetadataLookup *lookup = CreateLookup(kMetadataRecording);

    lookup->SetAutomatic(false);
    m_metadataFactory->Lookup(lookup);
}

void MetadataOptions::OnSearchListSelection(const RefCountHandler<MetadataLookup>& lookup)
{
    QueryComplete(lookup);
}

void MetadataOptions::OnImageSearchListSelection(const ArtworkInfo& info,
                                                 VideoArtworkType type)
{
    QString msg = tr("Downloading selected artwork...");
    CreateBusyDialog(msg);

    auto *lookup = new MetadataLookup();

    lookup->SetType(kMetadataVideo);
    lookup->SetHost(gCoreContext->GetMasterHostName());
    lookup->SetAutomatic(true);
    lookup->SetData(QVariant::fromValue<VideoArtworkType>(type));

    DownloadMap downloads;
    downloads.insert(type, info);
    lookup->SetDownloads(downloads);
    lookup->SetAllowOverwrites(true);
    lookup->SetTitle(m_recordingRule->m_title);
    lookup->SetSubtitle(m_recordingRule->m_subtitle);
    lookup->SetInetref(m_inetrefEdit->GetText());
    lookup->SetSeason(m_seasonSpin->GetIntValue());
    lookup->SetEpisode(m_episodeSpin->GetIntValue());

    m_imageDownload->addDownloads(lookup);
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

    // InetRef
    m_inetrefEdit->SetText(lookup->GetInetref());

    // Season
    m_seasonSpin->SetValue(lookup->GetSeason());

    // Episode
    m_episodeSpin->SetValue(lookup->GetEpisode());

    InfoMap metadataMap;
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

    auto *fb = new MythUIFileBrowser(popupStack, fp);
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
    for (const auto & ext : qAsConst(exts))
    {
        ret.append(QString("*.").append(ext));
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

MetadataLookup *MetadataOptions::CreateLookup(MetadataType mtype)
{
    auto *lookup = new MetadataLookup();
    lookup->SetStep(kLookupSearch);
    lookup->SetType(mtype);
    LookupType type = GuessLookupType(m_inetrefEdit->GetText());

    if (type == kUnknownVideo)
    {
        if ((m_recInfo && m_recInfo->GetCategoryType() == ProgramInfo::kCategoryMovie) ||
            (m_seasonSpin->GetIntValue() == 0 &&
             m_episodeSpin->GetIntValue() == 0))
        {
            lookup->SetSubtype(kProbableMovie);
        }
        else
        {
            lookup->SetSubtype(kProbableTelevision);
        }
    }
    else
    {
        // we could determine the type from the inetref
        lookup->SetSubtype(type);
    }
    lookup->SetAllowGeneric(true);
    lookup->SetHandleImages(false);
    lookup->SetHost(gCoreContext->GetMasterHostName());
    lookup->SetTitle(m_recordingRule->m_title);
    lookup->SetSubtitle(m_recordingRule->m_subtitle);
    lookup->SetInetref(m_inetrefEdit->GetText());
    lookup->SetCollectionref(m_inetrefEdit->GetText());
    lookup->SetSeason(m_seasonSpin->GetIntValue());
    lookup->SetEpisode(m_episodeSpin->GetIntValue());

    return lookup;
}

void MetadataOptions::FindNetArt(VideoArtworkType type)
{
    if (!CanSetArtwork())
        return;

    QString msg = tr("Searching for available artwork...");
    CreateBusyDialog(msg);

    MetadataLookup *lookup = CreateLookup(kMetadataVideo);

    lookup->SetAutomatic(true);
    lookup->SetData(QVariant::fromValue<VideoArtworkType>(type));
    m_imageLookup->addLookup(lookup);
}

void MetadataOptions::OnArtworkSearchDone(MetadataLookup *lookup)
{
    if (!lookup)
        return;

    if (m_busyPopup)
    {
        m_busyPopup->Close();
        m_busyPopup = nullptr;
    }

    auto type = lookup->GetData().value<VideoArtworkType>();
    ArtworkList list = lookup->GetArtwork(type);

    if (list.isEmpty())
    {
        MythWarningNotification n(tr("No image found"), tr("Schedule Editor"));
        GetNotificationCenter()->Queue(n);
        return;
    }

    auto *resultsdialog = new ImageSearchResultsDialog(m_popupStack, list, type);

    connect(resultsdialog, &ImageSearchResultsDialog::haveResult,
            this, &MetadataOptions::OnImageSearchListSelection);

    if (resultsdialog->Create())
        m_popupStack->AddScreen(resultsdialog);
}

void MetadataOptions::HandleDownloadedImages(MetadataLookup *lookup)
{
    if (!lookup)
        return;

    DownloadMap map = lookup->GetDownloads();

    if (map.isEmpty())
        return;

    for (DownloadMap::const_iterator i = map.cbegin(); i != map.cend(); ++i)
    {
        VideoArtworkType type = i.key();
        const ArtworkInfo& info = i.value();

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
            m_busyPopup = nullptr;
        }

        auto *mfmr = dynamic_cast<MetadataFactoryMultiResult*>(levent);
        if (!mfmr)
            return;

        MetadataLookupList list = mfmr->m_results;

        if (list.count() > 1)
        {
            int yearindex = -1;

            for (int p = 0; p != list.size(); ++p)
            {
                if (!m_recordingRule->m_seriesid.isEmpty() &&
                    m_recordingRule->m_seriesid == (list[p])->GetTMSref())
                {
                    MetadataLookup *lookup = list[p];
                    QueryComplete(lookup);
                    return;
                }
                if (m_recInfo &&
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
                MetadataLookup *lookup = list[yearindex];
                QueryComplete(lookup);
                return;
            }

            LOG(VB_GENERAL, LOG_INFO, "Falling through to selection dialog.");
            auto *resultsdialog = new MetadataResultsDialog(m_popupStack, list);

            connect(resultsdialog, &MetadataResultsDialog::haveResult,
                    this, &MetadataOptions::OnSearchListSelection,
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
            m_busyPopup = nullptr;
        }

        auto *mfsr = dynamic_cast<MetadataFactorySingleResult*>(levent);
        if (!mfsr)
            return;

        MetadataLookup *lookup = mfsr->m_result;

        if (!lookup)
            return;

        QueryComplete(lookup);
    }
    else if (levent->type() == MetadataFactoryNoResult::kEventType)
    {
        if (m_busyPopup)
        {
            m_busyPopup->Close();
            m_busyPopup = nullptr;
        }

        auto *mfnr = dynamic_cast<MetadataFactoryNoResult*>(levent);
        if (!mfnr)
            return;

        QString title = tr("No match found for this recording. You can "
                           "try entering a TVDB/TMDB number, season, and "
                           "episode manually.");

        auto *okPopup = new MythConfirmationDialog(m_popupStack, title, false);

        if (okPopup->Create())
            m_popupStack->AddScreen(okPopup);
    }
    else if (levent->type() == MetadataLookupEvent::kEventType)
    {
        if (m_busyPopup)
        {
            m_busyPopup->Close();
            m_busyPopup = nullptr;
        }

        auto *lue = (MetadataLookupEvent *)levent;

        MetadataLookupList lul = lue->m_lookupList;

        if (lul.isEmpty())
            return;

        if (lul.count() >= 1)
        {
            OnArtworkSearchDone(lul[0]);
        }
    }
    else if (levent->type() == MetadataLookupFailure::kEventType)
    {
        if (m_busyPopup)
        {
            m_busyPopup->Close();
            m_busyPopup = nullptr;
        }

        auto *luf = (MetadataLookupFailure *)levent;

        MetadataLookupList lul = luf->m_lookupList;

        if (!lul.empty())
        {
            QString title = tr("This number, season, and episode combination "
                               "does not appear to be valid (or the site may "
                               "be down). Check your information and try "
                               "again.");

            auto *okPopup = new MythConfirmationDialog(m_popupStack, title, false);

            if (okPopup->Create())
                m_popupStack->AddScreen(okPopup);
        }
    }
    else if (levent->type() == ImageDLEvent::kEventType)
    {
        if (m_busyPopup)
        {
            m_busyPopup->Close();
            m_busyPopup = nullptr;
        }

        auto *ide = (ImageDLEvent *)levent;

        MetadataLookup *lookup = ide->m_item;

        if (!lookup)
            return;

        HandleDownloadedImages(lookup);
    }
    else if (levent->type() == ImageDLFailureEvent::kEventType)
    {
        if (m_busyPopup)
        {
            m_busyPopup->Close();
            m_busyPopup = nullptr;
        }
        MythErrorNotification n(tr("Failed to retrieve image(s)"),
                                tr("Schedule Editor"),
                                tr("Check logs"));
        GetNotificationCenter()->Queue(n);
    }
    else if (levent->type() == DialogCompletionEvent::kEventType)
    {
        auto *dce = (DialogCompletionEvent*)(levent);

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

////////////////////////////////////////////////////////

/** \class SchedOptMixin
 *  \brief Mixin for schedule options
 *
 */

SchedOptMixin::SchedOptMixin(MythScreenType &screen, RecordingRule *rule,
                             SchedOptMixin *other)
    : m_screen(&screen), m_rule(rule), m_other(other),
      m_haveRepeats(gCoreContext->GetBoolSetting("HaveRepeats", false))
{
}

void SchedOptMixin::Create(bool *err)
{
    if (!m_rule)
        return;

    if (m_other && !m_other->m_prioritySpin)
        UIUtilE::Assign(m_screen, m_prioritySpin, "priority", err);
    else
        UIUtilW::Assign(m_screen, m_prioritySpin, "priority");

    if (m_other && !m_other->m_startoffsetSpin)
        UIUtilE::Assign(m_screen, m_startoffsetSpin, "startoffset", err);
    else
        UIUtilW::Assign(m_screen, m_startoffsetSpin, "startoffset");

    if (m_other && !m_other->m_endoffsetSpin)
        UIUtilE::Assign(m_screen, m_endoffsetSpin, "endoffset", err);
    else
        UIUtilW::Assign(m_screen, m_endoffsetSpin, "endoffset");

    if (m_other && !m_other->m_dupmethodList)
        UIUtilE::Assign(m_screen, m_dupmethodList, "dupmethod", err);
    else
        UIUtilW::Assign(m_screen, m_dupmethodList, "dupmethod");

    if (m_other && !m_other->m_dupscopeList)
        UIUtilE::Assign(m_screen, m_dupscopeList, "dupscope", err);
    else
        UIUtilW::Assign(m_screen, m_dupscopeList, "dupscope");

    if (m_other && !m_other->m_autoExtendList)
        UIUtilE::Assign(m_screen, m_autoExtendList, "autoextend", err);
    else
        UIUtilW::Assign(m_screen, m_autoExtendList, "autoextend");

    if (m_other && !m_other->m_inputList)
        UIUtilE::Assign(m_screen, m_inputList, "input", err);
    else
        UIUtilW::Assign(m_screen, m_inputList, "input");

    if (m_other && !m_other->m_ruleactiveCheck)
        UIUtilE::Assign(m_screen, m_ruleactiveCheck, "ruleactive", err);
    else
        UIUtilW::Assign(m_screen, m_ruleactiveCheck, "ruleactive");

    UIUtilW::Assign(m_screen, m_newrepeatList, "newrepeat");
}

void SchedOptMixin::Load(void)
{
    if (!m_rule)
        return;

    // Priority
    if (m_prioritySpin)
    {
        if (!m_loaded)
            m_prioritySpin->SetRange(-99,99,1,5);
        m_prioritySpin->SetValue(m_rule->m_recPriority);
    }

    // Start Offset
    if (m_startoffsetSpin)
    {
        if (!m_loaded)
            m_startoffsetSpin->SetRange(480,-480,1,10);
        m_startoffsetSpin->SetValue(m_rule->m_startOffset);
    }

    // End Offset
    if (m_endoffsetSpin)
    {
        if (!m_loaded)
            m_endoffsetSpin->SetRange(-480,480,1,10);
        m_endoffsetSpin->SetValue(m_rule->m_endOffset);
    }

    // Duplicate Match Type
    if (m_dupmethodList)
    {
        if (!m_loaded)
        {
            RecordingDupMethodType dupMethod = m_rule->m_dupMethod;

            new MythUIButtonListItem(m_dupmethodList,
                                     toDescription(kDupCheckSubDesc),
                                     toVariant(kDupCheckSubDesc));
            new MythUIButtonListItem(m_dupmethodList,
                                     toDescription(kDupCheckSubThenDesc),
                                     toVariant(kDupCheckSubThenDesc));
            new MythUIButtonListItem(m_dupmethodList,
                                     toDescription(kDupCheckSub),
                                     toVariant(kDupCheckSub));
            new MythUIButtonListItem(m_dupmethodList,
                                     toDescription(kDupCheckDesc),
                                     toVariant(kDupCheckDesc));
            new MythUIButtonListItem(m_dupmethodList,
                                     toDescription(kDupCheckNone),
                                     toVariant(kDupCheckNone));

            m_rule->m_dupMethod = dupMethod;
        }
        m_dupmethodList->SetValueByData(toVariant(m_rule->m_dupMethod));
    }

    // Duplicate Matching Scope
    if (m_dupscopeList)
    {
        if (!m_loaded)
        {
            new MythUIButtonListItem(m_dupscopeList,
                                     toDescription(kDupsInAll),
                                     toVariant(kDupsInAll));
            new MythUIButtonListItem(m_dupscopeList,
                                     toDescription(kDupsInRecorded),
                                     toVariant(kDupsInRecorded));
            new MythUIButtonListItem(m_dupscopeList,
                                     toDescription(kDupsInOldRecorded),
                                     toVariant(kDupsInOldRecorded));
            if (m_haveRepeats && !m_newrepeatList &&
                (!m_other || !m_other->m_newrepeatList))
            {
                int value = static_cast<int>(kDupsNewEpi|kDupsInAll);
                new MythUIButtonListItem(m_dupscopeList,
                                 toDescription(kDupsNewEpi),
                                 QVariant::fromValue(value));
            }
        }
        m_dupscopeList->SetValueByData(toVariant(m_rule->m_dupIn));
    }

    // Auto Extend Services
    if (m_autoExtendList)
    {
        if (!m_loaded)
        {
            new MythUIButtonListItem(m_autoExtendList,
                                     toDescription(AutoExtendType::None),
                                     toVariant(AutoExtendType::None));
            new MythUIButtonListItem(m_autoExtendList,
                                     toDescription(AutoExtendType::ESPN),
                                     toVariant(AutoExtendType::ESPN));
            new MythUIButtonListItem(m_autoExtendList,
                                     toDescription(AutoExtendType::MLB),
                                     toVariant(AutoExtendType::MLB));
        }
        m_autoExtendList->SetValueByData(toVariant(m_rule->m_autoExtend));
    }

    // Preferred Input
    if (m_inputList)
    {
        if (!m_loaded)
        {
            new MythUIButtonListItem(m_inputList,
                                     QObject::tr("Use any available input"),
                                     QVariant::fromValue(0));

            std::vector<uint> inputids = CardUtil::GetSchedInputList();
            for (uint id : inputids)
            {
                new MythUIButtonListItem(m_inputList,
                    QObject::tr("Prefer input %1")
                    .arg(CardUtil::GetDisplayName(id)), id);
            }
        }
        m_inputList->SetValueByData(m_rule->m_prefInput);
    }

    // Active/Disabled
    if (m_ruleactiveCheck)
    {
        m_ruleactiveCheck->SetCheckState(!m_rule->m_isInactive);
    }

    // Record new and repeat
    if (m_newrepeatList)
    {
        if (!m_loaded)
        {
            new MythUIButtonListItem(m_newrepeatList,
                                     QObject::tr("Record new and repeat "
                                         "episodes"), toVariant(kDupsUnset));
            new MythUIButtonListItem(m_newrepeatList,
                                     QObject::tr("Record new episodes only"),
                                     toVariant(kDupsNewEpi));
        }
        RecordingDupInType value =
            (m_rule->m_dupIn & kDupsNewEpi)?kDupsNewEpi:kDupsUnset;
        m_newrepeatList->SetValueByData(toVariant(value));
    }

    m_loaded = true;

    RuleChanged();
}

void SchedOptMixin::Save(void)
{
    if (!m_rule)
        return;

    if (m_prioritySpin)
        m_rule->m_recPriority = m_prioritySpin->GetIntValue();
    if (m_startoffsetSpin)
        m_rule->m_startOffset = m_startoffsetSpin->GetIntValue();
    if (m_endoffsetSpin)
        m_rule->m_endOffset = m_endoffsetSpin->GetIntValue();
    if (m_dupmethodList)
        m_rule->m_dupMethod = static_cast<RecordingDupMethodType>
            (m_dupmethodList->GetDataValue().toInt());
    if (m_dupscopeList)
    {
        int mask = ((m_other && m_other->m_newrepeatList) ||
                    m_newrepeatList) ? kDupsInAll : ~0;
        int val = ((m_rule->m_dupIn & ~mask) |
                   m_dupscopeList->GetDataValue().toInt());
        m_rule->m_dupIn = static_cast<RecordingDupInType>(val);
    }
    if (m_autoExtendList)
    {
        int val = m_autoExtendList->GetDataValue().toInt();
        m_rule->m_autoExtend = static_cast<AutoExtendType>(val);
    }
    if (m_inputList)
        m_rule->m_prefInput = m_inputList->GetDataValue().toInt();
    if (m_ruleactiveCheck)
        m_rule->m_isInactive = !m_ruleactiveCheck->GetBooleanCheckState();
    if (m_newrepeatList)
    {
        int val = ((m_rule->m_dupIn & ~kDupsNewEpi) |
                   m_newrepeatList->GetDataValue().toInt());
        m_rule->m_dupIn = static_cast<RecordingDupInType>(val);
    }
}

void SchedOptMixin::RuleChanged(void)
{
    if (!m_rule)
        return;

    bool isScheduled = (m_rule->m_type != kNotRecording &&
                        m_rule->m_type != kDontRecord);
    bool isSingle = (m_rule->m_type == kSingleRecord ||
                     m_rule->m_type == kOverrideRecord);
    bool isManual = (m_rule->m_searchType == kManualSearch);

    if (m_prioritySpin)
        m_prioritySpin->SetEnabled(isScheduled);
    if (m_startoffsetSpin)
        m_startoffsetSpin->SetEnabled(isScheduled);
    if (m_endoffsetSpin)
        m_endoffsetSpin->SetEnabled(isScheduled);
    if (m_dupmethodList)
    {
        m_dupmethodList->SetEnabled(
            isScheduled && !isSingle &&
            (!isManual || m_rule->m_dupMethod != kDupCheckNone));
    }
    if (m_dupscopeList)
        m_dupscopeList->SetEnabled(isScheduled && !isSingle &&
                                   m_rule->m_dupMethod != kDupCheckNone);
    if (m_inputList)
        m_inputList->SetEnabled(isScheduled);
    if (m_ruleactiveCheck)
        m_ruleactiveCheck->SetEnabled(isScheduled);
    if (m_newrepeatList)
        m_newrepeatList->SetEnabled(isScheduled && !isSingle && m_haveRepeats);
}

void SchedOptMixin::DupMethodChanged(MythUIButtonListItem *item)
{
    if (!item || !m_rule)
        return;

    m_rule->m_dupMethod = static_cast<RecordingDupMethodType>
        (item->GetData().toInt());

    if (m_dupscopeList)
        m_dupscopeList->SetEnabled(m_rule->m_dupMethod != kDupCheckNone);
}

////////////////////////////////////////////////////////

/** \class FilterOptMixin
 *  \brief Mixin for Filters
 *
 */

void FilterOptMixin::Create(bool *err)
{
    if (!m_rule)
        return;

    if (m_other && !m_other->m_filtersList)
        UIUtilE::Assign(m_screen, m_filtersList, "filters", err);
    else
        UIUtilW::Assign(m_screen, m_filtersList, "filters");

    UIUtilW::Assign(m_screen, m_activeFiltersList, "activefilters");
    if (m_activeFiltersList)
        m_activeFiltersList->SetCanTakeFocus(false);
}

void FilterOptMixin::Load(void)
{
    if (!m_rule)
        return;

    if (!m_loaded)
    {
        MSqlQuery query(MSqlQuery::InitCon());

        query.prepare("SELECT filterid, description, newruledefault "
                      "FROM recordfilter ORDER BY filterid");

        if (query.exec())
        {
            while (query.next())
            {
                m_descriptions << QCoreApplication::translate("SchedFilterEditor",
                                    query.value(1).toString().toUtf8().constData());
            }
        }
        m_loaded = true;
    }

    if (m_activeFiltersList)
        m_activeFiltersList->Reset();

    MythUIButtonListItem *button = nullptr;
    QStringList::iterator Idesc;
    int  idx = 0;
    bool not_empty = m_filtersList && !m_filtersList->IsEmpty();
    for (Idesc = m_descriptions.begin(), idx = 0;
         Idesc != m_descriptions.end(); ++Idesc, ++idx)
    {
        bool active = (m_rule->m_filter & (1 << idx)) != 0U;
        if (m_filtersList)
        {
            if (not_empty)
                button = m_filtersList->GetItemAt(idx);
            else
                button = new MythUIButtonListItem(m_filtersList, *Idesc, idx);
            button->setCheckable(true);
            button->setChecked(active ? MythUIButtonListItem::FullChecked
                                      : MythUIButtonListItem::NotChecked);
        }
        if (active && m_activeFiltersList)
        {
            /* Create a simple list of active filters the theme can
               use for informational purposes. */
            button = new MythUIButtonListItem(m_activeFiltersList,
                                              *Idesc, idx);
            button->setCheckable(false);
        }
    }

    if (m_activeFiltersList && m_activeFiltersList->IsEmpty())
    {
        button = new MythUIButtonListItem(m_activeFiltersList,
                                          QObject::tr("None"), idx);
        button->setCheckable(false);
    }

    RuleChanged();
}

void FilterOptMixin::Save()
{
    if (!m_rule || !m_filtersList)
        return;

    // Iterate through button list, and build the mask
    uint32_t filter_mask = 0;

    int end = m_filtersList->GetCount();
    for (int idx = 0; idx < end; ++idx)
    {
        MythUIButtonListItem *button = m_filtersList->GetItemAt(idx);
        if (button != nullptr &&
            button->state() == MythUIButtonListItem::FullChecked)
            filter_mask |= (1 << button->GetData().value<uint32_t>());
    }
    m_rule->m_filter = filter_mask;
}

void FilterOptMixin::RuleChanged(void)
{
    if (!m_rule)
        return;

    bool enabled = m_rule->m_type != kNotRecording &&
                   m_rule->m_type != kDontRecord;
    if (m_filtersList)
        m_filtersList->SetEnabled(enabled);
    if (m_activeFiltersList)
        m_activeFiltersList->SetEnabled(enabled);
}

void FilterOptMixin::ToggleSelected(MythUIButtonListItem *item)
{
    item->setChecked(item->state() == MythUIButtonListItem::FullChecked ?
                     MythUIButtonListItem::NotChecked :
                     MythUIButtonListItem::FullChecked);
}


////////////////////////////////////////////////////////

/** \class StoreOptMixin
 *  \brief Mixin for storage options
 *
 */

void StoreOptMixin::Create(bool *err)
{
    if (!m_rule)
        return;

    if (m_other && !m_other->m_recprofileList)
        UIUtilE::Assign(m_screen, m_recprofileList, "recprofile", err);
    else
        UIUtilW::Assign(m_screen, m_recprofileList, "recprofile");

    if (m_other && !m_other->m_recgroupList)
        UIUtilE::Assign(m_screen, m_recgroupList, "recgroup", err);
    else
        UIUtilW::Assign(m_screen, m_recgroupList, "recgroup");

    if (m_other && !m_other->m_storagegroupList)
        UIUtilE::Assign(m_screen, m_storagegroupList, "storagegroup", err);
    else
        UIUtilW::Assign(m_screen, m_storagegroupList, "storagegroup");

    if (m_other && !m_other->m_playgroupList)
        UIUtilE::Assign(m_screen, m_playgroupList, "playgroup", err);
    else
        UIUtilW::Assign(m_screen, m_playgroupList, "playgroup");

    if (m_other && !m_other->m_maxepSpin)
        UIUtilE::Assign(m_screen, m_maxepSpin, "maxepisodes", err);
    else
        UIUtilW::Assign(m_screen, m_maxepSpin, "maxepisodes");

    if (m_other && !m_other->m_maxbehaviourList)
        UIUtilE::Assign(m_screen, m_maxbehaviourList, "maxnewest", err);
    else
        UIUtilW::Assign(m_screen, m_maxbehaviourList, "maxnewest");

    if (m_other && !m_other->m_autoexpireCheck)
        UIUtilE::Assign(m_screen, m_autoexpireCheck, "autoexpire", err);
    else
        UIUtilW::Assign(m_screen, m_autoexpireCheck, "autoexpire");
}

void StoreOptMixin::Load(void)
{
    if (!m_rule)
        return;

    QString label;
    QStringList groups;
    QStringList::Iterator it;
    MSqlQuery query(MSqlQuery::InitCon());

    // Recording Profile
    if (m_recprofileList)
    {
        if (!m_loaded)
        {
            label = QObject::tr("Record using the %1 profile");

            new MythUIButtonListItem(m_recprofileList,
                                        label.arg(QObject::tr("Default")),
                                        QVariant::fromValue(QString("Default")));
            // LiveTV profile - it's for LiveTV not scheduled recordings??
            new MythUIButtonListItem(m_recprofileList,
                                        label.arg(QObject::tr("LiveTV")),
                                        QVariant::fromValue(QString("LiveTV")));
            new MythUIButtonListItem(m_recprofileList,
                                        label.arg(QObject::tr("High Quality")),
                                        QVariant::fromValue(QString("High Quality")));
            new MythUIButtonListItem(m_recprofileList,
                                        label.arg(QObject::tr("Low Quality")),
                                        QVariant::fromValue(QString("Low Quality")));
        }
        m_recprofileList->SetValueByData(m_rule->m_recProfile);
    }

    // Recording Group
    if (m_recgroupList)
    {
        if (!m_loaded)
        {
            label = QObject::tr("Include in the \"%1\" recording group");
            new MythUIButtonListItem(m_recgroupList,
                                  QObject::tr("Create a new recording group"),
                                  QVariant::fromValue(QString("__NEW_GROUP__")));

            query.prepare("SELECT recgroupid, recgroup FROM recgroups "
                          "WHERE recgroup <> 'Deleted' AND "
                          "      recgroup <> 'LiveTV' "
                          "ORDER BY special DESC, recgroup ASC"); // Special groups first
            if (query.exec())
            {
                while (query.next())
                {
                    int id = query.value(0).toInt();
                    QString name = query.value(1).toString();

                    if (name == "Default")
                        name = QObject::tr("Default");
                    new MythUIButtonListItem(m_recgroupList, label.arg(name),
                                             QVariant::fromValue(id));
                }
            }

        }
        m_recgroupList->SetValueByData(m_rule->m_recGroupID);
    }

    // Storage Group
    if (m_storagegroupList)
    {
        if (!m_loaded)
        {
            label = QObject::tr("Store in the \"%1\" storage group");
            new MythUIButtonListItem(m_storagegroupList,
                                     label.arg(QObject::tr("Default")),
                                     QVariant::fromValue(QString("Default")));

            groups = StorageGroup::getRecordingsGroups();
            for (it = groups.begin(); it != groups.end(); ++it)
            {
                if ((*it).compare("Default", Qt::CaseInsensitive) != 0)
                    new MythUIButtonListItem(m_storagegroupList,
                                       label.arg(*it), QVariant::fromValue(*it));
            }
        }
        m_storagegroupList->SetValueByData(m_rule->m_storageGroup);
    }

    // Playback Group
    if (m_playgroupList)
    {
        if (!m_loaded)
        {
            label = QObject::tr("Use \"%1\" playback group settings");
            new MythUIButtonListItem(m_playgroupList,
                                     label.arg(QObject::tr("Default")),
                                     QVariant::fromValue(QString("Default")));

            groups = PlayGroup::GetNames();
            for (it = groups.begin(); it != groups.end(); ++it)
            {
                new MythUIButtonListItem(m_playgroupList, label.arg(*it),
                                         QVariant::fromValue(*it));
            }
        }
        m_playgroupList->SetValueByData(m_rule->m_playGroup);
    }

    // Max Episodes
    if (m_maxepSpin)
    {
        if (!m_loaded)
        {
            int maxEpisodes = m_rule->m_maxEpisodes;
            m_maxepSpin->SetRange(0,100,1,5);
            m_rule->m_maxEpisodes = maxEpisodes;
        }
        m_maxepSpin->SetValue(m_rule->m_maxEpisodes);
    }

    // Max Episode Behaviour
    if (m_maxbehaviourList)
    {
        if (!m_loaded)
        {
            new MythUIButtonListItem(m_maxbehaviourList,
                      QObject::tr("Don't record if this would exceed the max "
                                  "episodes"), QVariant::fromValue(false));
            new MythUIButtonListItem(m_maxbehaviourList,
                      QObject::tr("Delete oldest if this would exceed the max "
                                  "episodes"), QVariant::fromValue(true));
        }
        m_maxbehaviourList->SetValueByData(m_rule->m_maxNewest);
    }

    // Auto-Expire
    if (m_autoexpireCheck)
    {
        m_autoexpireCheck->SetCheckState(m_rule->m_autoExpire);
    }

    m_loaded = true;

    RuleChanged();
}

void StoreOptMixin::Save(void)
{
    if (!m_rule)
        return;

    if (m_recprofileList)
        m_rule->m_recProfile = m_recprofileList->GetDataValue().toString();

    if (m_recgroupList)
    {
        // If the user selected 'Create a new regroup' but failed to enter a
        // name when prompted, restore the original value
        if (m_recgroupList->GetDataValue().toString() == "__NEW_GROUP__")
            m_recgroupList->SetValueByData(m_rule->m_recGroupID);
        m_rule->m_recGroupID = m_recgroupList->GetDataValue().toInt();
    }

    if (m_storagegroupList)
        m_rule->m_storageGroup = m_storagegroupList->GetDataValue().toString();

    if (m_playgroupList)
        m_rule->m_playGroup = m_playgroupList->GetDataValue().toString();

    if (m_maxepSpin)
        m_rule->m_maxEpisodes = m_maxepSpin->GetIntValue();

    if (m_maxbehaviourList)
        m_rule->m_maxNewest = m_maxbehaviourList->GetDataValue().toBool();

    if (m_autoexpireCheck)
        m_rule->m_autoExpire = m_autoexpireCheck->GetBooleanCheckState();
}

void StoreOptMixin::RuleChanged(void)
{
    if (!m_rule)
        return;

    bool isScheduled = (m_rule->m_type != kNotRecording &&
                        m_rule->m_type != kDontRecord);
    bool isSingle = (m_rule->m_type == kSingleRecord ||
                     m_rule->m_type == kOverrideRecord);

    if (m_recprofileList)
        m_recprofileList->SetEnabled(isScheduled);
    if (m_recgroupList)
        m_recgroupList->SetEnabled(isScheduled);
    if (m_storagegroupList)
        m_storagegroupList->SetEnabled(isScheduled);
    if (m_playgroupList)
        m_playgroupList->SetEnabled(isScheduled);
    if (m_maxepSpin)
        m_maxepSpin->SetEnabled(isScheduled && !isSingle);
    if (m_maxbehaviourList)
        m_maxbehaviourList->SetEnabled(isScheduled && !isSingle &&
                                       m_rule->m_maxEpisodes != 0);
    if (m_autoexpireCheck)
        m_autoexpireCheck->SetEnabled(isScheduled);
}

void StoreOptMixin::MaxEpisodesChanged(MythUIButtonListItem *item)
{
    if (!item || !m_rule)
        return;

    m_rule->m_maxEpisodes = item->GetData().toInt();

    if (m_maxbehaviourList)
        m_maxbehaviourList->SetEnabled(m_rule->m_maxEpisodes != 0);
}

void StoreOptMixin::PromptForRecGroup(void)
{
    if (!m_rule)
        return;

    if (m_recgroupList->GetDataValue().toString() != "__NEW_GROUP__")
        return;

    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");

    QString label =
        QObject::tr("New Recording group name: ");

    auto *textDialog = new MythTextInputDialog(popupStack, label,
                static_cast<InputFilter>(FilterSymbols | FilterPunct));

    textDialog->SetReturnEvent(m_screen, "newrecgroup");

    if (textDialog->Create())
        popupStack->AddScreen(textDialog, false);
}

void StoreOptMixin::SetRecGroup(int recgroupID, QString recgroup)
{
    if (!m_rule || recgroupID <= 0)
        return;

    if (m_recgroupList)
    {
        recgroup = recgroup.trimmed();
        if (recgroup.isEmpty())
            return;

        QString label = QObject::tr("Include in the \"%1\" recording group");
        auto *item = new MythUIButtonListItem(m_recgroupList, label.arg(recgroup),
                                              QVariant::fromValue(recgroupID));
        m_recgroupList->SetItemCurrent(item);

        if (m_other && m_other->m_recgroupList)
        {
            item = new MythUIButtonListItem(m_other->m_recgroupList,
                             label.arg(recgroup), QVariant::fromValue(recgroupID));
            m_other->m_recgroupList->SetItemCurrent(item);
        }
    }
}

int StoreOptMixin::CreateRecordingGroup(const QString& groupName)
{
    int groupID = -1;
    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("INSERT INTO recgroups SET recgroup = :NAME, "
                  "displayname = :DISPLAYNAME");
    query.bindValue(":NAME", groupName);
    query.bindValue(":DISPLAYNAME", groupName);

    if (query.exec())
        groupID = query.lastInsertId().toInt();

    if (groupID <= 0)
        LOG(VB_GENERAL, LOG_ERR, QString("Could not create recording group (%1). "
                                         "Does it already exist?").arg(groupName));

    return groupID;
}

////////////////////////////////////////////////////////

/** \class PostProcMixin
 *  \brief Mixin for post processing
 *
 */

void PostProcMixin::Create(bool *err)
{
    if (!m_rule)
        return;

    if (m_other && !m_other->m_commflagCheck)
        UIUtilE::Assign(m_screen, m_commflagCheck, "autocommflag", err);
    else
        UIUtilW::Assign(m_screen, m_commflagCheck, "autocommflag");

    if (m_other && !m_other->m_transcodeCheck)
        UIUtilE::Assign(m_screen, m_transcodeCheck, "autotranscode", err);
    else
        UIUtilW::Assign(m_screen, m_transcodeCheck, "autotranscode");

    if (m_other && !m_other->m_transcodeprofileList)
        UIUtilE::Assign(m_screen, m_transcodeprofileList, "transcodeprofile", err);
    else
        UIUtilW::Assign(m_screen, m_transcodeprofileList, "transcodeprofile");

    if (m_other && !m_other->m_userjob1Check)
        UIUtilE::Assign(m_screen, m_userjob1Check, "userjob1", err);
    else
        UIUtilW::Assign(m_screen, m_userjob1Check, "userjob1");

    if (m_other && !m_other->m_userjob2Check)
        UIUtilE::Assign(m_screen, m_userjob2Check, "userjob2", err);
    else
        UIUtilW::Assign(m_screen, m_userjob2Check, "userjob2");

    if (m_other && !m_other->m_userjob3Check)
        UIUtilE::Assign(m_screen, m_userjob3Check, "userjob3", err);
    else
        UIUtilW::Assign(m_screen, m_userjob3Check, "userjob3");

    if (m_other && !m_other->m_userjob4Check)
        UIUtilE::Assign(m_screen, m_userjob4Check, "userjob4", err);
    else
        UIUtilW::Assign(m_screen, m_userjob4Check, "userjob4");

    UIUtilW::Assign(m_screen, m_metadataLookupCheck, "metadatalookup");
}

void PostProcMixin::Load(void)
{
    if (!m_rule)
        return;

    // Auto-commflag
    if (m_commflagCheck)
    {
        m_commflagCheck->SetCheckState(m_rule->m_autoCommFlag);
    }

    // Auto-transcode
    if (m_transcodeCheck)
    {
        m_transcodeCheck->SetCheckState(m_rule->m_autoTranscode);
    }

    // Transcode Method
    if (m_transcodeprofileList)
    {
        if (!m_loaded)
        {
            QMap<int, QString> profiles = RecordingProfile::GetTranscodingProfiles();
            QMap<int, QString>::iterator it;
            for (it = profiles.begin(); it != profiles.end(); ++it)
            {
                new MythUIButtonListItem(m_transcodeprofileList, it.value(),
                                        QVariant::fromValue(it.key()));
            }
        }
        m_transcodeprofileList->SetValueByData(m_rule->m_transcoder);
    }

    // User Job #1
    if (m_userjob1Check)
    {
        if (!m_loaded)
        {
        MythUIText *userjob1Text = nullptr;
        UIUtilW::Assign(m_screen, userjob1Text, "userjob1text");
        if (userjob1Text)
            userjob1Text->SetText(QObject::tr("Run '%1'")
                .arg(gCoreContext->GetSetting("UserJobDesc1", "User Job 1")));
        }
        m_userjob1Check->SetCheckState(m_rule->m_autoUserJob1);
    }

    // User Job #2
    if (m_userjob2Check)
    {
        if (!m_loaded)
        {
        MythUIText *userjob2Text = nullptr;
        UIUtilW::Assign(m_screen, userjob2Text, "userjob2text");
        if (userjob2Text)
            userjob2Text->SetText(QObject::tr("Run '%1'")
                .arg(gCoreContext->GetSetting("UserJobDesc2", "User Job 2")));
        }
        m_userjob2Check->SetCheckState(m_rule->m_autoUserJob2);
    }

    // User Job #3
    if (m_userjob3Check)
    {
        if (!m_loaded)
        {
        MythUIText *userjob3Text = nullptr;
        UIUtilW::Assign(m_screen, userjob3Text, "userjob3text");
        if (userjob3Text)
            userjob3Text->SetText(QObject::tr("Run '%1'")
                .arg(gCoreContext->GetSetting("UserJobDesc3", "User Job 3")));
        }
        m_userjob3Check->SetCheckState(m_rule->m_autoUserJob3);
    }

    // User Job #4
    if (m_userjob4Check)
    {
        if (!m_loaded)
        {
        MythUIText *userjob4Text = nullptr;
        UIUtilW::Assign(m_screen, userjob4Text, "userjob4text");
        if (userjob4Text)
            userjob4Text->SetText(QObject::tr("Run '%1'")
                .arg(gCoreContext->GetSetting("UserJobDesc4", "User Job 4")));
        }
        m_userjob4Check->SetCheckState(m_rule->m_autoUserJob4);
    }

    // Auto Metadata Lookup
    if (m_metadataLookupCheck)
    {
        m_metadataLookupCheck->SetCheckState(m_rule->m_autoMetadataLookup);
    }

    m_loaded = true;

    RuleChanged();
}

void PostProcMixin::Save(void)
{
    if (!m_rule)
        return;

    if (m_commflagCheck)
        m_rule->m_autoCommFlag = m_commflagCheck->GetBooleanCheckState();
    if (m_transcodeCheck)
        m_rule->m_autoTranscode = m_transcodeCheck->GetBooleanCheckState();
    if (m_transcodeprofileList)
        m_rule->m_transcoder = m_transcodeprofileList->GetDataValue().toInt();
    if (m_userjob1Check)
        m_rule->m_autoUserJob1 = m_userjob1Check->GetBooleanCheckState();
    if (m_userjob2Check)
        m_rule->m_autoUserJob2 = m_userjob2Check->GetBooleanCheckState();
    if (m_userjob3Check)
        m_rule->m_autoUserJob3 = m_userjob3Check->GetBooleanCheckState();
    if (m_userjob4Check)
        m_rule->m_autoUserJob4 = m_userjob4Check->GetBooleanCheckState();
    if (m_metadataLookupCheck)
        m_rule->m_autoMetadataLookup =
            m_metadataLookupCheck->GetBooleanCheckState();
}

void PostProcMixin::RuleChanged(void)
{
    if (!m_rule)
        return;

    bool isScheduled = (m_rule->m_type != kNotRecording &&
                        m_rule->m_type != kDontRecord);

    if (m_commflagCheck)
        m_commflagCheck->SetEnabled(isScheduled);
    if (m_transcodeCheck)
        m_transcodeCheck->SetEnabled(isScheduled);
    if (m_transcodeprofileList)
        m_transcodeprofileList->SetEnabled(isScheduled &&
                                           m_rule->m_autoTranscode);
    if (m_userjob1Check)
        m_userjob1Check->SetEnabled(isScheduled);
    if (m_userjob2Check)
        m_userjob2Check->SetEnabled(isScheduled);
    if (m_userjob3Check)
        m_userjob3Check->SetEnabled(isScheduled);
    if (m_userjob4Check)
        m_userjob4Check->SetEnabled(isScheduled);
    if (m_metadataLookupCheck)
        m_metadataLookupCheck->SetEnabled(isScheduled);
}

void PostProcMixin::TranscodeChanged(bool enable)
{
    if (!m_rule)
        return;

    m_rule->m_autoTranscode = enable;

    if (m_transcodeprofileList)
        m_transcodeprofileList->SetEnabled(m_rule->m_autoTranscode);
}
