#ifndef VIDEOSOURCE_H
#define VIDEOSOURCE_H

#include <vector>
using namespace std;

#include "settings.h"
#include "datadirect.h"

class SignalTimeout;
class ChannelTimeout;
class UseEIT;

class VideoSource;
class VSSetting: public SimpleDBStorage {
protected:
    VSSetting(const VideoSource& _parent, QString name):
        SimpleDBStorage("videosource", name),
        parent(_parent) {
        setName(name);
    };

    virtual QString setClause(void);
    virtual QString whereClause(void);

    const VideoSource& parent;
};

class FreqTableSelector: public ComboBoxSetting, public VSSetting {
    Q_OBJECT
public:
    FreqTableSelector(const VideoSource& parent);
protected:
    QString freq;
};

class DataDirectLineupSelector: public ComboBoxSetting, public VSSetting {
   Q_OBJECT
public:
   DataDirectLineupSelector(const VideoSource& parent): 
       VSSetting(parent, "lineupid") {
       setLabel(QObject::tr("Data Direct Lineup"));
   };

 public slots:
    void fillSelections(const QString& uid, const QString& pwd, int source);
};

class DataDirectButton: public ButtonSetting, public TransientStorage {
   Q_OBJECT
public:
   DataDirectButton() {
       setLabel(QObject::tr("Retrieve Lineups"));
   };
};

class DataDirectUserID;
class DataDirectPassword;

class DataDirect_config: public VerticalConfigurationGroup
{
    Q_OBJECT
  public:
    DataDirect_config(const VideoSource& _parent, int _source = DD_ZAP2IT); 

    virtual void load(void);

    QString getLineupID(void) const { return lineupselector->getValue(); };

  protected slots:
    void fillDataDirectLineupSelector(void);

  protected:
    const VideoSource        &parent;
    DataDirectUserID         *userid;
    DataDirectPassword       *password;
    DataDirectButton         *button;
    DataDirectLineupSelector *lineupselector;
    QString lastloadeduserid;
    QString lastloadedpassword;
    int source;
};

class XMLTV_generic_config: public VerticalConfigurationGroup 
{
public:
    XMLTV_generic_config(const VideoSource& _parent, QString _grabber);

    virtual void save();
    virtual void save(QString) { save(); }

protected:
    const VideoSource& parent;
    QString grabber;
};

class EITOnly_config: public VerticalConfigurationGroup
{
public:
    EITOnly_config(const VideoSource& _parent);
    virtual void save();
    virtual void save(QString) { save(); }

protected:
    UseEIT *useeit;
};

class XMLTVConfig: public VerticalConfigurationGroup, 
                   public TriggeredConfigurationGroup {
public:
    XMLTVConfig(const VideoSource& parent);
};

class VideoSource: public ConfigurationWizard {
public:
    VideoSource();
        
    int getSourceID(void) const { return id->intValue(); };

    void loadByID(int id);

    static void fillSelections(SelectSetting* setting);
    static QString idToName(int id);

    QString getSourceName(void) const { return name->getValue(); };

    virtual void save() {
        if (name)
            ConfigurationWizard::save();
    };
    virtual void save(QString destination) {
        if (name)
            ConfigurationWizard::save(destination);
    };

private:
    class ID: virtual public IntegerSetting,
              public AutoIncrementStorage {
    public:
        ID():
            AutoIncrementStorage("videosource", "sourceid") {
            setName("VideoSourceName");
            setVisible(false);
        };
        virtual QWidget* configWidget(ConfigurationGroup *cg, 
                                      QWidget* parent,
                                      const char* widgetName = 0) {
            (void)cg; (void)parent; (void)widgetName;
            return NULL;
        };
    };
    class Name: virtual public VSSetting,
                virtual public LineEditSetting {
    public:
        Name(const VideoSource& parent):
            VSSetting(parent, "name") {
            setLabel(QObject::tr("Video source name"));
        };
    };

private:
    ID   *id;
    Name *name;
};

class CaptureCard;
class CCSetting: virtual public Setting, public SimpleDBStorage {
protected:
    CCSetting(const CaptureCard& _parent, QString name):
        SimpleDBStorage("capturecard", name),
        parent(_parent) {
        setName(name);
    };

    int getCardID(void) const;

protected:
    virtual QString setClause(void);
    virtual QString whereClause(void);
private:
    const CaptureCard& parent;
};

class TunerCardInput: public ComboBoxSetting, public CCSetting
{
    Q_OBJECT
  public:
    TunerCardInput(const CaptureCard &parent);

  public slots:
    void fillSelections(const QString &device);
    void diseqcType(const QString &diseqcType);

  private:
    QString last_device;
    QString last_cardtype;
    int     last_diseqct;
};

class DVBAudioDevice: public LineEditSetting, public CCSetting {    
    Q_OBJECT
public:
    DVBAudioDevice(const CaptureCard& parent):
        CCSetting(parent,"audiodevice") {
        setVisible(false);
    };

    void save() {
        changed = true;
        settingValue = "";
        SimpleDBStorage::save();
    };
    void save(QString destination) {
        changed = true;
        settingValue = "";
        SimpleDBStorage::save(destination);
    };
};

class DVBVbiDevice: public LineEditSetting, public CCSetting {
    Q_OBJECT
public:
    DVBVbiDevice(const CaptureCard& parent):
        CCSetting(parent,"vbidevice") {
        setVisible(false);
    };
    void save() {
        changed = true;
        settingValue = "";
        SimpleDBStorage::save();
    };
    void save(QString destination) {
        changed = true;
        settingValue = "";
        SimpleDBStorage::save(destination);
    };
};

class CardType: public ComboBoxSetting, public CCSetting {
public:
    CardType(const CaptureCard& parent);
    static void fillSelections(SelectSetting* setting);
};

class TunerCardInput;
class DVBCardName;
class DVBCardType;

class DVBConfigurationGroup: public VerticalConfigurationGroup {
    Q_OBJECT
public:
    DVBConfigurationGroup(CaptureCard& a_parent);

public slots:
    void probeCard(const QString& cardNumber);

private:
    CaptureCard        &parent;

    TunerCardInput     *defaultinput;
    DVBCardName        *cardname;
    DVBCardType        *cardtype;
    SignalTimeout      *signal_timeout;
    ChannelTimeout     *channel_timeout;
    TransButtonSetting *buttonAnalog;
};

class CaptureCardGroup: public VerticalConfigurationGroup,
                        public TriggeredConfigurationGroup {
    Q_OBJECT
public:
    CaptureCardGroup(CaptureCard& parent);

protected slots:
    virtual void triggerChanged(const QString& value);
};

class CaptureCard: public ConfigurationWizard {
    Q_OBJECT
public:
    CaptureCard(bool use_card_group = true);

    int  getCardID(void) const { return id->intValue(); }

    void loadByID(int id);
    void setParentID(int id);

    static void fillSelections(SelectSetting* setting);
    static void fillSelections(SelectSetting* setting, bool no_children);

    void load() {
        ConfigurationWizard::load();
    };

public slots:
    void DiSEqCPanel();
    void analogPanel();
    void recorderOptionsPanel();

private:
    void reload(void);

    class ID: virtual public IntegerSetting,
              public AutoIncrementStorage {
    public:
        ID():
            AutoIncrementStorage("capturecard", "cardid") {
            setVisible(false);
            setName("ID");
        };
    };

    class ParentID: public CCSetting
    {
      public:
        ParentID(const CaptureCard &parent) : CCSetting(parent, "parentid")
        {
            setValue("0");
            setVisible(false);
        }
    };

    class Hostname: public HostnameSetting, public CCSetting {
    public:
        Hostname(const CaptureCard& parent): CCSetting(parent, "hostname") {};
    };

private:
    ID       *id;
    ParentID *parentid;
};

class CardInput;
class CISetting: virtual public Setting, public SimpleDBStorage {
protected:
    CISetting(const CardInput& _parent, QString name):
        SimpleDBStorage("cardinput", name),
        parent(_parent) {
        setName(name);
    };

    int getInputID(void) const;

    void fillSelections();

protected:
    virtual QString setClause(void);
    virtual QString whereClause(void);
private:
    const CardInput& parent;
};

class CaptureCardEditor: public ListBoxSetting, public ConfigurationDialog {
    Q_OBJECT
public:
    CaptureCardEditor() {
        setLabel(tr("Capture cards"));
    };

    virtual MythDialog* dialogWidget(MythMainWindow* parent,
                                     const char* widgetName=0);
    virtual int exec();
    virtual void load();
    virtual void save() { };

public slots:
    void menu();
    void edit();
    void del();

protected:
};

class VideoSourceEditor: public ListBoxSetting, public ConfigurationDialog {
    Q_OBJECT
public:
    VideoSourceEditor() {
        setLabel(tr("Video sources"));
    };

    virtual MythDialog* dialogWidget(MythMainWindow* parent,
                                     const char* widgetName=0);

    bool cardTypesInclude(const int& SourceID, 
                          const QString& thecardtype);

    virtual int exec();
    virtual void load();
    virtual void save() { };

public slots:
    void menu(); 
    void edit();
    void del();

protected:
};

class CardInputEditor: public ListBoxSetting, public ConfigurationDialog {
public:
    CardInputEditor() {
        setLabel(QObject::tr("Input connections"));
    };
    virtual ~CardInputEditor();

    virtual int exec();
    virtual void load();
    virtual void save() { };

protected:
    vector<CardInput*> cardinputs;
};

class StartingChannel : public ComboBoxSetting, public CISetting
{
    Q_OBJECT
  public:
    StartingChannel(const CardInput &parent) :
        ComboBoxSetting(false, 1), CISetting(parent, "startchan")
    {
        setLabel(QObject::tr("Starting channel"));
        setHelpText(QObject::tr("Starting LiveTV channel.") + " " +
                    QObject::tr("This is updated on every successful "
                                "channel change."));
    }
    void fillSelections(void) {;}
  public slots:
    void SetSourceID(const QString &sourceid);
};

class CardID;
class ChildID;
class InputName;
class SourceID;
class DVBLNBChooser;
class DiSEqCPos;
class DiSEqCPort;
class LNBLofSwitch;
class LNBLofLo;
class LNBLofHi;

class CardInput: public ConfigurationWizard
{
    Q_OBJECT
  public:
    CardInput(bool is_dvb_card);

    int getInputID(void) const { return id->intValue(); };

    void loadByID(int id);
    void loadByInput(int cardid, QString input);
    QString getSourceName(void) const;

    void fillDiseqcSettingsInput(QString _pos, QString _port);
    void SetChildCardID(uint);

    virtual void save();
    virtual void save(QString /*destination*/) { save(); }

  public slots:
    void channelScanner();
    void sourceFetch();

  private:
    class ID: virtual public IntegerSetting,
              public AutoIncrementStorage
    {
      public:
        ID():
            AutoIncrementStorage("cardinput", "cardid")
        {
            setVisible(false);
            setName("CardInputID");
        };
        virtual QWidget* configWidget(ConfigurationGroup *cg, QWidget* parent,
                                      const char* widgetName = 0)
        {
            (void)cg; (void)parent; (void)widgetName;
            return NULL;
        };
    };

    ID              *id;
    CardID          *cardid;
    ChildID         *childid;
    InputName       *inputname;
    SourceID        *sourceid;
    DVBLNBChooser   *lnbsettings;
    DiSEqCPos       *diseqcpos;
    DiSEqCPort      *diseqcport;
    LNBLofSwitch    *lnblofswitch;
    LNBLofLo        *lnbloflo;
    LNBLofHi        *lnblofhi;
    StartingChannel *startchan;
};

#endif
