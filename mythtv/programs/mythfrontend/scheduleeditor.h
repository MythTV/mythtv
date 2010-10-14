#ifndef SCHEDULERECORDING_H_
#define SCHEDULERECORDING_H_

#include "mythscreentype.h"
#include "schedulecommon.h"

// LibmythDB
#include "mythdb.h"

// Libmyth
#include "mythcontext.h"

// Libmythtv
#include "recordingrule.h"
#include "recordinginfo.h"

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

    MythUICheckBox *m_ruleactiveCheck;
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
};

#endif
