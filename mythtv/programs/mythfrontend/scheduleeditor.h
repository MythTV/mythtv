#ifndef SCHEDULERECORDING_H_
#define SCHEDULERECORDING_H_

// MythTV
#include "libmyth/mythcontext.h"
#include "libmythbase/mythdb.h"
#include "libmythmetadata/metadatafactory.h"
#include "libmythtv/recordinginfo.h"
#include "libmythtv/recordingrule.h"
#include "libmythui/mythscreentype.h"

// MythFrontend
#include "schedulecommon.h"

class ProgramInfo;
class MythUIText;
class MythUIButton;
class MythUIButtonList;
class MythUIButtonListItem;
class MythUIStateType;
class MythUISpinBox;
class TV;

class ScheduleEditor;
class SchedEditChild;

class SchedOptMixin
{
  protected:
    SchedOptMixin(MythScreenType &screen, RecordingRule *rule,
                  SchedOptMixin *other = nullptr);
    void SetRule(RecordingRule *rule) { m_rule = rule; };
    void Create(bool *err);
    void Load(void);
    void Save(void);
    void RuleChanged(void);
    void DupMethodChanged(MythUIButtonListItem *item);

    MythUISpinBox    *m_prioritySpin    {nullptr};
    MythUISpinBox    *m_startoffsetSpin {nullptr};
    MythUISpinBox    *m_endoffsetSpin   {nullptr};
    MythUIButtonList *m_dupmethodList   {nullptr};
    MythUIButtonList *m_dupscopeList    {nullptr};
    MythUIButtonList *m_autoExtendList  {nullptr};
    MythUIButtonList *m_inputList       {nullptr};
    MythUICheckBox   *m_ruleactiveCheck {nullptr};
    MythUIButtonList *m_newrepeatList   {nullptr};

  private:
    MythScreenType   *m_screen          {nullptr};
    RecordingRule    *m_rule            {nullptr};
    SchedOptMixin    *m_other           {nullptr};
    bool              m_loaded          {false};
    bool              m_haveRepeats     {false};
};

class StoreOptMixin
{
  protected:
    StoreOptMixin(MythScreenType &screen, RecordingRule *rule,
                  StoreOptMixin *other = nullptr)
        : m_screen(&screen), m_rule(rule), m_other(other) {}
    void SetRule(RecordingRule *rule) { m_rule = rule; };
    void Create(bool *err);
    void Load(void);
    void Save(void);
    void RuleChanged(void);
    void MaxEpisodesChanged(MythUIButtonListItem *item);
    void PromptForRecGroup(void);
    void SetRecGroup(int recgroupID, QString recgroup);

    static int CreateRecordingGroup(const QString &groupName);

    MythUIButtonList *m_recprofileList   {nullptr};
    MythUIButtonList *m_recgroupList     {nullptr};
    MythUIButtonList *m_storagegroupList {nullptr};
    MythUIButtonList *m_playgroupList    {nullptr};
    MythUISpinBox    *m_maxepSpin        {nullptr};
    MythUIButtonList *m_maxbehaviourList {nullptr};
    MythUICheckBox   *m_autoexpireCheck  {nullptr};

  private:
    MythScreenType   *m_screen           {nullptr};
    RecordingRule    *m_rule             {nullptr};
    StoreOptMixin    *m_other            {nullptr};
    bool              m_loaded           {false};
};

class PostProcMixin
{
  protected:
    PostProcMixin(MythScreenType &screen, RecordingRule *rule,
                  PostProcMixin *other= nullptr)
        : m_screen(&screen), m_rule(rule), m_other(other) {}
    void SetRule(RecordingRule *rule) { m_rule = rule; };
    void Create(bool *err);
    void Load(void);
    void Save(void);
    void RuleChanged(void);
    void TranscodeChanged(bool enable);

    MythUICheckBox   *m_commflagCheck        {nullptr};
    MythUICheckBox   *m_transcodeCheck       {nullptr};
    MythUIButtonList *m_transcodeprofileList {nullptr};
    MythUICheckBox   *m_userjob1Check        {nullptr};
    MythUICheckBox   *m_userjob2Check        {nullptr};
    MythUICheckBox   *m_userjob3Check        {nullptr};
    MythUICheckBox   *m_userjob4Check        {nullptr};
    MythUICheckBox   *m_metadataLookupCheck  {nullptr};

  private:
    MythScreenType   *m_screen               {nullptr};
    RecordingRule    *m_rule                 {nullptr};
    PostProcMixin    *m_other                {nullptr};
    bool              m_loaded               {false};
};

class FilterOptMixin
{
  protected:
    FilterOptMixin(MythScreenType &screen, RecordingRule *rule,
                  FilterOptMixin *other = nullptr)
        : m_screen(&screen), m_rule(rule), m_other(other) {}
    void SetRule(RecordingRule *rule) { m_rule = rule; };
    void Create(bool *err);
    void Load(void);
    void Save(void);
    void RuleChanged(void);
    static void ToggleSelected(MythUIButtonListItem *item);

    MythUIButtonList *m_filtersList       {nullptr};
    MythUIButtonList *m_activeFiltersList {nullptr};

  private:
    MythScreenType   *m_screen            {nullptr};
    RecordingRule    *m_rule              {nullptr};
    FilterOptMixin   *m_other             {nullptr};
    bool              m_loaded            {false};

    QStringList       m_descriptions;
};

class ScheduleEditor : public ScheduleCommon,
    public SchedOptMixin, public FilterOptMixin,
    public StoreOptMixin, public PostProcMixin
{
    friend class SchedEditChild;
  Q_OBJECT
  public:
    ScheduleEditor(MythScreenStack *parent, RecordingInfo* recinfo,
                   TV *player = nullptr);
    ScheduleEditor(MythScreenStack *parent, RecordingRule* recrule,
                   TV *player = nullptr);
   ~ScheduleEditor() override;

    bool Create(void) override; // MythScreenType
    bool keyPressEvent(QKeyEvent *event) override; // MythScreenType
    void customEvent(QEvent *event) override; // ScheduleCommon

    void showMenu(void);
    void showUpcomingByRule(void);
    void showUpcomingByTitle(void);

    /// Callback
    static void *RunScheduleEditor(ProgramInfo *proginfo, void *player = nullptr);

  signals:
    void ruleSaved(int ruleId);
    void ruleDeleted(int ruleId);
    void templateLoaded(void);

  public slots:
    void ShowSchedOpt(void);
    void ShowFilters(void);
    void ShowStoreOpt(void);
    void ShowPostProc(void);
    void ShowMetadataOptions(void);
    void ShowPreviousView(void);
    void ShowNextView(void);
    void ShowPreview(void);
    void Save(void);

  protected slots:
    void RuleChanged(MythUIButtonListItem *item);
    void DupMethodChanged(MythUIButtonListItem *item);
    static void FilterChanged(MythUIButtonListItem *item);
    void MaxEpisodesChanged(MythUIButtonListItem *item);
    void PromptForRecGroup(void);
    void TranscodeChanged(bool enable);
    void ShowSchedInfo(void);
    void ChildClosing(void);
    void Close(void) override; // MythScreenType

  private:
    Q_DISABLE_COPY(ScheduleEditor);
    void Load(void) override; // MythScreenType
    void LoadTemplate(const QString& name);
    void DeleteRule(void);

    void showTemplateMenu(void);

    ProgramInfo *GetCurrentProgram(void) const override // ScheduleCommon
        { return m_recInfo; };

    RecordingInfo    *m_recInfo         {nullptr};
    RecordingRule    *m_recordingRule   {nullptr};

    bool              m_sendSig         {false};

    MythUIButton     *m_saveButton      {nullptr};
    MythUIButton     *m_cancelButton    {nullptr};

    MythUIButtonList *m_rulesList       {nullptr};

    MythUIButton     *m_schedOptButton  {nullptr};
    MythUIButton     *m_storeOptButton  {nullptr};
    MythUIButton     *m_postProcButton  {nullptr};
    MythUIButton     *m_schedInfoButton {nullptr};
    MythUIButton     *m_previewButton   {nullptr};
    MythUIButton     *m_metadataButton  {nullptr};
    MythUIButton     *m_filtersButton   {nullptr};

    TV               *m_player          {nullptr};

    bool              m_loaded          {false};

    enum View : std::uint8_t
    {
        kMainView,
        kSchedOptView,
        kFilterView,
        kStoreOptView,
        kPostProcView,
        kMetadataView
    };

    int               m_view            {kMainView};
    SchedEditChild   *m_child           {nullptr};
};

class SchedEditChild : public MythScreenType
{
  Q_OBJECT
  protected:
    SchedEditChild(MythScreenStack *parent, const QString &name,
                   ScheduleEditor &editor, RecordingRule &rule,
                   RecordingInfo *recinfo);
   ~SchedEditChild() override = default;

    bool keyPressEvent(QKeyEvent *event) override; // MythScreenType
    virtual bool CreateEditChild(
        const QString &xmlfile, const QString &winname, bool isTemplate);

  signals:
    void Closing(void);

  public slots:
    void Close(void) override; // MythScreenType
    void Load(void) override = 0; // MythScreenType
    virtual void Save(void) = 0;

  protected:
    void SetTextFromMaps(void);

    ScheduleEditor *m_editor {nullptr};
    RecordingRule  *m_recordingRule {nullptr};
    RecordingInfo  *m_recInfo {nullptr};

    MythUIButton   *m_backButton {nullptr};
    MythUIButton   *m_saveButton {nullptr};
    MythUIButton   *m_previewButton {nullptr};
};

class SchedOptEditor : public SchedEditChild, public SchedOptMixin
{
  Q_OBJECT
  public:
    SchedOptEditor(MythScreenStack *parent, ScheduleEditor &editor,
                   RecordingRule &rule, RecordingInfo *recinfo);
   ~SchedOptEditor() override = default;

    bool Create(void) override; // MythScreenType

  protected slots:
    void DupMethodChanged(MythUIButtonListItem *item);

  private:
    void Load(void) override; // SchedEditChild
    void Save(void) override; // SchedEditChild

    MythUIButton  *m_filtersButton {nullptr};
};

class SchedFilterEditor : public SchedEditChild, public FilterOptMixin
{
  Q_OBJECT
  public:
    SchedFilterEditor(MythScreenStack *parent, ScheduleEditor &editor,
                      RecordingRule &rule, RecordingInfo *recinfo);
   ~SchedFilterEditor() override = default;

    bool Create(void) override; // MythScreenType

  protected slots:
    static void ToggleSelected(MythUIButtonListItem *item);

  private:
    void Load(void) override; // SchedEditChild
    void Save(void) override; // SchedEditChild
};

class StoreOptEditor : public SchedEditChild, public StoreOptMixin
{
  Q_OBJECT
  public:
    StoreOptEditor(MythScreenStack *parent, ScheduleEditor &editor,
                   RecordingRule &rule, RecordingInfo *recinfo);
   ~StoreOptEditor() override = default;

    bool Create(void) override; // MythScreenType
    void customEvent(QEvent *event) override; // MythUIType

  protected slots:
    void MaxEpisodesChanged(MythUIButtonListItem *item);
    void PromptForRecGroup(void);

  private:
    void Load(void) override; // SchedEditChild
    void Save(void) override; // SchedEditChild
};

class PostProcEditor : public SchedEditChild, public PostProcMixin
{
  Q_OBJECT
  public:
    PostProcEditor(MythScreenStack *parent, ScheduleEditor &editor,
                   RecordingRule &rule, RecordingInfo *recinfo);
   ~PostProcEditor() override = default;

    bool Create(void) override; // MythScreenType

  protected slots:
    void TranscodeChanged(bool enable);

  private:
    void Load(void) override; // SchedEditChild
    void Save(void) override; // SchedEditChild
};

class MetadataOptions : public SchedEditChild
{
  Q_OBJECT
  public:
    MetadataOptions(MythScreenStack *parent, ScheduleEditor &editor,
                    RecordingRule &rule, RecordingInfo *recinfo);
   ~MetadataOptions() override;

    bool Create(void) override; // MythScreenType

  protected slots:
    void ClearInetref();
    void PerformQuery();
    void SelectLocalFanart();
    void SelectLocalCoverart();
    void SelectLocalBanner();
    void SelectOnlineFanart();
    void SelectOnlineCoverart();
    void SelectOnlineBanner();
    void QueryComplete(MetadataLookup *lookup);
    void OnSearchListSelection(const RefCountHandler<MetadataLookup>& lookup);
    void OnImageSearchListSelection(const ArtworkInfo& info,
                               VideoArtworkType type);
    void OnArtworkSearchDone(MetadataLookup *lookup);
    void FindNetArt(VideoArtworkType type);

    void ValuesChanged();

  private:
    void Load(void) override; // SchedEditChild
    void Save(void) override; // SchedEditChild

    void CreateBusyDialog(const QString& title);
    static void FindImagePopup(const QString &prefix,
                               const QString &prefixAlt,
                               QObject &inst,
                               const QString &returnEvent);
    static QStringList GetSupportedImageExtensionFilter();

    void HandleDownloadedImages(MetadataLookup *lookup);
    MetadataLookup *CreateLookup(MetadataType mtype);

    bool CanSetArtwork(void);

    void customEvent(QEvent *event) override; // MythUIType

    // For all metadata downloads
    MetadataFactory *m_metadataFactory {nullptr};

    // For image picking
    MetadataDownload *m_imageLookup {nullptr};
    MetadataImageDownload *m_imageDownload {nullptr};

    MythScreenStack  *m_popupStack {nullptr};
    MythUIBusyDialog *m_busyPopup {nullptr};

    MythUIImage     *m_fanart {nullptr};
    MythUIImage     *m_coverart {nullptr};
    MythUIImage     *m_banner {nullptr};

    MythUITextEdit  *m_inetrefEdit {nullptr};

    MythUISpinBox   *m_seasonSpin {nullptr};
    MythUISpinBox   *m_episodeSpin {nullptr};

    MythUIButton    *m_inetrefClear {nullptr};
    MythUIButton    *m_queryButton {nullptr};
    MythUIButton    *m_localFanartButton {nullptr};
    MythUIButton    *m_localCoverartButton {nullptr};
    MythUIButton    *m_localBannerButton {nullptr};
    MythUIButton    *m_onlineFanartButton {nullptr};
    MythUIButton    *m_onlineCoverartButton {nullptr};
    MythUIButton    *m_onlineBannerButton {nullptr};

    ArtworkMap       m_artworkMap;
};

#endif
