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

class PlaybackSettings : public GroupSetting
{
    Q_DECLARE_TR_FUNCTIONS(PlaybackSettings)

  public:
    PlaybackSettings();
};

class VideoModeSettings : public HostCheckBoxSetting
{
    Q_OBJECT

  public:
    VideoModeSettings(const char *c);
    virtual void updateButton(MythUIButtonListItem *item);
};

class LcdSettings : public TriggeredConfigurationGroup
{
    Q_OBJECT

  public:
    LcdSettings();
};


class WatchListSettings : public TriggeredConfigurationGroup
{
    Q_OBJECT

  public:
    WatchListSettings();
};

class ChannelGroupSettings : public TriggeredConfigurationGroup
{
    Q_OBJECT

  public:
    ChannelGroupSettings();
};

#if CONFIG_DARWIN
class MacMainSettings : public TriggeredConfigurationGroup
{
    Q_OBJECT

  public:
    MacMainSettings();
};

class MacFloatSettings : public TriggeredConfigurationGroup
{
    Q_OBJECT

  public:
    MacFloatSettings();
};


class MacDockSettings : public TriggeredConfigurationGroup
{
    Q_OBJECT

  public:
    MacDockSettings();
};


class MacDesktopSettings : public TriggeredConfigurationGroup
{
    Q_OBJECT

  public:
    MacDesktopSettings();
};
#endif

class OSDSettings: public GroupSetting
{
    Q_DECLARE_TR_FUNCTIONS(OSDSettings)

  public:
    OSDSettings();
};

class GeneralSettings : public GroupSetting
{
    Q_DECLARE_TR_FUNCTIONS(GeneralSettings)

  public:
    GeneralSettings();
};

class EPGSettings : public GroupSetting
{
    Q_DECLARE_TR_FUNCTIONS(EPGSettings)

  public:
    EPGSettings();
};

class AppearanceSettings : public GroupSetting
{
    Q_DECLARE_TR_FUNCTIONS(AppearanceSettings)

  public:
    AppearanceSettings();
    virtual void applyChange();
};

class HostRefreshRateComboBoxSetting : public HostComboBoxSetting
{
    Q_OBJECT

  public:
    HostRefreshRateComboBoxSetting(const QString &name) :
        HostComboBoxSetting(name) { }
    virtual ~HostRefreshRateComboBoxSetting() { }

  public slots:
    virtual void ChangeResolution(StandardSetting *);

  private:
    static const vector<double> GetRefreshRates(const QString &resolution);
};

class MainGeneralSettings : public GroupSetting
{
    Q_DECLARE_TR_FUNCTIONS(MainGeneralSettings)

  public:
    MainGeneralSettings();
    virtual void applyChange();
};

class GeneralRecPrioritiesSettings : public GroupSetting
{
    Q_DECLARE_TR_FUNCTIONS(GeneralRecPrioritiesSettings)

  public:
    GeneralRecPrioritiesSettings();
};

#if 0
class PlaybackProfileItemConfig : public QObject, public ConfigurationWizard
{
    Q_OBJECT

  public:
    PlaybackProfileItemConfig(ProfileItem &_item);

    virtual void Load(void);
    virtual void Save(void);
    virtual void Save(QString /*destination*/) { Save(); }

  private slots:
    void decoderChanged(const QString &dec);
    void vrenderChanged(const QString &renderer);
    void orenderChanged(const QString &renderer);
    void deint0Changed(const QString &deint);
    void deint1Changed(const QString &deint);

  private:
    ProfileItem          &item;
    TransComboBoxSetting *cmp[2];
    TransSpinBoxSetting  *width[2];
    TransSpinBoxSetting  *height[2];
    TransComboBoxSetting *decoder;
    TransSpinBoxSetting  *max_cpus;
    TransCheckBoxSetting *skiploop;
    TransComboBoxSetting *vidrend;
    TransComboBoxSetting *osdrend;
    TransCheckBoxSetting *osdfade;
    TransComboBoxSetting *deint0;
    TransComboBoxSetting *deint1;
    TransLineEditSetting *filters;
};

class PlaybackProfileConfig : public VerticalConfigurationGroup
{
    Q_OBJECT

  public:
    PlaybackProfileConfig(const QString &profilename);
    virtual ~PlaybackProfileConfig();

    virtual void Load(void);
    virtual void Save(void);
    virtual void Save(QString /*destination*/) { Save(); }

    void swap(int indexA, int intexB);

  private slots:
    void pressed(QString);
    void priorityChanged(const QString &name, int);

  private:
    void InitLabel(uint);
    void InitUI(void);

  private:
    item_list_t items;
    item_list_t del_items;
    QString     profile_name;
    bool        needs_save;
    uint        groupid;

    VerticalConfigurationGroup  *last_main;
    vector<TransLabelSetting*>   labels;
    vector<TransButtonSetting*>  editProf;
    vector<TransButtonSetting*>  delProf;
    vector<TransSpinBoxSetting*> priority;
};

class PlaybackProfileConfigs : public TriggeredConfigurationGroup
{
    Q_OBJECT

  public:
    PlaybackProfileConfigs(const QString &str);
    virtual ~PlaybackProfileConfigs();

  private:
    void InitUI(void);

  private slots:
    void btnPress(QString);
    void triggerChanged(const QString&);

  private:
    QStringList   profiles;
    HostComboBox *grouptrigger;
};
#endif

class PlayBackGroupsSetting : public GroupSetting
{
    Q_OBJECT

  public:
    PlayBackGroupsSetting();
    virtual void Load();
    virtual void Open();

  public slots:
    //void newPlayBackGroup(StandardSetting *);
    //void editPlayBackGroup(StandardSetting *);
    //void RefreshList();

  private:
    ButtonStandardSetting *m_addGroupButton;
};

class ChannelGroupSetting : public GroupSetting
{
  public:
    ChannelGroupSetting(const QString &groupName, int groupId);
    virtual void Load();
    virtual void Open();
    virtual void Close();

  private:
    int m_groupId;
    TransTextEditSetting       *m_groupName;
    TransMythUICheckBoxSetting *m_markForDeletion;

};

class ChannelGroupsSetting : public GroupSetting
{
  public:
    ChannelGroupsSetting();
    virtual void Load();
    virtual void Open();

  private:
    ButtonStandardSetting *m_addGroupButton;
};

#endif
