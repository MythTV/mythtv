#ifndef MYTHSETTINGS_H
#define MYTHSETTINGS_H

#include <QStringList>
#include <QObject>
#include <QCoreApplication>

#include "mythconfig.h"
#include "standardsettings.h"
#include "mythcontext.h"
#include "videodisplayprofile.h"

#include <QMutex>

class QFileInfo;

class PlaybackSettingsDialog : public StandardSettingDialog
{
    Q_OBJECT

  public:
    explicit PlaybackSettingsDialog(MythScreenStack *stack);
    void ShowMenu(void) override; // StandardSettingDialog

  protected slots:
    void ShowPlaybackProfileMenu(MythUIButtonListItem *item);
    void DeleteProfileItem(void);
    void MoveProfileItemDown(void);
    void MoveProfileItemUp(void);
};

class PlaybackSettings : public GroupSetting
{
    Q_OBJECT

  public:
    PlaybackSettings();
    void Load(void) override; // StandardSetting

  private slots:
    void NewPlaybackProfileSlot(void);
    void CreateNewPlaybackProfileSlot(const QString &name);

  private:
    ButtonStandardSetting *m_newPlaybackProfileButton {nullptr};
    MythUIComboBoxSetting *m_playbackProfiles {nullptr};
};

class VideoModeSettings : public HostCheckBoxSetting
{
    Q_OBJECT

  public:
    explicit VideoModeSettings(const char *c);
#if defined(USING_XRANDR) || CONFIG_DARWIN
    void updateButton(MythUIButtonListItem *item) override; // MythUICheckBoxSetting
#endif
};

class LcdSettings
{
    Q_DECLARE_TR_FUNCTIONS(LcdSettings)
};


class WatchListSettings
{
    Q_DECLARE_TR_FUNCTIONS(WatchListSettings)
};

class ChannelGroupSettings
{
    Q_DECLARE_TR_FUNCTIONS(ChannelGroupSettings)
};

#if CONFIG_DARWIN
class MacMainSettings : public GroupSetting
{
    Q_OBJECT

  public:
    MacMainSettings();
};

class MacFloatSettings : public GroupSetting
{
    Q_OBJECT

  public:
    MacFloatSettings();
};


class MacDockSettings : public GroupSetting
{
    Q_OBJECT

  public:
    MacDockSettings();
};


class MacDesktopSettings : public GroupSetting
{
    Q_OBJECT

  public:
    MacDesktopSettings();
};
#endif

class OSDSettings: public GroupSetting
{
    Q_DECLARE_TR_FUNCTIONS(OSDSettings);

  public:
    OSDSettings();
};

class GeneralSettings : public GroupSetting
{
    Q_DECLARE_TR_FUNCTIONS(GeneralSettings);

  public:
    GeneralSettings();
};

class EPGSettings : public GroupSetting
{
    Q_DECLARE_TR_FUNCTIONS(EPGSettings);

  public:
    EPGSettings();
};

class MythDisplay;
class AppearanceSettings : public GroupSetting
{
    Q_OBJECT

  public:
    AppearanceSettings();
   ~AppearanceSettings() override;
    void applyChange() override; // GroupSetting

  public slots:
    void PopulateScreens(int Screens);

  private:
    HostComboBoxSetting *m_xineramaScreen { nullptr };
    HostComboBoxSetting *m_xineramaAspect { nullptr };
    MythDisplay         *m_display        { nullptr };
};

class HostRefreshRateComboBoxSetting : public HostComboBoxSetting
{
    Q_OBJECT

  public:
    explicit HostRefreshRateComboBoxSetting(const QString &name) :
        HostComboBoxSetting(name) { }
    virtual ~HostRefreshRateComboBoxSetting() = default;

  public slots:
#if defined(USING_XRANDR) || CONFIG_DARWIN
    virtual void ChangeResolution(StandardSetting *);
#endif

  private:
    static vector<double> GetRefreshRates(const QString &resolution);
};

class MainGeneralSettings : public GroupSetting
{
    Q_OBJECT

  public:
    MainGeneralSettings();
    void applyChange() override; // GroupSetting

#ifdef USING_LIBCEC
  public slots:
    void cecChanged(bool);
  protected:
    HostCheckBoxSetting *m_CECPowerOnTVAllowed  {nullptr};
    HostCheckBoxSetting *m_CECPowerOffTVAllowed {nullptr};
    HostCheckBoxSetting *m_CECPowerOnTVOnStart  {nullptr};
    HostCheckBoxSetting *m_CECPowerOffTVOnExit  {nullptr};
#endif  // USING_LIBCEC
};

class GeneralRecPrioritiesSettings : public GroupSetting
{
    Q_DECLARE_TR_FUNCTIONS(GeneralRecPrioritiesSettings);

  public:
    GeneralRecPrioritiesSettings();
};

class PlaybackProfileConfig;
class PlaybackProfileItemConfig : public GroupSetting
{
    Q_OBJECT

  public:
    PlaybackProfileItemConfig(PlaybackProfileConfig *parent, uint idx,
                              ProfileItem &_item);

    void Load(void) override; // StandardSetting
    void Save(void) override; // StandardSetting

    bool keyPressEvent(QKeyEvent *) override; // StandardSetting
    uint GetIndex(void) const;
    void ShowDeleteDialog(void);
    void DecreasePriority(void);
    void IncreasePriority(void);

  private slots:
    void widthChanged(const QString &val);
    void heightChanged(const QString &val);
    void framerateChanged(const QString &val);
    void decoderChanged(const QString &dec);
    void vrenderChanged(const QString &renderer);
    void SingleQualityChanged(const QString &Quality);
    void DoubleQualityChanged(const QString &Quality);
    static void LoadQuality(TransMythUIComboBoxSetting *Deint,
                            TransMythUICheckBoxSetting *Shader,
                            TransMythUICheckBoxSetting *Driver,
                            QString &Value);
    static QString GetQuality(TransMythUIComboBoxSetting *Deint,
                              TransMythUICheckBoxSetting *Shader,
                              TransMythUICheckBoxSetting *Driver);
    void InitLabel(void);
    void DoDeleteSlot(bool);

  private:
    ProfileItem                &m_item;
    TransTextEditSetting       *m_width_range  {nullptr};
    TransTextEditSetting       *m_height_range {nullptr};
    MythUIComboBoxSetting      *m_codecs       {nullptr};
    TransTextEditSetting       *m_framerate    {nullptr};
    TransMythUIComboBoxSetting *m_decoder      {nullptr};
    TransMythUISpinBoxSetting  *m_max_cpus     {nullptr};
    TransMythUICheckBoxSetting *m_skiploop     {nullptr};
    TransMythUIComboBoxSetting *m_vidrend      {nullptr};
    TransMythUIComboBoxSetting *m_singleDeint  {nullptr};
    TransMythUICheckBoxSetting *m_singleShader {nullptr};
    TransMythUICheckBoxSetting *m_singleDriver {nullptr};
    TransMythUIComboBoxSetting *m_doubleDeint  {nullptr};
    TransMythUICheckBoxSetting *m_doubleShader {nullptr};
    TransMythUICheckBoxSetting *m_doubleDriver {nullptr};
    PlaybackProfileConfig      *m_parentConfig {nullptr};
    uint                        m_index        {0};
};

class PlaybackProfileConfig : public GroupSetting
{
    Q_OBJECT

  public:
    PlaybackProfileConfig(QString profilename, StandardSetting *parent);
    virtual ~PlaybackProfileConfig() = default;

    void Save(void) override; // StandardSetting

    void DeleteProfileItem(PlaybackProfileItemConfig *profile);

    void swap(int indexA, int indexB);

  private slots:
    void AddNewEntry(void);

  private:
    void InitUI(StandardSetting *parent);
    StandardSetting * InitProfileItem(uint, StandardSetting *);

  private:
    void ReloadSettings(void);
    vector<ProfileItem> m_items;
    vector<ProfileItem> m_del_items;
    QString     m_profile_name;
    uint        m_groupid {0};

    TransMythUICheckBoxSetting *m_markForDeletion {nullptr};
    ButtonStandardSetting      *m_addNewEntry     {nullptr};
    vector<PlaybackProfileItemConfig*> m_profiles;
    vector<TransMythUISpinBoxSetting*> priority;
};

class ChannelGroupSetting : public GroupSetting
{
  public:
    ChannelGroupSetting(const QString &groupName, int groupId);
    void Load() override; // StandardSetting
    void Save() override; // StandardSetting
    bool canDelete(void) override; // GroupSetting
    void deleteEntry(void) override; // GroupSetting

  private:
    int                   m_groupId   {-1};
    TransTextEditSetting *m_groupName {nullptr};
};

class ChannelGroupsSetting : public GroupSetting
{
    Q_OBJECT

  public:
    ChannelGroupsSetting();
    void Load() override; // StandardSetting

  public slots:
    void ShowNewGroupDialog(void);
    void CreateNewGroup(const QString& name);

  private:
    ButtonStandardSetting *m_addGroupButton {nullptr};
};

#endif
