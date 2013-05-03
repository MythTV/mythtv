
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
static QString fs8(QT_TRANSLATE_NOOP("SchedFilterEditor", "This time"));
static QString fs9(QT_TRANSLATE_NOOP("SchedFilterEditor", "This day and time"));
static QString fs10(QT_TRANSLATE_NOOP("SchedFilterEditor", "This channel"));

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
            SchedOptMixin(*this, NULL), StoreOptMixin(*this, NULL),
            PostProcMixin(*this, NULL),
            m_recInfo(new RecordingInfo(*recInfo)), m_recordingRule(NULL),
            m_sendSig(false),
            m_saveButton(NULL), m_cancelButton(NULL), m_rulesList(NULL),
            m_schedOptButton(NULL), m_storeOptButton(NULL),
            m_postProcButton(NULL), m_schedInfoButton(NULL),
            m_previewButton(NULL), m_metadataButton(NULL),
            m_filtersButton(NULL),
            m_player(player), m_loaded(false), m_view(kMainView), m_child(NULL)
{
    m_recordingRule = new RecordingRule();
    m_recordingRule->m_recordID = m_recInfo->GetRecordingRuleID();
    SchedOptMixin::SetRule(m_recordingRule);
    StoreOptMixin::SetRule(m_recordingRule);
    PostProcMixin::SetRule(m_recordingRule);
}

ScheduleEditor::ScheduleEditor(MythScreenStack *parent,
                               RecordingRule *recRule, TV *player)
          : ScheduleCommon(parent, "ScheduleEditor"),
            SchedOptMixin(*this, recRule),
            StoreOptMixin(*this, recRule),
            PostProcMixin(*this, recRule),
            m_recInfo(NULL), m_recordingRule(recRule),
            m_sendSig(false),
            m_saveButton(NULL), m_cancelButton(NULL), m_rulesList(NULL),
            m_schedOptButton(NULL), m_storeOptButton(NULL),
            m_postProcButton(NULL), m_schedInfoButton(NULL),
            m_previewButton(NULL), m_metadataButton(NULL),
            m_filtersButton(NULL),
            m_player(player), m_loaded(false), m_view(kMainView), m_child(NULL)
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

    UIUtilW::Assign(this, m_schedOptButton, "schedoptions");
    UIUtilW::Assign(this, m_storeOptButton, "storeoptions");
    UIUtilW::Assign(this, m_postProcButton, "postprocessing");
    UIUtilW::Assign(this, m_metadataButton, "metadata");
    UIUtilW::Assign(this, m_schedInfoButton, "schedinfo");
    UIUtilW::Assign(this, m_previewButton, "preview");
    UIUtilW::Assign(this, m_filtersButton, "filters");

    SchedOptMixin::Create(&err);
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

    connect(m_rulesList, SIGNAL(itemSelected(MythUIButtonListItem *)),
                         SLOT(RuleChanged(MythUIButtonListItem *)));

    if (m_schedOptButton)
        connect(m_schedOptButton, SIGNAL(Clicked()), SLOT(ShowSchedOpt()));
    if (m_filtersButton)
        connect(m_filtersButton, SIGNAL(Clicked()), SLOT(ShowFilters()));
    if (m_storeOptButton)
        connect(m_storeOptButton, SIGNAL(Clicked()), SLOT(ShowStoreOpt()));
    if (m_postProcButton)
        connect(m_postProcButton, SIGNAL(Clicked()), SLOT(ShowPostProc()));
    if (m_schedInfoButton)
        connect(m_schedInfoButton, SIGNAL(Clicked()), SLOT(ShowSchedInfo()));
    if (m_previewButton)
        connect(m_previewButton, SIGNAL(Clicked()), SLOT(ShowPreview()));
    if (m_metadataButton)
        connect(m_metadataButton, SIGNAL(Clicked()), SLOT(ShowMetadataOptions()));

    if (m_cancelButton)
        connect(m_cancelButton, SIGNAL(Clicked()), SLOT(Close()));
    connect(m_saveButton, SIGNAL(Clicked()), SLOT(Save()));

    if (m_schedInfoButton)
        m_schedInfoButton->SetEnabled(!m_recordingRule->m_isTemplate);
    if (m_previewButton)
        m_previewButton->SetEnabled(!m_recordingRule->m_isTemplate);

    if (m_dupmethodList)
        connect(m_dupmethodList, SIGNAL(itemSelected(MythUIButtonListItem *)),
                SLOT(DupMethodChanged(MythUIButtonListItem *)));
    if (m_maxepSpin)
        connect(m_maxepSpin, SIGNAL(itemSelected(MythUIButtonListItem *)),
                SLOT(MaxEpisodesChanged(MythUIButtonListItem *)));
    if (m_recgroupList)
        connect(m_recgroupList, SIGNAL(LosingFocus()), 
                SLOT(PromptForRecGroup()));
    if (m_transcodeCheck)
        connect(m_transcodeCheck, SIGNAL(toggled(bool)),
                SLOT(TranscodeChanged(bool)));

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
                                         ENUM_TO_QVARIANT(kNotRecording));
            }
            new MythUIButtonListItem(m_rulesList,
                                     tr("Modify this recording rule template"),
                                     ENUM_TO_QVARIANT(kTemplateRecord));
        }
        else if (m_recordingRule->m_isOverride)
        {
            new MythUIButtonListItem(m_rulesList,
                                 tr("Record this showing with normal options"),
                                     ENUM_TO_QVARIANT(kNotRecording));
            new MythUIButtonListItem(m_rulesList,
                               tr("Record this showing with override options"),
                                     ENUM_TO_QVARIANT(kOverrideRecord));
            new MythUIButtonListItem(m_rulesList,
                                tr("Do not record this showing"),
                                     ENUM_TO_QVARIANT(kDontRecord));
        }
        else
        {
            bool hasChannel = !m_recordingRule->m_station.isEmpty();
            bool isManual = (m_recordingRule->m_searchType == kManualSearch);

            new MythUIButtonListItem(m_rulesList, 
                                     tr("Do not record this program"),
                                     ENUM_TO_QVARIANT(kNotRecording));
            if (hasChannel)
                new MythUIButtonListItem(m_rulesList,
                                         tr("Record only this showing"),
                                         ENUM_TO_QVARIANT(kSingleRecord));
            if (!isManual)
                new MythUIButtonListItem(m_rulesList,
                                         tr("Record only one showing"),
                                         ENUM_TO_QVARIANT(kOneRecord));
            if (!hasChannel || isManual)
                new MythUIButtonListItem(m_rulesList,
                                         tr("Record one showing every week"),
                                         ENUM_TO_QVARIANT(kWeeklyRecord));
            if (!hasChannel || isManual)
                new MythUIButtonListItem(m_rulesList,
                                         tr("Record one showing every day"),
                                         ENUM_TO_QVARIANT(kDailyRecord));
            if (!isManual)
                new MythUIButtonListItem(m_rulesList,
                                         tr("Record all showings"),
                                         ENUM_TO_QVARIANT(kAllRecord));
        }

        m_recordingRule->m_type = type;
    }
    m_rulesList->SetValueByData(ENUM_TO_QVARIANT(m_recordingRule->m_type));

    InfoMap progMap;

    m_recordingRule->ToMap(progMap);

    if (m_recInfo)
        m_recInfo->ToMap(progMap);

    SetTextFromMap(progMap);

    m_loaded = true;
}

void ScheduleEditor::LoadTemplate(QString name)
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
    StoreOptMixin::RuleChanged();
    PostProcMixin::RuleChanged();
}

void ScheduleEditor::DupMethodChanged(MythUIButtonListItem *item)
{
    SchedOptMixin::DupMethodChanged(item);
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
    SchedOptEditor *schedoptedit = new SchedOptEditor(mainStack, *this,
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
    StoreOptEditor *storeoptedit = new StoreOptEditor(mainStack, *this,
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
    PostProcEditor *ppedit = new PostProcEditor(mainStack, *this,
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
    MythDialogBox *menuPopup = new MythDialogBox(label, popupStack, "menuPopup");

    if (menuPopup->Create())
    {
        menuPopup->SetReturnEvent(this, "schedinfo");

        if (m_recInfo)
            menuPopup->AddButton(tr("Program Details"));
        menuPopup->AddButton(tr("Upcoming Episodes"));
        menuPopup->AddButton(tr("Upcoming Recordings"));
        menuPopup->AddButton(tr("Previously Scheduled"));

        popupStack->AddScreen(menuPopup);
    }
    else
        delete menuPopup;
}

bool ScheduleEditor::keyPressEvent(QKeyEvent *event)
{
    if (GetFocusWidget()->keyPressEvent(event))
        return true;

    bool handled = false;
    QStringList actions;
    handled = GetMythMainWindow()->
        TranslateKeyPress("TV Frontend", event, actions);

    for (int i = 0; i < actions.size() && !handled; i++)
    {   
        QString action = actions[i];
        handled = true;

        if (action == "MENU")
            showMenu();
        else if (action == "INFO")
            ShowDetails(m_recInfo);
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
        DialogCompletionEvent *dce = (DialogCompletionEvent*)(event);

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
                ShowDetails(m_recInfo);
            else if (resulttext == tr("Upcoming Episodes"))
                showUpcomingByTitle();
            else if (resulttext == tr("Upcoming Recordings"))
                showUpcomingByRule();
            else if (resulttext == tr("Previously Scheduled"))
                showPrevious();
        }
        else if (resultid == "newrecgroup")
        {
            StoreOptMixin::SetRecGroup(resulttext);
        }
    }
}

void ScheduleEditor::showPrevious(void)
{
    if (m_recordingRule->m_type == kTemplateRecord)
        return;

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
    if (m_recordingRule->m_type == kTemplateRecord)
        return;

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
    if (m_recordingRule->m_type == kTemplateRecord)
        return;

    // Existing rule and search?  Search by rule
    if (m_recordingRule->m_recordID > 0 && 
        m_recordingRule->m_searchType != kNoSearch)
        showUpcomingByRule();

    QString title = m_recordingRule->m_title;

    if (m_recordingRule->m_searchType != kNoSearch)
        title.remove(QRegExp(" \\(.*\\)$"));

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
    StoreOptMixin::Save();
    PostProcMixin::Save();

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
    if (m_recordingRule->m_type == kNotRecording ||
        m_recordingRule->m_type == kDontRecord ||
        m_recordingRule->m_type == kTemplateRecord)
        return;

    if (m_child)
        m_child->Close();

    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
    MetadataOptions *rad = new MetadataOptions(mainStack, *this,
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

    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
    SchedFilterEditor *schedfilteredit = new SchedFilterEditor(mainStack,
                                           *this, *m_recordingRule, m_recInfo);
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
    else if (m_view == kMainView)
        ShowPostProc();
    else if (m_view == kSchedOptView)
        m_child->Close();
    else if (m_view == kFilterView)
        ShowSchedOpt();
    else if (m_view == kStoreOptView)
        ShowFilters();
    else if (m_view == kPostProcView)
        ShowStoreOpt();
    else if (m_view == kMetadataView)
        ShowPostProc();
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
    else if (m_view == kPostProcView)
        m_child->Close();
    else if (m_view == kMetadataView)
        m_child->Close();
}

void ScheduleEditor::ChildClosing(void)
{
    if (m_view == kSchedOptView)
        SchedOptMixin::Load();
    else if (m_view == kStoreOptView)
        StoreOptMixin::Load();
    else if (m_view == kPostProcView)
        PostProcMixin::Load();

    m_child = NULL;
    m_view = kMainView;
}

void ScheduleEditor::showMenu(void)
{
    QString label = tr("Options");
    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");
    MythDialogBox *menuPopup = 
        new MythDialogBox(label, popupStack, "menuPopup");

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
            menuPopup->AddButton(tr("Schedule Info"));
        if (!m_recordingRule->m_isTemplate)
            menuPopup->AddButton(tr("Preview Changes"));
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
    MythDialogBox *menuPopup = 
        new MythDialogBox(label, popupStack, "menuPopup");   

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

SchedEditChild::SchedEditChild(MythScreenStack *parent, const QString name,
                               ScheduleEditor &editor, RecordingRule &rule,
                               RecordingInfo *recInfo)
    : MythScreenType(parent, name),
      m_editor(&editor), m_recordingRule(&rule), m_recInfo(recInfo),
      m_backButton(NULL), m_saveButton(NULL), m_previewButton(NULL)
{
}

SchedEditChild::~SchedEditChild(void)
{
}

bool SchedEditChild::keyPressEvent(QKeyEvent *event)
{
    if (GetFocusWidget()->keyPressEvent(event))
        return true;

    bool handled = false;
    QStringList actions;
    handled = GetMythMainWindow()->
        TranslateKeyPress("TV Frontend", event, actions);

    for (int i = 0; i < actions.size() && !handled; i++)
    {
        QString action = actions[i];
        handled = true;

        if (action == "MENU")
            m_editor->showMenu();
        else if (action == "INFO")
            m_editor->ShowDetails(m_recInfo);
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
    const QString xmlfile, const QString winname, bool isTemplate)
{
    if (!LoadWindowFromXML(xmlfile, winname, this))
        return false;

    UIUtilW::Assign(this, m_backButton, "back");
    UIUtilW::Assign(this, m_saveButton, "save");
    UIUtilW::Assign(this, m_previewButton, "preview");

    connect(this, SIGNAL(Closing()), m_editor, SLOT(ChildClosing()));
    connect(m_editor, SIGNAL(templateLoaded()), SLOT(Load()));

    if (m_backButton)
        connect(m_backButton, SIGNAL(Clicked()), SLOT(Close()));
    if (m_saveButton)
        connect(m_saveButton, SIGNAL(Clicked()), m_editor, SLOT(Save()));
    if (m_previewButton)
        connect(m_previewButton, SIGNAL(Clicked()),
                m_editor, SLOT(ShowPreview()));

    if (m_previewButton)
        m_previewButton->SetEnabled(!isTemplate);

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
      SchedOptMixin(*this, &rule, &editor), m_filtersButton(NULL)
{
}

SchedOptEditor::~SchedOptEditor(void)
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
        connect(m_dupmethodList, SIGNAL(itemSelected(MythUIButtonListItem *)),
                SLOT(DupMethodChanged(MythUIButtonListItem *)));

    if (m_filtersButton)
        connect(m_filtersButton, SIGNAL(Clicked()),
                m_editor, SLOT(ShowFilters()));

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
      m_filtersList(NULL), m_loaded(false)
{
}

SchedFilterEditor::~SchedFilterEditor(void)
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

    UIUtilE::Assign(this, m_filtersList, "filters", &err);

    if (err)
    {
        LOG(VB_GENERAL, LOG_ERR, "SchedFilterEditor, theme is missing "
                                 "required elements");
        return false;
    }

    connect(m_filtersList, SIGNAL(itemClicked(MythUIButtonListItem *)),
            SLOT(ToggleSelected(MythUIButtonListItem *)));

    BuildFocusList();

    return true;
}

void SchedFilterEditor::Load()
{
    int filterid;
    MythUIButtonListItem *button;

    if (!m_loaded)
    {
        MSqlQuery query(MSqlQuery::InitCon());

        query.prepare("SELECT filterid, description, newruledefault "
                      "FROM recordfilter ORDER BY filterid");

        if (query.exec())
        {
            while (query.next())
            {
                filterid = query.value(0).toInt();
                QString description = tr(query.value(1).toString()
                                         .toUtf8().constData());
                // Fill in list of possible filters
                button = new MythUIButtonListItem(m_filtersList, description,
                                                  filterid);
                button->setCheckable(true);
            }
        }
    }

    int idx, end = m_filtersList->GetCount();
    for (idx = 0; idx < end; ++idx)
    {
        button = m_filtersList->GetItemAt(idx);
        int filterid = qVariantValue<int>(button->GetData());
        button->setChecked(m_recordingRule->m_filter & (1 << filterid) ?
                           MythUIButtonListItem::FullChecked :
                           MythUIButtonListItem::NotChecked);
    }

    SetTextFromMaps();

    m_loaded = true;
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

StoreOptEditor::~StoreOptEditor(void)
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
        connect(m_maxepSpin, SIGNAL(itemSelected(MythUIButtonListItem *)),
                SLOT(MaxEpisodesChanged(MythUIButtonListItem *)));
    if (m_recgroupList)
        connect(m_recgroupList, SIGNAL(LosingFocus()), 
                SLOT(PromptForRecGroup()));

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
        DialogCompletionEvent *dce = (DialogCompletionEvent*)(event);

        QString resultid   = dce->GetId();
        QString resulttext = dce->GetResultText();

        if (resultid == "newrecgroup")
        {
            StoreOptMixin::SetRecGroup(resulttext);
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

PostProcEditor::~PostProcEditor(void)
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
        connect(m_transcodeCheck, SIGNAL(toggled(bool)),
                SLOT(TranscodeChanged(bool)));

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
    : SchedEditChild(parent, "MetadataOptions", editor, rule, recInfo),
      m_lookup(NULL),
      m_busyPopup(NULL), m_fanart(NULL), m_coverart(NULL),
      m_banner(NULL), m_inetrefEdit(NULL), m_seasonSpin(NULL),
      m_episodeSpin(NULL), m_queryButton(NULL), m_localFanartButton(NULL),
      m_localCoverartButton(NULL), m_localBannerButton(NULL),
      m_onlineFanartButton(NULL), m_onlineCoverartButton(NULL),
      m_onlineBannerButton(NULL)
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
    if (!SchedEditChild::CreateEditChild(
            "schedule-ui.xml", "metadataoptions",
            m_recordingRule->m_isTemplate))
    {
        return false;
    }

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

    if (err)
    {
        LOG(VB_GENERAL, LOG_ERR, "MetadataOptions, theme is missing "
                                 "required elements");
        return false;
    }

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
    m_seasonSpin->SetRange(0,9999,1,5);
    m_seasonSpin->SetValue(m_recordingRule->m_season);

    // Episode
    m_episodeSpin->SetRange(0,9999,1,10);
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
    SetTextFromMaps();
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

    CreateBusyDialog(tr("Trying to manually find this "
                        "recording online..."));

    m_lookup->SetStep(kLookupSearch);
    m_lookup->SetType(kMetadataRecording);
    if (m_seasonSpin->GetIntValue() > 0 ||
           m_episodeSpin->GetIntValue() > 0)
        m_lookup->SetSubtype(kProbableTelevision);
    else
        m_lookup->SetSubtype(kProbableMovie);
    m_lookup->SetAllowGeneric(true);
    m_lookup->SetAutomatic(false);
    m_lookup->SetHandleImages(false);
    m_lookup->SetHost(gCoreContext->GetMasterHostName());
    m_lookup->SetTitle(m_recordingRule->m_title);
    m_lookup->SetSubtitle(m_recordingRule->m_subtitle);
    m_lookup->SetInetref(m_inetrefEdit->GetText());
    m_lookup->SetCollectionref(m_inetrefEdit->GetText());
    m_lookup->SetSeason(m_seasonSpin->GetIntValue());
    m_lookup->SetEpisode(m_episodeSpin->GetIntValue());

    m_metadataFactory->Lookup(m_lookup);
}

void MetadataOptions::OnSearchListSelection(MetadataLookup *lookup)
{
    QueryComplete(lookup);
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
    m_lookup->SetAllowGeneric(true);
    m_lookup->SetData(qVariantFromValue<VideoArtworkType>(type));
    m_lookup->SetHost(gCoreContext->GetMasterHostName());
    m_lookup->SetTitle(m_recordingRule->m_title);
    m_lookup->SetSubtitle(m_recordingRule->m_subtitle);
    m_lookup->SetInetref(m_inetrefEdit->GetText());
    m_lookup->SetCollectionref(m_inetrefEdit->GetText());
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

    if (map.isEmpty())
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

        QString title = tr("No match found for this recording. You can "
                           "try entering a TVDB/TMDB number, season, and "
                           "episode manually.");

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

            QString title = tr("This number, season, and episode combination "
                               "does not appear to be valid (or the site may "
                               "be down). Check your information and try "
                               "again.");

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

////////////////////////////////////////////////////////

/** \class SchedOptMixin
 *  \brief Mixin for schedule options
 *
 */

SchedOptMixin::SchedOptMixin(MythScreenType &screen, RecordingRule *rule,
                             SchedOptMixin *other)
    : m_prioritySpin(NULL), m_startoffsetSpin(NULL), m_endoffsetSpin(NULL), 
      m_dupmethodList(NULL), m_dupscopeList(NULL), m_inputList(NULL), 
      m_ruleactiveCheck(NULL), m_newrepeatList(NULL),
      m_screen(&screen), m_rule(rule), m_other(other), m_loaded(false),
      m_haveRepeats(gCoreContext->GetNumSetting("HaveRepeats", 0))
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
               QObject::tr("Match duplicates using subtitle & description"),
                                     ENUM_TO_QVARIANT(kDupCheckSubDesc));
            new MythUIButtonListItem(m_dupmethodList,
               QObject::tr("Match duplicates using subtitle then description"),
                                     ENUM_TO_QVARIANT(kDupCheckSubThenDesc));
            new MythUIButtonListItem(m_dupmethodList,
               QObject::tr("Match duplicates using subtitle"),
                                     ENUM_TO_QVARIANT(kDupCheckSub));
            new MythUIButtonListItem(m_dupmethodList,
               QObject::tr("Match duplicates using description"),
                                     ENUM_TO_QVARIANT(kDupCheckDesc));
            new MythUIButtonListItem(m_dupmethodList,
               QObject::tr("Don't match duplicates"),
                                     ENUM_TO_QVARIANT(kDupCheckNone));

            m_rule->m_dupMethod = dupMethod;
        }
        m_dupmethodList->SetValueByData(ENUM_TO_QVARIANT(m_rule->m_dupMethod));
    }

    // Duplicate Matching Scope
    if (m_dupscopeList)
    {
        if (!m_loaded)
        {
            new MythUIButtonListItem(m_dupscopeList,
                QObject::tr("Look for duplicates in current and previous "
                            "recordings"), ENUM_TO_QVARIANT(kDupsInAll));
            new MythUIButtonListItem(m_dupscopeList,
                QObject::tr("Look for duplicates in current recordings only"),
                                     ENUM_TO_QVARIANT(kDupsInRecorded));
            new MythUIButtonListItem(m_dupscopeList,
                QObject::tr("Look for duplicates in previous recordings only"),
                                     ENUM_TO_QVARIANT(kDupsInOldRecorded));
            if (m_haveRepeats && !m_newrepeatList && 
                (!m_other || !m_other->m_newrepeatList))
            {
                new MythUIButtonListItem(m_dupscopeList,
                    QObject::tr("Record new episodes only"),
                                 ENUM_TO_QVARIANT(kDupsNewEpi|kDupsInAll));
            }
        }
        m_dupscopeList->SetValueByData(ENUM_TO_QVARIANT(m_rule->m_dupIn));
    }

    // Preferred Input
    if (m_inputList)
    {
        if (!m_loaded)
        {
            new MythUIButtonListItem(m_inputList, 
                                     QObject::tr("Use any available input"),
                                     qVariantFromValue(0));

            vector<uint> inputids = CardUtil::GetAllInputIDs();
            for (uint i = 0; i < inputids.size(); ++i)
            {
                new MythUIButtonListItem(m_inputList, 
                    QObject::tr("Prefer input %1")
                    .arg(CardUtil::GetDisplayName(inputids[i])), inputids[i]);
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
                                         "episodes"), ENUM_TO_QVARIANT(0));
            new MythUIButtonListItem(m_newrepeatList,
                                     QObject::tr("Record new episodes only"),
                                     ENUM_TO_QVARIANT(kDupsNewEpi));
        }
        m_newrepeatList->SetValueByData(ENUM_TO_QVARIANT
                                        (m_rule->m_dupIn & kDupsNewEpi));
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

    if (m_prioritySpin)
        m_prioritySpin->SetEnabled(isScheduled);
    if (m_startoffsetSpin)
        m_startoffsetSpin->SetEnabled(isScheduled);
    if (m_endoffsetSpin)
        m_endoffsetSpin->SetEnabled(isScheduled);
    if (m_dupmethodList)
        m_dupmethodList->SetEnabled(isScheduled && !isSingle);
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

/** \class StoreOptMixin
 *  \brief Mixin for storage options
 *
 */

StoreOptMixin::StoreOptMixin(MythScreenType &screen, RecordingRule *rule,
                             StoreOptMixin *other)
    : m_recprofileList(NULL), m_recgroupList(NULL), m_storagegroupList(NULL), 
      m_playgroupList(NULL), m_maxepSpin(NULL), m_maxbehaviourList(NULL), 
      m_autoexpireCheck(NULL),
      m_screen(&screen), m_rule(rule), m_other(other), m_loaded(false)
{
}

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
            QMap<int, QString> profiles = RecordingProfile::listProfiles(0);
            QMap<int, QString>::iterator pit;
            for (pit = profiles.begin(); pit != profiles.end(); ++pit)
            {
                new MythUIButtonListItem(m_recprofileList, 
                                         label.arg(pit.value()),
                                         qVariantFromValue(pit.value()));
            }
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
                                  qVariantFromValue(QString("__NEW_GROUP__")));
            new MythUIButtonListItem(m_recgroupList, 
                                     label.arg(QObject::tr("Default")),
                                     qVariantFromValue(QString("Default")));

            groups.clear();
            if (m_rule->m_recGroup != "Default" &&
                m_rule->m_recGroup != "__NEW_GROUP__")
                groups << m_rule->m_recGroup;
            query.prepare("SELECT DISTINCT recgroup FROM recorded "
                          "WHERE recgroup <> 'Default' AND "
                          "      recgroup <> 'Deleted'");
            if (query.exec())
            {
                while (query.next())
                    groups += query.value(0).toString();
            }
            query.prepare("SELECT DISTINCT recgroup FROM record "
                          "WHERE recgroup <> 'Default'");
            if (query.exec())
            {
                while (query.next())
                    groups += query.value(0).toString();
            }

            groups.sort();
            groups.removeDuplicates();
            for (it = groups.begin(); it != groups.end(); ++it)
            {
                new MythUIButtonListItem(m_recgroupList, label.arg(*it),
                                         qVariantFromValue(*it));
            }
        }
        m_recgroupList->SetValueByData(m_rule->m_recGroup);
    }

    // Storage Group
    if (m_storagegroupList)
    {
        if (!m_loaded)
        {
            label = QObject::tr("Store in the \"%1\" storage group");
            new MythUIButtonListItem(m_storagegroupList, 
                                     label.arg(QObject::tr("Default")),
                                     qVariantFromValue(QString("Default")));

            groups = StorageGroup::getRecordingsGroups();
            for (it = groups.begin(); it != groups.end(); ++it)
            {
                if ((*it).compare("Default", Qt::CaseInsensitive) != 0)
                    new MythUIButtonListItem(m_storagegroupList, 
                                       label.arg(*it), qVariantFromValue(*it));
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
                                     qVariantFromValue(QString("Default")));

            groups = PlayGroup::GetNames();
            for (it = groups.begin(); it != groups.end(); ++it)
            {
                new MythUIButtonListItem(m_playgroupList, label.arg(*it),
                                         qVariantFromValue(*it));
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
                                  "episodes"), qVariantFromValue(false));
            new MythUIButtonListItem(m_maxbehaviourList,
                      QObject::tr("Delete oldest if this would exceed the max "
                                  "episodes"), qVariantFromValue(true));
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
            m_recgroupList->SetValueByData(m_rule->m_recGroup);
        m_rule->m_recGroup = m_recgroupList->GetDataValue().toString();
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
        QObject::tr("Create New Recording Group. Enter group name: ");

    MythTextInputDialog *textDialog = 
        new MythTextInputDialog(popupStack, label,
                static_cast<InputFilter>(FilterSymbols | FilterPunct));

    textDialog->SetReturnEvent(m_screen, "newrecgroup");

    if (textDialog->Create())
        popupStack->AddScreen(textDialog, false);
}

void StoreOptMixin::SetRecGroup(QString recgroup)
{
    if (!m_rule)
        return;

    if (m_recgroupList)
    {
        recgroup = recgroup.trimmed();
        if (recgroup.isEmpty())
            return;

        QString label = QObject::tr("Include in the \"%1\" recording group");
        MythUIButtonListItem *item =
            new MythUIButtonListItem(m_recgroupList, label.arg(recgroup),
                                     qVariantFromValue(recgroup));
        m_recgroupList->SetItemCurrent(item);

        if (m_other && m_other->m_recgroupList)
        {
            item = new MythUIButtonListItem(m_other->m_recgroupList,
                             label.arg(recgroup), qVariantFromValue(recgroup));
            m_other->m_recgroupList->SetItemCurrent(item);
        }
    }
}

////////////////////////////////////////////////////////

/** \class PostProcMixin
 *  \brief Mixin for post processing
 *
 */

PostProcMixin::PostProcMixin(MythScreenType &screen, RecordingRule *rule,
                             PostProcMixin *other)
    : m_commflagCheck(NULL), m_transcodeCheck(NULL), 
      m_transcodeprofileList(NULL), m_userjob1Check(NULL), 
      m_userjob2Check(NULL), m_userjob3Check(NULL), m_userjob4Check(NULL), 
      m_metadataLookupCheck(NULL),
      m_screen(&screen), m_rule(rule), m_other(other), m_loaded(false)
{
}

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
        QMap<int, QString> profiles = 
            RecordingProfile::listProfiles(RecordingProfile::TranscoderGroup);
        QMap<int, QString>::iterator it;
        for (it = profiles.begin(); it != profiles.end(); ++it)
        {
            new MythUIButtonListItem(m_transcodeprofileList, it.value(),
                                     qVariantFromValue(it.key()));
        }
        }
        m_transcodeprofileList->SetValueByData(m_rule->m_transcoder);
    }

    // User Job #1
    if (m_userjob1Check)
    {
        if (!m_loaded)
        {
        MythUIText *userjob1Text = NULL;
        UIUtilW::Assign(m_screen, userjob1Text, "userjob1text");
        if (userjob1Text)
            userjob1Text->SetText(QObject::tr("Run '%1'")
                .arg(gCoreContext->GetSetting("UserJobDesc1"), "User Job 1"));
        }
        m_userjob1Check->SetCheckState(m_rule->m_autoUserJob1);
    }

    // User Job #2
    if (m_userjob2Check)
    {
        if (!m_loaded)
        {
        MythUIText *userjob2Text = NULL;
        UIUtilW::Assign(m_screen, userjob2Text, "userjob2text");
        if (userjob2Text)
            userjob2Text->SetText(QObject::tr("Run '%1'")
                .arg(gCoreContext->GetSetting("UserJobDesc2"), "User Job 2"));
        }
        m_userjob2Check->SetCheckState(m_rule->m_autoUserJob2);
    }

    // User Job #3
    if (m_userjob3Check)
    {
        if (!m_loaded)
        {
        MythUIText *userjob3Text = NULL;
        UIUtilW::Assign(m_screen, userjob3Text, "userjob3text");
        if (userjob3Text)
            userjob3Text->SetText(QObject::tr("Run '%1'")
                .arg(gCoreContext->GetSetting("UserJobDesc3"), "User Job 3"));
        }
        m_userjob3Check->SetCheckState(m_rule->m_autoUserJob3);
    }

    // User Job #4
    if (m_userjob4Check)
    {
        if (!m_loaded)
        {
        MythUIText *userjob4Text = NULL;
        UIUtilW::Assign(m_screen, userjob4Text, "userjob4text");
        if (userjob4Text)
            userjob4Text->SetText(QObject::tr("Run '%1'")
                .arg(gCoreContext->GetSetting("UserJobDesc4"), "User Job 4"));
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

