#ifndef VIDEOSOURCE_H
#define VIDEOSOURCE_H

#include "libmyth/settings.h"
#include <qregexp.h>
#include <vector>
#include <qdir.h>
#include <qstringlist.h>

#include "channelsettings.h"
#include "datadirect.h"

#ifdef USING_DVB
#include "dvbchannel.h"
#endif

class CardUtil
{
  public:
    enum DVB_TYPES {ERROR_OPEN, ERROR_PROBE, QPSK, QAM, OFDM, ATSC};
    enum DISEQC_TYPES { SINGLE=0, MINI_2, SWITCH_2_1_0, SWITCH_2_1_1,
                        SWITCH_4_1_0, SWITCH_4_1_1, POSITIONER_1_2,
                        POSITIONER_X, POSITIONER_1_2_SWITCH_2,
                        POSITIONER_X_SWITCH_2 };
    static const QString DVB;

    static bool isCardPresent(const QString &strType);
    static DVB_TYPES cardDVBType(unsigned nVideoDev);
    static DVB_TYPES cardDVBType(unsigned nVideoDev, QString &name);
    static int videoDeviceFromCardID(unsigned nCardID);
    static bool isDVB(unsigned nCardID);
    static DISEQC_TYPES diseqcType(unsigned nCardID);
};

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

class RegionSelector: public ComboBoxSetting, public TransientStorage {
    Q_OBJECT
public:
    RegionSelector() {
        setLabel(QObject::tr("Region"));
        fillSelections();
    };

public slots:
    void fillSelections();
};

class ProviderSelector: public ComboBoxSetting, public TransientStorage {
    Q_OBJECT
public:
    ProviderSelector(const QString& _grabber) :
        grabber(_grabber) { setLabel(QObject::tr("Provider")); };

public slots:
    void fillSelections(const QString& location);

protected:
    QString grabber;
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

class DataDirect_config: public VerticalConfigurationGroup {
    Q_OBJECT
public:
    DataDirect_config(const VideoSource& _parent, int _source = DD_ZAP2IT); 

    virtual void load();

    QString getLineupID(void) const { return lineupselector->getValue(); };

protected slots:
    void fillDataDirectLineupSelector();

protected:
    const VideoSource& parent;
    DataDirectUserID* userid;
    DataDirectPassword* password;
    DataDirectLineupSelector* lineupselector;
    DataDirectButton* button;
    QString lastloadeduserid;
    QString lastloadedpassword;
    int source;
};

class XMLTV_uk_config: public VerticalConfigurationGroup {
public:
    XMLTV_uk_config(const VideoSource& _parent);

    virtual void save();

protected:
    const VideoSource& parent;
    RegionSelector* region;
    ProviderSelector* provider;
};

class XMLTV_generic_config: public LabelSetting {
public:
    XMLTV_generic_config(const VideoSource& _parent, QString _grabber);

    virtual void load() {};
    virtual void save();

protected:
    const VideoSource& parent;
    QString grabber;
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
    ID* id;
    Name* name;

protected:
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

class TunerCardInput: public ComboBoxSetting, public CCSetting {
    Q_OBJECT
public:
    TunerCardInput(const CaptureCard& parent):
        CCSetting(parent, "defaultinput") {
        setLabel(QObject::tr("Default input"));
    };

public slots:
    void fillSelections(const QString& device);
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
};

class DVBDiseqcInputList
{
  public:
    DVBDiseqcInputList() { clearValues(); }
    DVBDiseqcInputList(const QString& in, const QString& prt, 
                       const QString& pos) : input(in), port(prt), position(pos)
    {}

    void clearValues() 
    {
        input = "";
        port = "";
        position = "";
    }

    QString input;
    QString port;
    QString position;
};

class DVBDefaultInput: public ComboBoxSetting, public CCSetting {
    Q_OBJECT
public:

    DVBDefaultInput(const CaptureCard& parent):
        CCSetting(parent,"defaultinput") {
        setLabel(QObject::tr("Default input"));
    };

public slots:
    void fillSelections(const QString& type);
};

class CardType: public ComboBoxSetting, public CCSetting {
public:
    CardType(const CaptureCard& parent);
    static void fillSelections(SelectSetting* setting);
};

class DVBCardName;
class DVBCardType;
class DVBDiseqcType;

class DVBConfigurationGroup: public VerticalConfigurationGroup {
    Q_OBJECT
public:
    DVBConfigurationGroup(CaptureCard& a_parent);

    void load() {
        VerticalConfigurationGroup::load();
    };

public slots:
    void probeCard(const QString& cardNumber);

private:
    CaptureCard& parent;

    DVBDefaultInput* defaultinput;
    DVBCardName* cardname;
    DVBCardType* cardtype;
    DVBDiseqcType* diseqctype;
    TransButtonSetting *advcfg;
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
    CaptureCard();

    int getCardID(void) const {
        return id->intValue();
    };

    QString getDvbCard() { return dvbCard; };

    void loadByID(int id);

    static void fillSelections(SelectSetting* setting);

    void load() {
        ConfigurationWizard::load();
    };

public slots:
    void execDVBConfigMenu();
    void execDiseqcWiz();
    void execAdvConfigWiz();
    void setDvbCard(const QString& card) { dvbCard = card; };

private:
    class ID: virtual public IntegerSetting,
              public AutoIncrementStorage {
    public:
        ID():
            AutoIncrementStorage("capturecard", "cardid") {
            setVisible(false);
            setName("ID");
        };
    };

    class Hostname: public HostnameSetting, public CCSetting {
    public:
        Hostname(const CaptureCard& parent): CCSetting(parent, "hostname") {};
    };

private:
    ID* id;
    QString dvbCard;
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

#endif
