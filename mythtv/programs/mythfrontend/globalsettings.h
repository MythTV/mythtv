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
    ButtonStandardSetting *m_newPlaybackProfileButton;
    MythUIComboBoxSetting *m_playbackProfiles;
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

class AppearanceSettings : public GroupSetting
{
    Q_DECLARE_TR_FUNCTIONS(AppearanceSettings);

  public:
    AppearanceSettings();
    void applyChange() override; // GroupSetting
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
    static const vector<double> GetRefreshRates(const QString &resolution);
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
    HostCheckBoxSetting *m_CECPowerOnTVAllowed;
    HostCheckBoxSetting *m_CECPowerOffTVAllowed;
    HostCheckBoxSetting *m_CECPowerOnTVOnStart;
    HostCheckBoxSetting *m_CECPowerOffTVOnExit;
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
    void widthChanged(const QString &dec);
    void heightChanged(const QString &dec);
    void framerateChanged(const QString &dec);
    void decoderChanged(const QString &dec);
    void vrenderChanged(const QString &renderer);
    void orenderChanged(const QString &renderer);
    void deint0Changed(const QString &deint);
    void deint1Changed(const QString &deint);
    void InitLabel(void);
    void DoDeleteSlot(bool);

  private:
    ProfileItem          &item;
    TransTextEditSetting      *width_range;
    TransTextEditSetting      *height_range;
    MythUIComboBoxSetting      *codecs;
    TransTextEditSetting      *framerate;
    TransMythUIComboBoxSetting *decoder;
    TransMythUISpinBoxSetting  *max_cpus;
    TransMythUICheckBoxSetting *skiploop;
    TransMythUIComboBoxSetting *vidrend;
    TransMythUIComboBoxSetting *osdrend;
    TransMythUICheckBoxSetting *osdfade;
    TransMythUIComboBoxSetting *deint0;
    TransMythUIComboBoxSetting *deint1;
    TransTextEditSetting *filters;
    PlaybackProfileConfig *parentConfig;
    uint index;
};

class PlaybackProfileConfig : public GroupSetting
{
    Q_OBJECT

  public:
    PlaybackProfileConfig(const QString &profilename, StandardSetting *parent);
    virtual ~PlaybackProfileConfig() = default;

    void Save(void) override; // StandardSetting

    void DeleteProfileItem(PlaybackProfileItemConfig *profile);

    void swap(int indexA, int intexB);

  private slots:
    void AddNewEntry(void);

  private:
    void InitUI(StandardSetting *parent);
    StandardSetting * InitProfileItem(uint, StandardSetting *);

  private:
    void ReloadSettings(void);
    item_list_t items;
    item_list_t del_items;
    QString     profile_name;
    uint        groupid;

    TransMythUICheckBoxSetting *m_markForDeletion;
    ButtonStandardSetting* m_addNewEntry;
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
    int m_groupId;
    TransTextEditSetting       *m_groupName;
};

class ChannelGroupsSetting : public GroupSetting
{
    Q_OBJECT

  public:
    ChannelGroupsSetting();
    void Load() override; // StandardSetting

  public slots:
    void ShowNewGroupDialog(void);
    void CreateNewGroup(QString name);

  private:
    ButtonStandardSetting *m_addGroupButton;
};

#endif
