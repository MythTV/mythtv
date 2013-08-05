#ifndef MYTHSETTINGS_H
#define MYTHSETTINGS_H

#include <QStringList>
#include <QObject>
#include <QCoreApplication>

#include "mythconfig.h"
#include "settings.h"
#include "mythcontext.h"
#include "videodisplayprofile.h"

#include <QMutex>

class QFileInfo;

class PlaybackSettings : public ConfigurationWizard
{
    Q_DECLARE_TR_FUNCTIONS(PlaybackSettings)

  public:
    PlaybackSettings();
};

class VideoModeSettings : public TriggeredConfigurationGroup
{
    Q_OBJECT

  public:
    VideoModeSettings();
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

class OSDSettings : virtual public ConfigurationWizard
{
    Q_DECLARE_TR_FUNCTIONS(OSDSettings)
        
  public:
    OSDSettings();
};

class GeneralSettings : public ConfigurationWizard
{
    Q_DECLARE_TR_FUNCTIONS(GeneralSettings)
        
  public:
    GeneralSettings();
};

class EPGSettings : public ConfigurationWizard
{
    Q_DECLARE_TR_FUNCTIONS(EPGSettings)

  public:
    EPGSettings();
};

class AppearanceSettings : public ConfigurationWizard
{
    Q_DECLARE_TR_FUNCTIONS(AppearanceSettings)

  public:
    AppearanceSettings();
};

class MainGeneralSettings : public ConfigurationWizard
{
    Q_DECLARE_TR_FUNCTIONS(MainGeneralSettings)

  public:
    MainGeneralSettings();
};

class GeneralRecPrioritiesSettings : public ConfigurationWizard
{
    Q_DECLARE_TR_FUNCTIONS(GeneralRecPrioritiesSettings)

  public:
    GeneralRecPrioritiesSettings();
};

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
