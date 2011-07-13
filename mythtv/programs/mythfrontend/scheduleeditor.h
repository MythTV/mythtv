#ifndef SCHEDULERECORDING_H_
#define SCHEDULERECORDING_H_

#include "mythscreentype.h"
#include "schedulecommon.h"

// libmythbase
#include "mythdb.h"

// libmyth
#include "mythcontext.h"

// libmythtv
#include "recordingrule.h"
#include "recordinginfo.h"

// libmythmetadata
#include "metadatafactory.h"

#ifndef ALLOW_MISSING_FILTERS
#define ALLOW_MISSING_FILTERS 1
#endif

class ProgramInfo;
class MythUIText;
class MythUIButton;
class MythUIButtonList;
class MythUIButtonListItem;
class MythUIStateType;
class MythUISpinBox;
class TV;

class ScheduleEditor : public ScheduleCommon
{
  Q_OBJECT
  public:
    ScheduleEditor(MythScreenStack *parent, RecordingInfo* recinfo,
                   TV *player = NULL);
    ScheduleEditor(MythScreenStack *parent, RecordingRule* recrule,
                   TV *player = NULL);
   ~ScheduleEditor();

    bool Create(void);
    void customEvent(QEvent *event);

    /// Callback
    static void *RunScheduleEditor(ProgramInfo *proginfo, void *player = NULL);

  signals:
    void ruleSaved(int ruleId);

  protected slots:
    void RuleChanged(MythUIButtonListItem *item);
    void ShowSchedOpt(void);
    void ShowStoreOpt(void);
    void ShowPostProc(void);
    void ShowSchedInfo(void);
    void ShowPreview(void);
    void ShowMetadataOptions(void);
    void Save(void);
    void Close(void);

  private:
    void Load(void);
    void DeleteRule(void);

    void showPrevious(void);
    void showUpcomingByRule(void);
    void showUpcomingByTitle(void);

    RecordingInfo *m_recInfo;
    RecordingRule *m_recordingRule;

    bool m_sendSig;

    MythUIButton    *m_saveButton;
    MythUIButton    *m_cancelButton;

    MythUIButtonList *m_rulesList;

    MythUIButton    *m_schedOptButton;
    MythUIButton    *m_storeOptButton;
    MythUIButton    *m_postProcButton;
    MythUIButton    *m_schedInfoButton;
    MythUIButton    *m_previewButton;
    MythUIButton    *m_metadataButton;

    TV *m_player;
};

class SchedOptEditor : public MythScreenType
{
  Q_OBJECT
  public:
    SchedOptEditor(MythScreenStack *parent, RecordingInfo *recinfo,
                   RecordingRule *rule);
   ~SchedOptEditor();

    bool Create(void);

  protected slots:
    void dupMatchChanged(MythUIButtonListItem *item);
    void ShowFilters(void);
    void Close(void);

  private:
    void Load(void);
    void Save(void);

    RecordingInfo *m_recInfo;
    RecordingRule *m_recordingRule;

    MythUIButton    *m_backButton;

    MythUISpinBox *m_prioritySpin;
    MythUIButtonList *m_inputList;
    MythUISpinBox *m_startoffsetSpin;
    MythUISpinBox *m_endoffsetSpin;
    MythUIButtonList *m_dupmethodList;
    MythUIButtonList *m_dupscopeList;
    MythUIButton  *m_filtersButton;

    MythUICheckBox *m_ruleactiveCheck;

#if (ALLOW_MISSING_FILTERS)
    bool m_missing_filters;
#endif
};

class SchedFilterEditor : public MythScreenType
{
  Q_OBJECT
  public:
    SchedFilterEditor(MythScreenStack *parent, RecordingInfo *recinfo,
		      RecordingRule *rule);
   ~SchedFilterEditor();

    bool Create(void);

  protected slots:
    void Close(void);
    void ToggleSelected(MythUIButtonListItem *item);

  private:
    void Load(void);
    void Save(void);

    RecordingInfo *m_recInfo;
    RecordingRule *m_recordingRule;

    MythUIButton    *m_backButton;
    MythUIButtonList *m_filtersList;
};

class StoreOptEditor : public MythScreenType
{
  Q_OBJECT
  public:
    StoreOptEditor(MythScreenStack *parent, RecordingInfo *recinfo,
                   RecordingRule *rule);
   ~StoreOptEditor();

    bool Create(void);
    void customEvent(QEvent *event);

  protected slots:
    void maxEpChanged(MythUIButtonListItem *item);
    void PromptForRecgroup(void);
    void Close(void);

  private:
    void Load(void);
    void Save(void);

    RecordingInfo *m_recInfo;
    RecordingRule *m_recordingRule;

    MythUIButton    *m_backButton;

    MythUIButtonList *m_recprofileList;
    MythUIButtonList *m_recgroupList;
    MythUIButtonList *m_storagegroupList;
    MythUIButtonList *m_playgroupList;
    MythUICheckBox *m_autoexpireCheck;
    MythUISpinBox *m_maxepSpin;
    MythUIButtonList *m_maxbehaviourList;
};

class PostProcEditor : public MythScreenType
{
  Q_OBJECT
  public:
    PostProcEditor(MythScreenStack *parent, RecordingInfo *recinfo,
                   RecordingRule *rule);
   ~PostProcEditor();

    bool Create(void);

  protected slots:
    void transcodeEnable(bool enable);
    void Close(void);

  private:
    void Load(void);
    void Save(void);

    RecordingInfo *m_recInfo;
    RecordingRule *m_recordingRule;

    MythUIButton    *m_backButton;

    MythUICheckBox *m_commflagCheck;
    MythUICheckBox *m_transcodeCheck;
    MythUIButtonList *m_transcodeprofileList;
    MythUICheckBox *m_userjob1Check;
    MythUICheckBox *m_userjob2Check;
    MythUICheckBox *m_userjob3Check;
    MythUICheckBox *m_userjob4Check;
    MythUICheckBox *m_metadataLookupCheck;
};

class MetadataOptions : public MythScreenType
{
  Q_OBJECT
  public:
    MetadataOptions(MythScreenStack *parent, RecordingInfo *recinfo,
                   RecordingRule *rule);
   ~MetadataOptions();

    bool Create(void);

  protected slots:
    void PerformQuery();
    void SelectLocalFanart();
    void SelectLocalCoverart();
    void SelectLocalBanner();
    void SelectOnlineFanart();
    void SelectOnlineCoverart();
    void SelectOnlineBanner();
    void QueryComplete(MetadataLookup *lookup);
    void OnSearchListSelection(MetadataLookup *lookup);
    void OnImageSearchListSelection(ArtworkInfo info,
                               VideoArtworkType type);
    void OnArtworkSearchDone(MetadataLookup *lookup);
    void FindNetArt(VideoArtworkType type);

    void ValuesChanged();

    void Close(void);

  private:
    void Load(void);
    void Save(void);

    void CreateBusyDialog(QString title);
    void FindImagePopup(const QString &prefix,
                        const QString &prefixAlt,
                        QObject &inst,
                        const QString &returnEvent);
    QStringList GetSupportedImageExtensionFilter();

    void HandleDownloadedImages(MetadataLookup *lookup);

    void customEvent(QEvent *event);

    RecordingInfo   *m_recInfo;
    RecordingRule   *m_recordingRule;

    // For all metadata downloads
    MetadataFactory *m_metadataFactory;

    // For image picking
    MetadataDownload *m_imageLookup;
    MetadataImageDownload *m_imageDownload;

    MetadataLookup  *m_lookup;

    MythScreenStack  *m_popupStack;
    MythUIBusyDialog *m_busyPopup;

    MythUIImage     *m_fanart;
    MythUIImage     *m_coverart;
    MythUIImage     *m_banner;

    MythUITextEdit  *m_inetrefEdit;

    MythUISpinBox   *m_seasonSpin;
    MythUISpinBox   *m_episodeSpin;

    MythUIButton    *m_queryButton;
    MythUIButton    *m_localFanartButton;
    MythUIButton    *m_localCoverartButton;
    MythUIButton    *m_localBannerButton;
    MythUIButton    *m_onlineFanartButton;
    MythUIButton    *m_onlineCoverartButton;
    MythUIButton    *m_onlineBannerButton;

    MythUIButton    *m_backButton;

    ArtworkMap       m_artworkMap;
};

#endif
