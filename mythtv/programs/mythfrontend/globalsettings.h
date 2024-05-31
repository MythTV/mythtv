#ifndef MYTHSETTINGS_H
#define MYTHSETTINGS_H

// Qt
#include <QtGlobal>
#include <QCoreApplication>
#include <QMutex>
#include <QObject>
#include <QStringList>

// MythTV
#include "libmyth/mythcontext.h"
#include "libmythui/standardsettings.h"
#include "libmythtv/mythvideoprofile.h"

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
    void NewPlaybackProfileSlot(void) const;
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
    void updateButton(MythUIButtonListItem *item) override;
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

#ifdef Q_OS_DARWIN
class MacMainSettings : public GroupSetting
{
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
    Q_OBJECT
#else
    Q_DECLARE_TR_FUNCTIONS(MacMainSettings);
#endif
  public:
    MacMainSettings();
};

class MacFloatSettings : public GroupSetting
{
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
    Q_OBJECT
#else
    Q_DECLARE_TR_FUNCTIONS(MacFloatSettings);
#endif

  public:
    MacFloatSettings();
};


class MacDockSettings : public GroupSetting
{
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
    Q_OBJECT
#else
    Q_DECLARE_TR_FUNCTIONS(MacDockSettings);
#endif

  public:
    MacDockSettings();
};


class MacDesktopSettings : public GroupSetting
{
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
    Q_OBJECT
#else
    Q_DECLARE_TR_FUNCTIONS(MacDesktopSettings);
#endif

  public:
    MacDesktopSettings();
};
#endif // Q_OS_DARWIN

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
   ~AppearanceSettings() override = default;
    void applyChange() override; // GroupSetting

  public slots:
    void PopulateScreens(int Screens);

  private:
    HostComboBoxSetting *m_screen       { nullptr };
    HostComboBoxSetting *m_screenAspect { nullptr };
    MythDisplay         *m_display      { nullptr };
};

class HostRefreshRateComboBoxSetting : public HostComboBoxSetting
{
    Q_OBJECT

  public:
    explicit HostRefreshRateComboBoxSetting(const QString &name) :
        HostComboBoxSetting(name) { }
    ~HostRefreshRateComboBoxSetting() override = default;

  public slots:
    virtual void ChangeResolution(StandardSetting *setting);

  private:
    static std::vector<double> GetRefreshRates(const QString &resolution);
};

class MainGeneralSettings : public GroupSetting
{
    Q_OBJECT

  public:
    MainGeneralSettings();
    void applyChange() override; // GroupSetting

#ifdef USING_LIBCEC
  public slots:
    void cecChanged(bool setting);
  protected:
    HostCheckBoxSetting *m_cecPowerOnTVAllowed  {nullptr};
    HostCheckBoxSetting *m_cecPowerOffTVAllowed {nullptr};
    HostCheckBoxSetting *m_cecPowerOnTVOnStart  {nullptr};
    HostCheckBoxSetting *m_cecPowerOffTVOnExit  {nullptr};
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
                              MythVideoProfileItem &_item);

    void Load(void) override; // StandardSetting
    void Save(void) override; // StandardSetting

    bool keyPressEvent(QKeyEvent *e) override; // StandardSetting
    uint GetIndex(void) const;
    void ShowDeleteDialog(void) const;
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
    void DoDeleteSlot(bool doDelete);

  private:
    MythVideoProfileItem       &m_item;
    TransTextEditSetting       *m_widthRange   {nullptr};
    TransTextEditSetting       *m_heightRange  {nullptr};
    MythUIComboBoxSetting      *m_codecs       {nullptr};
    TransTextEditSetting       *m_framerate    {nullptr};
    TransMythUIComboBoxSetting *m_decoder      {nullptr};
    TransMythUISpinBoxSetting  *m_maxCpus      {nullptr};
    TransMythUICheckBoxSetting *m_skipLoop     {nullptr};
    TransMythUIComboBoxSetting *m_vidRend      {nullptr};
    TransMythUIComboBoxSetting *m_upscaler     {nullptr};
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
    ~PlaybackProfileConfig() override = default;

    void Save(void) override; // StandardSetting

    void DeleteProfileItem(PlaybackProfileItemConfig *profile);

    // This function doesn't guarantee that no exceptions will be thrown.
    void swap(int indexA, int indexB); // NOLINT(performance-noexcept-swap)

  private slots:
    void AddNewEntry(void);

  private:
    void InitUI(StandardSetting *parent);
    StandardSetting * InitProfileItem(uint i, StandardSetting *parent);

  private:
    void ReloadSettings(void);
    std::vector<MythVideoProfileItem> m_items;
    std::vector<MythVideoProfileItem> m_delItems;
    QString     m_profileName;
    uint        m_groupId {0};

    TransMythUICheckBoxSetting *m_markForDeletion {nullptr};
    ButtonStandardSetting      *m_addNewEntry     {nullptr};
    std::vector<PlaybackProfileItemConfig*> m_profiles;
    std::vector<TransMythUISpinBoxSetting*> m_priority;
};

class ChannelGroupSetting : public GroupSetting
{
    Q_OBJECT

  public:
    ChannelGroupSetting(const QString &groupName, int groupId);
    void Load() override; // StandardSetting
    void Save() override; // StandardSetting
    bool canDelete(void) override; // GroupSetting
    void deleteEntry(void) override; // GroupSetting

 public slots:
    void LoadChannelGroup(void);
    void LoadChannelGroupChannels(void);

  private:
    int                   m_groupId   {-1};
    TransTextEditSetting *m_groupName {nullptr};
    HostComboBoxSetting  *m_groupSelection {nullptr};
    std::map<std::pair<int,uint>, TransMythUICheckBoxSetting *> m_boxMap;
};

class ChannelGroupsSetting : public GroupSetting
{
    Q_OBJECT

  public:
    ChannelGroupsSetting();
    void Load() override; // StandardSetting

  public slots:
    void ShowNewGroupDialog(void) const;
    void CreateNewGroup(const QString& name);

  private:
    ButtonStandardSetting *m_addGroupButton {nullptr};
};

#endif
