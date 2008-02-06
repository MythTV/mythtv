#ifndef MYTHSETTINGS_H
#define MYTHSETTINGS_H

#include "libmyth/settings.h"
#include "libmyth/mythcontext.h"
#include "libmythtv/videodisplayprofile.h"
#include "themeinfo.h"

class ThemeSelector : public HostImageSelect
{
  public:
    ThemeSelector(QString label);

  private:
    bool parseThemeInfo(QFileInfo *theme, QString &name, QFileInfo &preview);
};

class PlaybackSettings : public ConfigurationWizard
{
  public:
    PlaybackSettings();
};

class OSDSettings: virtual public ConfigurationWizard
{
  public:
    OSDSettings();
};

class GeneralSettings : public ConfigurationWizard
{
  public:
    GeneralSettings();
};

class EPGSettings : public ConfigurationWizard
{
  public:
    EPGSettings();
};

class AppearanceSettings : public ConfigurationWizard
{
  public:
    AppearanceSettings();
};

class MainGeneralSettings : public ConfigurationWizard
{
  public:
    MainGeneralSettings();
};

class GeneralRecPrioritiesSettings : public ConfigurationWizard
{
  public:
    GeneralRecPrioritiesSettings();
};

class XboxSettings : public ConfigurationWizard
{
  public:
    XboxSettings();
};

class PlaybackProfileItemConfig : public QObject, public ConfigurationWizard
{
    Q_OBJECT

  public:
    PlaybackProfileItemConfig(ProfileItem &_item);

    virtual void load(void);
    virtual void save(void);

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

    virtual void load(void);
    virtual void save(void);

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

#ifdef USING_IVTV
class PVR350VideoDevice : public PathSetting, public HostDBStorage
{
  public:
    PVR350VideoDevice();

    uint fillSelectionsFromDir(const QDir &dir,
                               uint minor_min, uint minor_max,
                               QString card, QString driver,
                               bool allow_duplicates);
  private:
    QMap<uint, uint> minor_list;
};
#endif // USING_IVTV

#endif
