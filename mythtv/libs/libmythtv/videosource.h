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
    void fillSelections(const QString& uid, const QString& pwd);
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
    DataDirect_config(const VideoSource& _parent); 

    virtual void load(QSqlDatabase* db);

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
};

class XMLTV_uk_config: public VerticalConfigurationGroup {
public:
    XMLTV_uk_config(const VideoSource& _parent);

    virtual void save(QSqlDatabase* db);

protected:
    const VideoSource& parent;
    RegionSelector* region;
    ProviderSelector* provider;
};

class XMLTV_generic_config: public LabelSetting {
public:
    XMLTV_generic_config(const VideoSource& _parent, QString _grabber);

    virtual void load(QSqlDatabase* db) { (void)db; };
    virtual void save(QSqlDatabase* db);

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

    void loadByID(QSqlDatabase* db, int id);

    static void fillSelections(QSqlDatabase* db, SelectSetting* setting);
    static QString idToName(QSqlDatabase* db, int id);

    QString getSourceName(void) const { return name->getValue(); };

    virtual void save(QSqlDatabase* db) {
        if (name)
            ConfigurationWizard::save(db);
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

class DvbSatSetting: virtual public Setting, public SimpleDBStorage {
protected:
    DvbSatSetting(const CaptureCard& _parent, int _satnum, QString name):
        SimpleDBStorage("dvb_sat", name),
        parent(_parent), satnum(_satnum) {
        setName(name);
    };

    int getCardID(void) const;
    int getSatNum(void) const;

protected:
    virtual QString setClause(void);
    virtual QString whereClause(void);
private:
    const CaptureCard& parent;
    int satnum;
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

    void save(QSqlDatabase* db) {
        changed = true;
        settingValue = "";
        SimpleDBStorage::save(db);
    };
};

class DVBVbiDevice: public LineEditSetting, public CCSetting {
    Q_OBJECT
public:
    DVBVbiDevice(const CaptureCard& parent):
        CCSetting(parent,"vbidevice") {
        setVisible(false);
    };
    void save(QSqlDatabase* db) {
        changed = true;
        settingValue = "";
        SimpleDBStorage::save(db);
    };
};

class DVBDefaultInput: public LineEditSetting, public CCSetting {
    Q_OBJECT
public:
    DVBDefaultInput(const CaptureCard& parent):
        CCSetting(parent,"defaultinput") {
        setVisible(false);
    };
    void save(QSqlDatabase* db) {
        changed = true;
        settingValue = "DVBInput";
        SimpleDBStorage::save(db);
    };
};

class DVBChannels: public ComboBoxSetting {
    Q_OBJECT
public:
    DVBChannels() : db(NULL)
    {
        setLabel(QObject::tr("Channels"));
        setHelpText(QObject::tr("This box contains all channels from the "
                    "selected video source. Select a channel here and press "
                    "the 'Load and Tune' button to load the channel settings "
                    "into the previous screen and try to tune it."));
    }

    void save(QSqlDatabase* db) { (void)db; };
    void load(QSqlDatabase* _db) {
        db = _db;
        fillSelections("All");
    };
public slots:
    void fillSelections(const QString& videoSource);

private:
    QSqlDatabase* db;
};

class DVBInfoLabel: public LabelSetting, public TransientStorage {
    Q_OBJECT
public:
    DVBInfoLabel(QString label):
      LabelSetting(), TransientStorage() {
        setLabel(label);
    };
public slots:
    void set(int val) {
        setValue(QString("%1").arg(val));
    };
};

class DVBStatusLabel: public LabelSetting, public TransientStorage {
    Q_OBJECT
public:
    DVBStatusLabel() {
        setLabel(QObject::tr("Status"));
    };
public slots:
    void set(QString str) {
        setValue(str);
    };
};

class DVBSatelliteList: public ListBoxSetting, public TransientStorage {
    Q_OBJECT
public:
    DVBSatelliteList(CaptureCard& _parent);

    void load(QSqlDatabase* _db);

public slots:
    void fillSelections(const QString& v);
protected:
    QSqlDatabase* db;
    CaptureCard& parent;
    int satellites;
};

class DVBSatelliteWizard: public ConfigurationWizard {
    Q_OBJECT
public:
    DVBSatelliteWizard(CaptureCard& _parent);

    int exec(QSqlDatabase* _db) {
        db = _db;
        return ConfigurationWizard::exec(db);
    };

public slots:
    void editSat(int satnum);

protected:
    QSqlDatabase* db;
    CaptureCard& parent;
    DVBSatelliteList* list;
};

class DVBSignalChannelOptions;

class DVBCardVerificationWizard: public ConfigurationWizard {
    Q_OBJECT
public:
    DVBCardVerificationWizard(int cardNum);
    ~DVBCardVerificationWizard(); 

    void load(QSqlDatabase* _db) {
        db = _db;
        ConfigurationWizard::load(db);
    };

private slots:
    void tuneConfigscreen();
    void tunePredefined();

private:
    QSqlDatabase* db;
    ChannelID cid;
#ifdef USING_DVB
    DVBChannel* chan;
    DVBSignalChannelOptions* dvbopts;
    DVBChannels* channels;
#endif
};

class DVBAdvancedConfigMenu: public ConfigurationPopupDialog,
                             public VerticalConfigurationGroup {
    Q_OBJECT
public:
    DVBAdvancedConfigMenu(CaptureCard& a_parent);

    void exec(QSqlDatabase* _db) {
        db = _db;
        ConfigurationPopupDialog::exec(db);
    };

public slots:
    void execCVW();
    void execACW();
    void execSAT();

private:
    CaptureCard& parent;
    QSqlDatabase* db;
};

class CardType: public ComboBoxSetting, public CCSetting {
public:
    CardType(const CaptureCard& parent);
    static void fillSelections(SelectSetting* setting);
};

class DVBCardName;
class DVBCardType;

class DVBConfigurationGroup: public VerticalConfigurationGroup {
    Q_OBJECT
public:
    DVBConfigurationGroup(CaptureCard& a_parent);

    void load(QSqlDatabase* _db) {
        db = _db;
        VerticalConfigurationGroup::load(db);
    };

public slots:
    void probeCard(const QString& cardNumber);

private:
    CaptureCard& parent;

    QSqlDatabase *db;
    DVBCardName* cardname;
    DVBCardType* cardtype;
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

    void loadByID(QSqlDatabase* db, int id);

    static void fillSelections(QSqlDatabase* db, SelectSetting* setting);

    void load(QSqlDatabase* _db) {
        db = _db;
        ConfigurationWizard::load(db);
    };

public slots:
    void execDVBConfigMenu();
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
    QSqlDatabase* db;
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

    void fillSelections(QSqlDatabase* db);

protected:
    virtual QString setClause(void);
    virtual QString whereClause(void);
private:
    const CardInput& parent;
};

class CaptureCardEditor: public ListBoxSetting, public ConfigurationDialog {
    Q_OBJECT
public:
    CaptureCardEditor(QSqlDatabase* _db):
        db(_db) {
        setLabel(tr("Capture cards"));
    };

    virtual MythDialog* dialogWidget(MythMainWindow* parent,
                                     const char* widgetName=0);
    virtual int exec(QSqlDatabase* db);
    virtual void load(QSqlDatabase* db);
    virtual void save(QSqlDatabase* db) { (void)db; };

public slots:
    void menu();
    void edit();
    void del();

protected:
    QSqlDatabase* db;
};

class VideoSourceEditor: public ListBoxSetting, public ConfigurationDialog {
    Q_OBJECT
public:
    VideoSourceEditor(QSqlDatabase* _db):
        db(_db) {
        setLabel(tr("Video sources"));
    };

    virtual MythDialog* dialogWidget(MythMainWindow* parent,
                                     const char* widgetName=0);

    virtual int exec(QSqlDatabase* db);
    virtual void load(QSqlDatabase* db);
    virtual void save(QSqlDatabase* db) { (void)db; };

public slots:
    void menu(); 
    void edit();
    void del();

protected:
    QSqlDatabase* db;
};

class CardInputEditor: public ListBoxSetting, public ConfigurationDialog {
public:
    CardInputEditor(QSqlDatabase* _db):
        db(_db) {
        setLabel(QObject::tr("Input connections"));
    };
    virtual ~CardInputEditor();

    virtual int exec(QSqlDatabase* db);
    virtual void load(QSqlDatabase* db);
    virtual void save(QSqlDatabase* db) { (void)db; };

protected:
    vector<CardInput*> cardinputs;
    QSqlDatabase* db;
};

#endif
